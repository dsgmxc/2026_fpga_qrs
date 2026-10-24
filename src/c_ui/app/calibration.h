/**
 * @file calibration.h
 * @brief 开路 / 短接 / 标准板 三标准校准 —— 数据、拟合与反演
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【为什么需要它】
 *   2026-10-23 的实测数据(同一器件、同一 50 V、三个挡位)显示:
 *
 *     | 挡位 | R0 | Re Z | Im Z |
 *     |---|---|---|---|
 *     | B | 100 Ω | 1.85 kΩ | −12.5 kΩ |
 *     | C | 1 kΩ | 17.7 kΩ | −11.9 kΩ |
 *     | D | 10 kΩ | 175 kΩ | −9.7 kΩ |
 *
 *   `Re Z` 几乎正比于 R0,而 `Im Z` 基本不动。按 `Z = R0·(VH−VL)/VL` 反推,
 *   这正好是**乘法型复数误差** `Z_m = (1 + jωR0·Cp)·Z_d` 的特征 ——
 *   `Cp ≈ 235 pF` 并在**参考电阻两端**(B/C 两挡拟合差 1 %)。
 *   这类误差不能用一次"减掉偏移"修掉,必须按挡位、按频率、按回路分别标定。
 *
 * 【模型】
 *   任何线性误差网络都把被测阻抗映射成一个 **Möbius(分式线性)变换**:
 *
 *       Z_m = (a·Z_d + b) / (c·Z_d + 1)
 *
 *   三个标准各给一个方程,`a / b / c` 唯一确定:
 *
 *       OPEN  (Z_d → ∞): a / c = M_o        (M_o = 开路实测)
 *       SHORT (Z_d = 0 ): b     = M_s        (M_s = 短接实测)
 *       LOAD  (Z_d = Z_L): c = (M_L − M_s) / (Z_L·(M_o − M_L))
 *                           a = c·M_o
 *
 *   反演:
 *
 *       Z_d = (Z_m − b) / (a − c·Z_m)
 *
 *   **OPEN 用"测量值"当参考,不假设理想无穷大** —— 夹具的并联电容本来
 *   就属于误差网络,`a/c = M_o` 这一条把它一起吸收掉(这正是 VNA 的
 *   open 定义)。所以"空载 = 直接拔掉被测件"就够了,不需要知道夹具电容。
 *
 * 【退化与兜底】
 *   · 开路实测接近无穷(`|VL| = 0`)⇒ 记 `CAL_F_OPEN_INF`,用 **SHORT+LOAD**
 *     的仿射模型(`c = 0`,`a = (M_L−M_s)/Z_L`,`b = M_s`);
 *   · 短接没采到 ⇒ 用 **OPEN+LOAD**(`b = 0`,`c = M_L/(Z_L(M_o−M_L))`);
 *   · **只有开路、没有负载** ⇒ 仍可做**仅开路平行杂散校正**
 *     (`a = 1`,`b = 0`,`c = 1/M_o`,即 `Z_d = 1/(1/Z_m − 1/M_o)`),
 *     标记 `CAL_F_FIT_OPEN`。它只减掉与器件并联的夹具杂散,**不识别**
 *     增益 `a` 与串联项 `b` —— 有了标准板后自动升级成上面的路径。
 *   · 加载和开路都没有 ⇒ 该点不拟合、不应用(不猜)。
 *
 * 【口径(必须与实测一起写进报告)】
 *   · 校准是 **(回路, 挡位, 激励频率)** 各一套;换频率必须重采;
 *   · 校准偏压只有一个 —— 误差网络假定**与偏压无关**(继电器/DAC/ADC
 *     通路不随 Vds 变),被测件的变化才是信号;
 *   · 标准板按数据手册定义折算成每腿的负载阻抗:
 *       CISS = Cgs + Cgd(+ Rg 串联),COSS = Cds + Cgd,CRSS = Cgd,RG = Rg。
 *     若测试盒的继电器拓扑与此不同,以实测等效值为准并在报告里说明。
 * ────────────────────────────────────────────────────────────────────────
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAL_LEG_COUNT   4u   /* 0 CISS / 1 COSS / 2 CRSS / 3 RG */
#define CAL_GEAR_COUNT  4u   /* 0 A 10Ω / 1 B 100Ω / 2 C 1kΩ / 3 D 10kΩ */
#define CAL_FREQ_COUNT  3u   /* 0 10 kHz / 1 100 kHz / 2 1 MHz */
#define CAL_POINT_COUNT (CAL_LEG_COUNT * CAL_GEAR_COUNT * CAL_FREQ_COUNT)

/** 三个标准。数值与采集顺序无关,只用来选"这次采的是哪一种"。 */
typedef enum
{
    CAL_STD_OPEN = 0,
    CAL_STD_SHORT = 1,
    CAL_STD_LOAD = 2
} CalStandard;

/** 标准板参数在界面上的字段号(@ref Calibration_SetBoardField)。 */
typedef enum
{
    CAL_FIELD_CGS = 0,     /* pF */
    CAL_FIELD_CGD = 1,     /* pF */
    CAL_FIELD_CDS = 2,     /* pF */
    CAL_FIELD_RG  = 3,     /* Ω  */
    CAL_FIELD_RATING = 4,  /* V  */
    CAL_FIELD_BIAS   = 5,  /* V  */
    CAL_FIELD_COUNT  = 6
} CalField;

/** 标准板定义 + 采集偏压。默认值见 `calibration.c`。 */
typedef struct
{
    double cgs_pf;
    double cgd_pf;
    double cds_pf;
    double rg_ohm;
    double rating_v;   /* 标准板耐压上限(只用来挡住超限的采集) */
    double bias_v;     /* 三种标准共用的采集偏压 */
} CalBoard;

/** 复数(不引入 <complex.h>,便于和 LVGL / 交叉编译共存)。 */
typedef struct
{
    double re;
    double im;
} CalZ;

/** 每个点的状态位(@ref Calibration_PointFlags)。 */
#define CAL_F_OPEN       0x01u
#define CAL_F_SHORT      0x02u
#define CAL_F_LOAD       0x04u
#define CAL_F_FIT        0x08u
#define CAL_F_OPEN_INF   0x10u   /* 开路实测 |VL| = 0 ⇒ 按无穷处理 */
#define CAL_F_WEAK_OPEN  0x20u   /* 开路信号低于噪声门槛 */
#define CAL_F_WEAK_LOAD  0x40u   /* 标准板信号低于噪声门槛 */
#define CAL_F_FIT_OPEN   0x80u   /* 仅开路平行杂散校正(a=1,b=0,c=1/M_o) */

/**
 * @brief 上电调用一次:从 `/home/xilinx/cv_calibration.dat` 读入。
 * @return true = 读到了有效文件;false = 没有/无效,用默认值(不算错误)
 *
 * @note 路径可用环境变量 `CV_CAL_PATH` 覆盖 —— **只给宿主单测用**,
 *       免得测试把板上的真校准文件写掉。
 */
bool Calibration_Init(void);

/* ---- 开关与板参数 ---------------------------------------------------- */

/** @brief 校准总开关。关掉时采集**照常进行**,只是不应用修正。 */
void Calibration_SetEnabled(bool on);
bool Calibration_IsEnabled(void);

void Calibration_GetBoard(CalBoard *out);
/** @brief 改一个字段。会夹到合法范围,并**重新拟合 + 存盘**。 */
bool Calibration_SetBoardField(CalField field, double value);

/** @brief 标准板参数是否足够标定 @p leg(该腿的负载阻抗有限且非零)。 */
bool Calibration_LegDefined(uint32_t leg);

/**
 * @brief 标准板在 @p leg 上、@p hz 下的标称阻抗。
 * @return false = 该腿参数未定义(例如 Cgd = 0),不能当负载点用
 */
bool Calibration_StandardZ(uint32_t leg, uint32_t hz, CalZ *out);

/* ---- 采集 ------------------------------------------------------------ */

/** @brief 开始一种标准的一次采集(不改变已采到的其它标准)。 */
void Calibration_BeginStandard(CalStandard standard);

/**
 * @brief 存一个点的实测原始阻抗。
 * @param leg 0..3
 * @param gear_index 0..3(A/B/C/D)
 * @param hz 当前激励频率
 * @param weak 1 = 该点 VL/VH 低于噪声门槛(只作标记,仍参与拟合)
 * @return true = 已在 [0,3] 范围内并保存
 */
bool Calibration_StoreRaw(CalStandard standard,
                          uint32_t leg, uint32_t gear_index, uint32_t hz,
                          double z_re, double z_im, int weak);

/** @brief 开路实测连 |VL| 都没有 ⇒ 记成"无穷大",拟合时走仿射兜底。 */
bool Calibration_StoreOpenInfinite(uint32_t leg, uint32_t gear_index,
                                   uint32_t hz);

/** @brief 一次采集结束:重拟合所有点并存盘。@return 拟合成功的点数 */
uint32_t Calibration_EndStandard(CalStandard standard);

/* ---- 查询 ------------------------------------------------------------ */

/** @brief 该点的状态位;没有该点返回 0。 */
uint8_t Calibration_PointFlags(uint32_t leg, uint32_t gear_index,
                               uint32_t freq_index);

/** @brief 某个频率下已拟合的点数(0..16)。 */
uint32_t Calibration_FittedCount(uint32_t freq_index);

/** @brief 全部频率已拟合的点数(0..48)。 */
uint32_t Calibration_TotalFitted(void);

/** @brief 频率 → 档位下标(10k/100k/1M);不认识返回 -1。 */
int32_t Calibration_FreqIndex(uint32_t hz);

/** @brief 供日志/界面显示的最后一条结果说明(静态字符串)。 */
const char *Calibration_LastMessage(void);

/* ---- 应用 ------------------------------------------------------------ */

/**
 * @brief 把实测 Z 反演成被测件 Z。
 * @param gear_onehot `0x18` 回读值(0x1/0x2/0x4/0x8)
 * @return true = 已应用(校准开、该点已拟合、反演结果有限);false = 原样使用
 */
bool Calibration_Apply(uint32_t leg, uint32_t gear_onehot, uint32_t hz,
                       double z_re_in, double z_im_in,
                       double *z_re_out, double *z_im_out);

/** @brief 清空全部原始数据与拟合结果并存盘。 */
void Calibration_ClearAll(void);

/** @brief 立刻存盘。@return true = 写入成功 */
bool Calibration_Save(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H */
