/**
 * @file adc_sample.h
 * @brief 采一次 AD9238 缓冲 + 正交解调 —— 结果页波形的**唯一**数据源
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【这个模块解决什么】
 *   结果页的「单点波形 / 扫压波形」两张图、以及扫压曲线上的 Cs,此前
 *   **没有任何数据源**:喂数据的 `UI_Demo_*Waveform*` 没有活调用者,
 *   扫压的 Cs 来自 `pynq_protocol.c::demo_capacitance()` 那个**模型**。
 *   本模块把 0x8C/0x90/0x94/0x98 真正用起来。
 *
 * 【两路"输入"是怎么来的 —— 这是理解本模块的关键】
 *   🔴 **`adc_capture.v` 里没有"两个通道",只有"两个时间槽"。**
 *   `adc_clk = (cnt < P/2)`、`tick = cnt − P/2`,而那 12 位总线是**双端口
 *   时分复用**的 —— 一条总线在一个周期里**轮流输出 INA / INB 两路**,
 *   各占半个周期(`report/llm-log/LLM-045.md` §G 实测坐实):
 *
 *   | 端口 | 窗口(tick) | 宽度 | 中心 |
 *   |---|---|---|---|
 *   | **A = INA** | `[2 − P/2, +1]` | P/2 | `1.5 − P/4` |
 *   | **B = INB** | `[+2, P/2 + 1]` | P/2 | `1.5 + P/4` |
 *
 *   ⚠️ **用户口径是"上升沿=通道 a、下降沿=通道 b"—— 那是简写,不是刻度**。
 *   上升沿在 `tick = −P/2`、下降沿在 `tick = 0`,而 P=13 时 A 窗是 `[−4,+1]`、
 *   B 窗是 `[+2,+6]∪{−6,−5}`(6/7 两段,与实测逐点吻合)⇒
 *   **tick −6 落在 B 窗、tick 0 落在 A 窗 —— 正好是反的**;
 *   而且两者都贴着窗沿,不是中心。**故本模块按"窗口中心"取刻度**
 *   (@ref AdcSample_PortATick / @ref AdcSample_PortBTick),不照抄 −P/2 与 0。
 *
 *   🔴 **一次 GO 只锁存一个 tick** ⇒ "两路"是靠**两次采集**拿到的:
 *      先 PushPhase(A 窗中心) 采 INA,再 PushPhase(B 窗中心) 采 INB。
 *      **不需要改 RTL、不需要重下位流。** 代价见下面「两路的相位不相关」。
 *
 *   🔴 **两路的相位不相关**:A、B 是两次独立采集,各自的 GO 时刻任意,
 *      所以**两路之间的相位差是随机的**。⇒ 只能比**幅值**,不能算阻抗的相位。
 *      本模块因此只出幅值,`Cs` 按"被测件是纯电容"来算(见 @ref AdcSample_CsPf),
 *      **不出 Cp / D** —— 那两个需要相位,现在算出来是噪声。
 *
 *   ⚠️ **P=2(65 MHz)那一档 A 窗只剩 {±1}、中心舍入后落在寄存器合法范围外**,
 *      本模块会在 @ref AdcSample_Capture 之前就拒绝并说明。该档位本来就
 *      接不出可用波形(杜邦线 tPD ≈ 4 ns 盖过眼宽,见 `LLM-044`)。
 *
 * 【寄存器归属】
 *   `0x88` 由 `measure_config.c` **独占写入**。本模块改采样刻度一律走
 *   @ref MeasureConfig_PushPhase / @ref MeasureConfig_PopPhase,自己不碰 0x88。
 * ⛔ 本模块**只读** `0x84`(CV_CTRL) —— DAC 放行权归 Python 侧。
 * ────────────────────────────────────────────────────────────────────────
 */
#ifndef ADC_SAMPLE_H
#define ADC_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 缓冲深度(与 `adc_capture.v` 的 `BUF_DEPTH` 一致)。采集一次总是收满这么多点。 */
#define ADC_SAMPLE_BUF_DEPTH   4096u

/**
 * 每次读走的点数。**不是 4096** —— 结果页波形图的点数上限是 480
 * (`ui_demo.c::UI_WAVE_MAX_SAMPLES`),多读没有地方放,还白占 AXI 带宽。
 *
 * 取 250 = 25 个 1 MHz 激励周期(默认 DIV=13 → 10 MSPS ⇒ 每周期 10 点)。
 * ⛔ **不要取 256** —— 256 不是 10 的倍数,正交解调的平均窗盖不住整数个
 *    激励周期,2f_e 谐波抵不干净,曲线会带频谱泄漏的纹波。
 */
#define ADC_SAMPLE_POINTS      250u

/**
 * AD9238 满量程输入(差分,p-p,伏)。取 **2.0** —— AD9238 数据手册值。
 *
 * 🔴 **2.0 而不是 20**(2026-10-06 核过 RTL,记下来免得再被带偏):
 *    `ad9238.v:32` 写着「ADC_DATA * (20/4096)」,但**那个模块全工程没有例化**
 *    (`grep -n "ad9238" *.v` 只有它自己的 `module` 行) —— 它是前序 ATK 例程
 *    留下的**死代码**。实际数据通路是 `adc_capture` 的 `.adc_d (adc_d)`
 *    **直连 ADC 引脚**(`cv_sys.v:776`),中间没有任何换算模块。
 *    ⇒ 缓冲里存的就是 AD9238 的**原始输出码**,别去套 `ad9238.v` 的公式。
 *
 * ⚠️ **模拟前端到 ADC 引脚之间的衰减比未知** —— 若被测件与 ADC 之间还有
 *    分压/放大,这里的 2.0 就要跟着改。**未标定**,只对相对形状负责。
 */
#define ADC_SAMPLE_VFS_PP      2.0f

/**
 * 跨阻放大器反馈电阻(Ω)。**未标定** —— 原理图上的标称值,不是测出来的。
 * 它的误差直接进 `Cs`,所以本模块输出的 Cs 只能当"相对形状"看。
 */
#define ADC_SAMPLE_R_TIA       1000.0f

/**
 * @brief **port A(INA)** 时间槽的窗口中心 tick = `1.5 − P/4`。
 *
 * 直接转发 `cv_adc_phase_center()` —— 与本工程既有的 `PH_OFF` 默认值**同源**,
 * 不另算一份。P=13 → −2、26 → −5、130 → −31(实测三点)。
 */
int32_t AdcSample_PortATick(uint32_t div);

/**
 * @brief **port B(INB)** 时间槽的窗口中心 tick = `1.5 + P/4`。
 *
 * P=13 → **+5**(窗 `[+2,+7]`);已折回寄存器合法范围(见 `cv_adc_phase_center_b()`)。
 * ⛔ 不要拿 0 或 −P/2 代替 —— 见文件头那张窗口表。
 */
int32_t AdcSample_PortBTick(uint32_t div);

/**
 * @brief 该 tick 是否落在 `0x88` 的**寄存器合法范围** `[−P/2, P−1−P/2]` 内。
 *
 * ⛔ 落在外面的刻度**永远不会命中**,现象是 `done` 不置位(不是数据错)。
 * 调用方在推刻度前先查一次,好给出"换哪个 DIV"这种可执行的提示。
 */
bool AdcSample_TickLegal(uint32_t div, int32_t tick);

/**
 * @brief 采一次:改刻度 → GO → 等 done → 读 @p n 点 → **把刻度写回**。
 *
 * 调用序列(顺序不能改,理由逐条见下):
 *   ① 等 `0x90.busy == 0`
 *      🔴 `adc_capture.v` 文件头明写:「改 P / PH_OFF 之前确认不在采集」。
 *         采集途中改会让 clk_base 域看到瞬时垃圾值。
 *   ② @ref MeasureConfig_PushPhase(ph_off)
 *   ③ 写 `0x8C = 1`(GO)
 *   ④ 等 `0x90.busy == 1` —— 确认这一拍 GO 真的落地了
 *   ⑤ 等 `0x90.done == 1` —— RTL 在 go_pulse 上**同时清 done**,
 *      所以这里等到的 done **必属本次采集**,不是上一次的残留。
 *   ⑥ 逐点写 `0x94` 索引、读 `0x98` 数据
 *   ⑦ @ref MeasureConfig_PopPhase() —— **失败路径也必须走到**
 *
 * @param ph_off   本次要用的采样刻度(见 @ref AdcSample_PortATick / @ref AdcSample_PortBTick)
 * @param n        要读的点数,上限 @ref ADC_SAMPLE_POINTS
 * @param out_code 长度 ≥ @p n,收 12 位采样码
 * @param out_otr  长度 ≥ @p n,收每点的 OTR 位;**可传 NULL**
 * @param out_otr_count 本次 OTR 置起的点数;**可传 NULL**
 * @return true = 读到了完整 @p n 点
 */
bool AdcSample_Capture(int32_t ph_off, uint32_t n,
                       uint32_t *out_code, uint32_t *out_otr,
                       uint32_t *out_otr_count);

/**
 * @brief 正交解调:算激励频率上的 I / Q / 幅值。
 *
 * 平均窗**截断到整数个激励周期**再去算 —— 不截断就有频谱泄漏。
 * 归一化到「幅值」而不是「半幅」:`x[n] = A·cos(ωn+φ)` 时返回的 mag = A。
 *
 * ⚠️ 每周期采样点数 `N = sample_hz / fe_hz` 小于 @ref ADC_SAMPLE_DEMOD_MIN_N
 *    时**拒绝解调**(返回 false)—— N=2 刚好在 Nyquist 上、N=1 直接混叠到 DC,
 *    那两种情况下算出来的数字没有意义,不如明说。
 *
 * @param code      采样码数组
 * @param n         有效点数
 * @param sample_hz 采样率(Hz)
 * @param fe_hz     激励频率(Hz)
 * @param out_i     输出 I(单位 = ADC 码)
 * @param out_q     输出 Q(单位 = ADC 码)
 * @param out_mag   输出幅值(单位 = ADC 码);**可传 NULL**
 * @return true = 解调成功
 */
bool AdcSample_Demodulate(const uint32_t *code, uint32_t n,
                          uint32_t sample_hz, uint32_t fe_hz,
                          float *out_i, float *out_q, float *out_mag);

/** 每周期点数低于此值就不解调。N=2 在 Nyquist 上,N=1 混叠到 DC。 */
#define ADC_SAMPLE_DEMOD_MIN_N 4u

/**
 * @brief ADC 码 → 伏(按 @ref ADC_SAMPLE_VFS_PP 线性换算)。
 * ⚠️ 满量程与偏置**未标定**,只对相对形状负责。
 * 入参是 float 而不是 uint32_t —— 解调出来的**幅值**是个小数码,不是整数码。
 */
float AdcSample_CodesToVolts(float codes);

/**
 * @brief 幅值 → 串联等效电容(pF)。
 *
 * `Cs = |I| / (2π·f_e·|V|)` —— **假设被测件是纯电容**。
 * 🔴 这个假设成立的前提是知道 V 与 I 的相位差,而两路是分两次采的、
 *    相位不相关 ⇒ 这里**只能用幅值比**,算出来的 Cs 不含真实损耗分量。
 *    如实标明"未标定"。
 *
 * @param v_exc_v 通道 A 的幅值(伏)
 * @param v_cur_v 通道 B 的幅值(伏,已过跨阻)
 * @param fe_hz   激励频率(Hz)
 * @return pF。@p v_exc_v 或 @p fe_hz 为 0 时返回 0。
 */
float AdcSample_CsPf(float v_exc_v, float v_cur_v, uint32_t fe_hz);

/** @brief 最近一次失败的原因(静态字符串,永不为 NULL)。 */
const char *AdcSample_LastError(void);

/**
 * @brief 由**调用方**(而不是本模块)写入一条错误原因。
 *
 * 用途:`pynq_protocol.c` 在推刻度**之前**会做本模块看不见的检查
 * (比如"这个档位的窗口中心够不着"),那种失败的说明比本模块内部的
 * 更具体,该由它来写。⛔ 不要拿它覆盖一条已经更有信息量的原因。
 */
void AdcSample_SetErrorf(const char *fmt, ...);

/** @brief 清错误串(每次成功采集后由本模块自己清)。 */
void AdcSample_ClearError(void);

/** @brief 底层链路是否就绪(`0x90[0] mmcm_locked`)。 */
bool AdcSample_LinkReady(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_SAMPLE_H */
