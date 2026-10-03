/**
 * @file main.c
 * @brief `c_ui` 入口 —— PYNQ 上的 LVGL 界面进程
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【这个进程负责什么】
 *   把前序 STM32 的 `ui_demo.c`(6 个页面)跑在 Zynq 的 PS 上,
 *   往 **DDR 双帧缓存**里画;PL 侧的 `lcd_fb_reader` 按行读出去。
 *   触摸坐标从 PL 的寄存器里读。
 *
 *   ⇒ **它不管测量。** 测量指令(`DisplayProtocol_Request*`)本轮是空实现,
 *     见 `pynq_protocol_stub.c`。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 📌 【命令行契约】与 `src/python/drivers/ui_process.py` 的文件头一致。
 *    改这里要同时改那里(两份都写了,就是为了防止单边漂移)。
 *
 *     c_ui --axi-base <hex> --fb0 <hex> --fb1 <hex> [--touch] [--poll-ms <n>]
 *
 *     --axi-base    AXI 从机基址            默认 0x43C00000
 *     --fb0/--fb1   两块帧缓存的**物理**基址   **必填**(Python 用 pynq.allocate 分好传进来)
 *     --poll-ms     触摸轮询周期            默认 10
 *     --touch       开触摸(默认就是开)—— 写出来只是为了命令行自解释
 *     --no-touch    关触摸。**屏上不接触摸底板时用它**,免得每 10 ms 空读一次 AXI
 *
 *     退出码:0 正常;1 初始化失败;2 参数错
 *
 * 🔴 **必须是 root** —— 要 open("/dev/mem")。
 *
 * ────────────────────────────────────────────────────────────────────────
 * ⚠️ 【本文件**故意**只做装配,不写业务】
 *    用户的要求是"每个模块有自己的基础驱动库,别什么都堆到 main 里,
 *    尽量别混在一起"。所以这里只有:解析参数 → 按序初始化 → 主循环转发。
 *    任何"顺手多做一点"(读坐标、算颜色、发指令)都该落在对应驱动里。
 */

#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "pynq_regs.h"

#include "BSP/LVGL/lv_port_disp.h"
#include "BSP/LVGL/lv_port_indev.h"
#include "BSP/TOUCH/lcd_touch.h"

#include "ui_demo.h"

/* ------------------------------------------------------------------ */
/* 缺省与常量                                                          */
/* ------------------------------------------------------------------ */

#define DEFAULT_AXI_BASE    0x43C00000u
#define DEFAULT_POLL_MS     10u

/** 主循环里最长睡多久。即使 LVGL 说"还早",也至少这么频繁回来一次,
 *  好让触摸轮询与信号处理保持及时。 */
#define LOOP_MAX_SLEEP_MS   20u

/* ------------------------------------------------------------------ */
/* 运行状态                                                            */
/* ------------------------------------------------------------------ */
static volatile sig_atomic_t g_running = 1;
static uint32_t g_touch_polls;
static uint32_t g_touch_frames;      /* 收到「新帧」的次数(不是读寄存器次数) */
static uint32_t g_touch_errors;

/* 用于排障的跨文件声明 —— 见 pynq_touch.c / pynq_indev.c 尾部注释 */
int      pynq_touch_debug_line(char *buf, unsigned len);
uint32_t LV_PortIndev_GetOutOfRangeCount(void);
uint32_t DisplayProtocol_GetRefusedCount(void);

/* ------------------------------------------------------------------ */
static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;      /* 只置标志 —— 信号处理里不碰 LVGL/mmap */
}

static void install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    /* 不设 SA_RESTART:让 cv_delay_ms 里的 nanosleep 被打断后立刻回来,
     * 否则最长要睡满 20 ms 才响应 Ctrl-C(能接受,但没必要)。 */
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ------------------------------------------------------------------ */
static void usage(const char *argv0)
{
    printf(
        "用法: %s --fb0 <hex> --fb1 <hex> [--axi-base <hex>] [--poll-ms <n>] [--no-touch]\n"
        "\n"
        "  --fb0/--fb1   两块帧缓存的**物理**基址(由 main.py 用 pynq.allocate 分配后传入)\n"
        "  --axi-base    AXI 从机基址,默认 0x%08X\n"
        "  --poll-ms     触摸轮询周期(ms),默认 %u\n"
        "  --no-touch    不读触摸寄存器\n"
        "\n"
        "⚠️ 需要 root(要 open /dev/mem)。\n"
        "   独立运行示例:\n"
        "     sudo ./c_ui --fb0 0x1F000000 --fb1 0x1F200000\n",
        argv0, DEFAULT_AXI_BASE, DEFAULT_POLL_MS);
}

/* ------------------------------------------------------------------ */
static int parse_hex(const char *s, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;

    if (s == NULL) {
        return -1;
    }
    v = strtoul(s, &end, 0);          /* 0 ⇒ 自动认 0x 前缀 */
    if (end == s || *end != '\0' || v > 0xFFFFFFFFul) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

/* ------------------------------------------------------------------ */
/**
 * @brief 触摸轮询:读 → 喂给 LVGL 输入层。
 *
 * 🔴 **返回 `NO_UPDATE` 时也照样转发,不能当成"松手"。**
 *    GT911 在手指停住时不产生新帧,PL 这条路上 `seq` 就停住、
 *    而 `pen_down`/`count` 由 PL **保持**(gt911_ctrl.v:532)。
 *    `pynq_touch.c` 已经把这份**保持态**填进 `state` 了,
 *    直接转给 LVGL 就是对的。
 */
static void poll_touch(uint32_t poll_ms)
{
    static uint32_t next_due;
    static int      arm_release;      /* 上一次是否报过按下,用来只打一次日志 */

    uint32_t now = cv_millis();
    if ((int32_t)(now - next_due) < 0) {
        return;                        /* 还没到点 */
    }
    next_due = now + poll_ms;

    LCD_TouchState st;
    uint32_t r = LCD_Touch_Read(&st);
    g_touch_polls++;

    switch (r) {
    case LCD_TOUCH_OK:
        g_touch_frames++;
        LV_PortIndev_Update(&st);
        if (st.count != 0 && !arm_release) {
            arm_release = 1;
            printf("[touch] 按下 (%u,%u) 点数 %u\n",
                   (unsigned)st.point[0].x, (unsigned)st.point[0].y,
                   (unsigned)st.count);
        } else if (st.count == 0 && arm_release) {
            arm_release = 0;
            printf("[touch] 抬起\n");
        }
        break;

    case LCD_TOUCH_NO_UPDATE:
        /* 不是新帧,但**保持态**仍要喂进去 —— 见函数头 */
        LV_PortIndev_Update(&st);
        break;

    case LCD_TOUCH_ERROR_BUS:
        /* ⚠️ 这是"PL 报过总线错"的**一次性**通知,不是"此刻在错"
         *    (粘滞位只有重配 PL 才清,见 pynq_touch.c)。
         *    ⇒ 什么也不做,尤其**不要** Release():那会凭空造一次抬起,
         *      正在按的按钮会被判成点了一下。 */
        g_touch_errors++;
        break;

    default:
        g_touch_errors++;
        break;
    }
}

/* ------------------------------------------------------------------ */
static void dump_stats(void)
{
    char line[160];

    pynq_touch_debug_line(line, sizeof(line));

    printf("\n──── c_ui 统计 ────\n");
    printf("  disp  flush=%u  换址=%u  矩形同步=%u  flush错=%u  换址超时=%u  最近错误=%u\n",
           (unsigned)LV_PortDisp_GetFlushCount(),
           (unsigned)LV_PortDisp_GetFrameSwapCount(),
           (unsigned)LV_PortDisp_GetCpuFallbackCount(),
           (unsigned)LV_PortDisp_GetFlushErrorCount(),
           (unsigned)LV_PortDisp_GetVBlankFallbackCount(),
           (unsigned)LV_PortDisp_GetLastError());
    printf("  前台物理地址 = 0x%08X\n", (unsigned)LV_PortDisp_GetFrontAddress());
    printf("  touch 轮询=%u  新帧=%u  错误=%u  越界点=%u  LVGL读取=%u\n",
           (unsigned)g_touch_polls, (unsigned)g_touch_frames,
           (unsigned)g_touch_errors, (unsigned)LV_PortIndev_GetOutOfRangeCount(),
           (unsigned)LV_PortIndev_GetReadCount());
    printf("  touch 末态 %s\n", line);
    printf("  UI    点击=%u  页面=%u  被拒的测量指令=%u\n",
           (unsigned)UI_Demo_GetClickCount(),
           (unsigned)UI_Demo_GetCurrentPage(),
           (unsigned)DisplayProtocol_GetRefusedCount());

    /* 帧缓存欠载是"PS 供给跟不上 PL 取数"的直接证据 —— 单列一行,别埋在计数里 */
    uint32_t st = cv_reg_read(CV_REG_FB_STATUS);
    printf("  0x64 = 0x%08X  underrun=%u  axierr=%u  vblank_cnt=%u  swap_cnt=%u\n",
           st, (unsigned)CV_FB_UNDERRUN(st), (unsigned)CV_FB_AXIERR(st),
           (unsigned)CV_FB_VBLANK_CNT(st), (unsigned)CV_FB_SWAP_CNT(st));
    if (CV_FB_UNDERRUN(st)) {
        printf("  ⚠️  underrun 已置起(粘滞,写 0x60 清):PL 取数时帧缓存还没备好。\n"
               "      上板判据是「跑 60 s 后必须为 0」,见 display_probe.py。\n");
    }
    printf("───────────────────\n");
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    uint32_t axi_base   = DEFAULT_AXI_BASE;
    uint32_t fb_phys[2] = {0, 0};
    uint32_t poll_ms    = DEFAULT_POLL_MS;
    int      use_touch  = 1;      /* 默认开 —— 本轮的验收项就有触摸 */
    int      have_fb[2] = {0, 0};

    static const struct option long_opts[] = {
        {"axi-base", required_argument, NULL, 'a'},
        {"fb0",      required_argument, NULL, '0'},
        {"fb1",      required_argument, NULL, '1'},
        {"poll-ms",  required_argument, NULL, 'p'},
        {"touch",    no_argument,       NULL, 't'},
        {"no-touch", no_argument,       NULL, 'T'},
        {"help",     no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    for (;;) {
        int c = getopt_long(argc, argv, "a:0:1:p:tTh", long_opts, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'a':
            if (parse_hex(optarg, &axi_base) != 0) {
                fprintf(stderr, "参数错: --axi-base 不是合法整数:%s\n", optarg);
                return 2;
            }
            break;
        case '0':
            if (parse_hex(optarg, &fb_phys[0]) != 0) {
                fprintf(stderr, "参数错: --fb0 不是合法整数:%s\n", optarg);
                return 2;
            }
            have_fb[0] = 1;
            break;
        case '1':
            if (parse_hex(optarg, &fb_phys[1]) != 0) {
                fprintf(stderr, "参数错: --fb1 不是合法整数:%s\n", optarg);
                return 2;
            }
            have_fb[1] = 1;
            break;
        case 'p': {
            char *end = NULL;
            long v = strtol(optarg, &end, 0);
            if (end == optarg || *end != '\0' || v < 1 || v > 1000) {
                fprintf(stderr, "参数错: --poll-ms 应在 1..1000,收到 %s\n", optarg);
                return 2;
            }
            poll_ms = (uint32_t)v;
            break;
        }
        case 't': use_touch = 1; break;
        case 'T': use_touch = 0; break;
        case 'h': usage(argv[0]); return 0;
        default:
            usage(argv[0]);
            return 2;
        }
    }

    /* ---- 参数自检:帧缓存地址是这个进程的**唯一**输入,错了后面全白干 ---- */
    if (!have_fb[0] || !have_fb[1]) {
        fprintf(stderr,
                "参数错: --fb0 与 --fb1 都必须给。\n"
                "        ⇒ 帧缓存由 Python 用 pynq.allocate 分配,物理地址只有它知道;\n"
                "          C 侧拿不到物理连续内存,所以**不能自己分配**。\n"
                "          正常路径是跑 `main.py`,它会算好传进来。\n");
        usage(argv[0]);
        return 2;
    }
    if (fb_phys[0] == fb_phys[1]) {
        fprintf(stderr, "参数错: 两块帧缓存地址相同(0x%08X),双缓冲无从谈起\n", fb_phys[0]);
        return 2;
    }

    install_signal_handlers();

    /* ⚠️ 行缓冲。默认 stdout 接管道/文件时是**全缓冲**,而 stderr 永远无缓冲
     *    ⇒ 日志重定向到文件后,每条 stdout 小字都比 stderr 的报错晚一大截,
     *      读起来像"先失败后启动"。上板排障全靠这些日志,顺序不能乱。 */
    setvbuf(stdout, NULL, _IOLBF, 0);

    printf("──── c_ui 启动 ────\n");
    printf("  axi_base = 0x%08X   fb0 = 0x%08X   fb1 = 0x%08X   poll = %u ms   touch = %s\n",
           axi_base, fb_phys[0], fb_phys[1], poll_ms, use_touch ? "开" : "关");

    /* ---- 1. 寄存器窗口 ---- */
    if (cv_regs_init(axi_base) != 0) {
        fprintf(stderr, "初始化失败: 打不开/映射不了 AXI 寄存器窗口(见上面 pynq_regs 的提示)\n");
        return 1;
    }

    /* ---- 2. 声明帧缓存 + 切到 DDR 源 ----
     * ⚠️ Python 的 LcdPanel.allocate() 也写过这两个寄存器(跑探针脚本时用)。
     *    这里**再写一遍是故意的**:地址值本来就来自命令行,写一遍让 c_ui
     *    能脱离 main.py 单独跑,也不受"谁先跑"的顺序影响。
     *    两边写的是**同一个值**,不会打架。 */
    cv_reg_write(CV_REG_FB_BASE0, fb_phys[0]);
    cv_reg_write(CV_REG_FB_BASE1, fb_phys[1]);

    /* ---- 3. LVGL 核心 ---- */
    lv_init();

    /* ---- 4. 显示驱动:两块整屏 DDR 缓冲,direct_mode ---- */
    if (LV_PortDisp_Init(fb_phys[0], fb_phys[1]) != LV_PORT_DISP_OK) {
        fprintf(stderr, "初始化失败: 显示驱动(见上面 [disp] 的提示)\n");
        cv_regs_close();
        return 1;
    }

    /* ---- 5. 输入驱动(必须在显示之后注册)---- */
    if (use_touch) {
        LCD_Touch_Init();                        /* 只做链路检查,不碰硬件 */
        if (LV_PortIndev_Init() != LV_PORT_INDEV_OK) {
            fprintf(stderr, "初始化失败: 输入驱动注册不上\n");
            cv_regs_close();
            return 1;
        }
    }

    /* ---- 6. 界面 ---- */
    if (UI_Demo_Init() != UI_DEMO_OK) {
        fprintf(stderr, "初始化失败: UI_Demo_Init 返回非 OK\n");
        cv_regs_close();
        return 1;
    }

    /* ---- 7. 接上 DDR 源 ----
     * 🔴 **必须在第一帧画完之后才切**,不能一开始就切:
     *    切过去的一瞬间 PL 就在读帧缓存了,而那时里面还是**上电残留的
     *    随机数据** —— 屏上会先闪一下雪花。等第一帧备好再切,屏上
     *    是从彩条**直接**变成首页,干净。 */
    printf("[main] 等第一帧画完再切到 DDR 源(期间屏上应显示厂商彩条)…\n");

    uint32_t last_tick = cv_millis();
    int      switched  = 0;

    while (g_running) {
        /* --- tick --- */
        uint32_t now = cv_millis();
        lv_tick_inc((uint32_t)(now - last_tick));
        last_tick = now;

        /* --- 触摸(每 poll_ms 读一次寄存器,喂给 LVGL) --- */
        if (use_touch) {
            poll_touch(poll_ms);
        }

        /* --- 界面重绘(内部:LVGL 需要重画时才画,画完调 flush 回调换址) --- */
        uint32_t till_next = lv_timer_handler();

        /* --- 第一帧备好了吗? --- */
        if (!switched && LV_PortDisp_GetFlushCount() > 0) {
            switched = 1;
            cv_reg_write(CV_REG_LCD_CTRL, CV_LCD_SRC_DDR);
            printf("[main] 首帧已提交(flush=%u),已切到 DDR 源(0x50 = %u)\n",
                   (unsigned)LV_PortDisp_GetFlushCount(), CV_LCD_SRC_DDR);
        }

        /* --- 睡到下一次该动的时候,但至少 1 ms、至多 LOOP_MAX_SLEEP_MS --- */
        if (till_next > LOOP_MAX_SLEEP_MS) {
            till_next = LOOP_MAX_SLEEP_MS;
        }
        if (till_next < 1u) {
            till_next = 1u;
        }
        cv_delay_ms(till_next);
    }

    /* ---- 收尾 ---- */
    printf("\n[main] 收到退出信号,收尾…\n");

    if (!switched) {
        printf("[main] ⚠️ 一帧都没画出来(flush=0)—— 界面没能起来,查上面的日志\n");
    }

    dump_stats();

    /* 把屏切回**厂商彩条**:这是不依赖 DDR、不依赖 PS 的确定状态。
     * 直接留着最后一帧的话,PL 还在读那块缓冲,而进程已经走了 ——
     * 后续谁去改那块内存都可能在屏上显形,不好判断"界面停了没有"。 */
    cv_reg_write(CV_REG_LCD_CTRL, CV_LCD_SRC_BARS);
    printf("[main] 屏已切回厂商彩条(0x50 = %u)\n", CV_LCD_SRC_BARS);

    cv_regs_close();
    return 0;
}
