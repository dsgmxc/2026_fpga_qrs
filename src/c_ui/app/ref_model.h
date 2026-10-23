#ifndef REF_MODEL_H
#define REF_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 器件参考模型(标准曲线)的只读访问层。
 *
 * 目前只有一项:TK9J90T 标准(10 颗同型号器件的中位曲线)。
 * ⚠️ 多型号时**只加表、不改调用方** —— 界面用 RefModel_Count() 遍历,
 *    不要在 ui_demo.c 里写死型号个数。 */

typedef enum
{
    REF_LEG_CISS = 0,
    REF_LEG_COSS = 1,
    REF_LEG_CRSS = 2,
    REF_LEG_RG   = 3
} RefLeg;

uint32_t RefModel_Count(void);

/* 弹出列表里显示的名字(如 "TK9J90T 标准(10 颗)")。越界返回 NULL。 */
const char *RefModel_Name(uint32_t model);

/* 参考曲线的采样点数(横轴 Vds)。leg 必须是 CISS/COSS/CRSS。
   越界返回 0 —— 调用方据此决定画不画。 */
uint32_t RefModel_PointCount(uint32_t model, uint32_t leg);

/* 取第 index 个参考点,单位 V / pF。成功返回 1。 */
int RefModel_CurvePoint(uint32_t model,
                        uint32_t leg,
                        uint32_t index,
                        float *vds_v,
                        float *cs_pf);

/* 在任意 Vds 处查参考值(单位 pF / pF / 无量纲)。
   网格外按端点夹住;Vds <= 0 视为无效。成功返回 1。
   cs: 串联等效电容(与实测 Cs 同一口径)
   cp: 并联等效电容 = cs / (1 + D^2)
   d : |R / X|                                                */
int RefModel_At(uint32_t model,
                uint32_t leg,
                float vds_v,
                float *cs_pf,
                float *cp_pf,
                float *d);

/* 典型 Rg(Ω)。它是 Vgs −3 .. +3 V 中位曲线的中位数,与偏压无关。 */
int RefModel_Rg(uint32_t model, float *rg_ohm);

#ifdef __cplusplus
}
#endif

#endif /* REF_MODEL_H */
