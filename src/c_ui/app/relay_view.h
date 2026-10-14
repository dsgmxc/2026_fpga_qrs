/**
 * @file relay_view.h
 * @brief 继电器 / 档位层 —— 读回显(喂顶栏)+ **`0x1C` 的唯一写入方**
 *
 * 🔴 **本模块既有 `cv_reg_read` 也有 `cv_reg_write`(2026-10-14 改)。**
 *    原文是「只有 `cv_reg_read`,避免第二个写入方」;改的原因见
 *    `relay_view.c` 的文件头 —— 一句话:那条约定**没有兑现**,
 *    Python 侧根本没有调用方,于是硬件永远停在 `ALL_OFF`。
 *
 * 用法:
 *   · 主循环每圈调一次 `RelayView_Refresh(0)`(内部自带节流);
 *   · 上电调 `RelayView_Refresh(1)` 强制立刻刷;
 *   · 要**真的**换回路就调 `RelayView_ApplyPath(path)`;
 *   · 复位要回安全态就调 `RelayView_ApplyCode(CV_RELAY_ALL_OFF)`。
 */

#ifndef RELAY_VIEW_H
#define RELAY_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读回 0x04 / 0x18 / 0x1C,刷新顶栏「回路:」「档位:」两栏。
 *
 * 读回值与上次不同才调 UI setter(免得每圈都触发 LVGL 重绘)。
 * `STATUS` 的 err_code / fault 非零且**与上次不同**时,弹一次提示框。
 *
 * @param force 1 = 忽略节流,立刻读一次(上电 / 用户点了按钮)
 * @return 本次读到的 err_code(0x00 = NONE);`force=0` 且被节流时
 *         返回上一次的值,不产生 AXI 事务
 */
uint32_t RelayView_Refresh(int force);

/**
 * @brief **直接**下发一个继电器组合码(白名单码,见 `pynq_regs.h` 的 `CV_RELAY_*`)。
 *
 * 这是全工程唯一写 `0x1C` 的地方。序列(镜像 `drivers/relay.py::apply()`,
 * 那条链 2026-10-10 上板实测过):
 *
 * ```
 * 1. 等 busy 落(≤200 ms)          ← 硬件在忙时【拒收】写,先写必吃 err=0x03
 * 2. 读回 0x1C;若 == code 且 err==0 → 直接返回 true   ★ 幂等
 * 3. cv_reg_write(0x1C, code)
 * 4. 等 busy 落(≤200 ms)
 * 5. 读 0x04:err != 0 → 报假,返回 false
 * 6. 回读 0x1C 比对新码,不等 → 报假,返回 false
 * 7. RelayView_Refresh(1)           ← 顶栏立刻变,不等 200 ms 节流
 * 8. printf("[relay] 回路 → %s(0x%02X)")
 * ```
 *
 * ⚠️ **会阻塞最多 ~400 ms**(两次 200 ms 超时)。调用方要么在主循环里、
 *    要么在本来就被 `AdcSample_CapturePair()` 阻塞的路径上 —— **不要放进
 *    触摸回调**,那会让触摸卡住。现有两个调用点都遵守这条:
 *    `pynq_protocol.c::DisplayProtocol_Process()`(单点)与
 *    `sweep_begin_leg()`(扫压换腿)。
 *
 * @param code 白名单组合码(非白名单直接被拒,旧组合一位不动)
 * @return true  = 已下发**且**读回确认
 *         false = 未生效(超时 / 被拒 / 读回不一致),原因已 printf 出来
 */
bool RelayView_ApplyCode(uint32_t code);

/**
 * @brief 按**测量项目编号**切回路 —— 界面按钮直接调的就是它。
 *
 * @param path 0=CISS 1=COSS 2=CRSS 3=RG(**会真的切继电器**)
 *             4=「全部」→ **不切**,返回 false,只打印一句说明:
 *               它是"三条腿各测一次"的编排,切换发生在每条腿**测量开始时**
 *               (`pynq_protocol.c::Process()`),不在这里。
 * @return 见 `RelayView_ApplyCode()`;`path == 4` 恒 false
 */
bool RelayView_ApplyPath(uint32_t path);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_VIEW_H */
