/**
 * @file tb_gear_pick.c
 * @brief `cv_range_pick()`(扫压自动换挡的判据)的**主机侧自检型**测试。
 *
 * ⛔ **不连硬件、不开 AXI、不写寄存器。** 被测的只有一件事:
 *    「给一串 `Z`,`cv_range_pick()` 会怎么换挡」。
 *    寄存器通路那一半由 `RelayView_ApplyGear()` 自己保证(它 2026-10-15 已上板验证)。
 *
 * 编译运行(主机,不需要 Vivado、不需要板子):
 *     gcc -std=gnu11 -Wall -Wextra -O2 -I../src/c_ui/app -o /tmp/tb_gear_pick \
 *         sim/tb_gear_pick.c -lm && /tmp/tb_gear_pick
 *
 * 数据来源:`report/measurements/20261015_crss_sweep_manual_gear.md`
 *    —— 2026-10-15 那次 CRSS 扫压(60 点,0.1 V → 100 V,手动 B 挡 100 Ω)
 *    实测的 60 组 `Z`。**就是这次实测暴露出"高压段数据奇怪"**,
 *    这张表是那份实测的原文,不是构造的。
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "pynq_regs.h"      /* 被测函数 cv_range_pick() / cv_range_ohms() 就在这里 */

/* ---------------------------------------------------------------- */
/* 实测输入:CRSS 腿 60 点,Re(Z) / Im(Z),单位 Ω                     */
/* ---------------------------------------------------------------- */
typedef struct { double re; double im; } z_pt;

static const z_pt k_z[60] = {
    {     216.896,     -290.911},
    {     154.838,      -98.833},
    {     158.902,      -93.871},
    {     158.900,      -92.927},
    {     158.416,      -93.470},
    {     157.762,      -94.042},
    {     157.054,      -94.906},
    {     156.401,      -95.656},
    {     155.835,      -96.588},
    {     155.110,      -97.778},
    {     155.096,      -96.202},
    {     153.560,     -100.221},
    {     152.178,     -102.845},
    {     152.489,     -103.927},
    {     153.450,     -105.587},
    {     152.248,     -108.921},
    {     153.184,     -111.918},
    {     154.085,     -115.667},
    {     154.446,     -120.015},
    {     156.940,     -125.259},
    {     159.361,     -131.104},
    {     162.978,     -138.422},
    {     166.170,     -146.797},
    {     170.358,     -155.008},
    {     174.235,     -164.390},
    {     178.157,     -174.295},
    {     181.757,     -185.038},
    {     186.801,     -196.709},
    {     191.549,     -209.404},
    {     196.741,     -223.299},
    {     202.268,     -238.363},
    {     208.631,     -255.053},
    {     214.753,     -273.844},
    {     221.002,     -293.606},
    {     228.511,     -315.542},
    {     236.781,     -339.529},
    {     245.579,     -366.181},
    {     258.582,     -396.941},
    {     324.707,     -422.655},
    {    1463.841,    -3234.756},
    {     942.887,    -4116.187},
    {    1046.920,    -4659.262},
    {    1095.345,    -5114.636},
    {    1155.400,    -5574.816},
    {    1159.494,    -6004.101},
    {    1202.654,    -6490.394},
    {    1326.686,    -6973.920},
    {    1466.897,    -7187.837},
    {    1350.751,    -8145.461},
    {    1125.448,    -8688.822},
    {    1174.209,    -9204.532},
    {    1015.317,    -9736.308},
    {    2122.429,   -10906.807},
    {    3430.288,   -11221.679},
    {    1710.689,   -12221.747},
    {    2311.552,   -12823.680},
    {    2376.130,   -13403.205},
    {    1669.216,   -14183.538},
    {    1942.723,   -14538.144},
    {    1653.202,   -15223.801},
};

/* ================================================================ */
/* 三条跑道                                                          */
/* ================================================================ */

/** 挡位在 `VL/VH = R0/|R0+Z|` 下测到的分压比(百分比)。**相量和的模**,不是标量式。 */
static double vh_pct(double r0, z_pt z)
{
    double dr = r0 + z.re;
    return 100.0 * r0 / sqrt((dr * dr) + (z.im * z.im));
}

static const uint32_t k_bit[4] = {CV_RANGE_A, CV_RANGE_B, CV_RANGE_C, CV_RANGE_D};
static const char *k_name[4] = {"A 10Ω", "B 100Ω", "C 1kΩ", "D 10kΩ"};

static int gear_index(uint32_t onehot)
{
    int i;
    for (i = 0; i < 4; ++i) {
        if (k_bit[i] == onehot) { return i; }
    }
    return -1;
}

typedef struct {
    int      switches;      /* 换挡次数                     */
    int      reversals;     /* 换挡方向反转次数(= 抖动)     */
    double   vh_min;        /* 全程最小的 VL/VH(%)          */
    double   vh_max;
    uint32_t last;
    int      seq_ok;        /* 挡位序号是否单调不减         */
} run_t;

/**
 * @brief 跑一遍。
 * @param margin   迟滞宽度(十倍程);0 = 关掉迟滞
 * @param fixed    非 0 = **不换挡**,永远用这一挡(复现"手动固定挡"的旧行为)
 */
static run_t run(const char *tag, double margin, uint32_t fixed)
{
    run_t r;
    uint32_t gear = CV_RANGE_RESET;     /* 起手 = B 挡(手动没动过时的复位值) */
    int i, prev_dir = 0, prev_idx = gear_index(gear);
    int trace_open = 0;

    memset(&r, 0, sizeof(r));
    r.vh_min = 1e9;
    r.vh_max = -1e9;
    r.seq_ok = 1;

    printf("\n── %s ──\n", tag);
    for (i = 0; i < 60; ++i) {
        double   zm = sqrt((k_z[i].re * k_z[i].re) + (k_z[i].im * k_z[i].im));
        double   vh = vh_pct(cv_range_ohms(gear), k_z[i]);
        uint32_t next;
        int      idx;

        if (vh < r.vh_min) { r.vh_min = vh; }
        if (vh > r.vh_max) { r.vh_max = vh; }

        if (fixed != 0u) {
            continue;
        }
        next = cv_range_pick(zm, gear, margin);
        if (next == 0u) { r.seq_ok = 0; }
        if (next != gear) {
            idx = gear_index(next);
            if (idx < 0) { r.seq_ok = 0; }
            else {
                if (idx < prev_idx) { r.seq_ok = 0; }        /* 挡位往回退了 */
                if (prev_dir != 0 && (idx - prev_idx) * prev_dir < 0) { r.reversals++; }
                prev_dir = idx - prev_idx;
                prev_idx = idx;
            }
            r.switches++;
            if (!trace_open) { printf("   点 %2d |Z| = %8.0f 起:\n", i + 1, zm); trace_open = 1; }
            printf("     🔁 → %-7s |Z| = %8.0f Ω  VL/VH %5.1f%% → %5.1f%%\n",
                   (idx >= 0) ? k_name[idx] : "?", zm, vh,
                   (idx >= 0) ? vh_pct(cv_range_ohms(k_bit[idx]), k_z[i]) : 0.0);
            gear = next;
        }
    }
    r.last = gear;
    printf("   换挡 %d 次(%d 次方向反转)· 挡位序号单调:%s\n",
           r.switches, r.reversals, r.seq_ok ? "是" : "**否**");
    printf("   VL/VH 全程 %.2f%% … %.2f%%  ⇒ 最坏 %.2f%%\n",
           r.vh_min, r.vh_max, r.vh_min);
    printf("   末挡 = %s\n", k_name[gear_index(gear)]);
    return r;
}

/* ================================================================ */
/* ④ 迟滞的**直接**测试:让 |Z| 停在两个挡位的分界线上左右晃           */
/*    B(100Ω) 与 C(1kΩ) 的分界 = √(100·1000) = 316.2 Ω             */
/* ================================================================ */

static int boundary_test(double margin, int n)
{
    uint32_t gear = CV_RANGE_B;
    int i, sw = 0;

    for (i = 0; i < n; ++i) {
        double zm = 316.2 * ((i & 1) ? 1.05 : 0.95);    /* ±5%,跨在分界线上 */
        uint32_t next = cv_range_pick(zm, gear, margin);
        if (next != gear) { sw++; gear = next; }
    }
    return sw;
}

/* ================================================================ */

int main(void)
{
    run_t old_gear, no_hyst, with_hyst;
    int    b0, b15, fails = 0;
    int    n = 40;

    printf("================================================================\n");
    printf(" cv_range_pick() 自检 —— 输入 = 2026-10-15 CRSS 扫压实测的 60 组 Z\n");
    printf("================================================================\n");

    old_gear  = run("① 旧行为:手动固定 B 挡 100 Ω(复现用户看到的\"数据奇怪\")",
                    0.0, CV_RANGE_B);
    no_hyst   = run("② 自动换挡,迟滞 = 0", 0.0, 0u);
    with_hyst = run("③ 自动换挡,迟滞 = 0.15 十倍程(**本次落到板上的配置**)",
                    0.15, 0u);

    printf("\n── ④ 迟滞的作用:让 |Z| 停在 B/C 分界 316.2 Ω 上左右晃 ±5%%,跑 %d 次 ──\n", n);
    b0  = boundary_test(0.0,  n);
    b15 = boundary_test(0.15, n);
    printf("   迟滞 0    → 换挡 %d 次 / %d 次\n", b0, n);
    printf("   迟滞 0.15 → 换挡 %d 次 / %d 次\n", b15, n);

    printf("\n================================================================\n");
    printf(" 判据(自检:每条都要有失败的能力)\n");
    printf("================================================================\n");

#define CHECK(cond, text)                                                   \
    do {                                                                    \
        printf(" [%s] %s\n", (cond) ? "PASS" : "**FAIL**", text);           \
        if (!(cond)) { fails++; }                                           \
    } while (0)

    /* ① 先证明这个测试有失败能力:旧行为必须真的差。不差就说明数据/算式不对。 */
    CHECK(old_gear.vh_min < 1.0,
          "① 固定 B 挡时 VL/VH 会掉到 1% 以下(旧行为**确实**会坏 —— 负对照)");

    /* ③ 本次配置的效果 */
    CHECK(with_hyst.vh_min >= 20.0,
          "③ 迟滞 0.15 时 VL/VH 全程不低于 20%(旧行为最坏 0.64%)");
    CHECK(with_hyst.reversals == 0,
          "③ 换挡方向从不反转(不会在分界线上来回跳)");
    CHECK(with_hyst.seq_ok != 0,
          "③ 挡位序号单调不减(|Z| 单调 ⇒ 挡位只升不降)");
    CHECK(with_hyst.last == CV_RANGE_D,
          "③ 末挡落在 D 10kΩ(100 V 时 |Z| = 15.3 kΩ,本就该用最大挡)");

    /* ② 无迟滞的坏处 —— 必须在**真实数据**上也现形,不能只在④那种人造序列上现形 */
    CHECK(no_hyst.reversals > 0,
          "② 无迟滞时换挡方向发生反转(真实数据上就会来回跳)");
    CHECK(no_hyst.switches >= with_hyst.switches,
          "② 无迟滞的换挡次数不少于有迟滞(迟滞只会减少换挡,不会增加)");
    CHECK(no_hyst.seq_ok == 0,
          "② 无迟滞时挡位序号不单调(退挡 = 曲线换标定基础)");

    /* ④ 迟滞本身 */
    CHECK(b15 < b0,
          "④ 迟滞 0.15 显著减少分界处的抖动(0 迟滞会来回跳)");
    CHECK(b15 <= 1,
          "④ 迟滞 0.15 在分界线上最多只换 1 次(换过去就不回来)");

    printf("\n%s(失败 %d 项)\n", (fails == 0) ? "✅ 全部通过" : "❌ 有失败", fails);
    return (fails == 0) ? 0 : 1;
}
