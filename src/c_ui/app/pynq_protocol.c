/**
 * @file pynq_protocol.c
 * @brief `display_protocol.h` 的**真实实现** —— 把界面指令变成 PL 寄存器写
 *
 * 取代 `app/_retired/pynq_protocol_stub.c`(那个是空实现,`Request*` 一律返回
 * `false`,屏上每个测量按钮都显示「指令发送失败」)。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【这一层是什么】
 *   `display_protocol.h` 是**前序的接口,一行没改**。前序它跑在 UART 上,
 *   是一套带 CRC 的帧协议:界面往外发指令,`Process()` 收回来更新界面。
 *   本项目的架构里它对应的是「UI ↔ 测量后端」那条边。
 *
 *   🔴 **本文件实现的是「方案 A」:C 侧直写 AXI 寄存器,不建 IPC。**
 *      理由(2026-10-05 定):
 *        · C 进程**本来就已经** mmap 了 AXI 窗口(`--axi-base`),
 *          `pynq_disp.c` 写 LCD 寄存器走的就是这条路。写偏压只是同一套。
 *        · 走 socket 的对端(`ui_process.py`)现在**没有测量数据可发** ——
 *          DLIA / CORDIC 在 PL 里都还没有(继电器/档位寄存器 2026-10-10
 *          已上板,但界面只**读回显**,不产生"测量数据")。建 IPC 只增加两进程
 *          故障面,不增加能力。
 *        · 文档里 pipe/socket 的定位是「接**测量数据**」,那件事属于 P3–P4。
 *          到那时换掉的是**本文件**,`ui_demo.c` 仍然一行不动。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 【已经真的动硬件的】
 *   · 「偏压设置」/ 点测  → 写 0x14 BIAS_CODE → dac_1220 → 高压级 → 栅极
 *   · 「开始扫压」       → `Process()` 里逐点写 0x14,指数间隔,带驻留时间
 *   · 「停止扫压」       → 停状态机(偏压停在当前点,不回零)
 *   · 「发送复位指令」    → 停扫压 + 偏压归 0 V + **所有测量参数回默认** + 清界面
 *                          (采样率与相位也复 —— 见 RequestReset 的函数头;
 *                           ⛔ **不复 0x84**,DAC 放行权归 Python 侧)
 *   · 上电               → 偏压 0 V(写 0x14);激励频率 / 采样率 / 相位
 *                          由 `MeasureConfig_Init()` 一次下发
 *
 * 🆕 2026-10-06:激励频率(0x80)与采样率+相位(0x88)**不再由本文件写** ——
 *    归 `measure_config.c` 单写。「系统复位」页新增了五档激励频率、
 *    五档采样率、三档相位微调;前序其余四页逐字未动。
 *
 * 【继电器 / 档位(2026-10-10 起有硬件)】
 *   · `0x18 RANGE_SEL` / `0x1C RELAY_CFG` / `0x04 STATUS` 已在位流里,
 *     上板验证通过(见 `docs/project_state.md` 2026-10-10 那一条)。
 *     ⛔ 旧写法「本位流里没有 `relay_ctrl`,`cv_regs.py` 里也没有继电器
 *        寄存器」**作废** —— 那是本次接线之前的实况。
 *   · 🟢 **2026-10-14 起 C 界面是 `0x1C` 的唯一写入方**(用户 2026-10-14 拍板)。
 *     原文是「C 界面本轮只读,写入权归 Python 侧,避免第二个写入方」——
 *     **那条约定没有兑现**:Python 侧根本没有调用方(`main.py` 只 import
 *     `drivers.board`),于是硬件永远停在复位值 `ALL_OFF`。根因与证据表见
 *     `app/relay_view.c` 文件头。
 *   · 现在点「回路」按钮**真的切继电器**(`RelayView_ApplyPath()`);
 *     顶栏那两栏仍然只显示**回读的已生效值**,不显示"用户点了什么"。
 *   · 🟢 **2026-10-15 起 `0x18`(挡位)也由本进程写** —— 测量页新增
 *     A/B/C/D 四颗按钮,走 `DisplayProtocol_RequestGear()` →
 *     `RelayView_ApplyGear()`。原文是「`0x18` 本轮仍只读,挡位不在本次
 *     范围内」,**那句只在 2026-10-14 那一轮的范围内成立**;用户随后
 *     接上了挡位矩阵并要求控制它。
 *     ⇒ 至此 `0x1C` 与 `0x18` **两个寄存器都是本进程独占写**,
 *        `drivers/relay.py` 仍然是没有任何调用方的取证材料。
 * 🆕 2026-10-06:**Cs 读数有了,但不是 DLIA 算的。** 走的是
 *    `adc_sample.c` —— 采 AD9238 缓冲(0x8C/0x90/0x94/0x98)+ C 侧正交解调。
 *    结果页的「单点波形 / 扫压波形」两张图和扫压曲线上的 Cs 都由此来。
 *    ⚠️ **DLIA(`dlia_core` / `cordic_polar`)仍然没做** —— 这是 P3 之前的
 *    替代路径,不是终点。差别在于:软件解调在 PS 上跑,受 `cv_millis()`
 *    调度和 AXI 事务开销限制;**PL 侧 DLIA 才是评分对象**(见 CLAUDE.md §二)。
 *
 * 🆕 **2026-10-14:一次 GO 同时收 INA/INB 两路,相位通了。**
 *    `adc_capture.v` 改成 A 窗、B 窗各写一次(偶地址 A、奇地址 B),
 *    两路来自**同一个 ADC 转换周期** ⇒ 相位相关 ⇒ 能算复数阻抗。
 *
 * ⚠️ **但读数仍然只是"能出数",不是"测得准"**:
 *    · 两路 A/B 通路的**增益与相位失配未标定** —— `Z = R0·(VH−VL)/VL` 的
 *      正确性依赖两路一致,那是 OPEN/SHORT 校准(B8)的活,本轮没做;
 *    · `(A − B)` 相减前**没有相位微调**,两路各按自己的 `PH_OFF` 取点时,
 *      残余的时基偏移会**直接进 Z 的相位**;
 *    · 模拟输入现在**没接**,采到的可能是贴顶的平线 —— 那是**真数据**,
 *      不是画错了。判据:`dump_stats` 的 `ADC 采集 成功/失败` 计数。
 *
 * 🔴 **状态行有字数上限,写长了会被截掉右半边。**
 *    `g_header_operation_label` 在 x=696、宽 `LCD_PIXEL_WIDTH − 706`
 *    = **318 px**、**右对齐**、`ui_font_cn_20`。
 *    🆕 2026-10-10 顶栏由 7 栏加到 8 栏(插了「档位:」),右边六栏的宽度
 *    一起重算过 —— 完整表在 `ui_demo.c::UI_CreateCommonStatusHeader()`。
 *    20 px 字体 ⇒ 一个汉字 20 px、一个半角字符约 10 px。
 *    ⇒ 大约 **15 个汉字**,或"8 汉字 + 10 半角"。**超了从左边开始被吃掉**
 *    (右对齐),用户看到的正好是「状态:…」被啃掉的那种别扭样子。
 *    ⛔ 别再往状态行里塞解释 —— 解释写 `printf`,日志里看得见。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 【为什么写 0x14 就等于"下发到 DAC"】
 *   `cv_axi_lite_slave.v:454` —— 每次对 0x14 的写都会顺带发**一拍**
 *   `cfg_bias_set` 脉冲给 `dac_1220.set_in`。不需要额外的"执行"寄存器。
 *   0x80 对 `adc_9833` 同理(`:455`)。
 *
 *   ⚠️ 但 `dac_1220` 只在 `NORMAL_WAIT` 状态里锁存 `set_in` 的上升沿。
 *      它上电要 200 ms + 自校准才进 `NORMAL_WAIT`,所以上位机**开机第一次**
 *      写偏压如果太早,那一拍会被丢掉。从机里已经补了「等 `dac_init_done`
 *      再发一次上电脉冲」的兜底;`DisplayProtocol_Init()` 里也重写一次,
 *      两次都落在 `init_done` 之后,不必在上位机侧再等。
 *      证据:`cv_axi_lite_slave.v` §set 脉冲。
 *
 * 【回读核对】
 *   每次写偏压后立刻读回 0x14 比对。不一致 ⇒ AXI 通路有问题(地址错、
 *   位流不对、DS 关掉了),这里会打一行 `[proto] ⚠️` 并计数,退出时进
 *   `dump_stats`。**这是判"寄存器写没写进去"最快的证据。**
 */

#include "display_protocol.h"
#include "ui_demo.h"
#include "pynq_regs.h"
/* 🆕 2026-10-06:激励频率 / 采样率 / 相位 归 `measure_config.c` 单写。
 *    本文件原先自己写 0x80,现在改成转发 —— 见下面 `dds_set_hz()` 的说明。 */
#include "measure_config.h"
/* 🆕 2026-10-06:结果页波形的真数据源 —— 读 AD9238 缓冲 + 正交解调。
 *    此前 `UI_Demo_*Waveform*` 没有任何活调用者,扫压的 Cs 是 `demo_capacitance()`
 *    编的模型。现在默认走真实采集,模型只剩 `CV_UI_DEMO=1` 一条退路。 */
#include "adc_sample.h"
/* 🆕 2026-10-10:继电器 / 档位的回显。🔄 2026-10-14:本文件**会写** `0x1C`
 *    (经 `RelayView_ApplyPath()` / `RelayView_ApplyCode()`),不再是只读。
 *    顶栏仍只显示回读真值。见 `relay_view.c` 文件头。 */
#include "relay_view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         /* access() —— 扫压自动换挡的开关文件,见 SWEEP_AUTO_GEAR_FLAG */

/* ==========================================================================
 * 参数
 * ========================================================================== */

/* 扫压点数。扫压页 X 轴是 log10(V),范围 −1…+2(即 0.1 V … 100 V,见
 * ui_demo.c:38-39),所以**指数间隔**才铺得满整条轴。
 * 60 点 = 3 个数量级上每十倍 20 点,对 C-V 曲线的形状足够;
 * 上限是 ui_demo.h 的 201,留了余量。 */
#define SWEEP_MAX_POINTS       60u

/* 扫压起点。界面只给**终止电压**(`g_ui_sweep_end_tenths`),没有起点控件,
 * 所以起点定死在上限轴左端 0.1 V —— 和界面对偏压的允许下限是同一个值。 */
#define SWEEP_START_VOLTS      0.1f

/* 每点驻留。真扫压要等高压级和被测器件稳定,不能像演示那样一拍一个点。
 * 60 点 × 300 ms ≈ 18 s 一条曲线。可由 `CV_SWEEP_DWELL_MS` 覆盖。 */
#define SWEEP_DWELL_MS_DEF     300u
#define SWEEP_DWELL_MS_MIN     1u
#define SWEEP_DWELL_MS_MAX     10000u

/* 🆕 2026-10-15 扫压自动换挡的迟滞宽度(十倍程),交给 `cv_range_pick()`。
 * 为什么必须有迟滞、0.15 这个数怎么来的 —— 见 `pynq_regs.h::cv_range_pick()`。
 * 一句话:挡位边界恰在相邻两挡正中间,不给迟滞就会在边界附近来回跳。 */
#define SWEEP_GEAR_MARGIN      0.15

/* 🔴 **2026-10-15 起扫压自动换挡默认【关闭】—— 挡位固定,由用户手动选。**
 *
 * 为什么关:接线整理后的一次上板,打开自动换挡跑出来是这样 ——
 *   COSS 腿把挡位从 B 顶到 C,CRSS 腿**继承 C** 再顶到 D;而 **CRSS 恰恰是当时
 *   唯一一条随偏压变化的曲线**,抬到 D 之后出来的是 |Z| = 140 kΩ、Cs 为负、
 *   D = 71 的垃圾(见 `report/measurements/20261015_uilog_*.txt`)。
 *   换挡判据本身没错 —— **错的是它脚下的 |Z| 本身就不可信**(参考电阻是否
 *   真是挡位标称值,尚未用已知件标定过)。在一个坏数上乘一次错比例,只会放大。
 *   ⇒ 在测量链标定完之前,先把挡位钉死,少一个自变量。
 *
 * 开关用**文件**不用环境变量:环境变量要重启服务才生效,而 `qrs-ui.service`
 *   重启 = 第二次加载位流(**红线**)。这个文件在 `sweep_start()` 时读,随时可改:
 *       sudo touch /home/xilinx/sweep_auto_gear     # 打开自动换挡
 *       sudo rm    /home/xilinx/sweep_auto_gear     # 关闭(默认)
 * ⚠️ 两种模式在 `sweep_start()` 的日志里都会明说,事后对日志不会有歧义。 */
#define SWEEP_AUTO_GEAR_FLAG    "/home/xilinx/sweep_auto_gear"

/* 激励频率与采样率**不在这里定义** —— 归 `measure_config.h` / `pynq_regs.h`。
 * 本文件只负责"什么时候下发",不负责"下发什么值"。 */

/* 扫压页要用的点数与驻留见上;激励频率的默认值 = `CV_DDS_HZ_DEFAULT`(1 MHz,决策 D08)。 */

/* ==========================================================================
 * 状态
 * ========================================================================== */

static int      s_demo_enabled;         /* CV_UI_DEMO=1:叠加模型读数 */
static uint32_t s_dwell_ms;

static uint32_t s_sweep_active;
static uint32_t s_sweep_mask;           /* bit0..2 = CISS/COSS/CRSS,【要跑哪几条腿】 */
static uint32_t s_sweep_leg;            /* 🆕 【当前正在跑的那条腿】(0..2);mask 里可能有多条 */
static float    s_sweep_stop_volts;
static uint32_t s_sweep_total;
static uint32_t s_sweep_index;          /* 已下发的点数(1 起) */
static uint32_t s_sweep_next_ms;
static float    s_bias_volts;           /* 最后一次真正下发的偏压(量化后的真值) */

/* 🆕 2026-10-15:自动换挡**这一轮已被关掉**(换失败了,或非独热码)。
 * ⚠️ 必须是粘滞的:判据是确定性的,失败后每一点都会算同一个"该换"的结论,
 *    不粘滞就会**每 300 ms 重试一次并弹一次框**,把扫压搅成一片报错。 */
static uint32_t s_auto_gear_off;

/* 🔴 扫压自动换挡的**总开关**(与上面那个粘滞位不同:这个是用户意图,那是失败状态)。
 *    默认关 —— 理由与开关文件路径见 `SWEEP_AUTO_GEAR_FLAG` 的注释。
 *    ⚠️ 每次 `sweep_start()` 现读,不缓存:这样"改文件 → 下一场扫压"就生效,
 *       不必重启服务(重启 = 第二次加载位流,红线)。 */
static int sweep_auto_gear_enabled(void)
{
    return access(SWEEP_AUTO_GEAR_FLAG, F_OK) == 0;
}

static uint32_t s_demo_auto_page;       /* CV_UI_DEMO_PAGE:跑完切到这一页 */
static uint32_t s_demo_auto_stage;

/* 统计(排障用;不在 display_protocol.h 里 —— 那是前序的接口) */
static uint32_t s_cmd_total;            /* 收到的指令总数             */
static uint32_t s_readback_pending;     /* 要读数、但读不到的指令数   */
static uint32_t s_path_cmds;            /* 回路按钮点击次数(只读)  */
static uint32_t s_bias_writes;          /* 写 0x14 的次数             */
static uint32_t s_bias_verify_fail;     /* 回读与写入不一致的次数     */
static int      s_verify_notice;        /* 回读失败提示只打一次       */
static int      s_dac_rst_notice;       /* "DAC 还没放行"提示只打一次 */

/* ==========================================================================
 * ADC 采集(结果页波形的数据源)
 *
 * 【两路怎么来的】
 *   `adc_capture.v`:`adc_clk = (cnt < P/2)`,刻度 `tick = cnt − P/2`。
 *   ⇒ **两路不是一个"眼"的两端,是两个时间槽**:port A = INA 窗 `[2−P/2,+1]`
 *      (中心 `1.5−P/4`)、port B = INB 窗 `[+2,P/2+1]`(中心 `1.5+P/4`)。
 *      ⚠️ 用户口径的「上升沿=通道 a / 下降沿=通道 b」**正好是反的**,见 `adc_capture_pair()`。
 *
 * 🟢 **2026-10-14 起:一次 GO 同时收下 A 和 B**(RTL 已改,见 `adc_capture.v` ④):
 *    同一个 ADC 转换沿产生的那一对字,现在各写各的缓冲 —— 偶地址给 A、奇地址给 B,
 *    **同一次 GO、同一个转换周期** ⇒ **两路相位相关**,可以算阻抗。
 *
 * 🔴 **原稿这一段是反的,保留一句以免旧结论被当成现状**:原文写
 *    「一次 GO 只锁一个刻度 ⇒ 两路靠两次采集」「两路的相位不相关,只能比幅值」,
 *    那是 2026-10-14 之前的形态。相位原点不同 ⇒ Cp / D 只能填 `--`。
 *    **本轮把这条路打通了**,现在 `Cs / Cp / D / Rg` 都从同一个复数 Z 出。
 * ========================================================================== */

/** 单点测量:bias_apply 之后等多久再采。复用扫压的驻留时间 —— 那是本项目
 *  对"高压级 + 被测件稳定下来要多久"的唯一口径,不另立一个数。 */
#define POINT_SETTLE_MS_DEF   300u

typedef struct
{
    float      mag_a_v;     /* 通道 A(INA = VH)解调幅值,伏 —— 未标定 */
    float      mag_b_v;     /* 通道 B(INB = VL)解调幅值,伏 —— 未标定 */
    AdcSampleZ z;           /* 🆕 复数阻抗及其派生量(Cs / Cp / D / 电流)   */
    uint32_t   gear;        /* 🆕 本次读到的 0x18 原始码(挡位是谁说了算)   */
    double     r0_ohm;      /* 🆕 本次用的参考电阻 = 挡位电阻(回读换算)     */
    float      pp_a;        /* 通道 A 峰峰值,ADC 码(排障:输入有没有信号)  */
    float      pp_b;        /* 通道 B 峰峰值,ADC 码 */
    float      mag_a_codes; /* 🆕 通道 A 解调幅值,**码**(自洽判据要的就是它)   */
    float      mag_b_codes; /* 🆕 通道 B 解调幅值,**码**                        */
    uint32_t   otr;         /* 两路 OTR 置起的字数合计 */
    uint32_t   z_valid;     /* 🆕 1 = `z` 是本次算出来的;0 = 保留上一次的值 */
    uint32_t   tries;       /* 🆕 本点连采了几次才自洽(1 = 一次就对)            */
} AdcPair;

static uint32_t s_code_a[ADC_SAMPLE_POINTS];
static uint32_t s_code_b[ADC_SAMPLE_POINTS];
static AdcPair  s_pair;

/* 🆕 自洽判据的账本(2026-10-15)。`s_adc_points` 数的是"实际采了几次",
 * 这两个数的是"其中被拒了几次 / 有几个点靠重采才救回来"。
 * 出问题时看这三个数的比值就知道毛刺有多勤 —— 比翻日志快。 */
static uint32_t s_adc_rej;          /* 被自洽判据拒掉的采集次数 */
static uint32_t s_adc_rescued;      /* 靠重采救回来的点数(第一次就被拒的那些) */
static uint32_t s_adc_void;         /* 连采满仍不自洽、判无读数的点数 */
/* 一次测量最多采几次。默认 `ADC_SAMPLE_TRIES_MAX`,可用 `CV_ADC_TRIES` 覆盖
 * (设 1 = 关掉重采,退回旧行为 —— 用来 A/B 对照验证本判据确实在起作用)。 */
static uint32_t s_adc_tries_max = ADC_SAMPLE_TRIES_MAX;

/** 本次采集**是在哪条腿上**做的(0/1/2/3 = CISS/COSS/CRSS/RG)。
 *  只用于日志 —— 出问题时第一句话是"这是在哪一路采的"。
 *  由 `sweep_start()` / 单点排程在切完继电器之后设置。 */
static uint32_t s_pair_path;

static uint32_t s_adc_points;       /* 一轮总共采了多少次(统计) */
static uint32_t s_adc_fail;         /* 采集/解调失败次数       */
static int      s_adc_notice;       /* 失败提示只打一次        */

/* 单点测量:请求 → 等偏压稳定 → 采 → 报结果 */
static uint32_t s_point_pending;
static uint32_t s_point_due_ms;
static uint32_t s_point_path;
static uint32_t s_point_volts_x10;
static uint32_t s_point_all;        /* 1 = 「CISS+COSS+CRSS」→ **三腿序列** */
static uint32_t s_point_leg;        /* 🆕 当前腿(0..3);`all=1` 时 0→1→2 依次走 */
static uint32_t s_point_leg_n;      /* 🆕 总腿数:1(单路)或 3(「全部」) */

/* ==========================================================================
 * 小工具
 * ========================================================================== */

static const char *path_name(uint32_t path)
{
    static const char *const names[] = {"CISS", "COSS", "CRSS", "RG"};

    return (path < 4u) ? names[path] : "?";
}

/**
 * @brief 挡位编号 → 名字(2026-10-15 加)。
 *
 * ⚠️ 与 `relay_view.c` 里那个 `range_name()` **是两份** —— 那个是 `static`,
 *    且它的输入是**独热码**(`0x1/0x2/0x4/0x8`),本函数的输入是**下标**(0..3)。
 *    ⛔ 别把两者接错:传独热码进来会得到 `CV_RANGE_B(2) → "C"`,错一位且不报。
 *    名字带 R0 而不只是挡号 —— 用户看到的"A/B/C/D"本身不带信息量,
 *    R0 才是它在算式里的含义(`Z = R0·(VH−VL)/VL`)。
 */
static const char *gear_name(uint32_t index)
{
    static const char *const names[] = {"A(10Ω)", "B(100Ω)", "C(1kΩ)", "D(10kΩ)"};

    return (index < 4u) ? names[index] : "?";
}

/**
 * @brief 写偏压码,并**回读核对**。
 *
 * 这一次写会顺带触发 `cfg_bias_set` 一拍脉冲(见文件头),所以返回 0
 * 就意味着"DAC 已经收到新码",不需要额外的启动动作。
 *
 * @param code 20 位偏压码(0x80000 = 0 V)
 * @return 0 回读一致;1 回读不一致(AXI 通路可疑)
 */
static uint32_t bias_write_code(uint32_t code)
{
    uint32_t back;

    cv_reg_write(CV_REG_BIAS_CODE, code);
    s_bias_writes++;
    s_bias_volts = cv_bias_code_to_volts(code);

    back = cv_reg_read(CV_REG_BIAS_CODE);
    if (back != code) {
        s_bias_verify_fail++;
        if (!s_verify_notice) {
            s_verify_notice = 1;
            fprintf(stderr,
                    "[proto] ⚠️ 偏压回读不一致:写 0x%05X,读回 0x%05X\n"
                    "        ⇒ AXI 通路有问题。依次查:\n"
                    "          ① --axi-base 是不是 0x43C00000;\n"
                    "          ② 位流是不是含 DAC/DDS 那一版(board/qrs.bit);\n"
                    "          ③ 读 0x64(显示状态)有没有值 —— 全 0 说明压根没通。\n",
                    (unsigned)code, (unsigned)back);
        }
        return 1u;
    }

    /* 回读一致 ≠ DAC 真的收到了 —— 还得看初始化放行了没有。
     * ⚠️ **只读不写**:放行权归 Python 侧独占(见 pynq_regs.h 的 CV_CTRL 段),
     *    这里只负责把"还没放行"喊出来。不喊的话,现象就是
     *    "界面上偏压从 0.1 V 调到 5.0 V、寄存器也回读正常,**而 DAC 输出不动**"
     *    —— 2026-10-05 就是这个现象,查了很久。 */
    if ((cv_reg_read(CV_REG_CV_CTRL) & CV_CTRL_DAC_RST_N) == 0u) {
        if (!s_dac_rst_notice) {
            s_dac_rst_notice = 1;
            fprintf(stderr,
                    "[proto] 🔴 0x84[0] = 0 ⇒ **dac_1220 仍被按在复位态**,"
                    "初始化没跑。\n"
                    "        偏压码会被硬件收下(回读正常),但**不会到达 DAC 输出**,\n"
                    "        输出会一直停在芯片默认的 −10 V。\n"
                    "        ⇒ 放行必须在 FCLK0 补到 50 MHz **之后**做(该窗口\n"
                    "           [46.1, 54.0] MHz 很窄,开机默认 27.78 在外),\n"
                    "           正常路径是跑 main.py —— 它在本步之前已调过\n"
                    "           drivers/board.release_dac()。\n");
        }
    }

    return 0u;
}

/** @brief 把偏压写进表头的「偏压:」一栏(界面初始化值是 0.1V,这里给真值)。 */
static void bias_announce(float volts)
{
    char text[16];

    /* 两位小数:扫压低压段的相邻点只差几毫伏,一位小数会显示成同一串 */
    (void)snprintf(text, sizeof(text), "%.2fV", (double)volts);
    UI_Demo_SetActualBias(text);
}

/**
 * @brief 下发一个偏压:**全精度换算 → 写 → 回读核对 → 报回界面**。
 *
 * ⚠️ 显示的是**写完之后回算的值**(`s_bias_volts`),不是请求值 ——
 *    这样界面上看到的永远是 DAC 真正拿到的那个电压。
 *
 * @return 0 回读一致;1 回读不一致。原先返回 void,2026-10-06 改成返回状态:
 *         「所有参数恢复默认」的成败要如实上报,偏压也是"所有参数"之一。
 */
static uint32_t bias_apply(float volts)
{
    uint32_t err = bias_write_code(cv_bias_volts_to_code(volts));

    bias_announce(s_bias_volts);
    return err;
}

/* ⛔ 这里原先有个 `dds_set_hz()` 自己写 `0x80`(当时的注释是「界面上没有频率控件」)。
 *    🔄 2026-10-06 用户明确要求把频率接口引出来 ⇒「系统复位」页加了
 *    五档激励频率 + 五档采样率按钮,前序其余四页仍逐字未动。
 *    **写入方必须唯一**:`0x80`/`0x88` 现在只由 `measure_config.c` 写,
 *    本文件不再直接碰这两个寄存器(两个写入方 = "谁最后写谁赢"的经典 bug)。
 *    ⇒ 这个空壳包装没有调用点了,已删除;要设激励频率请调
 *      `MeasureConfig_SetDdsHz()`。 */

/* ==========================================================================
 * ADC 采集 —— 结果页波形的真数据源
 * ========================================================================== */

/** @brief 采集/解调失败的提示(只打一次,免得扫压时刷 60 行)。 */
static void adc_report_failure(const char *where)
{
    s_adc_fail++;
    if (s_adc_notice) {
        return;
    }
    s_adc_notice = 1;
    fprintf(stderr,
            "[adc] 🔴 %s:采集或解调失败 —— %s\n"
            "      ⚠️ 此后不再重复打印这一条。\n"
            "      ⇒ 依次查:\n"
            "         ① 位流是不是含 0x88–0x98 的那一版;\n"
            "         ② 0x90[0] mmcm_locked 是不是 1(没锁就是 MMCM 没起来);\n"
            "         ③ 采样率档位是不是太低 —— 每周期点数 < %u 时本层**拒绝解调**\n"
            "            (N=2 正好在 Nyquist 上、N=1 混叠到 DC,算出来是噪声);\n"
            "         ④ 冒烟看 [adc] 那行的「峰峰」—— 个位数说明输入没信号。\n",
            where, AdcSample_LastError(), (unsigned)ADC_SAMPLE_DEMOD_MIN_N);
}

/** @brief 采**一对**(A = INA = VH / B = INB = VL)并解调。结果进 `s_pair` / `s_code_*`。
 *
 *  🔴 **一次 GO 收齐两路(2026-10-14 改)。** 原文是"先 PushPhase(A 窗中心)采 INA,
 *     再 PushPhase(B 窗中心)采 INB"—— 两次 GO 之间隔着一万多个转换周期,而解调
 *     基准锚在**各次采集的第 0 点**,⇒ 两路相位差**随机**,只能比幅值。
 *     现在 `adc_capture.v` 在 A 窗、B 窗各命中一次(RTL 按 B = A + P/2 自推),
 *     两路来自**同一个转换周期** ⇒ 同一个相位参考系,复数除法成立。
 *     **软件侧一个时间改正都不加** —— 硬件上本来就没有时间差。
 *
 *  ⚠️ `tick_a` 只是"软件以为 A 在哪";B 的刻度是**硬件**算的,这里用
 *     `AdcSample_PortBTickHw()` 把它复算一遍**只为日志**(排障时判断
 *     "硬件那一版是不是还在数据窗里")。⛔ 不要拿它去修正相位。
 *
 * 🔴 **2026-10-15 起本函数只负责"采一次"** —— 对外的入口是下面的
 *    `adc_capture_pair()`,它按自洽判据重采。**不要直接调本函数。** */
static bool adc_capture_pair_once(void)
{
    uint32_t div    = MeasureConfig_GetSampleDiv();
    uint32_t fs_hz  = cv_adc_sample_hz(div);
    uint32_t fe_hz  = MeasureConfig_GetDdsHz();
    /* 🔴 **按"时间槽窗口中心"取刻度,不是按上升沿/下降沿。** 那 12 位总线是
     *    双端口时分复用:port A = INA 窗 `[2−P/2,+1]`(中心 `1.5−P/4`)、
     *    port B = INB 窗 `[+2,P/2+1]`(中心 `1.5+P/4`)。用户口径说的
     *    「上升沿=通道 a、下降沿=通道 b」**正好是反的**(tick −P/2 落在 B 窗、
     *    tick 0 落在 A 窗),而且都贴窗沿。见 `adc_sample.h` 文件头与 `LLM-045` §G。
     *
     * 🆕 **两个刻度现在都在 `AdcSample_CapturePair()` 内部算**(A 由
     *    `AdcSample_PortATick()`、B 由 RTL 同款折叠规则),这里不再自己算一份 ——
     *    算两份等于留一个"哪天改了忘记同步"的口子。日志里要打的那份由
     *    `adc_log_pair()` 现算,同样是查表式的纯函数,不会漂。 */
    float    ia, qa, ib, qb;
    float    mag_a_codes, mag_b_codes;
    uint32_t otr_n = 0u;
    uint32_t i;
    uint32_t min_a, max_a, min_b, max_b;
    uint32_t path = s_pair_path;                     /* 本次测的是哪条腿 */

    /* 🔴 **参考电阻 R0 每次现读 0x18**,不硬编码、不缓存 ——
     *    挡位一变,Rg / Cs / Cp / 电流会**同时**错。见 `cv_range_ohms()`。
     *    🆕 2026-10-15 起挡位**真的会变**了(测量页 A/B/C/D 四颗按钮 →
     *    `DisplayProtocol_RequestGear()`),所以"必须现读"从"纪律"变成了
     *    "事实":写死一个常数现在会**当场**出错数,不再是"将来某天"。
     *    ⚠️ 读到的可能是**上一次写失败**遗留的旧挡 —— 判据在同一函数下面的
     *    `[adc]` 日志行:它把 `R0` 与挡位码一并打出来,事后对得上。 */
    s_pair.gear   = cv_reg_read(CV_REG_RANGE_SEL) & CV_RANGE_MASK;
    s_pair.r0_ohm = cv_range_ohms(s_pair.gear);

    /* ① 采一对。刻度合法性检查在 `AdcSample_CapturePair()` 内部(它才知道 DIV)。 */
    if (!AdcSample_CapturePair(s_code_a, s_code_b, &otr_n)) {
        return false;
    }

    /* ② 一路一路解调 —— 参考表是**同一条**(同一个 (m,l)),所以两路天然同相。 */
    if (!AdcSample_Demodulate(s_code_a, ADC_SAMPLE_POINTS, fs_hz, fe_hz,
                              &ia, &qa, &mag_a_codes)) {
        return false;
    }
    if (!AdcSample_Demodulate(s_code_b, ADC_SAMPLE_POINTS, fs_hz, fe_hz,
                              &ib, &qb, &mag_b_codes)) {
        return false;
    }

    /* 峰峰值只进日志 —— 它是"输入到底有没有信号"最快的判据 */
    min_a = max_a = s_code_a[0];
    min_b = max_b = s_code_b[0];
    for (i = 1u; i < ADC_SAMPLE_POINTS; ++i) {
        if (s_code_a[i] < min_a) { min_a = s_code_a[i]; }
        if (s_code_a[i] > max_a) { max_a = s_code_a[i]; }
        if (s_code_b[i] < min_b) { min_b = s_code_b[i]; }
        if (s_code_b[i] > max_b) { max_b = s_code_b[i]; }
    }

    s_pair.mag_a_codes = mag_a_codes;       /* 🆕 码,不是伏 —— 自洽判据用 */
    s_pair.mag_b_codes = mag_b_codes;
    s_pair.mag_a_v = AdcSample_CodesToVolts(mag_a_codes);
    s_pair.mag_b_v = AdcSample_CodesToVolts(mag_b_codes);
    s_pair.pp_a    = (float)(max_a - min_a);
    s_pair.pp_b    = (float)(max_b - min_b);
    s_pair.otr     = otr_n;
    s_pair.tries   = 1u;                    /* 由 `adc_capture_pair()` 累加 */

    /* ③ 复数阻抗 —— 这是本次改动的**目的**:Cs 不再靠"纯电容"近似,
     *    Cp / D 也从"算不出来"变成算得出来。
     *    ⚠️ 算不出来时**保留上一次的值**并把 `z_valid` 置 0,不写 0 ——
     *       0 会一路画成一条贴着横轴的曲线,看着像"电容很小",那是假数据。 */
    s_pair.z_valid = AdcSample_Impedance(ia, qa, ib, qb,
                                         s_pair.r0_ohm, fe_hz, &s_pair.z) ? 1u : 0u;
    if (!s_pair.z_valid) {
        /* 阻抗算不出来**不等于采集失败** —— 两路的幅值、波形都是真的,
         * 仍然要喂给波形页。所以这里不 return false,由调用方看 z_valid。 */
        fprintf(stderr, "[adc] ⚠️ %s 这一点算不出阻抗:%s\n",
                path_name(path), AdcSample_LastError());
    }

    s_adc_points++;
    return true;
}

/**
 * @brief 采一对,**直到这一次采集自洽为止**(2026-10-15 新增)。
 *
 * 【为什么是"重采"而不是"多采几次取中位数"】
 *   CRSS 高压端的尖峰不是"读数有噪声",是**整次采集里混进了坏样本**:
 *   同一个缓冲里,`幅值` 反推的峰峰是 27 码,而 `峰峰` 是 2588 码 —— **94 倍**。
 *   这种点**整次都错**,N 次取中位数会让"错的多数"赢 ⇒ **中位数治不了它**。
 *   有效的问法是先问"这一次算不算数"(判据见 `AdcSample_PpConsistent`),
 *   不算数就**换一次重采**。这是用户口径「多测几次取合理值」的正确形态:
 *   **"合理"由一条物理不变式定义,不由投票定义。**
 *
 * 【重采真的有用吗 —— 实测过】
 *   日志里同一偏压连测 ≥4 次的 **63 组,全部是"有时标、有时不标"**,
 *   **"次次都标"一组都没有** ⇒ 毛刺在同一点上不是恒定的,重采能救。
 *
 * 【返回值的语义(与 `_once` 一致 ⇒ 调用方一行都不用改)】
 *   · false = 采集/解调本身失败(MMCM 没锁 / done 超时 / 窗长不合法…)
 *             —— 重采救不了,直接上报。
 *   · true  = 采到了,再看 `s_pair.z_valid`:
 *              1 → 读数可用;0 → **本点判无读数**(曲线断一格,不填假值)。
 *
 * ⚠️ **判无读数时两路幅值与波形仍是那一次(坏)采集的真实内容** ——
 *    不清成 0:0 会画成一条贴着横轴的曲线,看着像"电容很小",那是**假数据**。
 *    日志里那行 ⛔ 会说明这一点。
 */
static bool adc_capture_pair(void)
{
    uint32_t k;

    for (k = 0u; k < s_adc_tries_max; ++k) {
        float ratio_a = 0.0f;
        float ratio_b = 0.0f;
        bool  ok_a;
        bool  ok_b;

        if (!adc_capture_pair_once()) {
            return false;                       /* 采集/解调失败 —— 重采救不了 */
        }

        ok_a = AdcSample_PpConsistent(s_pair.pp_a, s_pair.mag_a_codes, &ratio_a);
        ok_b = AdcSample_PpConsistent(s_pair.pp_b, s_pair.mag_b_codes, &ratio_b);

        if (ok_a && ok_b) {
            s_pair.tries = k + 1u;
            if (k > 0u) {
                s_adc_rescued++;
                printf("[adc] ✓ 重采第 %u 次自洽,采用(码/期望 A %.2f B %.2f)\n",
                       (unsigned)(k + 1u), (double)ratio_a, (double)ratio_b);
            }
            return true;
        }

        s_adc_rej++;
        printf("[adc] ⚠️ 拒收第 %u/%u 次采集:%s%s(码/期望 A %.2f B %.2f,允 ≤ %.1f)"
               " —— 重采\n",
               (unsigned)(k + 1u), (unsigned)s_adc_tries_max,
               ok_a ? "" : "A 路峰峰与解调幅值对不上;",
               ok_b ? "" : "B 路峰峰与解调幅值对不上;",
               (double)ratio_a, (double)ratio_b,
               (double)ADC_SAMPLE_PP_RATIO_MAX);
    }

    /* 连采满仍不自洽 ⇒ **本点判无读数**。
     * ⛔ 不要在这里"取个中位数凑一个值" —— 那会把 40 倍的假尖峰变成一个
     *    **看着很正常的假数**,比留空坏得多。 */
    s_pair.tries   = s_adc_tries_max;
    s_pair.z_valid = 0u;
    s_adc_void++;
    printf("[adc] ⛔ 本点连采 %u 次都不自洽 ⇒ **判无读数**(曲线断一格)。\n"
           "        排查:另开 SSH 跑 `sudo -E python3 tools/relay_hold.py --watch` 看回路;\n"
           "        对照:把 `CV_ADC_TRIES=1` 起来(关掉重采,退回旧行为,尖峰会回来)。\n"
           "        两路幅值与波形仍是那一次采集的真实内容(仅供波形页看),已置 z_valid=0。\n",
           (unsigned)s_adc_tries_max);
    return true;
}

/** @brief 一行采集证据。扫压时每点一行 —— 出问题时第一眼看的就是它。
 *
 *  🔴 **2026-10-14 改:加了 Z / Cs / Cp / D 与 R0。**
 *     原文只打印两路幅值与 Cs(那会儿相位不可用,打印 Z 是没意义的)。
 *     现在这几项是**要看的第一批数**:`Re Z` 与万用表量的栅极电阻对得上,
 *     就说明整条相位通路是通的 —— 这是 §五 验证里最关键的一条自证。
 *     `R0` 一并打出来,是为了让"挡位到底是多少"这件事**有出处**,不靠猜。 */
/** `码 / (2×幅值)` —— 正常点 ≈ 1.0;几十以上就是这一次采到了坏字。
 *  判据本体在 `AdcSample_PpConsistent()`;这里只是**同一个式子**的取值版,
 *  因为它要同时给出 true/false,没法顺手把比值带出来。 */
static double pp_ratio(float pp_codes, float mag_codes)
{
    double exp_pp = 2.0 * (double)mag_codes;

    return (exp_pp > 0.0) ? ((double)pp_codes / exp_pp) : 0.0;
}

static void adc_log_pair(const char *tag, float bias_v)
{
    uint32_t div = MeasureConfig_GetSampleDiv();

    /* 🆕 尾部两项是 2026-10-15 加的**自洽证据**:
     *    `码/期望` 正常 ≈ 1.0(实测中位 1.02/1.04),几十以上就是这一次采到坏字;
     *    `采 N 次` 就是这个点重采了几次才过判据。尖峰再冒出来时,
     *    第一眼看这两项就能分清"设备真变了"和"采集又采错了"。 */
    printf("[adc] %-8s 偏压 %7.3f V | A/INA tick%+d 幅值 %7.4f V 峰峰 %5.0f 码 | "
           "B/INB tick%+d 幅值 %7.4f V 峰峰 %5.0f 码 | OTR %u | "
           "码/期望 %.2f / %.2f | 采 %u 次\n",
           tag, (double)bias_v,
           (int)AdcSample_PortATick(div), (double)s_pair.mag_a_v, (double)s_pair.pp_a,
           (int)AdcSample_PortBTickHw(div, AdcSample_PortATick(div)),
           (double)s_pair.mag_b_v, (double)s_pair.pp_b,
           (unsigned)s_pair.otr,
           pp_ratio(s_pair.pp_a, s_pair.mag_a_codes),
           pp_ratio(s_pair.pp_b, s_pair.mag_b_codes),
           (unsigned)((s_pair.tries != 0u) ? s_pair.tries : 1u));

    if (s_pair.z_valid) {
        printf("[adc] %-8s Z = %9.3f %+.3f j Ω (R0 = %.0f Ω,挡位 0x%X) | "
               "Cs %9.3f pF | Cp %9.3f pF | D %.5f | I %8.4f mA  (均未标定)\n",
               tag,
               s_pair.z.z_re_ohm, s_pair.z.z_im_ohm,
               s_pair.r0_ohm, (unsigned)s_pair.gear,
               s_pair.z.cs_pf, s_pair.z.cp_pf, s_pair.z.d, s_pair.z.cur_ma);
    } else {
        printf("[adc] %-8s Z 无读数(原因见上面那一行 ⚠️);"
               "两路幅值仍有效,波形照画\n", tag);
    }
}

/**
 * @brief 把 `s_code_a` / `s_code_b` 灌进「单点波形」页(kind 0)。
 *
 * 用户口径:通道 A → 电压图,通道 B → 电流图(A = INA = VH,B = INB = VL)。
 * ⚠️ 通道 B 是**参考电阻 R0 两端的电压**,这里按 `I = VL / R0` 折算成电流,
 *    而 R0 是**从 `0x18` 回读现算的挡位电阻**(不是常数,见 `cv_range_ohms()`)。
 *
 * 🔴 **2026-10-08:采集点数与显示点数在这里分开。**
 *    以前两者是同一个数(`ADC_SAMPLE_POINTS` = 250),所以"读多少画多少"是自洽的。
 *    加长窗之后,解调要的是**满窗 8192 点**,而波形图的容量只有
 *    `UI_Demo_WaveMaxSamples()` = 480 点 —— 直接把 8192 点喂进去,
 *    `ui_demo.c` 会**静默截断**到前 480 点(94% 的采样被丢掉),
 *    屏上表现为"波形是平的或者只有一小段",**不报错**。
 *
 *    ⇒ 喂之前先算「画哪一段」和「抽稀多少」:
 *
 *    **当前档(2026-10-15 起,DIV=4 / 32.5 MSPS;`period` 是整除,`show = min(L,3·period)`)**
 *
 *    | 激励 | 窗长 L | 整周期 m | 一周期点数 period | 显示区间 show | 抽稀 stride | 实喂 fed |
 *    |---|---|---|---|---|---|---|
 *    | 10 kHz  | 6500 |  2 | 3250 | 6500 | 14 | 465 |
 *    | 100 kHz | 8125 | 25 |  325 |  975 |  3 | 325 |
 *    | 1 MHz   | 8190 | 252 |   32 |   96 |  1 |  96 |
 *
 *    **旧档(2026-10-08 ~ 10-14,DIV=2 / 65 MSPS)—— 留作对照,别照着它改代码**
 *
 *    | 激励 | 窗长 L | 整周期 m | period | show | stride |
 *    |---|---|---|---|---|---|
 *    | 10 kHz  | 6500 |    1 | 6500 | 6500 | 14 |
 *    | 100 kHz | 7800 |   12 |  650 | 1950 |  5 |
 *    | 1 MHz   | 8190 |  126 |   65 |  195 |  1 |
 *
 *    取「3 个周期」而不是整窗:整窗周期数一多,480 个像素画几百个周期
 *    只会画出摩尔纹,**看不出波形形状**。
 *    ⚠️ 10 kHz 在 32.5 MSPS 下 `m = 2`(65 MSPS 下是 `m = 1`,正好落在第 1 根
 *       谱线上),`3·period > L` ⇒ 它画的仍是整窗;`m` 从 1 涨到 2 是**副产品改进**。
 *    📌 10 MHz 档已于 2026-10-08 从界面删除(AD9833 的 25 MHz MCLK 输出不了),
 *       本表的对应行随之删除 —— 但**上面的 stride 公式一个字都不用改**：
 *       它是由 `L`/`m` 现算的,不是查表。
 */
static void adc_feed_point_waveform(void)
{
    uint32_t max_pts = UI_Demo_WaveMaxSamples();
    uint32_t l = 0u;
    uint32_t m = 0u;
    uint32_t period;
    uint32_t show;
    uint32_t stride;
    uint32_t i;
    uint32_t fed = 0u;

    AdcSample_LastWindow(&l, &m);

    if ((l == 0u) || (m == 0u)) {
        /* 还没成功解调过(或解调失败)—— 退回"把整块缓冲粗看一遍"。
         * 这时不该假装知道周期,所以整窗抽稀,至少让人看到有没有信号。 */
        l = ADC_SAMPLE_POINTS;
        m = 1u;
    }
    period = l / m;
    if (period == 0u) {
        period = 1u;
    }

    show = period * 3u;
    if (show > l)                  { show = l; }
    if (show > ADC_SAMPLE_POINTS)  { show = ADC_SAMPLE_POINTS; }

    stride = (max_pts == 0u) ? 1u : ((show + max_pts - 1u) / max_pts);
    if (stride == 0u) {
        stride = 1u;
    }

    /* 喂进去的总点数 = ceil(show / stride)。先算出来 —— 这个 API 的
     * `total_samples` 是"最终会有多少点",要一次给全,不能边喂边变。 */
    fed = (show + stride - 1u) / stride;

    /* B 的瞬时值 → 电流:`I = VL / R0`(R0 = 挡位电阻,从 0x18 回读换算)。
     * ⛔ **不要再用 `ADC_SAMPLE_R_TIA`** —— 那是 1 kΩ 的遗留常数,而硬件
     *    复位挡位是 100 Ω,差 10 倍。理由见 `adc_sample.h` 的说明。 */
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_POINT, fed);
    for (i = 0u; i < show; i += stride) {
        float  v   = AdcSample_CodesToVolts((float)s_code_a[i]);
        float  cur = 0.0f;

        if (s_pair.r0_ohm > 0.0) {
            cur = (float)(((double)AdcSample_CodesToVolts((float)s_code_b[i]) /
                           s_pair.r0_ohm) * 1000.0);      /* A → mA */
        }

        UI_Demo_UpdateWaveformPoint(UI_DEMO_WAVE_POINT, (i / stride) + 1u,
                                    fed, v, cur, NULL);
    }

    printf("[adc] 波形:窗长 %u 点(%u 个激励周期),显示前 %u 点、每 %u 点取一 → %u 点\n",
           (unsigned)l, (unsigned)m, (unsigned)show, (unsigned)stride,
           (unsigned)fed);
}

/**
 * @brief 单点测量:等偏压稳定 → 采 → 喂波形 + 填读数卡片。
 *
 * 🔴 **Cs / Cp / D 三项的填法(2026-10-14 改)** —— 这是本函数唯一要判断的地方:
 *    · `Cs` = `−1/(ω·Im Z)`(串联等效),`Cp` = `−Im Z/(ω·|Z|²)`(并联等效),
 *      `D` = `|Re Z / Im Z|`。三项**都由同一个复数 Z 出**(见 `AdcSample_Impedance`)。
 *      原文这里是「Cs 用幅值比(纯电容近似)、Cp/D 一律 `--`」—— 那个限制
 *      **随 §三 的实现一起消失了**:两路现在同相位,Z 算得出来。
 *    · ⛔ **Z 算不出来时三项都留 `--`,不要填 0。** 0 会画成一条贴着横轴的
 *      曲线,看起来像"电容很小",那是**假数据**。`z_valid` 就是这条闸。
 *    · ⚠️ **RG 那一路填的不是电容卡**,走 `UI_Demo_UpdateResistance()`
 *      (`Rg = Re Z + 0.7`,照抄前序 `periodic_measurement.c:549`)。 */
static void point_measure(void)
{
    char     cs_text[16];
    char     cp_text[16];
    char     d_text[16];
    char     rg_text[24];
    uint32_t path  = s_point_path;
    float    volts = (float)s_point_volts_x10 / 10.0f;
    uint32_t valid = 0u;

    s_pair_path = path;                     /* 日志要说明"这是在哪一路采的" */

    if (adc_capture_pair()) {
        valid = 1u;
        adc_feed_point_waveform();
        adc_log_pair("单点", volts);
    } else {
        adc_report_failure("单点测量");
    }

    if (valid && s_pair.z_valid) {
        (void)snprintf(cs_text, sizeof(cs_text), "%.3f", s_pair.z.cs_pf);
        (void)snprintf(cp_text, sizeof(cp_text), "%.3f", s_pair.z.cp_pf);
        (void)snprintf(d_text,  sizeof(d_text),  "%.5f", s_pair.z.d);
    } else {
        (void)snprintf(cs_text, sizeof(cs_text), "--");
        (void)snprintf(cp_text, sizeof(cp_text), "--");
        (void)snprintf(d_text,  sizeof(d_text),  "--");
    }

    /* 🔴 **不再有 `s_point_all` 分支**(2026-10-14 删)。
     *    原文是「一份读数填三张卡」,因为那时继电器从不切换、三条"回路"
     *    在硬件上本就是同一个回路。**这个前提现在没了** —— 三腿序列由
     *    `Process()` 逐腿驱动,每进这里一次就是**一条腿**,`s_point_path`
     *    已经指向它。⛔ 别把"三张卡一起填"加回来:那会让三条曲线重合,
     *    而 L5 判据恰恰是**三条曲线形状必须不同**。 */
    if (path < 3u) {
        UI_Demo_UpdateSingleCapacitance(path, cs_text, cp_text, d_text, valid);
    } else {
        /* ---- RG:栅极电阻 ------------------------------------------------
         * `Rg = Re(Z) + 0.7` —— 前序 `periodic_measurement.c:549` 逐字如此,
         * 那个 0.7 Ω 是前序的 `PERIODIC_MEASUREMENT_RG_CALIBRATION_OHM`
         * (线阻/接触电阻的经验补偿,不是理论值)。**照抄,不自己另立一个数。**
         *
         * ⚠️ 前序为 RG **强制 A 挡(10 Ω)并锁换挡**(`pc_protocol.c:282-284`)。
         *    🆕 **2026-10-15:这条现在可以照做了** —— 挡位有了写入者,
         *    要复现前序的对比度就点 A 挡(10 Ω)。
         *    · 但**没有**在此处自动强制 A 挡:挡位是用户在测量页上选的,
         *      这里替他把挡位改掉,他刚才那一下就成了没发生过的事(而且
         *      下一张卡的 R0 也会跟着变)。**让挡位只有一个人说了算。**
         *    · 留在 B 挡(100 Ω,复位值)也能测,只是对比度差:
         *      `VL/VH = R0/(R0+Rg)` —— Rg ≈ 12 Ω 时 B 挡差量 ~11%,
         *      A 挡 ~55%。**要准就点 A 挡,这是操作选择,不是代码问题。**
         *    · R0 一律回读 `0x18`,所以公式在哪个挡都成立,不受此影响。 */
        uint32_t ok_rg = valid && s_pair.z_valid;

        if (ok_rg) {
            double rg = s_pair.z.z_re_ohm + 0.7;

            /* 显示带单位,和扫压页的读数风格一致 */
            (void)snprintf(rg_text, sizeof(rg_text), "%.2f Ω", rg);
            UI_Demo_UpdateResistance(rg_text, 1u);
            printf("[proto] 点测 RG:Rg = Re Z + 0.7 = %.4f + 0.7 = %.4f Ω"
                   "(R0 = %.0f Ω,挡位 0x%X)\n",
                   s_pair.z.z_re_ohm, rg, s_pair.r0_ohm, (unsigned)s_pair.gear);
        } else {
            /* 算不出来就**留空**,不编一个电阻值出来。 */
            UI_Demo_UpdateResistance("--", 0u);
            printf("[proto] 点测 RG:无 Z 读数,电阻卡保持 --(原因见上面 [adc] 那一行)\n");
        }
    }

    UI_Demo_SetStatusText(valid ?
        "状态:单点完成(Cs 未标定)" :
        "状态:单点失败(见 [adc] 日志)");

    UI_Demo_RefreshResultViews();
}

/* ==========================================================================
 * 扫压状态机
 * ========================================================================== */

/** @brief 本次扫压几个点。终止电压 == 起点时退化成单点,不然会是 60 个重复点。 */
static uint32_t sweep_total_points(float stop_volts)
{
    return (stop_volts <= SWEEP_START_VOLTS) ? 1u : SWEEP_MAX_POINTS;
}

/**
 * @brief 第 @p index_1based 个点的偏压(V),指数间隔。
 *
 * 起止之间等比,和扫压页的 log 横轴一致 —— 曲线才会均匀铺满,不会
 * 全挤在左边。
 *
 * ⚠️ 返回值是**全精度电压**,不做 0.1 V 取整 —— 取整会让低压段最前面
 *    好几个点塌成同一个码(见 `cv_bias_volts_to_code` 的说明)。
 */
static float sweep_bias_volts_at(uint32_t index_1based,
                                 uint32_t total,
                                 float    stop_volts)
{
    float ratio;
    float volts;

    if (total <= 1u) {
        return stop_volts;
    }
    ratio = (float)(index_1based - 1u) / (float)(total - 1u);
    volts = SWEEP_START_VOLTS *
            powf(stop_volts / SWEEP_START_VOLTS, ratio);

    /* powf 会在两端差一两个 LSB —— 夹回去,免得出现 0 V 或越界点 */
    if (volts < SWEEP_START_VOLTS) { volts = SWEEP_START_VOLTS; }
    if (volts > stop_volts)        { volts = stop_volts; }
    return volts;
}

/* --------------------------------------------------------------------------
 * 演示叠加(默认关闭,`CV_UI_DEMO=1` 打开)
 *
 *   扫压在两种模式下**都真的驱动 DAC** —— 差别只在于要不要把
 *   模型编的 Cs/Cp/D 一并塞进曲线。
 *
 *   为什么留着:测量链没接通时,这是唯一能把扫压页画满、检查曲线渲染
 *   和游标交互的办法(也就是 `CV_UI_DEMO_PAGE` 自动截图那条路)。
 *   ⛔ **这里的数字是编的,不得作为任何测量结论的依据。**
 * -------------------------------------------------------------------------- */

static float demo_capacitance(uint32_t path, float voltage)
{
    /* 单调下降:低压平台 → 高压塌陷。量级取自典型 650 V 功率 MOSFET。 */
    static const float c_max[3] = {980.0f, 240.0f, 48.0f};
    static const float c_min[3] = {420.0f, 62.0f, 7.5f};
    float t;

    if (path >= 3u) {
        path = 0u;
    }
    t = 1.0f / (1.0f + powf(voltage / 2.2f, 0.85f));
    return c_min[path] + ((c_max[path] - c_min[path]) * t);
}

/**
 * @brief 测**当前**这一点,把真值填进扫压曲线与扫压波形页。
 *
 * 🔴 **调用时机是这一点的关键**:由 `sweep_step` 在**下一个点开始之前**调用,
 *    那时本点的偏压已经稳定了**一整个驻留时间**。
 *    顺序反过来(写完偏压立刻采)采到的是还没稳定下来的值 —— 曲线会在
 *    每个点刚跳变处失真,而且**不报错**,极难定位。
 *
 * @param index 1 起的点序号(偏压由 `sweep_bias_volts_at` 复算,不另存一份)
 */
static void sweep_measure_point(uint32_t index)
{
    float volts = sweep_bias_volts_at(index, s_sweep_total, s_sweep_stop_volts);
    char  bias_text[16];
    char  cap_text[16];
    uint32_t path = s_sweep_leg;            /* 🔴 只写**当前这条腿**,不再三路共用 */
    uint32_t valid;

    (void)snprintf(bias_text, sizeof(bias_text), "%.2f", (double)volts);
    (void)snprintf(cap_text,  sizeof(cap_text),  "--");

    valid = adc_capture_pair() ? 1u : 0u;
    if (valid && s_pair.z_valid) {
        (void)snprintf(cap_text, sizeof(cap_text), "%.3f", s_pair.z.cs_pf);
        adc_log_pair("扫压", volts);
    } else if (valid) {
        adc_log_pair("扫压", volts);        /* 有波形、只是 Z 算不出来:照打日志 */
    } else {
        adc_report_failure("扫压");
    }

    /* 🔴 **只填 `s_sweep_leg` 这一路。**
     *    原文是 `for (path = 0..2)` 把**同一份读数**写进 mask 里每一条 ——
     *    因为那时三条"回路"在硬件上根本是同一个回路(继电器从不切换)。
     *    现在每条腿各切各的继电器、各采各的,Z 各不相同 ⇒ 再那么填就是编数据。
     *
     * `model_values_valid = z_valid` —— 这个标志位在 `ui_demo.c:944/949` 是
     *    **Cp / D 两条曲线画不画的闸**,与"是不是模型值"无关。有真 Z 就画。 */
    if (path < 3u) {
        UI_Demo_UpdateSweepPoint(path, index, s_sweep_total, volts,
                                 (valid && s_pair.z_valid) ? (float)s_pair.z.cs_pf : 0.0f,
                                 (valid && s_pair.z_valid) ? (float)s_pair.z.cp_pf : 0.0f,
                                 (valid && s_pair.z_valid) ? (float)s_pair.z.d     : 0.0f,
                                 s_pair.z_valid,
                                 bias_text, cap_text, valid);
    }

    /* 「扫压波形」页(kind 1)按既有设计是**每步一个点**的电压/电流包络,
     * 不是 250 点原始波形(那是「单点波形」页的形态)。 */
    UI_Demo_UpdateWaveformPoint(UI_DEMO_WAVE_SWEEP, index, s_sweep_total,
                                volts,
                                (valid && s_pair.z_valid) ? (float)s_pair.z.cur_ma : 0.0f,
                                bias_text);

    UI_Demo_RefreshResultViews();
}

/** @brief 起跑**一条腿**:切它的继电器 → 开它的曲线。
 *
 * 🔴 **切换点在这里而不是按键回调里** —— `RelayView_ApplyPath()` 最坏阻塞
 *    ~400 ms(两次 200 ms 超时)。放进触摸回调会让整个界面卡住;
 *    而这里本来就被 `AdcSample_CapturePair()` 阻塞(采集 + 两次解调),
 *    多这一次不改变既有形态。单点测量的三腿序列同理,见 `DisplayProtocol_Process()`。
 *
 * ⚠️ **切失败不中止扫压** —— 继电器没切过去,那这条腿的曲线是**别条腿的数据**,
 *    但它仍然是一条自洽的曲线,而中止会把前一条腿已经跑出来的结果也丢掉。
 *    所以:打警告、继续跑。判据在 L1/L2(顶栏跟着变),不靠这里兜底。 */
static void sweep_begin_leg(uint32_t leg)
{
    s_sweep_leg = leg;

    printf("[proto] 扫压第 %u 腿:%s —— 先切继电器\n",
           (unsigned)(leg + 1u), path_name(leg));
    if (!RelayView_ApplyPath(leg)) {
        printf("[proto] ⚠️ %s 回路切换未确认 —— 本腿曲线可能是上一条腿的数据。\n"
               "        核实:另开一个 SSH 跑 `sudo -E python3 tools/relay_hold.py --watch`\n",
               path_name(leg));
    }

    UI_Demo_BeginSweep(leg, s_sweep_total, (s_sweep_mask == 0x07u) ? 1u : 0u);
    /* 🆕 「扫压波形」页要开一张新图 —— `UI_Demo_BeginSweep()` 只管扫压曲线,
     *    不碰 `g_result_wave[]`(那是结果页那两页的缓存)。不调这句的话,
     *    上一轮的点会留在图上,新曲线从中间接上去。 */
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_SWEEP, s_sweep_total);
}

/** @param stop_tenths 界面给的终止电压,单位 0.1 V(`g_ui_sweep_end_tenths`) */
static void sweep_start(uint32_t mask, uint32_t stop_tenths)
{
    uint32_t first = 0u;
    float    stop_volts = (float)stop_tenths / 10.0f;

    if (stop_volts < SWEEP_START_VOLTS) {
        stop_volts = SWEEP_START_VOLTS;
    }
    if ((mask & 0x07u) == 0u) {
        mask = 0x01u;
    }
    while ((first < 3u) && ((mask & (1u << first)) == 0u)) {
        first++;
    }

    s_sweep_mask       = mask;
    s_sweep_stop_volts = stop_volts;
    s_sweep_total      = sweep_total_points(stop_volts);
    s_sweep_index      = 0u;
    s_sweep_active     = 1u;
    s_sweep_next_ms    = cv_millis();       /* 第一点立刻发,不等一个驻留 */
    s_auto_gear_off    = 0u;                /* 🆕 新的一轮,重新允许自动换挡 */

    UI_Demo_ClearSweep(3u);                 /* 3 ≥ 3 = 清全部三路 */
    sweep_begin_leg(first);                 /* ← 切继电器 + 开第一条腿的曲线 */

    printf("[proto] 扫压开始:%s,%.2f V → %.2f V,%u 点/腿,每点 %u ms\n"
           "        %s**逐腿**切换继电器,每条腿各测各的。\n"
           "        读数:Cs/Cp/D 由同一个复数 Z 出(未标定)\n"
           "        挡位 0x%X(%.0f Ω):%s\n",
           (mask == 0x07u) ? "CISS+COSS+CRSS" : path_name(first),
           (double)SWEEP_START_VOLTS, (double)stop_volts,
           (unsigned)s_sweep_total, (unsigned)s_dwell_ms,
           (mask == 0x07u) ? "三条腿" : "一条腿",
           (unsigned)(cv_reg_read(CV_REG_RANGE_SEL) & CV_RANGE_MASK),
           cv_range_ohms(cv_reg_read(CV_REG_RANGE_SEL) & CV_RANGE_MASK),
           sweep_auto_gear_enabled()
               ? "🔁 扫压期间**自动换挡已开启**,手动那 4 颗按钮会被盖掉"
               : "🔒 **扫压期间固定不变**(自动换挡默认关闭,要开见 SWEEP_AUTO_GEAR_FLAG)");
}

static void sweep_finish(void)
{
    uint32_t next = s_sweep_leg + 1u;

    /* 🔴 **多腿:这一条跑完接着跑下一条,而不是收尾。**
     *    正文里的"扫压"是三条曲线各扫各的,一次任务要跑三遍全电压范围。
     *    原文在这里直接 `inspection_ready` 收尾 ⇒ 点了"全部"也只出一路。 */
    while ((next < 3u) && ((s_sweep_mask & (1u << next)) == 0u)) {
        next++;
    }
    if (next < 3u) {
        printf("[proto] %s 腿跑完(%u 点),接着跑 %s\n",
               path_name(s_sweep_leg), (unsigned)s_sweep_index, path_name(next));
        s_sweep_index   = 0u;
        s_sweep_next_ms = cv_millis();      /* 下一条腿第一点立刻发 */
        sweep_begin_leg(next);
        return;                             /* ⛔ 不置 inspection_ready —— 还没跑完 */
    }

    s_sweep_active = 0u;

    /* 🔴 **扫压结束必须把偏压收回 0 V —— 这里原来漏了。**
     *    用户 2026-10-15 实测发现:「它这扫完都没有置零」。收尾后读 `0x14 = 0x0007FFFF`
     *    ⇒ 栅极**一直顶着 +100.000 V**,人离开工位、接着点别的回路时都还带着。
     *    ⚠️ 走 `bias_apply(0.0f)` 而不是直接 `cv_reg_write(0x14, 0)`:
     *       表头「偏压:」那一栏只有这条路径会更新(`bias_announce`),
     *       绕过去界面就停在「100.00V」上 —— 那不是省事,是**在仪表上说一句谎话**。
     *    ⚠️ 用 `0.0f` 而不是界面旋钮下限 0.1 V:收尾要的是**真零**,
     *       即码 `0x00000`(**不是** `0x80000` —— 补码下那是 −100 V,见 `pynq_regs.h`)。 */
    (void)bias_apply(0.0f);

    UI_Demo_SetSweepInspectionReady();
    UI_Demo_SetStatusText(s_demo_enabled ?
        "状态:扫压结束(演示读数,偏压已归 0)" :
        "状态:扫压结束(偏压已归 0,Cs/Cp/D 未标定)");
    printf("[proto] 扫压结束:%s,共 %u 点已下发到 DAC\n",
           path_name(s_sweep_leg), (unsigned)s_sweep_index);

    /* 🆕 自洽判据的账本(2026-10-15)。三个数一起看才有意义:
     *    · `拒收` 高、`判无效` 为 0 ⇒ 毛刺勤,但重采每次都救回来了(**现状**)。
     *    · `判无效` 不为 0         ⇒ 有整点失守 —— 曲线会断格,去看 ⛔ 那几行。
     *    · 三个都是 0              ⇒ 这一轮干净(或 `CV_ADC_TRIES=1` 关掉了判据,
     *                                那种情况下**尖峰会回到曲线上**,别误读成"好了")。 */
    printf("[proto] 采集自洽账本:实际采 %u 次,判据拒收 %u 次,"
           "重采救回 %u 点,判无效 %u 点(CV_ADC_TRIES=%u)\n",
           (unsigned)s_adc_points, (unsigned)s_adc_rej,
           (unsigned)s_adc_rescued, (unsigned)s_adc_void,
           (unsigned)s_adc_tries_max);
}

/**
 * @brief 🆕 2026-10-15:用**刚测到的** `|Z|` 决定下一个点该用哪一挡参考电阻。
 *
 * 【为什么需要它 —— 用户 2026-10-15 的原话:「不然数据都很奇怪」】
 *   挡位原来全靠手动选,而一次扫压横跨 0.1 V → 100 V,`|Z|` 变 **40 倍以上**
 *   (实测 CRSS 腿:363 Ω → 15 313 Ω),却全程停在用户点的那一挡上。后果不是"略差":
 *
 *   | 偏压 | `|Z|` | 挡位(手动 B=100 Ω) | `VL/VH` | 结局 |
 *   |---|---|---|---|---|
 *   | 0.10 V | 363 Ω | B | 23 % | 可用(只是没到最优) |
 *   | 6.77 V | 441 Ω | B | 20 % | 开始吃紧 |
 *   | 13.66 V | 5 231 Ω | B | **1.9 %** | 已不可信 |
 *   | 100 V | 15 313 Ω | B | **0.64 %** | `VL` 只剩 23~36 码峰峰 |
 *
 *   高压段 `VL` 掉到 12 位 ADC 的量化/噪声底,**而 `Z` 恰恰是拿 `VH/VL` 这个
 *   比值算出来的** ⇒ `VL` 的噪声被按 `|Z|/R0` 倍放大**直接乘进 Cs**。
 *   实测那段 Cs 出现"一个倍压内掉 14 倍"(6.77 V:434 pF → 13.66 V:31 pF),
 *   那不是器件特性,是量程没跟上。
 *
 * 【为什么判据是 `|Z| ≈ R0` 而不是别的】
 *   误差的两个来源在此处相等,推导与迟滞宽度见 `pynq_regs.h::cv_range_pick()`。
 *
 * 【为什么"拿上一点的结果定下一点"是合法的】
 *   `|Z|` 随偏压**单调**(电容随偏压单调下降),所以相邻两点的最优挡位只会
 *   相邻或相同 —— 用上一点选下一点,**最多落后一个点**,而且第一跳之后
 *   就收敛了。日志每行都印了当时的挡位(`adc_log_pair`),落后与否看得见。
 *
 * ⛔ **不做的两件事(都是有意不做,不是漏了)**
 *   ① **不给第 1 点预扫一次**。预扫要阻塞一个驻留(≈300 ms + 采集),
 *      而本函数所在的 `sweep_step()` 由 `DisplayProtocol_Process()` 在主循环里调,
 *      `sweep_start()` 却是**在触摸回调里**调的 —— 在那里阻塞会让界面卡住,
 *      正是 `sweep_begin_leg()` 注释里立过的那条规矩。
 *      ⇒ 第 1 点用"当前挡位"(用户手动选的那一挡就是起手提示),第 2 点起自动。
 *   ② **跑完不复原用户的挡位**。扫压期间挡位归自动管(顶栏与按钮高亮都是
 *      `0x18` 回读,看得见);收尾时停在**最后一点真正用过的**那一挡上,
 *      那正是看曲线时最想知道的。
 */
static void sweep_autoscale(void)
{
    double   z_mag;
    uint32_t cur, pick;

    if (s_auto_gear_off != 0u) {
        return;
    }
    /* 🔴 **默认关闭** —— 挡位固定。理由与开关文件见 `SWEEP_AUTO_GEAR_FLAG`。 */
    if (!sweep_auto_gear_enabled()) {
        return;
    }
    /* ⚠️ `|Z|` 不可信就别据它换挡 —— 那等于拿噪声去选量程。
     *    这一点的读数会照常填进曲线(`s_pair.z_valid` 为 0 时曲线本来就不画点)。 */
    if (!s_pair.z_valid) {
        return;
    }
    z_mag = sqrt((s_pair.z.z_re_ohm * s_pair.z.z_re_ohm) +
                 (s_pair.z.z_im_ohm * s_pair.z.z_im_ohm));
    if (!(z_mag > 0.0)) {
        return;
    }

    cur  = cv_reg_read(CV_REG_RANGE_SEL) & CV_RANGE_MASK;
    pick = cv_range_pick(z_mag, cur, SWEEP_GEAR_MARGIN);
    if (pick == 0u) {
        /* 只有 |Z| 无效才会走到这里,上面已经挡过 —— 真到了就说明判据自身异常。 */
        printf("[proto] ⚠️ 自动换挡:判据返回 0(不该发生),本轮不再自动换挡\n");
        s_auto_gear_off = 1u;
        return;
    }
    if (pick == cur) {
        return;                       /* 迟滞窗口内 —— 什么都不做 */
    }

    if (!RelayView_ApplyGear(pick)) {
        printf("[proto] ⚠️ 自动换挡 %.0f Ω(0x%X)未确认 —— **本轮不再尝试**;\n"
               "        下一点仍按 0x%X 算(每行 Z 日志都印了当时挡位),\n"
               "        要手动接管:测量页那 4 颗挡位按钮。\n",
               cv_range_ohms(pick), (unsigned)pick, (unsigned)cur);
        s_auto_gear_off = 1u;
        return;
    }

    /* `VL/VH = R0 / |R0 + Z|` —— **相量和的模**,不是 `R0/(R0+|Z|)`。
     * 两者在 `|Z| ≫ R0` 时接近,在 `|Z| ≈ R0` 时差一倍 ——
     * 本函数恰恰常在 `|Z| ≈ R0` 附近做判断,所以只能写对的那个。
     * (左边那个数是**实测**的 `mag_b/mag_a`,右边是换算到新挡的**预测值**。) */
    {
        double r_cur  = cv_range_ohms(cur);
        double r_pick = cv_range_ohms(pick);
        double d_new  = sqrt(((r_pick + s_pair.z.z_re_ohm) * (r_pick + s_pair.z.z_re_ohm)) +
                             (s_pair.z.z_im_ohm * s_pair.z.z_im_ohm));

        printf("[proto] 🔁 自动换挡:%.0f Ω(0x%X)→ %.0f Ω(0x%X)| "
               "|Z| = %.0f Ω,VL/VH 实测 %.1f%% → 预计 %.1f%%(R0=%.0f Ω,+%.0f%% 于 |Z|)\n",
               r_cur, (unsigned)cur, r_pick, (unsigned)pick, z_mag,
               (s_pair.mag_a_v > 0.0f) ? (100.0 * (double)s_pair.mag_b_v / (double)s_pair.mag_a_v) : 0.0,
               (d_new > 0.0) ? (100.0 * r_pick / d_new) : 0.0,
               r_pick,
               (z_mag > 0.0) ? (100.0 * ((r_pick / z_mag) - 1.0)) : 0.0);
    }
}

/** @brief 下发一个扫压点。由 `DisplayProtocol_Process()` 按驻留时间调。 */
static void sweep_step(void)
{
    float    voltage;
    char     bias_text[16];
    char     cap_text[16];

    /* ① ★ 先收尾**上一个**点:它的偏压已经稳定了**整整一个驻留时间**。
     *    ⛔ 顺序不能倒 —— 先写偏压再立刻采,采到的是还没稳定下来的值,
     *      曲线会在每个点的跳变处失真,而且**不报错**,极难定位。
     *    (演示模式下读数由模型给,不采。) */
    if ((s_sweep_index > 0u) && !s_demo_enabled) {
        sweep_measure_point(s_sweep_index);
        /* 🆕 ①.5 **趁这一点的新鲜 |Z| 在手上,先把下一点该用的挡位换好。**
         *  放这个位置的三个理由:
         *   · 这里才刚拿到 |Z|,判据要的就是它;
         *   · 在②的"写下一个偏压"之前 ⇒ 换挡的机械稳定时间(挡位继电器)
         *     和偏压的建立时间**重叠**在同一个驻留里,不多花时间;
         *   · `RelayView_ApplyGear()` 只做几次寄存器读写、**不阻塞**
         *     (挡位分支不看 `busy_r`,见 `relay_ctrl.v`),所以放进
         *     `Process()` 这条主循环里是安全的 —— 与 `ApplyPath()` 不同,
         *     那个最坏阻塞 200 ms×2,只能待在腿与腿之间。
         *  ⛔ 演示模式不换挡:那读数是模型编的,拿它选量程毫无意义。 */
        sweep_autoscale();
    }

    /* ② 再推进下一个点。
     * ⚠️ 先判满再自增,顺序不能倒 —— 倒了的话 `s_sweep_index` 会在结束时
     *    等于 total+1,而它是要拿去打印和显示"已下发几点"的
     *    (实测:60 点的扫压会报成"61 点已下发")。 */
    if (s_sweep_index >= s_sweep_total) {
        sweep_finish();
        return;
    }
    s_sweep_index++;

    voltage = sweep_bias_volts_at(s_sweep_index, s_sweep_total,
                                  s_sweep_stop_volts);

    bias_apply(voltage);                    /* ← 真正的硬件动作 */

    (void)snprintf(bias_text, sizeof(bias_text), "%.2f", (double)voltage);

    /* 🔴 **只推进 `s_sweep_leg` 这一路**(原文 `for (path = 0..2)` 给每条置位腿
     *    都补一个点 —— 那是在"三条回路其实是同一个回路"的前提下成立的)。
     *    现在腿与腿之间靠 `sweep_finish()` 串起来,同一时刻只有一条腿在跑。 */
    {
        uint32_t path = s_sweep_leg;
        float    cs = 0.0f;
        float    cp = 0.0f;
        float    d  = 0.0f;
        uint32_t model_valid = 0u;
        uint32_t valid       = 0u;
        const char *cap = "--";

        if (s_demo_enabled) {
            cs = demo_capacitance(path, voltage);
            cp = cs * (1.0f + (0.02f * voltage));
            d  = 0.015f + (0.0012f * voltage);
            model_valid = 1u;
            valid       = 1u;
            (void)snprintf(cap_text, sizeof(cap_text), "%.2f", (double)cs);
            cap  = cap_text;
        }

        /* valid = 0 时 ui_demo.c 会把这个点记成无效:进度条照走、
           曲线不画、"Vd=" 那一栏显示真实偏压并缀上「无效」。
           这就是"偏压真走了、电容还没测"的正确形态。
           ⚠️ 这里**不填读数** —— 读数由 `sweep_measure_point()` 在**下一个点
              开始之前**补上(偏压稳定了整整一个驻留时间才采,见该函数头)。 */
        UI_Demo_UpdateSweepPoint(path, s_sweep_index, s_sweep_total,
                                 voltage, cs, cp, d, model_valid,
                                 bias_text, cap, valid);
    }

    s_sweep_next_ms = cv_millis() + s_dwell_ms;
}

/* --------------------------------------------------------------------------
 * 无值守自动跑一遍(只在设了 `CV_UI_DEMO_PAGE` 时):
 *   三路扫压 → 跑完切到指定页。用途只有一个:**截一张"有数据"的页面图**。
 * -------------------------------------------------------------------------- */

static void demo_auto_run(void)
{
    if (s_demo_auto_page == 0u) {
        return;
    }

    if (s_demo_auto_stage == 0u) {
        s_demo_auto_stage = 1u;
        sweep_start(0x07u, 1000u);          /* 0.1 V → 100 V */
        return;
    }

    if ((s_demo_auto_stage == 1u) && (s_sweep_active == 0u) &&
        (s_sweep_index >= s_sweep_total)) {
        UI_Demo_GotoPage(s_demo_auto_page);
        s_demo_auto_stage = 2u;
    }
}

/* ==========================================================================
 * 生命周期
 * ========================================================================== */

void DisplayProtocol_Init(void)
{
    const char *env;

    s_cmd_total         = 0u;
    s_readback_pending  = 0u;
    s_path_cmds         = 0u;
    s_bias_writes       = 0u;
    s_bias_verify_fail  = 0u;
    s_verify_notice     = 0;
    s_sweep_active      = 0u;
    s_sweep_index       = 0u;
    s_sweep_total       = 0u;
    s_demo_auto_stage   = 0u;
    s_demo_enabled      = 0;
    s_dwell_ms          = SWEEP_DWELL_MS_DEF;

    s_point_pending     = 0u;
    s_point_due_ms      = 0u;
    s_point_path        = 0u;
    s_point_volts_x10   = 0u;
    s_point_all         = 0u;
    s_point_leg         = 0u;
    s_point_leg_n       = 1u;
    s_sweep_leg         = 0u;
    s_pair_path         = 0u;
    s_adc_points        = 0u;
    s_adc_fail          = 0u;
    s_adc_notice        = 0;
    s_adc_rej           = 0u;
    s_adc_rescued       = 0u;
    s_adc_void          = 0u;
    s_adc_tries_max     = ADC_SAMPLE_TRIES_MAX;
    (void)memset(&s_pair, 0, sizeof(s_pair));

    env = getenv("CV_UI_DEMO");
    if ((env != NULL) && (env[0] == '1')) {
        s_demo_enabled = 1;
    }

    env = getenv("CV_SWEEP_DWELL_MS");
    if ((env != NULL) && (env[0] != '\0')) {
        long v = strtol(env, NULL, 10);
        if ((v >= (long)SWEEP_DWELL_MS_MIN) && (v <= (long)SWEEP_DWELL_MS_MAX)) {
            s_dwell_ms = (uint32_t)v;
        }
    }

    /* 🆕 自洽判据的**重采次数**(2026-10-15)。范围 [1,16]:
     *   1   = 关掉重采,退回旧行为 —— **用来做 A/B 对照**:同一个点连扫两轮,
     *        带 `CV_ADC_TRIES=1` 那轮尖峰应该回来,默认那轮不该有。
     *        这是证明"判据真的在起作用"最省事的办法(而不是只看代码)。
     *   >1  = 第 1 次不自洽就重采,最多采这么多次。
     * ⚠️ 上界 16 不是随手定的:一次采集 ~6 ms,扫压驻留 300 ms,
     *    16 次 = 96 ms ≈ 三分之一个驻留 —— 再大就会把扫压拖慢到看得见。 */
    env = getenv("CV_ADC_TRIES");
    if ((env != NULL) && (env[0] != '\0')) {
        long v = strtol(env, NULL, 10);
        if ((v >= 1) && (v <= 16)) {
            s_adc_tries_max = (uint32_t)v;
        }
        printf("[proto] 采集自洽判据:每点最多采 %u 次(CV_ADC_TRIES=%s)\n",
               (unsigned)s_adc_tries_max, env);
    }

    env = getenv("CV_UI_DEMO_PAGE");
    s_demo_auto_page = ((env != NULL) && (env[0] >= '0') && (env[0] <= '9')) ?
                       (uint32_t)(env[0] - '0') : 0u;

    /* ⚠️ 这两句必须在 UI_Demo_Init() 之后跑 —— 它们会动 LVGL 标签。
     *    调用顺序由 main.c:361→364 保证。 */

    /* ---- 测量参数:激励 1 MHz + 采样率(取 `CV_ADC_DIV_DEFAULT`,32.5 MSPS)+ 相位到窗中心 ----
     * 🔄 2026-10-06:原来这里只写 0x80(激励)。现在整组参数归 `MeasureConfig`,
     *    它同时会刷「系统复位」页那一行常驻状态 —— 本页此刻**还没构建**
     *    (首次进入才建),那次刷新会被静默丢弃,页面构建时会自己补拉,
     *    见 `ui_demo.c::UI_RefreshInitStatusLine()`。 */
    MeasureConfig_Init();

    /* ---- 偏压:0 V(中位码)---- */
    bias_apply(0.0f);

    UI_Demo_SetConnection(1u);              /* 寄存器通了,后端算"在线" */
    /* 🔴 上电**读回真值**,不再写死字符串 —— 原先这里是 `"CISS"`,
     *    而硬件复位态其实是 `ALL_OFF(0x10)`:顶栏从开机第一秒就在说谎。 */
    (void)RelayView_Refresh(1);

    if (s_demo_enabled) {
        UI_Demo_SetStatusText("状态:演示模式(偏压真下发,读数由模型生成)");
        printf("[proto] ⚠️  演示模式已开启(CV_UI_DEMO=1)\n"
               "        偏压/扫压**是真的**在驱动 DAC;\n"
               "        但 Cs/Cp/D 曲线是**模型生成的假数据**,只用于检查界面。\n"
               "        ⛔ 不得作为任何测量结论的依据。\n"
               "        关闭:不带 CV_UI_DEMO 重启。\n");
    } else {
        UI_Demo_SetStatusText("状态:就绪(Cs/Cp/D 取自 ADC,未标定)");
    }

    /* ⚠️ 激励那一项**从 MeasureConfig 读回**,不照抄常量 ——
     *    照抄的话哪天默认值改了,这一行会替一个没生效的值作证。 */
    printf("[proto] 已接管测量指令(C 侧直写寄存器,方案 A)\n"
           "        激励 = %u Hz(0x80 = 0x%05X);采样率/相位明细见上面 [mc] 那几行\n"
           "        偏压 = 0.0 V(0x14 = 0x%05X)\n"
           "        扫压驻留 = %u ms(可用 CV_SWEEP_DWELL_MS 覆盖)\n"
           "        🟢 回路:0x1C **本进程是唯一写入方**(点测量项目才切,见 relay_view.c);\n"
           "        🟢 档位:0x18 **本进程也是唯一写入方**(2026-10-15 起,"
           "测量页 A/B/C/D 四颗按钮)。\n"
           "           ⚠️ 开机挡位由 RTL 复位值决定(标称 B = 100 Ω),"
           "但**以 0x18 回读为准** —— 每次测量都现读,不缓存。\n"
           "           Cs/Cp/D 由同一个复数 Z 出(Z = R0·(VH−VL)/VL,未标定)\n",
           (unsigned)MeasureConfig_GetDdsHz(),
           (unsigned)cv_dds_hz_to_code(MeasureConfig_GetDdsHz()),
           (unsigned)cv_bias_tenths_to_code(0u),
           (unsigned)s_dwell_ms);
}

void DisplayProtocol_Process(void)
{
    /* 🔴 每圈都调(函数内部自带 200 ms 节流)—— 这是顶栏「回路:」「档位:」
     *    两栏的**轮询点**。放在最前面:它跟测量状态无关,扫压停着的时候
     *    也要跟着硬件走(比如另一个进程用 relay_hold.py 改了组合)。 */
    (void)RelayView_Refresh(0);

    demo_auto_run();

    /* ---- 单点测量:偏压稳定了再采 ----
     * 🔴 必须挂在下面那句 `if (!s_sweep_active) return;` **之前** ——
     *    非扫压期间才是单点测量的主场景,放到后面等于永远不执行。
     *
     * 🆕 **2026-10-14:继电器切换挪到这里,不放在按键回调。**
     *    `RelayView_ApplyPath()` 最坏阻塞 ~400 ms(两次 200 ms 超时),
     *    放进触摸回调会让界面卡住;而这一支本来就要被 `AdcSample_CapturePair()`
     *    阻塞(采集 + 两次解调),多这一次不改变既有形态。
     *
     * 「全部」的三腿序列就在这里推进:每条腿到点 → 切它的继电器 → 采 + 填它那张卡
     * → 还有下一腿就把 due 推后一个驻留、`pending` 重新置起。 */
    if (s_point_pending && ((int32_t)(cv_millis() - s_point_due_ms) >= 0)) {
        s_point_pending = 0u;

        if (s_point_all) {
            /* ① 切当前腿(幂等:已经是这一路就直接返回,见 `RelayView_ApplyCode()`) */
            if (!RelayView_ApplyPath(s_point_leg)) {
                printf("[proto] ⚠️ 三腿点测:第 %u 腿(%s)切换未确认 —— "
                       "该卡填出来的可能是上一条腿的数据,请核对顶栏「回路:」\n",
                       (unsigned)(s_point_leg + 1u), path_name(s_point_leg));
            }
            s_point_path = s_point_leg;

            /* ② 采 + 只填第 `s_point_leg` 张卡(`point_measure()` 内按 s_point_path 分支) */
            point_measure();

            /* ③ 还有下一腿 → 推到下一个驻留后,不立刻采 */
            s_point_leg++;
            if (s_point_leg < s_point_leg_n) {
                s_point_due_ms  = cv_millis() + s_dwell_ms;
                s_point_pending = 1u;
            } else {
                printf("[proto] 三腿点测完成(%u 条腿,每条各切各的继电器)\n",
                       (unsigned)s_point_leg_n);
            }
        } else {
            /* 单路(含 RG):先切,再采 */
            if (!RelayView_ApplyPath(s_point_leg)) {
                printf("[proto] ⚠️ 点测 %s:切换未确认 —— 填入的读数可能是上一条腿的\n",
                       path_name(s_point_leg));
            }
            s_point_path = s_point_leg;
            point_measure();
        }
    }

    if (!s_sweep_active) {
        return;
    }

    /* 有符号比较:cv_millis() 会回绕,直接比大小会在回绕点卡住 */
    if ((int32_t)(cv_millis() - s_sweep_next_ms) < 0) {
        return;
    }

    sweep_step();
}

/* ==========================================================================
 * 请求
 *
 * ⚠️ 返回值只影响界面状态栏那一行(见 ui_demo.c:2815)。
 *    这里**如实返回**:偏压真的动了就 true;动了但读数回不来也 true,
 *    因为"指令发出去了"这件事没有说谎 —— 说谎的是"测到了"。
 *    别拿"状态栏没报错"当测量成功的判据。
 * ========================================================================== */

/**
 * @brief 「系统复位」页那个按钮 —— **所有参数回默认** + 停扫压 + 清界面。
 *
 * 🔄 2026-10-06:原先只复三样(偏压 / 激励 / 界面),用户要求"按下后所有参数
 *    恢复默认",所以补上了**采样率与相位**。现在覆盖面是:
 *
 *      `0x14` 偏压      → 0 V(码 `0x00000`)
 *                          ⚠️ 原文这里写 `0x80000` —— 那是**补码下的 −100 V**
 *                          (见 `pynq_regs.h` 的 2026-10-12 实测段),已经写错了。
 *                          实际执行的是 `bias_apply(0.0f)`,码 `0x00000` 才对。
 *      `0x80` 激励      → 1 MHz
 *      `0x88` 采样率    → `CV_ADC_DIV_DEFAULT` → 32.5 MSPS(DIV=4,2026-10-15 起)
 *      `0x88` 相位      → 该分频比的窗口中心(DIV=4 → **+1**;公式现算,不是常数)
 *      `0x1C` 回路      → **ALL_OFF(0x10)**(🆕 2026-10-14:回安全态)
 *      扫压状态 / 曲线  → 清空
 *      常驻状态行       → 刷新(由 MeasureConfig 自己推)
 *
 * 🔴 **`0x1C` 只在复位时写 `ALL_OFF`,不复位到别的组合。** `ALL_OFF` = K5 单合,
 *    是 `relay_ctrl.v` 自己的复位值 —— 即"这颗芯片认为的安全态"。
 *    ⚠️ 提醒:`relay_ctrl.v:183-186` 自认「这两个复位值**我没有独立复核过物理安全性**」,
 *       上电第一次请按 L1(听咔哒声 + 不接 DUT 先量电位)验,别把复位当已验证。
 *
 * 🔴 **`0x84`(CV_CTRL)不在复位范围内,一个字都不许写。**
 *    `[0]` 是 `dac_rst_n`,**放行权由 Python 独占**(`drivers/board.py::release_dac()`,
 *    它先验 FCLK0 在不在 [46.1, 54.0] MHz 窗口内,不在就拒绝)。
 *    C 侧把它清掉 = 把 DAC 重新按回复位态 ⇒ 输出跳回 −10 V,
 *    而界面与寄存器回读**一切正常** —— 2026-10-05 就是从这个现象查起的。
 *
 * ⚠️ 返回值仍是**如实**的:偏压真的动了、激励/采样率真的写了就 true。
 *    🆕 2026-10-06:复位后采样率/相位**会**变(MeasureConfig 回默认),
 *    而 `adc_sample.c` 每次采集都现读 `MeasureConfig` 再下发刻度,所以
 *    复位不需要额外通知采集层。但**在途的单点采集要作废**(见下面
 *    `s_point_pending = 0` 那句),否则会把复位前的偏压读数填进卡片。
 */
bool DisplayProtocol_RequestReset(void)
{
    bool     ok;
    uint32_t err;

    s_cmd_total++;
    s_sweep_active = 0u;
    s_sweep_total  = 0u;
    s_sweep_index  = 0u;
    /* 在途的单点采集也作废 —— 复位之后偏压已经归 0,
     * 这一拍采到的是复位前的偏压,填进卡片就是**陈旧数据冒充新测量**。 */
    s_point_pending = 0u;

    err = bias_apply(0.0f);                 /* 0x14 偏压归 0 V */

    /* 0x80 + 0x88(DIV 与 PH_OFF 一起)回默认,并刷本页常驻状态行。
     * ⚠️ 不复位 0x84 —— 理由见函数头。 */
    ok = MeasureConfig_RestoreDefaults();
    ok = ok && (err == 0u);

    /* 🆕 回路回安全态。⚠️ 放在 `ClearAllResults()` **之前** —— 它最坏阻塞
     *    ~400 ms,而这期间顶栏会由 `RelayView_Refresh(1)` 自己刷成 ALL_OFF;
     *    顺序反过来只是"先清界面再等继电器",观感一样,但万一卡住,先清完
     *    至少界面是干净的。 */
    if (!RelayView_ApplyCode(CV_RELAY_ALL_OFF)) {
        printf("[proto] ⚠️ 复位:回路未能切回 ALL_OFF —— 硬件仍停在原组合。\n"
               "        这不是「复位失败」:偏压/激励/采样率都回默认了,"
               "只有继电器没确认。手动兜底:sudo -E python3 tools/relay_hold.py --code 0x10\n");
    }

    UI_Demo_ClearSweep(3u);
    UI_Demo_ClearAllResults();
    /* 🆕 结果页那两张「调试曲线」也得清 —— `ClearAllResults()` 只管读数卡片
     *    和结果快照,`g_result_wave[]` 不在它管的范围里。不清的话复位之后
     *    波形还留着上一轮的采集,曲线状态行也照旧写「已采集 N 点」——
     *    正是用户提过的「复位了下面的还不变」。
     *    (传 0 点:`BeginWaveform` 会把缓冲清空并置 `have = 0`,
     *     `UI_ResultWaveRefresh()` 随即把状态行改回「尚未收到…」。) */
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_POINT, 0u);
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_SWEEP, 0u);
    /* 这次刷新是**必须**的:上面 `ClearAllResults()` 内部那次刷新发生在
     * 清波形**之前**,不重刷的话图上还留着旧点。 */
    UI_Demo_RefreshResultViews();
    UI_Demo_SetStatusText(ok ?
        "状态:已复位(偏压 0 V,激励 1 MHz,采样率回默认)" :
        "状态:复位已下发,但参数回读不一致(查日志)");

    printf("[proto] 复位:0x14 ← 0x%05X(0.0 V);激励/采样率/相位回默认见上面 [mc] 那行\n"
           "        ⚠️ 0x84(CV_CTRL)未动 —— DAC 放行权归 Python 侧\n",
           (unsigned)cv_bias_tenths_to_code(0u));
    return ok;
}

/**
 * @brief 「测量项目」按钮 —— **这一支现在真的切继电器**(用户 2026-10-14 的问题就是它)。
 *
 * 🔴 **原文是「只读回读,未改硬件」**,根因见 `relay_view.c` 文件头那张表:
 *    PL 的 `relay_ctrl.v` 早就写好了,`drivers/relay.py` 也实测过,但
 *    **Python 侧零调用方、C 侧只读** ⇒ 硬件永远停在 `ALL_OFF`。按钮只换了高亮颜色。
 *
 * 顶栏那一栏仍然归 `RelayView_Refresh()` 管,显示的永远是**硬件现在在哪路**
 * (回读 `0x1C`);按钮高亮(`g_ui_single_path`)表示「我**要**测哪路」——
 * 两者本来是两件事,分开才诚实。切换**成功**时它们自然一致;失败时不一致,
 * 那正是要看见的状态(见下面 printf)。
 *
 * @param path 0=CISS 1=COSS 2=CRSS 3=RG(**都真的切**)
 *             4=「全部」→ 不在这里切,见 `RelayView_ApplyPath()` 的说明
 * @return 切换是否确认成功;`path == 4` 恒 true(它本来就不该在这里切)
 */
bool DisplayProtocol_RequestPath(uint32_t path)
{
    s_cmd_total++;
    s_path_cmds++;

    (void)RelayView_Refresh(1);             /* force:先把真值读上来打底 */

    if (path > 3u) {
        printf("[proto] 回路按钮 → 全部:**不在这里切**。"
               "「全部」是「三条腿各测一次」的编排,\n"
               "        切换发生在每条腿**测量开始时**(点测见 Process(),扫压见 sweep_begin_leg())\n");
        return true;
    }

    if (RelayView_ApplyPath(path)) {
        printf("[proto] 回路按钮 → %s:已切换并回读确认\n", path_name(path));
    } else {
        printf("[proto] ⚠️ 回路按钮 → %s:切换**未确认** —— 顶栏显示的是硬件真值(可能还是旧回路)。\n"
               "        另开 SSH 核对:sudo -E python3 tools/relay_hold.py --watch\n",
               path_name(path));
    }
    return true;
}

/**
 * @brief 挡位按钮 A/B/C/D —— 真的写 `0x18`(2026-10-15 新增)。
 *
 * 🔴 **在此之前全工程没有 `0x18` 的写入者**:顶栏显示它、`cv_range_ohms()`
 *    换算 R0、`point_measure()` 每次测量现读它 —— 但**没有人写它**。
 *    于是挡位矩阵即使物理接上了,也永远停在 RTL 复位值 **B 挡(100 Ω)**。
 *    ⇒ 与 2026-10-14 那个"回路按钮只换颜色"是**同一类缺陷**,只是那一轮
 *      收 `0x1C` 时把 `0x18` 漏掉了(当时挡位不在范围内)。
 *
 * ⚠️ **与 `RequestPath()` 的一处不同**:那边切完要等继电器稳定窗(≤400 ms),
 *    所以实际切换推迟到 `Process()` 到点时做;这里**当场写**
 *    —— 挡位是模拟开关、`0x18` 分支在 RTL 里当拍生效且不看 busy
 *    (理由见 `relay_view.h::RelayView_ApplyGear()` 的说明),
 *    阻塞只有两次 AXI 往返(µs 级),放在触摸回调里安全。
 *
 * ⛔ **不碰 `s_bias`、不重排采集** —— 挡位变了之后正在扫压/正在等的那个点,
 *    其 R0 会变而它是在写 `0x18` **之前**起的,归属说不清。所以:
 *    正在跑扫压时挡位按钮**照切但不推荐**(切完这条曲线的 R0 前后不一致,
 *    数据里看得出来)。这一条**没有加互锁** —— 加锁会让用户以为按钮坏了;
 *    判据是日志:每个 `[relay] 挡位 →` 行都带 R0,事后能对上。
 *
 * @param index 0=A(10Ω) 1=B(100Ω) 2=C(1kΩ) 3=D(10kΩ);越界直接拒,不动硬件
 * @return 是否已下发**且**回读确认(`false` 时原因已 printf + 弹框)
 */
bool DisplayProtocol_RequestGear(uint32_t index)
{
    static const uint32_t onehot[4] =
        {CV_RANGE_A, CV_RANGE_B, CV_RANGE_C, CV_RANGE_D};

    s_cmd_total++;
    /* 挡位也算"测量设置类"指令,与回路按钮共用计数 ——
     * `DisplayProtocol_GetPathCommandCount()` 报的是"设置类按钮总点击数",
     * 语义从 2026-10-15 起略微变宽。⛔ 不新开一个计数器:它只被打印用,
     * 再分一类只会让那行日志更长而不更有用。 */
    s_path_cmds++;

    if (index > 3u) {
        printf("[proto] ⛔ 挡位按钮下标 %u 越界(只有 0..3),不下发\n",
               (unsigned)index);
        return false;
    }

    /* force 刷一次:让顶栏与按钮先站到"硬件现在在哪挡"上。
     * ⚠️ 不能省 —— 若上一次写失败,按钮高亮还停在旧挡,先刷一次保证
     *    "写之前显示的是真值",失败之后显示的也还是真值。 */
    (void)RelayView_Refresh(1);

    if (RelayView_ApplyGear(onehot[index])) {
        printf("[proto] 挡位按钮 → %s:已切换并回读确认(R0 = %.0f Ω)\n",
               gear_name(index), cv_range_ohms(onehot[index]));
        return true;
    }

    printf("[proto] ⚠️ 挡位按钮 → %s:切换**未确认** —— 顶栏与按钮高亮显示的"
           "仍是硬件真值(可能还是旧挡)。\n"
           "        四个读数(Rg / Cs / Cp / I)都按【当前挡位】反演,"
           "挡位没切成功就不要拿这批数当真。\n",
           gear_name(index));
    return false;
}

bool DisplayProtocol_RequestBias(uint32_t bias_tenths)
{
    s_cmd_total++;

    if (bias_tenths < CV_BIAS_TENTHS_MIN) { bias_tenths = CV_BIAS_TENTHS_MIN; }
    if (bias_tenths > CV_BIAS_TENTHS_MAX) { bias_tenths = CV_BIAS_TENTHS_MAX; }

    bias_apply((float)bias_tenths / 10.0f);

    printf("[proto] 偏压 → %.2f V  (0x14 = 0x%05X,模块级 %.3f V ×10)\n",
           (double)s_bias_volts,
           (unsigned)cv_bias_tenths_to_code(bias_tenths),
           (double)s_bias_volts / (double)CV_BIAS_HV_RATIO);
    return true;
}

bool DisplayProtocol_RequestPoint(uint32_t path, uint32_t bias_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    /* ⚠️ 这里**不**把顶栏写成请求的那一路 —— 顶栏显示硬件真值(见 RequestPath
     *    的说明)。本次点的是哪个回路,下面的 printf 会说清楚。 */
    (void)RelayView_Refresh(1);

    if (bias_tenths < CV_BIAS_TENTHS_MIN) { bias_tenths = CV_BIAS_TENTHS_MIN; }
    if (bias_tenths > CV_BIAS_TENTHS_MAX) { bias_tenths = CV_BIAS_TENTHS_MAX; }

    bias_apply((float)bias_tenths / 10.0f);

    /* 🆕 排一次采集。**不当场采** —— 偏压(尤其高压级)要时间稳定,
     *    交给 `Process()` 到点再采。见 `point_measure()` 的说明。
     *    🔴 **继电器也交给 `Process()` 到点时切**(2026-10-14):这里只记下"要哪路",
     *       切换最坏阻塞 ~400 ms,放回调里会让触摸卡住(见 `sweep_begin_leg()` 的说明)。 */
    if (path > 3u) {
        path = 0u;                          /* 越界收敛到 CISS,不崩 */
    }
    s_point_path      = path;
    s_point_volts_x10 = bias_tenths;
    s_point_all       = 0u;
    s_point_leg       = path;
    s_point_leg_n     = 1u;
    s_point_due_ms    = cv_millis() + s_dwell_ms;
    s_point_pending   = 1u;

    printf("[proto] 点测 %s @ %.2f V:偏压已下发,%u ms 后**切回路再采一次**(波形进结果页)\n",
           path_name(path), (double)s_bias_volts, (unsigned)s_dwell_ms);
    return true;
}

bool DisplayProtocol_RequestPointAll(uint32_t bias_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    if (bias_tenths < CV_BIAS_TENTHS_MIN) { bias_tenths = CV_BIAS_TENTHS_MIN; }
    if (bias_tenths > CV_BIAS_TENTHS_MAX) { bias_tenths = CV_BIAS_TENTHS_MAX; }

    bias_apply((float)bias_tenths / 10.0f);

    /* 🔴 **「全部」= 三条腿各切各的继电器、各采各的一次**(2026-10-14 改)。
     *    原文是"一份读数填三张卡" —— 那是在"继电器从不切换、三条回路
     *    在硬件上本就是同一个回路"的前提下成立的。现在继电器真的会切,
     *    这个前提没了,再共用一份读数就是**编数据**。
     *
     *    编排在 `Process()`:每条腿到点时先 `RelayView_ApplyPath(leg)` 再采,
     *    采完若还有下一腿就把 due 往后推一个驻留、`pending` 重新置起。 */
    s_point_path      = 0u;             /* all 模式下 path 不参与分支 */
    s_point_volts_x10 = bias_tenths;
    s_point_all       = 1u;
    s_point_leg       = 0u;
    s_point_leg_n     = 3u;
    s_point_due_ms    = cv_millis() + s_dwell_ms;
    s_point_pending   = 1u;

    printf("[proto] 点测 CISS+COSS+CRSS @ %.2f V:偏压已下发,"
           "%u ms 后开始**三条腿依次测**(每条腿各切继电器,各填各的卡)\n",
           (double)s_bias_volts, (unsigned)s_dwell_ms);
    return true;
}

bool DisplayProtocol_RequestSweep(uint32_t path, uint32_t stop_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    if (path >= 3u) {
        /* 扫压页把「全部」派发到 RequestRgThenSweepAll,走不到这里;
         * 真到了就是个越界参数,收敛到 CISS 而不是崩掉。 */
        path = 0u;
        (void)RelayView_Refresh(1);     /* 顶栏显示硬件真值,不写请求值 */
    }
    sweep_start(1u << path, stop_tenths);
    return true;
}

bool DisplayProtocol_RequestRgThenSweepAll(uint32_t stop_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    /* 「先测 RG,再三路扫压」—— 2026-10-14 起**真的先把 RG 测了**。
     * 原文是「界面不切继电器组合,跳过」,那是在写入权归 Python 的时期写的;
     * 现在 C 侧是唯一写入方,这条路切得过去。 */
    printf("[proto] 先测 RG:切 RG 回路 → 采一次 → 填电阻卡,然后跑三路扫压\n");

    if (!RelayView_ApplyPath(3u /* RG */)) {
        printf("[proto] ⚠️ RG 回路切换未确认 —— 下面的 RG 读数不可信。\n"
               "        核实:sudo -E python3 tools/relay_hold.py --watch\n");
    }
    s_pair_path = 3u;

    /* ⚠️ **当场采**(不等驻留):偏压此刻是用户当前设的那个,不是扫压的起点,
     *    而 RG 这一项本身就只是个"顺带测一下"。真要精确,RG 应当和别的
     *    测量项目一样走 `s_point_pending` 那条排程 —— 记进文档作为后续项,
     *    本轮不为此改结构(改完这个函数就要拆成两段状态机,收益不值)。 */
    if (adc_capture_pair() && s_pair.z_valid) {
        char rg_text[24];
        double rg = s_pair.z.z_re_ohm + 0.7;

        (void)snprintf(rg_text, sizeof(rg_text), "%.2f Ω", rg);
        UI_Demo_UpdateResistance(rg_text, 1u);
        printf("[proto] RG = Re Z + 0.7 = %.4f + 0.7 = %.4f Ω(R0 = %.0f Ω,挡位 0x%X)\n",
               s_pair.z.z_re_ohm, rg, s_pair.r0_ohm, (unsigned)s_pair.gear);
    } else {
        UI_Demo_UpdateResistance("--", 0u);
        printf("[proto] RG 读数拿不到(Z 未算出)—— 电阻卡留 --,扫压照跑\n");
    }

    sweep_start(0x07u, stop_tenths);
    return true;
}

bool DisplayProtocol_RequestSweepStop(void)
{
    s_cmd_total++;

    if (!s_sweep_active) {
        printf("[proto] 停止扫压:本来就没在扫\n");
        return true;
    }

    printf("[proto] 停止扫压:已下发 %u/%u 点,偏压停在 %.2f V\n",
           (unsigned)s_sweep_index, (unsigned)s_sweep_total,
           (double)s_bias_volts);
    s_sweep_active = 0u;
    /* 🔴 **「中止」与「跑完」同样要归零** —— 理由见 `sweep_finish()` 那段。
     *   原文这里写「偏压保持在当前点」并且照做了:中止后栅极顶在当时的电压上,
     *   跟"扫完了不置零"是同一个洞,只是走的是另一条出口。
     *   中止不需要保留工作点:没有"继续扫"这条路,要接着测只能重新发起,
     *   而重新发起会自己写第一点。 */
    (void)bias_apply(0.0f);
    UI_Demo_SetSweepInspectionReady();      /* 已经拿到的点仍可查看 */
    UI_Demo_SetStatusText("状态:扫压已停止(偏压已归 0)");
    return true;
}

bool DisplayProtocol_RequestSweepStatus(void)
{
    s_cmd_total++;

    /* 本层就是发起方,状态在本地内存里 —— 直接答,不用往返。
     * ⚠️ 界面没调用过它(ui_demo.c 里 grep 无命中),保留是为了把
     *    display_protocol.h 实现完整。 */
    printf("[proto] 扫压状态:%s(%u/%u)\n",
           s_sweep_active ? "进行中" : "空闲",
           (unsigned)s_sweep_index, (unsigned)s_sweep_total);
    return true;
}

/* ==========================================================================
 * 统计上报
 * ========================================================================== */

/* 这一层已经没有"帧"了 —— 没有 UART、没有 CRC、没有丢包。
 * 报 0 是实话,不是"忘了填"。 */
uint32_t DisplayProtocol_GetReceivedLineCount(void)  { return 0; }
uint32_t DisplayProtocol_GetCrcErrorCount(void)      { return 0; }
uint32_t DisplayProtocol_GetTransmitDropCount(void)  { return 0; }

/* 排障用:main.c 退出时打印一行。
 * ⚠️ 名字沿用旧符号(前序接口),但**语义变了** —— 以前是"被拒的指令数",
 *    现在是"要读数、但读数还没接通的指令数"。改 main.c 的打印标签对齐。 */
uint32_t DisplayProtocol_GetRefusedCount(void)
{
    return s_readback_pending;
}

uint32_t DisplayProtocol_GetAdcCaptureCount(void)    { return s_adc_points; }
uint32_t DisplayProtocol_GetAdcFailCount(void)       { return s_adc_fail; }
uint32_t DisplayProtocol_GetBiasWriteCount(void)     { return s_bias_writes; }
uint32_t DisplayProtocol_GetBiasVerifyFailCount(void) { return s_bias_verify_fail; }
uint32_t DisplayProtocol_GetCommandCount(void)       { return s_cmd_total; }
uint32_t DisplayProtocol_GetPathCommandCount(void)   { return s_path_cmds; }
