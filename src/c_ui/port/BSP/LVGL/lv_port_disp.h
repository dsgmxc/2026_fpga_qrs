/**
 * @file lv_port_disp.h
 * @brief LVGL 显示设备接口(PYNQ 版)
 *
 * 实现: `app/pynq_disp.c`(替换前序的 `lv_port_disp.c`)
 *
 * 🔴 **与前序版本唯一的接口差异**:`LV_PortDisp_Init` 收**两个**地址。
 *    前序只有一个参数,另一个地址从 STM32 的链接脚本常量
 *    (`LCD_FRAME_BUFFER_0/1_ADDRESS`,即 `lcd_memory_layout.h`)里取。
 *    PYNQ 上没有链接脚本可依赖 —— 帧缓存是**运行时由 Python 用
 *    `pynq.allocate` 分配**的,物理地址只有运行时才知道
 *    ⇒ 必须**显式传进来**。
 *
 * 其余接口与语义保持前序一致(计数器们是排障用的,别删)。
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LV_PORT_DISP_OK     0u
#define LV_PORT_DISP_ERROR  1u

/**
 * @brief 初始化显示驱动。
 *
 * @param fb0_phys 帧缓存 0 的**物理**基址(64 KB 对齐)
 * @param fb1_phys 帧缓存 1 的**物理**基址(64 KB 对齐)
 * @return `LV_PORT_DISP_OK` / `LV_PORT_DISP_ERROR`
 *
 * 采用 **direct_mode**:LVGL 直接往这两块 DDR 帧缓存里画,
 * flush 回调只负责在帧末请求 PL 换址。⇒ 不需要额外的拷贝缓冲。
 */
uint32_t LV_PortDisp_Init(uint32_t fb0_phys, uint32_t fb1_phys);

/** @brief 当前前台缓冲的**物理地址**(排障用)。 */
uint32_t LV_PortDisp_GetFrontAddress(void);

uint32_t LV_PortDisp_GetFlushCount(void);
uint32_t LV_PortDisp_GetFlushErrorCount(void);
uint32_t LV_PortDisp_GetCpuFallbackCount(void);
uint32_t LV_PortDisp_GetFrameSwapCount(void);
uint32_t LV_PortDisp_GetVBlankFallbackCount(void);
uint32_t LV_PortDisp_GetLastError(void);

#ifdef __cplusplus
}
#endif

#endif
