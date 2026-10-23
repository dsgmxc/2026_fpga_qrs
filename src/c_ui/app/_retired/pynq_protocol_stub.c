/**
 * @file pynq_protocol_stub.c
 * @brief `display_protocol.h` 的**空实现**(本轮不接测量链)
 *
 * 接口在 `ui/display_protocol.h`(**前序的,一行没改**),实现是新的。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【前序这一层是干什么的】
 *   它是 UI ↔ 测量后端之间的**协议层**:前序是 UART 上跑一套带 CRC 的帧,
 *   `Request*` 往外发指令,`Process()` 收回来更新界面。
 *   本项目的架构里,**它对应的是 UI 进程 ↔ Python/PS 的那条边**
 *   (`drivers/ui_process.py` 那一侧),但那一侧本轮**还没做**。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 🔴 【`Request*` 一律返回 `false`,不是 `true`】
 *
 *   看 `ui_demo.c` 怎么用返回值(例:`ui_demo.c:2739`):
 *     `command_queued = DisplayProtocol_RequestPath(...);`
 *   然后:
 *     · `true`  ⇒ 状态栏显示「指令发送完成」;
 *                 `RequestSweep` 那条路还会顺手清 `g_sweep_inspection_ready`
 *                 并切到扫压工程页 —— 即**假装一次测量已经开始**。
 *     · `false` ⇒ 状态栏显示「指令发送失败」(例:`ui_demo.c` 的
 *        `UI_SimpleActionEvent` 末尾)。
 *
 *   ⇒ 返回 `true` 是**对用户撒谎**:屏幕上会出现"指令发送完成"、
 *     扫压页会摆出"正在测量"的样子,而底下**什么都没发生**。
 *     返回 `false` 则如实说出"这条指令没发出去",用户一眼就知道
 *     测量链还没接 —— 这正是一期该有的样子。
 *
 *   ⚠️ 副作用要提前知道:本轮**每个测量按钮**点下去都会显示
 *      「指令发送失败」。这**不代表触屏坏了** —— 触屏好不好,
 *      看 `tools/touch_probe.py`,别拿这个状态栏做判据。
 *      (页面之间的**导航**不走这一层,是纯 LVGL,不受影响。)
 *
 *   ⚠️ 上报的 0 是真话:没收到过任何一行,CRC 错 0 次,丢包 0 次 ——
 *      不是"忘了填",是确实没有这条通路。
 */

#include "display_protocol.h"
#include "ui_demo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 演示数据发生器(默认关闭)
 *
 *   打开:`CV_UI_DEMO=1` 启动 c_ui
 *
 *   用途 —— **只用来在测量链接通之前,把界面喂上假数据以检查布局**:
 *     · `Request*` 改为返回 `true`,并按"一次测量该花多久"延迟若干拍
 *     · 延迟到点后,按一条**单调下降的 MOSFET 寄生电容模型**生成读数,
 *       经 `UI_Demo_Update*` 写进界面(与将来 Python 侧要走的是同一组入口)
 *     · 扫压请求会逐点推进,把 C-V 曲线真的画出来
 *
 *   ⛔ **这里的数字是编的,不得作为任何测量结论的依据。**
 *      它只回答"界面画得对不对",不回答"测出来是多少"。
 *      接上 `ui_process.py` 之后本模块连同 `CV_UI_DEMO` 一起删除。
 * ========================================================================== */

#define DEMO_LATENCY_TICKS   30u   /* 模拟一次点测的耗时(约半秒) */
#define DEMO_SWEEP_POINTS    60u   /* 扫压点数 */
#define DEMO_POINT_SAMPLES   240u   /* 单点测量过程的波形采样点数 */
#define DEMO_SWEEP_V_MAX     100.0f /* 扫压终止偏压(V):与扫压页 X 轴量程
                                       (0.1 V – 100 V)对齐,曲线才能占满全宽 */

typedef enum
{
    DEMO_ACT_NONE = 0u,
    DEMO_ACT_RESET,
    DEMO_ACT_POINT,        /* arg0 = 回路(0..3,RG=3) */
    DEMO_ACT_POINT_ALL,    /* 依次测 CISS/COSS/CRSS */
    DEMO_ACT_SWEEP,        /* arg0 = 回路(0..2)或 3 = 先测 RG 再三路扫 */
    DEMO_ACT_SWEEP_STOP
} DemoAction;

static int      s_demo_enabled;
static uint32_t s_demo_action;
static uint32_t s_demo_arg0;
static uint32_t s_demo_delay;
static uint32_t s_demo_sweep_index;   /* 已发出的扫压点序号(1 起) */
static uint32_t s_demo_sweep_mask;    /* 本次扫压要画哪几条曲线 */
static uint32_t s_demo_sweep_path;    /* 动作里的回路参数(3 = 全部) */
static int      s_demo_notice;
static uint32_t s_demo_auto_page;     /* CV_UI_DEMO_PAGE:跑完自动切到这一页(0=不切) */
static uint32_t s_demo_auto_stage;

static float demo_capacitance(uint32_t path, float voltage)
{
    /* 单调下降:低压平台 → 高压塌陷。量级取自典型 650 V 功率 MOSFET。 */
    static const float c_max[3] = {980.0f, 240.0f, 48.0f};
    static const float c_min[3] = {420.0f, 62.0f, 7.5f};
    float t;

    if (path >= 3u)
    {
        path = 0u;
    }
    t = 1.0f / (1.0f + powf(voltage / 2.2f, 0.85f));
    return c_min[path] + ((c_max[path] - c_min[path]) * t);
}

/* 1 MHz 激励下的容性电流(mA):I = 2·π·f·C·V。只用于让波形有合理的量级。 */
static float demo_current_ma(float capacitance_pf, float voltage)
{
    return 6.2831853f * 1.0e6f * capacitance_pf * 1.0e-12f * voltage * 1000.0f;
}

static float demo_bias_at(uint32_t index_1based)
{
    float ratio;

    if (DEMO_SWEEP_POINTS <= 1u)
    {
        return 0.1f;
    }
    ratio = (float)(index_1based - 1u) / (float)(DEMO_SWEEP_POINTS - 1u);
    /* 对数分布:低压段点密,高压段点疏,和工程习惯一致 */
    return 0.1f * powf(DEMO_SWEEP_V_MAX / 0.1f, ratio);
}

static void demo_emit_point_waveform(uint32_t path, float bias)
{
    float capacitance = demo_capacitance(path, bias);
    float current = (path == UI_SINGLE_PATH_RG) ?
                    (bias / 12.43f) * 1000.0f :
                    demo_current_ma(capacitance, bias);
    uint32_t index;

    UI_Demo_BeginWaveform(UI_DEMO_WAVE_POINT, DEMO_POINT_SAMPLES);
    for (index = 1u; index <= DEMO_POINT_SAMPLES; ++index)
    {
        float progress = (float)(index - 1u) / 12.0f;
        float settled = expf(-progress);
        float ripple = 0.02f * sinf((float)index * 0.9f);

        UI_Demo_UpdateWaveformPoint(UI_DEMO_WAVE_POINT, index,
                                    DEMO_POINT_SAMPLES,
                                    bias * (1.0f - settled) + ripple,
                                    current * (1.0f - settled) *
                                    (1.0f + ripple),
                                    NULL);
    }
}

static void demo_emit_point(uint32_t path)
{
    char cs_text[24];
    char cp_text[24];
    char d_text[24];
    float voltage = 1.0f;
    float cs = demo_capacitance(path, voltage);
    float cp = cs * 1.02f;
    float d = 0.018f;

    (void)snprintf(cs_text, sizeof(cs_text), "%.2f", (double)cs);
    (void)snprintf(cp_text, sizeof(cp_text), "%.2f", (double)cp);
    (void)snprintf(d_text, sizeof(d_text), "%.4f", (double)d);

    if (path == UI_SINGLE_PATH_RG)
    {
        UI_Demo_UpdateResistance("12.43", 1u);
        return;
    }
    UI_Demo_UpdateSingleCapacitance(path, cs_text, cp_text, d_text, 1u, voltage);
    UI_Demo_UpdateAllCapacitance(path, cs_text, cp_text, d_text, 1u);
    demo_emit_point_waveform(path, 1.0f);
}

static void demo_start_sweep(void)
{
    uint32_t multi = (s_demo_sweep_mask == 0x07u) ? 1u : 0u;
    uint32_t first = 0u;

    while ((first < 3u) && ((s_demo_sweep_mask & (1u << first)) == 0u))
    {
        first++;
    }
    UI_Demo_ClearSweep(3u);   /* ≥3 = 清全部三路 */
    UI_Demo_BeginSweep(first, DEMO_SWEEP_POINTS, multi);
    UI_Demo_BeginWaveform(UI_DEMO_WAVE_SWEEP, DEMO_SWEEP_POINTS);
    s_demo_sweep_index = 0u;

    if (s_demo_sweep_path == 3u)
    {
        /* 「先测 RG,再三路扫压」—— 与扫压页第 4 个选项同一套语义 */
        UI_Demo_UpdateResistance("12.43", 1u);
    }
}

static uint32_t demo_first_visible_path(void)
{
    uint32_t path = 0u;

    while ((path < 3u) && ((s_demo_sweep_mask & (1u << path)) == 0u))
    {
        path++;
    }
    return path;
}

static void demo_emit_sweep_step(void)
{
    char bias_text[24];
    char cap_text[24];
    uint32_t path;

    s_demo_sweep_index++;
    if (s_demo_sweep_index > DEMO_SWEEP_POINTS)
    {
        UI_Demo_SetSweepInspectionReady();
        s_demo_action = DEMO_ACT_NONE;
        return;
    }

    for (path = 0u; path < 3u; ++path)
    {
        float voltage;
        float cs;
        float cp;
        float d;

        if ((s_demo_sweep_mask & (1u << path)) == 0u)
        {
            continue;
        }

        voltage = demo_bias_at(s_demo_sweep_index);
        cs = demo_capacitance(path, voltage);
        cp = cs * (1.0f + (0.02f * voltage));
        d = 0.015f + (0.0012f * voltage);

        (void)snprintf(bias_text, sizeof(bias_text), "%.2f",
                       (double)voltage);
        (void)snprintf(cap_text, sizeof(cap_text), "%.2f", (double)cs);

        UI_Demo_UpdateSweepPoint(path,
                                 s_demo_sweep_index,
                                 DEMO_SWEEP_POINTS,
                                 voltage, cs, cp, d, 1u,
                                 bias_text, cap_text, 1u);

        if (path == demo_first_visible_path())
        {
            /* 包络线只画一条:多路扫压时画第一路,免得三条曲线叠在一起没法看 */
            UI_Demo_UpdateWaveformPoint(
                UI_DEMO_WAVE_SWEEP, s_demo_sweep_index, DEMO_SWEEP_POINTS,
                voltage, demo_current_ma(cs, voltage), NULL);
        }
    }
}

/*
 * 自动演示序列(仅在设了 `CV_UI_DEMO_PAGE` 时跑):
 *   先点测四条回路 → 再跑一次三路扫压 → 完成后切到指定页。
 * 用途只有一个:**无人值守地截到一张"有数据"的页面图**。
 * 不设这个环境变量时演示模式仍是可交互的,与真人操作一致。
 */
static void demo_auto_run(void)
{
    if (s_demo_auto_page == 0u)
    {
        return;
    }

    if (s_demo_auto_stage == 0u)
    {
        demo_emit_point(0u);
        demo_emit_point(1u);
        demo_emit_point(2u);
        demo_emit_point(3u);
        s_demo_sweep_path = 3u;
        s_demo_sweep_mask = 0x07u;
        s_demo_action = DEMO_ACT_SWEEP;
        demo_start_sweep();
        s_demo_auto_stage = 1u;
        return;
    }

    if ((s_demo_auto_stage == 1u) && (s_demo_action != DEMO_ACT_SWEEP))
    {
        UI_Demo_GotoPage(s_demo_auto_page);
        s_demo_auto_stage = 2u;
    }
}

static void demo_step(void)
{
    demo_auto_run();

    if (!s_demo_enabled || (s_demo_action == DEMO_ACT_NONE))
    {
        return;
    }

    if (s_demo_action == DEMO_ACT_SWEEP)
    {
        demo_emit_sweep_step();
        return;
    }

    if (s_demo_delay > 0u)
    {
        s_demo_delay--;
        return;
    }

    switch (s_demo_action)
    {
        case DEMO_ACT_POINT:
            demo_emit_point(s_demo_arg0);
            break;

        case DEMO_ACT_POINT_ALL:
            demo_emit_point(0u);
            demo_emit_point(1u);
            demo_emit_point(2u);
            break;

        case DEMO_ACT_SWEEP_STOP:
            UI_Demo_SetSweepInspectionReady();
            break;

        default:
            break;
    }
    s_demo_action = DEMO_ACT_NONE;
}


/* ------------------------------------------------------------------ */
/* 统计(排障用,不进 display_protocol.h —— 那是前序的接口)               */
/* ------------------------------------------------------------------ */

static uint32_t s_request_count;        /* 一共被点了多少次要发指令 */
static int      s_notice_shown;         /* stderr 提示只打一次,别刷屏   */

/**
 * @brief 每个 Request 都走这里:记数 + 第一次打一行提示 + 返回"没发出去"。
 *
 * @param what 指令名(只用于打印)
 * @return 恒为 `false`(见文件头)
 */
static bool refuse(const char *what)
{
    s_request_count++;

    if (!s_notice_shown) {
        s_notice_shown = 1;
        fprintf(stderr,
                "[proto] 第 1 次测量指令(%s)被拒:本轮未接测量链。\n"
                "        ⇒ UI 会显示「指令发送失败」,这是**预期的**,\n"
                "          不代表触屏或界面有问题。\n"
                "          要接的话:把 `ui_process.py` 那侧做成协议对端,\n"
                "          再把本文件换掉(不要往这里塞测量逻辑)。\n",
                what);
    }
    return false;
}

/**
 * @brief 按「演示模式开没开」决定:回假成功 + 挂一个待发的假数据,还是照样拒。
 *
 * ⚠️ 返回 `true` 在演示模式下**仍然是对界面撒谎** —— 只是这次谎是**故意的**,
 *    目的是把测量链没接通时界面的样子先看一遍。所以它只在 `CV_UI_DEMO=1`
 *    时生效,且启动时会往 stderr 打一行醒目提示。
 */
static bool accept_or_refuse(const char *what, uint32_t action, uint32_t arg0)
{
    if (!s_demo_enabled)
    {
        return refuse(what);
    }

    if (action == DEMO_ACT_SWEEP)
    {
        s_demo_sweep_path = arg0;
        s_demo_sweep_mask = (arg0 >= 3u) ? 0x07u : (1u << arg0);
        s_demo_action = DEMO_ACT_SWEEP;
        demo_start_sweep();
        return true;
    }

    s_demo_action = action;
    s_demo_arg0 = arg0;
    s_demo_delay = DEMO_LATENCY_TICKS;
    return true;
}

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief 空实现。保留函数体而非空宏:留一行打印,好在日志里看出
 *        "这一层确实被调到了,只是里面没东西"。
 */
void DisplayProtocol_Init(void)
{
    const char *demo = getenv("CV_UI_DEMO");
    const char *page = getenv("CV_UI_DEMO_PAGE");

    s_request_count = 0;
    s_notice_shown  = 0;
    s_demo_enabled  = ((demo != NULL) && (demo[0] == '1')) ? 1 : 0;
    s_demo_action   = DEMO_ACT_NONE;
    s_demo_notice   = 0;
    s_demo_auto_page = ((page != NULL) && (page[0] >= '0') &&
                        (page[0] <= '9')) ?
                       (uint32_t)(page[0] - '0') : 0u;
    s_demo_auto_stage = 0u;

    if (s_demo_enabled)
    {
        /* ⚠️ 必须在 UI_Demo_Init() 之后调用 —— 这里会动 LVGL 对象 */
        UI_Demo_SetConnection(1u);
        UI_Demo_SetStatusText("状态:演示模式(读数为编造)");
        printf("[proto] ⚠️  演示模式已开启(CV_UI_DEMO=1)\n"
               "        屏幕上的测量读数是**模型生成的假数据**,只用于检查界面布局;\n"
               "        ⛔ 不得作为任何测量结论的依据。\n"
               "        关闭:不带 CV_UI_DEMO 重启。\n");
        return;
    }
    printf("[proto] 空实现已装载:测量指令不会真的发出(本轮范围)\n");
}

/**
 * @brief 空实现 —— 没有对端,就没有可收的帧。
 *
 * ⚠️ 前序的 `Process()` 里还有"超时了就把界面标成失联"的逻辑。
 *    这里**不做**:本轮没有对端,做超时判断只会让界面开机就报失联,
 *    属于自造噪声。
 */
void DisplayProtocol_Process(void)
{
    demo_step();

    if (s_demo_enabled && !s_demo_notice)
    {
        s_demo_notice = 1;
        fprintf(stderr,
                "[proto] 演示模式:第 %u 次测量指令已按「假成功」处理。\n",
                (unsigned)s_request_count);
    }
}

/* ------------------------------------------------------------------ */
/* 请求(全部返回 false,见文件头)                                        */
/* ------------------------------------------------------------------ */

bool DisplayProtocol_RequestReset(void)
{
    return accept_or_refuse("RequestReset", DEMO_ACT_RESET, 0u);
}

bool DisplayProtocol_RequestPath(uint32_t path)
{
    (void)path;
    /* 切回路本身不产生读数:演示模式下回"成功",真实模式下照旧拒 */
    return accept_or_refuse("RequestPath", DEMO_ACT_NONE, 0u);
}

bool DisplayProtocol_RequestBias(uint32_t bias_tenths)
{
    (void)bias_tenths;
    return accept_or_refuse("RequestBias", DEMO_ACT_NONE, 0u);
}

bool DisplayProtocol_RequestPoint(uint32_t path, uint32_t bias_tenths)
{
    (void)bias_tenths;
    return accept_or_refuse("RequestPoint", DEMO_ACT_POINT, path);
}

bool DisplayProtocol_RequestPointAll(uint32_t bias_tenths)
{
    (void)bias_tenths;
    return accept_or_refuse("RequestPointAll", DEMO_ACT_POINT_ALL, 0u);
}

bool DisplayProtocol_RequestSweep(uint32_t path, uint32_t stop_tenths)
{
    (void)stop_tenths;
    return accept_or_refuse("RequestSweep", DEMO_ACT_SWEEP, path);
}

bool DisplayProtocol_RequestRgThenSweepAll(uint32_t stop_tenths)
{
    (void)stop_tenths;
    return accept_or_refuse("RequestRgThenSweepAll", DEMO_ACT_SWEEP, 3u);
}

bool DisplayProtocol_RequestSweepStop(void)
{
    /* 演示模式下不真的打断曲线,只把状态清掉 */
    return accept_or_refuse("RequestSweepStop", DEMO_ACT_SWEEP_STOP, 0u);
}

bool DisplayProtocol_RequestSweepStatus(void)
{
    return accept_or_refuse("RequestSweepStatus", DEMO_ACT_NONE, 0u);
}

/* ------------------------------------------------------------------ */
/* 统计上报                                                            */
/* ------------------------------------------------------------------ */

uint32_t DisplayProtocol_GetReceivedLineCount(void)  { return 0; }
uint32_t DisplayProtocol_GetCrcErrorCount(void)      { return 0; }
uint32_t DisplayProtocol_GetTransmitDropCount(void)  { return 0; }

/* 排障用:main.c 退出时打印一行 */
uint32_t DisplayProtocol_GetRefusedCount(void)
{
    return s_request_count;
}
