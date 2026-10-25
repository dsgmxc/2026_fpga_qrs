/* Host-side check for the AI anomaly detector (app/anomaly_detect.c).
 *
 *   gcc -std=gnu11 -O2 -Wall -Wextra -I src/c_ui/app \
 *       src/c_ui/tests/test_anomaly_detect.c src/c_ui/app/anomaly_detect.c \
 *       -lm -o /tmp/test_anomaly_detect
 *   /tmp/test_anomaly_detect
 *
 * 基线曲线取自 `report/measurements/artifacts/20261024_leg_settle_sweep.csv`
 * (本机健康件完整扫压,60 点/腿)。**它是生成模型时用的同一份数据** ——
 * 所以基线得分必须≈0;这不是"自己考自己":电平乘 1.05 / 1.10 的注入样本
 * 完全不在建模数据里,检测它们才是模型有效性的证据。
 *
 * 电压网格由固件生成:0.1 V → 100 V,60 点对数等分(3 decade / 59 间隔)。
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "anomaly_detect.h"

#define POINT_COUNT 60u

static const float g_ciss_pf[60] =
{
    3378.808f, 3375.219f, 3370.829f, 3365.965f, 3361.205f, 3354.975f,
    3348.573f, 3340.721f, 3331.139f, 3321.679f, 3310.800f, 3297.066f,
    3283.106f, 3266.209f, 3246.088f, 3223.266f, 3197.052f, 3168.177f,
    3132.373f, 3090.636f, 3043.969f, 2993.526f, 2941.262f, 2887.627f,
    2834.967f, 2784.745f, 2737.136f, 2691.727f, 2645.996f, 2602.442f,
    2561.006f, 2521.074f, 2483.163f, 2446.586f, 2411.868f, 2377.792f,
    2345.759f, 2314.506f, 2281.122f, 2144.592f, 2126.477f, 2115.764f,
    2105.721f, 2097.678f, 2088.825f, 2081.222f, 2073.905f, 2067.554f,
    2060.900f, 2054.306f, 2048.513f, 2043.547f, 2038.435f, 2034.189f,
    2030.233f, 2027.114f, 2025.625f, 2024.857f, 2023.575f, 2022.046f
};

static const float g_coss_pf[60] =
{
    3568.799f, 3559.530f, 3535.160f, 3510.090f, 3485.137f, 3455.799f,
    3425.786f, 3392.123f, 3355.720f, 3314.990f, 3272.983f, 3225.345f,
    3176.216f, 3122.215f, 3065.010f, 3002.312f, 2935.303f, 2862.079f,
    2782.371f, 2696.618f, 2604.963f, 2509.577f, 2410.385f, 2308.271f,
    2210.686f, 2114.220f, 2020.693f, 1930.585f, 1840.891f, 1754.435f,
    1669.937f, 1587.299f, 1504.676f, 1428.682f, 1354.379f, 1283.805f,
    1214.570f, 1144.766f, 1066.473f, 384.869f, 265.534f, 245.156f,
    222.832f, 212.463f, 198.804f, 186.620f, 175.176f, 164.773f,
    155.066f, 145.863f, 137.446f, 129.552f, 122.273f, 115.372f,
    109.045f, 103.177f, 97.530f, 92.240f, 87.464f, 83.834f
};

static const float g_crss_pf[60] =
{
    1749.573f, 1795.659f, 1785.527f, 1777.942f, 1768.087f, 1757.378f,
    1744.909f, 1730.187f, 1714.761f, 1697.008f, 1679.121f, 1657.405f,
    1631.664f, 1605.365f, 1577.833f, 1541.202f, 1504.379f, 1460.754f,
    1411.821f, 1357.072f, 1296.546f, 1232.055f, 1161.898f, 1091.202f,
    1023.507f, 959.449f, 900.213f, 844.086f, 789.251f, 737.704f,
    689.058f, 642.336f, 596.322f, 554.933f, 515.285f, 478.218f,
    442.615f, 408.207f, 384.525f, 48.356f, 37.865f, 33.557f,
    30.525f, 27.811f, 25.343f, 23.071f, 21.246f, 19.442f,
    17.871f, 16.553f, 15.329f, 14.254f, 13.293f, 12.312f,
    11.503f, 10.979f, 10.439f, 10.049f, 9.792f, 9.510f
};

static int g_fail;

static void check(const char *what, int cond, double got, double want)
{
    if (!cond)
    {
        printf("FAIL %-44s got %.6g want %.6g\n", what, got, want);
        g_fail++;
    }
    else
    {
        printf("ok   %-44s %.6g\n", what, got);
    }
}

static void fill_voltage(float *v)
{
    uint32_t i;

    for (i = 0u; i < POINT_COUNT; ++i)
    {
        v[i] = (float)pow(10.0, -1.0 + (3.0 * (double)i) / 59.0);
    }
}

int main(void)
{
    static const float *const curves[3] = {g_ciss_pf, g_coss_pf, g_crss_pf};
    static const double expect_score[3] = {1709.2, 712.0, 53.2};
    static const double expect_feat[3][4] =
    {
        {3.5208, 3.3058, 0.4897, 0.7930},
        {3.5183, 1.9234, 0.9675, 0.4401},
        {3.2279, 0.9782, 0.9594, 0.2693}
    };
    float v[POINT_COUNT];
    float c[POINT_COUNT];
    uint8_t valid[POINT_COUNT];
    AnomalyResult r;
    uint32_t leg;
    uint32_t i;

    fill_voltage(v);
    memset(valid, 1, sizeof(valid));

    /* ① 平台基线:得分必须≈0,特征必须等于模型常数 */
    for (leg = 0u; leg < 3u; ++leg)
    {
        memcpy(c, curves[leg], sizeof(c));
        check("baseline evaluate", Anomaly_Evaluate(leg, v, c, valid,
                                                    POINT_COUNT, &r) == 1,
              1.0, 1.0);
        check("baseline score < 0.05", r.score < 0.05f, (double)r.score, 0.0);
        check("baseline level NORMAL", r.level == ANOMALY_LEVEL_NORMAL,
              (double)r.level, 0.0);
        for (i = 0u; i < 4u; ++i)
        {
            check("baseline feature", fabsf(r.features[i] -
                                            (float)expect_feat[leg][i]) < 2e-3f,
                  (double)r.features[i], expect_feat[leg][i]);
        }
    }

    /* ② 电平 x1.10:三条腿都必须离开"正常" */
    for (leg = 0u; leg < 3u; ++leg)
    {
        memcpy(c, curves[leg], sizeof(c));
        for (i = 0u; i < POINT_COUNT; ++i)
        {
            c[i] *= 1.10f;
        }
        check("x1.10 evaluate", Anomaly_Evaluate(leg, v, c, valid,
                                                 POINT_COUNT, &r) == 1,
              1.0, 1.0);
        check("x1.10 score", fabs((double)r.score - expect_score[leg]) /
                              expect_score[leg] < 0.02,
              (double)r.score, expect_score[leg]);
        check("x1.10 not NORMAL", r.level != ANOMALY_LEVEL_NORMAL,
              (double)r.level, 1.0);
    }

    /* ③ 电平 x1.05:CISS/COSS 仍能抓到,CRSS 在 25 以下(如实测统计) */
    for (leg = 0u; leg < 3u; ++leg)
    {
        memcpy(c, curves[leg], sizeof(c));
        for (i = 0u; i < POINT_COUNT; ++i)
        {
            c[i] *= 1.05f;
        }
        check("x1.05 evaluate", Anomaly_Evaluate(leg, v, c, valid,
                                                 POINT_COUNT, &r) == 1,
              1.0, 1.0);
        check("x1.05 CISS/COSS detected",
              (leg == 2u) ? (r.level == ANOMALY_LEVEL_NORMAL) :
                            (r.level != ANOMALY_LEVEL_NORMAL),
              (double)r.level, (leg == 2u) ? 0.0 : 2.0);
    }

    /* ④ 数据不足 / 全无效:必须拒绝,不能给分数 */
    check("5 points rejected",
          Anomaly_Evaluate(0u, v, curves[0], valid, 5u, &r) == 0, 0.0, 0.0);
    check("rejected -> valid 0", r.valid == 0u, (double)r.valid, 0.0);
    memset(valid, 0, sizeof(valid));
    check("all invalid rejected",
          Anomaly_Evaluate(0u, v, curves[0], valid, POINT_COUNT, &r) == 0,
          0.0, 0.0);
    check("NULL out rejected",
          Anomaly_Evaluate(0u, v, curves[0], valid, POINT_COUNT, NULL) == 0,
          0.0, 0.0);

    printf("\n%s\n", (g_fail == 0) ? "ALL OK" : "FAILURES");
    return (g_fail == 0) ? 0 : 1;
}
