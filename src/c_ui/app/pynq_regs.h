/**
 * @file pynq_regs.h
 * @brief PS 侧 AXI 寄存器访问 + 物理内存映射(/dev/mem)
 *
 * ⚠️ **本文件里的寄存器偏移与位域是 `src/python/cv_regs.py` 的镜像。**
 *    那份 Python 是单一事实源;改寄存器**先改它**,再同步到这里。
 *    两处不一致的后果是"写进去了、没反应、不报错"。
 *
 * ⚠️ 本进程必须能读 /dev/mem(即必须是 root)。若内核开了 `CONFIG_STRICT_DEVMEM`,
 *    `mmap` 普通 RAM 会被拒 —— 那就是风险 R1,`cv_phys_map()` 会返回 NULL 并给出提示。
 *
 * ⚠️ **为什么用 /dev/mem 而不是 malloc**:PL 读的是**物理内存**,而 malloc 给的是
 *    虚拟地址。帧缓存必须由 Python(`pynq.allocate`,走 CMA)分配后把**物理地址**
 *    传进来,C 侧再映射。
 *
 * ⚠️ 这个映射是**非缓存**的 ⇒ CPU 写进去,PL 立刻看得到,**不需要 flush**。
 *    这正是绕开风险 D1(PS cache 与 PL 读到的不一致)的地方。
 *    代价:非缓存写比缓存写慢(没有 write-back 合并)。UI 大部分是静态重绘,
 *    实测帧率见 report/measurements/。
 */
#ifndef PYNQ_REGS_H
#define PYNQ_REGS_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>       /* log10/fabs —— `cv_range_pick()` 的十倍程判据用 */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 寄存器字节偏移(cv_regs.py 的 REG 表)                                */
/* ------------------------------------------------------------------ */
#define CV_REG_BIAS_CODE     0x14u   /* RW [19:0] → dac_1220.code          */
#define CV_REG_DDS_FREQ      0x80u   /* RW [27:0] → adc_9833.code          */
#define CV_REG_CV_CTRL       0x84u   /* RW [0] dac_rst_n ★ 见下             */

/* CV_CTRL(0x84)—— dac_1220 初始化的放行位。2026-10-05 加。
 *
 * 🔴 **复位值 = 0 = DAC 被按在复位态,初始化不跑。**
 *    DAC1220 没有复位引脚,靠一串**三段高电平的串行复位图案**复位,而那三段是
 *    **双向窗口**(手册 Figure 12 / Table 5):t16 = 208.3~325.5 µs、
 *    t18 = 416.7~732.4 µs、t19 = 833.3~976.6 µs —— **超上界就不触发**。
 *    dac_1220.v 的常数按 50 MHz 折,而本模块跑 PS FCLK0;板上 BOOT.BIN 把
 *    FCLK0 留在 **27.778 MHz** ⇒ 三段变成 468/1026/1620 µs,全部超上界
 *    ⇒ 复位不触发 ⇒ DIR 写入无效 ⇒ 输出停在芯片默认的 **−10 V**。
 *
 * ✅ **放行由 Python 侧独占**(`drivers/board.py::release_dac()`,它先验 FCLK0
 *    在不在 [46.1, 54.0] MHz 窗口内,不在就拒绝)。**C 侧只读不写** ——
 *    避免两个写入方;这里只负责把"还没放行"这件事**喊出来**,不要静默。
 */
#define CV_CTRL_DAC_RST_N    0x1u    /* [0] 1 = 已放行 / 0 = 仍按在复位     */

/* ------------------------------------------------------------------ */
/* 继电器矩阵 + 档位选择(2026-10-10;源 = `src/python/cv_regs.py`)      */
/*   ⚠️ 写 = 请求,读 = **已生效**值;非法请求被拒且状态不变,原因看      */
/*      STATUS.err_code。                                              */
/*   🟢 **写入权归 C 界面(2026-10-14 改)** —— `0x1C` 的唯一写入方是     */
/*      `relay_view.c::RelayView_ApplyCode()`。原文写的是「C 界面本轮  */
/*      只读,写入权归 Python 侧,避免两个写入方」,**那条约定没有兑现**:*/
/*      `drivers/relay.py` 零调用方 ⇒ 硬件一直停在复位值。改动理由和    */
/*      "为什么不是两个写入方"见 `relay_view.c` 的文件头。              */
/*   🟢 **`0x18`(挡位)2026-10-15 起同样归 C 界面写** —— 唯一写入方是   */
/*      `relay_view.c::RelayView_ApplyGear()`。原文是「`0x18` 本轮仍    */
/*      不写 —— 复位值 B 挡 100 Ω,够用」,**那句已作废**:用户接上了    */
/*      挡位矩阵并要求控制它。⛔ 不是 RTL 缺功能,是 C 侧一直没人写。   */
/* ------------------------------------------------------------------ */
#define CV_REG_RELAY_STATUS  0x04u   /* RO [0]busy [1]done [2]fault
                                        [15:8]err_code                      */
#define CV_REG_RANGE_SEL     0x18u   /* RW [3:0] 档位独热码,读回已生效      */
#define CV_REG_RELAY_CFG     0x1Cu   /* RW [6:0] = K1..K7;[7] 保留,恒判非法 */
#define CV_REG_SETTLE_CFG    0x24u   /* RW [31:16] 稳定 ms(复位 40)        */

/* 0x04 位域 */
#define CV_RELAY_STS_BUSY(s)  ((uint32_t)((s) & 0x1u))
#define CV_RELAY_STS_DONE(s)  ((uint32_t)(((s) >> 1) & 0x1u))
#define CV_RELAY_STS_FAULT(s) ((uint32_t)(((s) >> 2) & 0x1u))
#define CV_RELAY_STS_ERR(s)   ((uint32_t)(((s) >> 8) & 0xFFu))

/* STATUS[15:8] err_code(逐字镜像 cv_regs.py 的 RELAY_ERR) */
#define CV_RELAY_ERR_NONE    0x00u   /* 正常                                 */
#define CV_RELAY_ERR_RELAY   0x01u   /* 0x1C 组合不在白名单                   */
#define CV_RELAY_ERR_GEAR    0x02u   /* 0x18 非独热                           */
#define CV_RELAY_ERR_BUSY    0x03u   /* 上一次切换未完成时又写                */

/* 0x18 [3:0] 档位独热码:bit0=A(10Ω) bit1=B(100Ω) bit2=C(1kΩ) bit3=D(10kΩ) */
#define CV_RANGE_A           0x1u
#define CV_RANGE_B           0x2u
#define CV_RANGE_C           0x4u
#define CV_RANGE_D           0x8u
#define CV_RANGE_RESET       0x2u    /* B = 100 Ω(RTL 复位值)               */
#define CV_RANGE_MASK        0xFu

/* 0x1C [6:0] 继电器白名单(镜像 cv_regs.py 的 RELAY_STATE / RELAY_LEGAL)。
 * 🔴 **是白名单,不是黑名单** —— 只有这 7 个值合法,其余(含 [7]=1)一律
 *    err_code=0x01。出处 = 前序 STM32 工程 `relay_matrix.c` 的宏定义与
 *    `BSRR` 写法(高电平吸合),不是从原理图推的。 */
#define CV_RELAY_MASK        0x7Fu
#define CV_RELAY_COSS_DISCH  0x00u   /* COSS_DISCHARGE(全断/放电态)         */
#define CV_RELAY_ALL_OFF     0x10u   /* ALL_OFF = K5 ← **复位值**           */
#define CV_RELAY_CISS_DISCH  0x1Eu   /* CISS_DISCHARGE = K2 K3 K4 K5        */
#define CV_RELAY_RG          0x3Cu   /* RG       = K3 K4 K5 K6              */
#define CV_RELAY_CRSS        0x51u   /* CRSS     = K1 K5 K7                 */
#define CV_RELAY_COSS        0x61u   /* COSS     = K1 K6 K7                 */
#define CV_RELAY_CISS        0x7Fu   /* CISS     = K1..K7 全合              */

/* ------------------------------------------------------------------ */
/* AD9238 采集通路(2026-10-06 进位流)                                  */
/* ------------------------------------------------------------------ */
#define CV_REG_ADC_CFG       0x88u   /* RW [7:0]DIV [15:8]PH_OFF(s) [16]CONT
                                        [17]OEB_N [18]PDN                   */
#define CV_REG_ADC_CTRL      0x8Cu   /* W  [0]GO —— ★ 命令,1 拍脉冲,读回恒 0 */
#define CV_REG_ADC_STS       0x90u   /* RO [0]mmcm_locked [1]done [2]busy
                                        [3]otr_sticky                       */
#define CV_REG_ADC_BUF_IDX   0x94u   /* RW [13:0] 采样缓冲读指针(16384 深) */
#define CV_REG_ADC_BUF_DATA  0x98u   /* RO [11:0]sample [12]OTR            */

/* 0x90 位域 */
#define CV_ADC_STS_LOCKED(s) ((uint32_t)((s) & 0x1u))
#define CV_ADC_STS_DONE(s)   ((uint32_t)(((s) >> 1) & 0x1u))
#define CV_ADC_STS_BUSY(s)   ((uint32_t)(((s) >> 2) & 0x1u))
#define CV_ADC_STS_OTR(s)    ((uint32_t)(((s) >> 3) & 0x1u))

/* 0x98 位域 */
#define CV_ADC_SAMPLE(d)     ((uint32_t)((d) & 0xFFFu))
#define CV_ADC_OTR(d)        ((uint32_t)(((d) >> 12) & 0x1u))

/* 0x88 [17]/[18] —— 🔴 **不要往界面上暴露**。
 * 实测(2026-10-06):把 [17] 写 1 后 128 个采样点**全部等于 4087**(p-p = 0),
 * 写 0 才恢复正弦。也就是说这一位会让 ADC 输出**冻结**。
 * 板上丝印是 `OEB`,RTL 里的信号名却是 `OEB_N`,极性约定还没有从 RTL 追到引脚;
 * 在追清之前,它只该在排障脚本里出现,不该变成一个"一点就坏"的界面按钮。 */
#define CV_ADC_CFG_OEB_N     0x20000u
#define CV_ADC_CFG_PDN       0x40000u

/* 0x88 [16] CONT:1 = 连续采集,0 = 单次(等 PS 写 GO)。
 * 本项目用**单次** —— 采 8192 点置 done,PS 读完再发下一次,不抢 AXI 带宽。 */
#define CV_ADC_CFG_CONT      0x10000u

/**
 * @brief `0x18` 挡位独热码 → 该挡的**参考电阻 R0**(Ω)。
 *
 * R0 就是 `Z = R0·(VH−VL)/VL` 里的那个 R0,也是"挡位"这个概念的物理内容:
 * A/B/C/D 四挡 = 10 Ω / 100 Ω / 1 kΩ / 10 kΩ 四条换挡支路。
 * 前序 STM32 工程 `ad9238_capture.c` 的 `reference_resistance_ohm` 是同一个量。
 *
 * 🔴 **必须从 `0x18` 回读再换算,不要在任何地方硬编码。** 当前复位值是
 *    B 挡(100 Ω),但 `0x18` 是 RW —— 硬编码等于假设"挡位永远不会变",
 *    而挡位一变,Rg / Cs / 电流三个数会**同时**错,且不报错。
 *
 * @return 挡位电阻(Ω);**非独热码返回 0**(哨兵,调用方必须当错误处理,
 *         不要拿 0 去做除数 —— 那就是除零)
 */
static inline double cv_range_ohms(uint32_t range_sel)
{
    switch (range_sel & CV_RANGE_MASK) {
    case CV_RANGE_A: return 10.0;
    case CV_RANGE_B: return 100.0;
    case CV_RANGE_C: return 1000.0;
    case CV_RANGE_D: return 10000.0;
    default:         return 0.0;
    }
}

/**
 * @brief 由 `|Z|` 选**最合适**的参考电阻挡位(独热码)。自动换挡的判据只有这一份。
 *
 * 【判据:让 `|Z|` 落在 R0 的**最近十倍程**上】
 *   `Z = R0·(VH−VL)/VL`,误差只有两个来源,且方向相反:
 *     · `|Z| ≫ R0` ⇒ `VL` 小,`VL` 的**相对**误差被放大 `|Z|/R0` 倍
 *       (高压段实测:`VL/VH` 掉到 0.64 %,只有 23~36 码峰峰,贴 12 位量化底);
 *     · `|Z| ≪ R0` ⇒ `VL ≈ VH`,差值 `VH−VL` 小,它的相对误差被放大 `R0/|Z|` 倍。
 *   两者在 **`|Z| = R0`** 处相等 ⇒ 最优点就是 `|Z| ≈ R0`,
 *   此时 `VL/VH ≈ 0.707`,两种失效模式各占一半、谁也不主导。
 *   四挡是等比级数(10 / 100 / 1 k / 10 k)⇒ "最近"就是**在 log 轴上取最近的挡**。
 *
 * 【为什么必须有迟滞】
 *   边界恰在两个挡位**正中间**(各差 0.5 个十倍程)。`|Z|` 有噪声,
 *   逐点取最近会让边界附近的点来回跳挡,而**每跳一次,曲线就换一次标定基础**。
 *   故只有新挡位比当前挡位**明显更近**(≥ @p margin_decades)时才切。
 *
 * ⚠️ **本函数只回答"该用哪一挡",不写寄存器。** 下发走
 *    `RelayView_ApplyGear()`(全工程唯一的 `0x18` 写入方)。
 *
 * @param z_ohm          |Z|(Ω);≤ 0 视为无效
 * @param cur_onehot     当前挡位独热码(`0x18` 回读值);非独热则无"当前挡"可比
 * @param margin_decades 迟滞宽度(十倍程)。0.15 ≈ 1.41×,见上
 * @return 建议的独热码;**`z_ohm` 无效时返回 0**(哨兵,调用方必须当错误处理)
 */
static inline uint32_t cv_range_pick(double z_ohm, uint32_t cur_onehot,
                                     double margin_decades)
{
    static const uint32_t cand[4] = {CV_RANGE_A, CV_RANGE_B,
                                     CV_RANGE_C, CV_RANGE_D};
    double   log_z, best_d, cur_d;
    uint32_t best = 0u;
    int      i;

    if (!(z_ohm > 0.0)) {
        return 0u;
    }
    log_z  = log10(z_ohm);
    best_d = 1e9;
    for (i = 0; i < 4; ++i) {
        double d = fabs(log10(cv_range_ohms(cand[i])) - log_z);
        if (d < best_d) {
            best_d = d;
            best   = cand[i];
        }
    }

    cur_onehot &= CV_RANGE_MASK;
    if (cur_onehot == 0u) {
        return best;                    /* 当前挡未知:直接取最优 */
    }
    cur_d = fabs(log10(cv_range_ohms(cur_onehot)) - log_z);
    if (best_d + margin_decades >= cur_d) {
        return cur_onehot;              /* 优势不够明显 —— **不动**,防抖 */
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* AD9238 采样率与相位                                                 */
/* ------------------------------------------------------------------ */
#define CV_ADC_CLK_BASE_HZ   130000000u  /* MMCM:50×26/1 = 1300,CLKOUT0 = 130 MHz */
/* 🔴 **2026-10-15 由 2 改 4**(用户 2026-10-14 拍板)。130/4 = **32.5 MSPS**。
 *
 *   为什么:P=2 时整张采样刻度表只有 `{−1, 0}` 两点,**正好贴死 `adc_clk` 的
 *   两个边沿**,零建立裕量 ⇒ 杜邦线几 ns 的 tPD 离散直接变成**总线位翻转**
 *   (实测:同一档连采,`B/INB 峰峰` 在 400↔2500 之间跳)。P=4 的表是
 *   `{−2,−1,0,+1}`,`cv_adc_phase_center(4)` 折回后 = **+1** ⇒ A 落到 tick+1、
 *   B 落到 tick−1(RTL 按 B = A + P/2 自推),两路各离边沿
 *   **1 个 130 MHz 拍 = 7.69 ns**。
 *
 *   实测(同一回路 CRSS、R0 = 100 Ω、f_e = 1 MHz、挡位不动、各连采 30 次):
 *
 *     DIV/PH        f_s      Re Z ±SD         Im Z ±SD         Cs ±SD (pF)        D ±SD
 *     2:−1        65.0      231.44±1.02(0.44%) −75.33±2.91(3.86%) 2115.90±81.66(3.86%) 3.08±0.13(4.20%)
 *     4:+1        32.5      228.52±0.78(0.34%) −86.30±0.36(0.42%) 1844.19± 7.79(0.42%) 2.65±0.01(0.19%)
 *
 *   ⇒ Cs 的重复性 **81.66 → 7.79 pF,约 10 倍**。复现:
 *     `sudo -E python3 tools/adc_cfg_probe.py --cfg '2:-1' --only-z --z 30`
 *     `sudo -E python3 tools/adc_cfg_probe.py --cfg '4:+1' --only-z --z 30`
 *
 *   ⚠️ **代价(不是免费的)**:`f_s` 65.0 → 32.5 MSPS;`Cs` 整体偏 **−12.8%**
 *   (上表 2115.90 → 1844.19)⇒ **2026-10-14 之前那批 65 MSPS 的读数不能再与
 *   之后的直接比**。相干窗 (m,l) 由 `cv_adc_sample_hz()` 现算,三档激励在
 *   32.5 MSPS 下重算后仍全部成立(1 MHz → 252/8190;100 kHz → 25/8125;
 *   10 kHz → 2/6500),**不需要改别处**。
 *
 *   ⚠️ **这里与 RTL 复位值(2)【故意不一致】—— 不要照着"三处一致"改回去。**
 *      2026-10-08 那条"必须与 `cv_axi_lite_slave.v` 的 `adc_cfg <= 32'h0000_0002`
 *      和 `adc_capture.v` 的 `DIV_DEFAULT = 2` 三处一致"的约束,前提是
 *      **"P 只有 2 这一个可选值"**;该前提已随本次修改作废。
 *      `0x88` 由界面在**每次采集开头**自己写一遍(`measure_config.c::PushPhase`),
 *      RTL 的复位值 2 只管辖"界面第一次推之前"那一段 —— 那段本来也不出数。
 *   ⚠️ `CV_ADC_DIV_DEFAULT` 是**唯一**的软件默认值来源:`measure_config.c` 的
 *      `s_div` 初值、复位恢复、以及界面分度表高亮都从它取。改这里一处即可。 */
#define CV_ADC_DIV_DEFAULT   4u          /* 130/4 = 32.5 MSPS;PH_OFF 折回到 +1(窗正中) */

/**
 * @brief 分频比 → 端口 A 采样窗中心(PH_OFF,单位 = tick = 一个 130 MHz 拍)。
 *
 * = `1.5 − P/4`,四舍五入到整数 tick,再**折回寄存器合法范围**。**四点实测吻合**:
 *     DIV=13 → −2      DIV=26 → −5      DIV=130 → −31   (2026-10-06)
 *     DIV=2  → −1      ← **折回后才对**,不折回是 +1(2026-10-07 实测)
 *
 * 🔴 **换采样率必须同时换 PH_OFF。** 窗口位置随 P 走,只改 DIV 会让采样点滑出
 *    被驱动的窗口 —— 现象是"换了速率就没波形了",**不报错**。
 *    窗 = tick ∈ [2 − P/2, +1],宽 P/2(P=13 时就是 −4…+1 这 6 拍)。
 *
 * ⚠️ 整数运算,**与 `cv_regs.py::adc_phase_center()` 逐位等价**。
 *    别写成 `lround((6.0 - p) / 4.0)`:那在负半轴是"五 away from zero",
 *    与 Python 的 `round()`("五取偶")在 (6−P) ≡ 2 (mod 4) 上会差 1。
 */
static inline int32_t cv_adc_phase_center(uint32_t div)
{
    int32_t p    = (int32_t)((div < 2u) ? 2u : div);
    int32_t n    = 6 - p;
    int32_t t    = (n >= 0) ? ((n + 2) / 4) : -(((-n) + 2) / 4);
    int32_t half = p >> 1;
    int32_t hi   = p - 1 - half;            /* 寄存器合法上界 P−1−P/2 */

    /* 🔴 **P=2 这一档必须折回**(2026-10-07 上板实测补)。
     *
     *   P=2 时窗 = [2−1, +1] = [1, 1],而寄存器合法范围是 [−1, 0] —— **不相交**:
     *   1 这个刻度**永远命中不了**,现象是 GO 下去 `done` 永不置位
     *   (不是数据错,是根本采不到)。
     *   窗绕回一周即得 −1 ⇒ `t -= P`。**与 `cv_adc_phase_center_b()` 同一条折回**,
     *   那一侧本来就有。
     *
     *   实测依据(DIV=2,INB 接 DDS 1 MHz,全范围 `eye` 扫完只有两个可达刻度):
     *       PH_OFF=−1 → INA,p-p=5  平(死)
     *       PH_OFF= 0 → INB,p-p=42 干净正弦,250 点 3.85 个周期
     *   ⇒ A 在 −1、B 在 0；`_b()` 在 P=2 算出 0 本来就是对的,只有本函数是 1。
     *
     *   ⚠️ 折回**只改动 P=2 这一档**。其余 8 档(P=3/4/5/10/13/26/130,含非 2 的幂)
     *      上界都不越界,`t` 逐位不变 —— 已逐个核过。 */
    if (t > hi) {
        t -= p;
    }
    return t;
}

/**
 * @brief 分频比 → **port B 时间槽**的窗口中心 tick,已折回寄存器合法范围。
 *
 * 🔴 **为什么有 A 和 B 两个中心**(2026-10-06 实测,见 `report/llm-log/LLM-045.md` §G):
 *    那 12 位总线是**双端口时分复用**的 —— 一条总线在一个 `adc_clk` 周期里
 *    **轮流输出两路**(INA / INB),各占半个周期:
 *
 *    | 端口 | 窗口(单位 130 MHz tick) | 宽度 | 中心 |
 *    |---|---|---|---|
 *    | A(INA) | `tick ∈ [2 − P/2, +1]` | P/2 | `1.5 − P/4` |
 *    | B(INB) | `tick ∈ [+2, P/2 + 1]` | P/2 | `1.5 + P/4` |
 *
 *    ⇒ **两个中心各在一边**,`cv_adc_phase_center()` 只是 A 的。
 *    P=13 时 A = −2、B = **+5**(窗 `[+2,+7]`,实测那一段正是"有信号/静止"
 *    交替的两条带之一)。⛔ **不能拿 `PH_OFF = 0` 当"通用"值** ——
 *    它对 A 是窗内一点、对 B **已在窗外**。
 *
 * ⚠️ 与 `cv_adc_phase_center()` **同一条舍入规则**(半远离零,
 *    整数算术,避开 Python 银行家舍入);`cv_regs.py` 侧**尚无 port B 对应物**,
 *    哪天 Python 也要用,记得一起补 —— 这里先留个明处,别让它悄悄分叉。
 *
 * 🔴 **2026-10-14 起本函数降级为「参考资料」—— 不再是权威,只留作对照。**
 *    原因:`adc_capture.v` 现在**一次 GO 同时存 A/B**(B 的刻度由硬件按
 *    `B = A + P/2` 自推),所以"B 该采哪个 tick"这件事**已经不由软件决定**了。
 *    两者的差别只在 **P 为奇数**时:窗口宽 P/2 个 tick ⇒ 窗内**不止一个**
 *    合法刻度,取哪个都在 B 的数据窗里,只是采样相位差一个 tick。
 *      P=13 → 硬件取 +4,本函数给 +5(都在窗 [2,7] 内,都不算错)
 *      P=2  → 两者都是 0 ✓(**当前三档激励只用 P=2**,见 CLAUDE.md §八)
 *    ⛔ **不要拿本函数的返回值去"修正"硬件采到的相位** —— 那等于在已对齐的
 *       两路之间又插一个软件时延。它现在的唯一用途是**排障时对照**:
 *       "硬件那一版是不是还在窗里"。
 */
static inline int32_t cv_adc_phase_center_b(uint32_t div)
{
    int32_t p    = (int32_t)((div < 2u) ? 2u : div);
    int32_t m    = 6 + p;                   /* 1.5 + P/4 == (6 + P) / 4 */
    int32_t t    = (m + 2) / 4;             /* 半远离零舍入 */
    int32_t half = p >> 1;
    int32_t hi   = p - 1 - half;            /* 寄存器合法上界 */

    /* 窗可能绕回(小 P 时 `P/2+1` 会越过合法上界),折回可达范围 */
    if (t > hi) {
        t -= p;
    }
    return t;
}

/** @brief 分频比 → 采样率(Hz)。与 `adc_capture.v` 的 `P = max(div, 2)` 一致。 */
static inline uint32_t cv_adc_sample_hz(uint32_t div)
{
    uint32_t p = (div < 2u) ? 2u : div;

    return CV_ADC_CLK_BASE_HZ / p;
}

/** @brief DIV + 有符号 PH_OFF → 0x88 的 32 位字(其余位保持 0)。 */
static inline uint32_t cv_adc_cfg_word(uint32_t div, int32_t ph_off)
{
    return ((div & 0xFFu) | (uint32_t)(((uint32_t)ph_off & 0xFFu) << 8));
}

#define CV_REG_LCD_CTRL      0x50u   /* RW [1:0] src_sel                   */
#define CV_REG_LCD_FILL      0x54u   /* RW [23:0] RGB888 整屏填充          */
#define CV_REG_FB_BASE0      0x58u   /* RW 帧缓存 0 物理基址(64 KB 对齐)  */
#define CV_REG_FB_BASE1      0x5Cu   /* RW 帧缓存 1 物理基址               */
#define CV_REG_LCD_SWAP      0x60u   /* W  [0]目标索引 / R [0]front [1]pending */
#define CV_REG_FB_STATUS     0x64u   /* RO 场消隐/换址计数 + 粘滞错误标志  */
#define CV_REG_TOUCH_STATUS  0x68u   /* RO ★ 一次触摸事件的提交点,必须最先读 */
#define CV_REG_TOUCH_PT0     0x6Cu   /* RO 触点 0 快照                     */
#define CV_REG_TOUCH_PT1     0x70u   /* RO 触点 1 快照                     */

/* ------------------------------------------------------------------ */
/* 显示源选择 —— ★ 帧缓存是 2,不是 1(见 cv_regs.py 文件头"错 1")      */
/* ------------------------------------------------------------------ */
#define CV_LCD_SRC_BARS      0u
#define CV_LCD_SRC_FILL      1u
#define CV_LCD_SRC_DDR       2u

/* ------------------------------------------------------------------ */
/* 位域                                                                */
/* ------------------------------------------------------------------ */
/* 0x68: [2:0]count [3]pen_down [4]err(粘滞) [7:5]保留 [15:8]seq
 *       出自 gt911_ctrl.v:278 {16'b0, seq, 3'b0, err_sticky, pen_down, cur_count} */
#define CV_TOUCH_COUNT(r)    ((uint32_t)((r) & 0x7u))
#define CV_TOUCH_PEN(r)      ((uint32_t)(((r) >> 3) & 0x1u))
#define CV_TOUCH_ERR(r)      ((uint32_t)(((r) >> 4) & 0x1u))
#define CV_TOUCH_SEQ(r)      ((uint32_t)(((r) >> 8) & 0xFFu))
#define CV_TOUCH_MASK_KNOWN  0x0000FF1Fu   /* count|pen|err|seq 之外全是 0 */

/* 0x6C / 0x70: [11:0]x [27:16]y
 *       出自 gt911_ctrl.v:280 {4'b0, pt0_y, 4'b0, pt0_x}
 *    ⚠️ x/y 各只有 12 位 ⇒ 越界的形态是 1024~4095,不是"很大" */
#define CV_TOUCH_X(p)        ((uint32_t)((p) & 0x0FFFu))
#define CV_TOUCH_Y(p)        ((uint32_t)(((p) >> 16) & 0x0FFFu))

/* 0x60(读)/ 0x64: [0]front [1]pending 与粘滞标志 */
#define CV_SWAP_FRONT(s)     ((uint32_t)((s) & 0x1u))
#define CV_SWAP_PENDING(s)   ((uint32_t)(((s) >> 1) & 0x1u))
#define CV_FB_UNDERRUN(s)    ((uint32_t)(((s) >> 19) & 0x1u))   /* 写 0x60 清 */
#define CV_FB_AXIERR(s)      ((uint32_t)(((s) >> 20) & 0x1u))
#define CV_FB_VBLANK_CNT(s)  ((uint32_t)(((s) >> 2) & 0x1FFu))
#define CV_FB_SWAP_CNT(s)    ((uint32_t)(((s) >> 11) & 0xFFu))

/* ------------------------------------------------------------------ */
/* 模拟前端:偏压(DAC1220)/ 激励(AD9833)                              */
/* ------------------------------------------------------------------ */
/* 0x14 [19:0] → dac_1220.code。DAC 是**模块级 ±10 V**:
 *     V_module = −10 + 20 × ((code − 0x80000) mod 2^20) / 1048575
 * 再到被栅极看到的偏压,还要乘【外部高压级的 10:1】——
 *     V_bias = V_module × 10        ⇒ 量程 −100 V … +100 V
 *
 * 🔴 **那个 10:1 是硬件接线,不是寄存器。** 换一块高压板就得改这个常量,
 *    而 RTL / 寄存器映射一个字都不用动 —— 这正是换算放在 PS 侧的理由。
 *
 * 🔴🔴 **2026-10-12 实测:芯片跑在「偏移二进制补码」,不是直通二进制。**
 *     SBAS082G Table 13,CMR bit5 `DF`:0 = 偏移二进制补码(**上电默认**)、
 *     1 = 直通二进制;Table 9:20 位下 0 V 对应码 `80000h`(补码)/ `00000h`(直通)。
 *     RTL 写 `CMR = 0x20A1` 本意是 DF=1(直通),**但板上行为是补码
 *     ⇒ 这次 CMR 写没落进芯片**(附带:该值 bit7 `RES`=0 是 16 位分辨率,
 *     与 20 位 DIR 左对齐不符 —— 这两条都归 PL 侧待修,不阻塞软件)。
 *     **两种格式只差半个量程,故 PS 侧旋转 `0x80000` 即可修好,不必重新综合。**
 *     实测四点(模块级万用表):`0x7FFFF→+9.72`、`0x80000→−10.0`、
 *     `0x99999→−8.0`、`0xA6666→−7.0`,与上面这个补码式全部吻合。
 *
 * ⚠️ 因此 **`code = 0x00000` 才是 0 V**;`code = 0x80000` 实际是 **−100 V**
 *    (它是 RTL 复位值,对栅极是个安全的"全关"默认,但**别当 0 V 用**)。
 *    界面的偏压范围是 0.1–100.0 V(只走正向半轴),下限 0.1 V 对应 code = 0x0020C。 */
#define CV_BIAS_CODE_MAX     0xFFFFFu      /* 20 位满量程 = 1048575 */
#define CV_BIAS_CODE_MID     0x80000u      /* ⚠️ 只是 RTL 复位值;补码下它 = −100 V,不是 0 V */
#define CV_BIAS_HV_RATIO     10.0f         /* 外部高压级倍数(硬件,非寄存器) */
#define CV_BIAS_TENTHS_MIN   1u            /* 0.1 V —— 界面下限 */
#define CV_BIAS_TENTHS_MAX   1000u         /* 100.0 V —— 界面上限 */

/**
 * @brief 偏压 → 偏压码(**全精度**,不经 0.1 V 量化)。越界夹到 [0, 0xFFFFF]。
 *
 * ⚠️ 扫压必须走这个,不能走 @ref cv_bias_tenths_to_code。
 *    索引间隔在低压段每步增量会小于 0.1 V,先取整到 0.1 V 再换算,
 *    最前面一串点会**塌成同一个 0.1 V**(实测:60 点里前 6 点重复),
 *    而低压段恰恰是 C-V 斜率最大、最不该丢点的地方。
 *    0.1 V 量化只该用在**界面显示**上。
 */
static inline uint32_t cv_bias_volts_to_code(float volts)
{
    float module_v = volts / CV_BIAS_HV_RATIO;
    float raw      = (module_v + 10.0f) * 1048575.0f / 20.0f;

    if (raw < 0.0f)       { raw = 0.0f; }
    if (raw > 1048575.0f) { raw = 1048575.0f; }
    /* 直通二进制 → 偏移二进制补码 = 旋转半个量程(2026-10-12 实测补上)。
       删掉这个 ^ 就复现"设多少都读出 −10 V 偏置"的老毛病。 */
    return ((uint32_t)(raw + 0.5f)) ^ 0x80000u;
}

/**
 * @brief 0.1 V 计数 → 偏压码。界面的 0.1 V 栅格用这个。
 * @note  `tenths = 0` → `0x00000`(0 V);`1000` → `0x7FFFF`(+100 V)。
 */
static inline uint32_t cv_bias_tenths_to_code(uint32_t tenths)
{
    return cv_bias_volts_to_code((float)tenths / 10.0f);
}

/** @brief 偏压码 → 偏压(V),全精度。@ref cv_bias_volts_to_code 的逆。 */
static inline float cv_bias_code_to_volts(uint32_t code)
{
    /* 补码 → 直通二进制(转回来才能套那个 −10…+10 的线性式) */
    uint32_t straight = (code & CV_BIAS_CODE_MAX) ^ 0x80000u;
    float    module_v = -10.0f +
        (20.0f * (float)straight / 1048575.0f);

    return module_v * CV_BIAS_HV_RATIO;
}

/**
 * @brief 偏压码 → 0.1 V 计数(四舍五入)。把回读值变成人看的数字。
 * @note  负偏压返回 0 —— 界面的量程是正向半轴,负半轴不往外显示。
 */
static inline uint32_t cv_bias_code_to_tenths(uint32_t code)
{
    float tenths = cv_bias_code_to_volts(code) * 10.0f;

    return (tenths <= 0.0f) ? 0u : (uint32_t)(tenths + 0.5f);
}

/* 0x80 [27:0] → adc_9833.code:f_out = code × MCLK / 2^28
 * ⚠️ MCLK 是 AD9833 **板载焊死的 25 MHz 晶振**,FPGA 供不了(决策 D10)
 *    ⇒ 这里只能写常量。换晶体就要改这一行 + cv_regs.py 的 DDS_MCLK_HZ。 */
#define CV_DDS_MCLK_HZ       25000000u
#define CV_DDS_FREQ_1MHZ     0x0A3D70Au    /* 10737418 → 999 999.98 Hz */
/* 🔄 2026-10-06:界面上**有**频率控件了(「系统复位」页五档按钮),
 *    实现在 `app/measure_config.c`。原注释"界面上没有频率控件"已作废。
 *    默认值/复位值仍是 1 MHz(决策 D08),只是不再"固定"。 */
#define CV_DDS_HZ_MIN        1000u         /* 与 AD9833 无关的下限,防呆 */
#define CV_DDS_HZ_MAX        12500000u     /* AD9833 手册:输出上限 MCLK/2 */

/** @brief 频率 → 28 位频率字。 */
static inline uint32_t cv_dds_hz_to_code(uint32_t hz)
{
    return (uint32_t)(((double)hz * 268435456.0) / (double)CV_DDS_MCLK_HZ + 0.5);
}

/* ------------------------------------------------------------------ */
/* 面板                                                                */
/* ------------------------------------------------------------------ */
#define CV_PANEL_W           1024
#define CV_PANEL_H           600
#define CV_FB_FRAME_BYTES    (CV_PANEL_W * CV_PANEL_H * 2)   /* 1 228 800 */
#define CV_FB_ALIGNMENT      0x10000u                        /* 64 KB */

/** 面板装配方向:1 = 面板是翻过来装的,UI 整体旋转 180° 后再出图。
 *  ⚠️ **显示(`pynq_disp.c`)与触摸(`pynq_indev.c`)都读这一个宏。**
 *     只改一处会让"看到的位置"与"点到的位置"对不上 —— 这类错查起来极费劲。 */
#define CV_PANEL_ROTATE_180  1

/* ------------------------------------------------------------------ */
/* 接口                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 打开 /dev/mem 并映射 AXI 寄存器窗口。
 * @param axi_base AXI 从机物理基址(本项目 0x43C00000)
 * @return 0 成功;负数为错误码(-1 打不开 /dev/mem,-2 mmap 被拒)
 */
int cv_regs_init(uint32_t axi_base);

/** @brief 收掉寄存器映射。 */
void cv_regs_close(void);

/** @brief 读 32 位寄存器。@param off 字节偏移(CV_REG_*) */
uint32_t cv_reg_read(uint32_t off);

/** @brief 写 32 位寄存器。 */
void cv_reg_write(uint32_t off, uint32_t val);

/**
 * @brief 把一段**物理**内存映射进来(非缓存)。
 *
 * 用来映射 Python 分配好的 DDR 帧缓存。`phys` 必须是页对齐的
 * (帧缓存要求 64 KB 对齐,天然满足)。
 *
 * @return 映射到的指针;失败返回 NULL 并置 errno。
 */
void *cv_phys_map(uint32_t phys, uint32_t len);

/** @brief 解除映射。 */
void cv_phys_unmap(void *p, uint32_t len);

/**
 * @brief 请求换址并**等它生效**。
 *
 * 🔴 不等的话调用方会以为"画完了",而 PS 往往接着去写另一块 ——
 *    那一块此刻还在被扫描。等 `pending` 落下 = 扫描已经翻到新前台。
 *
 * @param target     目标缓冲索引(0/1)
 * @param timeout_ms 超时(一帧 17.07 ms,给 60 ms 足够)
 * @return 0 成功;负数为 -1 超时 / -2 参数错
 */
int cv_lcd_swap_wait(int target, int timeout_ms);

/** @brief 取单调时钟毫秒数(替代 STM32 的 HAL_GetTick)。 */
uint32_t cv_millis(void);

/** @brief 睡眠 @p ms 毫秒。 */
void cv_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* PYNQ_REGS_H */
