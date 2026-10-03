/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备(触摸)接口 —— **与前序 STM32 版逐字相同**
 *
 * 📌 接口一行没改,实现换成了 PYNQ 版(`app/pynq_indev.c`,数据来自
 *    `app/pynq_touch.c` 读 PL 寄存器)。
 *    这样 `ui_demo.c` 里 `LV_PortIndev_GetTouchState(&touch_state)` 那一处
 *    **原样可用**,不需要动前序代码。
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#include <stdint.h>

#include "BSP/TOUCH/lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_PORT_INDEV_OK     0u
#define LV_PORT_INDEV_ERROR  1u

uint32_t LV_PortIndev_Init(void);
void LV_PortIndev_Update(const LCD_TouchState *touch_state);
void LV_PortIndev_Release(void);
uint32_t LV_PortIndev_GetTouchState(LCD_TouchState *touch_state);
uint32_t LV_PortIndev_GetReadCount(void);

#ifdef __cplusplus
}
#endif

#endif
