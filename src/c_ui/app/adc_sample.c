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
 * 真实耗时,不然会把"正常但慢"判成超时。真实耗时量级(2026-10-14 缓冲加倍后):
 *   收满 16384 字:最快档(DIV=2,65 MSPS)= 252 µs;最慢档(DIV=130)= 16.4 ms
 * ⇒ done 给 200 ms 仍有 ~12 倍余量,不会误报。 */
#define ADC_WAIT_IDLE_MS      50u
#define ADC_WAIT_GO_MS        50u
#define ADC_WAIT_DONE_MS      200u

/** 🔴 **参考表的点数上限 = 一次解调用多少个采样点(窗长)**,不是"每周期最多几点"。
 *
 *  2026-10-07 改。旧实现按**一个激励周期**建表,于是表长 = `f_s/f_e`,
 *  激励一调低表就爆(`100 kHz + 10 MSPS` → 100 点 > 64)——
 *  那是**这张表能装多少**,不是 DFT 的限制。DFT 的唯一要求是
 *  **窗覆盖整数个激励周期**(`L·f_e/f_s ∈ ℤ`),它**不要求 `f_s/f_e` 是整数**。
 *  改成按**整窗 `L` 点**建表后,表长只跟窗长走,与频率比**无关**。
 *
 *  🔴 **2026-10-08 由 256 改 8192。** 这是"加长窗"改动里的**第二道闸**:
 *     `ADC_SAMPLE_POINTS` 从 250 提到 8192 后,各档激励的窗长
 *     `L = 6500 / 7800 / 8190` **全部超过 256** ⇒ 表长不改的话
 *     `build_table()` 直接 `return false`,用户看到的是
 *     「解调不可用:窗长 6500 点,超出参考表上限 256」——
 *     **数字上算得出来、这里装不下**,与 `ui_demo.c` 那个停在旧档位的
 *     回调守卫是同一个形状(算得对、闸在中间)。
 *
 *  ⚠️ 它是 `static` 的 `.bss`,**不是栈**:8192 × 4 B × 2 张表 = 64 KB,
 *     改大不会炸栈。若哪天改成局部变量,64 KB 就会当场爆栈。
 *  ⚠️ 必须 ≥ `ADC_SAMPLE_POINTS`(`L ≤ n`)。两个值一起改。 */
#define ADC_DEMOD_MAX_L       ADC_SAMPLE_POINTS

/** 窗内至少要有几个激励周期。低于此值 ⇒ 窗里连一个完整周期都没有,
 *  相关运算等于在比"两个不同的东西",不如明说不解调。
 *  (当前实现下不会触发 —— 由"窗长必须是 `q` 的整数倍"这条保证 `m ≥ 1`;
 *   留作断言用。) */
#define ADC_DEMOD_MIN_M       1u

#define ADC_TWO_PI            6.28318530717958647692

/* ==========================================================================
 * 状态
 * ========================================================================== */

/* 🔴 320 不是随手写的:下面"超时诊断"和"每周期点数不足"两条消息
 *    最长可达 ~261 / ~249 字节(算上 `%s` 展开的那个 0x90 状态串)。
 *    原来写 192,`-Wformat-truncation` 实测报**截断** —— 恰好在这种
 *    "就靠这行字定位"的时候把最关键的尾句吃掉。加宽到 320 留余量。 */
static char     s_err[320];
static uint32_t s_tab_m;                            /* 表是按哪组 (M, L) 建的 */
static uint32_t s_tab_l;
static float    s_tab_cos[ADC_DEMOD_MAX_L];
static float    s_tab_sin[ADC_DEMOD_MAX_L];

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
    return cv_adc_phase_center_b(div);      /* 1.5 + P/4,已折回合法范围 —— 只作对照了 */
}

int32_t AdcSample_PortBTickHw(uint32_t div, int32_t tick_a)
{
    /* 🔴 **必须与 `adc_capture.v` 的那三行逐位同构**(`ph_b_s` 那段):
     *
     *     hi_s     = P − 1 − P/2                (寄存器合法上界)
     *     ph_b_sum = A + P/2                    (B 就在 A 后面半个 ADC 周期)
     *     ph_b     = (ph_b_sum > hi) ? ph_b_sum − P : ph_b_sum
     *
     * ⛔ **不要"优化"成 `(A + half) % P` 之类** —— 折回范围不是 `[0,P)`,
     *    而是 `[−P/2, P−1−P/2]`(偏移二进制式的刻度)。
     * ⛔ 也**不要**借 `cv_adc_phase_center_b()`:那个是"独立地把物理中心
     *    1.5+P/4 取整",P 为奇数时会与硬件差一个 tick(见 `pynq_regs.h` 的说明)。
     *    本函数是硬件的**镜像**,只用于日志。
     *
     * ⚠️ 数学上 A 合法 ⇒ B 必然合法(至多减一次 P 就落回范围),所以本函数
     *    不需要、也没有"B 越界"这个失败态。 */
    int32_t p    = (int32_t)((div < 2u) ? 2u : div);
    int32_t half = p >> 1;
    int32_t hi   = p - 1 - half;
    int32_t b    = tick_a + half;

    return (b > hi) ? (b - p) : b;
}

bool AdcSample_TickLegal(uint32_t div, int32_t tick)
{
    uint32_t p    = (div < 2u) ? 2u : div;  /* 与 adc_capture.v 的 P = max(div,2) 一致 */
    int32_t  half = (int32_t)(p >> 1);

    return (tick >= (0 - half)) && (tick <= ((int32_t)p - 1 - half));
}

float AdcSample_CodesToVolts(float codes)
{
    /* ⚠️ 线性、无偏置改正 —— 要求缓冲里是**偏移二进制**(不是二进制补码),
     *    且没有去掉直流偏置。对解调**不影响**:平均窗盖整数个激励周期时
     *    Σcos = Σsin = 0,直流分量被精确消掉(见 `AdcSample_Demodulate`)。
     *
     *    🔴 **2026-10-12:这条要求原先只是"假设",且**不成立**——**
     *    板上实测 AD9238 出的是**二进制补码**,而 RTL 原样存进缓冲 ⇒ 每个负半周
     *    被折到 `4096+|x|`。后果:峰峰恒 4095 码(看着像超量程,其实 OTR 一次没置)、
     *    直流看着停在半量程、屏上波形变成"满幅方波"、Cs 反演无意义。
     *    修法在 **RTL**:`adc_capture.v` 写入时 `adc_d ^ 12'h800`(补码→偏移二进制)。
     *    ⇒ 现在这条假设由硬件通路保证。**不要在这里再转一次**(会拐回去)。 */
    return (codes * ADC_SAMPLE_VFS_PP) / 4096.0f;
}

bool AdcSample_PpConsistent(float pp_codes, float mag_codes, float *out_ratio)
{
    /* 幅值已经是**码**了(`AdcSample_Demodulate` 返回的就是码),不要再过一次
     * `CodesToVolts` —— 那要多除一次 VFS_PP、再多乘回来,白绕一圈还多两次浮点舍入。
     * 纯正弦的峰峰值 = 2×振幅,这是本判据的全部前提。 */
    double exp_pp = 2.0 * (double)mag_codes;
    double limit  = ((double)ADC_SAMPLE_PP_RATIO_MAX * exp_pp) + (double)ADC_SAMPLE_PP_FLOOR;

    if (out_ratio != NULL) {
        /* 幅值 0 时比值无意义(除数 0)—— 返 0 而不是 inf,
         * 免得日志里打出 `inf` 又被当成"特别坏"。此时 limit = FLOOR,
         * 一段真正的直流/死信号只要 `码 <= 64` 仍被判自洽,这是有意的:
         * 判据只管"有没有毛刺",不管"有没有信号"(那是波形页的活)。 */
        *out_ratio = (exp_pp > 0.0) ? (float)((double)pp_codes / exp_pp) : 0.0f;
    }

    return ((double)pp_codes <= limit);
}

bool AdcSample_Impedance(float ia, float qa, float ib, float qb,
                         double r0_ohm, uint32_t fe_hz, AdcSampleZ *out)
{
    double dv_re, dv_im, vb2, w, mag2;

    if (out == NULL) {
        set_err("阻抗参数错:out 为空");
        return false;
    }
    if ((r0_ohm <= 0.0) || (fe_hz == 0u)) {
        /* r0 <= 0 就是"挡位码非独热"那个哨兵(见 cv_range_ohms)。**绝不能
         * 放过去** —— 它会变成一个除零,出来的 inf 会一路画到曲线上。 */
        (void)snprintf(s_err, sizeof(s_err),
                       "阻抗算不了:R0 = %.3f Ω(挡位码非独热?)或激励 %u Hz 为 0",
                       r0_ohm, (unsigned)fe_hz);
        return false;
    }

    /* Vx = VH − VL(全在 ADC 码域;比例因子稍后与 VL 一起约掉) */
    dv_re = (double)ia - (double)ib;
    dv_im = (double)qa - (double)qb;
    vb2   = ((double)ib * (double)ib) + ((double)qb * (double)qb);

    if (vb2 <= 0.0) {
        /* |VL| = 0 ⇒ 选择支路没有信号(继电器没切到、R0 支路断路、或输入没接)。
         * ⚠️ 这不是"算出来是 0",是**这个测量不成立** —— 必须报错,
         *    不能返回一个 0 Ω 让界面画成一条贴着横轴的直线。 */
        set_err("阻抗算不了:|VL| = 0 ⇒ 参考电阻支路没有信号"
                "(查:① 回路继电器真的切过去了吗;② R0 支路是否断路;③ 输入有没有接)");
        return false;
    }

    w = ADC_TWO_PI * (double)fe_hz;                 /* ω */

    out->z_re_ohm = r0_ohm * ((dv_re * (double)ib) + (dv_im * (double)qb)) / vb2;
    out->z_im_ohm = r0_ohm * ((dv_im * (double)ib) - (dv_re * (double)qb)) / vb2;

    mag2 = (out->z_re_ohm * out->z_re_ohm) + (out->z_im_ohm * out->z_im_ohm);

    /* 串联等效电容。**容性时 Im Z < 0,所以负号让它为正。**
     * 若被测件呈感性(Im Z > 0),Cs 会算出负值 —— **不要夹到 0**,
     * 负号就是"这不是个电容"的信号,夹掉等于把诊断信息扔了。 */
    out->cs_pf = (out->z_im_ohm != 0.0)
               ? ((-1e12 / w) / out->z_im_ohm)
               : 0.0;

    /* 并联等效电容 Cp = Cs/(1 + D²) = −Im Z /(ω·|Z|²) */
    out->cp_pf = (mag2 > 0.0) ? ((-(out->z_im_ohm) * 1e12) / (w * mag2)) : 0.0;

    /* 损耗因数 D = tanδ = ωCsRs = |Re Z / Im Z| */
    out->d = (out->z_im_ohm != 0.0)
           ? fabs(out->z_re_ohm / out->z_im_ohm)
           : 0.0;

    /* 流过被测件的电流 = |VL| / R0。用码算幅值再换算成伏:
     * ⚠️ 这一项**仍然用未标定的 ADC_SAMPLE_VFS_PP**(它不像 Z 那样能约掉),
     *    所以电流读数是"相对形状可信、绝对值未标定"。 */
    {
        double mag_b_codes = sqrt(((double)ib * (double)ib) + ((double)qb * (double)qb));
        double mag_b_v     = (double)AdcSample_CodesToVolts((float)mag_b_codes);

        out->cur_ma = (mag_b_v / r0_ohm) * 1000.0;
    }

    return true;
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

bool AdcSample_CapturePair(uint32_t *out_code_a, uint32_t *out_code_b,
                           uint32_t *out_otr_count)
{
    uint32_t div;
    int32_t  tick_a;
    uint32_t i;
    uint32_t otr_n = 0u;
    bool     pushed = false;
    bool     ok     = false;

    AdcSample_ClearError();

    if ((out_code_a == NULL) || (out_code_b == NULL)) {
        set_err("参数错:out_code_a / out_code_b 不能为空");
        return false;
    }
    if (!AdcSample_LinkReady()) {
        set_err("0x90[0] mmcm_locked = 0 ⇒ MMCM 没锁,采集通路没起来(先查位流)");
        return false;
    }

    /* ⓪ A 的刻度与合法性。B 的刻度不在这里推 —— 硬件按 A+P/2 自算。
     *    落在寄存器合法范围外的刻度**永不命中**:现象是 `done` 不置位(超时),
     *    很容易被误判成"位流不对"。所以提前给出可执行的判断。
     *    ⚠️ 原文案是"换更大的 DIV(1/2/5/10/13 MSPS 里挑)"—— 2026-10-08 起
     *       那是**做不到的建议**:采样率由 `CV_ADC_DIV_DEFAULT` 一个地方定死、
     *       界面上没有换档入口(2026-10-15 起 = DIV=4 / 32.5 MSPS)。
     *       真遇到它,说明 `cv_adc_phase_center()` 的折回与当前
     *       `CV_ADC_DIV_DEFAULT` 不一致 —— 那是**代码缺陷**,不是"挑一档"。 */
    div    = MeasureConfig_GetSampleDiv();
    tick_a = AdcSample_PortATick(div);
    if (!AdcSample_TickLegal(div, tick_a)) {
        AdcSample_SetErrorf("DIV=%u 下窗口中心越出寄存器合法范围(A tick %+d)。"
                            "采样率由 CV_ADC_DIV_DEFAULT 定死,界面上没有别的档可换 ⇒ 这是"
                            "cv_adc_phase_center() 的折回与 DIV 不一致,属代码缺陷。",
                            (unsigned)div, (int)tick_a);
        return false;
    }

    /* ① 确认不在采集。
     * 🔴 `adc_capture.v` 文件头:「改 P / PH_OFF 之前确认不在采集
     *    (CONT=0 且 busy=0)」。采集途中改会让 clk_base 域看到瞬时垃圾值。 */
    if (!wait_until(ADC_STS_BUSY_MASK, 0u, ADC_WAIT_IDLE_MS, "等采集空闲(busy=0)")) {
        return false;
    }

    /* ② 推入本次要用的采样刻度(**只有 A 那一个**)。0x88 由 measure_config 代写。 */
    if (!MeasureConfig_PushPhase(tick_a)) {
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

    /* ⑥ 读**整块交织缓冲**并拆交织。单次模式下缓冲已**冻结**,可以慢慢读。
     *    读口是寄存器输出,rd_data 比 rd_idx 晚一拍 —— AXI 事务间隔远不止一拍。
     *
     *    🔴 布局:`地址 2k = A_k(INA)`、`2k+1 = B_k(INB)`(见 `adc_capture.v` 文件头 ④)。
     *    ⛔ **不要按"前半段是 A、后半段是 B"去切** —— 那样每路拿到的是交错的
     *       两路数据,解调出来看着**仍像正弦**(频率是一半),**不报错**。
     *       判据:两路的 p-p 会几乎相同且都偏小,而正确拆法下 A 是激励、B 是响应。 */
    for (i = 0u; i < ADC_SAMPLE_BUF_DEPTH; ++i) {
        uint32_t d;
        uint32_t o;

        cv_reg_write(CV_REG_ADC_BUF_IDX, i);
        d = cv_reg_read(CV_REG_ADC_BUF_DATA);

        if ((i & 1u) == 0u) {
            out_code_a[i >> 1] = CV_ADC_SAMPLE(d);   /* 偶 → A */
        } else {
            out_code_b[i >> 1] = CV_ADC_SAMPLE(d);   /* 奇 → B */
        }

        o = CV_ADC_OTR(d);
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
 * @brief 最大公约数(Euclid)。用来把激励/采样率化到最简比。
 */
static uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    while (b != 0u) {
        uint32_t t = a % b;

        a = b;
        b = t;
    }
    return a;
}

/**
 * @brief 按**整窗 @p l 点、内含 @p m 个激励周期**建 cos/sin 参考表(只在 (m,l) 变化时重建)。
 *
 * 🔴 **为什么是整窗表,不是"一个周期"的表**(2026-10-07 改):
 *    旧实现建的是**一个激励周期**的表(`cos(2πk/n)`,`k = 0..n-1`),调用处用
 *    `k % n` 循环取 ⇒ 表长 = `f_s/f_e`,**必须整除**,且激励一调低表就爆。
 *    那两条都是**实现**的派生品,不是 DFT 的要求。DFT 只要求窗覆盖整数个周期;
 *    按整窗建表时,周期数体现在**相位步进**里,与频率比是否整除**无关**。
 *    ⇒ 表长只跟窗长走(`l ≤ ADC_DEMOD_MAX_L`),与 `f_e/f_s` 无关。
 *
 * 相位步进必须是 `2π·m/l` **而不是** `2π/l`:后者是"一个周期占满整个窗",
 * 会把激励当成 DC,算出来恒等于窗内平均值。
 *
 * 🔴 **为什么是运行时建表,不是编译期常量表**:`m` 与 `l` 都随用户选的
 *    激励 / 采样率档位变。建表最多 `l` 次 `cos()`/`sin()`(≤ 250),
 *    一次采集摊到可以忽略;而且 `libm` 本来就已经链了(`pynq_protocol.c` 用了 `powf`)。
 */
static bool build_table(uint32_t m, uint32_t l)
{
    uint32_t k;

    if ((m == s_tab_m) && (l == s_tab_l)) {
        return true;                        /* 同一组参数,复用 */
    }
    if ((m == 0u) || (l == 0u) || (l > ADC_DEMOD_MAX_L)) {
        return false;
    }

    for (k = 0u; k < l; ++k) {
        double ph = ADC_TWO_PI * (double)m * (double)k / (double)l;

        s_tab_cos[k] = (float)cos(ph);
        s_tab_sin[k] = (float)sin(ph);
    }
    s_tab_m = m;
    s_tab_l = l;
    return true;
}

bool AdcSample_Demodulate(const uint32_t *code, uint32_t n,
                          uint32_t sample_hz, uint32_t fe_hz,
                          float *out_i, float *out_q, float *out_mag)
{
    uint32_t g;
    uint32_t q;
    uint32_t m;
    uint32_t l;
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

    /* ★ 相干采样只要求「窗覆盖整数个激励周期」,即 `L·f_e/f_s ∈ ℤ`。
     *   把它化到最简:`f_e/f_s = p/q`(gcd 约分)⇒ **`L` 必须是 `q` 的整数倍**,
     *   此时窗内恰好有 `m = L·p/q` 个整周期,参考取 `cos(2π·m·k/L)`。
     *   ⛔ 不要写成"`f_s/f_e` 必须整除" —— 那是个**更强、且 DFT 不要求**的条件,
     *      它只在"按一个周期循环查表"的旧实现下才必须(见 build_table 的注释)。
     *   ⛔ 也不要用 `round(m·f_s/f_e)` 反推 `l`:那个乘积**未必接近整数**
     *      (`2 MHz + 65 MSPS` → `8 × 32.5 = 260`、退一位得 `7 × 32.5 = 227.5`),
     *      四舍五入后这一窗**不是整数个周期**,泄漏消不掉 ——
     *      而且**不报错**。本实现第一版就是这么写的,靠数值自检才发现
     *      (`2 MHz + 65 MSPS` 的 mag 得 999.47 而非 1000.00)。 */
    g = gcd_u32(sample_hz, fe_hz);
    q = sample_hz / g;                      /* 最简分母:波形每 q 点重复一次 */
    l = (n / q) * q;                        /* ≤ n 的最大 q 的整数倍 = 窗长 */
    m = (l / q) * (fe_hz / g);              /* 窗内整周期数 */

    /* 窗内放不下整数个激励周期:激励太低 / 采样率太高(`q > n` ⇒ `l == 0`)。
     * ⚠️ 这个方向**与"每周期点数不够"正好相反** —— 两条各自写明往哪边调,
     *    否则照错的指引排查会往反方向走(`LLM-050` 记过这个缺陷)。 */
    if ((l == 0u) || (m < ADC_DEMOD_MIN_M)) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:%u 点里放不下整数个激励周期"
                       "(激励 %u Hz 对采样率 %u Hz 太低,窗长必须是 %u 点的整数倍)。"
                       "把激励调高,或把采样率调低。",
                       (unsigned)n, (unsigned)fe_hz, (unsigned)sample_hz,
                       (unsigned)q);
        return false;
    }

    /* 每周期点数 = `l/m`,**真比值**(= `q/p`),不是整数除法的截断值。
     * 🔴 旧实现用 `sample_hz / fe_hz` 的**整数商**当每周期点数:
     *    `2 MHz + 13 MSPS` 得 6、真值 6.5 ⇒ 建出的表对**任何一个周期都不成立**,
     *    而且**不报错**,静默返回一个错的数。改成真比值后这一格是精确的。 */
    if (((double)l / (double)m) < (double)ADC_SAMPLE_DEMOD_MIN_N) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:每周期只有 %.2f 点(下限 %u)—— "
                       "采样率 %u Hz 对 %u Hz 激励太低。"
                       "N=2 正好在 Nyquist 上、N=1 直接混叠到 DC,算出来是噪声。"
                       "把激励调低,或把采样率调高。",
                       (double)l / (double)m, (unsigned)ADC_SAMPLE_DEMOD_MIN_N,
                       (unsigned)sample_hz, (unsigned)fe_hz);
        return false;
    }

    if (!build_table(m, l)) {
        (void)snprintf(s_err, sizeof(s_err),
                       "解调不可用:窗长 %u 点,超出参考表上限 %u",
                       (unsigned)l, (unsigned)ADC_DEMOD_MAX_L);
        return false;
    }

    for (k = 0u; k < l; ++k) {
        double x = (double)code[k];

        si += x * (double)s_tab_cos[k];
        sq -= x * (double)s_tab_sin[k];
    }

    /* ×2/l 归一化到**幅值**:x[k] = A·cos(2πmk/l + φ) ⇒ I = A·cosφ、Q = A·sinφ。
     * 直流项被 Σcos = Σsin = 0 精确消掉(整周期 ⇒ 整圈单位根之和为零)
     * ⇒ ADC 的偏置不用另做改正。 */
    si = (2.0 * si) / (double)l;
    sq = (2.0 * sq) / (double)l;

    *out_i = (float)si;
    *out_q = (float)sq;
    if (out_mag != NULL) {
        *out_mag = (float)sqrt((si * si) + (sq * sq));
    }
    return true;
}

/* `s_tab_m` / `s_tab_l` 是 build_table() 的记忆,只在**成功建表**后更新,
 * 所以它们天然就是"最近一次成功解调用的窗参数" —— 不需要另存一份副本
 * (另存一份就有两个真相,迟早漂移)。 */
void AdcSample_LastWindow(uint32_t *out_l, uint32_t *out_m)
{
    if (out_l != NULL) { *out_l = s_tab_l; }
    if (out_m != NULL) { *out_m = s_tab_m; }
}
