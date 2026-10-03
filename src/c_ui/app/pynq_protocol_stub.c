/**
 * @file pynq_protocol_stub.c
 * @brief `display_protocol.h` 的**空实现**(本轮不接测量链)
 *
 * 接口在 `ui/display_protocol.h`(**前序的,一行没改**),实现是新的。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【前序这一层是干什么的】
 *   它是 UI ↔ 主控(STM32 板)之间的**板间协议层**:UART7 上跑一套
 *   带 CRC 的帧,`Request*` 往外发指令,`Process()` 收回来更新界面。
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
 *     · `false` ⇒ 状态栏显示「UART7发送失败」(例:`ui_demo.c:3417`)。
 *
 *   ⇒ 返回 `true` 是**对用户撒谎**:屏幕上会出现"指令发送完成"、
 *     扫压页会摆出"正在测量"的样子,而底下**什么都没发生**。
 *     返回 `false` 则如实说出"这条指令没发出去",用户一眼就知道
 *     测量链还没接 —— 这正是一期该有的样子。
 *
 *   ⚠️ 副作用要提前知道:本轮**每个测量按钮**点下去都会显示
 *      「UART7发送失败」。这**不代表触屏坏了** —— 触屏好不好,
 *      看 `tools/touch_probe.py`,别拿这个状态栏做判据。
 *      (页面之间的**导航**不走这一层,是纯 LVGL,不受影响。)
 *
 *   ⚠️ 上报的 0 是真话:没收到过任何一行,CRC 错 0 次,丢包 0 次 ——
 *      不是"忘了填",是确实没有这条通路。
 */

#include "display_protocol.h"

#include <stdio.h>

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
                "        ⇒ UI 会显示「UART7发送失败」,这是**预期的**,\n"
                "          不代表触屏或界面有问题。\n"
                "          要接的话:把 `ui_process.py` 那侧做成协议对端,\n"
                "          再把本文件换掉(不要往这里塞测量逻辑)。\n",
                what);
    }
    return false;
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
    s_request_count = 0;
    s_notice_shown  = 0;
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
}

/* ------------------------------------------------------------------ */
/* 请求(全部返回 false,见文件头)                                        */
/* ------------------------------------------------------------------ */

bool DisplayProtocol_RequestReset(void)
{
    return refuse("RequestReset");
}

bool DisplayProtocol_RequestPath(uint32_t path)
{
    (void)path;
    return refuse("RequestPath");
}

bool DisplayProtocol_RequestBias(uint32_t bias_tenths)
{
    (void)bias_tenths;
    return refuse("RequestBias");
}

bool DisplayProtocol_RequestPoint(uint32_t path, uint32_t bias_tenths)
{
    (void)path;
    (void)bias_tenths;
    return refuse("RequestPoint");
}

bool DisplayProtocol_RequestPointAll(uint32_t bias_tenths)
{
    (void)bias_tenths;
    return refuse("RequestPointAll");
}

bool DisplayProtocol_RequestSweep(uint32_t path, uint32_t stop_tenths)
{
    (void)path;
    (void)stop_tenths;
    return refuse("RequestSweep");
}

bool DisplayProtocol_RequestRgThenSweepAll(uint32_t stop_tenths)
{
    (void)stop_tenths;
    return refuse("RequestRgThenSweepAll");
}

bool DisplayProtocol_RequestSweepStop(void)
{
    return refuse("RequestSweepStop");
}

bool DisplayProtocol_RequestSweepStatus(void)
{
    return refuse("RequestSweepStatus");
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
