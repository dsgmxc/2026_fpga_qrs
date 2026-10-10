/**
 * @file relay_view.c
 * @brief 继电器 / 档位的**只读**回显层
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【这个模块解决什么】
 *   顶栏一直显示「回路:CISS」,而硬件实际停在 `ALL_OFF(0x10)` ——
 *   **界面在说谎**,根因是当初那条"本位流里没有 `relay_ctrl`"的过期假设:
 *   `DisplayProtocol_Init()` 把回路写死成字符串 `"CISS"`,`RequestPath()`
 *   只 printf 一句"仅界面状态"。PL 侧的继电器/档位寄存器
 *   (0x04/0x18/0x1C/0x24)2026-10-10 上板验证通过之后,这个谎就没有借口了。
 *
 * 🔴 **只读,一个寄存器都不写。**
 *   · 写入权归 Python 侧(`tools/relay_hold.py` 之类)。C 界面再插一个写入方,
 *     就会出现"谁最后写的算谁的",而两边都不知情。
 *   · 顺着本工程既有的习惯:界面显示的是硬件**回读生效值**,不是请求值
 *     (偏压 `bias_apply`、`0x60 LCD_SWAP` 都这么定)。
 *   · 所以回路按钮点下去**不再假装切换** —— 顶栏那一栏该显示"硬件现在在哪路",
 *     它跟按钮高亮(「我**要**测哪路」)本来就是两件事。
 *
 * 【为什么显示短名而不是 cv_regs.py 的全名】
 *   顶栏是**定宽**的(1024 一共 8 栏)。`COSS_DISCHARGE` 14 个字符
 *   ≈ 120 px,加上「回路:」前缀就塞不进 140 px 的列了。所以两个放电态
 *   缩成 `COSS_DIS` / `CISS_DIS`,其余保持 `cv_regs.py` 里的名字不变
 *   —— 报告里能一眼对上。
 *
 * ⚠️ **文案要过字库。** `ui/ui_font_cn_20.c` 是按 `--symbols` 生成的子集
 *    (另有 `-r 0x20-0x7E` 覆盖全部 ASCII),**缺字只会静默显示成空白,
 *    编译器不报**。本文件刻意只用 ASCII 短名,就是为了绕开这个坑。
 *    要加中文文案,先按 `docs/` 里的办法核字库。
 * ========================================================================== */

#include "relay_view.h"

#include "pynq_regs.h"
#include "ui_demo.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/* 节流与去重状态                                                      */
/* ------------------------------------------------------------------ */

/** 轮询周期(ms)。读 3 个 32 位寄存器很便宜,但没必要每圈都读 ——
 *  主循环 1..20 ms 一转(`main.c` 的 `LOOP_MAX_SLEEP_MS`)。 */
#define RELAY_POLL_MS   200u

static uint32_t s_next_ms;              /* 下次允许读的时刻          */
static uint32_t s_last_cfg  = 0xFFFFFFFFu;  /* 哨兵:首轮必刷        */
static uint32_t s_last_gear = 0xFFFFFFFFu;
static uint32_t s_last_notice_key;      /* (err<<1)|fault,去重用     */
static uint32_t s_last_err;             /* 被节流时返回它,不产生 AXI 读 */

/* ------------------------------------------------------------------ */
/* 值 → 显示名                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief `0x1C` 继电器组合 → 顶栏「回路:」显示名。
 * @return 静态字符串(不是调用方要释放的);**非白名单返回 NULL**
 */
static const char *relay_cfg_name(uint32_t v)
{
    switch (v & CV_RELAY_MASK) {
    case CV_RELAY_CISS:       return "CISS";
    case CV_RELAY_COSS:       return "COSS";
    case CV_RELAY_CRSS:       return "CRSS";
    case CV_RELAY_RG:         return "RG";
    case CV_RELAY_CISS_DISCH: return "CISS_DIS";
    case CV_RELAY_COSS_DISCH: return "COSS_DIS";
    case CV_RELAY_ALL_OFF:    return "ALL_OFF";
    default:                  return NULL;
    }
}

/**
 * @brief `0x18` 档位独热码 → 顶栏「档位:」显示名。
 * @return 静态字符串;**非独热返回 NULL**
 */
static const char *range_name(uint32_t g)
{
    switch (g & CV_RANGE_MASK) {
    case CV_RANGE_A: return "10Ω";      /* Ω = U+03A9,已在 cn_20 字库中 */
    case CV_RANGE_B: return "100Ω";
    case CV_RANGE_C: return "1kΩ";
    case CV_RANGE_D: return "10kΩ";
    default:         return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* 主入口                                                              */
/* ------------------------------------------------------------------ */

uint32_t RelayView_Refresh(int force)
{
    uint32_t now = cv_millis();
    uint32_t st, cfg, gear, err, fault, key;
    char     buf[24];

    if ((force == 0) && ((int32_t)(now - s_next_ms) < 0)) {
        return s_last_err;                  /* 还没到点,不产生 AXI 事务 */
    }
    s_next_ms = now + RELAY_POLL_MS;

    /* 三个都是普通 RW/RO 寄存器,读无副作用 ------------- */
    st   = cv_reg_read(CV_REG_RELAY_STATUS);
    cfg  = cv_reg_read(CV_REG_RELAY_CFG) & CV_RELAY_MASK;
    gear = cv_reg_read(CV_REG_RANGE_SEL) & CV_RANGE_MASK;
    err   = CV_RELAY_STS_ERR(st);
    fault = CV_RELAY_STS_FAULT(st);
    s_last_err = err;

    /* ---- 回路栏:只在值变了才调 setter,免得白触发重绘 ---- */
    if (cfg != s_last_cfg) {
        const char *name = relay_cfg_name(cfg);

        if (name != NULL) {
            UI_Demo_SetActualPath(name);
        } else {
            /* 读回的**已生效**值理论上必然在白名单里(非法请求根本不改状态);
             * 真走到这儿就是防呆 —— 照实显示原始码,别硬编一个好听的名字。 */
            (void)snprintf(buf, sizeof(buf), "0x%02X", (unsigned)cfg);
            UI_Demo_SetActualPath(buf);
            printf("[relay] ⚠️ RELAY_CFG 回读 0x%02X 不在白名单\n", (unsigned)cfg);
        }
        s_last_cfg = cfg;
    }

    /* ---- 档位栏 ---- */
    if (gear != s_last_gear) {
        const char *name = range_name(gear);

        if (name != NULL) {
            UI_Demo_SetActualGear(name);
        } else {
            (void)snprintf(buf, sizeof(buf), "0x%X", (unsigned)gear);
            UI_Demo_SetActualGear(buf);
            printf("[relay] ⚠️ RANGE_SEL 回读 0x%X 非独热\n", (unsigned)gear);
        }
        s_last_gear = gear;
    }

    /* ---- 异常弹框 ----
     * 🔴 **必须按值去重**。硬件被拒之后 err_code 粘滞多久,从 RTL 侧没有
     *    核实过;若它一直保持非零,不去重就会每 200 ms 弹一次,把界面刷爆。
     *    按 err 去重,对"粘滞"和"单拍脉冲"两种语义都安全:
     *    归零时 key 变 0,记录跟着清掉,下次同样的错还能再弹一次。
     * 🔴 **fault 不参与去重**。`cv_sys.v` 的
     *    `assign sts_status = {16'h0, io_err_code, 5'h0, (io_err_code != 8'h0), io_done, io_busy};`
     *    把 STATUS[2] 定义成 `(err_code != 0)` —— **fault 是 err_code 的派生冗余位**,
     *    不是独立故障。2026-10-10 上一版本写成 `if (fault) {...} else { switch(err) ... }`,
     *    结果 fault 分支恒真、把三条具体文案全部吞掉。板上实测:写 0x1C=0x7F 的真因
     *    是 err=0x03(BUSY),界面却弹「硬件出错,状态未确认」。 */
    key = err;
    if ((key != 0u) && (key != s_last_notice_key)) {
        const char *title;
        const char *msg;

        /* 只按 err_code 分支 —— 这才是「原因」,也是用户要的那句话。 */
        switch (err) {
        case CV_RELAY_ERR_RELAY:
            title = "回路设定非法"; msg = "回路设定非法,硬件已保持原值"; break;
        case CV_RELAY_ERR_GEAR:
            title = "档位编码非法"; msg = "档位编码非法,硬件已保持原值"; break;
        case CV_RELAY_ERR_BUSY:
            title = "继电器处理中"; msg = "上一个动作还没完成,请重试";   break;
        default:
            title = "继电器出错";   msg = "硬件出错,状态未确认";         break;
        }
        UI_Demo_ShowAlert(title, msg);

        printf("[relay] ⚠️ 读回异常:0x04=0x%08X err=0x%02X fault=%u "
               "(RELAY_CFG=0x%02X RANGE_SEL=0x%X)\n",
               (unsigned)st, (unsigned)err, (unsigned)fault,
               (unsigned)cfg, (unsigned)gear);
    }
    s_last_notice_key = key;

    return err;
}
