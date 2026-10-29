#ifndef ANOMALY_DETECT_H
#define ANOMALY_DETECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 扫压曲线的异常诊断(无监督,不需要故障样本)。
 *
 * 每条腿取 4 个与拟合无关的特征:
 *   f0 = log10(Cs @ 0.3 V)    低压电平
 *   f1 = log10(Cs @ 100 V)    高压电平
 *   f2 = log10(V50)           半降电压(对数中值电平处的电压)
 *   f3 = log10(V75 / V25)     拐点宽度(25%~75% 电平的电压比)
 * 健康总体 = 10 颗 TK9J90T 的收缩协方差,中心 = 本机健康基线。
 * 得分是马氏距离平方:score = d^T P d。
 *
 * ⚠️ 模型只对当前固定测量配置有效(1 MHz / 32.5 MSPS / 自动挡位)。
 *    换配置或改标定后,用 `src/c_ui/tools/fit_anomaly_model.py` 重新生成常数。 */

#define ANOMALY_LEG_COUNT      3u   /* 0=CISS 1=COSS 2=CRSS */
#define ANOMALY_FEATURE_COUNT  4u

typedef enum
{
    ANOMALY_LEVEL_NORMAL  = 0,  /* 健康 */
    ANOMALY_LEVEL_SUSPECT = 1,  /* 待查:超过健康样本最大离群度的 2.8 倍 */
    ANOMALY_LEVEL_FAULT   = 2   /* 异常 */
} AnomalyLevel;

typedef struct
{
    uint8_t valid;              /* 1 = 曲线足够,算出了分数 */
    uint8_t level;              /* AnomalyLevel */
    uint8_t dominant_feature;   /* 对得分贡献最大的特征 0..3 */
    float   score;              /* 马氏距离平方 */
    float   features[ANOMALY_FEATURE_COUNT];
    float   log_delta[ANOMALY_FEATURE_COUNT];   /* 相对模型中心的 log10 偏差 */
} AnomalyResult;

/* 评估一条腿。
 * @param voltage_v  扫压电压(V),与 cs_pf 一一对应
 * @param cs_pf      串联等效电容(pF)
 * @param valid      逐点有效性;为 0 的点跳过
 * @param count      数组长度(最多 256 点)
 * 成功返回 1 并填 out;数据不足 / 曲线不穿越 / 参数越界返回 0,
 * 此时 out->valid = 0(仍会写入,调用方可直接显示"数据不足")。 */
int Anomaly_Evaluate(uint32_t leg,
                     const float *voltage_v,
                     const float *cs_pf,
                     const uint8_t *valid,
                     uint32_t count,
                     AnomalyResult *out);

const char *Anomaly_LevelText(uint32_t level);
const char *Anomaly_FeatureText(uint32_t feature);

/* @brief 平台健康基线的采集日期(`YYYY-MM-DD`,来自生成的模型头文件)。
 *        界面用它显示"本机 YYYY-MM-DD",不要再把日期写死在 UI 里 ——
 *        校准开关一变,基线就得重采,日期也必须跟着变。 */
const char *Anomaly_BaselineDate(void);

#ifdef __cplusplus
}
#endif

#endif /* ANOMALY_DETECT_H */
