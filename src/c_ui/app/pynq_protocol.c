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
 *   · 「发送复位指令」    → 停扫压 + 偏压归 0 V + 激励回 1 MHz + 清界面
 *   · 上电               → 激励固定 1 MHz(写 0x80),偏压 0 V(写 0x14)
 *
 * 【还没有硬件、因此只在界面上生效的】
 *   · 回路切换(「当前选择:CISS/COSS/CRSS/RG」)—— **本位流里没有
 *     `relay_ctrl`**,`cv_regs.py` 的寄存器表里也没有继电器寄存器。
 *     本文件只把它回显到界面,并照实打印一行。
 *   · **Cs / Cp / D 读数** —— DLIA(`dlia_core` / `cordic_polar`)还没做,
 *     没有任何东西能测出电容。所以扫压曲线**是空的**,进度条和偏压数字
 *     是真的。这不是 bug,是 P3 之前的正确形态。
 *
 * ⚠️ 界面上「开始测量」这类按钮点下去**仍会显示成功**:偏压确实动了。
 *    但**读数不会回来** —— 别把"状态栏没报错"当成"测到了"。
 *    判据看屏幕上的 Cs 是不是 `--`,或退出时 `dump_stats` 那一行。
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

#define DDS_DEFAULT_HZ         1000000u     /* 决策 D08:1 MHz 固定 */
#define DDS_HZ_MIN             1000u
#define DDS_HZ_MAX             12500000u    /* AD9833 数据手册:输出上限 MCLK/2 */

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
 */
static void bias_apply(float volts)
{
    (void)bias_write_code(cv_bias_volts_to_code(volts));
    bias_announce(s_bias_volts);
}

/**
 * @brief 设激励频率。
 *
 * 上电只调一次(1 MHz)。界面上**没有频率控件** —— 前序 `ui_demo.c` 里
 * 一个都没有,加一个就要改它,而"和前序逐字对照"是报告的可辩护性资产。
 * 所以频率只从这一层和 `0x80` 走。
 */
static void dds_set_hz(uint32_t hz)
{
    if (hz < DDS_HZ_MIN) { hz = DDS_HZ_MIN; }
    if (hz > DDS_HZ_MAX) { hz = DDS_HZ_MAX; }
    cv_reg_write(CV_REG_DDS_FREQ, cv_dds_hz_to_code(hz));
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

    printf("[proto] 扫压开始:%s,%.2f V → %.2f V,%u 点,每点 %u ms\n"
           "        ⚠️ 只有偏压真的在走;Cs 曲线为空(待 DLIA)\n",
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
        "状态:扫压结束(偏压已到位,Cs 未测)");
    printf("[proto] 扫压结束:%u 点已下发到 DAC\n", (unsigned)s_sweep_index);
}

/** @brief 下发一个扫压点。由 `DisplayProtocol_Process()` 按驻留时间调。 */
static void sweep_step(void)
{
    uint32_t path;
    float    voltage;
    char     bias_text[16];
    char     cap_text[16];

    /* ⚠️ 先判满再自增,顺序不能倒 —— 倒了的话 `s_sweep_index` 会在结束时
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

    /* ---- 激励:固定 1 MHz(决策 D08) ---- */
    dds_set_hz(DDS_DEFAULT_HZ);
    (void)cv_reg_read(CV_REG_DDS_FREQ);     /* 读一下,让上面那次写落到总线上 */

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
        UI_Demo_SetStatusText("状态:就绪(偏压/激励已接 PL,Cs 待测量链)");
    }

    printf("[proto] 已接管测量指令(C 侧直写寄存器,方案 A)\n"
           "        激励 = %u Hz(0x80 = 0x%05X)\n"
           "        偏压 = 0.0 V(0x14 = 0x%05X)\n"
           "        扫压驻留 = %u ms(可用 CV_SWEEP_DWELL_MS 覆盖)\n"
           "        ⚠️ 回路切换 / Cs 读数尚无硬件,只回显到界面\n",
           (unsigned)DDS_DEFAULT_HZ,
           (unsigned)cv_dds_hz_to_code(DDS_DEFAULT_HZ),
           (unsigned)cv_bias_tenths_to_code(0u),
           (unsigned)s_dwell_ms);
}

void DisplayProtocol_Process(void)
{
    demo_auto_run();

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

bool DisplayProtocol_RequestReset(void)
{
    s_cmd_total++;
    s_sweep_active = 0u;
    s_sweep_total  = 0u;
    s_sweep_index  = 0u;

    bias_apply(0.0f);                       /* 偏压归 0 V */
    dds_set_hz(DDS_DEFAULT_HZ);             /* 激励回 1 MHz */

    UI_Demo_ClearSweep(3u);
    UI_Demo_ClearAllResults();
    UI_Demo_SetStatusText("状态:已复位(偏压 0 V,激励 1 MHz)");

    printf("[proto] 复位:0x14 ← 0x%05X(0.0 V),0x80 ← 0x%05X(1 MHz)\n",
           (unsigned)cv_bias_tenths_to_code(0u),
           (unsigned)cv_dds_hz_to_code(DDS_DEFAULT_HZ));
    return true;
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

    printf("[proto] 点测 %s @ %.2f V:偏压已下发,**未取读数**(DLIA 未实现)\n",
           path_name(path), (double)s_bias_volts);
    return true;
}

bool DisplayProtocol_RequestPointAll(uint32_t bias_tenths)
{
    s_cmd_total++;
    s_readback_pending++;

    if (bias_tenths < CV_BIAS_TENTHS_MIN) { bias_tenths = CV_BIAS_TENTHS_MIN; }
    if (bias_tenths > CV_BIAS_TENTHS_MAX) { bias_tenths = CV_BIAS_TENTHS_MAX; }

    bias_apply((float)bias_tenths / 10.0f);

    /* 三条回路共用同一个偏压 —— 没有继电器,挨着写三次同一个码没有意义 */
    printf("[proto] 点测 CISS+COSS+CRSS @ %.2f V:偏压已下发,"
           "**未取读数**(DLIA / 继电器均未实现)\n", (double)s_bias_volts);
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

uint32_t DisplayProtocol_GetBiasWriteCount(void)     { return s_bias_writes; }
uint32_t DisplayProtocol_GetBiasVerifyFailCount(void) { return s_bias_verify_fail; }
uint32_t DisplayProtocol_GetCommandCount(void)       { return s_cmd_total; }
uint32_t DisplayProtocol_GetPathCommandCount(void)   { return s_path_cmds; }
