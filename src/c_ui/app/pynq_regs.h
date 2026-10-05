/**
 * @file pynq_regs.h
 * @brief PS 侧 AXI 寄存器访问 + 物理内存映射(/dev/mem)
 *
 * ⚠️ **本文件里的寄存器偏移与位域是 `src/python/cv_regs.py` 的镜像。**
 *    那份 Python 是单一事实源;改寄存器**先改它**,再同步到这里。
 *    两处不一致的后果是"写进去了、没反应、不报错"。
 *
 * ⚠️ 本进程必须能读 /dev/mem(即必须是 root)。若内核开了 `CONFIG_STRICT_DEVMEM`,
 *    `mmap` 普通 RAM 会被拒 —— 那就是风险 R1,`cv_phys_map()` 会返回 NULL 并给出提示。
 *
 * ⚠️ **为什么用 /dev/mem 而不是 malloc**:PL 读的是**物理内存**,而 malloc 给的是
 *    虚拟地址。帧缓存必须由 Python(`pynq.allocate`,走 CMA)分配后把**物理地址**
 *    传进来,C 侧再映射。
 *
 * ⚠️ 这个映射是**非缓存**的 ⇒ CPU 写进去,PL 立刻看得到,**不需要 flush**。
 *    这正是绕开风险 D1(PS cache 与 PL 读到的不一致)的地方。
 *    代价:非缓存写比缓存写慢(没有 write-back 合并)。UI 大部分是静态重绘,
 *    实测帧率见 report/measurements/。
 */
#ifndef PYNQ_REGS_H
#define PYNQ_REGS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 寄存器字节偏移(cv_regs.py 的 REG 表)                                */
/* ------------------------------------------------------------------ */
#define CV_REG_BIAS_CODE     0x14u   /* RW [19:0] → dac_1220.code          */
#define CV_REG_DDS_FREQ      0x80u   /* RW [27:0] → adc_9833.code          */
#define CV_REG_CV_CTRL       0x84u   /* RW [0] dac_rst_n ★ 见下             */

/* CV_CTRL(0x84)—— dac_1220 初始化的放行位。2026-10-05 加。
 *
 * 🔴 **复位值 = 0 = DAC 被按在复位态,初始化不跑。**
 *    DAC1220 没有复位引脚,靠一串**三段高电平的串行复位图案**复位,而那三段是
 *    **双向窗口**(手册 Figure 12 / Table 5):t16 = 208.3~325.5 µs、
 *    t18 = 416.7~732.4 µs、t19 = 833.3~976.6 µs —— **超上界就不触发**。
 *    dac_1220.v 的常数按 50 MHz 折,而本模块跑 PS FCLK0;板上 BOOT.BIN 把
 *    FCLK0 留在 **27.778 MHz** ⇒ 三段变成 468/1026/1620 µs,全部超上界
 *    ⇒ 复位不触发 ⇒ DIR 写入无效 ⇒ 输出停在芯片默认的 **−10 V**。
 *
 * ✅ **放行由 Python 侧独占**(`drivers/board.py::release_dac()`,它先验 FCLK0
 *    在不在 [46.1, 54.0] MHz 窗口内,不在就拒绝)。**C 侧只读不写** ——
 *    避免两个写入方;这里只负责把"还没放行"这件事**喊出来**,不要静默。
 */
#define CV_CTRL_DAC_RST_N    0x1u    /* [0] 1 = 已放行 / 0 = 仍按在复位     */
#define CV_REG_LCD_CTRL      0x50u   /* RW [1:0] src_sel                   */
#define CV_REG_LCD_FILL      0x54u   /* RW [23:0] RGB888 整屏填充          */
#define CV_REG_FB_BASE0      0x58u   /* RW 帧缓存 0 物理基址(64 KB 对齐)  */
#define CV_REG_FB_BASE1      0x5Cu   /* RW 帧缓存 1 物理基址               */
#define CV_REG_LCD_SWAP      0x60u   /* W  [0]目标索引 / R [0]front [1]pending */
#define CV_REG_FB_STATUS     0x64u   /* RO 场消隐/换址计数 + 粘滞错误标志  */
#define CV_REG_TOUCH_STATUS  0x68u   /* RO ★ 一次触摸事件的提交点,必须最先读 */
#define CV_REG_TOUCH_PT0     0x6Cu   /* RO 触点 0 快照                     */
#define CV_REG_TOUCH_PT1     0x70u   /* RO 触点 1 快照                     */

/* ------------------------------------------------------------------ */
/* 显示源选择 —— ★ 帧缓存是 2,不是 1(见 cv_regs.py 文件头"错 1")      */
/* ------------------------------------------------------------------ */
#define CV_LCD_SRC_BARS      0u
#define CV_LCD_SRC_FILL      1u
#define CV_LCD_SRC_DDR       2u

/* ------------------------------------------------------------------ */
/* 位域                                                                */
/* ------------------------------------------------------------------ */
/* 0x68: [2:0]count [3]pen_down [4]err(粘滞) [7:5]保留 [15:8]seq
 *       出自 gt911_ctrl.v:278 {16'b0, seq, 3'b0, err_sticky, pen_down, cur_count} */
#define CV_TOUCH_COUNT(r)    ((uint32_t)((r) & 0x7u))
#define CV_TOUCH_PEN(r)      ((uint32_t)(((r) >> 3) & 0x1u))
#define CV_TOUCH_ERR(r)      ((uint32_t)(((r) >> 4) & 0x1u))
#define CV_TOUCH_SEQ(r)      ((uint32_t)(((r) >> 8) & 0xFFu))
#define CV_TOUCH_MASK_KNOWN  0x0000FF1Fu   /* count|pen|err|seq 之外全是 0 */

/* 0x6C / 0x70: [11:0]x [27:16]y
 *       出自 gt911_ctrl.v:280 {4'b0, pt0_y, 4'b0, pt0_x}
 *    ⚠️ x/y 各只有 12 位 ⇒ 越界的形态是 1024~4095,不是"很大" */
#define CV_TOUCH_X(p)        ((uint32_t)((p) & 0x0FFFu))
#define CV_TOUCH_Y(p)        ((uint32_t)(((p) >> 16) & 0x0FFFu))

/* 0x60(读)/ 0x64: [0]front [1]pending 与粘滞标志 */
#define CV_SWAP_FRONT(s)     ((uint32_t)((s) & 0x1u))
#define CV_SWAP_PENDING(s)   ((uint32_t)(((s) >> 1) & 0x1u))
#define CV_FB_UNDERRUN(s)    ((uint32_t)(((s) >> 19) & 0x1u))   /* 写 0x60 清 */
#define CV_FB_AXIERR(s)      ((uint32_t)(((s) >> 20) & 0x1u))
#define CV_FB_VBLANK_CNT(s)  ((uint32_t)(((s) >> 2) & 0x1FFu))
#define CV_FB_SWAP_CNT(s)    ((uint32_t)(((s) >> 11) & 0xFFu))

/* ------------------------------------------------------------------ */
/* 模拟前端:偏压(DAC1220)/ 激励(AD9833)                              */
/* ------------------------------------------------------------------ */
/* 0x14 [19:0] → dac_1220.code。DAC 是**模块级 ±10 V**:
 *     V_module = −10 + 20 × code / 1048575
 * 再到被栅极看到的偏压,还要乘【外部高压级的 10:1】——
 *     V_bias = V_module × 10        ⇒ 量程 −100 V … +100 V
 *
 * 🔴 **那个 10:1 是硬件接线,不是寄存器。** 换一块高压板就得改这个常量,
 *    而 RTL / 寄存器映射一个字都不用动 —— 这正是换算放在 PS 侧的理由。
 *
 * ⚠️ `code = 0` 是 **−100 V**,不是 0 V。空闲态用中位码(见 CV_BIAS_CODE_MID)。
 *    界面的偏压范围是 0.1–100.0 V(只走正向半轴),下限对应 code ≈ 0x8020C。 */
#define CV_BIAS_CODE_MAX     0xFFFFFu      /* 20 位满量程 = 1048575 */
#define CV_BIAS_CODE_MID     0x80000u      /* 中位码 = 0 V 偏压(RTL 复位值) */
#define CV_BIAS_HV_RATIO     10.0f         /* 外部高压级倍数(硬件,非寄存器) */
#define CV_BIAS_TENTHS_MIN   1u            /* 0.1 V —— 界面下限 */
#define CV_BIAS_TENTHS_MAX   1000u         /* 100.0 V —— 界面上限 */

/**
 * @brief 偏压 → 偏压码(**全精度**,不经 0.1 V 量化)。越界夹到 [0, 0xFFFFF]。
 *
 * ⚠️ 扫压必须走这个,不能走 @ref cv_bias_tenths_to_code。
 *    索引间隔在低压段每步增量会小于 0.1 V,先取整到 0.1 V 再换算,
 *    最前面一串点会**塌成同一个 0.1 V**(实测:60 点里前 6 点重复),
 *    而低压段恰恰是 C-V 斜率最大、最不该丢点的地方。
 *    0.1 V 量化只该用在**界面显示**上。
 */
static inline uint32_t cv_bias_volts_to_code(float volts)
{
    float module_v = volts / CV_BIAS_HV_RATIO;
    float raw      = (module_v + 10.0f) * 1048575.0f / 20.0f;

    if (raw < 0.0f)       { raw = 0.0f; }
    if (raw > 1048575.0f) { raw = 1048575.0f; }
    return (uint32_t)(raw + 0.5f);
}

/**
 * @brief 0.1 V 计数 → 偏压码。界面的 0.1 V 栅格用这个。
 * @note  `tenths = 0` → `0x80000`(0 V);`1000` → `0xFFFFF`(+100 V)。
 */
static inline uint32_t cv_bias_tenths_to_code(uint32_t tenths)
{
    return cv_bias_volts_to_code((float)tenths / 10.0f);
}

/** @brief 偏压码 → 偏压(V),全精度。 */
static inline float cv_bias_code_to_volts(uint32_t code)
{
    float module_v = -10.0f +
        (20.0f * (float)(code & CV_BIAS_CODE_MAX) / 1048575.0f);

    return module_v * CV_BIAS_HV_RATIO;
}

/**
 * @brief 偏压码 → 0.1 V 计数(四舍五入)。把回读值变成人看的数字。
 * @note  负偏压返回 0 —— 界面的量程是正向半轴,负半轴不往外显示。
 */
static inline uint32_t cv_bias_code_to_tenths(uint32_t code)
{
    float tenths = cv_bias_code_to_volts(code) * 10.0f;

    return (tenths <= 0.0f) ? 0u : (uint32_t)(tenths + 0.5f);
}

/* 0x80 [27:0] → adc_9833.code:f_out = code × MCLK / 2^28
 * ⚠️ MCLK 是 AD9833 **板载焊死的 25 MHz 晶振**,FPGA 供不了(决策 D10)
 *    ⇒ 这里只能写常量。换晶体就要改这一行 + cv_regs.py 的 DDS_MCLK_HZ。 */
#define CV_DDS_MCLK_HZ       25000000u
#define CV_DDS_FREQ_1MHZ     0x0A3D70Au    /* 10737418 → 999 999.98 Hz */

/** @brief 频率 → 28 位频率字。 */
static inline uint32_t cv_dds_hz_to_code(uint32_t hz)
{
    return (uint32_t)(((double)hz * 268435456.0) / (double)CV_DDS_MCLK_HZ + 0.5);
}

/* ------------------------------------------------------------------ */
/* 面板                                                                */
/* ------------------------------------------------------------------ */
#define CV_PANEL_W           1024
#define CV_PANEL_H           600
#define CV_FB_FRAME_BYTES    (CV_PANEL_W * CV_PANEL_H * 2)   /* 1 228 800 */
#define CV_FB_ALIGNMENT      0x10000u                        /* 64 KB */

/* ------------------------------------------------------------------ */
/* 接口                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 打开 /dev/mem 并映射 AXI 寄存器窗口。
 * @param axi_base AXI 从机物理基址(本项目 0x43C00000)
 * @return 0 成功;负数为错误码(-1 打不开 /dev/mem,-2 mmap 被拒)
 */
int cv_regs_init(uint32_t axi_base);

/** @brief 收掉寄存器映射。 */
void cv_regs_close(void);

/** @brief 读 32 位寄存器。@param off 字节偏移(CV_REG_*) */
uint32_t cv_reg_read(uint32_t off);

/** @brief 写 32 位寄存器。 */
void cv_reg_write(uint32_t off, uint32_t val);

/**
 * @brief 把一段**物理**内存映射进来(非缓存)。
 *
 * 用来映射 Python 分配好的 DDR 帧缓存。`phys` 必须是页对齐的
 * (帧缓存要求 64 KB 对齐,天然满足)。
 *
 * @return 映射到的指针;失败返回 NULL 并置 errno。
 */
void *cv_phys_map(uint32_t phys, uint32_t len);

/** @brief 解除映射。 */
void cv_phys_unmap(void *p, uint32_t len);

/**
 * @brief 请求换址并**等它生效**。
 *
 * 🔴 不等的话调用方会以为"画完了",而 PS 往往接着去写另一块 ——
 *    那一块此刻还在被扫描。等 `pending` 落下 = 扫描已经翻到新前台。
 *
 * @param target     目标缓冲索引(0/1)
 * @param timeout_ms 超时(一帧 17.07 ms,给 60 ms 足够)
 * @return 0 成功;负数为 -1 超时 / -2 参数错
 */
int cv_lcd_swap_wait(int target, int timeout_ms);

/** @brief 取单调时钟毫秒数(替代 STM32 的 HAL_GetTick)。 */
uint32_t cv_millis(void);

/** @brief 睡眠 @p ms 毫秒。 */
void cv_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* PYNQ_REGS_H */
