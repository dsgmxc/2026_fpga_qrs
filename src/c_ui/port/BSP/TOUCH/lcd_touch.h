/**
 * @file lcd_touch.h
 * @brief 触摸数据结构 + 读取接口(PYNQ 版)—— 前序 STM32 同名头文件的替代
 *
 * ────────────────────────────────────────────────────────────────────────
 * ⚠️ **本文件里的类型定义与前序逐字一致,不要改** ——
 *    `ui_demo.c` 靠 `LCD_TouchState` 的多点数组做手势/多指跟踪
 *    (`UI_FindTouchPointById`),字段名或顺序一变,`ui_demo.c` 就得跟着改。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 与前序最大的差别:**实现换了,语义没换**。
 *   前序 `lcd_touch.c` 用 STM32 HAL 的位翻转 I2C 直接跟 GT911 说话;
 *   本工程的 I2C 时序 + GT911 复位/配置/轮询**全在 PL 侧**
 *   (`i2c_master.v` + `gt911_ctrl.v`),PS 侧只读三个寄存器:
 *
 *       0x68 TOUCH_STATUS   →  count / pen_down / err(粘滞) / seq
 *       0x6C TOUCH_PT0      →  触点 0 x/y
 *       0x70 TOUCH_PT1      →  触点 1 x/y
 *
 * 🔴 **返回值语义(与 PL 的保持态配合,照抄前序的约定)**
 *       LCD_TOUCH_OK         这一次读到了**新的一帧**(`seq` 变了)
 *       LCD_TOUCH_NO_UPDATE  `seq` 没变 —— **不是松手**,是"手指没动 /
 *                            还没来新帧"。此时 `*state` 填的是**保持态**,
 *                            调用方**不要**把它当成抬起。
 *       LCD_TOUCH_ERROR_BUS  PL 报了总线错(粘滞 err 位置起)。这是
 *                            **开机以来曾出错**的累计标志,不是"此刻坏了"。
 *
 *    ⚠️ `LCD_TOUCH_NO_UPDATE` 这一条最容易写反 —— 前序 `lcd_touch.c` 里
 *       那段长注释讲的就是它;现在这个判断在 PL 里做,PS 侧只需照约定分流。
 *
 * ⚠️ **PL 只导出 2 个触点,且不含 track_id。**
 *    前序结构里有 `id` 字段(多指时用来跟踪同一根手指),PL 的寄存器没有它
 *    ⇒ 本实现把 `point[i].id` 填成 i(0/1)。**三指以上、或"哪根手指是哪根"
 *    的语义与 STM32 版不同**,这一点如实记在报告里,不要假装等价。
 */

#ifndef LCD_TOUCH_H
#define LCD_TOUCH_H

#include <stdint.h>

#include "BSP/LCD/lcd_panel_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与前序一致:最多 5 点。PL 侧 N_PT 目前是 2 ⇒ 只有前 2 个会被填。 */
#define LCD_TOUCH_MAX_POINTS 5u

#define LCD_TOUCH_OK                 0u
#define LCD_TOUCH_ERROR_UNSUPPORTED  1u
#define LCD_TOUCH_ERROR_BUS          2u
#define LCD_TOUCH_ERROR_CONFIG       3u
#define LCD_TOUCH_ERROR_DATA         4u
#define LCD_TOUCH_NO_UPDATE          5u

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t  id;
    uint8_t  event;
} LCD_TouchPoint;

typedef struct
{
    uint8_t       count;
    LCD_TouchPoint point[LCD_TOUCH_MAX_POINTS];
} LCD_TouchState;

/**
 * @brief 触摸控制器信息(诊断用)。
 *
 * ⚠️ 前序版本里这些字段(芯片 ID / 固件版本 / I2C 地址 / 空闲电平)是
 *    STM32 直接读 GT911 拿到的;在 PYNQ 上 **PS 看不到 I2C 总线**
 *    ⇒ 只有 `communication_errors` 有真值(来自 PL 的 err 粘滞位),
 *    其余字段填 0 并在注释里标明"PL 不导出"。
 *    **不要**把它们印出来当成"芯片 ID 读到了"。
 */
typedef struct
{
    uint16_t library_version;
    uint8_t  chip_id;
    uint8_t  firmware_version;
    uint8_t  vendor_id;
    uint8_t  detected_i2c_address;
    uint8_t  scl_idle_level;
    uint8_t  sda_idle_level;
    uint8_t  interrupt_idle_level;
    uint32_t communication_errors;
} LCD_TouchInfo;

/**
 * @brief 初始化。PL 侧 `gt911_ctrl` 上电就自己跑复位序列了,
 *        所以这里**不做硬件动作**,只读一次寄存器确认链路通。
 */
uint32_t LCD_Touch_Init(void);

/** @brief 读一次触摸状态。返回 `LCD_TOUCH_OK` / `_NO_UPDATE` / `_ERROR_BUS`。 */
uint32_t LCD_Touch_Read(LCD_TouchState *state);

/** @brief 请求 PL 重新走一遍 GT911 复位序列(目前只是软件上标记,见实现)。 */
uint32_t LCD_Touch_ResetController(void);

/** @brief 取诊断信息。 */
const LCD_TouchInfo *LCD_Touch_GetInfo(void);

/* 前序还有 LCD_Touch_GetInterruptLevel() 与 LCD_Touch_RecoverBus():
 * 前者读的是 STM32 的 GPIO,后者做 9 个 SCL 脉冲的总线恢复 ——
 * 这两件事现在都在 PL 里(`gt911_ctrl` 的 INT 走快轮询、`i2c_master` 自带恢复),
 * PS 侧既看不到也不需要,故**不提供**,免得出现"调了但没作用"的空壳函数。 */

#ifdef __cplusplus
}
#endif

#endif /* LCD_TOUCH_H */
