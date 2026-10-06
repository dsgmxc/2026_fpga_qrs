/**
 * @file adc_sample.c
 * @brief @ref adc_sample.h 的实现 —— 采一次缓冲 + 正交解调
 *
 * 【本文件的编写纪律】
 *   ⛔ **不直接写 `0x88`。** 那个寄存器归 `measure_config.c` 独占
 *      (见 `measure_config.h` 的文件头)。改采样刻度一律走
 *      `MeasureConfig_PushPhase()` / `MeasureConfig_PopPhase()`。
 *   ⛔ **不写 `0x84`(CV_CTRL)。** DAC 放行权归 Python 侧。
 *   ✅ 只碰 `0x8C`(GO)、`0x90`(状态)、`0x94`/`0x98`(缓冲)。
 *
 * 【踩过的坑,别重犯】
 *   ⛔ **`done` 必须先清后等。** 幸好不需要软件去清:RTL 在 `go_pulse` 上
 *      **同时** `done <= 0`,所以"写完 GO 再等 done=1"等到的必定是本次的。
 *      但**不能省掉"等 busy=1"这一步** —— 少了它,首次轮询可能读到上一次
 *      留下的 `done=1`(CDC 要几个周期),然后读出**上一次**的数据。
 *      这正是 `measure_config.c` 文件头记的那个坑的另一种触发方式。
 *   ⛔ **失败路径也必须 PopPhase。** 漏掉的话硬件停在临时刻度上,而界面
 *      显示的是逻辑值 —— 没有症状,下次测量才发现。
 */

#include "adc_sample.h"

#include "measure_config.h"
#include "pynq_regs.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * 参数
 * ========================================================================== */

/* 0x90 的**掩码**。`pynq_regs.h` 里的 `CV_ADC_STS_*()` 宏要的是一个完整的
 * 状态字(内部自带移位),这里要的是掩码,所以另立常量而不是硬凑那个宏。 */
#define ADC_STS_LOCKED_MASK   0x1u
#define ADC_STS_DONE_MASK     0x2u
#define ADC_STS_BUSY_MASK     0x4u
#define ADC_STS_OTR_MASK      0x8u

/* 等待超时(ms)。`cv_millis()` 只有 1 ms 分辨力,所以这几个数要**明显大于**
 * 真实耗时,不然会把"正常但慢"判成超时。真实耗时量级:
 *   收满 4096 点:1 MSPS 档(DIV=130)最长 = 4096 µs ≈ 4.1 ms
 *   最慢档(DIV=2 → 65 MHz)只要 63 µs
 * ⇒ done 给 200 ms 是 ~50 倍余量,不会误报。 */
#define ADC_WAIT_IDLE_MS      50u
#define ADC_WAIT_GO_MS        50u
#define ADC_WAIT_DONE_MS      200u

/** 解调参考表的点数上限。DIV=2(65 MHz)时每周期有 65 点,取 64 不够 —— 但那一档
 *  本来就采不出可用波形(见 `adc_capture.v` 文件头),所以只需覆盖**实际会用**的
 *  档位:DIV=5 → 26 点、DIV=13 → 10 点、DIV=26 → 5 点。64 留了足够余量。 */
#define ADC_DEMOD_MAX_N       64u

#define ADC_TWO_PI            6.28318530717958647692

/* ==========================================================================
 * 状态
 * ========================================================================== */

/* 🔴 320 不是随手写的:下面"超时诊断"和"每周期点数不足"两条消息
 *    最长可达 ~261 / ~249 字节(算上 `%s` 展开的那个 0x90 状态串)。
 *    原来写 192,`-Wformat-truncation` 实测报**截断** —— 恰好在这种
 *    "就靠这行字定位"的时候把最关键的尾句吃掉。加宽到 320 留余量。 */
static char     s_err[320];
static uint32_t s_tab_n;                            /* 表是按哪个 N 建的 */
static float    s_tab_cos[ADC_DEMOD_MAX_N];
static float    s_tab_sin[ADC_DEMOD_MAX_N];

/* ==========================================================================
 * 小工具
 * ========================================================================== */

static void set_err(const char *msg)
{
    if (msg == NULL) {
        s_err[0] = '\0';
        return;
    }
    (void)snprintf(s_err, sizeof(s_err), "%s", msg);
}

void AdcSample_SetErrorf(const char *fmt, ...)
{
    va_list ap;

    if (fmt == NULL) {
        s_err[0] = '\0';
        return;
    }
    va_start(ap, fmt);
    (void)vsnprintf(s_err, sizeof(s_err), fmt, ap);
    va_end(ap);
}

void AdcSample_ClearError(void)
{
    s_err[0] = '\0';
}

const char *AdcSample_LastError(void)
{
    return (s_err[0] != '\0') ? s_err : "无错误";
}

static void sts_dump(char *buf, size_t cap, uint32_t st)
{
    (void)snprintf(buf, cap,
                   "0x90 = 0x%08X(locked=%u done=%u busy=%u otr=%u)",
                   (unsigned)st,
                   (unsigned)((st & ADC_STS_LOCKED_MASK) != 0u),
                   (unsigned)((st & ADC_STS_DONE_MASK) != 0u),
                   (unsigned)((st & ADC_STS_BUSY_MASK) != 0u),
                   (unsigned)((st & ADC_STS_OTR_MASK) != 0u));
}

/**
 * @brief 自旋等 `0x90 & mask == want`。
 *
 * @param what 超时提示里的动作名(便于一眼看出卡在哪一步)
 */
static bool wait_until(uint32_t mask, uint32_t want,
                       uint32_t timeout_ms, const char *what)
{
    uint32_t t0 = cv_millis();
    uint32_t st;

    for (;;) {
        st = cv_reg_read(CV_REG_ADC_STS);
        if ((st & mask) == want) {
            return true;
        }
        /* 无符号差 ⇒ 回绕安全(`cv_millis()` 会绕回) */
        if ((uint32_t)(cv_millis() - t0) > timeout_ms) {
            char d[128];

            sts_dump(d, sizeof(d), st);
            (void)snprintf(s_err, sizeof(s_err),
                           "%s 超时(%u ms):%s\n"
                           "        ⇒ 依次查:① 位流含 0x8C–0x98 吗;"
                           "② DIV/PH_OFF 是否落在合法范围;③ MMCM 是否锁定。",
                           what, (unsigned)timeout_ms, d);
            return false;
        }
    }
}

/* ==========================================================================
 * 基本换算
 * ========================================================================== */

int32_t AdcSample_PortATick(uint32_t div)
{
    return cv_adc_phase_center(div);        /* 1.5 − P/4,与 MeasureConfig 的默认 PH_OFF 同源 */
}

int32_t AdcSample_PortBTick(uint32_t div)
{
    return cv_adc_phase_center_b(div);      /* 1.5 + P/4,已折回合法范围 */
}

bool AdcSample_TickLegal(uint32_t div, int32_t tick)
{
    uint32_t p    = (div < 2u) ? 2u : div;  /* 与 adc_capture.v 的 P = max(div,2) 一致 */
    int32_t  half = (int32_t)(p >> 1);

    return (tick >= (0 - half)) && (tick <= ((int32_t)p - 1 - half));
}

float AdcSample_CodesToVolts(float codes)
{
    /* ⚠️ 线性、无偏置改正 —— 假设 ADC 输出是**直通二进制**(不是二进制补码),
     *    且没有去掉直流偏置。对解调**不影响**:平均窗盖整数个激励周期时
     *    Σcos = Σsin = 0,直流分量被精确消掉(见 `AdcSample_Demodulate`)。 */
    return (codes * ADC_SAMPLE_VFS_PP) / 4096.0f;
}

float AdcSample_CsPf(float v_exc_v, float v_cur_v, uint32_t fe_hz)
{
    double i_amp;
    double y;
    double cs;

    if ((v_exc_v <= 0.0f) || (fe_hz == 0u)) {
        return 0.0f;
    }

    i_amp = (double)v_cur_v / (double)ADC_SAMPLE_R_TIA;   /* A */
    y     = i_amp / (double)v_exc_v;                      /* S —— 假设纯电容 ⇒ = ωC */
    cs    = y / (ADC_TWO_PI * (double)fe_hz);             /* F */

    return (float)(cs * 1e12);                            /* pF */
}

/* ==========================================================================
 * 链路
 * ========================================================================== */

bool AdcSample_LinkReady(void)
{
    return (cv_reg_read(CV_REG_ADC_STS) & ADC_STS_LOCKED_MASK) != 0u;
}

/* ==========================================================================
 * 采集
 * ========================================================================== */

bool AdcSample_Capture(int32_t ph_off, uint32_t n,
                       uint32_t *out_code, uint32_t *out_otr,
                       uint32_t *out_otr_count)
{
    uint32_t i;
    uint32_t otr_n = 0u;
    bool     pushed = false;
    bool     ok     = false;

    AdcSample_ClearError();

    if ((out_code == NULL) || (n == 0u) || (n > ADC_SAMPLE_BUF_DEPTH)) {
        set_err("参数错:out_code 为空,或 n 为 0,或 n 超过缓冲深度 4096");
        return false;
    }
    if (!AdcSample_LinkReady()) {
        set_err("0x90[0] mmcm_locked = 0 ⇒ MMCM 没锁,采集通路没起来(先查位流)");
        return false;
    }

    /* ① 确认不在采集。
     * 🔴 `adc_capture.v` 文件头:「改 P / PH_OFF 之前确认不在采集
     *    (CONT=0 且 busy=0)」。采集途中改会让 clk_base 域看到瞬时垃圾值。 */
    if (!wait_until(ADC_STS_BUSY_MASK, 0u, ADC_WAIT_IDLE_MS, "等采集空闲(busy=0)")) {
        return false;
    }

    /* ② 推入本次要用的采样刻度。0x88 由 measure_config 代写。 */
    if (!MeasureConfig_PushPhase(ph_off)) {
        set_err("PushPhase 失败(刻度越界 / 0x88 回读不一致,见上面 [mc] 那行)");
        return false;
    }
    pushed = true;

    /* ③ GO —— 一次写就是一拍脉冲(`cv_axi_lite_slave.v`),读回恒 0,不回读 */
    cv_reg_write(CV_REG_ADC_CTRL, 1u);

    /* ④ 等 busy=1:确认这一拍 GO 真的落地了。
     *    省掉这一步就可能读到上一次残留的 done=1(CDC 要几个周期)。 */
    if (!wait_until(ADC_STS_BUSY_MASK, ADC_STS_BUSY_MASK,
                    ADC_WAIT_GO_MS, "等 GO 生效(busy=1)")) {
        goto out;
    }

    /* ⑤ 等 done=1。RTL 在 go_pulse 上**同时清 done**,所以这里的 done 必属本次。 */
    if (!wait_until(ADC_STS_DONE_MASK, ADC_STS_DONE_MASK,
                    ADC_WAIT_DONE_MS, "等采集完成(done=1)")) {
        goto out;
    }

    /* ⑥ 读缓冲。单次模式下缓冲已**冻结**,可以慢慢读。
     *    读口是寄存器输出,rd_data 比 rd_idx 晚一拍 —— AXI 事务间隔远不止一拍。 */
    for (i = 0u; i < n; ++i) {
        uint32_t d;
        uint32_t o;

        cv_reg_write(CV_REG_ADC_BUF_IDX, i);
        d = cv_reg_read(CV_REG_ADC_BUF_DATA);

        out_code[i] = CV_ADC_SAMPLE(d);
        o = CV_ADC_OTR(d);
        if (out_otr != NULL) {
            out_otr[i] = o;
        }
        if (o != 0u) {
            otr_n++;
        }
    }
    ok = true;

out:
    /* ⑦ ★ 成败都要把刻度写回。失败路径漏掉这个,硬件就停在临时刻度上,
     *    而界面显示的是逻辑值 —— **没有任何症状**,下次测量才发现。 */
    if (pushed && !MeasureConfig_PopPhase()) {
        ok = false;
        if (s_err[0] == '\0') {
            set_err("PopPhase 回读不一致:0x88 没写回原值,采样刻度可能与界面不符");
        }
    }
    if (out_otr_count != NULL) {
        *out_otr_count = otr_n;     /* ⚠️ 提前返回时**未赋值** */
    }
    return ok;
}

/* ==========================================================================
 * 正交解调
 * ========================================================================== */

/**
 * @brief 按每周期 @p n 点建 cos/sin 参考表(只在 n 变化时重建)。
 *
 * 🔴 **为什么是运行时建表,不是编译期常量表**:每周期采样点数
 *    `N = f_s / f_e = 130 / DIV` 随用户选的档位变(DIV=5 → 26 点、
 *    13 → 10 点、26 → 5 点)。写死 10 点只对 DIV=13 成立。
 *    建表最多 26 次 `cos()`/`sin()`,一次采集摊到可以忽略;
 *    而且 `libm` 本来就已经链了(`pynq_protocol.c` 用了 `powf`)。
 */
static bool build_table(uint32_t n)
{
    uint32_t k;

    if (n == s_tab_n) {
        return true;                        /* 同一档位,复用 */
    }
    if ((n == 0u) || (n > ADC_DEMOD_MAX_N)) {
        return false;
    }

    for (k = 0u; k < n; ++k) {
        double ph = ADC_TWO_PI * (double)k / (double)n;

        s_tab_cos[k] = (float)cos(ph);
        s_tab_sin[k] = (float)sin(ph);
    }
    s_tab_n = n;
    return true;
}

bool AdcSample_Demodulate(const uint32_t *code, uint32_t n,
                          uint32_t sample_hz, uint32_t fe_hz,
                          float *out_i, float *out_q, float *out_mag)
{
    uint32_t nn;
    uint32_t use;
    uint32_t k;
    double   si = 0.0;
    double   sq = 0.0;

    AdcSample_ClearError();

    if ((code == NULL) || (out_i == NULL) || (out_q == NULL)) {
        set_err("解调参数错:指针为空");
        return false;
    }
    if ((sample_hz == 0u) || (fe_hz == 0u)) {
        set_err("解调参数错:采样率或激励频率为 0");
        return false;
    }

    nn = sample_hz / fe_hz;                 /* 每周期采样点数 */
    if (nn < ADC_SAMPLE_DEMOD_MIN_N) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:每周期只有 %u 点(下限 %u) —— "
                       "采样率 %u Hz 对 %u Hz 激励太低。"
                       "N=2 正好在 Nyquist 上、N=1 直接混叠到 DC,算出来是噪声。"
                       "换 DIV 更小的档位(DIV=13 → 10 点、DIV=5 → 26 点)。",
                       (unsigned)nn, (unsigned)ADC_SAMPLE_DEMOD_MIN_N,
                       (unsigned)sample_hz, (unsigned)fe_hz);
        return false;
    }
    if (!build_table(nn)) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:每周期 %u 点,超出参考表上限 %u",
                       (unsigned)nn, (unsigned)ADC_DEMOD_MAX_N);
        return false;
    }

    /* ★ 截到**整数个激励周期**。不截的话平均窗盖不住整周期,
     *   Σcos / Σsin 不为 0 ⇒ 2f_e 谐波和直流都抵不干净,曲线带纹波。 */
    use = (n / nn) * nn;
    if (use == 0u) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:只有 %u 点,不足一个激励周期(%u 点)",
                       (unsigned)n, (unsigned)nn);
        return false;
    }

    for (k = 0u; k < use; ++k) {
        double x = (double)code[k];

        si += x * (double)s_tab_cos[k % nn];
        sq -= x * (double)s_tab_sin[k % nn];
    }

    /* ×2/use 归一化到**幅值**:x[n] = A·cos(ωn+φ) ⇒ I = A·cosφ、Q = A·sinφ。
     * 直流项被 Σcos = Σsin = 0 精确消掉 ⇒ ADC 的偏置不用另做改正。 */
    si = (2.0 * si) / (double)use;
    sq = (2.0 * sq) / (double)use;

    *out_i = (float)si;
    *out_q = (float)sq;
    if (out_mag != NULL) {
        *out_mag = (float)sqrt((si * si) + (sq * sq));
    }
    return true;
}
