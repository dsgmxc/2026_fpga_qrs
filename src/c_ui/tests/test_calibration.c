/* Host-side check for the three-standard calibration math and the file format.
 *
 *   gcc -std=gnu11 -O2 -I src/c_ui/app \
 *       src/c_ui/tests/test_calibration.c src/c_ui/app/calibration.c \
 *       -lm -o /tmp/test_calibration
 *   /tmp/test_calibration
 *
 * 验证的是**数学与存盘**,不是硬件:
 *   1. 已知 Möbius 误差模型 (a·Zd+b)/(c·Zd+1) 下,三种标准能否把 Zd 反演回来;
 *   2. OPEN 为无穷大 / 没有 SHORT / 只有 OPEN(平行杂散校正)/ 只有 SHORT
 *      几种退化情形;
 *   3. 存盘 → 重新 Init → 参数与拟合结果是否原样回来。
 *
 * ⚠️ 存盘路径用 CV_CAL_PATH 指到 /tmp —— 这些用例**绝不允许**写到板上的
 *    `/home/xilinx/cv_calibration.dat`。
 *
 * 不覆盖:继电器/挡位/ADC 的时序(那要上板,见 docs/project_state.md),
 * 也不判断误差模型本身是否足够描述真实前端(那要实测残差)。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calibration.h"

static const uint32_t k_hz = 1000000u;
static const uint32_t k_gears[4] = {0x1u, 0x2u, 0x4u, 0x8u};
static int g_fail;

typedef struct { double re, im; } Zc;

static Zc zadd(Zc x, Zc y) { Zc r = {x.re + y.re, x.im + y.im}; return r; }
static Zc zmul(Zc x, Zc y)
{
    Zc r = {x.re * y.re - x.im * y.im, x.re * y.im + x.im * y.re};
    return r;
}
static Zc zdiv(Zc x, Zc y)
{
    double d = y.re * y.re + y.im * y.im;
    Zc r = {(x.re * y.re + x.im * y.im) / d, (x.im * y.re - x.re * y.im) / d};
    return r;
}
/* 被测件 Zd 经过误差模型后,仪器看到的 Zm */
static Zc zmodel(Zc a, Zc b, Zc c, Zc zd)
{
    return zdiv(zadd(zmul(a, zd), b), zadd(zmul(c, zd), (Zc){1.0, 0.0}));
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

static void set_board(void)
{
    Calibration_SetBoardField(CAL_FIELD_CGS, 1000.0);
    Calibration_SetBoardField(CAL_FIELD_CGD, 50.0);
    Calibration_SetBoardField(CAL_FIELD_CDS, 500.0);
    Calibration_SetBoardField(CAL_FIELD_RG, 5.0);
}

/* 采一轮 16 点(4 腿 × 4 挡),全部由给定误差模型合成。 */
static void acquire(Zc a, Zc b, Zc c,
                    int open_inf, int have_short, int have_load)
{
    uint32_t leg, g;

    for (leg = 0; leg < 4u; ++leg)
    {
        for (g = 0; g < 4u; ++g)
        {
            if (open_inf)
            {
                Calibration_StoreOpenInfinite(leg, g, k_hz);
            }
            else
            {
                Zc mo = zdiv(a, c);   /* Zd → ∞ 时模型值 = a/c */
                Calibration_StoreRaw(CAL_STD_OPEN, leg, g, k_hz,
                                     mo.re, mo.im, 0);
            }

            if (have_short)
            {
                Calibration_StoreRaw(CAL_STD_SHORT, leg, g, k_hz,
                                     b.re, b.im, 0);
            }

            if (have_load)
            {
                CalZ zl;
                Zc m;

                if (!Calibration_StandardZ(leg, k_hz, &zl))
                {
                    printf("FAIL standard board leg %u undefined\n", leg);
                    g_fail++;
                    continue;
                }
                m = zmodel(a, b, c, (Zc){zl.re, zl.im});
                Calibration_StoreRaw(CAL_STD_LOAD, leg, g, k_hz, m.re, m.im, 0);
            }
        }
    }
}

/* 反演三个已知器件,要求相对误差 < 1e-6。返回失败点数。
 * @p wrong_leg:该腿被期望"反演不回来"(守卫降级后只剩仿射);传 -1 表示没有。 */
static int invert_and_compare(Zc a, Zc b, Zc c, const char *name,
                              int wrong_leg)
{
    static const Zc devs[3] = {{50.0, -300.0}, {1000.0, 0.0}, {20.0, 5.0}};
    int bad = 0;
    uint32_t leg, i;

    for (leg = 0; leg < 4u; ++leg)
    {
        for (i = 0; i < 3u; ++i)
        {
            Zc m = zmodel(a, b, c, devs[i]);
            double zr = 0.0, zi = 0.0;

            if (!Calibration_Apply(leg, k_gears[1], k_hz, m.re, m.im,
                                   &zr, &zi))
            {
                if ((int)leg == wrong_leg)
                {
                    continue;   /* 预期的降级 */
                }
                printf("FAIL %s: leg %u dev %u not calibrated\n",
                       name, leg, i);
                bad++;
                continue;
            }
            {
                double err = hypot(zr - devs[i].re, zi - devs[i].im);
                double mag = hypot(devs[i].re, devs[i].im);
                int off = (err > 1e-6 * (1.0 + mag));

                if (off != ((int)leg == wrong_leg))
                {
                    printf("FAIL %s: leg %u dev %u got %.6f%+.6fj "
                           "want %.6f%+.6fj\n",
                           name, leg, i, zr, zi, devs[i].re, devs[i].im);
                    bad++;
                }
            }
        }
    }
    return bad;
}

/* @p want_fitted 为 -1 表示不核对拟合点数(该情形下有腿会被守卫拒掉)。 */
static void case_model(const char *name, Zc a, Zc b, Zc c,
                       int open_inf, int have_short, int have_load,
                       int want_fitted)
{
    uint32_t fitted;
    int bad;

    Calibration_ClearAll();
    set_board();
    acquire(a, b, c, open_inf, have_short, have_load);
    fitted = Calibration_EndStandard(CAL_STD_LOAD);

    if (want_fitted < 0)
    {
        printf("--   %-28s fitted=%u/48\n", name, fitted);
    }
    else
    {
        printf("--   %-28s fitted=%u/48 (want %u)\n",
               name, fitted, (unsigned)want_fitted);
        if (fitted != (uint32_t)want_fitted)
        {
            g_fail++;
        }
    }

    bad = invert_and_compare(a, b, c, name, -1);
    if (bad != 0)
    {
        g_fail += bad;
    }
}

static void case_save_reload(void)
{
    uint32_t fitted;
    double zr = 0.0, zi = 0.0;
    Zc a = {2.0, 0.3};
    Zc b = {15.0, -4.0};
    Zc c = {3e-5, 3e-6};
    Zc m;

    Calibration_ClearAll();
    set_board();
    acquire(a, b, c, 0, 1, 1);
    fitted = Calibration_EndStandard(CAL_STD_LOAD);

    expect_true("save/reload: fitted before save", fitted == 16u);

    /* 重新从文件读 —— 模拟下一次开机 */
    (void)Calibration_Init();
    expect_true("save/reload: Init reports a usable file",
                Calibration_FittedCount(Calibration_FreqIndex(k_hz)) == 16u);

    m = zmodel(a, b, c, (Zc){50.0, -300.0});
    expect_true("save/reload: Apply still works after Init",
                Calibration_Apply(0u, k_gears[1], k_hz, m.re, m.im,
                                  &zr, &zi));
    expect_true("save/reload: recovered Z matches",
                (hypot(zr - 50.0, zi + 300.0) < 1e-6 * 301.0));
}

int main(void)
{
    /* 存盘一律落 /tmp,绝不碰板上的真校准文件 */
    if (setenv("CV_CAL_PATH", "/tmp/qrs_test_calibration.dat", 1) != 0)
    {
        printf("FAIL setenv\n");
        return 1;
    }
    (void)remove("/tmp/qrs_test_calibration.dat");
    /* Init 同时也是"允许应用校准"的开关(默认是关的) */
    expect_true("Init with no file returns false (not an error)",
                Calibration_Init() == false);
    expect_true("Init enables Apply (no data yet => no fit)",
                Calibration_IsEnabled());

    {
        Zc a   = {2.0, 0.3};
        Zc b   = {15.0, -4.0};
        /* c 小一点:否则 CRSS 腿(|Zl|≈3.2 kΩ)的开路/负载比只有 3.4 倍,
           会被 CAL_OPEN_MIN_RATIO 守卫拒掉 —— 那是另一个用例专门验的。 */
        Zc c   = {3e-5, 3e-6};
        Zc a0  = {1.05, 0.02};
        Zc b0  = {3.0, -1.0};
        Zc z0  = {0.0, 0.0};

        /* 完整三标准:分式线性模型可解,1 MHz 这一档 16/16 */
        case_model("full 3-standard (Moebius)", a, b, c, 0, 1, 1, 16);
        /* 误差模型退化成仿射(Zm = a·Zd + b) */
        case_model("affine: open+short+load", a0, b0, z0, 0, 1, 1, 16);
        case_model("affine: openINF+short+load", a0, b0, z0, 1, 1, 1, 16);
        /* 没有 SHORT:b = 0 的路径同样要能解 */
        case_model("no short: open+load", a, z0, c, 0, 0, 1, 16);
    }

    /* 守卫:CRSS 腿的实测负载 |Zl|≈5.9 kΩ,当 c 大到让它与开路值只差 3.4 倍时,
       开路点被判为"不可信",退回仿射(丢 c 那一项)。
       ⇒ 该腿**仍然标记 FIT**,但反演不回真实的非仿射误差 —— 这是已知代价。
       用例把"只有 leg 2 掉精度、其余三腿精确"钉住,免得守卫哪天被悄悄放宽/收紧。 */
    {
        Zc a = {2.0, 0.3};
        Zc b = {15.0, -4.0};
        Zc c = {1e-4, 1e-5};

        Calibration_ClearAll();
        set_board();
        acquire(a, b, c, 0, 1, 1);
        expect_true("guard: weak-open leg still recorded (16/48)",
                    Calibration_EndStandard(CAL_STD_LOAD) == 16u);
        expect_true("guard: only the weak-open leg loses accuracy",
                    invert_and_compare(a, b, c, "guard", 2) == 0);
    }

    /* 只有 OPEN(没有负载):走平行杂散校正 a=1,b=0,c=1/M_o。
       夹具取 10 pF @ 1 MHz ⇒ Z_o ≈ −j15.9 kΩ。 */
    {
        Zc a  = {1.0, 0.0};
        Zc b  = {0.0, 0.0};
        Zc c  = {0.0, 6.2831853e-5};   /* Z_o = 1/c ≈ −j15.9 kΩ = 10 pF @1MHz */
        uint32_t fitted;
        int bad;

        Calibration_ClearAll();
        set_board();
        acquire(a, b, c, 0, 0, 0);
        fitted = Calibration_EndStandard(CAL_STD_OPEN);
        printf("--   %-28s fitted=%u/48 (want 16)\n",
               "open only (parallel stray)", fitted);
        expect_true("open only fits the parallel-stray path", fitted == 16u);
        expect_true("open only: points marked FIT_OPEN",
                    (Calibration_PointFlags(0u, 1u, 2u) &
                     CAL_F_FIT_OPEN) != 0u);
        bad = invert_and_compare(a, b, c, "open only", -1);
        expect_true("open only: inversion matches", bad == 0);

        /* 器件导纳只有杂散的 ~8%(0.8 pF vs 10 pF)⇒ 分母 < 0.1,必须拒绝 */
        {
            Zc tiny = {0.0, -200000.0};
            Zc m = zmodel(a, b, c, tiny);
            double zr = 0.0, zi = 0.0;

            expect_true("open only: device below stray guard is refused",
                        !Calibration_Apply(0u, k_gears[1], k_hz,
                                           m.re, m.im, &zr, &zi));
        }
    }

    /* 2026-10-24 实测形状:同一开路在四挡上依次是 5.2k / 56k / 506k / 804k
       (随 R0 增长的地板)⇒ 不是夹具阻抗,一律作废。 */
    {
        static const double floor_mag[4] =
            {5215.0, 56041.0, 505922.0, 803522.0};
        uint32_t leg, g, fitted;

        Calibration_ClearAll();
        set_board();
        for (leg = 0; leg < 4u; ++leg)
        {
            for (g = 0; g < 4u; ++g)
            {
                Calibration_StoreRaw(CAL_STD_OPEN, leg, g, k_hz,
                                     -floor_mag[g], 0.0, 0);
            }
        }
        fitted = Calibration_EndStandard(CAL_STD_OPEN);
        printf("--   %-28s fitted=%u/48 (want 0)\n",
               "inconsistent open", fitted);
        expect_true("open inconsistent across gears is rejected", fitted == 0u);
    }

    /* 开路不可信、但有 SHORT+LOAD ⇒ 退回仿射,仍然精确 */
    {
        Zc a0 = {1.05, 0.02};
        Zc b0 = {3.0, -1.0};
        Zc z0 = {0.0, 0.0};
        uint32_t leg, g, fitted;
        int bad;

        Calibration_ClearAll();
        set_board();
        acquire(a0, b0, z0, 1, 1, 1);
        for (leg = 0; leg < 4u; ++leg)
        {
            for (g = 0; g < 4u; ++g)
            {
                Calibration_StoreRaw(CAL_STD_OPEN, leg, g, k_hz,
                                     -5215.0 * (double)(1u << (3u * g)),
                                     0.0, 0);
            }
        }
        fitted = Calibration_EndStandard(CAL_STD_LOAD);
        printf("--   %-28s fitted=%u/48 (want 16)\n",
               "bad open + short + load", fitted);
        expect_true("bad open falls back to short+load", fitted == 16u);
        bad = invert_and_compare(a0, b0, z0, "bad open fallback", -1);
        expect_true("bad open fallback stays exact", bad == 0);
    }

    /* 只有 OPEN + SHORT、没有负载:仍能走**仅开路**校正(短路数据留着,
       等有了标准板 refit 会自动升级成完整 Möbius)。 */
    {
        uint32_t leg, g, fitted;

        Calibration_ClearAll();
        set_board();
        for (leg = 0; leg < 4u; ++leg)
        {
            for (g = 0; g < 4u; ++g)
            {
                Calibration_StoreRaw(CAL_STD_OPEN, leg, g, k_hz,
                                     1000.0, 0.0, 0);
                Calibration_StoreRaw(CAL_STD_SHORT, leg, g, k_hz,
                                     1.0, 2.0, 0);
            }
        }
        fitted = Calibration_EndStandard(CAL_STD_OPEN);
        printf("--   %-28s fitted=%u/48 (want 16)\n",
               "open+short (no load)", fitted);
        expect_true("open+short without load falls back to open-only",
                    fitted == 16u);
    }

    /* 只有 SHORT、既没有开路也没有负载 => 不猜,一个点也不拟合 */
    {
        uint32_t leg, g, fitted;

        Calibration_ClearAll();
        set_board();
        for (leg = 0; leg < 4u; ++leg)
        {
            for (g = 0; g < 4u; ++g)
            {
                Calibration_StoreRaw(CAL_STD_SHORT, leg, g, k_hz,
                                     1.0, 2.0, 0);
            }
        }
        fitted = Calibration_EndStandard(CAL_STD_SHORT);
        printf("--   %-28s fitted=%u/48 (want 0)\n",
               "short only", fitted);
        expect_true("short only does not fit", fitted == 0u);
    }

    case_save_reload();

    (void)remove("/tmp/qrs_test_calibration.dat");

    printf(g_fail ? "FAIL (%d)\n" : "ALL OK\n", g_fail);
    return (g_fail != 0) ? 1 : 0;
}
