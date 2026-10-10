/**
 * @file relay_view.h
 * @brief 继电器 / 档位的**只读**回显层 —— 读 PL 寄存器,喂进顶栏
 *
 * 🔴 **本模块只有 `cv_reg_read`,没有 `cv_reg_write`。**
 *    原因见 `relay_view.c` 的文件头 —— 一句话:避免第二个写入方。
 *
 * 用法:主循环每圈调一次 `RelayView_Refresh(0)`(内部自带节流),
 *       上电 / 用户点回路按钮时调 `RelayView_Refresh(1)` 强制立刻刷。
 */

#ifndef RELAY_VIEW_H
#define RELAY_VIEW_H

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

#ifdef __cplusplus
}
#endif

#endif /* RELAY_VIEW_H */
