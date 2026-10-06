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
 *          DLIA / CORDIC / 继电器在 PL 里都还没有。建 IPC 只增加两进程
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
 * 【还没有硬件、因此只在界面上生效的】
 *   · 回路切换(「当前选择:CISS/COSS/CRSS/RG」)—— **本位流里没有
 *     `relay_ctrl`**,`cv_regs.py` 的寄存器表里也没有继电器寄存器。
 *     本文件只把它回显到界面,并照实打印一行。
 * 🆕 2026-10-06:**Cs 读数有了,但不是 DLIA 算的。** 走的是
 *    `adc_sample.c` —— 采 AD9238 缓冲(0x8C/0x90/0x94/0x98)+ C 侧正交解调。
 *    结果页的「单点波形 / 扫压波形」两张图和扫压曲线上的 Cs 都由此来。
 *    ⚠️ **DLIA(`dlia_core` / `cordic_polar`)仍然没做** —— 这是 P3 之前的
 *    替代路径,不是终点。差别在于:软件解调在 PS 上跑,受 `cv_millis()`
 *    调度和 AXI 事务开销限制;**PL 侧 DLIA 才是评分对象**(见 CLAUDE.md §二)。
 *
 * ⚠️ **Cs 只是"能出数",不是"测得准"**:
 *    · `R_tia` 是原理图标称值,**未标定**;
 *    · 两路(INA/INB 两个时间槽)是**两次独立采集**,相位不相关 ⇒ 只能比幅值,
 *      **Cp 与 D 一律显示 `--`**(算出来是噪声,不如不显示);
 *    · 模拟输入现在**没接**,采到的可能是贴顶的平线 —— 那是**真数据**,
 *      不是画错了。判据:`dump_stats` 的 `ADC 采集 成功/失败` 计数。
 *
 * 🔴 **状态行有字数上限,写长了会被截掉右半边。**
 *    `g_header_operation_label`(`ui_demo.c:2565`)在 x=704、宽
 *    `LCD_PIXEL_WIDTH − 714` = **310 px**、**右对齐**、`ui_font_cn_20`。
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 激励频率与采样率**不在这里定义** —— 归 `measure_config.h` / `pynq_regs.h`。
 * 本文件只负责"什么时候下发",不负责"下发什么值"。 */

/* 扫压页要用的点数与驻留见上;激励频率的默认值 = `CV_DDS_HZ_DEFAULT`(1 MHz,决策 D08)。 */

/* ==========================================================================
 * 状态
 * ========================================================================== */

static int      s_demo_enabled;         /* CV_UI_DEMO=1:叠加模型读数 */
static uint32_t s_dwell_ms;

static uint32_t s_sweep_active;
static uint32_t s_sweep_mask;           /* bit0..2 = CISS/COSS/CRSS */
static float    s_sweep_stop_volts;
static uint32_t s_sweep_total;
static uint32_t s_sweep_index;          /* 已下发的点数(1 起) */
static uint32_t s_sweep_next_ms;
static float    s_bias_volts;           /* 最后一次真正下发的偏压(量化后的真值) */

static uint32_t s_demo_auto_page;       /* CV_UI_DEMO_PAGE:跑完切到这一页 */
static uint32_t s_demo_auto_stage;

/* 统计(排障用;不在 display_protocol.h 里 —— 那是前序的接口) */
static uint32_t s_cmd_total;            /* 收到的指令总数             */
static uint32_t s_readback_pending;     /* 要读数、但读不到的指令数   */
static uint32_t s_path_cmds;            /* 回路切换指令数(无继电器)  */
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
 *   ⛔ 一次 GO 只锁一个刻度 ⇒ 两路靠**两次采集**,不需要改 RTL、不需要重下位流。
 *
 * 🔴 **两路的相位不相关** —— 两次采集的 GO 时刻任意,所以 A、B 之间差一个
 *    **随机相位**。⇒ 只能比**幅值**,不能算阻抗相位。因此 Cs 按"纯电容"算
 *    (`AdcSample_CsPf` 的说明),**不出 Cp / D** —— 那两个要相位,现在算出来是噪声。
 *    要真正同时采,得给 `adc_capture` 加第二组缓冲 + 重综合 + 重下位流,见
 *    `docs/display_subsystem.md` 的后续项。
 * ========================================================================== */

/** 单点测量:bias_apply 之后等多久再采。复用扫压的驻留时间 —— 那是本项目
 *  对"高压级 + 被测件稳定下来要多久"的唯一口径,不另立一个数。 */
#define POINT_SETTLE_MS_DEF   300u

typedef struct
{
    float mag_a_v;      /* 通道 A(port A = INA 时间槽)解调幅值,伏 */
    float mag_b_v;      /* 通道 B(port B = INB 时间槽)解调幅值,伏 */
    float cs_pf;        /* 幅值比算出的 Cs,pF —— **未标定** */
    float cur_ma;       /* 通道 B 折算成电流,mA */
    float pp_a;         /* 通道 A 峰峰值,ADC 码(排障用:判断输入有没有信号) */
    float pp_b;         /* 通道 B 峰峰值,ADC 码 */
    uint32_t otr;       /* 两路 OTR 置起的点数合计 */
} AdcPair;

static uint32_t s_code_a[ADC_SAMPLE_POINTS];
static uint32_t s_code_b[ADC_SAMPLE_POINTS];
static AdcPair  s_pair;

static uint32_t s_adc_points;       /* 一轮总共采了多少次(统计) */
static uint32_t s_adc_fail;         /* 采集/解调失败次数       */
static int      s_adc_notice;       /* 失败提示只打一次        */
static int      s_cpd_notice;       /* "Cp/D 为什么是 --" 只解释一次 */

/* 单点测量:请求 → 等偏压稳定 → 采 → 报结果 */
static uint32_t s_point_pending;
static uint32_t s_point_due_ms;
static uint32_t s_point_path;
static uint32_t s_point_volts_x10;
static uint32_t s_point_all;        /* 1 = 「CISS+COSS+CRSS」,三条卡片一起填 */

/* ==========================================================================
 * 小工具
 * ========================================================================== */

static const char *path_name(uint32_t path)
{
    static const char *const names[] = {"CISS", "COSS", "CRSS", "RG"};

    return (path < 4u) ? names[path] : "?";
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

/** @brief 采一对(通道 A = port A/INA 槽 / 通道 B = port B/INB 槽)并解调。结果进 `s_pair` / `s_code_*`。 */
static bool adc_capture_pair(void)
{
    uint32_t div    = MeasureConfig_GetSampleDiv();
    uint32_t fs_hz  = cv_adc_sample_hz(div);
    uint32_t fe_hz  = MeasureConfig_GetDdsHz();
    /* 🔴 **按"时间槽窗口中心"取刻度,不是按上升沿/下降沿。** 那 12 位总线是
     *    双端口时分复用:port A = INA 窗 `[2−P/2,+1]`(中心 `1.5−P/4`)、
     *    port B = INB 窗 `[+2,P/2+1]`(中心 `1.5+P/4`)。用户口径说的
     *    「上升沿=通道 a、下降沿=通道 b」**正好是反的**(tick −P/2 落在 B 窗、
     *    tick 0 落在 A 窗),而且都贴窗沿。见 `adc_sample.h` 文件头与 `LLM-045` §G。 */
    int32_t  tick_a = AdcSample_PortATick(div);      /* 通道 A = INA */
    int32_t  tick_b = AdcSample_PortBTick(div);      /* 通道 B = INB */
    float    ia, qa, ib, qb;
    float    mag_a_codes, mag_b_codes;
    uint32_t otr_a = 0u;
    uint32_t otr_b = 0u;
    uint32_t i;
    uint32_t min_a, max_a, min_b, max_b;

    /* ⛔ 先查刻度是否可达。落在寄存器合法范围外的刻度**永不命中** ——
     *    现象是 `done` 不置位(超时),很容易被误判成"位流不对"。
     *    这里提前给出"换哪个档位"这种可执行的提示。 */
    if (!AdcSample_TickLegal(div, tick_a) || !AdcSample_TickLegal(div, tick_b)) {
        AdcSample_SetErrorf("DIV=%u 下窗口中心越出寄存器合法范围"
                            "(A tick %+d / B tick %+d)。该档位下某一槽采不到,"
                            "换更大的 DIV(1/2/5/10/13 MSPS 里挑)。",
                            (unsigned)div, (int)tick_a, (int)tick_b);
        return false;
    }

    if (!AdcSample_Capture(tick_a, ADC_SAMPLE_POINTS, s_code_a, NULL, &otr_a)) {
        return false;
    }
    if (!AdcSample_Capture(tick_b, ADC_SAMPLE_POINTS, s_code_b, NULL, &otr_b)) {
        return false;
    }

    /* 🔴 **幅值必须一路一路算。** 两路是两次独立采集,GO 时刻任意 ⇒ 它们之间的
     *    相位差是**随机的**。所以这里不能做真双通道那种 V/I 复数除法,
     *    只能各算各的幅值。要同时采得给 `adc_capture` 加第二组缓冲(后续项)。 */
    if (!AdcSample_Demodulate(s_code_a, ADC_SAMPLE_POINTS, fs_hz, fe_hz,
                              &ia, &qa, &mag_a_codes)) {
        return false;
    }
    if (!AdcSample_Demodulate(s_code_b, ADC_SAMPLE_POINTS, fs_hz, fe_hz,
                              &ib, &qb, &mag_b_codes)) {
        return false;
    }
    (void)ia; (void)qa; (void)ib; (void)qb;   /* I/Q 本身当前没有用户;相位不可用 */

    /* 峰峰值只进日志 —— 它是"输入到底有没有信号"最快的判据 */
    min_a = max_a = s_code_a[0];
    min_b = max_b = s_code_b[0];
    for (i = 1u; i < ADC_SAMPLE_POINTS; ++i) {
        if (s_code_a[i] < min_a) { min_a = s_code_a[i]; }
        if (s_code_a[i] > max_a) { max_a = s_code_a[i]; }
        if (s_code_b[i] < min_b) { min_b = s_code_b[i]; }
        if (s_code_b[i] > max_b) { max_b = s_code_b[i]; }
    }

    s_pair.mag_a_v = AdcSample_CodesToVolts(mag_a_codes);
    s_pair.mag_b_v = AdcSample_CodesToVolts(mag_b_codes);
    s_pair.cs_pf   = AdcSample_CsPf(s_pair.mag_a_v, s_pair.mag_b_v, fe_hz);
    s_pair.cur_ma  = (s_pair.mag_b_v / ADC_SAMPLE_R_TIA) * 1000.0f;
    s_pair.pp_a    = (float)(max_a - min_a);
    s_pair.pp_b    = (float)(max_b - min_b);
    s_pair.otr     = otr_a + otr_b;

    s_adc_points++;
    return true;
}

/** @brief 一行采集证据。扫压时每点一行 —— 出问题时第一眼看的就是它。 */
static void adc_log_pair(const char *tag, float bias_v)
{
    uint32_t div = MeasureConfig_GetSampleDiv();

    printf("[adc] %-8s 偏压 %7.3f V | A/INA tick%+d 幅值 %7.4f V,峰峰 %5.0f 码 | "
           "B/INB tick%+d 幅值 %7.4f V,峰峰 %5.0f 码 | Cs %9.3f pF(未标定) | OTR %u\n",
           tag, (double)bias_v,
           (int)AdcSample_PortATick(div), (double)s_pair.mag_a_v, (double)s_pair.pp_a,
           (int)AdcSample_PortBTick(div), (double)s_pair.mag_b_v, (double)s_pair.pp_b,
           (double)s_pair.cs_pf, (unsigned)s_pair.otr);
}

/**
 * @brief 把 `s_code_a` / `s_code_b` 灌进「单点波形」页(kind 0)。
 *
 * 用户口径:通道 A → 电压图,通道 B → 电流图(A = INA 槽,B = INB 槽)。
 * ⚠️ 通道 B 本身是跨阻放大器的**输出电压**,这里按 `ADC_SAMPLE_R_TIA` 折算成电流。
 */
static void adc_feed_point_waveform(void)
{
    uint32_t i;

    UI_Demo_BeginWaveform(UI_DEMO_WAVE_POINT, ADC_SAMPLE_POINTS);
    for (i = 0u; i < ADC_SAMPLE_POINTS; ++i) {
        float v = AdcSample_CodesToVolts((float)s_code_a[i]);
        float m = (AdcSample_CodesToVolts((float)s_code_b[i]) /
                   ADC_SAMPLE_R_TIA) * 1000.0f;      /* A → mA */

        UI_Demo_UpdateWaveformPoint(UI_DEMO_WAVE_POINT, i + 1u,
                                    ADC_SAMPLE_POINTS, v, m, NULL);
    }
}

/**
 * @brief 单点测量:等偏压稳定 → 采 → 喂波形 + 填读数卡片。
 *
 * 🔴 **Cs / Cp / D 三项的填法**(这是本函数唯一需要判断的地方):
 *    · `Cs` 是真值 —— 由两路**幅值比**算出(见 `AdcSample_CsPf`)。未标定。
 *    · `Cp` / `D` 一律填 `--`。两者都需要 V 与 I 的**相位差**,而两路是分两次
 *      采的、相位不相关 ⇒ 算出来是噪声。**填上去等于编数据**,宁可显示占位符。
 */
static void point_measure(void)
{
    char     cs_text[16];
    char     cp_text[16];
    char     d_text[16];
    uint32_t path  = s_point_path;
    float    volts = (float)s_point_volts_x10 / 10.0f;
    uint32_t valid = 0u;

    if (adc_capture_pair()) {
        valid = 1u;
        adc_feed_point_waveform();
        (void)snprintf(cs_text, sizeof(cs_text), "%.3f", (double)s_pair.cs_pf);
        adc_log_pair("单点", volts);
    } else {
        adc_report_failure("单点测量");
        (void)snprintf(cs_text, sizeof(cs_text), "--");
    }
    (void)snprintf(cp_text, sizeof(cp_text), "--");
    (void)snprintf(d_text,  sizeof(d_text),  "--");

    /* 卡片上 Cp/D 是空的,屏幕上不解释原因 —— 这里补一次说明,免得以为是漏填。 */
    if (!s_cpd_notice) {
        s_cpd_notice = 1;
        printf("[proto] Cp / D 卡片显示 `--` 是**故意的**:两者都要 V 与 I 的相位差,\n"
               "        而 INA/INB 两个时间槽是**两次独立采集**、相位不相关 ⇒ 只能用幅值比\n"
               "        出 Cs。要填上 Cp/D 得让 PL 同时采两路(给 adc_capture 加第二组\n"
               "        缓冲 + 重下位流),本轮不做。见 adc_sample.h 文件头。\n");
    }

    if (s_point_all) {
        /* 「一键测量页」:三条回路共用同一份读数 —— 没有继电器,物理上就是同一个回路 */
        uint32_t p;

        for (p = 0u; p < 3u; ++p) {
            UI_Demo_UpdateAllCapacitance(p, cs_text, cp_text, d_text, valid);
        }
    } else if (path < 3u) {
        UI_Demo_UpdateSingleCapacitance(path, cs_text, cp_text, d_text, valid);
    } else {
        /* RG 回路要继电器切到栅极电阻那条路,本位流里没有 `relay_ctrl`
         * ⇒ **没有可测的量**。卡片照实留 `--`,不编一个电阻值出来。 */
        printf("[proto] 点测 RG:继电器未实现,无读数可填(卡片保持 --)\n");
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
    uint32_t path;
    uint32_t valid;

    (void)snprintf(bias_text, sizeof(bias_text), "%.2f", (double)volts);
    (void)snprintf(cap_text,  sizeof(cap_text),  "--");

    valid = adc_capture_pair() ? 1u : 0u;
    if (valid) {
        (void)snprintf(cap_text, sizeof(cap_text), "%.3f", (double)s_pair.cs_pf);
        adc_log_pair("扫压", volts);
    } else {
        adc_report_failure("扫压");
    }

    for (path = 0u; path < 3u; ++path) {
        if ((s_sweep_mask & (1u << path)) == 0u) {
            continue;
        }
        /* 🔴 `model_values_valid = 0` —— 告诉界面"Cp/D 不是模型值、也不是真值,
         *    别拿去画"。Cs 是真的(幅值比),`valid` 由它是否 > 0 决定
         *    (`UI_Demo_UpdateSweepPoint` 内部会判),所以曲线照画。 */
        UI_Demo_UpdateSweepPoint(path, index, s_sweep_total, volts,
                                 valid ? s_pair.cs_pf : 0.0f,
                                 0.0f, 0.0f, 0u,
                                 bias_text, cap_text, valid);
    }

    /* 「扫压波形」页(kind 1)按既有设计是**每步一个点**的电压/电流包络,
     * 不是 250 点原始波形(那是「单点波形」页的形态)。 */
    UI_Demo_UpdateWaveformPoint(UI_DEMO_WAVE_SWEEP, index, s_sweep_total,
                                volts, valid ? s_pair.cur_ma : 0.0f, bias_text);

    UI_Demo_RefreshResultViews();
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

    UI_Demo_ClearSweep(3u);                 /* 3 ≥ 3 = 清全部三路 */
    UI_Demo_BeginSweep(first, s_sweep_total, (mask == 0x07u) ? 1u : 0u);
    /* 🆕 「扫压波形」页要开一张新图 —— `UI_Demo_BeginSweep()` 只管扫压曲线,
     *    不碰 `g_result_wave[]`(那是结果页那两页的缓存)。不调这句的话,
     *    上一轮的点会留在图上,新曲线从中间接上去。 */
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_SWEEP, s_sweep_total);

    printf("[proto] 扫压开始:%s,%.2f V → %.2f V,%u 点,每点 %u ms\n"
           "        Cs 取**真实 ADC 数据**的幅值比(未标定);Cp/D 无相位,不填\n",
           (mask == 0x07u) ? "CISS+COSS+CRSS" : path_name(first),
           (double)SWEEP_START_VOLTS, (double)stop_volts,
           (unsigned)s_sweep_total, (unsigned)s_dwell_ms);
}

static void sweep_finish(void)
{
    s_sweep_active = 0u;
    UI_Demo_SetSweepInspectionReady();
    UI_Demo_SetStatusText(s_demo_enabled ?
        "状态:扫压结束(演示读数)" :
        "状态:扫压结束(Cs 未标定)");
    printf("[proto] 扫压结束:%u 点已下发到 DAC\n", (unsigned)s_sweep_index);
}

/** @brief 下发一个扫压点。由 `DisplayProtocol_Process()` 按驻留时间调。 */
static void sweep_step(void)
{
    uint32_t path;
    float    voltage;
    char     bias_text[16];
    char     cap_text[16];

    /* ① ★ 先收尾**上一个**点:它的偏压已经稳定了**整整一个驻留时间**。
     *    ⛔ 顺序不能倒 —— 先写偏压再立刻采,采到的是还没稳定下来的值,
     *      曲线会在每个点的跳变处失真,而且**不报错**,极难定位。
     *    (演示模式下读数由模型给,不采。) */
    if ((s_sweep_index > 0u) && !s_demo_enabled) {
        sweep_measure_point(s_sweep_index);
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

    for (path = 0u; path < 3u; ++path) {
        float    cs = 0.0f;
        float    cp = 0.0f;
        float    d  = 0.0f;
        uint32_t model_valid = 0u;
        uint32_t valid       = 0u;
        const char *cap = "--";

        if ((s_sweep_mask & (1u << path)) == 0u) {
            continue;
        }

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
           这就是"偏压真走了、电容还没测"的正确形态。 */
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
    s_adc_points        = 0u;
    s_adc_fail          = 0u;
    s_adc_notice        = 0;
    s_cpd_notice        = 0;
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

    env = getenv("CV_UI_DEMO_PAGE");
    s_demo_auto_page = ((env != NULL) && (env[0] >= '0') && (env[0] <= '9')) ?
                       (uint32_t)(env[0] - '0') : 0u;

    /* ⚠️ 这两句必须在 UI_Demo_Init() 之后跑 —— 它们会动 LVGL 标签。
     *    调用顺序由 main.c:361→364 保证。 */

    /* ---- 测量参数:激励 1 MHz + 采样 10 MSPS + 相位到窗中心,一次下发 ----
     * 🔄 2026-10-06:原来这里只写 0x80(激励)。现在整组参数归 `MeasureConfig`,
     *    它同时会刷「系统复位」页那一行常驻状态 —— 本页此刻**还没构建**
     *    (首次进入才建),那次刷新会被静默丢弃,页面构建时会自己补拉,
     *    见 `ui_demo.c::UI_RefreshInitStatusLine()`。 */
    MeasureConfig_Init();

    /* ---- 偏压:0 V(中位码)---- */
    bias_apply(0.0f);

    UI_Demo_SetConnection(1u);              /* 寄存器通了,后端算"在线" */
    UI_Demo_SetActualPath("CISS");

    if (s_demo_enabled) {
        UI_Demo_SetStatusText("状态:演示模式(偏压真下发,读数由模型生成)");
        printf("[proto] ⚠️  演示模式已开启(CV_UI_DEMO=1)\n"
               "        偏压/扫压**是真的**在驱动 DAC;\n"
               "        但 Cs/Cp/D 曲线是**模型生成的假数据**,只用于检查界面。\n"
               "        ⛔ 不得作为任何测量结论的依据。\n"
               "        关闭:不带 CV_UI_DEMO 重启。\n");
    } else {
        UI_Demo_SetStatusText("状态:就绪(Cs 取自 ADC,未标定)");
    }

    /* ⚠️ 激励那一项**从 MeasureConfig 读回**,不照抄常量 ——
     *    照抄的话哪天默认值改了,这一行会替一个没生效的值作证。 */
    printf("[proto] 已接管测量指令(C 侧直写寄存器,方案 A)\n"
           "        激励 = %u Hz(0x80 = 0x%05X);采样率/相位明细见上面 [mc] 那几行\n"
           "        偏压 = 0.0 V(0x14 = 0x%05X)\n"
           "        扫压驻留 = %u ms(可用 CV_SWEEP_DWELL_MS 覆盖)\n"
           "        ⚠️ 回路切换仍无硬件(本位流里没有 relay_ctrl),只回显到界面;\n"
           "           Cs 走真实 ADC 采样(adc_sample.c),未标定,Cp/D 因无相位不填\n",
           (unsigned)MeasureConfig_GetDdsHz(),
           (unsigned)cv_dds_hz_to_code(MeasureConfig_GetDdsHz()),
           (unsigned)cv_bias_tenths_to_code(0u),
           (unsigned)s_dwell_ms);
}

void DisplayProtocol_Process(void)
{
    demo_auto_run();

    /* ---- 单点测量:偏压稳定了再采 ----
     * 🔴 必须挂在下面那句 `if (!s_sweep_active) return;` **之前** ——
     *    非扫压期间才是单点测量的主场景,放到后面等于永远不执行。 */
    if (s_point_pending && ((int32_t)(cv_millis() - s_point_due_ms) >= 0)) {
        s_point_pending = 0u;
        point_measure();
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
 *      `0x14` 偏压      → 0 V(码 0x80000)
 *      `0x80` 激励      → 1 MHz
 *      `0x88` 采样率    → DIV=13 → 10 MSPS
 *      `0x88` 相位      → 该分频比的窗口中心(−2)
 *      扫压状态 / 曲线  → 清空
 *      常驻状态行       → 刷新(由 MeasureConfig 自己推)
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

bool DisplayProtocol_RequestPath(uint32_t path)
{
    s_cmd_total++;
    s_path_cmds++;

    if (path < 4u) {
        UI_Demo_SetActualPath(path_name(path));
    }

    /* 🔴 本位流里**没有 relay_ctrl**,寄存器表里也没有继电器寄存器
     *    (见 cv_regs.py)——所以这里没有硬件可写,只有界面状态。
     *    照实打印,别让日志看起来像"切了回路"。 */
    printf("[proto] 回路 → %s(仅界面状态;继电器模块不在本位流)\n",
           path_name(path));
    return true;
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

    if (path < 4u) {
        UI_Demo_SetActualPath(path_name(path));
    }
    if (bias_tenths < CV_BIAS_TENTHS_MIN) { bias_tenths = CV_BIAS_TENTHS_MIN; }
    if (bias_tenths > CV_BIAS_TENTHS_MAX) { bias_tenths = CV_BIAS_TENTHS_MAX; }

    bias_apply((float)bias_tenths / 10.0f);

    /* 🆕 排一次采集。**不当场采** —— 偏压(尤其高压级)要时间稳定,
     *    交给 `Process()` 到点再采。见 `point_measure()` 的说明。 */
    s_point_path      = path;
    s_point_volts_x10 = bias_tenths;
    s_point_all       = 0u;
    s_point_due_ms    = cv_millis() + s_dwell_ms;
    s_point_pending   = 1u;

    printf("[proto] 点测 %s @ %.2f V:偏压已下发,%u ms 后采一次(波形进结果页)\n",
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

    s_point_path      = 0u;             /* 三条一起填,path 不参与分支 */
    s_point_volts_x10 = bias_tenths;
    s_point_all       = 1u;
    s_point_due_ms    = cv_millis() + s_dwell_ms;
    s_point_pending   = 1u;

    /* 三条回路共用同一个偏压 —— 没有继电器,挨着写三次同一个码没有意义 */
    printf("[proto] 点测 CISS+COSS+CRSS @ %.2f V:偏压已下发,"
           "%u ms 后采一次(三条卡片共用同一份真实读数)\n",
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
        UI_Demo_SetActualPath(path_name(path));
    }
    sweep_start(1u << path, stop_tenths);
    return true;
}

bool DisplayProtocol_RequestRgThenSweepAll(uint32_t stop_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    /* 「先测 RG,再三路扫压」—— RG 要用继电器切到栅极电阻那条路,本位流没有。
     * 照实说,然后照常把三路扫压跑起来。 */
    printf("[proto] 先测 RG:继电器未实现,跳过;**只跑三路扫压**\n");
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
    UI_Demo_SetSweepInspectionReady();      /* 已经拿到的点仍可查看 */
    UI_Demo_SetStatusText("状态:扫压已停止(偏压保持在当前点)");
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
