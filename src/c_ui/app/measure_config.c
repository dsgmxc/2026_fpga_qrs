/**
 * @file measure_config.c
 * @brief @ref measure_config.h 的实现 —— 激励频率 `0x80` 与 采样率 + 相位 `0x88`
 *
 * 【为什么写完要回读核对】
 *   和偏压那条路一样(见 `pynq_protocol.c::bias_write_code`):AXI 通路出问题时,
 *   写下去**不报错**、寄存器里还是旧值。回读一次就能把"界面点了没反应"和
 *   "硬件没接通"分开 —— 这是判"寄存器写没写进去"最快的证据。
 *   失败只打一次提示(`s_verify_notice`),不刷屏。
 *
 * 【踩过的坑,别重犯】
 *   ⛔ 读 `0x88` 的采样缓冲(0x94/0x98)之前**必须等 `0x90.done`**。
 *      采集期间读回来的是**上一次**的残留,看起来和"没生效"一模一样。
 *      (2026-10-06 在这个坑上误判过一次"OEB 无效"。)
 *      本文件不碰缓冲,只碰配置寄存器,所以不受影响 —— 但改这个文件时要知道。
 */

#include "measure_config.h"

#include "pynq_regs.h"
#include "ui_demo.h"

#include <stdio.h>

/* ==========================================================================
 * 状态
 * ========================================================================== */

static uint32_t s_dds_hz   = CV_DDS_HZ_DEFAULT;
static uint32_t s_div      = CV_ADC_DIV_DEFAULT;
static int32_t  s_phase;                /* 上电由 Init 按中心算,不猜 */

/* 临时相位(push/pop)。`s_phase` 始终是**逻辑值**,这两个只记"推入前的逻辑值"
 * 和"当前是否处于推入态"。 */
static int32_t  s_phase_saved;
static int      s_phase_pushed;

static uint32_t s_verify_fail;
static int      s_verify_notice;        /* 回读失败提示只打一次 */

/* ==========================================================================
 * 小工具
 * ========================================================================== */

/** @brief 分频比夹到寄存器字段能表示的范围,且与 `adc_capture.v` 的 `P = max(div,2)` 一致。 */
static uint32_t clamp_div(uint32_t div)
{
    if (div < 2u)   { return 2u; }
    if (div > 255u) { return 255u; }
    return div;
}

/**
 * @brief 端口 A **数据稳定**的采样窗(闭区间,tick 单位)。⚠️ 不是"寄存器可写范围"。
 *
 * 两个范围别混(本文件一度写混过,记在这里):
 *
 * | 范围 | tick 区间 | P=13 | 越界后果 |
 * |---|---|---|---|
 * | **寄存器合法值**(`adc_capture.v:238`) | `[−P/2, P−1−P/2]` | `[−6, +6]` | 永不命中 ⇒ 采不到,`done` 不置位 —— **失败方向安全** |
 * | **数据稳定窗**(本函数) | `[2 − P/2, +1]` | `[−4, +1]` | 在合法范围内但落在数据未稳定的拍上 ⇒ **采到垃圾值,不报错** |
 *
 * 守卫**必须用窄的这一个** —— 越界的两种失败里,只有窄窗能挡住
 * "寄存器一切正常、回读全对,但数据是错的"那种最难查的。
 *
 * `tick = cnt − P/2`(相对 `adc_clk` 下降沿),中心 = `1.5 − P/4`。
 *
 * 🔴 **本式是本模块唯一的 PH_OFF 来源(是公式,不是查表)** —— 它由 AD9238
 *    数据手册的输出时序推出,**与接线无关** ⇒ 2026-10-07 换线**不动这一行**。
 * ⚠️ **但 2026-10-06 那三点实测吻合(DIV=13 → −2、26 → −5、130 → −31)
 *    是在【换线前】的接线下取的,已作废。** 手册窗是"相对 ADC 自己的时钟脚",
 *    而 FPGA 看到的是它**再加一个来回传播延迟**(输出脚出去 + 数据脚回来,
 *    单位 7.69 ns/tick)。旧接线凑巧落进 ±0.5 tick 才逐点对上。
 *    ⇒ 换线后若实测中心整体平移 ≥1 tick,说明来回延迟已不可忽略,那时
 *    **本函数的窗就不再是真实稳定窗**,守卫会放行"回读全对、数据是错的"那种
 *    垃圾值。上板必须重扫确认(见 `led.xdc` ADC 段末尾同一条待办)。
 */
static void adc_window(uint32_t div, int32_t *lo, int32_t *hi)
{
    uint32_t p    = clamp_div(div);
    int32_t  half = (int32_t)(p >> 1);   /* P/2 = adc_clk 下降沿刻度,也正好是窗宽 */

    *lo = 2 - half;
    *hi = 1;

    /* 🔴 **P=2 折回**(2026-10-07 实测补)。
     *   P=2 时窗 = [1,1],而寄存器合法范围是 [−1, 0] —— **两者不相交**,
     *   于是 `MeasureConfig_NudgePhase()` 会把刻度钳到 1,一个采不到数的位置。
     *   绕回一周即 [−1,−1],正是实测的端口 A 刻度。**与 `cv_adc_phase_center()`
     *   用同一条折回**,两处必须同时改,不然中心在窗内、窗却装不下中心。
     *   ⚠️ 只动 P=2:上界越界仅此一档(P=3/4 时 `P−1−P/2` 正好 = 1)。 */
    if (*hi > (int32_t)p - 1 - half) {
        *lo -= (int32_t)p;
        *hi -= (int32_t)p;
    }
}

/** @brief 回读不一致的提示(只打一次)。 */
static void verify_notice(const char *what, uint32_t wrote, uint32_t back)
{
    s_verify_fail++;
    if (s_verify_notice) {
        return;
    }
    s_verify_notice = 1;
    fprintf(stderr,
            "[mc] ⚠️ %s 回读不一致:写 0x%08X,读回 0x%08X\n"
            "     ⇒ AXI 通路有问题。依次查:\n"
            "       ① --axi-base 是不是 0x43C00000;\n"
            "       ② 位流是不是含 AD9238 采集那一版(0x88–0x98 要有响应);\n"
            "       ③ 读 0x90(ADC_STS)看 [0]mmcm_locked —— 恒 0 说明 MMCM 没锁。\n",
            what, (unsigned)wrote, (unsigned)back);
}

/** @brief 写 `0x88`(DIV + PH_OFF 一起写)并回读。@return 0 一致 / 1 不一致。 */
static uint32_t adc_cfg_write(void)
{
    uint32_t word = cv_adc_cfg_word(s_div, s_phase);
    uint32_t back;

    cv_reg_write(CV_REG_ADC_CFG, word);

    back = cv_reg_read(CV_REG_ADC_CFG);
    if (back != word) {
        verify_notice("ADC_CFG(0x88)", word, back);
        return 1u;
    }
    return 0u;
}

/** @brief 写 `0x80`(激励频率码)并回读。@return 0 一致 / 1 不一致。 */
static uint32_t dds_write(void)
{
    uint32_t code = cv_dds_hz_to_code(s_dds_hz);
    uint32_t back;

    cv_reg_write(CV_REG_DDS_FREQ, code);

    back = cv_reg_read(CV_REG_DDS_FREQ);
    if (back != code) {
        verify_notice("DDS_FREQ(0x80)", code, back);
        return 1u;
    }
    return 0u;
}

/** @brief 把当前参数拼一行推给界面常驻状态行。 */
static void refresh_line(void)
{
    char line[96];

    MeasureConfig_FormatLine(line, sizeof(line));
    UI_Demo_SetInitStatusText(line);
}

/* ==========================================================================
 * 接口
 * ========================================================================== */

void MeasureConfig_FormatLine(char *buf, size_t cap)
{
    if ((buf == NULL) || (cap == 0u)) {
        return;
    }
    /* 采样率用两位小数:2026-10-08 起固定 65.00(DIV=2)。
     * (原注是"10.00 / 13.00 一眼看出是哪一档" —— 采样率不可调后已失去前提,
     *  但**两位小数保留**:它对将来任何档位都成立,且这行是屏上唯一显示
     *  采样率的地方,见 measure_config.h 的说明。)
     * 相位带符号:`%+d` ⇒ "+1" / "-2",不然负数看着像减号丢了。 */
    (void)snprintf(buf, cap,
                   "激励 %.3fMHz   采样 %.2fMSPS(DIV%u)   相位 %+d",
                   (double)s_dds_hz / 1000000.0,
                   (double)cv_adc_sample_hz(s_div) / 1000000.0,
                   (unsigned)s_div, (int)s_phase);
}

void MeasureConfig_Init(void)
{
    uint32_t e1;
    uint32_t e2;
    uint32_t ctrl;
    int32_t  lo;
    int32_t  hi;

    s_dds_hz = CV_DDS_HZ_DEFAULT;
    s_div    = CV_ADC_DIV_DEFAULT;
    s_phase  = cv_adc_phase_center(s_div);   /* ★ 用窗中心,不用 0 */

    s_phase_saved   = s_phase;
    s_phase_pushed  = 0;                     /* 上电不可能处于推入态 */
    s_verify_fail   = 0u;
    s_verify_notice = 0;

    e1 = dds_write();
    e2 = adc_cfg_write();
    refresh_line();

    adc_window(s_div, &lo, &hi);

    printf("[mc] 测量参数已下发:\n"
           "       激励   %.3f MHz   → 0x80 = 0x%05X\n"
           "       采样   %.2f MSPS  → 0x88 = 0x%05X  (DIV=%u,PH_OFF=%+d,窗 [%+d,%+d])\n"
           "       (%s)\n",
           (double)s_dds_hz / 1000000.0, (unsigned)cv_dds_hz_to_code(s_dds_hz),
           (double)cv_adc_sample_hz(s_div) / 1000000.0,
           (unsigned)cv_adc_cfg_word(s_div, s_phase),
           (unsigned)s_div, (int)s_phase, (int)lo, (int)hi,
           ((e1 | e2) == 0u) ? "回读一致" : "⚠️ 回读不一致,见上");

    /* 🔴 0x84 只读不写 —— 放行权归 Python(`drivers/board.py::release_dac()`)。
     *    这里把它的**当前值喊出来**,不代它做决定:没放行的话偏压码会被硬件
     *    收下(回读还正常),但**到不了 DAC 输出**,停下来的是芯片默认的 −10 V。 */
    ctrl = cv_reg_read(CV_REG_CV_CTRL);
    if ((ctrl & CV_CTRL_DAC_RST_N) == 0u) {
        printf("[mc] 🔴 0x84 = 0x%08X ⇒ dac_1220 仍被按在复位态(DAC 输出停在 −10 V)。\n"
               "     放行由 Python 侧做(必须先 apply_fclk0() 把 FCLK0 补到 50 MHz),"
               "本模块不碰这一位。\n", (unsigned)ctrl);
    }
}

bool MeasureConfig_SetDdsHz(uint32_t hz)
{
    uint32_t old = s_dds_hz;

    if (hz < CV_DDS_HZ_MIN) { hz = CV_DDS_HZ_MIN; }
    if (hz > CV_DDS_HZ_MAX) { hz = CV_DDS_HZ_MAX; }

    s_dds_hz = hz;
    if (dds_write() != 0u) {
        /* 回读不一致 ⇒ 状态不可信。但**保留尝试值**:写可能已经落地了,
         * 把 s_ 退回旧值反而会让界面显示的和硬件里的不一致。
         * 与 `pynq_protocol.c::bias_write_code` 的处置一致。 */
        refresh_line();
        return false;
    }

    refresh_line();
    printf("[mc] 激励 → %.3f MHz(0x80 = 0x%05X)%s\n",
           (double)s_dds_hz / 1000000.0, (unsigned)cv_dds_hz_to_code(s_dds_hz),
           (s_dds_hz == old) ? "  (未变化)" : "");
    return true;
}

bool MeasureConfig_SetSampleDiv(uint32_t div)
{
    int32_t old_phase = s_phase;
    int32_t lo;
    int32_t hi;

    s_div   = clamp_div(div);
    s_phase = cv_adc_phase_center(s_div);    /* ★ 窗口跟着分频比走,一起写 */

    if (adc_cfg_write() != 0u) {
        refresh_line();
        return false;
    }

    adc_window(s_div, &lo, &hi);
    refresh_line();
    printf("[mc] 采样 → %.2f MSPS(DIV=%u,PH_OFF %+d → %+d,窗 [%+d,%+d])\n",
           (double)cv_adc_sample_hz(s_div) / 1000000.0, (unsigned)s_div,
           (int)old_phase, (int)s_phase, (int)lo, (int)hi);
    return true;
}

bool MeasureConfig_NudgePhase(int32_t delta)
{
    int32_t lo;
    int32_t hi;
    int32_t want;

    adc_window(s_div, &lo, &hi);
    want = s_phase + delta;

    if ((want < lo) || (want > hi)) {
        /* 走出来就**不写**。硬写下去采样点落在未被驱动的拍上,
         * 现象是"波形突然没了",而寄存器回读一切正常 —— 最难查的那种。 */
        printf("[mc] 相位 %+d 越出采样窗 [%+d,%+d],未改(当前 %+d)\n",
               (int)want, (int)lo, (int)hi, (int)s_phase);
        return false;
    }

    s_phase = want;
    if (adc_cfg_write() != 0u) {
        refresh_line();
        return false;
    }

    refresh_line();
    printf("[mc] 相位 → %+d(窗 [%+d,%+d])\n", (int)s_phase, (int)lo, (int)hi);
    return true;
}

bool MeasureConfig_CenterPhase(void)
{
    int32_t want = cv_adc_phase_center(s_div);

    if (want == s_phase) {
        refresh_line();
        printf("[mc] 相位已在窗口中心 %+d,未改\n", (int)s_phase);
        return true;
    }

    s_phase = want;
    if (adc_cfg_write() != 0u) {
        refresh_line();
        return false;
    }

    refresh_line();
    printf("[mc] 相位 → %+d(回到 DIV=%u 的窗口中心)\n",
           (int)s_phase, (unsigned)s_div);
    return true;
}

bool MeasureConfig_PushPhase(int32_t ph_off)
{
    uint32_t p    = clamp_div(s_div);
    int32_t  half = (int32_t)(p >> 1);
    /* ★ 这里用**寄存器合法范围**,不是 `adc_window()` 的数据稳定窗 ——
     *   刻意要走到刻度两端,理由见 `measure_config.h` 上这一对的说明。 */
    int32_t  lo   = 0 - half;                 /* = −P/2      */
    int32_t  hi   = (int32_t)p - 1 - half;    /* = P−1−P/2   */
    uint32_t word;
    uint32_t back;

    if (s_phase_pushed) {
        fprintf(stderr, "[mc] ⚠️ PushPhase 嵌套调用 —— 外层的保存值会被覆盖,已拒绝"
                        "(当前仍停在 PH_OFF=%+d)\n", (int)s_phase_saved);
        return false;
    }
    if ((ph_off < lo) || (ph_off > hi)) {
        fprintf(stderr, "[mc] ⚠️ PushPhase:PH_OFF %+d 越出寄存器合法范围 "
                        "[%+d,%+d](DIV=%u),未写\n",
                (int)ph_off, (int)lo, (int)hi, (unsigned)s_div);
        return false;
    }

    s_phase_saved  = s_phase;
    s_phase_pushed = 1;

    word = cv_adc_cfg_word(s_div, ph_off);
    cv_reg_write(CV_REG_ADC_CFG, word);
    back = cv_reg_read(CV_REG_ADC_CFG);
    if (back != word) {
        verify_notice("ADC_CFG(0x88)", word, back);
        /* 回读不一致 ⇒ 硬件里是什么不知道,别留一个"推入态"骗后面的 Pop */
        s_phase_pushed = 0;
        return false;
    }
    return true;
}

bool MeasureConfig_PopPhase(void)
{
    uint32_t word;
    uint32_t back;

    if (!s_phase_pushed) {
        return true;            /* 幂等:没推入过,硬件本来就是对的 */
    }
    s_phase_pushed = 0;

    word = cv_adc_cfg_word(s_div, s_phase_saved);
    cv_reg_write(CV_REG_ADC_CFG, word);
    back = cv_reg_read(CV_REG_ADC_CFG);
    if (back != word) {
        verify_notice("ADC_CFG(0x88)", word, back);
        return false;
    }
    return true;
}

bool MeasureConfig_RestoreDefaults(void)
{
    uint32_t e1;
    uint32_t e2;
    int32_t  lo;
    int32_t  hi;

    s_dds_hz = CV_DDS_HZ_DEFAULT;
    s_div    = CV_ADC_DIV_DEFAULT;
    s_phase  = cv_adc_phase_center(s_div);

    /* 复位把逻辑相位也重置了 ⇒ 任何在途的推入态作废,否则下一次 Pop
     * 会把一个陈旧的 s_phase_saved 写回硬件。 */
    s_phase_pushed = 0;
    s_phase_saved  = s_phase;

    /* ⚠️ 两次写分开调,不用 `&&` —— 短路会让第二次写被跳过,
     *    而"激励回默认了、采样率没回"是最难发现的那种半复位。 */
    e1 = dds_write();
    e2 = adc_cfg_write();

    refresh_line();
    adc_window(s_div, &lo, &hi);

    printf("[mc] 参数复位:激励 %.3f MHz(0x80 = 0x%05X),"
           "采样 %.2f MSPS(0x88 = 0x%05X,DIV=%u,PH_OFF=%+d,窗 [%+d,%+d])  (%s)\n",
           (double)s_dds_hz / 1000000.0, (unsigned)cv_dds_hz_to_code(s_dds_hz),
           (double)cv_adc_sample_hz(s_div) / 1000000.0,
           (unsigned)cv_adc_cfg_word(s_div, s_phase),
           (unsigned)s_div, (int)s_phase, (int)lo, (int)hi,
           ((e1 | e2) == 0u) ? "回读一致" : "⚠️ 回读不一致,见上");
    printf("[mc] ⚠️ 0x84(CV_CTRL)未被动过 —— DAC 放行权归 Python 侧,复位不含它\n");

    return ((e1 | e2) == 0u);
}

uint32_t MeasureConfig_GetDdsHz(void)
{
    return s_dds_hz;
}

uint32_t MeasureConfig_GetSampleDiv(void)
{
    return s_div;
}

int32_t MeasureConfig_GetPhase(void)
{
    return s_phase;
}

bool MeasureConfig_IsPhaseCentered(void)
{
    return (s_phase == cv_adc_phase_center(s_div));
}
