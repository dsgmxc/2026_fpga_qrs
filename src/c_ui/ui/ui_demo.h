#ifndef UI_DEMO_H
#define UI_DEMO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_DEMO_OK     0u
#define UI_DEMO_ERROR  1u

#define UI_SINGLE_PATH_CISS 0u
#define UI_SINGLE_PATH_COSS 1u
#define UI_SINGLE_PATH_CRSS 2u
#define UI_SINGLE_PATH_RG   3u

#define UI_DEMO_SWEEP_PATH_COUNT  3u
#define UI_DEMO_SWEEP_MAX_POINTS  201u

uint32_t UI_Demo_Init(void);
uint32_t UI_Demo_GetClickCount(void);
uint32_t UI_Demo_GetLastButton(void);
uint32_t UI_Demo_GetCurrentPage(void);
uint32_t UI_Demo_GetResetRequestCount(void);
/* ⚠️ 仅供**演示 / 自动化截图**使用:按 UI_Page 编号切页,等价于用户点首页卡片。
   正常业务流程里页面切换由 LVGL 事件驱动,不要从别的模块调它。 */
void UI_Demo_GotoPage(uint32_t page);

uint32_t UI_Demo_GetSinglePath(void);
uint32_t UI_Demo_GetSingleBiasTenths(void);
/* 原 UI_Demo_GetMeasureAllBiasTenths() 已随「一键测量页」合并进「测量页」而撤除:
   合并后单回路与「全部」共用 UI_Demo_GetSingleBiasTenths()。 */
uint32_t UI_Demo_GetSweepPath(void);
uint32_t UI_Demo_GetSweepEndTenths(void);

/* 协议层在LVGL主循环上下文中调用以下接口更新界面。 */
void UI_Demo_SetConnection(uint32_t online);
void UI_Demo_SetStatusText(const char *text);
/* 🆕 2026-10-06:「系统复位」页、复位按钮**正下方**的常驻一行。
   显示测量参数当前值(激励频率 / 采样率 / 相位),由 MeasureConfig 每次改动后刷新。
   ⚠️ 与表头的 SetStatusText 不是一回事:那个在顶栏、每页都有、显示操作结果;
   这个是本页专属的参数读数,常驻不消失。 */
void UI_Demo_SetInitStatusText(const char *text);
void UI_Demo_SetActualBias(const char *bias_text);
void UI_Demo_SetActualPath(const char *path_text);
/* 🆕 2026-10-10:顶栏「档位:」栏(参考电阻挡位 10Ω/100Ω/1kΩ/10kΩ)。
   与上面两个同一个口径 —— 灌进来的是 **PL 回读的已生效值**,不是请求值。
   调用方 = `app/relay_view.c`。

   🆕 2026-10-15 加了 @p gear_code —— **测量页那 4 颗挡位按钮的高亮也跟着它走**。
   在此之前本函数只更新顶栏那一行文字,按钮高亮没有来源。
   ⇒ 加了这个参数之后,「挡位」在界面上的**两处显示同源**,
     不会出现"顶栏说 B、按钮高亮在 D"那种自相矛盾。
   ⚠️ **别改成"按下就去高亮"** —— 写失败时按钮会变成一句谎话,而本模块的
     全部存在意义就是消灭界面上的谎话(见 `relay_view.c` 文件头)。
   @param gear_code `0x18` 的原始独热码(非独热会照实显示 `0xN`,不高亮任何一颗) */
void UI_Demo_SetActualGear(const char *gear_text, uint32_t gear_code);

/* 🆕 2026-10-10:只读的系统提示框(标题 + 正文 + 「知道了」)。
   给 `app/relay_view.c` 报继电器/档位读回异常用。
   ⚠️ 与「系统复位」确认框**共用** `g_modal_overlay` —— 已经有一层弹框时
   直接返回,不叠加。所以调用方要自己按值去重,别每轮都来敲门。
   ⚠️ 文案必须落在 `ui_font_cn_20` / `ui_font_cn_28` 的子集内(缺字不报错)。 */
void UI_Demo_ShowAlert(const char *title, const char *body);
/* 🆕 2026-10-23:@p bias_v 是该点**实际**偏压(Vds,V),只用于查器件标准曲线;
   结果页「单点结果」用它把四张卡的标准值放在实测值下面。
   传 <= 0 或非有限值时,标准值显示 "--"(不猜)。 */
void UI_Demo_UpdateSingleCapacitance(uint32_t path,
                                      const char *cs_text,
                                      const char *cp_text,
                                      const char *d_text,
                                      uint32_t valid,
                                      float bias_v);
void UI_Demo_UpdateAllCapacitance(uint32_t path,
                                  const char *cs_text,
                                  const char *cp_text,
                                  const char *d_text,
                                  uint32_t valid);
void UI_Demo_UpdateResistance(const char *resistance_text, uint32_t valid);
/* 调试波形(结果页的「单点波形」/「扫压波形」两页)。
   kind: 0 = 单点测量过程   1 = 扫压过程(电压/电流包络)
   采样序号从 1 开始;电压单位 V,电流单位 mA。 */
#define UI_DEMO_WAVE_POINT   0u
#define UI_DEMO_WAVE_SWEEP   1u

void UI_Demo_BeginWaveform(uint32_t kind, uint32_t total_samples);

/** @brief 波形图能存多少点(= `ui_demo.c::UI_WAVE_MAX_SAMPLES`)。
 *
 *  为什么要暴露:采集缓冲 8192 点**远大于**波形图容量,喂之前必须先抽稀。
 *  抽稀方(`pynq_protocol.c::adc_feed_point_waveform`)若自己写一份常数,
 *  就成了"同一个数写在两处" —— 改一处不改另一处的后果是**静默只画前 N 点**,
 *  屏上看不出少,只会觉得"波形怎么是平的"(2026-10-08 之前就是这个症状:
 *  480 上限截掉 8192 点里的 94%)。 ⇒ 只留这一个定义点。 */
uint32_t UI_Demo_WaveMaxSamples(void);
void UI_Demo_UpdateWaveformPoint(uint32_t kind,
                                 uint32_t index,
                                 uint32_t total_samples,
                                 float voltage_v,
                                 float current_ma,
                                 const char *index_text);

/* 🆕 2026-10-06:把上面两组数据**重画到结果页上**。
   🔴 为什么需要这个导出:`UI_ResultsRefresh()` 是 `static`,只在"进结果页 /
   切页签"时跑。数据喂进 `g_result_wave[]` / 扫压曲线之后**不会自动重绘**,
   屏上就出现"数据是新的、画的是旧的"。协议层每轮采集完调一次这个。
   ⚠️ 结果页**还没构建**时是空操作(内部 `g_results_built` 已挡住)—— 所以
   在任何页面下调用都安全。 */
void UI_Demo_RefreshResultViews(void);

void UI_Demo_ClearAllResults(void);
void UI_Demo_ClearSweep(uint32_t path_or_all);
void UI_Demo_BeginSweep(uint32_t path, uint32_t total_points,
                        uint32_t multi_path);
void UI_Demo_UpdateSweepPoint(uint32_t path,
                              uint32_t point_index,
                              uint32_t total_points,
                              float bias_voltage_v,
                              float series_capacitance_pf,
                              float parallel_capacitance_pf,
                              float dissipation_factor,
                              uint32_t model_values_valid,
                              const char *bias_text,
                              const char *capacitance_text,
                              uint32_t valid);
void UI_Demo_SetSweepInspectionReady(void);

/* 🆕 2026-10-17:结果汇总 > 数据传输页的二维码载荷。
   URL 由 `app/cv_qr_live.c` 生成(ACV1 + SCV1,照搬前序工程)。
   - @p url 为空 / 超长 ⇒ 等价于 `UI_Demo_ClearTransferPayload()`,回 DEMO 码;
   - 只在这里改状态并置脏位,真正的 QR 编码推迟到 `UI_UpdateTransferQRCode()`
     (在结果页刷新时调用),避免每 80 ms 重排一次二维码。 */
void UI_Demo_SetTransferPayload(const char *url,
                                uint32_t acv1_nodes,
                                uint32_t scv1_points);
void UI_Demo_ClearTransferPayload(void);

/* AI模块只读复制当前扫压数据，不取得UI内部缓存的写权限。 */
uint32_t UI_Demo_CopySweepPath(uint32_t path,
                               float *voltage_v,
                               float *series_capacitance_pf,
                               uint8_t *valid,
                               uint32_t capacity);
uint32_t UI_Demo_GetLatestResistance(float *resistance_ohm);

#ifdef __cplusplus
}
#endif

#endif
