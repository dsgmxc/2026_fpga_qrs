/**
 * @file calibration.c
 * @brief @ref calibration.h 的实现 —— 三标准 Möbius 拟合、反演、存盘
 *
 * 【本文件只做数学与存储,不碰寄存器、不碰 LVGL】
 *   采集由 `pynq_protocol.c` 的状态机驱动,界面在 `ui/ui_demo.c`。
 *   这样三件事可以分别验证:数学有宿主机单测,采集有板上日志,界面有抓帧。
 *
 * 【存盘位置】
 *   `/home/xilinx/cv_calibration.dat` —— 板上 `/home/xilinx` 跨重启保留;
 *   宿主机单测用环境变量 `CV_CAL_PATH` 指到 `/tmp`,不碰板上的真文件。
 *   路径不可写时 save 会如实返回 false,不算错误。
 *
 * 【为什么用 packed 结构】
 *   文件会被"下一次构建的同一个程序"读回,但仍按字节布局写死,避免哪天
 *   编译器/结构体对齐变了读出一堆 NaN。CRC 用来挡住截断和半写。
 */

#include "calibration.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 常数
 * ========================================================================== */

#define CAL_PATH        "/home/xilinx/cv_calibration.dat"
#define CAL_MAGIC       0x5143414Cu   /* "QCAL" */
/* v2(2026-10-27):标准板 2 的四个字段 + 每点的 flags 扩到 16 位 + load2 实测。
 * v1 文件会被拒(版本号不符),旧校准数据需要重采 —— 这是有意的:
 * 布局变了却"尽量兼容"比直接拒更危险。 */
#define CAL_VERSION     2u

/* 开路实测与负载实测至少差这么多倍,才把开路当作有效标准;
 * 否则开路那点信号已接近噪声,拟合出的 c 会把噪声放大。 */
#define CAL_OPEN_MIN_RATIO  5.0

/* 仅开路平行校正:开路实测本身得像个"开路"(太少就只是噪声/短路),
 * 且反演分母 |1 − Z_m/M_o| 不能太小 —— 否则等于把噪声放大几十倍。 */
#define CAL_OPEN_ONLY_MIN_Z    10.0   /* Ω */
#define CAL_OPEN_ONLY_MIN_DEN  0.1    /* Z_d 至少贡献 10% 的并联导纳 */

/* 🔴 2026-10-24 实测:同一物理开路在四个挡位上量出的 |M_o| 相差 150–1500 倍
 * (夹具真实杂散 <1 pF,低挡位全被通道地板占满)。**开路是物理量,与 R0 无关** ——
 * 四挡差这么多说明那根本不是夹具阻抗。差超过这个倍数就判"开路不可信",
 * 宁可退到 SHORT+LOAD,也不拿一个假的开路去拟合。 */
#define CAL_OPEN_MAX_GEAR_SPREAD  3.0

/* 🆕 2026-10-27 两负载仿射:两个已知负载的标称阻抗必须拉开,
 * 否则 a = (M2−M1)/(Z2−Z1) 的两个方程几乎重合,测量噪声按 1/间隔 放大。
 * 判据用**相对量级**:|Z2−Z1| ≥ 5% × max(|Z1|,|Z2|)。
 * 取 5% 是折中:再严格(10%)会把"两颗同型但离散 10% 的器件"判死;
 * 再松(1%)放大 100 倍噪声,拟合出来数字好看但没有意义。 */
#define CAL_LOAD_SEP_MIN  0.05

#define CAL_EPS_REL     1e-12

/* 反演结果的物理合理范围。超出不是"器件",是分母接近 0。 */
#define CAL_Z_MAX_OHM   1e12
#define CAL_Z_MIN_OHM   1e-6

static const uint32_t s_freq_hz[CAL_FREQ_COUNT] =
{
    10000u, 100000u, 1000000u
};

#define CAL_TWO_PI  6.283185307179586476925286766559

/* ==========================================================================
 * 文件布局
 * ========================================================================== */

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t crc;              /* 全缓冲区(本字段置 0)的 CRC32 */
    CalBoard board;            /* 10 × double = 80 B(v2 起) */
} CalFileHeader;

typedef struct __attribute__((packed))
{
    uint8_t  leg;              /* 0..3   */
    uint8_t  gear;             /* 0..3   */
    uint8_t  freq;             /* 0..2   */
    uint8_t  reserved;
    uint16_t flags;            /* CAL_F_*(v2 起 16 位) */
    uint16_t reserved2;
    double   open_re;
    double   open_im;
    double   short_re;
    double   short_im;
    double   load_re;          /* 标准板 1 实测 */
    double   load_im;
    double   load2_re;         /* 标准板 2 实测(🆕) */
    double   load2_im;
    double   a_re;
    double   a_im;
    double   b_re;
    double   b_im;
    double   c_re;
    double   c_im;
} CalFilePoint;

/* v2:4×4 + 80 = 96 B 的头;点为 8 + 14×8 = 120 B。
 * 头的 8 字节前缀 + 两个保留字是为了让 double 自然 8 字节对齐(不是必须,
 * 但读起来省心)。改布局必须同时改本断言与 `CAL_VERSION`
 * (已存的文件会因版本号不符被拒)。 */
_Static_assert(sizeof(CalFileHeader) == 96u, "CalFileHeader 布局变了,版本号必须跟着改");
_Static_assert(sizeof(CalFilePoint) == 120u, "CalFilePoint 布局变了,版本号必须跟着改");

/* ==========================================================================
 * 状态
 * ========================================================================== */

static CalFilePoint s_points[CAL_POINT_COUNT];
static CalBoard     s_board;
static int          s_enabled;
static char         s_message[160];

/* 当前正在采集的标准。只影响 StoreRaw 往哪一组字段写。 */
static CalStandard  s_collecting = CAL_STD_OPEN;
static uint32_t     s_collect_ok;
static uint32_t     s_collect_bad;

/* ==========================================================================
 * 复数小工具
 * ========================================================================== */

static CalZ cz(double re, double im)
{
    CalZ z;

    z.re = re;
    z.im = im;
    return z;
}

static CalZ csub(CalZ x, CalZ y) { return cz(x.re - y.re, x.im - y.im); }

static CalZ cmul(CalZ x, CalZ y)
{
    return cz((x.re * y.re) - (x.im * y.im),
              (x.re * y.im) + (x.im * y.re));
}

static CalZ cdiv(CalZ x, CalZ y)
{
    double d = (y.re * y.re) + (y.im * y.im);

    if (d <= 0.0) {
        return cz(0.0, 0.0);
    }
    return cz(((x.re * y.re) + (x.im * y.im)) / d,
              ((x.im * y.re) - (x.re * y.im)) / d);
}

static double cabs2(CalZ x) { return (x.re * x.re) + (x.im * x.im); }
static double cabs_(CalZ x) { return sqrt(cabs2(x)); }

static int cfinite(CalZ x)
{
    return isfinite(x.re) && isfinite(x.im);
}

/* ==========================================================================
 * CRC32(逐位,不建表)—— 文件只有几 KB,省一张 1 KB 表
 * ========================================================================== */

static uint32_t crc32_bytes(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    uint32_t bit;

    for (i = 0u; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t crc_of_buffer(const uint8_t *buf, size_t len, size_t crc_offset)
{
    /* 复制一份把 crc 字段清零再算 —— 不就地改调用方的缓冲区。 */
    uint8_t tmp[sizeof(CalFileHeader) +
                (CAL_POINT_COUNT * sizeof(CalFilePoint))];
    uint32_t zero = 0u;

    if (len != sizeof(tmp)) {
        return 0u;
    }
    (void)memcpy(tmp, buf, len);
    (void)memcpy(&tmp[crc_offset], &zero, sizeof(zero));
    return crc32_bytes(tmp, len);
}

/* ==========================================================================
 * 索引
 * ========================================================================== */

static uint32_t point_index(uint32_t leg, uint32_t gear, uint32_t freq)
{
    return (leg * CAL_GEAR_COUNT + gear) * CAL_FREQ_COUNT + freq;
}

static int gear_index(uint32_t onehot)
{
    switch (onehot & 0xFu) {
    case 0x1u: return 0;
    case 0x2u: return 1;
    case 0x4u: return 2;
    case 0x8u: return 3;
    default:   return -1;
    }
}

int32_t Calibration_FreqIndex(uint32_t hz)
{
    uint32_t i;

    for (i = 0u; i < CAL_FREQ_COUNT; ++i) {
        if (hz == s_freq_hz[i]) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ==========================================================================
 * 标准板 → 每腿负载阻抗
 * ========================================================================== */

/** 标准板 → 每腿负载阻抗的公共实现;两组参数只有这四个字段不同。 */
static bool standard_z_from(double cgs_pf, double cgd_pf, double cds_pf,
                            double rg_ohm, uint32_t leg, uint32_t hz,
                            CalZ *out)
{
    double w;
    double c_pf;

    if ((out == NULL) || (hz == 0u)) {
        return false;
    }
    w = CAL_TWO_PI * (double)hz;

    switch (leg) {
    case 0u:   /* CISS = Cgs + Cgd,栅极电阻串联(没有就填 0) */
        c_pf = cgs_pf + cgd_pf;
        if (c_pf <= 0.0) {
            return false;
        }
        *out = cz(rg_ohm, -1e12 / (w * c_pf));
        return true;
    case 1u:   /* COSS = Cds + Cgd */
        c_pf = cds_pf + cgd_pf;
        if (c_pf <= 0.0) {
            return false;
        }
        *out = cz(0.0, -1e12 / (w * c_pf));
        return true;
    case 2u:   /* CRSS = Cgd(数据手册定义) */
        c_pf = cgd_pf;
        if (c_pf <= 0.0) {
            return false;
        }
        *out = cz(0.0, -1e12 / (w * c_pf));
        return true;
    case 3u:   /* RG = Rg */
        if (rg_ohm <= 0.0) {
            return false;
        }
        *out = cz(rg_ohm, 0.0);
        return true;
    default:
        return false;
    }
}

bool Calibration_StandardZ(uint32_t leg, uint32_t hz, CalZ *out)
{
    return standard_z_from(s_board.cgs_pf, s_board.cgd_pf,
                           s_board.cds_pf, s_board.rg_ohm, leg, hz, out);
}

bool Calibration_StandardZ2(uint32_t leg, uint32_t hz, CalZ *out)
{
    return standard_z_from(s_board.cgs2_pf, s_board.cgd2_pf,
                           s_board.cds2_pf, s_board.rg2_ohm, leg, hz, out);
}

bool Calibration_LegDefined(uint32_t leg)
{
    CalZ z;

    return Calibration_StandardZ(leg, s_freq_hz[2], &z);
}

bool Calibration_LegDefined2(uint32_t leg)
{
    CalZ z;

    return Calibration_StandardZ2(leg, s_freq_hz[2], &z);
}

/* ==========================================================================
 * 拟合
 * ========================================================================== */

/**
 * @brief 重算一个点。
 *
 * 五条路径(按可用性降级,不是"猜"):
 *   1. OPEN(有限且 ≥ 5×负载)+ SHORT + LOAD → 完整 Möbius;
 *   2. SHORT + LOAD                          → 仿射(c = 0);
 *   3. LOAD1 + LOAD2                         → 两点仿射(c = 0,🆕);
 *   4. OPEN + LOAD                           → b = 0 的 Möbius;
 *   5. 只有 OPEN                             → 平行杂散校正(a = 1,b = 0,
 *                                              c = 1/M_o),标记 FIT_OPEN。
 * 一条都不满足 ⇒ 清掉 FIT,应用时不修正。
 *
 * 路径 1/2/4 里的 LOAD 可以是**任意一组**标准板参数:主负载优先标准板 1,
 * 它未定义时才用标准板 2(见下方 `zl = zl1_ok ? zl1 : zl2`)。
 */
/**
 * @brief 开路标准在四个挡位上是否讲得通。
 *
 * 开路阻抗是物理量,和 R0 无关 —— 四挡量出来必须差不多。差到 3 倍以上,
 * 说明至少三个挡位量的是**通道地板**(增益/相位失配),不是夹具。
 * 这种情况下开路不能当标准用,否则 `c = 1/M_o` 会把地板当成杂散减掉。
 */
static int open_is_consistent(uint32_t leg, uint32_t freq_idx)
{
    double lo = 0.0;
    double hi = 0.0;
    uint32_t n = 0u;
    uint32_t g;

    for (g = 0u; g < CAL_GEAR_COUNT; ++g) {
        const CalFilePoint *q = &s_points[point_index(leg, g, freq_idx)];
        double mag;

        if (((q->flags & CAL_F_OPEN) == 0u) ||
            ((q->flags & CAL_F_OPEN_INF) != 0u)) {
            continue;
        }
        mag = cabs_(cz(q->open_re, q->open_im));
        if (!isfinite(mag) || (mag <= 0.0)) {
            continue;
        }
        if ((n == 0u) || (mag < lo)) { lo = mag; }
        if ((n == 0u) || (mag > hi)) { hi = mag; }
        ++n;
    }

    return (n == CAL_GEAR_COUNT) && (lo > 0.0) &&
           (hi <= (CAL_OPEN_MAX_GEAR_SPREAD * lo));
}

static uint32_t refit_point(CalFilePoint *p)
{
    CalZ zl1;
    CalZ zl2;
    CalZ zl;
    CalZ ml1, ml2;
    CalZ ml, ms, mo;
    CalZ a, b, c;
    int zl1_ok = 0;
    int zl2_ok = 0;
    int have_open;
    int have_short;
    int have_load;
    int use_open;
    int solved = 0;
    int open_only = 0;

    p->flags &= (uint16_t)~(CAL_F_FIT | CAL_F_FIT_OPEN);

    if ((p->leg >= CAL_LEG_COUNT) ||
        (p->gear >= CAL_GEAR_COUNT) ||
        (p->freq >= CAL_FREQ_COUNT)) {
        return 0u;
    }

    have_open  = ((p->flags & CAL_F_OPEN) != 0u) &&
                 ((p->flags & CAL_F_OPEN_INF) == 0u);
    have_short = (p->flags & CAL_F_SHORT) != 0u;

    /* 负载点:标称值取决于**哪一组**标准板参数,所以两组分开算。 */
    if ((p->flags & CAL_F_LOAD) != 0u) {
        zl1_ok = Calibration_StandardZ(p->leg, s_freq_hz[p->freq], &zl1);
    }
    if ((p->flags & CAL_F_LOAD2) != 0u) {
        zl2_ok = Calibration_StandardZ2(p->leg, s_freq_hz[p->freq], &zl2);
    }
    have_load = zl1_ok || zl2_ok;

    /* 四挡对不上的开路不是夹具阻抗,是通道地板 —— 不能当标准用。 */
    if (have_open && !open_is_consistent(p->leg, p->freq)) {
        have_open = 0;
    }

    ml1 = cz(p->load_re, p->load_im);
    ml2 = cz(p->load2_re, p->load2_im);
    ms = cz(p->short_re, p->short_im);
    mo = cz(p->open_re, p->open_im);

    /* 单负载路径用"主负载":标准板 1 优先(它的参数是用户先填的那组),
     * 未定义时才退回标准板 2。 */
    zl = zl1_ok ? zl1 : zl2;
    ml = zl1_ok ? ml1 : ml2;

    a = cz(0.0, 0.0);
    b = cz(0.0, 0.0);
    c = cz(0.0, 0.0);

    if (!have_load) {
        /* 只有开路:做**平行杂散校正**。把夹具看成与器件并联的阻抗,
         *   1/Z_m = 1/Z_d + 1/M_o  ⇒  Z_d = 1/(1/Z_m − 1/M_o),
         * 即 a = 1、b = 0、c = 1/M_o。它吸收掉夹具并联电容(本项目实测的
         * 主要误差),但**不识别增益与串联项** —— 那必须有已知负载。
         * 标记 CAL_F_FIT_OPEN,有标准板之后 refit 会自动升级成完整路径。 */
        if (have_open && cfinite(mo) && (cabs_(mo) >= CAL_OPEN_ONLY_MIN_Z)) {
            CalZ c_open = cdiv(cz(1.0, 0.0), mo);

            if (cfinite(c_open)) {
                a = cz(1.0, 0.0);
                c = c_open;
                solved = 1;
                open_only = 1;
            }
        }
    } else {
        /* 有负载才需要知道标准板长什么样 —— `have_load` 已经保证主负载
         * 的标称阻抗解出来了,这里不再重复判断。 */

        /* 开路必须明显大于负载,否则那点信号在噪声底里,拟合出的 c 不可信。 */
        use_open = have_open &&
                   (cabs_(mo) > (CAL_OPEN_MIN_RATIO * cabs_(ml))) &&
                   cfinite(mo);

        /* 路径 1:三标准完整 Möbius */
        if (have_short && use_open) {
            CalZ denom = cmul(zl, csub(mo, ml));

            if (cabs_(denom) > CAL_EPS_REL) {
                c = cdiv(csub(ml, ms), denom);
                a = cmul(c, mo);
                b = ms;
                solved = 1;
            }
            /* 分母退化 → 掉到路径 2(仿射);不是错误,开路那点不能用而已 */
        }

        /* 路径 2:SHORT + LOAD 仿射 */
        if (!solved && have_short) {
            a = cdiv(csub(ml, ms), zl);
            b = ms;
            c = cz(0.0, 0.0);
            solved = 1;
        }

        /* 路径 3:LOAD1 + LOAD2 两点仿射(🆕 2026-10-27)
         *   两个已知 Z_d 定仿射的两个未知量:
         *       a = (M2 − M1)/(Z2 − Z1),  b = M1 − a·Z1,  c = 0
         *   两个负载标称值太接近时**不拟合** —— 见 `CAL_LOAD_SEP_MIN`。 */
        if (!solved && zl1_ok && zl2_ok) {
            CalZ dz = csub(zl2, zl1);
            double scale = cabs_(zl1);

            if (cabs_(zl2) > scale) {
                scale = cabs_(zl2);
            }
            if ((cabs_(dz) > CAL_EPS_REL) &&
                (cabs_(dz) >= (CAL_LOAD_SEP_MIN * scale))) {
                a = cdiv(csub(ml2, ml1), dz);
                b = csub(ml1, cmul(a, zl1));
                c = cz(0.0, 0.0);
                solved = 1;
            }
        }

        /* 路径 4:OPEN + LOAD(b = 0) */
        if (!solved && use_open) {
            CalZ denom = cmul(zl, csub(mo, ml));

            if (cabs_(denom) > CAL_EPS_REL) {
                c = cdiv(ml, denom);
                a = cmul(c, mo);
                b = cz(0.0, 0.0);
                solved = 1;
            }
        }
    }

    if (!solved) {
        return 0u;
    }

    if (!cfinite(a) || !cfinite(b) || !cfinite(c)) {
        return 0u;
    }
    if (cabs_(a) <= CAL_EPS_REL) {
        return 0u;
    }

    p->a_re = a.re;  p->a_im = a.im;
    p->b_re = b.re;  p->b_im = b.im;
    p->c_re = c.re;  p->c_im = c.im;
    p->flags |= CAL_F_FIT;
    if (open_only) {
        p->flags |= CAL_F_FIT_OPEN;
    }
    return 1u;
}

static void refit_all(void)
{
    uint32_t i;

    for (i = 0u; i < CAL_POINT_COUNT; ++i) {
        (void)refit_point(&s_points[i]);
    }
}

/* ==========================================================================
 * 存 / 取
 * ========================================================================== */

/** 存盘路径:`CV_CAL_PATH` 可覆盖(宿主单测用,免得写到板上真文件)。 */
static const char *cal_path(void)
{
    const char *env = getenv("CV_CAL_PATH");

    return ((env != NULL) && (env[0] != '\0')) ? env : CAL_PATH;
}

bool Calibration_Save(void)
{
    uint8_t buf[sizeof(CalFileHeader) +
                (CAL_POINT_COUNT * sizeof(CalFilePoint))];
    CalFileHeader *h = (CalFileHeader *)buf;
    char tmp_path[256];
    const char *path = cal_path();
    FILE *f;
    size_t n;

    (void)memset(buf, 0, sizeof(buf));
    h->magic   = CAL_MAGIC;
    h->version = CAL_VERSION;
    h->count   = CAL_POINT_COUNT;
    h->board   = s_board;
    (void)memcpy(&buf[sizeof(CalFileHeader)], s_points, sizeof(s_points));

    h->crc = crc_of_buffer(buf, sizeof(buf),
                           offsetof(CalFileHeader, crc));

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "wb");
    if (f == NULL) {
        (void)snprintf(s_message, sizeof(s_message),
                       "存盘失败:打不开 %.120s", tmp_path);
        return false;
    }
    n = fwrite(buf, 1u, sizeof(buf), f);
    if ((n != sizeof(buf)) || (fclose(f) != 0)) {
        (void)snprintf(s_message, sizeof(s_message),
                       "存盘失败:写入 %u/%u 字节",
                       (unsigned)n, (unsigned)sizeof(buf));
        (void)remove(tmp_path);
        return false;
    }
    if (rename(tmp_path, path) != 0) {
        (void)snprintf(s_message, sizeof(s_message),
                       "存盘失败:rename 到 %.120s", path);
        (void)remove(tmp_path);
        return false;
    }
    (void)snprintf(s_message, sizeof(s_message), "已存盘");
    return true;
}

static bool load_file(void)
{
    uint8_t buf[sizeof(CalFileHeader) +
                (CAL_POINT_COUNT * sizeof(CalFilePoint))];
    const CalFileHeader *h = (const CalFileHeader *)buf;
    FILE *f;
    size_t n;
    uint32_t crc;

    f = fopen(cal_path(), "rb");
    if (f == NULL) {
        return false;
    }
    n = fread(buf, 1u, sizeof(buf), f);
    (void)fclose(f);
    if (n != sizeof(buf)) {
        return false;
    }
    if ((h->magic != CAL_MAGIC) || (h->version != CAL_VERSION) ||
        (h->count != CAL_POINT_COUNT)) {
        return false;
    }
    crc = crc_of_buffer(buf, sizeof(buf), offsetof(CalFileHeader, crc));
    if (crc != h->crc) {
        return false;
    }

    s_board = h->board;
    (void)memcpy(s_points, &buf[sizeof(CalFileHeader)], sizeof(s_points));
    return true;
}

/* ==========================================================================
 * 公共接口
 * ========================================================================== */

bool Calibration_Init(void)
{
    (void)memset(s_points, 0, sizeof(s_points));
    s_board.cgs_pf   = 0.0;
    s_board.cgd_pf   = 0.0;
    s_board.cds_pf   = 0.0;
    s_board.rg_ohm   = 0.0;
    s_board.cgs2_pf  = 0.0;
    s_board.cgd2_pf  = 0.0;
    s_board.cds2_pf  = 0.0;
    s_board.rg2_ohm  = 0.0;
    s_board.rating_v = 100.0;
    s_board.bias_v   = 1.0;
    s_enabled        = 1;              /* 有数据就应用;没数据 Apply 自然返回 false */
    (void)snprintf(s_message, sizeof(s_message), "未设置标准板参数");

    if (load_file()) {
        refit_all();
        (void)snprintf(s_message, sizeof(s_message), "已读入校准文件");
        return true;
    }

    refit_all();                       /* 全空,不会拟合出任何点 */
    return false;
}

void Calibration_SetEnabled(bool on)
{
    s_enabled = on ? 1 : 0;
}

bool Calibration_IsEnabled(void)
{
    return s_enabled != 0;
}

void Calibration_GetBoard(CalBoard *out)
{
    if (out != NULL) {
        *out = s_board;
    }
}

static double clampd(double v, double lo, double hi)
{
    if (!isfinite(v)) {
        return lo;
    }
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

bool Calibration_SetBoardField(CalField field, double value)
{
    switch (field) {
    case CAL_FIELD_CGS:    s_board.cgs_pf   = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_CGD:    s_board.cgd_pf   = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_CDS:    s_board.cds_pf   = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_RG:     s_board.rg_ohm   = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_RATING: s_board.rating_v = clampd(value, 0.0, 1000.0); break;
    case CAL_FIELD_BIAS:   s_board.bias_v   = clampd(value, 0.1, 100.0);  break;
    case CAL_FIELD_CGS2:   s_board.cgs2_pf  = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_CGD2:   s_board.cgd2_pf  = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_CDS2:   s_board.cds2_pf  = clampd(value, 0.0, 1.0e6); break;
    case CAL_FIELD_RG2:    s_board.rg2_ohm  = clampd(value, 0.0, 1.0e6); break;
    default:
        return false;
    }

    /* 标准板的 C/R 变了 ⇒ 负载标称值变了 ⇒ 所有拟合都要重算。
     * 原始实测值不动,所以用户"先采完再改参数"也能立刻看到新结果。 */
    refit_all();
    (void)snprintf(s_message, sizeof(s_message),
                   "标准板参数已更新,已标定 %u 点",
                   (unsigned)Calibration_TotalFitted());
    return Calibration_Save();
}

void Calibration_BeginStandard(CalStandard standard)
{
    s_collecting  = standard;
    s_collect_ok  = 0u;
    s_collect_bad = 0u;
}

bool Calibration_StoreRaw(CalStandard standard,
                          uint32_t leg, uint32_t gear, uint32_t hz,
                          double z_re, double z_im, int weak)
{
    int32_t fi = Calibration_FreqIndex(hz);
    CalFilePoint *p;
    uint16_t weak_flag;

    if ((leg >= CAL_LEG_COUNT) || (gear >= CAL_GEAR_COUNT) ||
        (fi < 0) || !isfinite(z_re) || !isfinite(z_im)) {
        s_collect_bad++;
        return false;
    }

    p = &s_points[point_index(leg, gear, (uint32_t)fi)];
    p->leg  = (uint8_t)leg;
    p->gear = (uint8_t)gear;
    p->freq = (uint8_t)fi;

    /* 每一种标准只写自己那一组字段,其它标准保留 —— 用户可以先采开路、
     * 再采短接,中间换标准板不会把前面的数据冲掉。 */
    switch (standard) {
    case CAL_STD_OPEN:
        p->open_re  = z_re;
        p->open_im  = z_im;
        p->flags   |= CAL_F_OPEN;
        p->flags   &= (uint16_t)~CAL_F_OPEN_INF;
        weak_flag   = CAL_F_WEAK_OPEN;
        break;
    case CAL_STD_SHORT:
        p->short_re = z_re;
        p->short_im = z_im;
        p->flags   |= CAL_F_SHORT;
        weak_flag   = 0u;
        break;
    case CAL_STD_LOAD:
        p->load_re  = z_re;
        p->load_im  = z_im;
        p->flags   |= CAL_F_LOAD;
        weak_flag   = CAL_F_WEAK_LOAD;
        break;
    case CAL_STD_LOAD2:
        p->load2_re = z_re;
        p->load2_im = z_im;
        p->flags   |= CAL_F_LOAD2;
        weak_flag   = CAL_F_WEAK_LOAD2;
        break;
    default:
        s_collect_bad++;
        return false;
    }

    if (weak_flag != 0u) {
        if (weak) {
            p->flags |= weak_flag;
        } else {
            p->flags &= (uint16_t)~weak_flag;
        }
    }

    p->flags &= (uint16_t)~CAL_F_FIT;  /* 等 EndStandard 统一重拟合 */
    s_collect_ok++;
    (void)standard;
    return true;
}

bool Calibration_StoreOpenInfinite(uint32_t leg, uint32_t gear, uint32_t hz)
{
    int32_t fi = Calibration_FreqIndex(hz);
    CalFilePoint *p;

    if ((leg >= CAL_LEG_COUNT) || (gear >= CAL_GEAR_COUNT) || (fi < 0)) {
        s_collect_bad++;
        return false;
    }

    p = &s_points[point_index(leg, gear, (uint32_t)fi)];
    p->leg  = (uint8_t)leg;
    p->gear = (uint8_t)gear;
    p->freq = (uint8_t)fi;
    p->open_re = 0.0;
    p->open_im = 0.0;
    p->flags |= (uint16_t)(CAL_F_OPEN | CAL_F_OPEN_INF);
    p->flags &= (uint16_t)~(CAL_F_FIT | CAL_F_WEAK_OPEN);
    s_collect_ok++;
    return true;
}

/** 有多少点走的是"仅开路平行校正"这条弱路径。 */
static uint32_t open_only_count(void)
{
    uint32_t i;
    uint32_t n = 0u;

    for (i = 0u; i < CAL_POINT_COUNT; ++i) {
        if ((s_points[i].flags & CAL_F_FIT_OPEN) != 0u) {
            ++n;
        }
    }
    return n;
}

static const char *cal_std_label(CalStandard standard)
{
    switch (standard) {
    case CAL_STD_OPEN:  return "开路";
    case CAL_STD_SHORT: return "短接";
    case CAL_STD_LOAD:  return "标准板1";
    case CAL_STD_LOAD2: return "标准板2";
    default:            return "?";
    }
}

uint32_t Calibration_EndStandard(CalStandard standard)
{
    uint32_t fitted;
    uint32_t open_only;

    refit_all();
    fitted = Calibration_TotalFitted();
    open_only = open_only_count();

    if (open_only > 0u) {
        (void)snprintf(s_message, sizeof(s_message),
                       "%s:采到 %u 点,失败 %u;累计已标定 %u 点"
                       "(其中 %u 点仅开路平行校正)",
                       cal_std_label(standard),
                       (unsigned)s_collect_ok, (unsigned)s_collect_bad,
                       (unsigned)fitted, (unsigned)open_only);
    } else {
        (void)snprintf(s_message, sizeof(s_message),
                       "%s:采到 %u 点,失败 %u;累计已标定 %u 点",
                       cal_std_label(standard),
                       (unsigned)s_collect_ok, (unsigned)s_collect_bad,
                       (unsigned)fitted);
    }

    (void)Calibration_Save();
    return fitted;
}

uint16_t Calibration_PointFlags(uint32_t leg, uint32_t gear,
                                uint32_t freq_index)
{
    if ((leg >= CAL_LEG_COUNT) || (gear >= CAL_GEAR_COUNT) ||
        (freq_index >= CAL_FREQ_COUNT)) {
        return 0u;
    }
    return s_points[point_index(leg, gear, freq_index)].flags;
}

uint32_t Calibration_FittedCount(uint32_t freq_index)
{
    uint32_t leg;
    uint32_t gear;
    uint32_t n = 0u;

    if (freq_index >= CAL_FREQ_COUNT) {
        return 0u;
    }
    for (leg = 0u; leg < CAL_LEG_COUNT; ++leg) {
        for (gear = 0u; gear < CAL_GEAR_COUNT; ++gear) {
            if ((Calibration_PointFlags(leg, gear, freq_index) &
                 CAL_F_FIT) != 0u) {
                ++n;
            }
        }
    }
    return n;
}

uint32_t Calibration_TotalFitted(void)
{
    uint32_t f;
    uint32_t n = 0u;

    for (f = 0u; f < CAL_FREQ_COUNT; ++f) {
        n += Calibration_FittedCount(f);
    }
    return n;
}

const char *Calibration_LastMessage(void)
{
    return s_message;
}

bool Calibration_Apply(uint32_t leg, uint32_t gear_onehot, uint32_t hz,
                       double z_re_in, double z_im_in,
                       double *z_re_out, double *z_im_out)
{
    int gi;
    int32_t fi;
    const CalFilePoint *p;
    CalZ m, a, b, c, den, z;
    double zmag;

    if (!s_enabled || (z_re_out == NULL) || (z_im_out == NULL)) {
        return false;
    }
    gi = gear_index(gear_onehot);
    fi = Calibration_FreqIndex(hz);
    if ((gi < 0) || (fi < 0) || (leg >= CAL_LEG_COUNT)) {
        return false;
    }

    p = &s_points[point_index(leg, (uint32_t)gi, (uint32_t)fi)];
    if ((p->flags & CAL_F_FIT) == 0u) {
        return false;
    }

    m = cz(z_re_in, z_im_in);
    a = cz(p->a_re, p->a_im);
    b = cz(p->b_re, p->b_im);
    c = cz(p->c_re, p->c_im);

    /* Z = (m − b) / (a − c·m) */
    den = csub(a, cmul(c, m));
    if (cabs_(den) <=
        (CAL_EPS_REL * (1.0 + cabs_(a) + cabs_(cmul(c, m))))) {
        return false;               /* 实测点落在开路极点上 ⇒ 无读数 */
    }
    if (((p->flags & CAL_F_FIT_OPEN) != 0u) &&
        (cabs_(den) < CAL_OPEN_ONLY_MIN_DEN)) {
        /* 只有开路时 Z_d = 1/(1/Z_m − 1/M_o),分母就是 1 − Z_m/M_o。
         * 器件电容小于夹具杂散的约 1/9 时,两数相减会把噪声放大十倍以上 ——
         * 宁可报"没校正",也不给一个被放大过的数。 */
        return false;
    }
    z = cdiv(csub(m, b), den);
    if (!cfinite(z)) {
        return false;
    }
    zmag = cabs_(z);
    if ((zmag < CAL_Z_MIN_OHM) || (zmag > CAL_Z_MAX_OHM)) {
        return false;
    }

    *z_re_out = z.re;
    *z_im_out = z.im;
    return true;
}

void Calibration_ClearAll(void)
{
    (void)memset(s_points, 0, sizeof(s_points));
    (void)snprintf(s_message, sizeof(s_message), "已清除全部校准数据");
    (void)Calibration_Save();
}
