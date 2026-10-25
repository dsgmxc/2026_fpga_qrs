#include "anomaly_detect.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "anomaly_model_data.h"

/* 扫压一次最多 201 点(UI_DEMO_SWEEP_MAX_POINTS);留余量到 256。 */
#define ANOMALY_MAX_POINTS   256u
#define ANOMALY_MIN_POINTS   8u
#define ANOMALY_LOW_VOLTAGE  0.3f
#define ANOMALY_HIGH_VOLTAGE 100.0f

static const char *const s_level_text[] = {"正常", "待查", "异常"};
/* ⚠️ 这几个字会直接上屏。字库里没有"半/降/拐/宽",所以 V50 与宽度
   用符号表达,不写中文名(屏上显示成空白比说错更糟)。 */
static const char *const s_feature_text[] =
{
    "0.3V 电平", "100V 电平", "V50 电压", "V75/V25 比"
};

/* 在 (v, c) 上做 log-log 插值。目标点落在首段之前 / 末段之后时按该段外推,
   与 `tools/fit_anomaly_model.py::interpolate_log` 逐行对应。 */
static int interpolate_log(const float *v, const float *c, uint32_t n,
                           double target, double *out)
{
    uint32_t i;
    double v0, v1, c0, c1, t;

    if ((out == NULL) || (n < 2u) || !(target > 0.0))
    {
        return 0;
    }

    if (target <= (double)v[0])
    {
        i = 1u;
    }
    else if (target >= (double)v[n - 1u])
    {
        i = n - 1u;
    }
    else
    {
        i = 1u;
        while ((i < (n - 1u)) && ((double)v[i] < target))
        {
            ++i;
        }
    }

    v0 = (double)v[i - 1u];
    v1 = (double)v[i];
    c0 = (double)c[i - 1u];
    c1 = (double)c[i];
    if ((v0 <= 0.0) || (v1 <= 0.0) || (c0 <= 0.0) || (c1 <= 0.0) ||
        !isfinite(v0) || !isfinite(v1) || !isfinite(c0) || !isfinite(c1) ||
        (v0 == v1))
    {
        return 0;
    }

    t = (log10(target) - log10(v0)) / (log10(v1) - log10(v0));
    *out = pow(10.0, log10(c0) + t * (log10(c1) - log10(c0)));
    return isfinite(*out) && (*out > 0.0);
}

/* 找 Cs 从 level 上方穿到下方的电压;按同一套 log-log 插值。
   找不到返回 0(C 侧不用 NaN 做控制流)。 */
static int crossing_voltage(const float *v, const float *c, uint32_t n,
                            double level, double *out)
{
    uint32_t i;

    if ((out == NULL) || (n < 2u) || !(level > 0.0))
    {
        return 0;
    }

    for (i = 0u; i + 1u < n; ++i)
    {
        double a = (double)c[i] - level;
        double b = (double)c[i + 1u] - level;

        if (((a * b) <= 0.0) && (c[i] != c[i + 1u]) &&
            (c[i] > 0.0f) && (c[i + 1u] > 0.0f))
        {
            double t = (log10(level) - log10((double)c[i])) /
                       (log10((double)c[i + 1u]) - log10((double)c[i]));

            *out = pow(10.0, log10((double)v[i]) +
                             t * (log10((double)v[i + 1u]) -
                                  log10((double)v[i])));
            return isfinite(*out) && (*out > 0.0);
        }
    }
    return 0;
}

/* 只保留有效点并按电压升序排好。扫压本来就是升序,这里排序是为了
   万一中途恢复/重采把顺序打乱时,不会静默算出一条错的曲线。 */
static uint32_t compact_points(const float *voltage_v,
                               const float *cs_pf,
                               const uint8_t *valid,
                               uint32_t count,
                               float *v_out,
                               float *c_out)
{
    uint32_t i;
    uint32_t n = 0u;

    if (count > ANOMALY_MAX_POINTS)
    {
        count = ANOMALY_MAX_POINTS;
    }

    for (i = 0u; i < count; ++i)
    {
        if ((valid != NULL) && (valid[i] == 0u))
        {
            continue;
        }
        if (!isfinite(voltage_v[i]) || !isfinite(cs_pf[i]) ||
            (voltage_v[i] <= 0.0f) || (cs_pf[i] <= 0.0f))
        {
            continue;
        }
        v_out[n] = voltage_v[i];
        c_out[n] = cs_pf[i];
        ++n;
    }

    for (i = 1u; i < n; ++i)
    {
        float v = v_out[i];
        float c = c_out[i];
        uint32_t j = i;

        while ((j > 0u) && (v_out[j - 1u] > v))
        {
            v_out[j] = v_out[j - 1u];
            c_out[j] = c_out[j - 1u];
            --j;
        }
        v_out[j] = v;
        c_out[j] = c;
    }

    return n;
}

int Anomaly_Evaluate(uint32_t leg,
                     const float *voltage_v,
                     const float *cs_pf,
                     const uint8_t *valid,
                     uint32_t count,
                     AnomalyResult *out)
{
    float v[ANOMALY_MAX_POINTS];
    float c[ANOMALY_MAX_POINTS];
    double lo, hi, ratio, v50, v25, v75;
    double d[ANOMALY_FEATURE_COUNT];
    double pd[ANOMALY_FEATURE_COUNT];
    double contribution[ANOMALY_FEATURE_COUNT];
    double score = 0.0;
    float  best = -1.0f;
    uint32_t i;
    uint32_t j;
    uint32_t n;

    if (out == NULL)
    {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    if ((leg >= ANOMALY_LEG_COUNT) || (voltage_v == NULL) || (cs_pf == NULL))
    {
        return 0;
    }

    n = compact_points(voltage_v, cs_pf, valid, count, v, c);
    if (n < ANOMALY_MIN_POINTS)
    {
        return 0;
    }

    /* C-V 曲线是**单调下降**的:高压端必须比低压端小。写成 `hi > lo`
       会让所有健康曲线都判"数据不足"(2026-10-24 首版就踩了这个)。 */
    if (!interpolate_log(v, c, n, (double)ANOMALY_LOW_VOLTAGE, &lo) ||
        !interpolate_log(v, c, n, (double)ANOMALY_HIGH_VOLTAGE, &hi) ||
        !(hi < lo))
    {
        return 0;
    }
    ratio = hi / lo;

    if (!crossing_voltage(v, c, n, sqrt(lo * hi), &v50) ||
        !crossing_voltage(v, c, n, lo * pow(ratio, 0.25), &v25) ||
        !crossing_voltage(v, c, n, lo * pow(ratio, 0.75), &v75) ||
        !(v75 > v25) || !(v50 > 0.0))
    {
        return 0;
    }

    out->features[0] = (float)log10(lo);
    out->features[1] = (float)log10(hi);
    out->features[2] = (float)log10(v50);
    out->features[3] = (float)log10(v75 / v25);

    for (i = 0u; i < ANOMALY_FEATURE_COUNT; ++i)
    {
        d[i] = (double)out->features[i] -
               (double)g_anomaly_model_center[leg][i];
        out->log_delta[i] = (float)d[i];
    }
    for (i = 0u; i < ANOMALY_FEATURE_COUNT; ++i)
    {
        pd[i] = 0.0;
        for (j = 0u; j < ANOMALY_FEATURE_COUNT; ++j)
        {
            pd[i] += (double)g_anomaly_model_inv_cov[leg][i][j] * d[j];
        }
    }
    for (i = 0u; i < ANOMALY_FEATURE_COUNT; ++i)
    {
        contribution[i] = d[i] * pd[i];
        score += contribution[i];
    }
    if (!(score > 0.0))
    {
        score = 0.0;
    }

    out->score = (float)score;
    out->valid = 1u;
    if (score < (double)ANOMALY_MODEL_THRESHOLD)
    {
        out->level = ANOMALY_LEVEL_NORMAL;
    }
    else if (score < (double)ANOMALY_MODEL_FAULT_THRESHOLD)
    {
        out->level = ANOMALY_LEVEL_SUSPECT;
    }
    else
    {
        out->level = ANOMALY_LEVEL_FAULT;
    }

    /* 主导特征:只看正贡献里最大的那个。负贡献是协方差里的相互抵消,
       拿它解释"哪里最不对"会误导。 */
    out->dominant_feature = 0u;
    for (i = 0u; i < ANOMALY_FEATURE_COUNT; ++i)
    {
        if ((float)contribution[i] > best)
        {
            best = (float)contribution[i];
            out->dominant_feature = (uint8_t)i;
        }
    }

    return 1;
}

const char *Anomaly_LevelText(uint32_t level)
{
    return (level < 3u) ? s_level_text[level] : "?";
}

const char *Anomaly_FeatureText(uint32_t feature)
{
    return (feature < ANOMALY_FEATURE_COUNT) ? s_feature_text[feature] : "?";
}
