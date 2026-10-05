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

#define UI_DEMO_AI_QUALITY_ANOMALY   0u
#define UI_DEMO_AI_QUALITY_UNCERTAIN 1u
#define UI_DEMO_AI_QUALITY_TRUSTED   2u
#define UI_DEMO_AI_QUALITY_ERROR     3u
#define UI_DEMO_AI_QUALITY_WAITING   4u

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
void UI_Demo_SetActualBias(const char *bias_text);
void UI_Demo_SetActualPath(const char *path_text);
void UI_Demo_UpdateSingleCapacitance(uint32_t path,
                                      const char *cs_text,
                                      const char *cp_text,
                                      const char *d_text,
                                      uint32_t valid);
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
void UI_Demo_UpdateWaveformPoint(uint32_t kind,
                                 uint32_t index,
                                 uint32_t total_samples,
                                 float voltage_v,
                                 float current_ma,
                                 const char *index_text);

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
void UI_Demo_SetAIModelUrl(const char *model_url);
void UI_Demo_SetAIQualityState(uint32_t state,
                               uint32_t top_class,
                               float clean_probability);

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
