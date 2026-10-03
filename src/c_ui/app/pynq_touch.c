/**
 * @file pynq_touch.c
 * @brief 触摸数据生产者(PYNQ 版)—— 替换前序的 `lcd_touch.c`
 *
 * 接口在 `port/BSP/TOUCH/lcd_touch.h`。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【本文件只做三件事】读寄存器 → 判新旧 → 填结构体。
 *   I2C 时序、GT911 复位/配置、0x814E 状态位、150 ms 快轮询窗口、
 *   两级错误容忍、器件地址自愈 —— **全在 PL 里**(`i2c_master.v` +
 *   `gt911_ctrl.v`)。PS 侧**不要**重复实现其中任何一条。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 🔴 【读序:0x68 → 0x6C → 0x70,不可调换】
 *   读 `0x68` 那一拍是**快照提交点**,从机同时把 pt0/pt1 锁进 hold。
 *   先读坐标再读状态会拿到上一拍快照,与 `seq` 对不上 ——
 *   症状是"状态说 1 个点、坐标却是下一帧"的**撕裂**:不报错,界面偶尔跳。
 *   出处:`cv_axi_lite_slave.v:347-349`(`tch_pt0_hold` / `tch_pt1_hold`)。
 *
 * 🔴 【`seq` 不变 ≠ 松手】
 *   手指按着不动时 GT911 不产生新帧,`seq` 就停住。
 *   PL 在这段时间里**保持** `pen_down` / `count`(gt911_ctrl.v:532 那个分支),
 *   所以本文件遇到 `seq` 不变时返回 `LCD_TOUCH_NO_UPDATE` 并把**保持态**
 *   填进 `*state` —— 调用方必须原样沿用上次结论,**绝不能**当成抬起。
 *
 * ⚠️ 【PL 只导出 2 个触点,且不含 track_id】
 *   `point[i].id` 只能填 i。前序 STM32 版是读 GT911 的 track_id 的,
 *   所以"三指以上"和"哪根手指是哪根"的语义**与 STM32 版不同**。
 *   如实记在报告里,不要当成等价实现。
 */

#include "BSP/TOUCH/lcd_touch.h"

#include "pynq_regs.h"

#include <stdio.h>
#include <string.h>

/* PL 侧 gt911_ctrl 的 N_PT 参数。当前是 2 —— 改 PL 时这里要跟着改。 */
#define PL_MAX_POINTS   2

static LCD_TouchState  s_state;          /* 最近一次的**保持态** */
static LCD_TouchInfo   s_info;
static uint32_t        s_last_seq;
static int             s_seq_valid;      /* 首次读时 seq 无参照 */
static uint32_t        s_last_raw_status;/* 保留:排障时可看裸值 */
static uint32_t        s_error_count;
static int             s_err_reported;   /* 粘滞位只报一次,见 LCD_Touch_Read */

/* ------------------------------------------------------------------ */
/* 内部:把 12 位坐标做面板边界检查                                         */
/* ------------------------------------------------------------------ */
static void fill_point(LCD_TouchPoint *dst, uint32_t pt_raw, uint8_t id)
{
    uint16_t x = (uint16_t)CV_TOUCH_X(pt_raw);
    uint16_t y = (uint16_t)CV_TOUCH_Y(pt_raw);

    /* ⚠️ x/y 各只有 12 位 ⇒ 越界的形态是 1024~4095。
     *    **不做缩放** —— 那会把"GT911 配置表分辨率写错"这件事
     *    伪装成"能用了",以后再也查不出来。
     *    这里只如实记录 + 打印一次,让 touch_probe 能看见。 */
    if (x >= LCD_PIXEL_WIDTH || y >= LCD_PIXEL_HEIGHT) {
        static int warned = 0;
        if (!warned) {
            warned = 1;
            fprintf(stderr,
                    "[touch] 坐标越界:(%u,%u),面板是 %dx%d。\n"
                    "        ⇒ 几乎可以确定是 GT911 配置表的分辨率字段不对,\n"
                    "          不是 PS 侧能修的。**不要**在这里加缩放。\n",
                    x, y, LCD_PIXEL_WIDTH, LCD_PIXEL_HEIGHT);
        }
    }

    dst->x     = x;
    dst->y     = y;
    dst->id    = id;      /* PL 不导出 track_id,只能按序号填(见文件头) */
    dst->event = 0;       /* GT911 的事件类型 PL 未导出;ui_demo 不依赖它 */
}

/* ------------------------------------------------------------------ */
uint32_t LCD_Touch_Init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    memset(&s_info, 0, sizeof(s_info));
    s_last_seq     = 0;
    s_seq_valid    = 0;
    s_error_count  = 0;
    s_err_reported = 0;

    /* ⚠️ 这里**不做硬件动作**:GT911 的复位序列(20/6/100 ms)由 PL 在
     *    上电时就跑完了,PS 再插一脚只会把总线搞乱。
     *    前序的 LCD_Touch_Init 是"扫 I2C 找器件地址 + 配置芯片",
     *    那两件事在 PL 里(i2c_master 有地址自愈、gt911_ctrl 有复位序列)。
     *    所以这里只做一次寄存器读,用来确认链路是活的。 */
    uint32_t st = cv_reg_read(CV_REG_TOUCH_STATUS);

    /* PL 侧保留位 [7:5] / [31:16] 应为 0。不是 0 说明寄存器契约变了。 */
    if ((st & ~CV_TOUCH_MASK_KNOWN) != 0u) {
        fprintf(stderr,
                "[touch] 0x68 = 0x%08X,保留位非 0(掩码 0x%08X)。\n"
                "        ⇒ 寄存器契约与 cv_regs.py 不一致,先核对 RTL。\n",
                st, CV_TOUCH_MASK_KNOWN);
    }

    s_info.communication_errors = CV_TOUCH_ERR(st);
    /* 其余字段 PL 不导出,保持 0 —— 见 lcd_touch.h 的说明,不要印出来当"读到了" */

    printf("[touch] 链路检查:0x68 = 0x%08X(seq=%u count=%u pen=%u err=%u)\n",
           st, CV_TOUCH_SEQ(st), CV_TOUCH_COUNT(st), CV_TOUCH_PEN(st), CV_TOUCH_ERR(st));
    return LCD_TOUCH_OK;
}

/* ------------------------------------------------------------------ */
uint32_t LCD_Touch_Read(LCD_TouchState *state)
{
    if (state == NULL) {
        return LCD_TOUCH_ERROR_DATA;
    }

    /* ★ 固定读序,三行不许动、不许拆(见文件头) */
    uint32_t raw_status = cv_reg_read(CV_REG_TOUCH_STATUS);
    uint32_t raw_pt0    = cv_reg_read(CV_REG_TOUCH_PT0);
    uint32_t raw_pt1    = cv_reg_read(CV_REG_TOUCH_PT1);
    s_last_raw_status   = raw_status;

    const uint32_t seq   = CV_TOUCH_SEQ(raw_status);
    const uint32_t count = CV_TOUCH_COUNT(raw_status);
    const uint32_t pen   = CV_TOUCH_PEN(raw_status);
    const uint32_t err   = CV_TOUCH_ERR(raw_status);

    /* ---- 粘滞错误位 ---------------------------------------------------
     * 🔴 **`err` 置起后只有重新配置 PL 才清**(`gt911_ctrl.v:264` 是唯一的
     *    清零点)。所以它回答的是"**开机以来曾经**出过错",**不是**"此刻在错"。
     *
     *    ⇒ **绝不能**每次读都因为它是 1 就返回 BUS:那样一次上电噪声会让
     *      UI 永久认为触摸坏了,之后手指怎么按都没反应 —— 而其实一切正常。
     *
     *    ⇒ 本实现只在**第一次**看到它置起时报一次 BUS(让上层知道发生过),
     *      之后照常走正常流程。真正的"此刻是否在错"需要 PL 提供一个可读的
     *      **连续错误计数**,PL 目前没有(那是 RTL 改动,超出本轮范围)。
     *      这一条如实记在报告里。 */
    if (err) {
        s_error_count++;
        s_info.communication_errors = s_error_count;
        if (!s_err_reported) {
            s_err_reported = 1;
            fprintf(stderr,
                    "[touch] PL 报过总线错(err 粘滞位置起,0x68=0x%08X)。\n"
                    "        这是**历史累计**标志,只有重新配置 PL 才清 ——\n"
                    "        本次只报告一次,之后正常读。若触摸一直无反应,\n"
                    "        那才需要查 I2C 上拉 / GT911 底板。\n", raw_status);
            memcpy(state, &s_state, sizeof(*state));
            return LCD_TOUCH_ERROR_BUS;
        }
    }

    const int fresh = (!s_seq_valid) || (seq != s_last_seq);
    s_seq_valid = 1;
    s_last_seq  = seq;

    if (fresh) {
        /* 新的一帧:把保持态整个换掉 */
        memset(&s_state, 0, sizeof(s_state));
        if (pen && count > 0) {
            uint32_t n = count > PL_MAX_POINTS ? PL_MAX_POINTS : count;
            s_state.count = (uint8_t)n;
            fill_point(&s_state.point[0], raw_pt0, 0);
            if (n >= 2) {
                fill_point(&s_state.point[1], raw_pt1, 1);
            }
        } else {
            /* count == 0 且是**新帧** ⇒ 这才是明确释放 */
            s_state.count = 0;
        }
    }

    memcpy(state, &s_state, sizeof(*state));

    /* ⚠️ seq 没变时返回 NO_UPDATE,而**不是 OK** ——
     *    调用方据此知道"这不是新事件",从而不去重算手势。
     *    前序的枚举语义就是如此,照搬。 */
    return fresh ? LCD_TOUCH_OK : LCD_TOUCH_NO_UPDATE;
}

/* ------------------------------------------------------------------ */
uint32_t LCD_Touch_ResetController(void)
{
    /* ⚠️ **目前是空实现,返回 UNSUPPORTED** —— 不假装成功。
     *
     * 复位 GT911 需要同时摆布 INT 与 RST 两根线(时序 20/6/100 ms),
     * 而那两根线接在 PL 上、由 `gt911_ctrl` 的复位状态机独占。
     * PS 侧没有任何寄存器能触发它(那个状态机在连错 5 次时会自己重跑,
     * 见 gt911_ctrl.v 的 ER_RST 分支)。
     *
     * ⇒ 要让 PS 也能主动触发,得**在 PL 里加一个 CTRL 位**
     *   (比如 TOUCH_STATUS 旁边的写寄存器),那是 RTL 改动,
     *   属于本轮范围之外。**先如实返回不支持,不要返回 OK 骗调用方。** */
    fprintf(stderr, "[touch] ResetController 未实现:PL 没有暴露复位触发位。\n"
                    "        (gt911_ctrl 连错 5 次会自己重跑复位序列)\n");
    return LCD_TOUCH_ERROR_UNSUPPORTED;
}

/* ------------------------------------------------------------------ */
const LCD_TouchInfo *LCD_Touch_GetInfo(void)
{
    return &s_info;
}

/* ------------------------------------------------------------------ */
/* 排障用:本文件不导出到 lcd_touch.h(接口是前序的,不加新函数污染它)。   */
/* main.c 通过 pynq_touch_debug_line() 取一行字符串打印。                  */
/* ------------------------------------------------------------------ */
int pynq_touch_debug_line(char *buf, unsigned len)
{
    return snprintf(buf, len,
                    "seq=%u count=%u pen=%u err=%u raw68=0x%08X",
                    (unsigned)CV_TOUCH_SEQ(s_last_raw_status),
                    (unsigned)CV_TOUCH_COUNT(s_last_raw_status),
                    (unsigned)CV_TOUCH_PEN(s_last_raw_status),
                    (unsigned)CV_TOUCH_ERR(s_last_raw_status),
                    s_last_raw_status);
}
