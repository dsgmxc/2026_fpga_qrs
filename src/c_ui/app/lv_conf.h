/**
 * @file lv_conf.h
 * @brief LVGL 8.2.0 配置(PYNQ 版)
 *
 * ────────────────────────────────────────────────────────────────────────
 * 📌 **开关值全部照抄前序 STM32H7 工程的 `Middlewares/LVGL/lv_conf.h`** ——
 *    那一份是在真机上跑通过的,控件集与 `ui_demo.c` 的需求严丝合缝。
 *    自己"顺手打开几个用不上的控件"只会让 A9 上的编译时间变长、
 *    还多一份没验证过的代码路径。
 * ────────────────────────────────────────────────────────────────────────
 *
 * 与前序版本**只有两处**差别(都是因为换了平台,不是喜好):
 *
 *  ① **删掉 `#include "BSP/LCD/lcd_memory_layout.h"`**,并把
 *     `LV_MEM_SIZE = LCD_LVGL_HEAP_SIZE` / `LV_MEM_ADR = LCD_LVGL_HEAP_ADDRESS`
 *     改成 `LV_MEM_CUSTOM 1`。
 *     理由:前序是把 LVGL 的堆**钉死在一块固定的物理 RAM**(链接脚本给的),
 *     因为裸机上没有 malloc。Linux 上没必要 —— 直接用 libc 的 `malloc/free`,
 *     既不用关心堆多大,也不会因为 `LV_MEM_SIZE` 估小了而在运行时炸。
 *
 *  ② 其余一切不变 —— 包括 `LV_COLOR_DEPTH 16` / `LV_COLOR_16_SWAP 0`。
 *     ⚠️ `LV_COLOR_16_SWAP` 若实测发现**整屏颜色对但像素两两互换**才改 1;
 *        那种情况的另一半证据是 `display_probe.py` 的色条也糊。
 *        两处必须同时判,不要只看一处就改。
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   颜色设置
 *====================*/

/* 面板是 RGB565;PL 侧 cv_sys.v 按 {R[4:0], G[5:0], B[4:0]} 展开成 RGB888 */
#define LV_COLOR_DEPTH      16
#define LV_COLOR_16_SWAP    0

/*====================
   内存
 *====================*/

/* ★ PYNQ 改动:用 libc 的 malloc/free,不用 LVGL 自带的静态堆 */
#define LV_MEM_CUSTOM       1
#if LV_MEM_CUSTOM == 0
    /* 只在误把上面改成 0 时才有意义 —— 这里给一个保守值,免得编译不过 */
    #define LV_MEM_SIZE     (192u * 1024u)
#endif
#define LV_MEM_BUF_MAX_NUM  16
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL 设置
 *====================*/

/* 一帧 17.07 ms ⇒ 19 ms 的默认重绘周期比一帧略长,不会追帧追到卡住 */
#define LV_DISP_DEF_REFR_PERIOD 19

/* ⚠️ 必须是 0 —— tick 由 `main.c` 用 CLOCK_MONOTONIC 喂 `lv_tick_inc`。
 *    置 1 会让 LVGL 自己去调一个不存在的 `lv_tick_get_cb`。 */
#define LV_TICK_CUSTOM      0

/* 旋转/双缓冲的临时缓冲上限。前序是 10 KB,照抄 —— 我们不做旋转,
 * 这个值只影响"旋转时能一次处理多大一块",不动它最安全。 */
#define LV_DISP_ROT_MAX_BUF (10u * 1024u)

/*====================
   GPU(全部关闭)
 *====================*/

#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/*====================
   日志与断言
 *====================*/

/* 打开日志会往串口/标准输出狂刷 —— 上板时先关着,排障再临时开 */
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0

/*====================
   其它
 *====================*/

#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0
#define LV_USE_REFR_DEBUG       0
#define LV_SPRINTF_CUSTOM       0
#define LV_SPRINTF_USE_FLOAT    0
#define LV_USE_USER_DATA        1
#define LV_USE_LARGE_COORD      0

/*====================
   字体
 *====================*/

/* ★ 中文字模不在这里 —— `ui_demo.c` 用 `LV_FONT_DECLARE` 声明了
 *   `ui_font_cn_20` / `ui_font_cn_28`,定义在 `ui/ui_font_cn_*.c` 里。 */
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   0
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   0
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK   0
#define LV_FONT_UNSCII_8        0
#define LV_FONT_UNSCII_16       0
#define LV_FONT_CUSTOM_DECLARE
#define LV_FONT_DEFAULT         &lv_font_montserrat_16

/*====================
   文本
 *====================*/

#define LV_USE_BIDI                 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   控件
 *====================*/

#define LV_USE_ARC          0
#define LV_USE_BAR          0
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       1
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMG          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         0
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       0
#define LV_USE_SWITCH       0
#define LV_USE_TEXTAREA     0
#define LV_USE_TABLE        0

/*====================
   扩展控件
 *====================*/

#define LV_USE_ANIMIMG      0
#define LV_USE_CALENDAR     0
#define LV_USE_CALENDAR_HEADER_ARROW     0
#define LV_USE_CALENDAR_HEADER_DROPDOWN  0
#define LV_USE_CHART        1
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          0
#define LV_USE_LIST         0
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_TABVIEW      0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

/*====================
   主题与布局
 *====================*/

/* 前序全关 —— ui_demo.c 自己设样式,不用 LVGL 主题 */
#define LV_USE_THEME_DEFAULT    0
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0
#define LV_USE_FLEX             0
#define LV_USE_GRID             0

/*====================
   例子与 demo
 *====================*/

#define LV_BUILD_EXAMPLES           0
#define LV_USE_DEMO_WIDGETS         0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK       0
#define LV_USE_DEMO_STRESS          0
#define LV_USE_DEMO_MUSIC           0

/* 前序的扫压页用 LVGL 内置的 1 位画布二维码组件 */
#define LV_USE_QRCODE       1

/*====================
   第三方库(前序未启用的一律关掉)
 *====================*/

#define LV_USE_PNG      0
#define LV_USE_BMP      0
#define LV_USE_SJPG     0
#define LV_USE_GIF      0
#define LV_USE_FREETYPE 0
#define LV_USE_RLOTTIE  0
#define LV_USE_FFMPEG   0

#endif /* LV_CONF_H */
