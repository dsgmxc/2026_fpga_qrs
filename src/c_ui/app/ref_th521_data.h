#ifndef REF_TH521_DATA_H
#define REF_TH521_DATA_H

/* 典型 C-V / Rg 参考表 —— 数据本体在 `ref_th521_data.c`,由
 * `tools/make_th521_reference.py --emit-c` 生成,**不要手改**。
 *
 * 来源:同惠 TH521(仪器)× 十颗 TK9J90T(器件 1..10),2026-07-16;逐点取中位数。
 * 网格:Vds = 0.1 .. 100 V,201 个对数等距点。
 * 单位:V / pF / 无量纲(D)。
 *
 * ⚠️ Cs 是**串联等效电容**(由电抗 X 反算),与本项目的 Cs 定义相同;
 *    D = |R / X|。**Cp 不单独存表** —— 由 Cs / (1 + D^2) 现算,
 *    这样显示的 Cs / Cp / D 三个数始终自洽。夹具串阻只影响 Cp / D,不影响 Cs。 */

#define REF_TH521_LEGS        3u   /* 0 = Ciss, 1 = Coss, 2 = Crss */
#define REF_TH521_CV_POINTS   201u

extern const float g_ref_th521_vds[REF_TH521_CV_POINTS];
extern const float g_ref_th521_cs_pf[REF_TH521_LEGS][REF_TH521_CV_POINTS];
extern const float g_ref_th521_d[REF_TH521_LEGS][REF_TH521_CV_POINTS];

/* 典型 Rg:101 点中位曲线的中位数(Vgs −3 .. +3 V 的平均水平)。 */
extern const float g_ref_th521_rg_ohm;

#endif /* REF_TH521_DATA_H */
