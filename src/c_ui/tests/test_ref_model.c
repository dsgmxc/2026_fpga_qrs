/* Host-side check for the reference model lookup layer.
 *
 *   gcc -std=gnu11 -O2 -I src/c_ui/app \
 *       src/c_ui/tests/test_ref_model.c src/c_ui/app/ref_model.c \
 *       src/c_ui/app/ref_th521_data.c -lm -o /tmp/test_ref_model
 *   /tmp/test_ref_model
 *
 * 只验证"查表 / 插值 / 夹紧 / Cs-Cp-D 自洽"这几件事,不验证数据本身是否物理
 * 正确(那由 report/measurements/.../README.md 的记录负责)。
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ref_model.h"
#include "ref_th521_data.h"

static int g_fail;

static void expect_rel(const char *what, double got, double want, double tol)
{
    double scale = (fabs(want) > 1e-9) ? fabs(want) : 1.0;
    double err = fabs(got - want) / scale;

    if (!(err <= tol))
    {
        printf("FAIL %-34s got %.9g want %.9g (rel %.3g > %.3g)\n",
               what, got, want, err, tol);
        g_fail++;
    }
    else
    {
        printf("ok   %-34s %.6f\n", what, got);
    }
}

static void expect_true(const char *what, int cond)
{
    if (!cond)
    {
        printf("FAIL %s\n", what);
        g_fail++;
    }
    else
    {
        printf("ok   %s\n", what);
    }
}

int main(void)
{
    static const uint32_t sample_index[] = {0u, 67u, 134u, 200u};
    float cs;
    float cp;
    float d;
    float vds;
    float rg;
    uint32_t i;
    uint32_t leg;

    expect_true("model count is 1", RefModel_Count() == 1u);
    expect_true("name is not NULL", RefModel_Name(0u) != NULL);
    /* 名字必须是**器件**型号 TK9J90T。TH521 是测量仪器,2026-10-23 曾误用。 */
    expect_true("name is TK9J90T",
                (RefModel_Name(0u) != NULL) &&
                (strcmp(RefModel_Name(0u), "TK9J90T 标准(10 颗)") == 0));
    expect_true("name rejects model 1", RefModel_Name(1u) == NULL);
    expect_true("point count 201", RefModel_PointCount(0u, REF_LEG_CISS) == 201u);
    expect_true("RG has no curve", RefModel_PointCount(0u, REF_LEG_RG) == 0u);

    /* 网格点必须原样返回 */
    for (leg = 0u; leg < 3u; ++leg)
    {
        for (i = 0u; i < 4u; ++i)
        {
            uint32_t index = sample_index[i];
            char what[64];

            expect_true("CurvePoint vds", RefModel_CurvePoint(
                0u, leg, index, &vds, &cs) &&
                fabsf(vds - g_ref_th521_vds[index]) < 1e-5f);
            snprintf(what, sizeof(what), "leg %u idx %u cs", leg, index);
            expect_rel(what, cs, g_ref_th521_cs_pf[leg][index], 1e-6);
        }
    }

    /* 网格点上:At() 与表值一致,且 Cp / D 必须自洽 */
    for (leg = 0u; leg < 3u; ++leg)
    {
        for (i = 0u; i < 4u; ++i)
        {
            uint32_t index = sample_index[i];
            char what[64];

            expect_true("At() accepts a grid point",
                        RefModel_At(0u, leg, g_ref_th521_vds[index], &cs, &cp, &d));
            snprintf(what, sizeof(what), "leg %u idx %u At d ", leg, index);
            expect_rel(what, d, g_ref_th521_d[leg][index], 1e-6);
            snprintf(what, sizeof(what), "leg %u idx %u At cp", leg, index);
            expect_rel(what, cp,
                       g_ref_th521_cs_pf[leg][index] /
                       (1.0 + g_ref_th521_d[leg][index] * g_ref_th521_d[leg][index]),
                       1e-6);
        }
    }

    /* 网格外夹紧:0.05 V 等于 0.1 V,200 V 等于 100 V */
    {
        float cs_a, cs_b, cp_a, d_a;

        RefModel_At(0u, REF_LEG_CISS, 0.05f, &cs_a, &cp_a, &d_a);
        RefModel_At(0u, REF_LEG_CISS, g_ref_th521_vds[0], &cs_b, &cp_a, &d_a);
        expect_rel("clamp low", cs_a, cs_b, 1e-6);
        RefModel_At(0u, REF_LEG_CISS, 200.0f, &cs_a, &cp_a, &d_a);
        RefModel_At(0u, REF_LEG_CISS, g_ref_th521_vds[200], &cs_b, &cp_a, &d_a);
        expect_rel("clamp high", cs_a, cs_b, 1e-6);
    }

    /* 1 V 落在两个网格点之间;Cs 随 Vds 单调下降 ⇒ 必须在两点之间 */
    {
        float cs1, ca, cb, cpa, da;

        expect_true("At(1 V) works", RefModel_At(0u, REF_LEG_CISS, 1.0f, &cs1, &cp, &d));
        RefModel_At(0u, REF_LEG_CISS, 0.977237f, &ca, &cpa, &da);
        RefModel_At(0u, REF_LEG_CISS, 1.01158f, &cb, &cpa, &da);
        expect_true("1 V is bracketed", (cs1 < ca) && (cs1 > cb));
        printf("     1 V Ciss = %.2f pF (bracket %.2f .. %.2f), Cp = %.2f, D = %.5f\n",
               cs1, cb, ca, cp, d);
    }

    expect_true("At() rejects 0 V", RefModel_At(0u, REF_LEG_CISS, 0.0f, &cs, &cp, &d) == 0);
    expect_true("At() rejects RG", RefModel_At(0u, REF_LEG_RG, 1.0f, &cs, &cp, &d) == 0);
    expect_true("At() rejects model 1", RefModel_At(1u, REF_LEG_CISS, 1.0f, &cs, &cp, &d) == 0);

    expect_true("Rg ok", RefModel_Rg(0u, &rg) == 1);
    expect_rel("Rg value", rg, 3.06448, 1e-5);

    /* 任意电压下 Cp = Cs/(1+D^2) 都必须成立(界面三行显示的是同一组数) */
    for (leg = 0u; leg < 3u; ++leg)
    {
        float max_err = 0.0f;
        float v;

        for (v = 0.1f; v <= 100.0f; v *= 1.07f)
        {
            float want;

            if (!RefModel_At(0u, leg, v, &cs, &cp, &d))
            {
                max_err = 1.0f;
                break;
            }
            want = cs / (1.0f + d * d);
            if (fabsf(cp - want) > max_err)
            {
                max_err = fabsf(cp - want) / want;
            }
        }
        printf("     leg %u Cp vs Cs/(1+D^2) max rel err = %.3g\n", leg, max_err);
        if (max_err > 1e-5f)
        {
            printf("FAIL leg %u Cp/Cs/D are inconsistent\n", leg);
            g_fail++;
        }
    }

    printf("\n%s (%d failure%s)\n", (g_fail == 0) ? "PASS" : "FAIL",
           g_fail, (g_fail == 1) ? "" : "s");
    return (g_fail == 0) ? 0 : 1;
}
