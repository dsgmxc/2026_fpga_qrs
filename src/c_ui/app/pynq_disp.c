/**
 * @file pynq_disp.c
 * @brief LVGL 显示驱动(PYNQ 版)—— 替换前序的 `lv_port_disp.c`
 *
 * 接口在 `port/BSP/LVGL/lv_port_disp.h`(与前序版只差 `Init` 收两个地址)。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【架构:direct_mode + 两块整屏 DDR 缓冲】
 *
 *   LVGL 直接往**前台/后台这两块 DDR 帧缓存**里画(direct_mode = 1),
 *   flush 回调只做两件事:帧末请求 PL 换址、把刚改过的矩形同步到新后台。
 *   ⇒ 没有额外的拷贝缓冲,一帧只写一次屏。
 *
 *   为什么必须是两块:**换址只在 VBlank 生效**,屏幕上永远在扫其中一块。
 *   只有一块的话,PS 画到一半就会被扫出去(撕裂)。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 🔴 【为什么一定要做"矩形同步" SyncInvalidAreas】
 *   direct_mode + full_refresh = 0 时,LVGL **只重画失效区域**,
 *   其余像素指望缓冲里还是上一帧的样子。
 *   可换址之后,**新后台**是更早的那一帧 —— 不补的话,界面会一块块
 *   "退回到两帧前"。这个 bug 平时看不出来(只有局部刷新时才显形),
 *   一动界面就"局部花",非常难归因。
 *   ⇒ 每帧把本次失效的矩形从新前台 memcpy 到新后台。
 *
 * 🔴 【与前序的一处**做不到**的等价:没有 DMA2D】
 *   前序用 STM32 的 DMA2D 硬件搬矩形,失败才退回 CPU 逐行拷贝。
 *   PYNQ 上 zynq-7000 的 PL 里我们**没有实现矩形搬运器**
 *   (是刻意的:见下方"资源取舍"),所以**只有 CPU 逐行 memcpy 这一条路**。
 *   代价:整屏同步要搬 1.2 MB,而映射是**非缓存**的(mmap /dev/mem),
 *   带宽比缓存低不少 ⇒ 首帧(全屏失效)会明显慢于后续帧。
 *   ⇒ **实测帧率必须回填**,这是风险 R2 的判据;不够再考虑
 *      「PL 加一个矩形拷贝器」或「改用 dma-heap 缓存映射 + sync ioctl」。
 *
 * 📌 资源取舍(B04 素材):为什么不在 PL 里做矩形搬运?
 *   矩形搬运要么自己写地址发生器+突发状态机,要么用 AXI CDMA。
 *   而本项目的**主路径根本不需要它** —— 只有"界面局部刷新且两块缓冲要
 *   保持一致"时才用得上。为此在 PL 里加一个只会被 UI 用到的模块,
 *   属于"以显著的资源代价换取有限的性能收益",与 B4 的判据相悖。
 *   ⇒ 先用 CPU 路径,把实测数据拿到手再决定。
 */

#include "BSP/LVGL/lv_port_disp.h"

#include "pynq_regs.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "src/core/lv_refr.h"      /* _lv_refr_get_disp_refreshing / inv_areas */

/* ------------------------------------------------------------------ */
/* 参数                                                                */
/* ------------------------------------------------------------------ */

/** 换址等待上限。一帧 17.07 ms,给 50 ms(与前序 LV_PORT_DISP_VBLANK_TIMEOUT_MS 一致) */
#define VBLANK_TIMEOUT_MS   50

/* 每像素字节数。从 cv_regs 的帧缓存尺寸推,不写死 2 —— 写死的话以后换
 * RGB888 面板会出现"行内错位但整幅还在"的鬼影,极难归因。 */
#define BYTES_PER_PIXEL     (CV_FB_FRAME_BYTES / (CV_PANEL_W * CV_PANEL_H))

/* 私有错误码(前序的 DMA2D 一档换成本项目的 COPY) */
enum {
    LV_PORT_DISP_ERROR_NONE = 0,
    LV_PORT_DISP_ERROR_FRAME,
    LV_PORT_DISP_ERROR_AREA,
    LV_PORT_DISP_ERROR_REGISTER,
    LV_PORT_DISP_ERROR_VBLANK,
    LV_PORT_DISP_ERROR_COPY,
};

/* ------------------------------------------------------------------ */
/* 状态                                                                */
/* ------------------------------------------------------------------ */

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;

static uint16_t *s_fb[2];            /* 映射后的 CPU 虚拟地址 */
static uint32_t  s_fb_phys[2];       /* 物理地址(给 PL 写的那个) */
static int       s_front = 0;        /* 当前前台 = 屏幕上正在扫的那块 */
static int       s_back  = 1;        /* 当前后台 = LVGL 正在画的那块 */

static uint32_t s_flush_count;
static uint32_t s_flush_error_count;
static uint32_t s_cpu_copy_count;    /* 本工程里 == 矩形同步次数(无 DMA2D) */
static uint32_t s_frame_swap_count;
static uint32_t s_vblank_fallback_count;
static uint32_t s_last_error;

/* ------------------------------------------------------------------ */
/* 小工具                                                              */
/* ------------------------------------------------------------------ */

/** 判断某个地址落在哪块帧缓存里。认不出返回 -1。 */
static int fb_index_of(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    for (int i = 0; i < 2; i++) {
        uintptr_t base = (uintptr_t)s_fb[i];
        if (a >= base && a < base + CV_FB_FRAME_BYTES) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 把一个矩形从一块帧缓存拷到另一块。
 *
 * ⚠️ **必须逐行拷,不能整块一次 memcpy** —— 目标可能是"同一行的另一段",
 *    整块拷会把不相干的像素也带过去(行首列偏移会算错)。
 *    这是前序 `LV_PortDisp_CopyByCpu` 的做法,照搬。
 */
static void copy_rect(int src_idx, int dst_idx,
                      const lv_area_t *area)
{
    /* 夹到面板范围内。越界会写坏别的内存 —— 而这是**外部输入**(LVGL 的
     * 失效矩形),不能假定它一定合法。 */
    int x1 = area->x1 < 0 ? 0 : area->x1;
    int y1 = area->y1 < 0 ? 0 : area->y1;
    int x2 = area->x2 >= CV_PANEL_W ? CV_PANEL_W - 1 : area->x2;
    int y2 = area->y2 >= CV_PANEL_H ? CV_PANEL_H - 1 : area->y2;
    if (x1 > x2 || y1 > y2) {
        return;
    }

    const size_t row_bytes = (size_t)(x2 - x1 + 1) * BYTES_PER_PIXEL;
    for (int y = y1; y <= y2; y++) {
        const uint8_t *src = (const uint8_t *)s_fb[src_idx] + ((size_t)y * CV_PANEL_W + x1) * BYTES_PER_PIXEL;
        uint8_t       *dst = (uint8_t *)s_fb[dst_idx]       + ((size_t)y * CV_PANEL_W + x1) * BYTES_PER_PIXEL;
        memcpy(dst, src, row_bytes);
    }
}

/**
 * @brief 把**本次失效的矩形**从新前台补到新后台(见文件头的长注释)。
 *
 * 用到 LVGL 内部结构 `inv_areas` / `inv_area_joined` / `inv_p` ——
 * 这是前序的做法,也是 LVGL 8.2 里唯一能拿到"这一轮画了哪些矩形"的地方。
 * ⚠️ 因此本文件**绑死 LVGL 8.2**:升 LVGL 版本时这里要先看。
 */
static void sync_invalid_areas(int front_idx, int back_idx)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    if (disp == NULL) {
        return;                        /* 不是从刷新流程里调过来的,无事可做 */
    }

    for (uint32_t i = 0; i < disp->inv_p && i < LV_INV_BUF_SIZE; i++) {
        if (disp->inv_area_joined[i]) {
            continue;                  /* 已经被合并进更早的矩形,跳过 */
        }
        copy_rect(front_idx, back_idx, &disp->inv_areas[i]);
        s_cpu_copy_count++;
    }
}

/* ------------------------------------------------------------------ */
/* flush 回调                                                          */
/* ------------------------------------------------------------------ */
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px_map)
{
    if (drv == NULL || area == NULL) {
        s_last_error = LV_PORT_DISP_ERROR_AREA;
        s_flush_error_count++;
        if (drv != NULL) {
            lv_disp_flush_ready(drv);
        }
        return;
    }

    /* 区域越界:LVGL 不该给这种值,给了就是我们的 res 设错了。
     * **不要**照着画 —— 越界写会踩坏别的内存。 */
    if (area->x1 < 0 || area->y1 < 0 ||
        area->x2 >= CV_PANEL_W || area->y2 >= CV_PANEL_H) {
        s_last_error = LV_PORT_DISP_ERROR_AREA;
        s_flush_error_count++;
        lv_disp_flush_ready(drv);
        return;
    }

    /* direct_mode 下一轮刷新会分多次调 flush(每个失效区一次),
     * 但**只有最后一次**代表"这一帧画完了"。前序也是只认最后一次。 */
    if (!lv_disp_flush_is_last(drv)) {
        lv_disp_flush_ready(drv);
        return;
    }

    /* 本次画的是哪一块?direct_mode 下 flush 时 buf_act 仍指向刚画完的那块
     * (LVGL 在调用完 flush_cb **之后**才交换 buf_act,见 lv_refr.c:1008)。 */
    int rendered = fb_index_of(drv->draw_buf->buf_act);
    if (rendered < 0) {
        rendered = fb_index_of((const void *)px_map);   /* 兜底 */
    }
    if (rendered < 0) {
        s_last_error = LV_PORT_DISP_ERROR_FRAME;
        s_flush_error_count++;
        lv_disp_flush_ready(drv);
        return;
    }

    const int next_back = 1 - rendered;

    /* ---- 提交:请求 PL 在 VBlank 换到 rendered,并**等它生效** ---- */
    if (cv_lcd_swap_wait(rendered, VBLANK_TIMEOUT_MS) != 0) {
        /* ⚠️ 这里没有"立即换址"的退路 —— PL 的换址**设计上只在 VBlank 生效**
         *    (lcd_vblank_ctrl 的唯一职责),不存在绕过它的写法。
         *    请求仍然挂着,通常下一个 VBlank 就会生效,所以这里**不中断 UI**,
         *    只记数 + 记错误,让排障脚本能看见。 */
        s_last_error = LV_PORT_DISP_ERROR_VBLANK;
        s_vblank_fallback_count++;
        s_flush_error_count++;
    }
    s_frame_swap_count++;

    s_front = rendered;
    s_back  = next_back;

    /* ---- 让新后台与新前台一致,否则下一帧的局部重画会退回两帧前 ---- */
    sync_invalid_areas(s_front, s_back);

    /* 错误码只保留"最近一次"的:本帧正常就清掉,好让上层脚本判"此刻是否健康"
     * 而不是"历史上是否出过"。历史计数在 *_count 里,两者不要混。 */
    if (s_last_error != LV_PORT_DISP_ERROR_VBLANK) {
        s_last_error = LV_PORT_DISP_ERROR_NONE;
    }

    s_flush_count++;
    lv_disp_flush_ready(drv);
}

/* ------------------------------------------------------------------ */
/* 初始化                                                              */
/* ------------------------------------------------------------------ */
uint32_t LV_PortDisp_Init(uint32_t fb0_phys, uint32_t fb1_phys)
{
    /* 64 KB 对齐是**硬要求**:PL 的 lcd_fb_reader 按行取数,
     * 基址不对齐会整幅错位(而且不报错,只是花屏)。 */
    if ((fb0_phys % CV_FB_ALIGNMENT) != 0 || (fb1_phys % CV_FB_ALIGNMENT) != 0) {
        fprintf(stderr, "[disp] 帧缓存地址未 64 KB 对齐:fb0=0x%08X fb1=0x%08X\n",
                fb0_phys, fb1_phys);
        s_last_error = LV_PORT_DISP_ERROR_FRAME;
        return LV_PORT_DISP_ERROR;
    }
    if (fb0_phys == fb1_phys) {
        fprintf(stderr, "[disp] 两块帧缓存地址相同(0x%08X)—— 双缓冲无从谈起\n", fb0_phys);
        s_last_error = LV_PORT_DISP_ERROR_FRAME;
        return LV_PORT_DISP_ERROR;
    }

    s_fb_phys[0] = fb0_phys;
    s_fb_phys[1] = fb1_phys;

    for (int i = 0; i < 2; i++) {
        s_fb[i] = (uint16_t *)cv_phys_map(s_fb_phys[i], CV_FB_FRAME_BYTES);
        if (s_fb[i] == NULL) {
            fprintf(stderr, "[disp] 映射帧缓存 %d(0x%08X)失败\n", i, s_fb_phys[i]);
            s_last_error = LV_PORT_DISP_ERROR_FRAME;
            return LV_PORT_DISP_ERROR;
        }
    }

    s_front = 0;
    s_back  = 1;
    s_flush_count = s_flush_error_count = s_cpu_copy_count = 0;
    s_frame_swap_count = s_vblank_fallback_count = 0;
    s_last_error = LV_PORT_DISP_ERROR_NONE;

    /* ---- LVGL 侧:两块整屏缓冲,direct_mode ---- */
    /* ⚠️ 顺序照前序:buf1 = 后台, buf2 = 前台。
     *    direct_mode 下 LVGL 先用 buf1 画 ⇒ 首帧画进"后台",正好合逻辑。 */
    lv_disp_draw_buf_init(&s_draw_buf, (void *)s_fb[s_back], (void *)s_fb[s_front],
                          CV_PANEL_W * CV_PANEL_H);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res     = CV_PANEL_W;
    s_disp_drv.ver_res     = CV_PANEL_H;
    s_disp_drv.flush_cb    = disp_flush;
    s_disp_drv.draw_buf    = &s_draw_buf;
    s_disp_drv.direct_mode = 1;
    /* full_refresh = 0:只重画失效区。**这正是不做矩形同步就会花屏的原因**。 */
    s_disp_drv.full_refresh = 0;

    if (lv_disp_drv_register(&s_disp_drv) == NULL) {
        s_last_error = LV_PORT_DISP_ERROR_REGISTER;
        return LV_PORT_DISP_ERROR;
    }

    printf("[disp] direct_mode 双缓冲就绪:fb0=0x%08X fb1=0x%08X 前台=%d"
           "(非缓存映射;无 DMA2D,矩形同步走 CPU memcpy)\n",
           s_fb_phys[0], s_fb_phys[1], s_front);
    return LV_PORT_DISP_OK;
}

/* ------------------------------------------------------------------ */
uint32_t LV_PortDisp_GetFrontAddress(void)    { return s_fb_phys[s_front]; }
uint32_t LV_PortDisp_GetFlushCount(void)      { return s_flush_count; }
uint32_t LV_PortDisp_GetFlushErrorCount(void) { return s_flush_error_count; }
/* 语义:CPU 矩形拷贝的**矩形个数**。本工程没有 DMA2D ⇒ 每条同步都算一次,
 * 所以这个数≈失效矩形总数,不是"失败次数"。前序是"DMA2D 失败退到 CPU 的次数"。
 * ⚠️ 名字沿用前序以免接口漂移,含义已变 —— 报告里要写清。 */
uint32_t LV_PortDisp_GetCpuFallbackCount(void) { return s_cpu_copy_count; }
uint32_t LV_PortDisp_GetFrameSwapCount(void)   { return s_frame_swap_count; }
uint32_t LV_PortDisp_GetVBlankFallbackCount(void) { return s_vblank_fallback_count; }
uint32_t LV_PortDisp_GetLastError(void)        { return s_last_error; }
