/**
 * @file measure_config.h
 * @brief 「测量参数」的当前值与下发 —— 激励频率(0x80)/ 采样率 + 相位(0x88)
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【为什么单独一个模块】
 *   偏压归 `pynq_protocol.c`(它是"测量指令"那条线,前序 `display_protocol.h`
 *   的语义所在)。激励频率与采样率是**另一件事**:它们是**仪器设置**,不随
 *   扫描/点测走,上电设一次、用户改一次,而且改了之后要能**看见当前值**。
 *   混进 `pynq_protocol.c` 会让那个文件同时管"指令"和"配置",所以分开。
 *
 * 【单一所有者】
 *   `0x80` 与 `0x88` **只由本模块写**。`pynq_protocol.c` 的 `dds_set_hz()`
 *   已经改成委托这里 —— 两个写入方是"谁最后写谁赢"的经典 bug 源。
 *   ⛔ 但 `0x84`(CV_CTRL)例外:**放行权由 Python 独占**
 *      (`drivers/board.py::release_dac()`),本模块**只读不写**。
 *
 * 【和界面的关系】
 *   本模块**不知道**界面长什么样 —— 档位表、按钮文字、选中态高亮都在
 *   `ui/ui_demo.c` 里。这里只有数值,以及一个把数值拼成一行文字的
 *   @ref MeasureConfig_FormatLine(界面拿去显示,但格式由这里定,免得
 *   两处各拼一份、改一处漏一处)。
 * ────────────────────────────────────────────────────────────────────────
 */
#ifndef MEASURE_CONFIG_H
#define MEASURE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 默认激励频率。决策 D08 —— 1 MHz 是功率 MOSFET Coss/Crss 表征的行业标准频率。 */
#define CV_DDS_HZ_DEFAULT    1000000u

/**
 * @brief 上电初始化:把默认值下发到 `0x80` / `0x88`,并刷出状态行。
 *
 * ⚠️ **必须在 `UI_Demo_Init()` 之后调** —— 它会动 LVGL 标签(状态行)。
 *    调用顺序由 `main.c` 的 `UI_Demo_Init()` → `DisplayProtocol_Init()` 保证。
 */
void MeasureConfig_Init(void);

/**
 * @brief 设激励频率。越界夹到 [`CV_DDS_HZ_MIN`, `CV_DDS_HZ_MAX`]。
 * @return true = 写入且**回读一致**;false = 回读不一致(AXI 通路可疑)
 */
bool MeasureConfig_SetDdsHz(uint32_t hz);

/**
 * @brief 设采样率分频比。**同时**把 PH_OFF 移到该分频比对应的窗口中心。
 *
 * 🔴 **为什么必须一起改**:端口 A 的采样窗位置随 P 走(窗 = tick ∈
 *    [2 − P/2, +1],中心 = 1.5 − P/4)。只改 DIV 不改 PH_OFF,采样点会滑出
 *    被驱动的窗口 —— 现象是"换了速率就没波形了",**不报错、不越界**,
 *    极难定位。所以本接口**不提供**单独改 DIV 的路径。
 *
 * @param div 分频比(130 MHz / div = 采样率)。< 2 夹到 2。
 * @return true = 写入且回读一致
 */
bool MeasureConfig_SetSampleDiv(uint32_t div);

/** @brief 相位微调 @p delta 个 tick。越出采样窗则**不写**,返回 false。 */
bool MeasureConfig_NudgePhase(int32_t delta);

/** @brief 相位回到当前分频比的窗口中心。 */
bool MeasureConfig_CenterPhase(void);

/**
 * @brief **临时**把 PH_OFF 挪到 @p ph_off(保持 DIV 不变),给采样读取用。
 *
 * 🔴 为什么要有这一对,而不是让 `adc_sample.c` 自己写 `0x88`:
 *    `0x80`/`0x88` 的**写入方必须唯一**(见本文件头),两个写入方是
 *    "谁最后写谁赢"的经典 bug 源。采两路波形需要把采样点挪到固定刻度上,
 *    这件事只能由本模块代劳。
 *
 * ⚠️ 与 @ref MeasureConfig_NudgePhase 的区别:**本函数故意不套数据稳定窗**。
 *    微调相位是"在数据眼内挪",而采两路是要走到**刻度两端**(见 `adc_sample.c`),
 *    落在窗外的后果由调用方承担,所以这里只用寄存器**合法范围**做守卫
 *    (`adc_capture.v` 的 `[−P/2, P−1−P/2]`),越界才拒绝。
 *
 * ⚠️ 推入期间 @ref MeasureConfig_GetPhase 仍返回**逻辑值**(用户设的那个),
 *    硬件上是临时值。窗口只有一次采集那么长(<1 ms),界面状态不会因此说谎。
 *
 * ⚠️ 不可嵌套 —— 嵌套会覆盖外层的保存值,第二次调用直接拒绝。
 *
 * @return true = 写入且回读一致;false = 越界 / 嵌套 / 回读不一致(未推入)
 */
bool MeasureConfig_PushPhase(int32_t ph_off);

/**
 * @brief 把 PH_OFF 写回 @ref MeasureConfig_PushPhase 之前的那个值,并回读核对。
 *
 * 幂等:没推入过时直接返回 true(不报错)。
 * @return true = 已恢复且回读一致
 */
bool MeasureConfig_PopPhase(void);

/** @brief 激励频率 + 采样率 + 相位 三项一起回默认,并刷状态行。 */
bool MeasureConfig_RestoreDefaults(void);

uint32_t MeasureConfig_GetDdsHz(void);
uint32_t MeasureConfig_GetSampleDiv(void);
/** @brief 当前 PH_OFF(tick,有符号)。 */
int32_t  MeasureConfig_GetPhase(void);
/**
 * @brief 当前相位是不是正好在窗口中心。
 *
 * 给界面点「居中」按钮的高亮用 —— 相位是**连续量**,没有"当前档位",
 * 只能问它"是不是在中心"。中心公式(`1.5 − P/4`)留在本模块里,
 * 界面不必知道它。
 */
bool     MeasureConfig_IsPhaseCentered(void);

/**
 * @brief 拼一行"当前状态",给界面常驻显示。
 *
 * 形如:`激励 1.000MHz   采样 10.00MSPS(DIV13)   相位 -2`
 *
 * ⚠️ **不含偏压/回路** —— 那两项由常驻表头每页显示(`g_header_bias_label`),
 *    重复放会让本模块需要反向去读 `pynq_protocol.c` 的状态,两个模块互相咬。
 */
void MeasureConfig_FormatLine(char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_CONFIG_H */
