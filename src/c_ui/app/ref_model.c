/* ============================================================================
 *  器件参考模型访问层
 *
 *  数据本体:`ref_th521_data.c`(生成文件,别手改)。
 *  这里只做三件事:型号枚举、网格外夹紧、对数插值。
 *
 *  ⚠️ 插值与生成脚本 `tools/make_th521_reference.py` 保持同一口径:
 *     Cs 在 (log Vds, log C) 里线性;
 *     D 在 (log Vds, D) 里线性 —— D 可以很小,不能再取对数。
 *     Cp 不插值,由插值后的 Cs、D 按 Cs/(1+D²) 现算(见 RefModel_At)。
 * ========================================================================== */

#include "ref_model.h"

#include <math.h>
#include <stddef.h>

#include "ref_th521_data.h"

#define REF_MODEL_COUNT_INTERNAL  1u

/* 名字是**器件型号**,不是仪器型号 —— TH521 是同惠那台 C-V 分析仪的型号,
   十颗被测管的型号是 TK9J90T(用户 2026-10-23 指出并更正)。
   括号里的「10 颗」= 中位曲线来自十颗同型号器件,不是十种型号。 */
static const char *const k_ref_model_names[REF_MODEL_COUNT_INTERNAL] =
{
    "TK9J90T 标准(10 颗)"
};

uint32_t RefModel_Count(void)
{
    return REF_MODEL_COUNT_INTERNAL;
}

const char *RefModel_Name(uint32_t model)
{
    if (model >= REF_MODEL_COUNT_INTERNAL)
    {
        return NULL;
    }
    return k_ref_model_names[model];
}

uint32_t RefModel_PointCount(uint32_t model, uint32_t leg)
{
    if ((model >= REF_MODEL_COUNT_INTERNAL) || (leg >= REF_TH521_LEGS))
    {
        return 0u;
    }
    return REF_TH521_CV_POINTS;
}

int RefModel_CurvePoint(uint32_t model,
                        uint32_t leg,
                        uint32_t index,
                        float *vds_v,
                        float *cs_pf)
{
    if ((model >= REF_MODEL_COUNT_INTERNAL) || (leg >= REF_TH521_LEGS) ||
        (index >= REF_TH521_CV_POINTS) || (vds_v == NULL) || (cs_pf == NULL))
    {
        return 0;
    }

    *vds_v = g_ref_th521_vds[index];
    *cs_pf = g_ref_th521_cs_pf[leg][index];
    return 1;
}

/* 201 点的线性扫描足够快,而且不引入二分查找的边界错误 */
static uint32_t ref_find_segment(float vds_v)
{
    uint32_t index = 0u;

    while ((index + 2u) < REF_TH521_CV_POINTS &&
           (g_ref_th521_vds[index + 1u] <= vds_v))
    {
        ++index;
    }
    return index;
}

int RefModel_At(uint32_t model,
                uint32_t leg,
                float vds_v,
                float *cs_pf,
                float *cp_pf,
                float *d)
{
    uint32_t index;
    float v;
    float t;
    float x0;
    float x1;

    if ((model >= REF_MODEL_COUNT_INTERNAL) || (leg >= REF_TH521_LEGS) ||
        (cs_pf == NULL) || (cp_pf == NULL) || (d == NULL) ||
        !isfinite(vds_v) || (vds_v <= 0.0f))
    {
        return 0;
    }

    v = vds_v;
    if (v < g_ref_th521_vds[0])
    {
        v = g_ref_th521_vds[0];
    }
    else if (v > g_ref_th521_vds[REF_TH521_CV_POINTS - 1u])
    {
        v = g_ref_th521_vds[REF_TH521_CV_POINTS - 1u];
    }

    index = ref_find_segment(v);
    if (index >= (REF_TH521_CV_POINTS - 1u))
    {
        index = REF_TH521_CV_POINTS - 2u;
    }

    x0 = logf(g_ref_th521_vds[index]);
    x1 = logf(g_ref_th521_vds[index + 1u]);
    t = (logf(v) - x0) / (x1 - x0);
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    else if (t > 1.0f)
    {
        t = 1.0f;
    }

    *cs_pf = expf(logf(g_ref_th521_cs_pf[leg][index]) +
                  (t * (logf(g_ref_th521_cs_pf[leg][index + 1u]) -
                        logf(g_ref_th521_cs_pf[leg][index]))));
    *d = g_ref_th521_d[leg][index] +
         (t * (g_ref_th521_d[leg][index + 1u] -
               g_ref_th521_d[leg][index]));
    /* 串联→并联定义:Cp = Cs / (1 + D^2)。这样三个显示值永远自洽。 */
    *cp_pf = (*cs_pf) / (1.0f + (*d) * (*d));
    return 1;
}

int RefModel_Rg(uint32_t model, float *rg_ohm)
{
    if ((model >= REF_MODEL_COUNT_INTERNAL) || (rg_ohm == NULL))
    {
        return 0;
    }
    *rg_ohm = g_ref_th521_rg_ohm;
    return 1;
}
