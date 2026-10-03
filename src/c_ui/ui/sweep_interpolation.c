#include "sweep_interpolation.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SWEEP_INTERPOLATION_X_EPSILON 1.0e-6f

static bool SweepInterpolation_SameDirection(float first, float second)
{
    return ((first > 0.0f) && (second > 0.0f)) ||
           ((first < 0.0f) && (second < 0.0f));
}

/* 计算PCHIP节点斜率，使用保形加权调和平均避免普通三次样条过冲。 */
static float SweepInterpolation_NodeSlope(
    const SweepInterpolationCurve *curve,
    uint32_t index)
{
    uint32_t count;
    float h_previous;
    float h_next;
    float delta_previous;
    float delta_next;
    float slope;

    if (curve == NULL)
    {
        return 0.0f;
    }
    count = curve->count;
    if (count < 2u)
    {
        return 0.0f;
    }
    if (count == 2u)
    {
        return (curve->y[1] - curve->y[0]) /
               (curve->x_log[1] - curve->x_log[0]);
    }

    if (index == 0u)
    {
        h_previous = curve->x_log[1] - curve->x_log[0];
        h_next = curve->x_log[2] - curve->x_log[1];
        delta_previous = (curve->y[1] - curve->y[0]) / h_previous;
        delta_next = (curve->y[2] - curve->y[1]) / h_next;
        slope = ((2.0f * h_previous + h_next) * delta_previous -
                 h_previous * delta_next) /
                (h_previous + h_next);
        if (!SweepInterpolation_SameDirection(slope, delta_previous))
        {
            return 0.0f;
        }
        if (!SweepInterpolation_SameDirection(delta_previous, delta_next) &&
            (fabsf(slope) > fabsf(3.0f * delta_previous)))
        {
            return 3.0f * delta_previous;
        }
        return slope;
    }

    if (index >= (count - 1u))
    {
        h_previous = curve->x_log[count - 2u] -
                     curve->x_log[count - 3u];
        h_next = curve->x_log[count - 1u] -
                 curve->x_log[count - 2u];
        delta_previous = (curve->y[count - 2u] -
                          curve->y[count - 3u]) / h_previous;
        delta_next = (curve->y[count - 1u] -
                      curve->y[count - 2u]) / h_next;
        slope = ((2.0f * h_next + h_previous) * delta_next -
                 h_next * delta_previous) /
                (h_next + h_previous);
        if (!SweepInterpolation_SameDirection(slope, delta_next))
        {
            return 0.0f;
        }
        if (!SweepInterpolation_SameDirection(delta_next, delta_previous) &&
            (fabsf(slope) > fabsf(3.0f * delta_next)))
        {
            return 3.0f * delta_next;
        }
        return slope;
    }

    h_previous = curve->x_log[index] - curve->x_log[index - 1u];
    h_next = curve->x_log[index + 1u] - curve->x_log[index];
    delta_previous = (curve->y[index] - curve->y[index - 1u]) /
                     h_previous;
    delta_next = (curve->y[index + 1u] - curve->y[index]) / h_next;
    if (!SweepInterpolation_SameDirection(delta_previous, delta_next))
    {
        return 0.0f;
    }

    return (3.0f * (h_previous + h_next)) /
           (((2.0f * h_next + h_previous) / delta_previous) +
            ((h_next + 2.0f * h_previous) / delta_next));
}

void SweepInterpolation_Reset(SweepInterpolationCurve *curve)
{
    if (curve != NULL)
    {
        (void)memset(curve, 0, sizeof(*curve));
    }
}

uint32_t SweepInterpolation_Build(SweepInterpolationCurve *curve,
                                  const float *voltage,
                                  const float *value,
                                  const uint8_t *valid,
                                  uint32_t point_count,
                                  bool logarithmic_value)
{
    uint32_t source_index;

    if ((curve == NULL) || (voltage == NULL) || (value == NULL))
    {
        return 0u;
    }

    SweepInterpolation_Reset(curve);
    curve->y_is_logarithmic = logarithmic_value ? 1u : 0u;
    if (point_count > SWEEP_INTERPOLATION_MAX_POINTS)
    {
        point_count = SWEEP_INTERPOLATION_MAX_POINTS;
    }

    for (source_index = 0u; source_index < point_count; ++source_index)
    {
        float x_log;
        float y_value;
        uint32_t insert_index;

        if (((valid != NULL) && (valid[source_index] == 0u)) ||
            !isfinite(voltage[source_index]) ||
            !isfinite(value[source_index]) ||
            (voltage[source_index] <= 0.0f) ||
            (logarithmic_value && (value[source_index] <= 0.0f)))
        {
            continue;
        }

        x_log = log10f(voltage[source_index]);
        y_value = logarithmic_value ?
                  log10f(value[source_index]) : value[source_index];
        if (!isfinite(x_log) || !isfinite(y_value))
        {
            continue;
        }

        insert_index = 0u;
        while ((insert_index < curve->count) &&
               (curve->x_log[insert_index] < x_log))
        {
            ++insert_index;
        }

        if ((insert_index < curve->count) &&
            (fabsf(curve->x_log[insert_index] - x_log) <=
             SWEEP_INTERPOLATION_X_EPSILON))
        {
            /* 相同偏压只保留最后收到的数据，避免零长度插值区间。 */
            curve->y[insert_index] = y_value;
            continue;
        }
        if (curve->count >= SWEEP_INTERPOLATION_MAX_POINTS)
        {
            continue;
        }

        if (insert_index < curve->count)
        {
            (void)memmove(&curve->x_log[insert_index + 1u],
                          &curve->x_log[insert_index],
                          (curve->count - insert_index) * sizeof(float));
            (void)memmove(&curve->y[insert_index + 1u],
                          &curve->y[insert_index],
                          (curve->count - insert_index) * sizeof(float));
        }
        curve->x_log[insert_index] = x_log;
        curve->y[insert_index] = y_value;
        ++curve->count;
    }

    return curve->count;
}

bool SweepInterpolation_GetLogVoltageRange(
    const SweepInterpolationCurve *curve,
    float *minimum_log_voltage,
    float *maximum_log_voltage)
{
    if ((curve == NULL) || (minimum_log_voltage == NULL) ||
        (maximum_log_voltage == NULL) || (curve->count == 0u))
    {
        return false;
    }

    *minimum_log_voltage = curve->x_log[0];
    *maximum_log_voltage = curve->x_log[curve->count - 1u];
    return true;
}

bool SweepInterpolation_EvaluateTransformed(
    const SweepInterpolationCurve *curve,
    float log_voltage,
    float *transformed_value)
{
    uint32_t low;
    uint32_t high;
    uint32_t segment;
    float x_start;
    float x_end;
    float interval;
    float ratio;
    float ratio_squared;
    float ratio_cubed;
    float start_slope;
    float end_slope;

    if ((curve == NULL) || (transformed_value == NULL) ||
        (curve->count == 0u) || !isfinite(log_voltage) ||
        (log_voltage < (curve->x_log[0] -
                        SWEEP_INTERPOLATION_X_EPSILON)) ||
        (log_voltage > (curve->x_log[curve->count - 1u] +
                        SWEEP_INTERPOLATION_X_EPSILON)))
    {
        return false;
    }
    if (log_voltage <= curve->x_log[0])
    {
        *transformed_value = curve->y[0];
        return true;
    }
    if (curve->count == 1u)
    {
        *transformed_value = curve->y[0];
        return true;
    }
    if (log_voltage >= curve->x_log[curve->count - 1u])
    {
        *transformed_value = curve->y[curve->count - 1u];
        return true;
    }

    low = 0u;
    high = curve->count - 1u;
    while ((high - low) > 1u)
    {
        uint32_t middle = low + (high - low) / 2u;

        if (curve->x_log[middle] <= log_voltage)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    segment = low;
    x_start = curve->x_log[segment];
    x_end = curve->x_log[segment + 1u];
    interval = x_end - x_start;
    if (interval <= 0.0f)
    {
        return false;
    }

    ratio = (log_voltage - x_start) / interval;
    ratio_squared = ratio * ratio;
    ratio_cubed = ratio_squared * ratio;
    start_slope = SweepInterpolation_NodeSlope(curve, segment);
    end_slope = SweepInterpolation_NodeSlope(curve, segment + 1u);
    *transformed_value =
        (2.0f * ratio_cubed - 3.0f * ratio_squared + 1.0f) *
            curve->y[segment] +
        (ratio_cubed - 2.0f * ratio_squared + ratio) *
            interval * start_slope +
        (-2.0f * ratio_cubed + 3.0f * ratio_squared) *
            curve->y[segment + 1u] +
        (ratio_cubed - ratio_squared) * interval * end_slope;
    return isfinite(*transformed_value);
}

bool SweepInterpolation_Evaluate(const SweepInterpolationCurve *curve,
                                 float voltage,
                                 float *value)
{
    float transformed_value;

    if ((curve == NULL) || (value == NULL) || !isfinite(voltage) ||
        (voltage <= 0.0f) ||
        !SweepInterpolation_EvaluateTransformed(
            curve, log10f(voltage), &transformed_value))
    {
        return false;
    }

    *value = (curve->y_is_logarithmic != 0u) ?
             powf(10.0f, transformed_value) : transformed_value;
    return isfinite(*value);
}
