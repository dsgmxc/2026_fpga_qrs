/**
 * @file pynq_indev.c
 * @brief LVGL 输入设备驱动(PYNQ 版)
 *
 * 接口在 `port/BSP/LVGL/lv_port_indev.h`(**与前序 STM32 版逐字相同**)。
 * 数据来自 `pynq_touch.c`(读 PL 寄存器),喂进来的动作在 `main.c`。
 *
 * ────────────────────────────────────────────────────────────────────────
 * 【为什么这里只做"缓存转发",不自己读寄存器】
 *   与前序 `lv_port_indev.c` 的分工一致:
 *     · 生产者(读硬件、判新旧、处理错误)= pynq_touch.c
 *     · 消费者(LVGL 回调)= 本文件
 *   分开的好处是触摸的时序问题不会渗进 UI 逻辑 —— 而且 `ui_demo.c`
 *   会反过来调 `LV_PortIndev_GetTouchState()` 拿当前触点做多指手势判断,
 *   所以必须有一份**与 LVGL 看到的一模一样**的缓存。
 *
 *   ⚠️ LVGL 的 read_cb 是**在 lv_timer_handler() 里被调的**,必须**非阻塞**。
 *      本文件里一行寄存器读都没有,就是为了这个:寄存器读走 AXI,
 *      慢的时候会让整个 UI 掉帧。
 * ────────────────────────────────────────────────────────────────────────
 */

#include "BSP/LVGL/lv_port_indev.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

/* ------------------------------------------------------------------ */
/* 给 read_cb 用的缓存(故意做成 volatile:回调与主循环不在同一层)         */
/* ------------------------------------------------------------------ */
static volatile lv_coord_t g_touch_x;
static volatile lv_coord_t g_touch_y;
static volatile int        g_touch_pressed;

static LCD_TouchState g_touch_state;      /* 最近一次 Update 进来的完整状态 */
static uint32_t       g_read_count;
static uint32_t       g_out_of_range;     /* 越界点数,给 main.c 打一行提示 */

/* ------------------------------------------------------------------ */
/* LVGL 回调:只读缓存,不做任何 I/O                                     */
/* ------------------------------------------------------------------ */
static void indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    data->point.x = g_touch_x;
    data->point.y = g_touch_y;
    data->state   = g_touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    g_read_count++;
}

/* ------------------------------------------------------------------ */
uint32_t LV_PortIndev_Init(void)
{
    static lv_indev_drv_t indev_drv;

    g_touch_x = g_touch_y = 0;
    g_touch_pressed = 0;
    g_read_count = 0;
    g_out_of_range = 0;
    memset(&g_touch_state, 0, sizeof(g_touch_state));

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = indev_read_cb;

    if (lv_indev_drv_register(&indev_drv) == NULL) {
        return LV_PORT_INDEV_ERROR;
    }

    printf("[indev] 指针设备已注册(坐标来自 PL 的 0x68/0x6C/0x70)\n");
    return LV_PORT_INDEV_OK;
}

/* ------------------------------------------------------------------ */
void LV_PortIndev_Update(const LCD_TouchState *touch_state)
{
    if (touch_state == NULL) {
        return;
    }

    if (touch_state->count != 0) {
        memcpy(&g_touch_state, touch_state, sizeof(g_touch_state));

        /* LVGL 只认一个点 —— 用触点 0。多点手势由 ui_demo.c 自己通过
         * LV_PortIndev_GetTouchState() 拿完整状态去判。 */
        g_touch_x = (lv_coord_t)touch_state->point[0].x;
        g_touch_y = (lv_coord_t)touch_state->point[0].y;
        g_touch_pressed = 1;

        /* ⚠️ 越界**不夹紧、不缩放**,只计数 + 提示一次。
         *    夹紧会让"GT911 配置表分辨率写错"变成"能用了但点不准",
         *    把可修的问题变成一个查不出的问题。 */
        if (g_touch_state.point[0].x >= LCD_PIXEL_WIDTH ||
            g_touch_state.point[0].y >= LCD_PIXEL_HEIGHT) {
            if (g_out_of_range == 0) {
                fprintf(stderr,
                        "[indev] 触点越界:(%u,%u),面板 %dx%d ⇒ "
                        "查 GT911 配置表的分辨率字段,不要在软件里缩放\n",
                        g_touch_state.point[0].x, g_touch_state.point[0].y,
                        LCD_PIXEL_WIDTH, LCD_PIXEL_HEIGHT);
            }
            g_out_of_range++;
        }
    } else {
        /* count == 0 ⇒ 明确释放(PL 只在"新帧且点数 0"时才给它 0) */
        memset(&g_touch_state, 0, sizeof(g_touch_state));
        g_touch_pressed = 0;
    }
}

/* ------------------------------------------------------------------ */
void LV_PortIndev_Release(void)
{
    memset(&g_touch_state, 0, sizeof(g_touch_state));
    g_touch_pressed = 0;
    /* ⚠️ 坐标**保留**:LVGL 的抬起事件要带原坐标,
     *    否则点击判定会落到 (0,0),按钮收不到 release。 */
}

/* ------------------------------------------------------------------ */
uint32_t LV_PortIndev_GetTouchState(LCD_TouchState *touch_state)
{
    if (touch_state == NULL) {
        return LV_PORT_INDEV_ERROR;
    }
    memcpy(touch_state, &g_touch_state, sizeof(*touch_state));
    return LV_PORT_INDEV_OK;
}

uint32_t LV_PortIndev_GetReadCount(void)
{
    return g_read_count;
}

/* 排障用(不进 lv_port_indev.h —— 那是前序的接口,不加新函数污染它) */
uint32_t LV_PortIndev_GetOutOfRangeCount(void)
{
    return g_out_of_range;
}
