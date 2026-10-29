/* Host-side check for the AI anomaly detector (app/anomaly_detect.c).
 *
 *   gcc -std=gnu11 -O2 -Wall -Wextra -I src/c_ui/app \
 *       src/c_ui/tests/test_anomaly_detect.c src/c_ui/app/anomaly_detect.c \
 *       -lm -o /tmp/test_anomaly_detect
 *   /tmp/test_anomaly_detect
 *
 * 基线曲线取自 `report/measurements/artifacts/20261029_leg_settle_sweep_cal_v2.csv`
 * (本机健康件完整扫压,**已应用三标准校准**,60 点/腿)。**它是生成模型时用的同一份数据** ——
 * v2 与 v1 的差别:2026-10-29 起校准改成**通道级**(按挡位/频率选一条拟合,
 * 不再按腿),v1 那次扫压三腿都套的是 RG 腿系数,CRSS 曲线是错的 ⇒ 已废弃。
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
    3312.001000f, 3308.100000f, 3304.228000f, 3299.263000f, 3293.780000f, 3286.651000f,
    3279.826000f, 3272.877000f, 3263.706000f, 3253.971000f, 3242.493000f, 3228.354000f,
    3213.644000f, 3196.513000f, 3176.720000f, 3152.921000f, 3126.296000f, 3095.818000f,
    3061.106000f, 3021.135000f, 2976.777000f, 2931.143000f, 2882.952000f, 2835.467000f,
    2789.322000f, 2745.432000f, 2701.554000f, 2659.182000f, 2617.750000f, 2578.303000f,
    2539.923000f, 2502.548000f, 2466.459000f, 2431.841000f, 2398.416000f, 2366.958000f,
    2335.886000f, 2305.594000f, 2271.865000f, 2142.904000f, 2125.062000f, 2114.652000f,
    2105.375000f, 2096.353000f, 2088.750000f, 2081.026000f, 2073.732000f, 2066.572000f,
    2060.675000f, 2054.316000f, 2048.919000f, 2043.011000f, 2039.101000f, 2034.120000f,
    2030.695000f, 2028.278000f, 2026.769000f, 2025.267000f, 2024.252000f, 2022.663000f
};

static const float g_coss_pf[60] =
{
    3480.832000f, 3466.516000f, 3447.353000f, 3423.807000f, 3399.739000f, 3371.617000f,
    3342.345000f, 3309.245000f, 3273.743000f, 3234.959000f, 3193.537000f, 3147.680000f,
    3098.829000f, 3044.886000f, 2988.793000f, 2926.899000f, 2861.107000f, 2789.043000f,
    2711.569000f, 2628.982000f, 2539.221000f, 2448.499000f, 2355.300000f, 2261.325000f,
    2170.717000f, 2080.895000f, 1992.727000f, 1905.785000f, 1820.335000f, 1737.049000f,
    1655.959000f, 1576.404000f, 1495.909000f, 1422.049000f, 1350.419000f, 1280.444000f,
    1211.972000f, 1142.078000f, 1054.751000f, 373.680000f, 267.832000f, 247.334000f,
    247.673000f, 214.442000f, 200.640000f, 188.225000f, 176.878000f, 166.349000f,
    156.590000f, 147.414000f, 138.886000f, 130.883000f, 123.365000f, 116.649000f,
    110.128000f, 104.243000f, 98.521000f, 93.279000f, 88.249000f, 85.222000f
};

static const float g_crss_pf[60] =
{
    1501.472000f, 1535.030000f, 1527.291000f, 1522.825000f, 1513.864000f, 1507.171000f,
    1498.582000f, 1488.380000f, 1477.430000f, 1464.333000f, 1449.360000f, 1433.132000f,
    1414.084000f, 1392.124000f, 1369.093000f, 1340.407000f, 1309.267000f, 1272.731000f,
    1231.563000f, 1186.572000f, 1135.880000f, 1082.499000f, 1025.915000f, 970.485000f,
    916.748000f, 865.502000f, 816.974000f, 769.508000f, 724.011000f, 680.349000f,
    638.093000f, 597.673000f, 557.194000f, 520.604000f, 485.522000f, 451.784000f,
    419.607000f, 387.613000f, 361.661000f, 47.136000f, 37.623000f, 33.355000f,
    30.154000f, 27.762000f, 25.275000f, 23.254000f, 21.180000f, 19.488000f,
    17.916000f, 16.641000f, 15.453000f, 14.262000f, 13.401000f, 12.356000f,
    11.605000f, 10.972000f, 10.573000f, 10.149000f, 9.979000f, 9.500000f
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
        {3.5118, 3.3059, 0.4938, 0.8091},
        {3.5077, 1.9306, 0.9665, 0.4315},
        {3.1639, 0.9777, 0.9602, 0.2451}
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
