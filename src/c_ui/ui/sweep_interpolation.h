#ifndef SWEEP_INTERPOLATION_H
#define SWEEP_INTERPOLATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWEEP_INTERPOLATION_MAX_POINTS 201u

/*
 * 扫压连续曲线数据。
 * x_log保存log10(V)，y保存原值或log10值；原始测量数组仍由UI模块独立保留。
 */
typedef struct
{
    uint16_t count;
    uint8_t y_is_logarithmic;
    float x_log[SWEEP_INTERPOLATION_MAX_POINTS];
    float y[SWEEP_INTERPOLATION_MAX_POINTS];
} SweepInterpolationCurve;

void SweepInterpolation_Reset(SweepInterpolationCurve *curve);

uint32_t SweepInterpolation_Build(SweepInterpolationCurve *curve,
                                  const float *voltage,
                                  const float *value,
                                  const uint8_t *valid,
                                  uint32_t point_count,
                                  bool logarithmic_value);

bool SweepInterpolation_GetLogVoltageRange(
    const SweepInterpolationCurve *curve,
    float *minimum_log_voltage,
    float *maximum_log_voltage);

bool SweepInterpolation_EvaluateTransformed(
    const SweepInterpolationCurve *curve,
    float log_voltage,
    float *transformed_value);

bool SweepInterpolation_Evaluate(const SweepInterpolationCurve *curve,
                                 float voltage,
                                 float *value);

#ifdef __cplusplus
}
#endif

#endif
