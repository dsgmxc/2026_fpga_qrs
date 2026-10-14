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
 * 🔴 **本模块是 `0x1C` / `0x18` 的【唯一写入方】—— 2026-10-14 改。**
 *
 *   · 原文(2026-10-10)是「**只读,一个寄存器都不写**;写入权归 Python 侧」。
 *     那条约定**没有兑现**,这正是"切换测量项目回路不跟着切"的根因:
 *
 *     | 位置 | 当时的状态 |
 *     |---|---|
 *     | PL `relay_ctrl.v` | ✅ 白名单互锁 / 原子翻转 / 稳定计时,**2026-10-10 上板验证过** |
 *     | `src/python/drivers/relay.py` | ✅ 写得完整、实测过 —— 但 `main.py` 只 import `drivers.board`,**它零调用方** |
 *     | 本文件 | ⛔ 只读回显 |
 *     | `pynq_protocol.c::RequestPath()` | ⛔ 只 printf「只读回读,未改硬件」 |
 *
 *     ⇒ 三处都不写 ⇒ 硬件永远停在复位值 `ALL_OFF(0x10)`。
 *       顶栏那一栏**没在说谎**(它读的是硬件真值),说谎的是**按钮高亮**
 *       —— 按下去只换了个颜色。
 *
 *   · 现在:C 界面是唯一写入方。`drivers/relay.py` **保留**(它是 §0 那个
 *     `struct.pack_into` 缺陷的取证材料),但**界面运行期间不要再用它写
 *     `0x1C`/`0x18`**;`tools/relay_hold.py --watch` 是只读的,不受影响。
 *
 *   · ⚠️ **口径澄清(别把它记成"把 PL 的活挪到 PS")**:C 界面本来就跑在
 *     PS 的 ARM 上。变的是 **PS 的哪个进程写**,不是软硬件划分。
 *
 *   · 顺着本工程既有的习惯:界面显示的是硬件**回读生效值**,不是请求值
 *     (偏压 `bias_apply`、`0x60 LCD_SWAP` 都这么定)。写完之后立刻
 *     `RelayView_Refresh(1)`,所以"请求"与"回读"在用户眼里看不见缝。
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

/* ------------------------------------------------------------------ */
/* 写入侧(2026-10-14 新增)—— 全工程唯一写 0x1C 的地方                */
/* ------------------------------------------------------------------ */

/**
 * 等 `0x04[0] busy` 落下的超时上限(ms)。
 *
 * 硬件稳定时间复位值是 40 ms(`relay_ctrl.v` 的 `SETTLE_RST_MS`),
 * 但**不能按 40 ms 等**:`relay_ctrl.v:66-72` 自己记着,板上 `BOOT.BIN` 把
 * FCLK0 留在 27.78 MHz 时,那个"40 ms"实际是 ~72 ms。
 * `relay.py` 用的是 `max(settle*4, 50)` = 160 ms,这里取同一量级。
 */
#define RELAY_SETTLE_WAIT_MS   200u

/**
 * @brief 等 `0x04[0] busy` 落下。
 * @return 0 = busy 已落;−1 = 超时。
 *
 * ⚠️ **不能借 `adc_sample.c` 的 `wait_until()`** —— 那个是 `static`,且把
 *    寄存器地址硬编码成 `CV_REG_ADC_STS`。本函数的范式抄的是
 *    `pynq_regs.c::cv_lcd_swap_wait()`(既有的"等状态位 + 超时"写法)。
 */
static int relay_wait_idle(uint32_t timeout_ms)
{
    uint32_t t0 = cv_millis();

    do {
        if (CV_RELAY_STS_BUSY(cv_reg_read(CV_REG_RELAY_STATUS)) == 0u) {
            return 0;
        }
        cv_delay_ms(1);          /* 稳定窗 40~72 ms,1 ms 粒度够细 */
    } while ((int)(cv_millis() - t0) < (int)timeout_ms);

    return -1;
}

bool RelayView_ApplyCode(uint32_t code)
{
    const char *name;
    uint32_t    st, err, back;

    code &= CV_RELAY_MASK;
    name  = relay_cfg_name(code);

    /* 白名单闸。⛔ 别把它删掉"省一次查表" —— `relay_ctrl.v` 是白名单互锁,
     *    送进去的非白名单值会被拒(err=0x01)且**旧组合一位不动**,表面看
     *    就是"点了没反应"。在这里拦下来,错误信息才落在发起方这一侧。 */
    if (name == NULL) {
        printf("[relay] ⛔ 组合 0x%02X 不在白名单,拒绝下发\n", (unsigned)code);
        return false;
    }

    /* 1. 先等上一次动作收尾 -------------------------------------------
     *    硬件在 busy 期间**拒收**写入并置 err=0x03(`relay_ctrl.v:291-295`),
     *    所以这一步不能省,否则"上一次还没稳,下一次就吃 BUSY"。 */
    if (relay_wait_idle(RELAY_SETTLE_WAIT_MS) != 0) {
        printf("[relay] ⚠️ 写入前 busy 超过 %u ms 不落,放弃本次切换\n",
               (unsigned)RELAY_SETTLE_WAIT_MS);
        RelayView_Refresh(1);
        return false;
    }

    /* 2. 幂等:已经在目标组合上就什么都不做 ---------------------------
     *    ★ 两个作用,都不是"优化"而是**功能**:
     *      ① 省掉一整个稳定窗(40~72 ms);
     *      ② 扫压会**每个偏压点**都调一次本函数 —— 若每次都重写,
     *         硬件每点重进一次 busy 不稳定窗,曲线会带上继电器抖动的尾巴。
     *    ⚠️ `err == NONE` 也是条件之一:err 是**粘滞**的(只有"被受理的写"
     *       才清它,见 `relay_ctrl.v:301`),粘着旧错时应当照写一次把它清掉。 */
    back = cv_reg_read(CV_REG_RELAY_CFG) & CV_RELAY_MASK;
    err  = CV_RELAY_STS_ERR(cv_reg_read(CV_REG_RELAY_STATUS));
    if (back == code && err == CV_RELAY_ERR_NONE) {
        RelayView_Refresh(1);
        return true;
    }

    /* 3. 写。⛔ **全工程只此一处 `cv_reg_write(CV_REG_RELAY_CFG)`。** ----
     *    C 侧的 `cv_reg_write` 是**单笔** volatile 存储 + 内存屏障
     *    (`pynq_regs.c:78-86`)⇒ **不存在** `drivers/relay.py` §0 里那个
     *    "`struct.pack_into` 发两笔事务 ⇒ 无论写什么都停在 0x00"的缺陷
     *    (`report/measurements/20261010_relay_cfg_write_anomaly.md`)。
     *    那边那套 `WORKAROUND_0x1C`(把稳定时间写 0、PS 自己 sleep)
     *    **这里一条都不需要**,也不要把 `busy` 关掉 —— 它是硬件互锁。 */
    cv_reg_write(CV_REG_RELAY_CFG, code);

    /* 4. 等硬件把倒计时跑完 ------------------------------------------- */
    if (relay_wait_idle(RELAY_SETTLE_WAIT_MS) != 0) {
        printf("[relay] ⚠️ 写 0x1C=0x%02X(%s) 后 busy 超过 %u ms 不落,状态未确认\n",
               (unsigned)code, name, (unsigned)RELAY_SETTLE_WAIT_MS);
        RelayView_Refresh(1);
        return false;
    }

    /* 5. 查错 —— ★ 先读 err,再比读回值 -------------------------------
     *    err 是**原因**,读回值是**现象**。反过来的话,「被拒绝」会表现成
     *    「读回不一致」,日志里就丢掉了 err_code=0x01/0x03 这条线索。 */
    st  = cv_reg_read(CV_REG_RELAY_STATUS);
    err = CV_RELAY_STS_ERR(st);
    if (err != CV_RELAY_ERR_NONE) {
        printf("[relay] ⚠️ 写 0x1C=0x%02X(%s) 被拒:err=0x%02X(读回 0x%02X)\n",
               (unsigned)code, name, (unsigned)err,
               (unsigned)(cv_reg_read(CV_REG_RELAY_CFG) & CV_RELAY_MASK));
        RelayView_Refresh(1);        /* 让弹框按 err_code 说具体原因 */
        return false;
    }

    back = cv_reg_read(CV_REG_RELAY_CFG) & CV_RELAY_MASK;
    if (back != code) {
        /* err 干净而读回不等 —— `relay_ctrl.v` 的因果链在这里断了。
         * 这是**真异常**,要留证,不要拿"重试一次"盖过去。 */
        printf("[relay] ⚠️ 写 0x1C=0x%02X(%s) 读回 0x%02X,err=0(因果链断裂)\n",
               (unsigned)code, name, (unsigned)back);
        RelayView_Refresh(1);
        return false;
    }

    /* 6. 顶栏立刻变,不等那 200 ms 节流 ------------------------------- */
    RelayView_Refresh(1);

    printf("[relay] 回路 → %s(0x%02X)\n", name, (unsigned)code);
    return true;
}

bool RelayView_ApplyPath(uint32_t path)
{
    switch (path) {
    case 0u: return RelayView_ApplyCode(CV_RELAY_CISS);
    case 1u: return RelayView_ApplyCode(CV_RELAY_COSS);
    case 2u: return RelayView_ApplyCode(CV_RELAY_CRSS);
    case 3u: return RelayView_ApplyCode(CV_RELAY_RG);
    default: break;
    }

    /* path == 4 =「全部」:**此处不切**,它不是一条回路。
     * 它是"三条腿各测一次"的编排,切换发生在**每条腿测量开始时**
     * (`pynq_protocol.c` 的 `Process()` 单点分支),那是唯一知道
     * "现在轮到哪条腿"的地方。在这里随便挑一条切,只会让顶栏闪一下。 */
    printf("[relay] 「全部」= 各回路分别测,切换发生在测量时,此处不动继电器\n");
    return false;
}
