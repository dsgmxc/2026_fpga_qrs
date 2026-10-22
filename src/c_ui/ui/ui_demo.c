#include "ui_demo.h"

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "BSP/LCD/lcd_panel_config.h"
#include "BSP/LVGL/lv_port_indev.h"
#include "display_protocol.h"
#include "engineering_parameters.h"
#include "lvgl.h"
#include "sweep_interpolation.h"
/* 🆕 2026-10-06:「系统复位」页新增的测量参数控件(激励频率 / 采样率 / 相位)。
 *    这是本文件相对前序工程的**唯一新增依赖** —— 界面控件调用它,其余四页未动。 */
#include "measure_config.h"

LV_FONT_DECLARE(ui_font_cn_20);
LV_FONT_DECLARE(ui_font_cn_28);

#define UI_COLOR_BACKGROUND       0x07111fu
#define UI_COLOR_SURFACE          0x0d1b2au
#define UI_COLOR_SURFACE_LIGHT    0x13263au
#define UI_COLOR_BORDER           0x24415cu
#define UI_COLOR_TEXT             0xf4f8fcu
#define UI_COLOR_TEXT_MUTED       0x8fa8c0u
#define UI_COLOR_STATUS           0xc6d7e8u
#define UI_COLOR_BLUE             0x2f80ffu
#define UI_COLOR_CYAN             0x22d3eeu
#define UI_COLOR_GREEN            0x2ce6a6u
#define UI_COLOR_ORANGE           0xffb547u
#define UI_COLOR_RED              0xff5d73u
#define UI_COLOR_PURPLE           0xa78bfau
#define UI_COLOR_PRESSED          0x1d3650u

#define UI_COMMON_HEADER_HEIGHT   52
#define UI_PAGE_MARGIN            24
#define UI_SWEEP_MAX_POINTS       UI_DEMO_SWEEP_MAX_POINTS
#define UI_SWEEP_RENDER_POINTS    401u
#define UI_SWEEP_X_MIN_LOG_MILLI  (-1000)
#define UI_SWEEP_X_MAX_LOG_MILLI  2000
#define UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI 0
#define UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI 4000
#define UI_SWEEP_DRAWER_WIDTH     218
#define UI_SWEEP_LOG_MINOR_COUNT  8u
#define UI_SWEEP_PROBE_RADIUS     7
#define UI_SWEEP_PROBE_HIT_RADIUS 40
#define UI_SWEEP_MIN_X_SPAN_LOG_MILLI 250
#define UI_SWEEP_MIN_Y_SPAN_LOG_MILLI 250
#define UI_SWEEP_PINCH_MIN_DISTANCE_PX 24.0f
#define UI_SWEEP_GESTURE_UPDATE_MS 30u
#define UI_SWEEP_DOUBLE_TAP_MS    350u
#define UI_SWEEP_TAP_MOVE_LIMIT_PX 12
#define UI_SWEEP_CHART_REFRESH_MS     80u
#define UI_SWEEP_PARAMETER_COUNT   6u
#define UI_TRANSFER_QR_SIZE        288
#define UI_TRANSFER_URL_CAPACITY   3072u
#define UI_RESULT_PROBE_HINT \
    "按住曲线查看该点数值 · 左右滑动连续查看"

#define UI_BIAS_MIN_TENTHS        1u
#define UI_BIAS_MAX_TENTHS        1000u

#define UI_KEYPAD_DECIMAL         10u
#define UI_KEYPAD_BACKSPACE       11u
#define UI_KEYPAD_CLEAR           12u
#define UI_KEYPAD_CANCEL          13u
#define UI_KEYPAD_MINIMUM         14u
#define UI_KEYPAD_CONFIRM         15u

typedef enum
{
    UI_PAGE_HOME = 0u,
    UI_PAGE_INITIALIZE = 1u,
    UI_PAGE_MEASURE = 2u,     /* 原 UI_PAGE_SINGLE ⊕ UI_PAGE_MEASURE_ALL 合并 */
    UI_PAGE_SWEEP = 3u,
    UI_PAGE_RESULTS = 4u      /* 投影视图:进入时重绘,不持有数据 */
} UI_Page;

typedef enum
{
    UI_BIAS_TARGET_SINGLE = 0u,   /* 合并页:单回路与「全部」共用同一偏压 */
    UI_BIAS_TARGET_SWEEP = 1u,
    UI_BIAS_TARGET_ANALYSIS = 2u
} UI_BiasTarget;

#define UI_DISPLAY_PATH_ALL       4u

typedef struct
{
    uint32_t action_code;
    const char *status_text;
} UI_Action;

typedef struct
{
    uint8_t active;
    uint8_t block_single_touch;
    uint8_t first_id;
    uint8_t second_id;
    float start_distance;
    float anchor_x_log_milli;
    float anchor_y_log_milli;
    int32_t start_x_min_log_milli;
    int32_t start_x_max_log_milli;
    int32_t start_y_min_log_milli;
    int32_t start_y_max_log_milli;
    uint32_t last_update_tick;
} UI_SweepGestureState;

typedef struct
{
    uint8_t active;
    lv_point_t start_point;
    uint32_t start_tick;
} UI_SweepPageSwipeState;

static lv_obj_t *g_status_label;
static lv_obj_t *g_modal_overlay;
static lv_obj_t *g_common_header;
static lv_obj_t *g_header_back_button;
static lv_obj_t *g_header_title_label;
static lv_obj_t *g_header_connection_label;
static lv_obj_t *g_header_bias_label;
static lv_obj_t *g_header_path_label;
static lv_obj_t *g_header_gear_label;        /* 🆕 2026-10-10 顶栏「档位:」栏 */
static lv_obj_t *g_header_operation_label;
static lv_obj_t *g_page_containers[5];
static uint8_t g_page_built[5];
static lv_obj_t *g_single_path_buttons[5];   /* CISS/COSS/CRSS/RG/全部 */
static lv_obj_t *g_sweep_path_buttons[4];
/* 🆕 2026-10-15:测量页的挡位按钮 A/B/C/D(10Ω/100Ω/1kΩ/10kΩ)。
 * ⚠️ 下标 = 独热码的**位号**,不是 `CV_RANGE_*` 的码值 ——
 *    高亮时用 `UI_GearButtonIndex(g_ui_gear)` 换算,别直接拿码值当下标。
 * 🔴 **挡位支路数一律走 `UI_GEAR_COUNT`**,照文件头那条纪律(`UI_INIT_DDS_COUNT`
 *    踩过的坑):数组维度、按钮数、高亮循环三处都由同一个数导出,
 *    改表就自动改到各处,不靠人记。下面还钉了一条编译期断言。 */
#define UI_GEAR_COUNT  4u
static lv_obj_t *g_gear_buttons[UI_GEAR_COUNT];
static lv_obj_t *g_single_bias_value_label;
static lv_obj_t *g_sweep_bias_value_label;
static lv_obj_t *g_single_result_label;
static lv_obj_t *g_single_result_note_label;
static lv_obj_t *g_single_result_panel;      /* 测量页:单回路结果视图 */
static lv_obj_t *g_measure_all_result_panel; /* 测量页:「全部」三路结果视图 */
static lv_obj_t *g_all_result_labels[3];
static lv_obj_t *g_sweep_chart;
static lv_obj_t *g_sweep_progress_label;
static lv_obj_t *g_sweep_progress_bar;
static lv_obj_t *g_sweep_progress_indicator;
static lv_obj_t *g_sweep_control_panel;
static lv_obj_t *g_sweep_settings_button;
static lv_timer_t *g_sweep_chart_refresh_timer;
static lv_obj_t *g_sweep_curve_page;
/* 结果页「扫压结果」页签上的分析电压控件(自 2026-10-17 从独立参数页搬来) */
static lv_obj_t *g_results_analysis_value_label;
static lv_obj_t *g_results_parameter_value_labels[UI_SWEEP_PARAMETER_COUNT];
static lv_obj_t *g_results_analysis_track;
static lv_obj_t *g_results_analysis_knob;
static lv_obj_t *g_results_probe_label;
/* 结果页「数据传输」页签的实测数据二维码 */
static lv_obj_t *g_results_transfer_qrcode;
static lv_obj_t *g_results_transfer_status_label;
static lv_obj_t *g_results_transfer_payload_label;
static lv_obj_t *g_results_transfer_model_label;
static char g_transfer_url[UI_TRANSFER_URL_CAPACITY];
static uint32_t g_transfer_acv1_nodes;
static uint32_t g_transfer_scv1_points;
static uint8_t g_transfer_has_payload;
static uint8_t g_transfer_qr_dirty;
/* 🆕 2026-10-06「系统复位」页的测量参数控件与常驻状态行(见 UI_ShowInitialize) */
static lv_obj_t *g_init_status_label;        /* 复位按钮正下方的常驻一行        */
/* 🔴 **档位数一律走这两个宏**(2026-10-08)。**
 *    原先每个数都在 5 处重复(回调守卫 / 档位表 / 按钮表 / 版面 row_count /
 *    高亮),**没有任何一处能自动发现别处的漂移** —— 2026-10-08 上板踩到的
 *    "65MSPS 那颗按钮点了没反应"就是守卫停在旧档位数导致的**静默 return**。
 *    收成宏之后,数组维度与守卫都由同一个数导出,这类漂移在编译期就现形。 */
#define UI_INIT_DDS_COUNT    3u   /* 激励频率档位(0x80)—— 10k/100k/1M */
#define UI_INIT_PHASE_COUNT  3u   /* 相位:−1 / 居中 / +1                    */

static lv_obj_t *g_init_dds_buttons[UI_INIT_DDS_COUNT];
/* ⛔ **采样率行已于 2026-10-08 删除** —— 采样率统一固定 65 MSPS 不可改,
 *    留一行"点了没反应"的按钮只会重现本次刚修掉的那个症状。
 *    当前采样率仍可见:常驻状态行(`MeasureConfig_FormatLine()`)里有。 */
static lv_obj_t *g_init_phase_buttons[UI_INIT_PHASE_COUNT];
static lv_chart_series_t *g_sweep_series[3];
static lv_obj_t *g_keypad_overlay;
static lv_obj_t *g_keypad_title_label;
static lv_obj_t *g_keypad_value_label;
static lv_obj_t *g_keypad_note_label;
static uint32_t g_ui_click_count;
static uint32_t g_ui_last_button;
static uint32_t g_ui_current_page;
static uint32_t g_ui_reset_request_count;
static uint32_t g_ui_single_path;
static uint32_t g_ui_sweep_path;
/* 🆕 2026-10-15:当前**已生效**的挡位独热码。
 * 🔴 初值 `0xFFFFFFFF` 是**哨兵**(= "还没从硬件回读过"),不是"未选挡"——
 *    `0x0` 是合法的"什么都不知道"的另一种写法,而 RTL 明确把全 0 判为非法,
 *    两者混在一起会让"上电那一瞬间"和"硬件真被写成全 0 了"分不开。
 *    哨兵值下**一颗按钮都不高亮**,顶栏同样是空的,不会替硬件先说一句话。
 * 写入方:`UI_Demo_SetActualGear()` —— 那是唯一入口,数据源是 PL 回读。 */
static uint32_t g_ui_gear = 0xFFFFFFFFu;
static uint32_t g_ui_single_bias_tenths;
static uint32_t g_ui_sweep_end_tenths;
static uint32_t g_ui_analysis_voltage_tenths;
static uint32_t g_ui_analysis_voltage_max_tenths;
static uint32_t g_ui_display_bias_tenths;
static uint32_t g_ui_display_path;
static uint8_t g_ui_immediate_refresh_enabled;
static UI_BiasTarget g_keypad_target;
static char g_keypad_buffer[8];
static char g_ui_actual_bias[16];
static char g_ui_actual_path[16];
static char g_ui_actual_gear[16];            /* 🆕 顶栏「档位:」栏的文字 */
static float g_sweep_voltage[3][UI_SWEEP_MAX_POINTS];
static float g_sweep_capacitance[3][UI_SWEEP_MAX_POINTS];
static float g_sweep_parallel_capacitance[3][UI_SWEEP_MAX_POINTS];
static float g_sweep_dissipation[3][UI_SWEEP_MAX_POINTS];
static uint8_t g_sweep_model_valid[3][UI_SWEEP_MAX_POINTS];
static uint8_t g_sweep_valid[3][UI_SWEEP_MAX_POINTS];
static uint32_t g_sweep_total_points[3];
static SweepInterpolationCurve g_sweep_cs_curve[3];
static SweepInterpolationCurve g_sweep_cp_curve[3];
static SweepInterpolationCurve g_sweep_d_curve[3];
static uint8_t g_sweep_interpolation_ready[3];
static uint32_t g_sweep_visible_mask;
static uint32_t g_sweep_probe_path;
static uint32_t g_sweep_probe_index;
static float g_sweep_probe_voltage;
static float g_sweep_probe_cs;
static float g_sweep_probe_cp;
static float g_sweep_probe_d;
static uint8_t g_sweep_probe_model_valid;
static uint8_t g_sweep_probe_interpolated;
static uint8_t g_sweep_probe_valid;
static uint8_t g_sweep_probe_tracking;
static uint8_t g_sweep_inspection_ready;
static uint8_t g_sweep_last_tap_valid;
static uint8_t g_sweep_single_touch_moved;
static uint32_t g_sweep_last_tap_tick;
static lv_point_t g_sweep_touch_press_point;
static int32_t g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
static int32_t g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
static int32_t g_sweep_y_min_log_milli =
    UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI;
static int32_t g_sweep_y_max_log_milli =
    UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI;
static int32_t g_sweep_full_y_min_log_milli =
    UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI;
static int32_t g_sweep_full_y_max_log_milli =
    UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI;
static UI_SweepGestureState g_sweep_gesture;
static uint8_t g_sweep_chart_dirty;
static uint8_t g_sweep_analysis_initialized;
static float g_latest_rg_ohm;
static uint8_t g_latest_rg_valid;
/* 探针归属:0 = 扫压页,1 = 结果页「扫压结果」。
   两张图共用一份探针数据,但只允许**当前操作的那张图**画十字线。 */
static lv_obj_t *g_sweep_probe_chart;
static uint32_t g_sweep_probe_owner;
/* 结果页图表 / 摘要:真正的定义在结果页那一节,但触摸回调与复位函数
   比它更早,这里先做文件作用域声明。 */
static lv_obj_t *g_results_chart;
static lv_obj_t *g_results_summary_label;

static const char g_transfer_demo_url[] =
    "https://fox4327.github.io/ai-cv2spice-web/";

static const uint32_t g_sweep_series_colors[] =
    {0xd65ab1u, 0xe5225bu, 0x8c971du};

static const UI_Action g_single_path_actions[] =
{
    {210u, "当前选择:CISS"},
    {211u, "当前选择:COSS"},
    {212u, "当前选择:CRSS"},
    {213u, "当前选择:RG"},
    {214u, "当前选择:全部"}
};

static const UI_Action g_sweep_path_actions[] =
{
    {310u, "扫压回路:CISS"},
    {311u, "扫压回路:COSS"},
    {312u, "扫压回路:CRSS"},
    {313u, "扫压回路:全部"}
};

/* 🆕 2026-10-15:挡位按钮。动作码 **215–218** —— 紧挨着回路选择那一段
 * (210–214),两者语义相邻(都是"测量设置"),且 215 起在原表里是空的。
 * ⚠️ 文案里的 Ω 是 U+03A9,已在 `ui_font_cn_20` 子集里(顶栏「档位:」栏
 *    从 2026-10-10 起一直在用它)。⛔ 加新字符前先核字库,缺字**静默**显示空白。 */
/* 按钮上的字(`g_gear_texts`)与顶栏状态提示(`g_gear_actions[].status_text`)
 * **分开写**,照回路那组的做法(`path_names` vs `g_single_path_actions`):
 * 按钮要短(半宽 126 px),提示要完整。 */
static const char *const g_gear_texts[] =
{
    "A 10Ω", "B 100Ω", "C 1kΩ", "D 10kΩ"
};
static const UI_Action g_gear_actions[] =
{
    {215u, "状态:档位 A(10Ω)"},
    {216u, "状态:档位 B(100Ω)"},
    {217u, "状态:档位 C(1kΩ)"},
    {218u, "状态:档位 D(10kΩ)"}
};

/* 🔴 **编译期把两张表钉到 `UI_GEAR_COUNT` 上。**
 *    少了项:第 4 颗按钮拿到空指针动作 / 越界文字 —— 崩,或者静默空白;
 *    多了项:多出来那项永远点不到。**两种编译器都不报** —— 除非这里报。
 *    这正是文件头那条「档位数一律走宏」纪律要防的那类漂移
 *    (2026-10-08「65MSPS 那颗按钮点了没反应」就是同一个病)。 */
_Static_assert(sizeof(g_gear_texts) / sizeof(g_gear_texts[0]) == UI_GEAR_COUNT,
               "g_gear_texts 的表长必须等于 UI_GEAR_COUNT");
_Static_assert(sizeof(g_gear_actions) / sizeof(g_gear_actions[0]) == UI_GEAR_COUNT,
               "g_gear_actions 的表长必须等于 UI_GEAR_COUNT");

static const UI_Action g_single_measure_action =
    {220u, "状态:单点测量指令发送完成"};
static const UI_Action g_sweep_start_action =
    {320u, "状态:扫压指令发送完成"};
static const UI_Action g_sweep_stop_action =
    {321u, "状态:停止扫压指令发送完成"};

static void UI_ShowHome(void);
static void UI_ShowInitialize(void);
static void UI_ShowMeasure(void);
static void UI_ShowSweep(void);
static void UI_ShowResults(void);
static void UI_ResultsRefresh(void);
static void UI_UpdateSinglePathStyle(void);
static void UI_UpdateGearStyle(void);
static void UI_UpdateMeasureResultMode(void);
static void UI_UpdateSweepPathStyle(void);
static void UI_UpdateSingleBiasLabel(void);
static void UI_UpdateSweepBiasLabel(void);
static void UI_UpdateCommonStatus(void);
static void UI_CreateBiasKeypad(void);
/* 🆕 2026-10-06「系统复位」页的测量参数控件 */
static void UI_UpdateInitConfigStyle(void);
static void UI_InitDdsEvent(lv_event_t *event);
static void UI_InitPhaseEvent(lv_event_t *event);
static void UI_ActivatePage(UI_Page page);
static void UI_RebuildSweepChart(void);
static void UI_SetSweepProgress(uint32_t progress_permille);
static void UI_SetSweepDrawerOpen(bool open);
static void UI_SweepDrawerEvent(lv_event_t *event);
static void UI_DrawSweepLogGrid(lv_obj_t *chart,
                                lv_draw_ctx_t *draw_context);
static void UI_SweepChartDrawEvent(lv_event_t *event);
static void UI_SweepChartProbeDrawEvent(lv_event_t *event);
static void UI_SweepChartTouchEvent(lv_event_t *event);
static void UI_ResetSweepProbe(void);
static void UI_ApplySweepViewport(void);
static void UI_ResetSweepViewport(void);
static void UI_ResetSweepInterpolation(uint32_t path);
static void UI_BuildSweepInterpolation(uint32_t path);
static void UI_RequestSweepChartRefresh(void);
static void UI_SweepChartRefreshTimer(lv_timer_t *timer);
static void UI_UpdateSweepAnalysisCard(void);
static void UI_UpdateSweepAnalysisRange(void);
static void UI_UpdateTransferQRCode(void);
static bool UI_IsSweepDataComplete(void);
static void UI_ResultsSweepTouchEvent(lv_event_t *event);
static void UI_BackEvent(lv_event_t *event);

static void UI_RecordAction(uint32_t action_code)
{
    ++g_ui_click_count;
    g_ui_last_button = action_code;
}

static void UI_UpdateSinglePathStyle(void)
{
    static const uint32_t accent_colors[] =
        {UI_COLOR_BLUE, UI_COLOR_GREEN, UI_COLOR_ORANGE, UI_COLOR_PURPLE,
         UI_COLOR_CYAN};
    uint32_t index;

    for (index = 0u; index < 5u; ++index)
    {
        lv_obj_t *button = g_single_path_buttons[index];

        if (button == NULL)
        {
            continue;
        }

        if (index == g_ui_single_path)
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(accent_colors[index]),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(accent_colors[index]),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(UI_COLOR_SURFACE),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(UI_COLOR_BORDER),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
        }
    }

    UI_UpdateMeasureResultMode();
}

/* ------------------------------------------------------------------ */
/* 🆕 2026-10-15:挡位按钮的高亮                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief 挡位独热码 → 按钮下标。
 *
 * 独热码是 `1<<n`,下标就是 `n` —— 但**别直接把码值当下标用**:
 * `CV_RANGE_B` 是 2,而 B 挡的按钮下标是 1。两处混用能编译、能跑,
 * 只是高亮永远差一位,是典型的静默错位。
 *
 * ⛔ 非独热(含全 0,以及"还没回读过"的哨兵 `0xFFFFFFFF`)返回
 *    `UI_GEAR_COUNT` —— 一个**保证越界**的值,调用方据此一颗都不高亮。
 *    别让它返回 0:那会把"不知道"画成"A 挡",正是本模块要消灭的那类谎话。
 */
static uint32_t UI_GearButtonIndex(uint32_t onehot)
{
    uint32_t bits = onehot & 0xFu;
    uint32_t index = 0u;

    if (bits == 0u) {
        return UI_GEAR_COUNT;              /* 全 0 = 硬件判为非法的组合 */
    }
    while ((bits & 1u) == 0u) {
        bits >>= 1;
        ++index;
        if (index >= UI_GEAR_COUNT) {
            return UI_GEAR_COUNT;          /* 不是恰好一位(或高位有残留) */
        }
    }
    return (bits == 1u) ? index : UI_GEAR_COUNT;
}

static void UI_UpdateGearStyle(void)
{
    /* 四挡各自的强调色。与回路按钮那组**故意不同** —— 回路那组有 5 颗、
     * 挡位只有 4 颗,若共用一组色,两组按钮并排时"颜色相同"会被误读成
     * "这两个是一回事"。 */
    static const uint32_t accent_colors[UI_GEAR_COUNT] =
        {UI_COLOR_CYAN, UI_COLOR_GREEN, UI_COLOR_ORANGE, UI_COLOR_PURPLE};
    uint32_t active = UI_GearButtonIndex(g_ui_gear);
    uint32_t index;

    for (index = 0u; index < UI_GEAR_COUNT; ++index)
    {
        lv_obj_t *button = g_gear_buttons[index];

        if (button == NULL)
        {
            continue;
        }

        if (index == active)
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(accent_colors[index]),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(accent_colors[index]),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(UI_COLOR_SURFACE),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(UI_COLOR_BORDER),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
        }
    }
}

/*
 * 测量页的结果区有两种视图,取决于「测量回路」选的是单路还是「全部」:
 *   单路  → 大号单组读数(CS/CP/D)
 *   全部  → 三张卡片并排(CISS / COSS / CRSS)
 * 两个面板常驻,只切 HIDDEN 标志,不重建控件。
 */
static void UI_UpdateMeasureResultMode(void)
{
    if ((g_single_result_panel == NULL) ||
        (g_measure_all_result_panel == NULL))
    {
        return;
    }

    if (g_ui_single_path == UI_DISPLAY_PATH_ALL)
    {
        lv_obj_add_flag(g_single_result_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_measure_all_result_panel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(g_single_result_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_measure_all_result_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void UI_UpdateSweepPathStyle(void)
{
    static const uint32_t accent_colors[] =
        {UI_COLOR_BLUE, UI_COLOR_GREEN, UI_COLOR_ORANGE, UI_COLOR_PURPLE};
    uint32_t index;

    for (index = 0u; index < 4u; ++index)
    {
        lv_obj_t *button = g_sweep_path_buttons[index];

        if (button == NULL)
        {
            continue;
        }

        if (index == g_ui_sweep_path)
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(accent_colors[index]),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(accent_colors[index]),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(UI_COLOR_SURFACE),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(UI_COLOR_BORDER),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
        }
    }
}

static void UI_UpdateSingleBiasLabel(void)
{
    if (g_single_bias_value_label != NULL)
    {
        lv_label_set_text_fmt(
            g_single_bias_value_label,
            "偏压设置:%lu.%lu V",
            (unsigned long)(g_ui_single_bias_tenths / 10u),
            (unsigned long)(g_ui_single_bias_tenths % 10u));
    }
}

static void UI_UpdateSweepBiasLabel(void)
{
    if (g_sweep_bias_value_label != NULL)
    {
        lv_label_set_text_fmt(
            g_sweep_bias_value_label,
            "终止电压:%lu.%lu V",
            (unsigned long)(g_ui_sweep_end_tenths / 10u),
            (unsigned long)(g_ui_sweep_end_tenths % 10u));
    }
}

static void UI_UpdateCommonStatus(void)
{
    if (g_header_bias_label != NULL)
    {
        lv_label_set_text_fmt(g_header_bias_label,
                              "偏压:%s",
                              g_ui_actual_bias);
    }

    if (g_header_path_label != NULL)
    {
        lv_label_set_text_fmt(g_header_path_label,
                              "回路:%s",
                              g_ui_actual_path);
    }

    if (g_header_gear_label != NULL)
    {
        lv_label_set_text_fmt(g_header_gear_label,
                              "档位:%s",
                              g_ui_actual_gear);
    }
}

static lv_obj_t *UI_CreatePanel(lv_obj_t *parent,
                                lv_coord_t x,
                                lv_coord_t y,
                                lv_coord_t width,
                                lv_coord_t height,
                                uint32_t color,
                                lv_coord_t radius)
{
    lv_obj_t *panel = lv_obj_create(parent);

    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel,
                                  lv_color_hex(UI_COLOR_BORDER),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);

    return panel;
}

static lv_obj_t *UI_CreateText(lv_obj_t *parent,
                               const char *text,
                               const lv_font_t *font,
                               uint32_t color,
                               lv_coord_t x,
                               lv_coord_t y,
                               lv_coord_t width,
                               lv_coord_t height,
                               lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);

    return label;
}

static lv_obj_t *UI_CreateButton(lv_obj_t *parent,
                                 const char *text,
                                 uint32_t color,
                                 lv_coord_t x,
                                 lv_coord_t y,
                                 lv_coord_t width,
                                 lv_coord_t height,
                                 lv_event_cb_t event_callback,
                                 void *user_data)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(color),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(UI_COLOR_PRESSED),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(button,
                                  lv_color_hex(UI_COLOR_BORDER),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);

    if (event_callback != NULL)
    {
        lv_obj_add_event_cb(button,
                            event_callback,
                            LV_EVENT_CLICKED,
                            user_data);
    }

    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width - 20);
    lv_obj_set_style_text_font(label, &ui_font_cn_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label,
                                lv_color_hex(UI_COLOR_TEXT),
                                LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return button;
}

static void UI_FormatSweepLogTick(int32_t log_milli,
                                  bool voltage_axis,
                                  char *text,
                                  uint32_t text_length)
{
    float value;
    float display_value;
    uint32_t scale;
    uint32_t scaled_value;
    uint32_t integer_value;
    uint32_t fraction_value;
    const char *suffix = "";

    if ((text == NULL) || (text_length == 0u))
    {
        return;
    }

    value = powf(10.0f, (float)log_milli / 1000.0f);
    if (!isfinite(value) || (value <= 0.0f))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    display_value = value;
    if (!voltage_axis && (display_value >= 1000000.0f))
    {
        display_value /= 1000000.0f;
        suffix = "M";
    }
    else if (!voltage_axis && (display_value >= 1000.0f))
    {
        display_value /= 1000.0f;
        suffix = "k";
    }

    if (display_value < 1.0f)
    {
        scale = 100u;
    }
    else if (display_value < 10.0f)
    {
        scale = 10u;
    }
    else
    {
        scale = 1u;
    }
    if (display_value > ((float)UINT32_MAX / (float)scale))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    scaled_value = (uint32_t)(display_value * (float)scale + 0.5f);
    integer_value = scaled_value / scale;
    fraction_value = scaled_value % scale;
    if (scale == 100u)
    {
        (void)lv_snprintf(text, text_length, "%lu.%02lu%s",
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value, suffix);
    }
    else if (scale == 10u)
    {
        (void)lv_snprintf(text, text_length, "%lu.%01lu%s",
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value, suffix);
    }
    else
    {
        (void)lv_snprintf(text, text_length, "%lu%s",
                          (unsigned long)integer_value, suffix);
    }
}

static void UI_DrawSweepLogGrid(lv_obj_t *chart,
                                lv_draw_ctx_t *draw_context)
{
    static const int32_t minor_log_offset[UI_SWEEP_LOG_MINOR_COUNT] =
        {301, 477, 602, 699, 778, 845, 903, 954};
    lv_draw_line_dsc_t major_horizontal;
    lv_draw_line_dsc_t minor_horizontal;
    lv_draw_line_dsc_t major_vertical;
    lv_draw_line_dsc_t minor_vertical;
    lv_coord_t border_width;
    lv_coord_t left;
    lv_coord_t top;
    lv_coord_t plot_width;
    lv_coord_t plot_height;
    lv_point_t start;
    lv_point_t end;
    int32_t exponent;
    int32_t x_range;
    int32_t y_range;
    uint32_t minor_index;

    if ((chart == NULL) || (draw_context == NULL))
    {
        return;
    }

    border_width = lv_obj_get_style_border_width(chart, LV_PART_MAIN);
    left = chart->coords.x1 +
           lv_obj_get_style_pad_left(chart, LV_PART_MAIN) +
           border_width;
    top = chart->coords.y1 +
          lv_obj_get_style_pad_top(chart, LV_PART_MAIN) +
          border_width;
    plot_width = lv_obj_get_content_width(chart) - 1;
    plot_height = lv_obj_get_content_height(chart) - 1;
    if ((plot_width <= 0) || (plot_height <= 0))
    {
        return;
    }
    x_range = g_sweep_x_max_log_milli - g_sweep_x_min_log_milli;
    y_range = g_sweep_y_max_log_milli - g_sweep_y_min_log_milli;
    if ((x_range <= 0) || (y_range <= 0))
    {
        return;
    }

    lv_draw_line_dsc_init(&major_horizontal);
    major_horizontal.color = lv_color_hex(0xffffffu);
    major_horizontal.width = 1;
    major_horizontal.opa = (lv_opa_t)190u;

    lv_draw_line_dsc_init(&minor_horizontal);
    minor_horizontal.color = lv_color_hex(0xffffffu);
    minor_horizontal.width = 1;
    minor_horizontal.opa = (lv_opa_t)85u;
    minor_horizontal.dash_width = 1;
    minor_horizontal.dash_gap = 3;

    lv_draw_line_dsc_init(&major_vertical);
    major_vertical.color = lv_color_hex(0xffffffu);
    major_vertical.width = 1;
    major_vertical.opa = (lv_opa_t)125u;

    lv_draw_line_dsc_init(&minor_vertical);
    minor_vertical.color = lv_color_hex(0xffffffu);
    minor_vertical.width = 1;
    minor_vertical.opa = (lv_opa_t)55u;
    minor_vertical.dash_width = 1;
    minor_vertical.dash_gap = 3;

    start.y = top;
    end.y = top + plot_height;
    for (exponent = UI_SWEEP_X_MIN_LOG_MILLI / 1000;
         exponent <= UI_SWEEP_X_MAX_LOG_MILLI / 1000;
         ++exponent)
    {
        int32_t log_milli = exponent * 1000;
        lv_coord_t x;

        if ((log_milli < g_sweep_x_min_log_milli) ||
            (log_milli > g_sweep_x_max_log_milli))
        {
            continue;
        }
        x = left + (lv_coord_t)(((int32_t)plot_width *
                  (log_milli - g_sweep_x_min_log_milli)) / x_range);

        start.x = x;
        end.x = x;
        lv_draw_line(draw_context, &major_vertical, &start, &end);
    }

    for (exponent = UI_SWEEP_X_MIN_LOG_MILLI / 1000;
         exponent < UI_SWEEP_X_MAX_LOG_MILLI / 1000;
         ++exponent)
    {
        for (minor_index = 0u;
             minor_index < UI_SWEEP_LOG_MINOR_COUNT;
             ++minor_index)
        {
            int32_t log_milli =
                exponent * 1000 + minor_log_offset[minor_index];
            lv_coord_t x;

            if ((log_milli < g_sweep_x_min_log_milli) ||
                (log_milli > g_sweep_x_max_log_milli))
            {
                continue;
            }
            x = left + (lv_coord_t)(((int32_t)plot_width *
                      (log_milli - g_sweep_x_min_log_milli)) / x_range);

            start.x = x;
            end.x = x;
            lv_draw_line(draw_context, &minor_vertical, &start, &end);
        }
    }

    start.x = left;
    end.x = left + plot_width;
    for (exponent = g_sweep_full_y_min_log_milli / 1000;
         exponent <= g_sweep_full_y_max_log_milli / 1000;
         ++exponent)
    {
        int32_t log_milli = exponent * 1000;
        lv_coord_t y;

        if ((log_milli < g_sweep_y_min_log_milli) ||
            (log_milli > g_sweep_y_max_log_milli))
        {
            continue;
        }
        y = top + (lv_coord_t)(((int32_t)plot_height *
                 (g_sweep_y_max_log_milli - log_milli)) / y_range);

        start.y = y;
        end.y = y;
        lv_draw_line(draw_context, &major_horizontal, &start, &end);
    }

    for (exponent = g_sweep_full_y_min_log_milli / 1000;
         exponent < g_sweep_full_y_max_log_milli / 1000;
         ++exponent)
    {
        for (minor_index = 0u;
             minor_index < UI_SWEEP_LOG_MINOR_COUNT;
             ++minor_index)
        {
            int32_t log_milli =
                exponent * 1000 + minor_log_offset[minor_index];
            lv_coord_t y;

            if ((log_milli < g_sweep_y_min_log_milli) ||
                (log_milli > g_sweep_y_max_log_milli))
            {
                continue;
            }
            y = top + (lv_coord_t)(((int32_t)plot_height *
                     (g_sweep_y_max_log_milli - log_milli)) / y_range);

            start.y = y;
            end.y = y;
            lv_draw_line(draw_context, &minor_horizontal, &start, &end);
        }
    }
}

static void UI_SweepChartDrawEvent(lv_event_t *event)
{
    lv_obj_draw_part_dsc_t *draw;
    lv_obj_t *chart;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_DRAW_PART_BEGIN))
    {
        return;
    }

    draw = lv_event_get_draw_part_dsc(event);
    if ((draw == NULL) || (draw->class_p != &lv_chart_class))
    {
        return;
    }

    chart = lv_event_get_target(event);
    if (draw->type == LV_CHART_DRAW_PART_DIV_LINE_INIT)
    {
        UI_DrawSweepLogGrid(chart, draw->draw_ctx);
        return;
    }

    if ((draw->type == LV_CHART_DRAW_PART_TICK_LABEL) &&
        (draw->text != NULL) && (draw->text_length != 0u))
    {
        UI_FormatSweepLogTick(
            draw->value,
            draw->id == LV_CHART_AXIS_PRIMARY_X,
            draw->text,
            draw->text_length);
        if (draw->label_dsc != NULL)
        {
            draw->label_dsc->font = &lv_font_montserrat_16;
            draw->label_dsc->color = lv_color_hex(0xffffffu);
        }
        if (draw->line_dsc != NULL)
        {
            draw->line_dsc->color = lv_color_hex(0xffffffu);
            draw->line_dsc->opa = LV_OPA_COVER;
        }
    }
}

static bool UI_GetSweepPlotArea(lv_obj_t *chart, lv_area_t *area)
{
    lv_coord_t border_width;
    lv_coord_t width;
    lv_coord_t height;

    if ((chart == NULL) || (area == NULL))
    {
        return false;
    }

    border_width = lv_obj_get_style_border_width(chart, LV_PART_MAIN);
    width = lv_obj_get_content_width(chart);
    height = lv_obj_get_content_height(chart);
    if ((width <= 1) || (height <= 1))
    {
        return false;
    }

    area->x1 = chart->coords.x1 +
               lv_obj_get_style_pad_left(chart, LV_PART_MAIN) +
               border_width;
    area->y1 = chart->coords.y1 +
               lv_obj_get_style_pad_top(chart, LV_PART_MAIN) +
               border_width;
    area->x2 = area->x1 + width - 1;
    area->y2 = area->y1 + height - 1;
    return true;
}

static void UI_ResetSweepInterpolation(uint32_t path)
{
    if (path >= 3u)
    {
        return;
    }

    SweepInterpolation_Reset(&g_sweep_cs_curve[path]);
    SweepInterpolation_Reset(&g_sweep_cp_curve[path]);
    SweepInterpolation_Reset(&g_sweep_d_curve[path]);
    g_sweep_interpolation_ready[path] = 0u;
}

static void UI_BuildSweepInterpolation(uint32_t path)
{
    uint32_t point_count;

    if (path >= 3u)
    {
        return;
    }
    point_count = g_sweep_total_points[path];
    if (point_count > UI_SWEEP_MAX_POINTS)
    {
        point_count = UI_SWEEP_MAX_POINTS;
    }

    /*
     * 原始数据完全保留；连续曲线只从有效点构建。
     * Cs/Cp在log(V)-log(C)空间插值，D在log(V)-D空间插值。
     */
    (void)SweepInterpolation_Build(&g_sweep_cs_curve[path],
                                   g_sweep_voltage[path],
                                   g_sweep_capacitance[path],
                                   g_sweep_valid[path],
                                   point_count, true);
    (void)SweepInterpolation_Build(&g_sweep_cp_curve[path],
                                   g_sweep_voltage[path],
                                   g_sweep_parallel_capacitance[path],
                                   g_sweep_model_valid[path],
                                   point_count, true);
    (void)SweepInterpolation_Build(&g_sweep_d_curve[path],
                                   g_sweep_voltage[path],
                                   g_sweep_dissipation[path],
                                   g_sweep_model_valid[path],
                                   point_count, false);
    g_sweep_interpolation_ready[path] =
        (g_sweep_cs_curve[path].count >= 2u) ? 1u : 0u;
}

static bool UI_SweepValueToPixel(lv_obj_t *chart,
                                 float voltage,
                                 float capacitance,
                                 lv_point_t *point)
{
    lv_area_t plot_area;
    int32_t x_log_milli;
    int32_t y_log_milli;
    int32_t x_range;
    int32_t y_range;
    lv_coord_t plot_width;
    lv_coord_t plot_height;

    if ((chart == NULL) || (point == NULL) ||
        !UI_GetSweepPlotArea(chart, &plot_area))
    {
        return false;
    }

    if (!isfinite(voltage) || !isfinite(capacitance) ||
        (voltage <= 0.0f) || (capacitance <= 0.0f))
    {
        return false;
    }
    if (voltage < 0.1f)
    {
        voltage = 0.1f;
    }

    x_log_milli = (int32_t)(log10f(voltage) * 1000.0f);
    y_log_milli = (int32_t)(log10f(capacitance) * 1000.0f);
    if ((x_log_milli < g_sweep_x_min_log_milli) ||
        (x_log_milli > g_sweep_x_max_log_milli) ||
        (y_log_milli < g_sweep_y_min_log_milli) ||
        (y_log_milli > g_sweep_y_max_log_milli))
    {
        return false;
    }

    x_range = g_sweep_x_max_log_milli - g_sweep_x_min_log_milli;
    y_range = g_sweep_y_max_log_milli - g_sweep_y_min_log_milli;
    if ((x_range <= 0) || (y_range <= 0))
    {
        return false;
    }

    plot_width = lv_area_get_width(&plot_area) - 1;
    plot_height = lv_area_get_height(&plot_area) - 1;
    point->x = plot_area.x1 +
               (lv_coord_t)(((int32_t)plot_width *
                             (x_log_milli - g_sweep_x_min_log_milli)) /
                            x_range);
    point->y = plot_area.y1 +
               (lv_coord_t)(((int32_t)plot_height *
                             (g_sweep_y_max_log_milli - y_log_milli)) /
                            y_range);
    return true;
}

static bool UI_SweepPointToPixel(lv_obj_t *chart,
                                 uint32_t path,
                                 uint32_t index,
                                 lv_point_t *point)
{
    if ((path >= 3u) || (index >= UI_SWEEP_MAX_POINTS) ||
        (g_sweep_valid[path][index] == 0u))
    {
        return false;
    }

    return UI_SweepValueToPixel(chart,
                                g_sweep_voltage[path][index],
                                g_sweep_capacitance[path][index],
                                point);
}

static void UI_FormatSweepFixed(float value,
                                uint32_t decimal_count,
                                char *text,
                                uint32_t text_length)
{
    float absolute_value;
    uint32_t scale;
    uint32_t scaled_value;
    uint32_t integer_value;
    uint32_t fraction_value;
    const char *sign;

    if ((text == NULL) || (text_length == 0u))
    {
        return;
    }
    if (!isfinite(value))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    sign = (value < 0.0f) ? "-" : "";
    absolute_value = (value < 0.0f) ? -value : value;
    scale = (decimal_count == 6u) ? 1000000u : 1000u;
    if (absolute_value > ((float)UINT32_MAX / (float)scale))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    scaled_value = (uint32_t)(absolute_value * (float)scale + 0.5f);
    integer_value = scaled_value / scale;
    fraction_value = scaled_value % scale;
    if (decimal_count == 6u)
    {
        (void)lv_snprintf(text, text_length, "%s%lu.%06lu",
                          sign,
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value);
    }
    else
    {
        (void)lv_snprintf(text, text_length, "%s%lu.%03lu",
                          sign,
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value);
    }
}

static void UI_FormatEngineeringFixed(float value,
                                      uint32_t decimal_count,
                                      char *text,
                                      uint32_t text_length)
{
    uint32_t scale;
    uint32_t scaled_value;
    uint32_t integer_value;
    uint32_t fraction_value;

    if ((text == NULL) || (text_length == 0u))
    {
        return;
    }
    if (!isfinite(value) || (value < 0.0f))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    switch (decimal_count)
    {
        case 1u:
            scale = 10u;
            break;
        case 2u:
            scale = 100u;
            break;
        default:
            decimal_count = 3u;
            scale = 1000u;
            break;
    }
    if (value > ((float)UINT32_MAX / (float)scale))
    {
        (void)lv_snprintf(text, text_length, "--");
        return;
    }

    scaled_value = (uint32_t)(value * (float)scale + 0.5f);
    integer_value = scaled_value / scale;
    fraction_value = scaled_value % scale;
    if (decimal_count == 1u)
    {
        (void)lv_snprintf(text, text_length, "%lu.%01lu",
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value);
    }
    else if (decimal_count == 2u)
    {
        (void)lv_snprintf(text, text_length, "%lu.%02lu",
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value);
    }
    else
    {
        (void)lv_snprintf(text, text_length, "%lu.%03lu",
                          (unsigned long)integer_value,
                          (unsigned long)fraction_value);
    }
}

static bool UI_ParseNonnegativeDecimal(const char *text, float *value)
{
    float result = 0.0f;
    float fraction_scale = 0.1f;
    uint8_t decimal_seen = 0u;
    uint8_t digit_seen = 0u;
    uint32_t index;

    if ((text == NULL) || (value == NULL))
    {
        return false;
    }
    for (index = 0u; text[index] != '\0'; ++index)
    {
        char character = text[index];

        if ((character >= '0') && (character <= '9'))
        {
            float digit = (float)(character - '0');

            digit_seen = 1u;
            if (decimal_seen == 0u)
            {
                result = result * 10.0f + digit;
            }
            else
            {
                result += digit * fraction_scale;
                fraction_scale *= 0.1f;
            }
            if (!isfinite(result))
            {
                return false;
            }
        }
        else if ((character == '.') && (decimal_seen == 0u))
        {
            decimal_seen = 1u;
        }
        else
        {
            return false;
        }
    }
    if (digit_seen == 0u)
    {
        return false;
    }
    *value = result;
    return true;
}

static void UI_UpdateSweepAnalysisSlider(void)
{
    lv_coord_t track_width;
    lv_coord_t knob_width;
    lv_coord_t travel;
    lv_coord_t knob_x;
    uint32_t span;

    if ((g_results_analysis_track == NULL) ||
        (g_results_analysis_knob == NULL))
    {
        return;
    }

    track_width = lv_obj_get_width(g_results_analysis_track);
    knob_width = lv_obj_get_width(g_results_analysis_knob);
    travel = track_width - knob_width;
    span = (g_ui_analysis_voltage_max_tenths > UI_BIAS_MIN_TENTHS) ?
           (g_ui_analysis_voltage_max_tenths - UI_BIAS_MIN_TENTHS) : 0u;
    knob_x = 0;
    if ((travel > 0) && (span != 0u))
    {
        knob_x = (lv_coord_t)(
            ((uint64_t)travel *
             (g_ui_analysis_voltage_tenths - UI_BIAS_MIN_TENTHS)) /
            span);
    }
    lv_obj_set_x(g_results_analysis_knob, knob_x);
}

static void UI_UpdateSweepAnalysisRange(void)
{
    uint32_t path;
    uint32_t maximum_tenths = UI_BIAS_MAX_TENTHS;
    uint8_t found_curve = 0u;

    for (path = 0u; path < 3u; ++path)
    {
        float minimum_log_voltage;
        float maximum_log_voltage;
        float maximum_voltage;
        uint32_t path_maximum_tenths;

        if (((g_sweep_visible_mask & (1u << path)) == 0u) ||
            (g_sweep_interpolation_ready[path] == 0u) ||
            !SweepInterpolation_GetLogVoltageRange(
                &g_sweep_cs_curve[path],
                &minimum_log_voltage, &maximum_log_voltage))
        {
            continue;
        }
        (void)minimum_log_voltage;
        maximum_voltage = powf(10.0f, maximum_log_voltage);
        if (!isfinite(maximum_voltage) || (maximum_voltage <= 0.0f))
        {
            continue;
        }
        path_maximum_tenths = (uint32_t)floorf(maximum_voltage * 10.0f +
                                                0.001f);
        if (path_maximum_tenths > UI_BIAS_MAX_TENTHS)
        {
            path_maximum_tenths = UI_BIAS_MAX_TENTHS;
        }
        if ((found_curve == 0u) ||
            (path_maximum_tenths < maximum_tenths))
        {
            maximum_tenths = path_maximum_tenths;
            found_curve = 1u;
        }
    }

    if ((found_curve == 0u) || (maximum_tenths < UI_BIAS_MIN_TENTHS))
    {
        maximum_tenths = UI_BIAS_MIN_TENTHS;
    }
    g_ui_analysis_voltage_max_tenths = maximum_tenths;
    if ((g_sweep_analysis_initialized == 0u) &&
        UI_IsSweepDataComplete())
    {
        g_ui_analysis_voltage_tenths = g_ui_sweep_end_tenths;
        g_sweep_analysis_initialized = 1u;
    }
    if (g_ui_analysis_voltage_tenths > maximum_tenths)
    {
        g_ui_analysis_voltage_tenths = maximum_tenths;
    }
    if (g_ui_analysis_voltage_tenths < UI_BIAS_MIN_TENTHS)
    {
        g_ui_analysis_voltage_tenths = UI_BIAS_MIN_TENTHS;
    }
}

static bool UI_IsSweepDataComplete(void)
{
    uint32_t path;

    if (g_sweep_visible_mask == 0u)
    {
        return false;
    }
    for (path = 0u; path < 3u; ++path)
    {
        if (((g_sweep_visible_mask & (1u << path)) != 0u) &&
            (g_sweep_total_points[path] == 0u))
        {
            return false;
        }
    }
    return true;
}

static void UI_UpdateSweepAnalysisCard(void)
{
    static const char *const units[UI_SWEEP_PARAMETER_COUNT] =
        {"pF", "pF", "pF", "ohm", "nC", "uJ"};
    EngineeringParametersResult result;
    uint8_t available_curves[3];
    float values[UI_SWEEP_PARAMETER_COUNT];
    uint8_t valid[UI_SWEEP_PARAMETER_COUNT];
    uint32_t decimals[UI_SWEEP_PARAMETER_COUNT] =
        {1u, 1u, 1u, 2u, 2u, 3u};
    uint32_t index;

    if (g_results_analysis_value_label != NULL)
    {
        lv_label_set_text_fmt(
            g_results_analysis_value_label,
            "%lu.%lu V",
            (unsigned long)(g_ui_analysis_voltage_tenths / 10u),
            (unsigned long)(g_ui_analysis_voltage_tenths % 10u));
    }
    UI_UpdateSweepAnalysisSlider();

    for (index = 0u; index < 3u; ++index)
    {
        available_curves[index] =
            (((g_sweep_visible_mask & (1u << index)) != 0u) &&
             (g_sweep_interpolation_ready[index] != 0u)) ? 1u : 0u;
    }
    EngineeringParameters_Compute(
        g_sweep_cs_curve, available_curves,
        (float)g_ui_analysis_voltage_tenths / 10.0f,
        g_latest_rg_ohm, g_latest_rg_valid != 0u, &result);

    values[0] = result.ciss_pf;
    values[1] = result.coss_pf;
    values[2] = result.crss_pf;
    values[3] = result.rg_ohm;
    values[4] = result.qoss_nc;
    values[5] = result.eoss_uj;
    valid[0] = result.ciss_valid;
    valid[1] = result.coss_valid;
    valid[2] = result.crss_valid;
    valid[3] = result.rg_valid;
    valid[4] = result.qoss_valid;
    valid[5] = result.eoss_valid;

    for (index = 0u; index < UI_SWEEP_PARAMETER_COUNT; ++index)
    {
        char value_text[28];

        if (g_results_parameter_value_labels[index] == NULL)
        {
            continue;
        }
        if (valid[index] != 0u)
        {
            char number[20];

            UI_FormatEngineeringFixed(values[index], decimals[index],
                                      number, sizeof(number));
            (void)lv_snprintf(value_text, sizeof(value_text), "%s %s",
                              number, units[index]);
        }
        else
        {
            (void)lv_snprintf(value_text, sizeof(value_text), "--");
        }
        lv_label_set_text(g_results_parameter_value_labels[index],
                          value_text);
        lv_obj_set_style_text_color(
            g_results_parameter_value_labels[index],
            lv_color_hex((valid[index] != 0u) ?
                         UI_COLOR_TEXT : UI_COLOR_TEXT_MUTED),
            LV_PART_MAIN);
    }
}

static void UI_UpdateSweepProbeLabel(void)
{
    static const char *const names[] = {"CISS", "COSS", "CRSS"};
    lv_obj_t *label;
    char position[20];
    char voltage[20];
    char cs[20];
    char cp[20];
    char dissipation[20];
    uint32_t path;

    label = (g_sweep_probe_owner == 1u) ?
            g_results_probe_label : g_sweep_progress_label;
    if ((label == NULL) || (g_sweep_probe_valid == 0u))
    {
        return;
    }

    path = g_sweep_probe_path;
    UI_FormatSweepFixed(g_sweep_probe_voltage, 3u,
                        voltage, sizeof(voltage));
    UI_FormatSweepFixed(g_sweep_probe_cs, 3u,
                        cs, sizeof(cs));
    if (g_sweep_probe_model_valid != 0u)
    {
        UI_FormatSweepFixed(g_sweep_probe_cp, 3u,
                            cp, sizeof(cp));
        UI_FormatSweepFixed(g_sweep_probe_d, 6u,
                            dissipation, sizeof(dissipation));
    }
    else
    {
        (void)lv_snprintf(cp, sizeof(cp), "--");
        (void)lv_snprintf(dissipation, sizeof(dissipation), "--");
    }
    if (g_sweep_probe_interpolated != 0u)
    {
        (void)lv_snprintf(position, sizeof(position), "INTP");
    }
    else
    {
        (void)lv_snprintf(position, sizeof(position), "#%lu/%lu",
                          (unsigned long)(g_sweep_probe_index + 1u),
                          (unsigned long)g_sweep_total_points[path]);
    }

    lv_label_set_text_fmt(
        label,
        "%s  %s  VBIAS=%sV  Cs=%spF  Cp=%spF  D=%s",
        names[path], position, voltage, cs, cp, dissipation);
}

static void UI_ResetSweepProbe(void)
{
    uint8_t was_valid = g_sweep_probe_valid;

    g_sweep_probe_path = 0u;
    g_sweep_probe_index = 0u;
    g_sweep_probe_voltage = 0.0f;
    g_sweep_probe_cs = 0.0f;
    g_sweep_probe_cp = 0.0f;
    g_sweep_probe_d = 0.0f;
    g_sweep_probe_model_valid = 0u;
    g_sweep_probe_interpolated = 0u;
    g_sweep_probe_valid = 0u;
    g_sweep_probe_tracking = 0u;
    if ((was_valid != 0u) && (g_sweep_probe_chart != NULL))
    {
        lv_obj_invalidate(g_sweep_probe_chart);
    }
    if ((g_sweep_probe_owner == 1u) && (g_results_probe_label != NULL))
    {
        lv_label_set_text(g_results_probe_label, UI_RESULT_PROBE_HINT);
        lv_obj_set_style_text_color(g_results_probe_label,
                                    lv_color_hex(UI_COLOR_TEXT_MUTED),
                                    LV_PART_MAIN);
    }
    g_sweep_probe_chart = NULL;
    g_sweep_probe_owner = 0u;
}

/* 把当前对数视口施加到**指定**图表上。
   结果页的「扫压结果」是扫压页的投影,两张图共用同一套视口参数,
   所以这里必须能传图表进来,不能写死 g_sweep_chart。 */
static void UI_ApplyChartViewport(lv_obj_t *chart)
{
    int32_t x_span;
    int32_t y_span;
    uint32_t x_major_count;
    uint32_t y_major_count;

    if (chart == NULL)
    {
        return;
    }

    x_span = g_sweep_x_max_log_milli - g_sweep_x_min_log_milli;
    y_span = g_sweep_y_max_log_milli - g_sweep_y_min_log_milli;
    if ((x_span <= 0) || (y_span <= 0))
    {
        return;
    }

    x_major_count = (uint32_t)((x_span + 999) / 1000) + 1u;
    y_major_count = (uint32_t)((y_span + 999) / 1000) + 1u;
    if (x_major_count < 3u)
    {
        x_major_count = 3u;
    }
    else if (x_major_count > 6u)
    {
        x_major_count = 6u;
    }
    if (y_major_count < 3u)
    {
        y_major_count = 3u;
    }
    else if (y_major_count > 6u)
    {
        y_major_count = 6u;
    }

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X,
                       (lv_coord_t)g_sweep_x_min_log_milli,
                       (lv_coord_t)g_sweep_x_max_log_milli);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
                       (lv_coord_t)g_sweep_y_min_log_milli,
                       (lv_coord_t)g_sweep_y_max_log_milli);
    lv_chart_set_axis_tick(chart,
                           LV_CHART_AXIS_PRIMARY_X,
                           6, 3, (lv_coord_t)x_major_count,
                           1, true, 38);
    lv_chart_set_axis_tick(chart,
                           LV_CHART_AXIS_PRIMARY_Y,
                           6, 3, (lv_coord_t)y_major_count,
                           1, true, 58);
    lv_chart_set_div_line_count(chart, 0u, 0u);
    lv_chart_refresh(chart);
}

static void UI_ApplySweepViewport(void)
{
    UI_ApplyChartViewport(g_sweep_chart);
}

static void UI_ResetSweepViewport(void)
{
    g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
    g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
    g_sweep_y_min_log_milli = g_sweep_full_y_min_log_milli;
    g_sweep_y_max_log_milli = g_sweep_full_y_max_log_milli;
    UI_ApplySweepViewport();
}

static bool UI_FindTouchPointById(const LCD_TouchState *touch_state,
                                  uint8_t id,
                                  lv_point_t *point)
{
    uint32_t index;

    if ((touch_state == NULL) || (point == NULL))
    {
        return false;
    }
    for (index = 0u;
         (index < touch_state->count) && (index < LCD_TOUCH_MAX_POINTS);
         ++index)
    {
        if (touch_state->point[index].id == id)
        {
            point->x = (lv_coord_t)touch_state->point[index].x;
            point->y = (lv_coord_t)touch_state->point[index].y;
            return true;
        }
    }
    return false;
}

static float UI_SweepTouchDistance(const lv_point_t *first,
                                   const lv_point_t *second)
{
    float delta_x;
    float delta_y;

    if ((first == NULL) || (second == NULL))
    {
        return 0.0f;
    }
    delta_x = (float)second->x - (float)first->x;
    delta_y = (float)second->y - (float)first->y;
    return sqrtf(delta_x * delta_x + delta_y * delta_y);
}

static float UI_ClampSweepRatio(float ratio)
{
    if (ratio < 0.0f)
    {
        return 0.0f;
    }
    if (ratio > 1.0f)
    {
        return 1.0f;
    }
    return ratio;
}

static void UI_ClampSweepViewRange(float *minimum,
                                   float *maximum,
                                   float full_minimum,
                                   float full_maximum,
                                   float minimum_span)
{
    float full_span;
    float span;

    if ((minimum == NULL) || (maximum == NULL))
    {
        return;
    }

    full_span = full_maximum - full_minimum;
    if (full_span <= 0.0f)
    {
        *minimum = full_minimum;
        *maximum = full_maximum;
        return;
    }
    if (minimum_span > full_span)
    {
        minimum_span = full_span;
    }

    span = *maximum - *minimum;
    if (span < minimum_span)
    {
        float center = (*minimum + *maximum) * 0.5f;

        span = minimum_span;
        *minimum = center - span * 0.5f;
        *maximum = center + span * 0.5f;
    }
    if (span >= full_span)
    {
        *minimum = full_minimum;
        *maximum = full_maximum;
        return;
    }
    if (*minimum < full_minimum)
    {
        *maximum += full_minimum - *minimum;
        *minimum = full_minimum;
    }
    if (*maximum > full_maximum)
    {
        *minimum -= *maximum - full_maximum;
        *maximum = full_maximum;
    }
}

static bool UI_StartSweepGesture(const LCD_TouchState *touch_state)
{
    lv_area_t plot_area;
    lv_point_t first;
    lv_point_t second;
    float center_x;
    float center_y;
    float x_ratio;
    float y_ratio;
    float x_span;
    float y_span;

    if ((touch_state == NULL) || (touch_state->count < 2u) ||
        (g_sweep_chart == NULL) || (g_sweep_inspection_ready == 0u) ||
        !UI_GetSweepPlotArea(g_sweep_chart, &plot_area))
    {
        return false;
    }

    first.x = (lv_coord_t)touch_state->point[0].x;
    first.y = (lv_coord_t)touch_state->point[0].y;
    second.x = (lv_coord_t)touch_state->point[1].x;
    second.y = (lv_coord_t)touch_state->point[1].y;
    g_sweep_gesture.start_distance =
        UI_SweepTouchDistance(&first, &second);
    if (g_sweep_gesture.start_distance < UI_SWEEP_PINCH_MIN_DISTANCE_PX)
    {
        return false;
    }

    center_x = ((float)first.x + (float)second.x) * 0.5f;
    center_y = ((float)first.y + (float)second.y) * 0.5f;
    x_ratio = UI_ClampSweepRatio(
        (center_x - (float)plot_area.x1) /
        (float)(lv_area_get_width(&plot_area) - 1));
    y_ratio = UI_ClampSweepRatio(
        (center_y - (float)plot_area.y1) /
        (float)(lv_area_get_height(&plot_area) - 1));
    x_span = (float)(g_sweep_x_max_log_milli -
                     g_sweep_x_min_log_milli);
    y_span = (float)(g_sweep_y_max_log_milli -
                     g_sweep_y_min_log_milli);

    g_sweep_gesture.first_id = touch_state->point[0].id;
    g_sweep_gesture.second_id = touch_state->point[1].id;
    g_sweep_gesture.start_x_min_log_milli = g_sweep_x_min_log_milli;
    g_sweep_gesture.start_x_max_log_milli = g_sweep_x_max_log_milli;
    g_sweep_gesture.start_y_min_log_milli = g_sweep_y_min_log_milli;
    g_sweep_gesture.start_y_max_log_milli = g_sweep_y_max_log_milli;
    g_sweep_gesture.anchor_x_log_milli =
        (float)g_sweep_x_min_log_milli + x_ratio * x_span;
    g_sweep_gesture.anchor_y_log_milli =
        (float)g_sweep_y_max_log_milli - y_ratio * y_span;
    g_sweep_gesture.last_update_tick = lv_tick_get();
    g_sweep_gesture.active = 1u;
    g_sweep_gesture.block_single_touch = 1u;
    g_sweep_probe_tracking = 0u;
    g_sweep_last_tap_valid = 0u;
    return true;
}

static void UI_UpdateSweepGesture(const LCD_TouchState *touch_state)
{
    lv_area_t plot_area;
    lv_point_t first;
    lv_point_t second;
    float current_distance;
    float scale;
    float center_x;
    float center_y;
    float x_ratio;
    float y_ratio;
    float new_x_span;
    float new_y_span;
    float new_x_min;
    float new_x_max;
    float new_y_min;
    float new_y_max;
    int32_t x_minimum;
    int32_t x_maximum;
    int32_t y_minimum;
    int32_t y_maximum;
    uint32_t now;

    if ((touch_state == NULL) || (touch_state->count < 2u) ||
        (g_sweep_gesture.active == 0u) ||
        !UI_GetSweepPlotArea(g_sweep_chart, &plot_area))
    {
        return;
    }
    if (!UI_FindTouchPointById(touch_state, g_sweep_gesture.first_id,
                               &first) ||
        !UI_FindTouchPointById(touch_state, g_sweep_gesture.second_id,
                               &second))
    {
        g_sweep_gesture.active = 0u;
        (void)UI_StartSweepGesture(touch_state);
        return;
    }

    now = lv_tick_get();
    if (lv_tick_elaps(g_sweep_gesture.last_update_tick) <
        UI_SWEEP_GESTURE_UPDATE_MS)
    {
        return;
    }
    g_sweep_gesture.last_update_tick = now;

    current_distance = UI_SweepTouchDistance(&first, &second);
    if (current_distance < UI_SWEEP_PINCH_MIN_DISTANCE_PX)
    {
        return;
    }
    scale = g_sweep_gesture.start_distance / current_distance;
    center_x = ((float)first.x + (float)second.x) * 0.5f;
    center_y = ((float)first.y + (float)second.y) * 0.5f;
    x_ratio = UI_ClampSweepRatio(
        (center_x - (float)plot_area.x1) /
        (float)(lv_area_get_width(&plot_area) - 1));
    y_ratio = UI_ClampSweepRatio(
        (center_y - (float)plot_area.y1) /
        (float)(lv_area_get_height(&plot_area) - 1));

    new_x_span =
        (float)(g_sweep_gesture.start_x_max_log_milli -
                g_sweep_gesture.start_x_min_log_milli) * scale;
    new_y_span =
        (float)(g_sweep_gesture.start_y_max_log_milli -
                g_sweep_gesture.start_y_min_log_milli) * scale;
    new_x_min = g_sweep_gesture.anchor_x_log_milli -
                x_ratio * new_x_span;
    new_x_max = new_x_min + new_x_span;
    new_y_max = g_sweep_gesture.anchor_y_log_milli +
                y_ratio * new_y_span;
    new_y_min = new_y_max - new_y_span;

    UI_ClampSweepViewRange(&new_x_min, &new_x_max,
                           (float)UI_SWEEP_X_MIN_LOG_MILLI,
                           (float)UI_SWEEP_X_MAX_LOG_MILLI,
                           (float)UI_SWEEP_MIN_X_SPAN_LOG_MILLI);
    UI_ClampSweepViewRange(&new_y_min, &new_y_max,
                           (float)g_sweep_full_y_min_log_milli,
                           (float)g_sweep_full_y_max_log_milli,
                           (float)UI_SWEEP_MIN_Y_SPAN_LOG_MILLI);

    x_minimum = (int32_t)lroundf(new_x_min);
    x_maximum = (int32_t)lroundf(new_x_max);
    y_minimum = (int32_t)lroundf(new_y_min);
    y_maximum = (int32_t)lroundf(new_y_max);
    if ((abs(x_minimum - g_sweep_x_min_log_milli) < 2) &&
        (abs(x_maximum - g_sweep_x_max_log_milli) < 2) &&
        (abs(y_minimum - g_sweep_y_min_log_milli) < 2) &&
        (abs(y_maximum - g_sweep_y_max_log_milli) < 2))
    {
        return;
    }

    g_sweep_x_min_log_milli = x_minimum;
    g_sweep_x_max_log_milli = x_maximum;
    g_sweep_y_min_log_milli = y_minimum;
    g_sweep_y_max_log_milli = y_maximum;
    UI_ApplySweepViewport();
}

static bool UI_SelectInterpolatedSweepProbe(
    lv_obj_t *chart,
    uint32_t owner,
    const lv_point_t *touch_point,
    bool lock_current_path)
{
    lv_area_t plot_area;
    float x_ratio;
    float log_voltage;
    float voltage;
    uint32_t path_offset;
    uint32_t start_path;
    uint32_t best_path = 0u;
    uint32_t best_distance = UINT32_MAX;
    float best_cs = 0.0f;
    float probe_cp = 0.0f;
    float probe_d = 0.0f;
    uint8_t probe_model_valid;
    bool found = false;

    if ((touch_point == NULL) || (chart == NULL) ||
        !UI_GetSweepPlotArea(chart, &plot_area) ||
        (touch_point->x < plot_area.x1) ||
        (touch_point->x > plot_area.x2) ||
        ((!lock_current_path) &&
         ((touch_point->y < plot_area.y1) ||
          (touch_point->y > plot_area.y2))))
    {
        return false;
    }

    x_ratio = UI_ClampSweepRatio(
        ((float)touch_point->x - (float)plot_area.x1) /
        (float)(lv_area_get_width(&plot_area) - 1));
    log_voltage =
        ((float)g_sweep_x_min_log_milli +
         x_ratio * (float)(g_sweep_x_max_log_milli -
                           g_sweep_x_min_log_milli)) /
        1000.0f;
    voltage = powf(10.0f, log_voltage);
    if (!isfinite(voltage) || (voltage <= 0.0f))
    {
        return false;
    }

    start_path = (g_sweep_probe_valid != 0u) ?
                 ((g_sweep_probe_path + 1u) % 3u) : 0u;
    for (path_offset = 0u; path_offset < 3u; ++path_offset)
    {
        uint32_t path = lock_current_path ?
                        g_sweep_probe_path :
                        ((start_path + path_offset) % 3u);
        float cs;
        lv_point_t curve_point;
        int32_t delta_x;
        int32_t delta_y;
        uint32_t distance;

        if ((path >= 3u) ||
            ((g_sweep_visible_mask & (1u << path)) == 0u) ||
            (g_sweep_interpolation_ready[path] == 0u))
        {
            if (lock_current_path)
            {
                break;
            }
            continue;
        }
        if (!SweepInterpolation_Evaluate(&g_sweep_cs_curve[path],
                                         voltage, &cs) ||
            !UI_SweepValueToPixel(chart, voltage, cs,
                                  &curve_point))
        {
            if (lock_current_path)
            {
                break;
            }
            continue;
        }

        delta_x = (int32_t)curve_point.x - (int32_t)touch_point->x;
        if (lock_current_path)
        {
            distance = (uint32_t)((delta_x < 0) ? -delta_x : delta_x);
        }
        else
        {
            delta_y = (int32_t)curve_point.y - (int32_t)touch_point->y;
            distance = (uint32_t)(delta_x * delta_x +
                                  delta_y * delta_y);
        }
        if (distance < best_distance)
        {
            best_distance = distance;
            best_path = path;
            best_cs = cs;
            found = true;
        }
        if (lock_current_path)
        {
            break;
        }
    }

    if (!found ||
        ((!lock_current_path) &&
         (best_distance > (UI_SWEEP_PROBE_HIT_RADIUS *
                           UI_SWEEP_PROBE_HIT_RADIUS))))
    {
        return false;
    }

    probe_model_valid =
        (SweepInterpolation_Evaluate(&g_sweep_cp_curve[best_path],
                                     voltage, &probe_cp) &&
         SweepInterpolation_Evaluate(&g_sweep_d_curve[best_path],
                                     voltage, &probe_d)) ? 1u : 0u;
    if ((g_sweep_probe_valid == 0u) ||
        (g_sweep_probe_interpolated == 0u) ||
        (g_sweep_probe_path != best_path) ||
        (fabsf(g_sweep_probe_voltage - voltage) > 0.0001f))
    {
        g_sweep_probe_path = best_path;
        g_sweep_probe_index = 0u;
        g_sweep_probe_voltage = voltage;
        g_sweep_probe_cs = best_cs;
        g_sweep_probe_cp = probe_cp;
        g_sweep_probe_d = probe_d;
        g_sweep_probe_model_valid = probe_model_valid;
        g_sweep_probe_interpolated = 1u;
        g_sweep_probe_valid = 1u;
        g_sweep_probe_chart = chart;
        g_sweep_probe_owner = owner;
        UI_UpdateSweepProbeLabel();
        lv_obj_invalidate(chart);
    }
    return true;
}

/* prefer_raw = true:先按原始采样点吸附(结果页按下的瞬间),失败再退回插值。
   扫压页用 false,保留原有的「完成后直接走插值」行为。 */
static bool UI_SelectSweepProbe(lv_obj_t *chart,
                                uint32_t owner,
                                const lv_point_t *touch_point,
                                bool lock_current_path,
                                bool prefer_raw)
{
    lv_area_t plot_area;
    uint32_t path_offset;
    uint32_t best_path = 0u;
    uint32_t best_index = 0u;
    uint32_t best_distance = UINT32_MAX;
    uint32_t start_path;
    bool found = false;

    if ((g_sweep_inspection_ready != 0u) && !prefer_raw)
    {
        uint32_t path;
        bool interpolation_available = false;

        if (lock_current_path)
        {
            interpolation_available =
                (g_sweep_probe_path < 3u) &&
                (g_sweep_interpolation_ready[g_sweep_probe_path] != 0u);
        }
        else
        {
            for (path = 0u; path < 3u; ++path)
            {
                if (((g_sweep_visible_mask & (1u << path)) != 0u) &&
                    (g_sweep_interpolation_ready[path] != 0u))
                {
                    interpolation_available = true;
                    break;
                }
            }
        }
        if (interpolation_available)
        {
            return UI_SelectInterpolatedSweepProbe(chart, owner,
                                                   touch_point,
                                                   lock_current_path);
        }
    }

    if ((touch_point == NULL) || (chart == NULL) ||
        !UI_GetSweepPlotArea(chart, &plot_area) ||
        (touch_point->x < plot_area.x1) ||
        (touch_point->x > plot_area.x2) ||
        ((!lock_current_path) &&
         ((touch_point->y < plot_area.y1) ||
          (touch_point->y > plot_area.y2))))
    {
        return false;
    }

    start_path = (g_sweep_probe_valid != 0u) ?
                 ((g_sweep_probe_path + 1u) % 3u) : 0u;
    for (path_offset = 0u; path_offset < 3u; ++path_offset)
    {
        uint32_t path = lock_current_path ?
                        g_sweep_probe_path :
                        ((start_path + path_offset) % 3u);
        uint32_t index;

        if ((path >= 3u) ||
            ((g_sweep_visible_mask & (1u << path)) == 0u))
        {
            if (lock_current_path)
            {
                break;
            }
            continue;
        }

        for (index = 0u;
             (index < g_sweep_total_points[path]) &&
             (index < UI_SWEEP_MAX_POINTS);
             ++index)
        {
            lv_point_t data_point;
            int32_t delta_x;
            int32_t delta_y;
            uint32_t distance;

            if (!UI_SweepPointToPixel(chart, path, index,
                                      &data_point))
            {
                continue;
            }

            delta_x = (int32_t)data_point.x - (int32_t)touch_point->x;
            if (lock_current_path)
            {
                distance = (uint32_t)((delta_x < 0) ? -delta_x : delta_x);
            }
            else
            {
                delta_y = (int32_t)data_point.y - (int32_t)touch_point->y;
                distance = (uint32_t)(delta_x * delta_x +
                                      delta_y * delta_y);
            }
            if (distance < best_distance)
            {
                best_distance = distance;
                best_path = path;
                best_index = index;
                found = true;
            }
        }

        if (lock_current_path)
        {
            break;
        }
    }

    if (!found ||
        ((!lock_current_path) &&
         (best_distance > (UI_SWEEP_PROBE_HIT_RADIUS *
                           UI_SWEEP_PROBE_HIT_RADIUS))))
    {
        if (prefer_raw && (g_sweep_inspection_ready != 0u))
        {
            return UI_SelectInterpolatedSweepProbe(chart, owner,
                                                   touch_point,
                                                   lock_current_path);
        }
        return false;
    }

    if ((g_sweep_probe_valid == 0u) ||
        (g_sweep_probe_path != best_path) ||
        (g_sweep_probe_index != best_index))
    {
        g_sweep_probe_path = best_path;
        g_sweep_probe_index = best_index;
        g_sweep_probe_voltage = g_sweep_voltage[best_path][best_index];
        g_sweep_probe_cs = g_sweep_capacitance[best_path][best_index];
        g_sweep_probe_cp =
            g_sweep_parallel_capacitance[best_path][best_index];
        g_sweep_probe_d = g_sweep_dissipation[best_path][best_index];
        g_sweep_probe_model_valid =
            g_sweep_model_valid[best_path][best_index];
        g_sweep_probe_interpolated = 0u;
        g_sweep_probe_valid = 1u;
        g_sweep_probe_chart = chart;
        g_sweep_probe_owner = owner;
        UI_UpdateSweepProbeLabel();
        lv_obj_invalidate(chart);
    }
    return true;
}

/* 结果页「扫压结果」的触摸读数:按下先吸附原始点,滑动时沿曲线插值。
   结果页不做捏合缩放 —— 缩放只在扫压页,视口由 UI_ApplyChartViewport() 同步。 */
static void UI_ResultsSweepTouchEvent(lv_event_t *event)
{
    lv_event_code_t code;
    lv_indev_t *input_device;
    lv_point_t touch_point;

    if (event == NULL)
    {
        return;
    }
    code = lv_event_get_code(event);
    if ((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST))
    {
        g_sweep_probe_tracking = 0u;
        return;
    }
    if ((code != LV_EVENT_PRESSED) && (code != LV_EVENT_PRESSING))
    {
        return;
    }
    input_device = lv_indev_get_act();
    if (input_device == NULL)
    {
        return;
    }
    lv_indev_get_point(input_device, &touch_point);

    if (code == LV_EVENT_PRESSED)
    {
        g_sweep_probe_tracking =
            UI_SelectSweepProbe(g_results_chart, 1u, &touch_point,
                                false, true) ? 1u : 0u;
        if (g_sweep_probe_tracking != 0u)
        {
            lv_obj_set_style_text_color(g_results_probe_label,
                                        lv_color_hex(UI_COLOR_TEXT),
                                        LV_PART_MAIN);
        }
    }
    else if (g_sweep_probe_tracking != 0u)
    {
        (void)UI_SelectSweepProbe(g_results_chart, 1u, &touch_point,
                                  true, false);
    }
}

static void UI_SweepChartTouchEvent(lv_event_t *event)
{
    lv_event_code_t code;
    lv_indev_t *input_device;
    lv_point_t touch_point;
    LCD_TouchState touch_state = {0};

    if (event == NULL)
    {
        return;
    }

    code = lv_event_get_code(event);
    if ((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST))
    {
        uint8_t gesture_was_active =
            g_sweep_gesture.block_single_touch;

        g_sweep_probe_tracking = 0u;
        g_sweep_gesture.active = 0u;
        g_sweep_gesture.block_single_touch = 0u;
        if ((code == LV_EVENT_RELEASED) &&
            (gesture_was_active == 0u) &&
            (g_sweep_single_touch_moved == 0u) &&
            (g_sweep_inspection_ready != 0u))
        {
            uint32_t now = lv_tick_get();

            if ((g_sweep_last_tap_valid != 0u) &&
                (lv_tick_elaps(g_sweep_last_tap_tick) <=
                 UI_SWEEP_DOUBLE_TAP_MS))
            {
                g_sweep_last_tap_valid = 0u;
                UI_ResetSweepViewport();
            }
            else
            {
                g_sweep_last_tap_tick = now;
                g_sweep_last_tap_valid = 1u;
            }
        }
        g_sweep_single_touch_moved = 0u;
        return;
    }
    if ((code != LV_EVENT_PRESSED) && (code != LV_EVENT_PRESSING))
    {
        return;
    }

    (void)LV_PortIndev_GetTouchState(&touch_state);
    if ((touch_state.count >= 2u) &&
        (g_sweep_inspection_ready != 0u))
    {
        g_sweep_single_touch_moved = 1u;
        if (g_sweep_gesture.active == 0u)
        {
            (void)UI_StartSweepGesture(&touch_state);
        }
        else
        {
            UI_UpdateSweepGesture(&touch_state);
        }
        return;
    }
    if (g_sweep_gesture.active != 0u)
    {
        g_sweep_gesture.active = 0u;
        g_sweep_gesture.block_single_touch = 1u;
        g_sweep_probe_tracking = 0u;
        return;
    }
    if (g_sweep_gesture.block_single_touch != 0u)
    {
        return;
    }

    input_device = lv_indev_get_act();
    if (input_device == NULL)
    {
        return;
    }
    lv_indev_get_point(input_device, &touch_point);

    if (code == LV_EVENT_PRESSED)
    {
        g_sweep_touch_press_point = touch_point;
        g_sweep_single_touch_moved = 0u;
        g_sweep_probe_tracking =
            UI_SelectSweepProbe(g_sweep_chart, 0u, &touch_point,
                                false, false) ? 1u : 0u;
    }
    else
    {
        if ((abs((int)touch_point.x -
                 (int)g_sweep_touch_press_point.x) >
             UI_SWEEP_TAP_MOVE_LIMIT_PX) ||
            (abs((int)touch_point.y -
                 (int)g_sweep_touch_press_point.y) >
             UI_SWEEP_TAP_MOVE_LIMIT_PX))
        {
            g_sweep_single_touch_moved = 1u;
        }
        if (g_sweep_probe_tracking != 0u)
        {
            (void)UI_SelectSweepProbe(g_sweep_chart, 0u, &touch_point,
                                      true, false);
        }
    }
}

static void UI_SweepChartProbeDrawEvent(lv_event_t *event)
{
    lv_obj_t *chart;
    lv_draw_ctx_t *draw_context;
    lv_area_t plot_area;
    lv_point_t point;
    lv_point_t line_start;
    lv_point_t line_end;
    lv_area_t marker_area;
    lv_draw_line_dsc_t guide_line;
    lv_draw_rect_dsc_t marker;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_DRAW_POST) ||
        (g_sweep_probe_valid == 0u))
    {
        return;
    }

    chart = lv_event_get_target(event);
    /* 两张图共用一份探针数据,只让**当前操作的那张**画十字线。 */
    if (chart != g_sweep_probe_chart)
    {
        return;
    }
    draw_context = lv_event_get_draw_ctx(event);
    if ((draw_context == NULL) ||
        !UI_GetSweepPlotArea(chart, &plot_area) ||
        !UI_SweepValueToPixel(chart, g_sweep_probe_voltage,
                              g_sweep_probe_cs, &point))
    {
        return;
    }

    lv_draw_line_dsc_init(&guide_line);
    guide_line.color = lv_color_hex(0xffffffu);
    guide_line.width = 1;
    guide_line.opa = (lv_opa_t)110u;
    guide_line.dash_width = 3;
    guide_line.dash_gap = 3;

    line_start.x = point.x;
    line_start.y = plot_area.y1;
    line_end.x = point.x;
    line_end.y = plot_area.y2;
    lv_draw_line(draw_context, &guide_line, &line_start, &line_end);
    line_start.x = plot_area.x1;
    line_start.y = point.y;
    line_end.x = plot_area.x2;
    line_end.y = point.y;
    lv_draw_line(draw_context, &guide_line, &line_start, &line_end);

    marker_area.x1 = point.x - UI_SWEEP_PROBE_RADIUS;
    marker_area.y1 = point.y - UI_SWEEP_PROBE_RADIUS;
    marker_area.x2 = point.x + UI_SWEEP_PROBE_RADIUS;
    marker_area.y2 = point.y + UI_SWEEP_PROBE_RADIUS;
    lv_draw_rect_dsc_init(&marker);
    marker.radius = LV_RADIUS_CIRCLE;
    marker.bg_color =
        lv_color_hex(g_sweep_series_colors[g_sweep_probe_path]);
    marker.bg_opa = LV_OPA_COVER;
    marker.border_color = lv_color_hex(0xffffffu);
    marker.border_width = 2;
    marker.border_opa = LV_OPA_COVER;
    lv_draw_rect(draw_context, &marker, &marker_area);
}

static void UI_SetSweepDrawerOpen(bool open)
{
    if (g_sweep_control_panel != NULL)
    {
        if (open)
        {
            lv_obj_clear_flag(g_sweep_control_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(g_sweep_control_panel);
        }
        else
        {
            lv_obj_add_flag(g_sweep_control_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_sweep_settings_button != NULL)
    {
        if (open)
        {
            lv_obj_add_flag(g_sweep_settings_button, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(g_sweep_settings_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(g_sweep_settings_button);
        }
    }
}

static void UI_SweepDrawerEvent(lv_event_t *event)
{
    bool open;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    open = ((uintptr_t)lv_event_get_user_data(event) != 0u);
    UI_SetSweepDrawerOpen(open);
}

static void UI_SetSweepProgress(uint32_t progress_permille)
{
    lv_coord_t full_width;
    lv_coord_t indicator_width;

    if ((g_sweep_progress_bar == NULL) ||
        (g_sweep_progress_indicator == NULL))
    {
        return;
    }

    if (progress_permille > 1000u)
    {
        progress_permille = 1000u;
    }
    if (progress_permille == 0u)
    {
        lv_obj_add_flag(g_sweep_progress_indicator, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    full_width = lv_obj_get_width(g_sweep_progress_bar) - 2;
    indicator_width =
        (lv_coord_t)(((int32_t)full_width * progress_permille) / 1000);
    if (indicator_width < 1)
    {
        indicator_width = 1;
    }
    lv_obj_set_width(g_sweep_progress_indicator, indicator_width);
    lv_obj_clear_flag(g_sweep_progress_indicator, LV_OBJ_FLAG_HIDDEN);
}

static void UI_CreateCommonStatusHeader(lv_obj_t *screen)
{
    g_common_header = UI_CreatePanel(screen,
                                     0,
                                     0,
                                     (lv_coord_t)LCD_PIXEL_WIDTH,
                                     UI_COMMON_HEADER_HEIGHT,
                                     UI_COLOR_SURFACE,
                                     0);
    lv_obj_set_style_border_color(g_common_header,
                                  lv_color_hex(UI_COLOR_BORDER),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(g_common_header, 1, LV_PART_MAIN);

    g_header_back_button =
        UI_CreateButton(g_common_header,
                        "返回",
                        UI_COLOR_BLUE,
                        8,
                        7,
                        68,
                        38,
                        UI_BackEvent,
                        NULL);
    /* 🔴 2026-10-10:7 栏 → **8 栏**(插入「档位:」)。原本 7 栏已经排满到
     *    右边界 1014,没有空位,所以右边六栏统统重新分配了宽度。
     *    新宽度是按 `ui_font_cn_20` 的**实测字宽**算的(汉字 20.0 px、
     *    数字约 11.1 px、':' 约 5.6 px),并用帧缓存量过实际占用,不是估的:
     *
     *      栏       x    width   右端   实测占用
     *      返回      8     68      76     —
     *      标题     84    110     194    最多 4 汉字 = 80
     *      后端    198    110     308    「后端:未连接」= 84
     *      偏压    312    116     428    「偏压:-10.00V」= 96
     *      回路    432    140     572    最长「回路:COSS_DIS」≈ 130
     *      档位    576    116     692    最长「档位:10kΩ」≈ 100
     *      状态    696    318    1014    「状态:就绪(…)」= 274
     *
     *    ⚠️ **连带**:首页的标题会被移到 x=16 并加宽(见 `UI_ActivatePage`
     *       的 `UI_PAGE_HOME` 分支),那个宽度**必须**跟着改,否则标题会压上
     *       「后端」栏(`x=198`)。那处是 `16 + 178 = 194`。 */
    g_header_title_label =
        UI_CreateText(g_common_header, "首页", &ui_font_cn_20,
                      UI_COLOR_TEXT, 84, 12, 110, 28,
                      LV_TEXT_ALIGN_LEFT);
    g_header_connection_label =
        UI_CreateText(g_common_header, "后端:未连接", &ui_font_cn_20,
                      UI_COLOR_ORANGE, 198, 12, 110, 28,
                      LV_TEXT_ALIGN_CENTER);
    g_header_bias_label =
        UI_CreateText(g_common_header, "", &ui_font_cn_20,
                      UI_COLOR_STATUS, 312, 12, 116, 28,
                      LV_TEXT_ALIGN_CENTER);
    g_header_path_label =
        UI_CreateText(g_common_header, "", &ui_font_cn_20,
                      UI_COLOR_STATUS, 432, 12, 140, 28,
                      LV_TEXT_ALIGN_CENTER);
    /* 🆕 2026-10-10 档位栏。内容是 **PL 回读的已生效值**(`app/relay_view.c`),
     *    不是界面请求值 —— 与「回路」「偏压」两栏同一个口径。 */
    g_header_gear_label =
        UI_CreateText(g_common_header, "", &ui_font_cn_20,
                      UI_COLOR_STATUS, 576, 12, 116, 28,
                      LV_TEXT_ALIGN_CENTER);
    g_header_operation_label =
        UI_CreateText(g_common_header, "系统就绪", &ui_font_cn_20,
                      UI_COLOR_TEXT_MUTED, 696, 12,
                      (lv_coord_t)LCD_PIXEL_WIDTH - 706, 28,
                      LV_TEXT_ALIGN_RIGHT);
    g_status_label = g_header_operation_label;
    UI_UpdateCommonStatus();
}

static lv_obj_t *UI_PrepareRootScreen(void)
{
    lv_obj_t *screen = lv_scr_act();
    uint32_t index;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen,
                              lv_color_hex(UI_COLOR_BACKGROUND),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, &ui_font_cn_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen,
                                lv_color_hex(UI_COLOR_TEXT),
                                LV_PART_MAIN);

    g_status_label = NULL;
    g_modal_overlay = NULL;
    g_common_header = NULL;
    g_header_back_button = NULL;
    g_header_title_label = NULL;
    g_header_connection_label = NULL;
    g_header_bias_label = NULL;
    g_header_path_label = NULL;
    g_header_gear_label = NULL;
    g_header_operation_label = NULL;
    g_keypad_overlay = NULL;
    g_keypad_title_label = NULL;
    g_single_bias_value_label = NULL;
    g_sweep_bias_value_label = NULL;
    g_single_result_label = NULL;
    g_single_result_note_label = NULL;
    g_single_result_panel = NULL;
    g_measure_all_result_panel = NULL;
    g_sweep_chart = NULL;
    g_sweep_progress_label = NULL;
    g_sweep_progress_bar = NULL;
    g_sweep_progress_indicator = NULL;
    g_sweep_control_panel = NULL;
    g_sweep_settings_button = NULL;
    g_sweep_curve_page = NULL;
    g_results_analysis_value_label = NULL;
    g_results_analysis_track = NULL;
    g_results_analysis_knob = NULL;
    g_results_probe_label = NULL;
    g_results_summary_label = NULL;
    (void)memset(g_results_parameter_value_labels, 0,
                 sizeof(g_results_parameter_value_labels));
    g_results_transfer_qrcode = NULL;
    g_results_transfer_status_label = NULL;
    g_results_transfer_payload_label = NULL;
    g_results_transfer_model_label = NULL;
    g_sweep_probe_chart = NULL;
    g_sweep_probe_owner = 0u;
    g_keypad_value_label = NULL;
    g_keypad_note_label = NULL;
    /* ⚠️ 两个数组**长度不同**(5 与 4),原来这条循环写死 `< 4u` ——
     *    `g_single_path_buttons[4]`(「全部」那颗)漏在外面。
     *    当前 `UI_PrepareRootScreen()` 只在启动时跑一次,且 `ui_demo.c`
     *    开头已经 `memset` 过整个数组,所以**还没有实际后果**;
     *    但这是一颗哑弹:哪天根屏重建第二次,下标 4 就是个悬垂指针,
     *    而 `UI_UpdateSinglePathStyle()` 只判 `NULL` 不判悬垂,当场野指针写。
     *    改成各清各的、上界从 `sizeof` 导出 —— 以后加挡位按钮不用回来改这里。 */
    for (index = 0u;
         index < (uint32_t)(sizeof(g_single_path_buttons) /
                            sizeof(g_single_path_buttons[0]));
         ++index)
    {
        g_single_path_buttons[index] = NULL;
    }
    for (index = 0u;
         index < (uint32_t)(sizeof(g_sweep_path_buttons) /
                            sizeof(g_sweep_path_buttons[0]));
         ++index)
    {
        g_sweep_path_buttons[index] = NULL;
    }
    for (index = 0u; index < UI_GEAR_COUNT; ++index)
    {
        g_gear_buttons[index] = NULL;
    }
    UI_CreateCommonStatusHeader(screen);
    return screen;
}

static lv_obj_t *UI_CreatePageContainer(lv_obj_t *screen)
{
    lv_obj_t *container = lv_obj_create(screen);

    lv_obj_set_pos(container, 0, 0);
    lv_obj_set_size(container,
                    (lv_coord_t)LCD_PIXEL_WIDTH,
                    (lv_coord_t)LCD_PIXEL_HEIGHT);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

    return container;
}

static void UI_ActivatePage(UI_Page page)
{
    static const char *const page_titles[] =
    {
        "首页", "系统复位", "单点测量", "扫压测量", "结果汇总"
    };
    static const char *const page_status[] =
    {
        "系统就绪", "等待操作", "请选择回路", "等待扫压", "等待测量"
    };
    uint32_t index;

    for (index = 0u; index < 5u; ++index)
    {
        if (g_page_containers[index] == NULL)
        {
            continue;
        }

        if (index == (uint32_t)page)
        {
            lv_obj_set_x(g_page_containers[index], 0);
            lv_obj_clear_flag(g_page_containers[index], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_x(g_page_containers[index], 0);
            lv_obj_add_flag(g_page_containers[index], LV_OBJ_FLAG_HIDDEN);
        }
    }

    g_ui_current_page = (uint32_t)page;
    g_status_label = g_header_operation_label;
    if (g_header_title_label != NULL)
    {
        lv_anim_del(g_header_title_label, NULL);
        lv_label_set_text(g_header_title_label, page_titles[(uint32_t)page]);
        if (page == UI_PAGE_HOME)
        {
            lv_obj_set_pos(g_header_title_label, 16, 12);
            /* ⚠️ 这个宽度与 `UI_CreateCommonStatusHeader()` 的「标题」栏右端
             *    对齐(16 + 178 = 194),**必须联动改** —— 首页没有返回键,
             *    标题左移并加宽,超了就会压上 x=198 的「后端」栏。 */
            lv_obj_set_width(g_header_title_label, 178);
        }
        else
        {
            lv_obj_set_pos(g_header_title_label, 84, 12);
            lv_obj_set_width(g_header_title_label, 110);
        }
    }
    if (g_header_back_button != NULL)
    {
        if (page == UI_PAGE_HOME)
        {
            lv_obj_add_flag(g_header_back_button, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(g_header_back_button, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (g_header_operation_label != NULL)
    {
        lv_label_set_text(g_header_operation_label,
                          page_status[(uint32_t)page]);
    }
    if (g_common_header != NULL)
    {
        lv_obj_move_foreground(g_common_header);
    }
    UI_UpdateCommonStatus();

    /*
     * 结果页是「投影视图」—— 自己不持有数据,进入时从各源页面重新取一次。
     * 因此用户在任何页面测完之后再点进结果页,看到的就是刚测出的那份数据。
     */
    if (page == UI_PAGE_MEASURE)
    {
        UI_UpdateMeasureResultMode();
    }
    else if (page == UI_PAGE_RESULTS)
    {
        UI_ResultsRefresh();
    }

    /*
     * 输入定时器可能刚好排在显示刷新定时器之后运行。页面切换完成后
     * 立即刷新，可避免再等待一个默认刷新周期才开始绘制新页面。
     */
    if (g_ui_immediate_refresh_enabled != 0u)
    {
        lv_refr_now(lv_disp_get_default());
    }
}

static void UI_BackEvent(lv_event_t *event)
{
    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    UI_RecordAction(100u);
    UI_ShowHome();
}


static void UI_SimpleActionEvent(lv_event_t *event)
{
    const UI_Action *action;
    bool command_queued = true;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    action = (const UI_Action *)lv_event_get_user_data(event);
    if (action == NULL)
    {
        return;
    }

    if ((action->action_code >= 210u) &&
        (action->action_code <= 214u))
    {
        g_ui_single_path = action->action_code - 210u;
        UI_UpdateSinglePathStyle();
        command_queued = DisplayProtocol_RequestPath(g_ui_single_path);
    }
    /* 🆕 2026-10-15:挡位 A/B/C/D(动作码 215–218)。
     * 🔴 **这里不设 `g_ui_gear`** —— 挡位按钮的高亮由**硬件回读**驱动
     *    (`UI_Demo_SetActualGear()` ← `relay_view.c`),不跟"用户点了哪颗"走。
     *    原因:写失败时按钮高亮就会变成一句谎话,而整个 `relay_view` 模块
     *    存在的意义就是消灭界面上的谎话(见其文件头)。
     *    所以这条分支只负责**下发**,高亮等 `RelayView_Refresh()` 回来再说。
     *    ⚠️ 副作用:硬件真被拒时按钮"点了没反应" —— 那是**对的**,
     *       弹框与 `[relay] ⚠️` 日志会说明原因,不能靠高亮掩盖。 */
    else if ((action->action_code >= 215u) &&
             (action->action_code <= 218u))
    {
        command_queued =
            DisplayProtocol_RequestGear(action->action_code - 215u);
    }
    else if ((action->action_code >= 310u) &&
             (action->action_code <= 313u))
    {
        g_ui_sweep_path = action->action_code - 310u;
        UI_UpdateSweepPathStyle();
    }
    else if (action->action_code == 220u)
    {
        /* 合并页:「全部」派发到 RequestPointAll,其余派发到单路 RequestPoint。
           与扫压页 320u 的分派写法一致。 */
        command_queued = (g_ui_single_path == UI_DISPLAY_PATH_ALL) ?
            DisplayProtocol_RequestPointAll(g_ui_single_bias_tenths) :
            DisplayProtocol_RequestPoint(g_ui_single_path,
                                         g_ui_single_bias_tenths);
    }
    else if (action->action_code == 320u)
    {
        command_queued = (g_ui_sweep_path == 3u) ?
            DisplayProtocol_RequestRgThenSweepAll(g_ui_sweep_end_tenths) :
            DisplayProtocol_RequestSweep(g_ui_sweep_path,
                                         g_ui_sweep_end_tenths);
        if (command_queued)
        {
            g_sweep_inspection_ready = 0u;
            UI_SetSweepDrawerOpen(false);
        }
    }
    else if (action->action_code == 321u)
    {
        command_queued = DisplayProtocol_RequestSweepStop();
    }

    UI_RecordAction(action->action_code);
    if (g_status_label != NULL)
    {
        if (command_queued)
        {
            lv_label_set_text(
                g_status_label,
                ((action->action_code == 320u) &&
                 (g_ui_sweep_path == 3u)) ?
                "状态:先测量RG,随后三回路扫压" :
                action->status_text);
        }
        else
        {
            lv_label_set_text(g_status_label,
                              "状态:指令发送失败,重新尝试");
        }
    }
}

static void UI_HomeButtonEvent(lv_event_t *event)
{
    uint32_t page;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    page = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    UI_RecordAction(page);

    switch (page)
    {
        case UI_PAGE_INITIALIZE:
            UI_ShowInitialize();
            break;

        case UI_PAGE_MEASURE:
            UI_ShowMeasure();
            break;

        case UI_PAGE_SWEEP:
            UI_ShowSweep();
            break;

        case UI_PAGE_RESULTS:
            UI_ShowResults();
            break;

        default:
            UI_ShowHome();
            break;
    }
}

static void UI_ShowHome(void)
{
    static const char *const button_text[] =
    {
        "系统复位\n复位后端与安全启动",
        "单点测量\n单回路与全部",
        "扫压测量\n显示MOSFET C-V曲线",
        "结果汇总\n数据与波形"
    };
    static const uint32_t button_colors[] =
    {
        UI_COLOR_BLUE,
        UI_COLOR_CYAN,
        UI_COLOR_GREEN,
        UI_COLOR_ORANGE
    };
    lv_obj_t *screen = g_page_containers[UI_PAGE_HOME];
    lv_coord_t button_width;
    lv_coord_t button_height;
    lv_coord_t horizontal_gap = 22;
    lv_coord_t vertical_gap = 18;
    lv_coord_t button_top;
    uint32_t index;

    g_ui_current_page = UI_PAGE_HOME;

    if (g_page_built[UI_PAGE_HOME] != 0u)
    {
        UI_ActivatePage(UI_PAGE_HOME);
        return;
    }

    UI_CreateText(screen,
                  "MOSFET 参数测试系统",
                  &ui_font_cn_28,
                  UI_COLOR_TEXT,
                  UI_PAGE_MARGIN,
                  64,
                  (lv_coord_t)LCD_PIXEL_WIDTH - (2 * UI_PAGE_MARGIN),
                  38,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateText(screen,
                  "C-V / RG 参数测量终端",
                  &ui_font_cn_20,
                  UI_COLOR_TEXT_MUTED,
                  UI_PAGE_MARGIN,
                  102,
                  (lv_coord_t)LCD_PIXEL_WIDTH - (2 * UI_PAGE_MARGIN),
                  30,
                  LV_TEXT_ALIGN_CENTER);

    button_width = ((lv_coord_t)LCD_PIXEL_WIDTH - 96 -
                    horizontal_gap) / 2;
    button_height = (LCD_PIXEL_HEIGHT > 500u) ? 156 : 110;
    button_top = (LCD_PIXEL_HEIGHT > 500u) ? 142 : 132;

    for (index = 0u; index < 4u; ++index)
    {
        lv_coord_t column = (lv_coord_t)(index % 2u);
        lv_coord_t row = (lv_coord_t)(index / 2u);
        lv_coord_t x = 48 + column * (button_width + horizontal_gap);
        lv_coord_t y = button_top + row *
                       (button_height + vertical_gap);

        lv_obj_t *button = UI_CreateButton(
            screen,
            button_text[index],
            UI_COLOR_SURFACE_LIGHT,
            x,
            y,
            button_width,
            button_height,
            UI_HomeButtonEvent,
            (void *)(uintptr_t)(index + 1u));

        lv_obj_set_style_border_color(button,
                                      lv_color_hex(button_colors[index]),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
    }

    g_page_built[UI_PAGE_HOME] = 1u;
    UI_ActivatePage(UI_PAGE_HOME);
}

static void UI_InitModalCancelEvent(lv_event_t *event)
{
    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    UI_RecordAction(202u);
    if (g_modal_overlay != NULL)
    {
        lv_obj_del(g_modal_overlay);
        g_modal_overlay = NULL;
    }
    if (g_status_label != NULL)
    {
        lv_label_set_text(g_status_label, "状态:复位操作取消");
    }
}

static void UI_InitModalConfirmEvent(lv_event_t *event)
{
    bool command_queued;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    UI_RecordAction(203u);
    ++g_ui_reset_request_count;
    command_queued = DisplayProtocol_RequestReset();
    if (command_queued)
    {
        /* ★ 复位把三项参数改回默认了,按钮高亮**必须跟着从 getter 重算** ——
         *   否则会出现"状态行写着 10.00MSPS、采样率那行却还亮着 5MSPS"这种
         *   自相矛盾(2026-10-06 上板实测到)。
         *   ⚠️ 只补高亮:状态行的**文字**由 `MeasureConfig_RestoreDefaults()`
         *   内部已经刷过,这里不重复调,免得两处都能刷新、以后改一处漏一处。 */
        UI_UpdateInitConfigStyle();
    }
    if (g_modal_overlay != NULL)
    {
        lv_obj_del(g_modal_overlay);
        g_modal_overlay = NULL;
    }
    if (g_status_label != NULL)
    {
        lv_label_set_text(g_status_label,
                          command_queued ?
                          "状态:复位指令已发送" :
                          "状态:复位指令发送失败");
    }
}

static void UI_InitResetEvent(lv_event_t *event)
{
    lv_obj_t *screen;
    lv_obj_t *dialog;
    lv_coord_t dialog_width;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED) ||
        (g_modal_overlay != NULL))
    {
        return;
    }

    UI_RecordAction(201u);
    screen = lv_scr_act();
    g_modal_overlay = UI_CreatePanel(screen,
                                     0,
                                     0,
                                     (lv_coord_t)LCD_PIXEL_WIDTH,
                                     (lv_coord_t)LCD_PIXEL_HEIGHT,
                                     0x020617u,
                                     0);
    lv_obj_set_style_bg_opa(g_modal_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_modal_overlay, 0, LV_PART_MAIN);

    dialog_width = (lv_coord_t)LCD_PIXEL_WIDTH - 220;
    if (dialog_width > 560)
    {
        dialog_width = 560;
    }
    dialog = UI_CreatePanel(g_modal_overlay,
                            ((lv_coord_t)LCD_PIXEL_WIDTH - dialog_width) / 2,
                            ((lv_coord_t)LCD_PIXEL_HEIGHT - 230) / 2,
                            dialog_width,
                            230,
                            UI_COLOR_SURFACE_LIGHT,
                            18);
    UI_CreateText(dialog,
                  "确认发送复位指令?",
                  &ui_font_cn_28,
                  UI_COLOR_TEXT,
                  20,
                  28,
                  dialog_width - 40,
                  40,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateText(dialog,
                  "显示板只发送复位请求,其他安全流程由测量后端完成",
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  28,
                  84,
                  dialog_width - 56,
                  56,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateButton(dialog,
                    "取消",
                    UI_COLOR_BORDER,
                    42,
                    160,
                    (dialog_width - 110) / 2,
                    50,
                    UI_InitModalCancelEvent,
                    NULL);
    UI_CreateButton(dialog,
                    "确认",
                    UI_COLOR_RED,
                    68 + (dialog_width - 110) / 2,
                    160,
                    (dialog_width - 110) / 2,
                    50,
                    UI_InitModalConfirmEvent,
                    NULL);
}

/**
 * @brief 提示框的「知道了」回调。
 *
 * ⚠️ **不复用 `UI_InitModalCancelEvent`** —— 那个会把状态行改写成
 *    「状态:复位操作取消」,对一条硬件提示是错文案。
 */
static void UI_NoticeCloseEvent(lv_event_t *event)
{
    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }
    if (g_modal_overlay != NULL)
    {
        lv_obj_del(g_modal_overlay);
        g_modal_overlay = NULL;
    }
}

/**
 * @brief 只读提示框:标题 + 正文 + 一个「知道了」。
 *
 * 🆕 2026-10-10,给 `app/relay_view.c` 报"继电器/档位读回异常"用。
 *
 * 🔴 **不用 `lv_msgbox`** —— `app/lv_conf.h` 里 `LV_USE_MSGBOX` 是 **0**,
 *    而且本工程从头到尾没人用过它。这里照抄上面「系统复位」确认框那套
 *    自建写法(`UI_CreatePanel` + `UI_CreateText` + `UI_CreateButton`)。
 *
 * ⚠️ **与确认框共用 `g_modal_overlay`** ⇒ 已经有一层弹框时**直接返回**,
 *    免得把用户正在等的确认框冲掉。也正因如此,调用方(relay_view.c)
 *    必须自己按值去重,否则异常持续存在时会每轮都来敲一次门。
 *
 * ⚠️ **文案要过字库**:`ui_font_cn_20` / `ui_font_cn_28` 都是按 `--symbols`
 *    生成的**子集**(588 个符号),缺字只会静默显示成空白,编译器不报。
 */
void UI_Demo_ShowAlert(const char *title, const char *body)
{
    lv_obj_t *dialog;
    lv_coord_t dialog_width;

    if ((title == NULL) || (body == NULL) || (g_modal_overlay != NULL))
    {
        return;
    }

    g_modal_overlay = UI_CreatePanel(lv_scr_act(),
                                     0,
                                     0,
                                     (lv_coord_t)LCD_PIXEL_WIDTH,
                                     (lv_coord_t)LCD_PIXEL_HEIGHT,
                                     0x020617u,
                                     0);
    lv_obj_set_style_bg_opa(g_modal_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_modal_overlay, 0, LV_PART_MAIN);

    dialog_width = (lv_coord_t)LCD_PIXEL_WIDTH - 220;
    if (dialog_width > 520)
    {
        dialog_width = 520;
    }
    dialog = UI_CreatePanel(g_modal_overlay,
                            ((lv_coord_t)LCD_PIXEL_WIDTH - dialog_width) / 2,
                            ((lv_coord_t)LCD_PIXEL_HEIGHT - 210) / 2,
                            dialog_width,
                            210,
                            UI_COLOR_SURFACE_LIGHT,
                            18);
    UI_CreateText(dialog,
                  title,
                  &ui_font_cn_28,
                  UI_COLOR_TEXT,
                  20,
                  26,
                  dialog_width - 40,
                  40,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateText(dialog,
                  body,
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  28,
                  82,
                  dialog_width - 56,
                  56,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateButton(dialog,
                    "知道了",
                    UI_COLOR_BLUE,
                    (dialog_width - 120) / 2,
                    150,
                    120,
                    48,
                    UI_NoticeCloseEvent,
                    NULL);
}

/* ==========================================================================
 * 🆕 2026-10-06「系统复位」页的测量参数控件
 *
 * 【档位表为什么放在界面这一层】
 *   按钮文字("10MSPS" vs "10M")是**给用户看的**,属于界面;
 *   数值本身与下发归 `measure_config.c`。分开之后,改档位表只动这里,
 *   改下发/回读/窗口计算只动那边,不会互相牵连。
 *
 * 【档位表要和 `measure_config.h` 的默认值对得上】
 *   `1MHz` 那档必须是 `CV_DDS_HZ_DEFAULT` —— 否则上电后没有任何一个按钮
 *   是高亮的,一看就知道错。
 *   (原表里还有一条 `10MSPS` 那档必须是 `CV_ADC_DIV_DEFAULT`;
 *    **采样率行 2026-10-08 已删**,那条约束随之消失。)
 * ========================================================================== */

/* 🔴 **2026-10-08 档位表由 5 档改成 4 档:`10k / 100k / 1M / 10M`;
 *    同日随后收成 3 档 —— 10 MHz 删掉(理由见下)。**
 *   ⛔ 原表 `{100k, 500k, 1M, 2M, 5M}` 的问题不是"不好",而是
 *      **它从没跟采样率一起被验算过**:采样率统一固定 65 MSPS(不更改)之后,
 *      解调要求窗覆盖整数个激励周期,而 65 MSPS 下**最低可用激励 = f_s/250 内**,
 *      10 kHz 与 100 kHz 在旧的 4096 点窗下**一个都放不下**
 *      (`q = 6500 / 650`),继续摆着就是"组合不合理"本身。
 *   ⇒ 配套改了采集缓冲(4096 → 8192,`adc_capture.v`)才让这些档成立:
 *
 *     | 激励 | q | 窗长 L | 整周期 m | 点/周期 |
 *     |---|---|---|---|---|
 *     | 10 kHz  | 6500 | 6500 |    1 | 6500 |
 *     | 100 kHz |  650 | 7800 |   12 |   65 |
 *     | 1 MHz   |   65 | 8190 |  126 |   65 |
 *     | ~~10 MHz~~ | ~~13~~ | ~~8190~~ | ~~1260~~ | ~~6.5~~ |  ← 已删
 *
 *   🔴 **10 MHz 为什么删:算术成立、物理不成立。**
 *      AD9833 的 MCLK 是**焊死的 25 MHz**(决策 D10,FPGA 供不了),
 *      `f_out = code × 25 MHz / 2^28` ⇒ 10 MHz 只有 **2.5 个 DAC 点/周期**,
 *      而且 15 MHz 镜像离主频只隔 5 MHz。**实测输出不了**(2026-10-08 用户判定),
 *      ⇒ 这一档留在表上就是又一个"点了没反应"。
 *      ⛔ 别按"再调调参数就行"处理:这是 DAC 采样定理的硬限制,不是配置问题。
 *      要 10 MHz 只能换激励源(MCLK ≥ 100 MHz 的 DDS),属硬件改动。
 *
 *   ⚠️ 10 kHz 那一档 `m = 1` ⇒ 激励正好落在**第 1 根谱线**上,
 *      整窗内的基线漂移会直接进信号 bin(DC 不会,整周期 Σcos = 0 精确抵消)。
 *      判据:同一器件同一偏压下,10 kHz 与 100 kHz 反演的 Cs 应当接近;
 *      差得多则嫌疑是漂移。
 */
static const uint32_t g_init_dds_choices[UI_INIT_DDS_COUNT] =
    {10000u, 100000u, 1000000u};
static const char *const g_init_dds_texts[UI_INIT_DDS_COUNT] =
    {"10kHz", "100kHz", "1MHz"};

static const int32_t g_init_phase_deltas[UI_INIT_PHASE_COUNT] = {-1, 0, 1};
static const char *const g_init_phase_texts[UI_INIT_PHASE_COUNT] =
    {"相位 -1", "居中", "相位 +1"};

/* 动作码。避开既有占用:100/101、201–203、210–214、220、240–242、310–313、320–322。
 * 250–253 给频率档位(现用 250–252,3 档),261–263 给相位(3 档)—— 与既有的分段习惯一致。
 * ⚠️ **253–260 现在是空的**:253 是 10 MHz 删除后空出来的,255–260 原属采样率段
 *    (2026-10-08 随该行删除)。相位段**故意留在 261 不动**:挪它要改历史记录里的
 *    动作码,而空着的码没有代价。将来加档位优先用 253,再往下用。 */
#define UI_INIT_ACTION_DDS_BASE    250u
#define UI_INIT_ACTION_PHASE_BASE  261u

/** @brief 一组档位按钮的选中态:选中的换底色 + 2 px 边框(照 UI_UpdateSinglePathStyle 的写法)。
 *  @param selected 选中下标;传 `count` 或更大值时**一个都不选**(相位行用得上)。 */
static void UI_InitStyleRow(lv_obj_t *const *buttons, uint32_t count,
                            uint32_t selected)
{
    uint32_t index;

    for (index = 0u; index < count; ++index)
    {
        lv_obj_t *button = buttons[index];

        if (button == NULL)
        {
            continue;
        }

        if (index == selected)
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(UI_COLOR_CYAN),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(UI_COLOR_CYAN),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(UI_COLOR_SURFACE),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(UI_COLOR_BORDER),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
        }
    }
}

/** @brief 在档位表里找 @p value 的下标。找不到返回 @p count(⇒ 一个都不高亮)。 */
static uint32_t UI_ChoiceIndex(const uint32_t *choices, uint32_t count,
                               uint32_t value)
{
    uint32_t index;

    for (index = 0u; index < count; ++index)
    {
        if (choices[index] == value)
        {
            return index;
        }
    }

    return count;
}

/** @brief 按当前参数(而不是按本地的点击记录)重算两行的高亮。
 *
 *  ⚠️ 原来还有一行"采样率",2026-10-08 随该行删除 —— 采样率固定 65 MSPS。
 *     它的高亮原本靠 `MeasureConfig_GetSampleDiv()` 对 `g_init_div_choices`
 *     取下标,现在连表一起没了,别再往这里加回来。 */
static void UI_UpdateInitConfigStyle(void)
{
    UI_InitStyleRow(g_init_dds_buttons, UI_INIT_DDS_COUNT,
                    UI_ChoiceIndex(g_init_dds_choices, UI_INIT_DDS_COUNT,
                                   MeasureConfig_GetDdsHz()));
    /* 相位是连续量,没有"当前档";只有"是不是正好在窗中心"。
     * 不在中心时传 count ⇒ 三个按钮都不高亮,如实反映"手动偏了"。 */
    UI_InitStyleRow(g_init_phase_buttons, UI_INIT_PHASE_COUNT,
                    MeasureConfig_IsPhaseCentered() ? 1u : UI_INIT_PHASE_COUNT);
}

/**
 * @brief 从 MeasureConfig **主动拉一次**当前参数,填进常驻状态行。
 *
 * 🔴 **为什么必须有这个函数**:本页是**首次进入才构建**的,而上电时
 *    `MeasureConfig_Init()`(在 `UI_Demo_Init()` 之后)就已经下发过参数、
 *    也推过一次状态行了 —— 但那时标签还不存在,`UI_Demo_SetInitStatusText()`
 *    按设计**静默丢弃**。不在这里补拉一次,页面会带着**空白状态行**出现,
 *    看起来像"参数没下发"。控件可以靠 `UI_UpdateInitConfigStyle()` 从
 *    getter 恢复高亮,文字不行 —— 文字只有这一个来源。
 */
static void UI_RefreshInitStatusLine(void)
{
    char line[96];

    MeasureConfig_FormatLine(line, sizeof(line));
    UI_Demo_SetInitStatusText(line);
}

/**
 * @brief 顶栏那一行也报一声(纯文本)。参数读数在常驻状态行里,这里只说"做了什么"。
 *
 * ⚠️ 带格式的那几条**不走这个函数**,直接在调用点用 `lv_label_set_text_fmt`
 *    (照 `UI_UpdateCommonStatus()` 的既有写法)—— 本文件**没有** include
 *    `<stdio.h>` 也没有 `<stdarg.h>`(前序就是这样,里面 19 处 "snprintf"
 *    其实全是 `lv_snprintf`)。为一个提示去动前序的 include 块不划算。
 */
static void UI_InitAnnounce(const char *text)
{
    if ((text != NULL) && (g_status_label != NULL))
    {
        lv_label_set_text(g_status_label, text);
    }
}

/* user_data 里装的是**下标 + 1**(0 会被 LVGL 当成 NULL user_data,不能用)。 */
static uint32_t UI_InitEventIndex(lv_event_t *event)
{
    return (uint32_t)(uintptr_t)lv_event_get_user_data(event);
}

static void UI_InitDdsEvent(lv_event_t *event)
{
    uint32_t index;

    if ((event == NULL) || (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    index = UI_InitEventIndex(event);
    /* 🔴 **上界一律写 `UI_INIT_DDS_COUNT`,不要再写字面量。**
     *    2026-10-08 上板踩到的"按钮点了没反应"就是这里写死成旧的档位数 5,
     *    而该行已扩到 6 档 ⇒ 最后一颗传进来 `index = 6` **当场静默 return**
     *    (触摸有事件、回调不执行、无任何日志)。
     *    现在档位数由宏导出,改表就自动改到这里 —— 编译期对齐,不靠人记。 */
    if ((index == 0u) || (index > UI_INIT_DDS_COUNT))
    {
        return;
    }
    index--;

    UI_RecordAction(UI_INIT_ACTION_DDS_BASE + index);
    (void)MeasureConfig_SetDdsHz(g_init_dds_choices[index]);
    UI_UpdateInitConfigStyle();

    if (g_status_label != NULL)
    {
        lv_label_set_text_fmt(g_status_label, "状态:激励已设为 %s",
                              g_init_dds_texts[index]);
    }
}

/* ⛔ `UI_InitDivEvent()` 已于 2026-10-08 **删除** —— 采样率固定 65 MSPS,
 *    该行连同 `g_init_div_choices/texts`、`g_init_div_buttons`、
 *    `UI_INIT_ACTION_DIV_BASE` 一起移除。
 *    📌 历史:`LLM-054` 记录的"65MSPS 那颗按钮点了没反应"就发生在这个函数里
 *    (守卫写死 `> 5u`,而该行已扩到 6 档)。**留着这行就等于留着那个缺陷的宿主。**
 *    采样率本身仍由 `CV_ADC_DIV_DEFAULT`(= 2 → 65 MSPS)下发,
 *    并显示在常驻状态行里(`MeasureConfig_FormatLine()`)。 */

static void UI_InitPhaseEvent(lv_event_t *event)
{
    uint32_t index;

    if ((event == NULL) || (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    index = UI_InitEventIndex(event);
    if ((index == 0u) || (index > 3u))
    {
        return;
    }
    index--;

    UI_RecordAction(UI_INIT_ACTION_PHASE_BASE + index);

    if (g_init_phase_deltas[index] == 0)
    {
        (void)MeasureConfig_CenterPhase();
        UI_InitAnnounce("状态:相位已回到窗口中心");
    }
    else if (!MeasureConfig_NudgePhase(g_init_phase_deltas[index]))
    {
        /* 越出采样窗 —— **没有写下去**。如实说,别让用户以为动了。 */
        UI_InitAnnounce("状态:相位已到采样窗边缘,未改动");
    }
    else if (g_status_label != NULL)
    {
        lv_label_set_text_fmt(g_status_label, "状态:相位已调到 %+d",
                              (int)MeasureConfig_GetPhase());
    }

    UI_UpdateInitConfigStyle();
}

static void UI_ShowInitialize(void)
{
    lv_obj_t *screen = g_page_containers[UI_PAGE_INITIALIZE];
    lv_obj_t *panel;
    lv_coord_t panel_width = (lv_coord_t)LCD_PIXEL_WIDTH - 220;
    /* 三行参数控件共用的排版参数(与 UI_CreatePathButtons 的取法一致) */
    lv_coord_t row_x = 152;
    lv_coord_t row_gap = 10;
    lv_coord_t row_width;
    uint32_t row_count;
    uint32_t index;

    g_ui_current_page = UI_PAGE_INITIALIZE;

    if (g_page_built[UI_PAGE_INITIALIZE] != 0u)
    {
        UI_ActivatePage(UI_PAGE_INITIALIZE);
        return;
    }
    panel = UI_CreatePanel(screen,
                           110,
                           94,
                           panel_width,
                           400,          /* 原 330 —— 下面加了状态行 + 三行参数 */
                           UI_COLOR_SURFACE_LIGHT,
                           20);
    lv_obj_set_style_border_color(panel,
                                  lv_color_hex(UI_COLOR_RED),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    UI_CreateText(panel,
                  "安全初始化",
                  &ui_font_cn_28,
                  UI_COLOR_TEXT,
                  24,
                  20,
                  panel_width - 48,
                  36,
                  LV_TEXT_ALIGN_CENTER);
    UI_CreateText(panel,
                  "显示板只发送 mcu_reset 指令\n偏压降低与安全等待由测量后端完成",
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  36,
                  60,
                  panel_width - 72,
                  52,
                  LV_TEXT_ALIGN_CENTER);

    UI_CreateButton(panel,
                    "发送复位指令",
                    UI_COLOR_RED,
                    (panel_width - 320) / 2,
                    120,
                    320,
                    56,
                    UI_InitResetEvent,
                    NULL);

    /* ★ 常驻状态行 —— **就在复位按钮正下方**,不随操作消失,每次参数改动后刷新。
     *   内容由 `MeasureConfig_FormatLine()` 拼(激励 / 采样率 / 相位)。
     *   偏压与回路**不在这儿重复** —— 它们在常驻顶栏(`g_header_bias_label`),
     *   重复一份就多一个"两处显示不一致"的机会。 */
    g_init_status_label =
        UI_CreateText(panel, "", &ui_font_cn_20, UI_COLOR_CYAN,
                      24, 184, panel_width - 48, 28, LV_TEXT_ALIGN_CENTER);

    /* ---- 第 1 行:激励频率(0x80)------------------------------------ */
    UI_CreateText(panel,
                  "激励频率",
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  18, 224, 120, 30,
                  LV_TEXT_ALIGN_LEFT);
    row_count = UI_INIT_DDS_COUNT;
    row_width = (panel_width - 18 - row_x -
                 ((lv_coord_t)row_count - 1) * row_gap) / (lv_coord_t)row_count;
    for (index = 0u; index < row_count; ++index)
    {
        g_init_dds_buttons[index] =
            UI_CreateButton(panel,
                            g_init_dds_texts[index],
                            UI_COLOR_SURFACE,
                            row_x + (lv_coord_t)index * (row_width + row_gap),
                            216,
                            row_width,
                            44,
                            UI_InitDdsEvent,
                            (void *)(uintptr_t)(index + 1u));
    }

    /* ---- 第 2 行:采样相位微调(给手动找眼用)-----------------------
     * ⛔ 原来的"第 2 行:采样率"已于 2026-10-08 删除(采样率固定 65 MSPS),
     *    所以相位行**从 y = 328 上移到 y = 272**,顶掉空出来的那一格。
     *    ⚠️ 上移要连标签一起移(336 → 280),只移按钮会让标签悬在下一行的位置。 */
    UI_CreateText(panel,
                  "采样相位",
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  18, 280, 120, 30,
                  LV_TEXT_ALIGN_LEFT);
    row_count = UI_INIT_PHASE_COUNT;
    row_width = (panel_width - 18 - row_x -
                 ((lv_coord_t)row_count - 1) * row_gap) / (lv_coord_t)row_count;
    for (index = 0u; index < row_count; ++index)
    {
        g_init_phase_buttons[index] =
            UI_CreateButton(panel,
                            g_init_phase_texts[index],
                            UI_COLOR_SURFACE,
                            row_x + (lv_coord_t)index * (row_width + row_gap),
                            272,
                            row_width,
                            44,
                            UI_InitPhaseEvent,
                            (void *)(uintptr_t)(index + 1u));
    }

    /* ★ 按**当前参数**点亮,不是按默认值 —— 界面可能是在参数已被改过之后才
     *   第一次构建的(切页顺序不保证),照默认值点亮会和状态行自相矛盾。 */
    UI_UpdateInitConfigStyle();
    /* ★ 状态行的文字同理,必须**主动拉**:上电那次推送发生在本页构建之前,
     *   已经被静默丢弃了(详见 UI_RefreshInitStatusLine 的注释)。 */
    UI_RefreshInitStatusLine();

    UI_CreateText(screen,
                  "二次确认后发送指令   ·   采样率固定 65 MSPS(不可调,发复位即回此档)",
                  &ui_font_cn_20,
                  UI_COLOR_TEXT_MUTED,
                  60,
                  506,
                  (lv_coord_t)LCD_PIXEL_WIDTH - 120,
                  32,
                  LV_TEXT_ALIGN_CENTER);

    g_page_built[UI_PAGE_INITIALIZE] = 1u;
    UI_ActivatePage(UI_PAGE_INITIALIZE);
}

static void UI_CreatePathButtons(lv_obj_t *panel,
                                 lv_coord_t panel_width,
                                 const char *const *names,
                                 const UI_Action *actions,
                                 uint32_t action_count)
{
    lv_coord_t start_x = 152;
    lv_coord_t gap = 10;
    lv_coord_t button_width =
        (panel_width - start_x - 18 -
         ((lv_coord_t)action_count - 1) * gap) /
        (lv_coord_t)action_count;
    uint32_t index;

    UI_CreateText(panel,
                  "测量回路",
                  &ui_font_cn_20,
                  UI_COLOR_STATUS,
                  18,
                  24,
                  120,
                  30,
                  LV_TEXT_ALIGN_LEFT);

    for (index = 0u; index < action_count; ++index)
    {
        lv_obj_t *button =
            UI_CreateButton(panel,
                            names[index],
                            UI_COLOR_CYAN,
                            start_x + (lv_coord_t)index *
                            (button_width + gap),
                            13,
                            button_width,
                            52,
                            UI_SimpleActionEvent,
                            (void *)&actions[index]);

        if ((actions == g_single_path_actions) && (index < 5u))
        {
            g_single_path_buttons[index] = button;
        }
        else if ((actions == g_sweep_path_actions) && (index < 4u))
        {
            g_sweep_path_buttons[index] = button;
        }
    }

    if (actions == g_single_path_actions)
    {
        UI_UpdateSinglePathStyle();
    }
    else if (actions == g_sweep_path_actions)
    {
        UI_UpdateSweepPathStyle();
    }
}

static void UI_BiasKeypadRefresh(void)
{
    if (g_keypad_value_label == NULL)
    {
        return;
    }

    if (g_keypad_buffer[0] == '\0')
    {
        lv_label_set_text(g_keypad_value_label, "0.0 V");
    }
    else
    {
        lv_label_set_text_fmt(g_keypad_value_label,
                              "%s V",
                              g_keypad_buffer);
    }
}

static uint32_t UI_ParseBiasTenths(uint32_t *bias_tenths)
{
    uint32_t integer_part = 0u;
    uint32_t fraction_part = 0u;
    uint32_t decimal_seen = 0u;
    uint32_t fraction_digits = 0u;
    uint32_t digit_seen = 0u;
    uint32_t index;

    if (bias_tenths == NULL)
    {
        return 0u;
    }

    for (index = 0u; g_keypad_buffer[index] != '\0'; ++index)
    {
        char character = g_keypad_buffer[index];

        if (character == '.')
        {
            if (decimal_seen != 0u)
            {
                return 0u;
            }
            decimal_seen = 1u;
            continue;
        }

        if ((character < '0') || (character > '9'))
        {
            return 0u;
        }

        digit_seen = 1u;
        if (decimal_seen == 0u)
        {
            integer_part = integer_part * 10u +
                           (uint32_t)(character - '0');
            if (integer_part > 100u)
            {
                return 0u;
            }
        }
        else
        {
            if (fraction_digits >= 1u)
            {
                return 0u;
            }
            fraction_part = (uint32_t)(character - '0');
            ++fraction_digits;
        }
    }

    if (digit_seen == 0u)
    {
        return 0u;
    }

    *bias_tenths = integer_part * 10u + fraction_part;
    if ((*bias_tenths < UI_BIAS_MIN_TENTHS) ||
        (*bias_tenths > UI_BIAS_MAX_TENTHS))
    {
        return 0u;
    }

    return 1u;
}

static void UI_CloseBiasKeypad(void)
{
    if (g_keypad_overlay != NULL)
    {
        lv_obj_add_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void UI_BiasKeypadEvent(lv_event_t *event)
{
    uint32_t action;
    size_t length;
    char *decimal;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    action = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    length = strlen(g_keypad_buffer);
    decimal = strchr(g_keypad_buffer, '.');

    if (action <= 9u)
    {
        uint32_t integer_digits =
            (decimal == NULL) ? (uint32_t)length :
            (uint32_t)(decimal - g_keypad_buffer);
        uint32_t fraction_digits =
            (decimal == NULL) ? 0u :
            (uint32_t)(length - (size_t)(decimal - g_keypad_buffer) - 1u);

        if ((length < (sizeof(g_keypad_buffer) - 1u)) &&
            (((decimal == NULL) && (integer_digits < 3u)) ||
             ((decimal != NULL) && (fraction_digits < 1u))))
        {
            g_keypad_buffer[length] = (char)('0' + action);
            g_keypad_buffer[length + 1u] = '\0';
        }
    }
    else if (action == UI_KEYPAD_DECIMAL)
    {
        if ((decimal == NULL) &&
            (length < (sizeof(g_keypad_buffer) - 1u)))
        {
            if (length == 0u)
            {
                g_keypad_buffer[0] = '0';
                g_keypad_buffer[1] = '.';
                g_keypad_buffer[2] = '\0';
            }
            else
            {
                g_keypad_buffer[length] = '.';
                g_keypad_buffer[length + 1u] = '\0';
            }
        }
    }
    else if (action == UI_KEYPAD_BACKSPACE)
    {
        if (length != 0u)
        {
            g_keypad_buffer[length - 1u] = '\0';
        }
    }
    else if (action == UI_KEYPAD_CLEAR)
    {
        g_keypad_buffer[0] = '\0';
    }
    else if (action == UI_KEYPAD_CANCEL)
    {
        UI_RecordAction(241u);
        UI_CloseBiasKeypad();
        if (g_status_label != NULL)
        {
            lv_label_set_text(g_status_label, "状态:偏压设置取消");
        }
        return;
    }
    else if (action == UI_KEYPAD_MINIMUM)
    {
        (void)strcpy(g_keypad_buffer, "0.1");
    }
    else if (action == UI_KEYPAD_CONFIRM)
    {
        uint32_t bias_tenths;

        if (UI_ParseBiasTenths(&bias_tenths) == 0u)
        {
            if (g_keypad_note_label != NULL)
            {
                lv_label_set_text(g_keypad_note_label,
                                  "输入错误  偏压:0.1-100.0 V");
                lv_obj_set_style_text_color(g_keypad_note_label,
                                            lv_color_hex(UI_COLOR_RED),
                                            LV_PART_MAIN);
            }
            return;
        }
        if ((g_keypad_target == UI_BIAS_TARGET_ANALYSIS) &&
            ((g_sweep_inspection_ready == 0u) ||
             (bias_tenths > g_ui_analysis_voltage_max_tenths)))
        {
            if (g_keypad_note_label != NULL)
            {
                lv_label_set_text_fmt(
                    g_keypad_note_label,
                    "输入错误  参数电压:0.1-%lu.%lu V",
                    (unsigned long)(g_ui_analysis_voltage_max_tenths / 10u),
                    (unsigned long)(g_ui_analysis_voltage_max_tenths % 10u));
                lv_obj_set_style_text_color(g_keypad_note_label,
                                            lv_color_hex(UI_COLOR_RED),
                                            LV_PART_MAIN);
            }
            return;
        }

        if (g_keypad_target == UI_BIAS_TARGET_ANALYSIS)
        {
            g_ui_analysis_voltage_tenths = bias_tenths;
            UI_UpdateSweepAnalysisCard();
        }
        else if (g_keypad_target == UI_BIAS_TARGET_SWEEP)
        {
            g_ui_sweep_end_tenths = bias_tenths;
            g_ui_display_path = (g_ui_sweep_path == 3u) ?
                                UI_DISPLAY_PATH_ALL : g_ui_sweep_path;
            UI_UpdateSweepBiasLabel();
        }
        else
        {
            g_ui_single_bias_tenths = bias_tenths;
            g_ui_display_path = (g_ui_single_path == UI_DISPLAY_PATH_ALL) ?
                                UI_DISPLAY_PATH_ALL : g_ui_single_path;
            UI_UpdateSingleBiasLabel();
        }
        UI_RecordAction(242u);
        UI_CloseBiasKeypad();
        if (g_status_label != NULL)
        {
            if (g_keypad_target == UI_BIAS_TARGET_ANALYSIS)
            {
                lv_label_set_text_fmt(
                    g_status_label,
                    "参数电压:%lu.%lu V",
                    (unsigned long)(bias_tenths / 10u),
                    (unsigned long)(bias_tenths % 10u));
            }
            else if ((g_keypad_target == UI_BIAS_TARGET_SINGLE) &&
                (g_ui_single_path != UI_SINGLE_PATH_RG))
            {
                if (DisplayProtocol_RequestBias(bias_tenths))
                {
                    lv_label_set_text_fmt(
                        g_status_label,
                        "状态:偏压指令发送完成:%lu.%lu V",
                        (unsigned long)(bias_tenths / 10u),
                        (unsigned long)(bias_tenths % 10u));
                }
                else
                {
                    lv_label_set_text(g_status_label,
                                      "状态:指令发送失败");
                }
            }
            else
            {
                lv_label_set_text_fmt(
                    g_status_label,
                    "状态:测量参数设置完成:%lu.%lu V",
                    (unsigned long)(bias_tenths / 10u),
                    (unsigned long)(bias_tenths % 10u));
            }
        }
        return;
    }

    if (g_keypad_note_label != NULL)
    {
        if (g_keypad_target == UI_BIAS_TARGET_ANALYSIS)
        {
            lv_label_set_text_fmt(
                g_keypad_note_label,
                "参数电压:0.1-%lu.%lu V",
                (unsigned long)(g_ui_analysis_voltage_max_tenths / 10u),
                (unsigned long)(g_ui_analysis_voltage_max_tenths % 10u));
        }
        else
        {
            lv_label_set_text(g_keypad_note_label, "偏压:0.1-100.0 V");
        }
        lv_obj_set_style_text_color(g_keypad_note_label,
                                    lv_color_hex(UI_COLOR_TEXT_MUTED),
                                    LV_PART_MAIN);
    }
    UI_BiasKeypadRefresh();
}

static void UI_CreateBiasKeypad(void)
{
    static const char *const key_text[] =
    {
        "7", "8", "9", "<-",
        "4", "5", "6", "清空",
        "1", "2", "3", "取消",
        "0", ".", "0.1", "确认"
    };
    static const uint32_t key_action[] =
    {
        7u, 8u, 9u, UI_KEYPAD_BACKSPACE,
        4u, 5u, 6u, UI_KEYPAD_CLEAR,
        1u, 2u, 3u, UI_KEYPAD_CANCEL,
        0u, UI_KEYPAD_DECIMAL, UI_KEYPAD_MINIMUM, UI_KEYPAD_CONFIRM
    };
    lv_obj_t *dialog;
    lv_obj_t *value_panel;
    lv_coord_t dialog_width;
    lv_coord_t dialog_x;
    lv_coord_t button_width;
    lv_coord_t gap = 8;
    uint32_t index;

    if (g_keypad_overlay != NULL)
    {
        return;
    }

    g_keypad_overlay = UI_CreatePanel(lv_layer_top(),
                                      0,
                                      0,
                                      (lv_coord_t)LCD_PIXEL_WIDTH,
                                      (lv_coord_t)LCD_PIXEL_HEIGHT,
                                      0x020617u,
                                      0);
    lv_obj_set_style_bg_opa(g_keypad_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_keypad_overlay, 0, LV_PART_MAIN);

    dialog_width = (lv_coord_t)LCD_PIXEL_WIDTH - 180;
    if (dialog_width > 620)
    {
        dialog_width = 620;
    }
    dialog_x = ((lv_coord_t)LCD_PIXEL_WIDTH - dialog_width) / 2;
    dialog = UI_CreatePanel(g_keypad_overlay,
                            dialog_x,
                            ((lv_coord_t)LCD_PIXEL_HEIGHT - 430) / 2,
                            dialog_width,
                            430,
                            UI_COLOR_SURFACE_LIGHT,
                            18);

    g_keypad_title_label =
        UI_CreateText(dialog, "设置偏压", &ui_font_cn_28,
                      UI_COLOR_TEXT, 20, 16,
                      dialog_width - 40, 38, LV_TEXT_ALIGN_CENTER);

    value_panel = UI_CreatePanel(dialog,
                                 24,
                                 58,
                                 dialog_width - 48,
                                 58,
                                 UI_COLOR_BACKGROUND,
                                 10);
    g_keypad_value_label = UI_CreateText(value_panel,
                                         "",
                                         &ui_font_cn_28,
                                         UI_COLOR_TEXT,
                                         12,
                                         12,
                                         dialog_width - 72,
                                         38,
                                         LV_TEXT_ALIGN_RIGHT);
    UI_BiasKeypadRefresh();

    g_keypad_note_label = UI_CreateText(dialog,
                                        "偏压:0.1-100.0 V",
                                        &ui_font_cn_20,
                                        UI_COLOR_TEXT_MUTED,
                                        24,
                                        122,
                                        dialog_width - 48,
                                        28,
                                        LV_TEXT_ALIGN_CENTER);

    button_width = (dialog_width - 72) / 4;
    for (index = 0u; index < 16u; ++index)
    {
        lv_coord_t column = (lv_coord_t)(index % 4u);
        lv_coord_t row = (lv_coord_t)(index / 4u);
        uint32_t color = UI_COLOR_BLUE;

        if (key_action[index] == UI_KEYPAD_CONFIRM)
        {
            color = UI_COLOR_GREEN;
        }
        else if (key_action[index] == UI_KEYPAD_CANCEL)
        {
            color = UI_COLOR_RED;
        }
        else if ((key_action[index] == UI_KEYPAD_CLEAR) ||
                 (key_action[index] == UI_KEYPAD_BACKSPACE))
        {
            color = UI_COLOR_ORANGE;
        }

        UI_CreateButton(dialog,
                        key_text[index],
                        color,
                        24 + column * (button_width + gap),
                        154 + row * 62,
                        button_width,
                        54,
                        UI_BiasKeypadEvent,
                        (void *)(uintptr_t)key_action[index]);
    }

    lv_obj_add_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void UI_BiasOpenEvent(lv_event_t *event)
{
    uint32_t current_bias_tenths;
    const char *dialog_title;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED) ||
        (g_modal_overlay != NULL) ||
        (g_keypad_overlay == NULL) ||
        !lv_obj_has_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN))
    {
        return;
    }

    UI_RecordAction(240u);
    g_keypad_target =
        (UI_BiasTarget)(uintptr_t)lv_event_get_user_data(event);
    if (g_keypad_target == UI_BIAS_TARGET_ANALYSIS)
    {
        current_bias_tenths = g_ui_analysis_voltage_tenths;
        dialog_title = "设置参数电压";
    }
    else if (g_keypad_target == UI_BIAS_TARGET_SWEEP)
    {
        current_bias_tenths = g_ui_sweep_end_tenths;
        dialog_title = "设置终止电压";
    }
    else
    {
        g_keypad_target = UI_BIAS_TARGET_SINGLE;
        current_bias_tenths = g_ui_single_bias_tenths;
        dialog_title = "设置偏压";
    }

    lv_snprintf(g_keypad_buffer,
                sizeof(g_keypad_buffer),
                "%lu.%lu",
                (unsigned long)(current_bias_tenths / 10u),
                (unsigned long)(current_bias_tenths % 10u));
    lv_label_set_text(g_keypad_title_label, dialog_title);
    if (g_keypad_target == UI_BIAS_TARGET_ANALYSIS)
    {
        lv_label_set_text_fmt(
            g_keypad_note_label,
            "参数电压:0.1-%lu.%lu V",
            (unsigned long)(g_ui_analysis_voltage_max_tenths / 10u),
            (unsigned long)(g_ui_analysis_voltage_max_tenths % 10u));
    }
    else
    {
        lv_label_set_text(g_keypad_note_label, "偏压:0.1-100.0 V");
    }
    lv_obj_set_style_text_color(g_keypad_note_label,
                                lv_color_hex(UI_COLOR_TEXT_MUTED),
                                LV_PART_MAIN);
    UI_BiasKeypadRefresh();
    lv_obj_clear_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_keypad_overlay);
}

/*
 * 测量页 —— 原「单点测量」页与「一键测量」页合并而成。
 * 单页内通过「测量回路」的第 5 个选项「全部」在两种测量模式间切换:
 *   选 CISS/COSS/CRSS/RG  → 单回路点测,读回单组结果
 *   选 全部               → 依次测 CISS/COSS/CRSS,三张卡片并排
 * 共用同一个偏压设置(g_ui_single_bias_tenths)与同一个「开始测量」按钮,
 * 由 UI_SimpleActionEvent 的 220u 分支按 g_ui_single_path 派发到不同的协议请求。
 */
static void UI_ShowMeasure(void)
{
    static const char *const path_names[] =
        {"CISS", "COSS", "CRSS", "RG", "全部"};
    lv_obj_t *screen = g_page_containers[UI_PAGE_MEASURE];
    lv_obj_t *path_panel;
    lv_obj_t *control_panel;
    lv_coord_t content_width =
        (lv_coord_t)LCD_PIXEL_WIDTH - (2 * UI_PAGE_MARGIN);
    lv_coord_t result_x = UI_PAGE_MARGIN + 312;
    lv_coord_t result_width = content_width - 312;
    uint32_t index;

    g_ui_current_page = UI_PAGE_MEASURE;
    g_ui_display_bias_tenths = g_ui_single_bias_tenths;
    g_ui_display_path = (g_ui_single_path == UI_DISPLAY_PATH_ALL) ?
                        UI_DISPLAY_PATH_ALL : g_ui_single_path;

    if (g_page_built[UI_PAGE_MEASURE] != 0u)
    {
        UI_ActivatePage(UI_PAGE_MEASURE);
        return;
    }
    path_panel = UI_CreatePanel(screen,
                                UI_PAGE_MARGIN,
                                68,
                                content_width,
                                76,
                                UI_COLOR_SURFACE_LIGHT,
                                14);
    UI_CreatePathButtons(path_panel,
                         content_width,
                         path_names,
                         g_single_path_actions,
                         5u);

    control_panel = UI_CreatePanel(screen,
                                   UI_PAGE_MARGIN,
                                   156,
                                   300,
                                   420,
                                   UI_COLOR_SURFACE_LIGHT,
                                   16);
    lv_obj_set_style_border_color(control_panel,
                                  lv_color_hex(UI_COLOR_BLUE),
                                  LV_PART_MAIN);
    UI_CreateText(control_panel, "测量设置", &ui_font_cn_28,
                  UI_COLOR_TEXT, 20, 18, 260, 38, LV_TEXT_ALIGN_CENTER);
    UI_CreateText(control_panel, "设置目标偏压后开始测量",
                  &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                  20, 62, 260, 30, LV_TEXT_ALIGN_CENTER);
    {
        lv_obj_t *bias_button =
            UI_CreateButton(control_panel,
                            "偏压设置:0.1 V",
                            UI_COLOR_BLUE,
                            20,
                            108,
                            260,
                            58,
                            UI_BiasOpenEvent,
                            (void *)(uintptr_t)UI_BIAS_TARGET_SINGLE);

        g_single_bias_value_label = lv_obj_get_child(bias_button, 0);
        UI_UpdateSingleBiasLabel();
    }

    /* ---- 🆕 2026-10-15:参考电阻挡位 A/B/C/D -------------------------
     * 2×2 网格:可用宽 260(300 − 两侧各 20),两颗 + 8 px 缝 = 260。
     * y 取 180..296,正好落在「偏压设置」底(166)与「开始测量」顶(322)
     * 之间的空档里 —— 没有挪动任何既有控件,所以不会连带改到别处的坐标。
     *
     * 🔴 **挡位为什么必须由人指定**:`Z = R0·(VH−VL)/VL` 里的 R0 就是挡位,
     *    Rg / Cs / Cp / 电流四个数**同时**依赖它。挡位选错不会报错,
     *    只会四个数一起偏 —— 前序 `acquisition_gain.c` 为 RG 强制 A 挡
     *    (10 Ω)正是为了拉开 `VL/VH = R0/(R0+Rg)` 的对比度。
     * ⛔ 前序那套自动换挡**没有抄** —— 它在前序固件里是**死的**
     *    (`sweep_measurement.c:201` 的 `UsesFixedGear()` 硬编码 `return true`,
     *    `pc_protocol.c` 的 `auto` 命令只回 `E,AUTO_DISABLED`),从未验证过。
     *    抄一堆没跑过的阈值不如先让人拍板。 */
    /* ⚠️ **文案用「电阻档位」而不是「参考电阻挡位」** —— 不是简称,是字库。
     *    `--symbols` 子集里**有 `档`(U+6863)、没有 `挡`(U+6321)**,
     *    `考` 也不在其中;缺字在 LVGL 里**静默显示成空白**,编译器一句不报。
     *    2026-10-15 我第一版写的正是「参考电阻挡位」,核字库时才发现
     *    会渲染成「参 电阻 位」。⇒ **加中文文案前先拿 `--symbols` 那行核一遍。** */
    UI_CreateText(control_panel, "电阻档位", &ui_font_cn_20,
                  UI_COLOR_STATUS, 20, 180, 260, 26, LV_TEXT_ALIGN_LEFT);
    for (index = 0u; index < UI_GEAR_COUNT; ++index)
    {
        lv_coord_t gear_x = 20 + (lv_coord_t)(index % 2u) * 134;
        lv_coord_t gear_y = 210 + (lv_coord_t)(index / 2u) * 46;

        g_gear_buttons[index] =
            UI_CreateButton(control_panel,
                            g_gear_texts[index],
                            UI_COLOR_SURFACE,
                            gear_x,
                            gear_y,
                            126,
                            40,
                            UI_SimpleActionEvent,
                            (void *)&g_gear_actions[index]);
    }
    UI_UpdateGearStyle();      /* 上电那一刻还没回读过 → 一颗都不高亮 */
    /* ⚠️ **本轮挡位按钮只放在测量页**。扫压页没有 —— 即扫压前若要换挡,
     *    得先回测量页点一下。这是个**已知取舍**,不是遗漏:
     *      · 挡位是全局设置,顶栏一直显示它 ⇒ 不存在"以为换了其实没换";
     *      · 但扫压页正是最需要它的一处(前序为 RG 扫压强制 A 挡 10 Ω)——
     *        所以这条迟早要补。⛔ 补的时候**别复制这十几行**:
     *        抽成 `UI_CreateGearButtons(parent, x, y)` 两个页面共用,
     *        否则两处高亮会各持一份 `g_ui_gear` 的副本,迟早不同步。
     *    ⛔ 现在先不补的理由:**板上没有显示器,我改完看不到版面** ——
     *        每多改一处未经验证的坐标,就多一个只能靠猜的赌注。
     *        等能上电、能看见屏,再加这一行不迟。 */

    UI_CreateButton(control_panel,
                    "开始测量",
                    UI_COLOR_GREEN,
                    20,
                    322,
                    260,
                    72,
                    UI_SimpleActionEvent,
                    (void *)&g_single_measure_action);

    /* ---- 结果视图 A:单回路 ---- */
    g_single_result_panel = UI_CreatePanel(screen,
                                           result_x,
                                           156,
                                           result_width,
                                           420,
                                           UI_COLOR_SURFACE_LIGHT,
                                           16);
    lv_obj_set_style_border_color(g_single_result_panel,
                                  lv_color_hex(UI_COLOR_CYAN),
                                  LV_PART_MAIN);
    UI_CreateText(g_single_result_panel, "实时测量结果", &ui_font_cn_28,
                  UI_COLOR_TEXT, 26, 22, result_width - 52, 38,
                  LV_TEXT_ALIGN_CENTER);
    g_single_result_label =
        UI_CreateText(g_single_result_panel,
                      "Cs  -- pF\nCp  -- pF\nD   --",
                      &ui_font_cn_28, UI_COLOR_TEXT,
                      34, 92, result_width - 68, 202,
                      LV_TEXT_ALIGN_LEFT);
    g_single_result_note_label =
        UI_CreateText(g_single_result_panel, "尚未读取测量结果", &ui_font_cn_20,
                      UI_COLOR_TEXT_MUTED, 34, 344,
                      result_width - 68, 30, LV_TEXT_ALIGN_CENTER);

    /* ---- 结果视图 B:「全部」三路 ---- */
    g_measure_all_result_panel = UI_CreatePanel(screen,
                                                result_x,
                                                156,
                                                result_width,
                                                420,
                                                UI_COLOR_SURFACE_LIGHT,
                                                16);
    lv_obj_set_style_border_color(g_measure_all_result_panel,
                                  lv_color_hex(UI_COLOR_CYAN),
                                  LV_PART_MAIN);
    for (index = 0u; index < 3u; ++index)
    {
        static const char *const empty_text[] =
        {
            "CISS\n\nCs  -- pF\nCp  -- pF\nD   --",
            "COSS\n\nCs  -- pF\nCp  -- pF\nD   --",
            "CRSS\n\nCs  -- pF\nCp  -- pF\nD   --"
        };
        static const uint32_t colors[] =
            {UI_COLOR_BLUE, UI_COLOR_GREEN, UI_COLOR_ORANGE};
        lv_coord_t card_width = (result_width - 24 - 24) / 3;
        lv_obj_t *card = UI_CreatePanel(
            g_measure_all_result_panel,
            12 + (lv_coord_t)index * (card_width + 12),
            12,
            card_width,
            396,
            UI_COLOR_SURFACE,
            14);

        lv_obj_set_style_border_color(card, lv_color_hex(colors[index]),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
        g_all_result_labels[index] =
            UI_CreateText(card, empty_text[index], &ui_font_cn_28,
                          UI_COLOR_TEXT, 10, 30, card_width - 20, 300,
                          LV_TEXT_ALIGN_CENTER);
    }

    UI_UpdateMeasureResultMode();
    g_page_built[UI_PAGE_MEASURE] = 1u;
    UI_ActivatePage(UI_PAGE_MEASURE);
}

static lv_obj_t *UI_CreateSweepSubpageContainer(lv_obj_t *parent,
                                                 lv_coord_t x)
{
    lv_obj_t *container = lv_obj_create(parent);

    lv_obj_set_pos(container, x, UI_COMMON_HEADER_HEIGHT);
    lv_obj_set_size(container,
                    (lv_coord_t)LCD_PIXEL_WIDTH,
                    (lv_coord_t)LCD_PIXEL_HEIGHT -
                    UI_COMMON_HEADER_HEIGHT);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(container,
                              lv_color_hex(UI_COLOR_BACKGROUND),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    return container;
}

static void UI_SweepAnalysisStepEvent(lv_event_t *event)
{
    uint32_t increment;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }
    increment = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    if (increment != 0u)
    {
        if (g_ui_analysis_voltage_tenths <
            g_ui_analysis_voltage_max_tenths)
        {
            ++g_ui_analysis_voltage_tenths;
        }
    }
    else if (g_ui_analysis_voltage_tenths > UI_BIAS_MIN_TENTHS)
    {
        --g_ui_analysis_voltage_tenths;
    }
    UI_UpdateSweepAnalysisCard();
}

static void UI_SweepAnalysisSliderEvent(lv_event_t *event)
{
    lv_event_code_t code;
    lv_indev_t *input_device;
    lv_point_t point;
    lv_coord_t width;
    int32_t relative_x;
    uint32_t span;

    if ((event == NULL) || (g_results_analysis_track == NULL))
    {
        return;
    }
    code = lv_event_get_code(event);
    if ((code != LV_EVENT_PRESSED) && (code != LV_EVENT_PRESSING))
    {
        return;
    }
    input_device = lv_indev_get_act();
    if (input_device == NULL)
    {
        return;
    }
    lv_indev_get_point(input_device, &point);
    width = lv_obj_get_width(g_results_analysis_track);
    if (width <= 1)
    {
        return;
    }
    relative_x = (int32_t)point.x -
                 (int32_t)g_results_analysis_track->coords.x1;
    if (relative_x < 0)
    {
        relative_x = 0;
    }
    else if (relative_x > (width - 1))
    {
        relative_x = width - 1;
    }
    span = (g_ui_analysis_voltage_max_tenths > UI_BIAS_MIN_TENTHS) ?
           (g_ui_analysis_voltage_max_tenths - UI_BIAS_MIN_TENTHS) : 0u;
    g_ui_analysis_voltage_tenths = UI_BIAS_MIN_TENTHS;
    if (span != 0u)
    {
        g_ui_analysis_voltage_tenths +=
            (uint32_t)(((uint64_t)span * (uint32_t)relative_x +
                        (uint32_t)(width - 1) / 2u) /
                       (uint32_t)(width - 1));
    }
    UI_UpdateSweepAnalysisCard();
}

static void UI_UpdateTransferQRCode(void)
{
    const char *qr_data;
    lv_res_t result;

    /* QR 编码很贵(每次都要重排整张码),只在载荷真的变了时重画。 */
    if (g_transfer_qr_dirty == 0u)
    {
        return;
    }
    g_transfer_qr_dirty = 0u;

    qr_data = (g_transfer_has_payload != 0u) ?
              g_transfer_url : g_transfer_demo_url;
    if (g_results_transfer_status_label != NULL)
    {
        lv_label_set_text(g_results_transfer_status_label,
                          (g_transfer_has_payload != 0u) ?
                          "数据就绪,可扫码传输" : "等待三回路扫压");
        lv_obj_set_style_text_color(
            g_results_transfer_status_label,
            lv_color_hex((g_transfer_has_payload != 0u) ?
                         UI_COLOR_GREEN : UI_COLOR_ORANGE),
            LV_PART_MAIN);
    }
    if (g_results_transfer_payload_label != NULL)
    {
        if (g_transfer_has_payload != 0u)
        {
            lv_label_set_text_fmt(
                g_results_transfer_payload_label,
                "ACV1 %lu 节点 · SCV1 %lu 点 · URL %lu B",
                (unsigned long)g_transfer_acv1_nodes,
                (unsigned long)g_transfer_scv1_points,
                (unsigned long)strlen(g_transfer_url));
        }
        else
        {
            lv_label_set_text(g_results_transfer_payload_label,
                              "ACV1 -- · SCV1 -- · URL --");
        }
    }
    if (g_results_transfer_model_label != NULL)
    {
        lv_label_set_text(g_results_transfer_model_label,
                          (g_transfer_has_payload != 0u) ?
                          "LIVE / MEASURED / PHYSICAL" :
                          "DEMO / NO DATA");
    }
    if (g_results_transfer_qrcode == NULL)
    {
        return;
    }

    result = lv_qrcode_update(g_results_transfer_qrcode, qr_data,
                              (uint32_t)strlen(qr_data));
    if (result == LV_RES_OK)
    {
        lv_obj_clear_flag(g_results_transfer_qrcode, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(g_results_transfer_qrcode, LV_OBJ_FLAG_HIDDEN);
        if (g_results_transfer_status_label != NULL)
        {
            lv_label_set_text(g_results_transfer_status_label,
                              "QR RENDER ERROR");
            lv_obj_set_style_text_color(g_results_transfer_status_label,
                                        lv_color_hex(UI_COLOR_RED),
                                        LV_PART_MAIN);
        }
    }
}

/* 结果页「扫压结果」的工程参数卡:3 列 × 2 行,每张 318×38。
   名称左、数值右对齐;强调色条只占 4 px,把高度留给文字。 */
static lv_obj_t *UI_CreateResultsParameterCard(lv_obj_t *parent,
                                               const char *name,
                                               lv_coord_t x,
                                               lv_coord_t y,
                                               uint32_t accent_color)
{
    lv_obj_t *card = UI_CreatePanel(parent, x, y, 318, 38,
                                    UI_COLOR_SURFACE_LIGHT, 8);
    lv_obj_t *accent = lv_obj_create(card);

    lv_obj_set_pos(accent, 0, 0);
    lv_obj_set_size(accent, 4, 38);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(accent, lv_color_hex(accent_color),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(accent, 0, LV_PART_MAIN);
    UI_CreateText(card, name, &lv_font_montserrat_16,
                  UI_COLOR_TEXT_MUTED, 14, 9, 86, 22,
                  LV_TEXT_ALIGN_LEFT);
    return UI_CreateText(card, "--", &lv_font_montserrat_24,
                         UI_COLOR_TEXT, 104, 5, 200, 28,
                         LV_TEXT_ALIGN_RIGHT);
}

/* ===========================================================================
 * 结果页(UI_PAGE_RESULTS) —— 顶排 5 个页签,每个页签一个子页
 *
 *   [单点结果] 最近一次点测的读数(CISS / COSS / CRSS / RG 四张卡片)
 *   [单点波形] 单点测量过程中采到的 电压 / 电流 波形(调试用)
 *   [扫压结果] C-V 曲线(与扫压页同源)+ 触摸读数 + 6 张工程参数卡
 *   [扫压波形] 扫压全过程的 电压 / 电流 包络线(调试用)
 *   [数据传输] 实测数据二维码(ACV1 + SCV1,照搬前序工程)
 *
 * 定位:**投影视图** —— 自己不产生数据,进入页面时把已有数据重画一遍。
 * 因此「在测量页 / 扫压页测完 → 退回首页 → 点进结果页」看到的
 * 一定是刚测出的那份;没测过的回路显示 "--",不编造数值。
 *
 * ⚠️ 波形两页的缓存是**结果页自己的一份**(源页面上并没有这两张图),
 *    由测量后端经 UI_Demo_UpdateWaveformPoint() 逐点写回。
 * =========================================================================== */

#define UI_RESULT_ROUTE_COUNT  4u    /* CISS / COSS / CRSS / RG */
#define UI_RESULT_TAB_COUNT    5u
#define UI_WAVE_KIND_COUNT     2u    /* 0 = 单点波形  1 = 扫压包络 */
#define UI_WAVE_MAX_SAMPLES    480u

/* 🔴 波形图的坐标只能是整数 —— LVGL 8.2 的刻度文字**不能自定义格式**
   (扫压页那套「0.10 / 1.0 / 10 / 100」是用自绘回调硬画出来的,这里不走那条路),
   所以每条轴都要挑一个缩放系数,把量程搬成整数,单位写进轴标题。

   ⛔ **系数不是随便挑的:`app/lv_conf.h` 里 `LV_USE_LARGE_COORD = 0`,
      即 `lv_coord_t` 是 int16,坐标值不能超过 ±32767。**
      统一乘 1000 会把扫压那两条轴直接撑爆:
        · 电压 100 V ×1000 = 107992 → 截成 **-23080**
        · 电流 277.1 mA ×1000 = 299221 → 截成 **-28459**
      屏上就出现一串"凭空多出来的负刻度",而曲线只剩几段竖线
      (2026-10-03 实际踩到,Y 轴刻度与上面两个数逐位吻合)。
      ⇒ 系数必须让每条轴的最大值留在 ±32000 以内。 */
#define UI_WAVE_SCALE_COUNT    2u
static const float g_wave_scale[UI_WAVE_KIND_COUNT][UI_WAVE_SCALE_COUNT] = {
    {1000.0f, 1000.0f},   /* 单点:电压 mV(≤1.1k)、电流 μA(≤0.3k) */
    { 100.0f,  100.0f}    /* 扫压:电压 10 mV(≤10k)、电流 10 μA(≤28k) */
};

typedef struct
{
    char     cs[24];    /* RG 回路用这一格存电阻文本 */
    char     cp[24];
    char     d[24];
    uint8_t  have;      /* 0 = 从未收到过该回路的数据 */
    uint8_t  valid;     /* 后端给的有效性标志 */
} UI_ResultSnapshot;

typedef struct
{
    float    voltage[UI_WAVE_MAX_SAMPLES];
    float    current[UI_WAVE_MAX_SAMPLES];
    uint32_t total;
    uint8_t  have;
    float    v_min;
    float    v_max;
    float    i_min;
    float    i_max;
} UI_WaveBuffer;

static UI_ResultSnapshot g_result_snapshot[UI_RESULT_ROUTE_COUNT];
static UI_WaveBuffer     g_result_wave[UI_WAVE_KIND_COUNT];

static lv_obj_t *g_result_cards[UI_RESULT_ROUTE_COUNT];
static lv_obj_t *g_result_value_labels[UI_RESULT_ROUTE_COUNT][3];
static lv_obj_t *g_result_state_labels[UI_RESULT_ROUTE_COUNT];

static lv_obj_t *g_result_tab_buttons[UI_RESULT_TAB_COUNT];
static lv_obj_t *g_result_subpages[UI_RESULT_TAB_COUNT];
static uint32_t  g_result_tab;

static lv_obj_t *g_results_chart;
static lv_chart_series_t *g_results_series[3];
static lv_obj_t *g_results_summary_label;

/* [波形种类][0 = 电压, 1 = 电流] —— 电压与电流量程差几个数量级,各占一张图 */
static lv_obj_t *g_result_wave_chart[UI_WAVE_KIND_COUNT][2];
static lv_chart_series_t *g_result_wave_series[UI_WAVE_KIND_COUNT][2];
static lv_obj_t *g_result_wave_status[UI_WAVE_KIND_COUNT];
/* 「还没收到」文案:建页时写一次,之后由 UI_ResultWaveRefresh 按数据改写 */
static const char *const g_wave_waiting_text[UI_WAVE_KIND_COUNT] =
{
    "尚未收到单点波形数据",
    "尚未收到扫压包络数据"
};

static uint8_t g_results_built;

static void UI_ResultTabApply(void);

/*
 * 记录一次读数。三条路径都从这里进,保证结果页与源页面永远同源。
 * route: 0=CISS 1=COSS 2=CRSS 3=RG
 */
static void UI_ResultRecord(uint32_t route,
                            const char *cs_text,
                            const char *cp_text,
                            const char *d_text,
                            uint32_t valid)
{
    UI_ResultSnapshot *snap;

    if (route >= UI_RESULT_ROUTE_COUNT)
    {
        return;
    }
    snap = &g_result_snapshot[route];
    if (cs_text != NULL)
    {
        (void)strncpy(snap->cs, cs_text, sizeof(snap->cs) - 1u);
        snap->cs[sizeof(snap->cs) - 1u] = '\0';
    }
    if (cp_text != NULL)
    {
        (void)strncpy(snap->cp, cp_text, sizeof(snap->cp) - 1u);
        snap->cp[sizeof(snap->cp) - 1u] = '\0';
    }
    if (d_text != NULL)
    {
        (void)strncpy(snap->d, d_text, sizeof(snap->d) - 1u);
        snap->d[sizeof(snap->d) - 1u] = '\0';
    }
    snap->have = 1u;
    snap->valid = (uint8_t)((valid != 0u) ? 1u : 0u);
}

static void UI_ResultSnapshotsClear(void)
{
    (void)memset(g_result_snapshot, 0, sizeof(g_result_snapshot));
    (void)memset(g_result_wave, 0, sizeof(g_result_wave));
}

/*
 * 子页容器:一层**透明整屏叠层**,子页之间只切 HIDDEN,不重建控件。
 * ⚠️ 不能用 UI_CreatePageContainer() —— 那个是给顶层页用的,会带自己的背景与内边距。
 */
static lv_obj_t *UI_CreateResultSubpage(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);

    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, (lv_coord_t)LCD_PIXEL_WIDTH,
                    (lv_coord_t)LCD_PIXEL_HEIGHT);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);

    return page;
}

/* --------------------------------------------------------------------------
 * 波形缓存(单点波形 / 扫压包络)
 * ------------------------------------------------------------------------ */

uint32_t UI_Demo_WaveMaxSamples(void)
{
    return UI_WAVE_MAX_SAMPLES;
}

void UI_Demo_BeginWaveform(uint32_t kind, uint32_t total_samples)
{
    UI_WaveBuffer *buf;

    if (kind >= UI_WAVE_KIND_COUNT)
    {
        return;
    }
    if (total_samples > UI_WAVE_MAX_SAMPLES)
    {
        total_samples = UI_WAVE_MAX_SAMPLES;
    }

    buf = &g_result_wave[kind];
    (void)memset(buf, 0, sizeof(*buf));
    buf->total = total_samples;
    buf->have = 0u;
    buf->v_min = buf->i_min = 0.0f;
    buf->v_max = buf->i_max = 0.0f;
}

void UI_Demo_UpdateWaveformPoint(uint32_t kind,
                                 uint32_t index,
                                 uint32_t total_samples,
                                 float voltage_v,
                                 float current_ma,
                                 const char *index_text)
{
    UI_WaveBuffer *buf;
    uint32_t slot;

    if ((kind >= UI_WAVE_KIND_COUNT) || (index == 0u) ||
        (index > UI_WAVE_MAX_SAMPLES))
    {
        return;
    }
    if (!isfinite(voltage_v) || !isfinite(current_ma))
    {
        return;
    }

    slot = index - 1u;
    buf = &g_result_wave[kind];

    if (total_samples > UI_WAVE_MAX_SAMPLES)
    {
        total_samples = UI_WAVE_MAX_SAMPLES;
    }
    if (total_samples > buf->total)
    {
        buf->total = total_samples;
    }

    buf->voltage[slot] = voltage_v;
    buf->current[slot] = current_ma;

    if ((buf->have == 0u) || (slot == 0u))
    {
        buf->v_min = buf->v_max = voltage_v;
        buf->i_min = buf->i_max = current_ma;
    }
    else
    {
        if (voltage_v < buf->v_min) { buf->v_min = voltage_v; }
        if (voltage_v > buf->v_max) { buf->v_max = voltage_v; }
        if (current_ma < buf->i_min) { buf->i_min = current_ma; }
        if (current_ma > buf->i_max) { buf->i_max = current_ma; }
    }
    buf->have = 1u;

    if (g_result_wave_status[kind] != NULL)
    {
        lv_label_set_text_fmt(g_result_wave_status[kind],
                              "已采集 %lu 点%s%s",
                              (unsigned long)index,
                              (index_text != NULL) ? "  " : "",
                              (index_text != NULL) ? index_text : "");
        lv_obj_set_style_text_color(g_result_wave_status[kind],
                                    lv_color_hex(UI_COLOR_GREEN),
                                    LV_PART_MAIN);
    }
}

/* --------------------------------------------------------------------------
 * 刷新
 * ------------------------------------------------------------------------ */

/* 把扫压页的曲线数据整段复制到结果页的图上(纯投影,不重算) */
static void UI_ResultsCopyChart(void)
{
    uint32_t index;

    if ((g_results_chart == NULL) || (g_sweep_chart == NULL))
    {
        return;
    }

    for (index = 0u; index < 3u; ++index)
    {
        lv_chart_series_t *source = g_sweep_series[index];
        lv_chart_series_t *target = g_results_series[index];
        lv_coord_t *source_x;
        lv_coord_t *source_y;
        lv_coord_t *target_x;
        lv_coord_t *target_y;

        if ((source == NULL) || (target == NULL))
        {
            continue;
        }

        source_x = lv_chart_get_x_array(g_sweep_chart, source);
        source_y = lv_chart_get_y_array(g_sweep_chart, source);
        target_x = lv_chart_get_x_array(g_results_chart, target);
        target_y = lv_chart_get_y_array(g_results_chart, target);
        if ((source_x == NULL) || (source_y == NULL) ||
            (target_x == NULL) || (target_y == NULL))
        {
            continue;
        }

        (void)memcpy(target_x, source_x,
                     sizeof(lv_coord_t) * UI_SWEEP_RENDER_POINTS);
        (void)memcpy(target_y, source_y,
                     sizeof(lv_coord_t) * UI_SWEEP_RENDER_POINTS);
    }

    /* 曲线是复制过来的,视口也得跟着扫压页走 —— 否则刻度标签还停在上次建页时的量程 */
    UI_ApplyChartViewport(g_results_chart);

    if (g_results_summary_label != NULL)
    {
        uint32_t path;
        uint32_t best = 0u;
        uint32_t drawn = 0u;

        for (path = 0u; path < 3u; ++path)
        {
            if (g_sweep_total_points[path] > best)
            {
                best = g_sweep_total_points[path];
            }
            if (g_sweep_valid[path][0] != 0u)
            {
                drawn++;
            }
        }
        lv_label_set_text_fmt(g_results_summary_label,
                              "每路点数 %lu · 有效曲线 %lu/3 · RG %s",
                              (unsigned long)best,
                              (unsigned long)drawn,
                              (g_latest_rg_valid != 0u) ? "已测" : "--");
    }
}

/* 波形两页:把缓存里的浮点数组搬进 lv_chart 的整型数组(×1000 保精度) */
static void UI_ResultWaveRefresh(void)
{
    uint32_t kind;
    uint32_t axis;

    for (kind = 0u; kind < UI_WAVE_KIND_COUNT; ++kind)
    {
        const UI_WaveBuffer *buf = &g_result_wave[kind];

        for (axis = 0u; axis < 2u; ++axis)
        {
            lv_obj_t *chart = g_result_wave_chart[kind][axis];
            lv_chart_series_t *series = g_result_wave_series[kind][axis];
            lv_coord_t *xs;
            lv_coord_t *ys;
            float lo;
            float hi;
            uint32_t index;

            if ((chart == NULL) || (series == NULL))
            {
                continue;
            }

            xs = lv_chart_get_x_array(chart, series);
            ys = lv_chart_get_y_array(chart, series);
            if ((xs == NULL) || (ys == NULL))
            {
                continue;
            }

            for (index = 0u; index < UI_WAVE_MAX_SAMPLES; ++index)
            {
                if ((buf->have != 0u) && (index < buf->total))
                {
                    xs[index] = (lv_coord_t)index;
                    ys[index] = (lv_coord_t)(((axis == 0u) ?
                                              buf->voltage[index] :
                                              buf->current[index]) *
                                             g_wave_scale[kind][axis]);
                }
                else
                {
                    xs[index] = LV_CHART_POINT_NONE;
                    ys[index] = LV_CHART_POINT_NONE;
                }
            }

            if ((axis == 0u) && (g_result_wave_status[kind] != NULL))
            {
                /* 🔴 状态行跟数据走。只在建页时写「尚未收到」的话,
                   数据到了它也不会改口 —— 屏幕上就出现「画着曲线却说没数据」。 */
                if (buf->have == 0u)
                {
                    lv_label_set_text(g_result_wave_status[kind],
                                      g_wave_waiting_text[kind]);
                    lv_obj_set_style_text_color(g_result_wave_status[kind],
                                                lv_color_hex(UI_COLOR_TEXT_MUTED),
                                                LV_PART_MAIN);
                }
                else
                {
                    lv_label_set_text_fmt(g_result_wave_status[kind],
                                          "已采集 %lu 点",
                                          (unsigned long)buf->total);
                    lv_obj_set_style_text_color(g_result_wave_status[kind],
                                                lv_color_hex(UI_COLOR_GREEN),
                                                LV_PART_MAIN);
                }
            }

            lo = (axis == 0u) ? buf->v_min : buf->i_min;
            hi = (axis == 0u) ? buf->v_max : buf->i_max;
            if (buf->have == 0u)
            {
                lo = 0.0f;
                hi = 1.0f;
            }
            else if (hi - lo < 1e-6f)
            {
                lo -= 0.5f;
                hi += 0.5f;
            }

            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0,
                               (buf->total > 1u) ?
                               (lv_coord_t)(buf->total - 1u) : 1);
            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
                               (lv_coord_t)((lo - ((hi - lo) * 0.08f)) *
                                            g_wave_scale[kind][axis]),
                               (lv_coord_t)((hi + ((hi - lo) * 0.08f)) *
                                            g_wave_scale[kind][axis]));
            /* Y 轴以「实际单位」显示,所以刻度值要除以 1000 */
            lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y,
                                   5, 2, 5, 1, true, 62);
            lv_chart_refresh(chart);
        }
    }
}

static void UI_ResultsRefresh(void)
{
    static const char *const value_keys[] = {"Cs", "Cp", "D "};
    uint32_t route;
    uint32_t line;

    if (g_results_built == 0u)
    {
        return;
    }

    for (route = 0u; route < UI_RESULT_ROUTE_COUNT; ++route)
    {
        const UI_ResultSnapshot *snap = &g_result_snapshot[route];
        uint32_t card_color;

        if (snap->have == 0u)
        {
            card_color = UI_COLOR_BORDER;
        }
        else
        {
            card_color = (snap->valid != 0u) ?
                         UI_COLOR_GREEN : UI_COLOR_RED;
        }

        if (g_result_cards[route] != NULL)
        {
            lv_obj_set_style_border_color(g_result_cards[route],
                                          lv_color_hex(card_color),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(g_result_cards[route],
                                          (snap->have != 0u) ? 2 : 1,
                                          LV_PART_MAIN);
        }

        if (g_result_state_labels[route] != NULL)
        {
            lv_label_set_text(g_result_state_labels[route],
                              (snap->have == 0u) ? "--" :
                              ((snap->valid != 0u) ? "OK" : "NG"));
            lv_obj_set_style_text_color(g_result_state_labels[route],
                                        lv_color_hex(card_color),
                                        LV_PART_MAIN);
        }

        for (line = 0u; line < 3u; ++line)
        {
            lv_obj_t *label = g_result_value_labels[route][line];

            if (label == NULL)
            {
                continue;
            }

            if (route == 3u)
            {
                /* RG 只有一个电阻读数,占第三行(其余两行留空) */
                if (line == 2u)
                {
                    lv_label_set_text_fmt(
                        label, "RG  %s Ω",
                        (snap->have != 0u) ? snap->cs : "--");
                }
                else
                {
                    lv_label_set_text(label, "");
                }
            }
            else
            {
                const char *value;

                if (snap->have == 0u)
                {
                    value = "--";
                }
                else if (line == 0u)
                {
                    value = snap->cs;
                }
                else if (line == 1u)
                {
                    value = snap->cp;
                }
                else
                {
                    value = snap->d;
                }
                lv_label_set_text_fmt(label, "%s  %s%s",
                                      value_keys[line], value,
                                      (line < 2u) ? " pF" : "");
            }
        }
    }

    UI_ResultsCopyChart();
    UI_UpdateSweepAnalysisRange();
    UI_UpdateSweepAnalysisCard();
    UI_UpdateTransferQRCode();
    UI_ResultWaveRefresh();
    UI_ResultTabApply();
}

/* 协议层用的导出包装。
 * 🔴 为什么必须有:上面那个是 `static`,而数据(`g_result_wave[]` 与扫压曲线)
 *    喂进来之后**没有任何东西会重画** —— 只有"进结果页 / 切页签"这两个
 *    事件会调 `UI_ResultsRefresh()`。⇒ 屏上表现为"数据是新的、画的是旧的"。
 * ⚠️ 只在结果页**可见**时才真的重画。不可见时 `UI_ResultsRefresh()` 里那几
 *    条 `lv_chart_refresh` 只是把对象标脏,LVGL 不会为不可见对象做无效区计算,
 *    代价很小;但每轮采集都调一次的频率远高于界面变化频率,所以**节流放在
 *    调用方**(见 `pynq_protocol.c::adc_service`),这里不做内部节流 ——
 *    内部节流会让"改完数据立刻想看"变成"等 80 ms 再说",反而更难用。 */
void UI_Demo_RefreshResultViews(void)
{
    UI_ResultsRefresh();
}

/* --------------------------------------------------------------------------
 * 页签
 * ------------------------------------------------------------------------ */

static void UI_ResultTabApply(void)
{
    uint32_t index;

    for (index = 0u; index < UI_RESULT_TAB_COUNT; ++index)
    {
        lv_obj_t *page = g_result_subpages[index];
        lv_obj_t *button = g_result_tab_buttons[index];

        if (page != NULL)
        {
            if (index == g_result_tab)
            {
                lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (button != NULL)
        {
            if (index == g_result_tab)
            {
                lv_obj_set_style_bg_color(button,
                                          lv_color_hex(UI_COLOR_CYAN),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(button,
                                              lv_color_hex(UI_COLOR_CYAN),
                                              LV_PART_MAIN);
                lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_bg_color(button,
                                          lv_color_hex(UI_COLOR_SURFACE),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(button,
                                              lv_color_hex(UI_COLOR_BORDER),
                                              LV_PART_MAIN);
                lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
            }
        }
    }
}

static void UI_ResultTabEvent(lv_event_t *event)
{
    uint32_t tab;

    if ((event == NULL) ||
        (lv_event_get_code(event) != LV_EVENT_CLICKED))
    {
        return;
    }

    tab = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    if (tab >= UI_RESULT_TAB_COUNT)
    {
        return;
    }

    g_result_tab = tab;
    UI_ResultTabApply();
    UI_ResultsRefresh();
}

/* 造一张调试波形图:电压/电流各一张,共用同一段样本序号当 X 轴 */
static lv_obj_t *UI_CreateWaveChart(lv_obj_t *parent,
                                    uint32_t kind,
                                    uint32_t axis,
                                    const char *title,
                                    const char *unit,
                                    lv_coord_t y)
{
    static const uint32_t colors[2] = {UI_COLOR_CYAN, UI_COLOR_ORANGE};
    lv_obj_t *panel = UI_CreatePanel(parent, 0, y, 976, 212,
                                     UI_COLOR_SURFACE_LIGHT, 14);
    lv_obj_t *chart;

    lv_obj_set_style_border_color(panel, lv_color_hex(UI_COLOR_BORDER),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    /* 标题压在面板顶行 —— 图表顶端的那个 Y 刻度文字正好落在同一高度,
       两者会叠在一起(已踩),所以标题只占 2..22,图表从 34 起。 */
    UI_CreateText(panel, title, &ui_font_cn_20, UI_COLOR_TEXT,
                  16, 2, 200, 20, LV_TEXT_ALIGN_LEFT);
    /* ⚠️ 单位串用**中文字库**画,不能用 montserrat ——
       它没有 μ/Ω,缺字会静默渲染成空白(屏幕上就成了"/ 10 A")。 */
    UI_CreateText(panel, unit, &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                  240, 2, 200, 20, LV_TEXT_ALIGN_LEFT);

    chart = lv_chart_create(panel);
    /* X 起点 88 = Y 刻度文字宽度(80)+ 间隙。刻度可以是 6 位数(100000 mV),
       留窄了左边几位会被裁掉(已踩)。 */
    lv_obj_set_pos(chart, 88, 34);
    /* 高度 146 = 面板 212 − 上留 34(标题 + 顶刻度) − 下留 32(X 刻度文字) */
    lv_obj_set_size(chart, 848, 146);
    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart, UI_WAVE_MAX_SAMPLES);
    lv_chart_set_div_line_count(chart, 0u, 0u);
    /* X 轴刻度只在下面那张图上标:两张图共用同一段样本序号,标一次就够 */
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X,
                           5, 2, 6, 1, (axis != 0u), 34);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y,
                           5, 2, 5, 1, true, 80);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x050505u), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart, lv_color_hex(0xffffffu),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_hex(0xffffffu),
                                LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(chart, &lv_font_montserrat_16, LV_PART_TICKS);
    lv_obj_set_style_text_color(chart, lv_color_hex(0xffffffu),
                                LV_PART_TICKS);
    lv_obj_set_style_pad_left(chart, 5, LV_PART_TICKS);
    lv_obj_set_style_pad_right(chart, 14, LV_PART_TICKS);   /* 末位 X 刻度文字不再出界 */
    lv_obj_set_style_pad_bottom(chart, 4, LV_PART_TICKS);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE);

    g_result_wave_chart[kind][axis] = chart;
    g_result_wave_series[kind][axis] = lv_chart_add_series(
        chart, lv_color_hex(colors[axis]), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, g_result_wave_series[kind][axis],
                           LV_CHART_POINT_NONE);

    return panel;
}

static void UI_ShowResults(void)
{
    static const char *const tab_names[UI_RESULT_TAB_COUNT] =
    {
        "单点结果", "单点波形", "扫压结果", "扫压波形", "数据传输"
    };
    static const char *const route_names[] =
        {"CISS", "COSS", "CRSS", "RG"};
    lv_obj_t *screen = g_page_containers[UI_PAGE_RESULTS];
    lv_obj_t *tab_panel;
    lv_obj_t *point_value_page;
    lv_obj_t *sweep_value_page;
    lv_obj_t *export_page;
    lv_coord_t content_width =
        (lv_coord_t)LCD_PIXEL_WIDTH - (2 * UI_PAGE_MARGIN);
    lv_coord_t card_gap = 14;
    lv_coord_t card_width =
        (content_width - (3 * card_gap)) / (lv_coord_t)UI_RESULT_ROUTE_COUNT;
    uint32_t index;
    uint32_t line;

    if (screen == NULL)
    {
        return;
    }

    if (g_results_built != 0u)
    {
        UI_ActivatePage(UI_PAGE_RESULTS);
        return;
    }

    /* ---- 顶部页签 ---- */
    tab_panel = UI_CreatePanel(screen, UI_PAGE_MARGIN, 64, content_width, 50,
                               UI_COLOR_SURFACE_LIGHT, 12);
    for (index = 0u; index < UI_RESULT_TAB_COUNT; ++index)
    {
        lv_coord_t tab_width = (content_width - 24 - 4 * 8) / 5;
        lv_obj_t *button = UI_CreateButton(
            tab_panel, tab_names[index], UI_COLOR_SURFACE,
            12 + (lv_coord_t)index * (tab_width + 8), 8,
            tab_width, 34, UI_ResultTabEvent,
            (void *)(uintptr_t)index);

        g_result_tab_buttons[index] = button;
    }

    /* ================= 子页 0:单点结果 ================= */
    point_value_page = UI_CreateResultSubpage(screen);
    g_result_subpages[0] = point_value_page;

    for (index = 0u; index < UI_RESULT_ROUTE_COUNT; ++index)
    {
        lv_obj_t *card = UI_CreatePanel(
            point_value_page,
            UI_PAGE_MARGIN + (lv_coord_t)index * (card_width + card_gap),
            124, card_width, 168, UI_COLOR_SURFACE_LIGHT, 14);

        g_result_cards[index] = card;
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);

        UI_CreateText(card, route_names[index], &ui_font_cn_28,
                      UI_COLOR_TEXT, 14, 10, card_width - 80, 34,
                      LV_TEXT_ALIGN_CENTER);
        g_result_state_labels[index] =
            UI_CreateText(card, "--", &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                          card_width - 62, 14, 48, 26, LV_TEXT_ALIGN_RIGHT);

        for (line = 0u; line < 3u; ++line)
        {
            g_result_value_labels[index][line] =
                UI_CreateText(card, "--", &ui_font_cn_20, UI_COLOR_TEXT,
                              14, 56 + (lv_coord_t)line * 32,
                              card_width - 28, 28, LV_TEXT_ALIGN_LEFT);
        }
    }
    UI_CreateText(point_value_page,
                  "数据来源:测量页最近一次写回的读数;未测过的回路显示 --",
                  &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                  UI_PAGE_MARGIN, 306, content_width, 28,
                  LV_TEXT_ALIGN_LEFT);

    /* ================= 子页 1:单点波形 ================= */
    g_result_subpages[1] = UI_CreateResultSubpage(screen);
    UI_CreateWaveChart(g_result_subpages[1], 0u, 0u, "激励与偏置电压",
                       "/ mV", 118);
    UI_CreateWaveChart(g_result_subpages[1], 0u, 1u, "回路电流",
                       "/ μA", 336);
    g_result_wave_status[0] =
        UI_CreateText(g_result_subpages[1], g_wave_waiting_text[0],
                      &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                      8, 552, 960, 24, LV_TEXT_ALIGN_LEFT);

    /* ================= 子页 2:扫压结果(曲线 + 触摸读数 + 参数卡) ================= */
    sweep_value_page = UI_CreateResultSubpage(screen);
    g_result_subpages[2] = sweep_value_page;
    {
        static const char *const legend_names[] =
            {"CISS", "COSS", "CRSS"};
        lv_obj_t *chart_panel = UI_CreatePanel(sweep_value_page,
                                               UI_PAGE_MARGIN, 124,
                                               content_width, 286,
                                               UI_COLOR_SURFACE_LIGHT, 14);

        UI_CreateText(chart_panel, "C-V 曲线(与扫压页同源)", &ui_font_cn_20,
                      UI_COLOR_TEXT_MUTED, 16, 4, 250, 24,
                      LV_TEXT_ALIGN_LEFT);

        for (index = 0u; index < 3u; ++index)
        {
            lv_coord_t item_x = 280 + (lv_coord_t)index * 110;
            lv_obj_t *line = lv_obj_create(chart_panel);

            lv_obj_set_pos(line, item_x, 14);
            lv_obj_set_size(line, 26, 4);
            lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(
                line, lv_color_hex(g_sweep_series_colors[index]), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(line, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);
            UI_CreateText(chart_panel, legend_names[index],
                          &lv_font_montserrat_16, UI_COLOR_TEXT_MUTED,
                          item_x + 32, 4, 70, 24, LV_TEXT_ALIGN_LEFT);
        }
        /* 摘要移到面板右上角,把屏下空间让给读数行与参数卡。 */
        g_results_summary_label =
            UI_CreateText(chart_panel, "尚无扫压数据", &ui_font_cn_20,
                          UI_COLOR_TEXT_MUTED, 610, 4, 350, 24,
                          LV_TEXT_ALIGN_RIGHT);

        g_results_chart = lv_chart_create(chart_panel);
        lv_obj_set_pos(g_results_chart, 64, 32);
        /* 高度留出底部给 X 轴刻度文字 —— 少留就会被面板裁掉(已踩) */
        lv_obj_set_size(g_results_chart, content_width - 80, 226);
        lv_chart_set_type(g_results_chart, LV_CHART_TYPE_SCATTER);
        lv_chart_set_point_count(g_results_chart, UI_SWEEP_RENDER_POINTS);
        lv_chart_set_range(g_results_chart, LV_CHART_AXIS_PRIMARY_X,
                           g_sweep_x_min_log_milli,
                           g_sweep_x_max_log_milli);
        lv_chart_set_range(g_results_chart, LV_CHART_AXIS_PRIMARY_Y,
                           g_sweep_y_min_log_milli,
                           g_sweep_y_max_log_milli);
        lv_chart_set_div_line_count(g_results_chart, 0u, 0u);
        lv_chart_set_axis_tick(g_results_chart, LV_CHART_AXIS_PRIMARY_X,
                               6, 3, 4, 1, true, 38);
        lv_chart_set_axis_tick(g_results_chart, LV_CHART_AXIS_PRIMARY_Y,
                               6, 3, 5, 1, true, 58);
        lv_obj_set_style_bg_color(g_results_chart,
                                  lv_color_hex(0x050505u), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_results_chart, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_results_chart,
                                      lv_color_hex(0xffffffu), LV_PART_MAIN);
        lv_obj_set_style_border_width(g_results_chart, 1, LV_PART_MAIN);
        lv_obj_set_style_line_color(g_results_chart,
                                    lv_color_hex(0xffffffu), LV_PART_MAIN);
        lv_obj_set_style_line_width(g_results_chart, 2, LV_PART_ITEMS);
        lv_obj_set_style_size(g_results_chart, 0, LV_PART_INDICATOR);
        lv_obj_set_style_text_font(g_results_chart, &lv_font_montserrat_16,
                                   LV_PART_TICKS);
        lv_obj_set_style_text_color(g_results_chart, lv_color_hex(0xffffffu),
                                    LV_PART_TICKS);
        lv_obj_set_style_pad_left(g_results_chart, 5, LV_PART_TICKS);
        lv_obj_set_style_pad_bottom(g_results_chart, 4, LV_PART_TICKS);
        lv_obj_clear_flag(g_results_chart, LV_OBJ_FLAG_SCROLLABLE);
        /* 结果页只做「按住 / 滑动读数」,不做捏合缩放。 */
        lv_obj_add_flag(g_results_chart,
                        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
        /* 🔴 必须挂上和扫压页**同一个**绘制回调:
           对数轴的刻度文字(0.10 / 1.0 / 10 / 100)是自绘的,
           LVGL 自带的整数刻度只会把 log-milli 原值(1000/2000/3000)印出来。 */
        lv_obj_add_event_cb(g_results_chart, UI_SweepChartDrawEvent,
                            LV_EVENT_DRAW_PART_BEGIN, NULL);
        lv_obj_add_event_cb(g_results_chart, UI_SweepChartProbeDrawEvent,
                            LV_EVENT_DRAW_POST, NULL);
        lv_obj_add_event_cb(g_results_chart, UI_ResultsSweepTouchEvent,
                            LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_results_chart, UI_ResultsSweepTouchEvent,
                            LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(g_results_chart, UI_ResultsSweepTouchEvent,
                            LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_results_chart, UI_ResultsSweepTouchEvent,
                            LV_EVENT_PRESS_LOST, NULL);

        for (index = 0u; index < 3u; ++index)
        {
            g_results_series[index] = lv_chart_add_series(
                g_results_chart,
                lv_color_hex(g_sweep_series_colors[index]),
                LV_CHART_AXIS_PRIMARY_Y);
            lv_chart_set_all_value(g_results_chart, g_results_series[index],
                                   LV_CHART_POINT_NONE);
        }
    }

    g_results_probe_label =
        UI_CreateText(sweep_value_page, UI_RESULT_PROBE_HINT,
                      &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                      UI_PAGE_MARGIN, 412, content_width, 28,
                      LV_TEXT_ALIGN_LEFT);

    /* 分析电压行:步进 0.1 V / 直接输入 / 滑条 */
    UI_CreateButton(sweep_value_page, "-0.1", UI_COLOR_SURFACE_LIGHT,
                    UI_PAGE_MARGIN, 444, 78, 44,
                    UI_SweepAnalysisStepEvent,
                    (void *)(uintptr_t)0u);
    {
        lv_obj_t *analysis_button = UI_CreateButton(
            sweep_value_page, "0.1 V", UI_COLOR_BLUE,
            UI_PAGE_MARGIN + 86, 444, 200, 44,
            UI_BiasOpenEvent,
            (void *)(uintptr_t)UI_BIAS_TARGET_ANALYSIS);

        g_results_analysis_value_label = lv_obj_get_child(analysis_button, 0);
        lv_obj_set_style_text_font(g_results_analysis_value_label,
                                   &lv_font_montserrat_24, LV_PART_MAIN);
    }
    UI_CreateButton(sweep_value_page, "+0.1", UI_COLOR_SURFACE_LIGHT,
                    UI_PAGE_MARGIN + 294, 444, 78, 44,
                    UI_SweepAnalysisStepEvent,
                    (void *)(uintptr_t)1u);
    g_results_analysis_track = UI_CreatePanel(
        sweep_value_page, 420, 457, 400, 18,
        UI_COLOR_BORDER, 9);
    lv_obj_add_flag(g_results_analysis_track, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(g_results_analysis_track,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(g_results_analysis_track,
                        UI_SweepAnalysisSliderEvent,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_results_analysis_track,
                        UI_SweepAnalysisSliderEvent,
                        LV_EVENT_PRESSING, NULL);
    g_results_analysis_knob = lv_obj_create(g_results_analysis_track);
    lv_obj_set_pos(g_results_analysis_knob, 0, -5);
    lv_obj_set_size(g_results_analysis_knob, 28, 28);
    lv_obj_clear_flag(g_results_analysis_knob,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(g_results_analysis_knob,
                              lv_color_hex(UI_COLOR_CYAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_results_analysis_knob,
                            LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_results_analysis_knob,
                                  lv_color_hex(0xffffffu), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_results_analysis_knob, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(g_results_analysis_knob,
                            LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_results_analysis_knob, 0, LV_PART_MAIN);
    UI_CreateText(sweep_value_page, "分析电压", &ui_font_cn_20,
                  UI_COLOR_TEXT_MUTED, 836, 451, 160, 28,
                  LV_TEXT_ALIGN_LEFT);

    /* 6 张工程参数卡:3 列 × 2 行 */
    {
        static const char *const names[UI_SWEEP_PARAMETER_COUNT] =
            {"CISS", "COSS", "CRSS", "Rg", "Qoss", "Eoss"};
        static const uint32_t colors[UI_SWEEP_PARAMETER_COUNT] =
            {UI_COLOR_BLUE, UI_COLOR_GREEN, UI_COLOR_ORANGE,
             UI_COLOR_PURPLE, UI_COLOR_CYAN, UI_COLOR_RED};

        for (index = 0u; index < UI_SWEEP_PARAMETER_COUNT; ++index)
        {
            lv_coord_t column = (lv_coord_t)(index % 3u);
            lv_coord_t row = (lv_coord_t)(index / 3u);

            g_results_parameter_value_labels[index] =
                UI_CreateResultsParameterCard(
                    sweep_value_page, names[index],
                    UI_PAGE_MARGIN + column * (318 + 11),
                    496 + row * 44,
                    colors[index]);
        }
    }

    /* ================= 子页 3:扫压波形(电压/电流包络) ================= */
    g_result_subpages[3] = UI_CreateResultSubpage(screen);
    UI_CreateWaveChart(g_result_subpages[3], 1u, 0u, "扫压偏置包络",
                       "/ 10 mV", 118);
    UI_CreateWaveChart(g_result_subpages[3], 1u, 1u, "回路电流包络",
                       "/ 10 μA", 336);
    g_result_wave_status[1] =
        UI_CreateText(g_result_subpages[3], g_wave_waiting_text[1],
                      &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                      8, 552, 960, 24, LV_TEXT_ALIGN_LEFT);

    /* ================= 子页 4:数据传输(实测数据二维码) ================= */
    export_page = UI_CreateResultSubpage(screen);
    g_result_subpages[4] = export_page;
    {
        lv_obj_t *panel = UI_CreatePanel(export_page,
                                         UI_PAGE_MARGIN, 124,
                                         content_width, 452,
                                         UI_COLOR_SURFACE_LIGHT, 16);
        lv_obj_t *qr_background;

        lv_obj_set_style_border_color(panel, lv_color_hex(UI_COLOR_BORDER),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);

        qr_background = UI_CreatePanel(panel, 36, 64, 328, 328,
                                       0xffffffu, 8);
        lv_obj_set_style_border_color(qr_background,
                                      lv_color_hex(0xffffffu), LV_PART_MAIN);
        g_results_transfer_qrcode = lv_qrcode_create(
            qr_background, UI_TRANSFER_QR_SIZE,
            lv_color_hex(0x000000u), lv_color_hex(0xffffffu));
        lv_obj_set_pos(g_results_transfer_qrcode, 20, 20);
        UI_CreateText(panel, "扫码后留在本机", &ui_font_cn_20,
                      UI_COLOR_TEXT_MUTED, 36, 404, 328, 26,
                      LV_TEXT_ALIGN_CENTER);

        UI_CreateText(panel, "实测数据二维码", &ui_font_cn_28,
                      UI_COLOR_TEXT, 390, 24, 546, 40,
                      LV_TEXT_ALIGN_LEFT);
        UI_CreateText(panel,
                      "手机扫码打开 AI-CV2SPICE,生成 LTspice 模型与完整曲线",
                      &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                      390, 70, 546, 30, LV_TEXT_ALIGN_LEFT);
        UI_CreateText(panel, "https://fox4327.github.io/ai-cv2spice-web/",
                      &lv_font_montserrat_16, UI_COLOR_CYAN,
                      390, 104, 546, 24, LV_TEXT_ALIGN_LEFT);
        g_results_transfer_status_label =
            UI_CreateText(panel, "等待三回路扫压", &ui_font_cn_20,
                          UI_COLOR_ORANGE, 390, 148, 546, 30,
                          LV_TEXT_ALIGN_LEFT);
        g_results_transfer_payload_label =
            UI_CreateText(panel, "ACV1 -- · SCV1 -- · URL --",
                          &ui_font_cn_20, UI_COLOR_TEXT,
                          390, 186, 546, 28, LV_TEXT_ALIGN_LEFT);
        g_results_transfer_model_label =
            UI_CreateText(panel, "DEMO / NO DATA", &lv_font_montserrat_16,
                          UI_COLOR_TEXT_MUTED, 390, 222, 546, 24,
                          LV_TEXT_ALIGN_LEFT);
        UI_CreateText(panel,
                      "数据全部放在 URL 片段中,不经任何服务器;\n"
                      "扫码方需要能访问上面的静态解码页。",
                      &ui_font_cn_20, UI_COLOR_TEXT_MUTED,
                      390, 270, 546, 70, LV_TEXT_ALIGN_LEFT);
    }
    g_transfer_qr_dirty = 1u;

    g_result_tab = 0u;
    {   /* ⚠️ 仅供**演示 / 自动化截图**:CV_UI_RESULT_TAB 指定进页时停在哪个页签,
           与 UI_Demo_GotoPage() 同一性质。正常业务流程里页签由触摸事件切换。 */
        const char *env = getenv("CV_UI_RESULT_TAB");
        if (env != NULL)
        {
            unsigned long v = strtoul(env, NULL, 0);
            if (v < UI_RESULT_TAB_COUNT) { g_result_tab = (uint32_t)v; }
        }
    }
    g_results_built = 1u;
    UI_ActivatePage(UI_PAGE_RESULTS);

    /* ⚠️ 仅供演示 / 自动化截图:`CV_UI_DEMO_PROBE=<电压>` 让结果页
       一进来就显示一次探针读数。正常运行时变量不设,零影响。 */
    {
        const char *env = getenv("CV_UI_DEMO_PROBE");

        if ((env != NULL) && (env[0] != '\0'))
        {
            float probe_voltage = strtof(env, NULL);
            float probe_cs;
            lv_point_t probe_point;

            if ((probe_voltage > 0.0f) && isfinite(probe_voltage) &&
                (g_results_chart != NULL) &&
                SweepInterpolation_Evaluate(&g_sweep_cs_curve[0],
                                            probe_voltage, &probe_cs) &&
                UI_SweepValueToPixel(g_results_chart, probe_voltage,
                                     probe_cs, &probe_point))
            {
                (void)UI_SelectSweepProbe(g_results_chart, 1u, &probe_point,
                                          false, true);
                if (g_results_probe_label != NULL)
                {
                    lv_obj_set_style_text_color(
                        g_results_probe_label,
                        lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
                }
            }
        }
    }
}

static void UI_ShowSweep(void)
{
    static const char *const path_names[] =
        {"CISS", "COSS", "CRSS", "全部"};
    static const char *const legend_names[] =
        {"CISS", "COSS", "CRSS"};
    lv_obj_t *screen = g_page_containers[UI_PAGE_SWEEP];
    lv_obj_t *chart_panel;
    lv_obj_t *legend_panel;
    lv_coord_t chart_width = (lv_coord_t)LCD_PIXEL_WIDTH - 16;
    lv_coord_t chart_height = (lv_coord_t)LCD_PIXEL_HEIGHT -
                              UI_COMMON_HEADER_HEIGHT - 40;
    lv_coord_t control_button_width = UI_SWEEP_DRAWER_WIDTH - 36;
    uint32_t index;

    g_ui_current_page = UI_PAGE_SWEEP;
    g_ui_display_bias_tenths = g_ui_sweep_end_tenths;
    g_ui_display_path = (g_ui_sweep_path == 3u) ?
                        UI_DISPLAY_PATH_ALL : g_ui_sweep_path;

    if (g_page_built[UI_PAGE_SWEEP] != 0u)
    {
        UI_SetSweepDrawerOpen(false);
        UI_ActivatePage(UI_PAGE_SWEEP);
        return;
    }
    g_sweep_curve_page = UI_CreateSweepSubpageContainer(screen, 0);
    chart_panel = UI_CreatePanel(g_sweep_curve_page,
                                 8,
                                 6,
                                 chart_width,
                                 chart_height,
                                 0x050505u,
                                 2);
    lv_obj_set_style_border_color(chart_panel,
                                  lv_color_hex(0xffffffu),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_panel, 1, LV_PART_MAIN);
    UI_CreateText(chart_panel,
                  "Cs / pF (log)",
                  &lv_font_montserrat_16, 0xffffffu,
                  8, 6, 132, 22, LV_TEXT_ALIGN_LEFT);
    legend_panel = UI_CreatePanel(chart_panel,
                                  (chart_width - 474) / 2,
                                  4,
                                  474,
                                  30,
                                  0x080808u,
                                  0);
    lv_obj_set_style_border_color(legend_panel,
                                  lv_color_hex(0x263747u),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(legend_panel, 1, LV_PART_MAIN);
    for (index = 0u; index < 3u; ++index)
    {
        lv_obj_t *line = lv_obj_create(legend_panel);
        lv_coord_t item_x = 18 + (lv_coord_t)index * 150;

        lv_obj_set_pos(line, item_x, 13);
        lv_obj_set_size(line, 34, 3);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(line,
                                  lv_color_hex(g_sweep_series_colors[index]),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(line, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);
        UI_CreateText(legend_panel, legend_names[index],
                      &lv_font_montserrat_16, 0xffffffu,
                      item_x + 42, 4, 76, 20,
                      LV_TEXT_ALIGN_LEFT);
    }
    g_sweep_chart = lv_chart_create(chart_panel);
    lv_obj_set_pos(g_sweep_chart, 64, 42);
    lv_obj_set_size(g_sweep_chart, chart_width - 82, 402);
    lv_chart_set_type(g_sweep_chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(g_sweep_chart, UI_SWEEP_RENDER_POINTS);
    lv_chart_set_range(g_sweep_chart, LV_CHART_AXIS_PRIMARY_X,
                       g_sweep_x_min_log_milli,
                       g_sweep_x_max_log_milli);
    lv_chart_set_range(g_sweep_chart, LV_CHART_AXIS_PRIMARY_Y,
                       g_sweep_y_min_log_milli,
                       g_sweep_y_max_log_milli);
    lv_chart_set_div_line_count(g_sweep_chart, 0u, 0u);
    lv_chart_set_axis_tick(g_sweep_chart,
                           LV_CHART_AXIS_PRIMARY_X,
                           6, 3, 4,
                           1,
                           true, 38);
    lv_chart_set_axis_tick(g_sweep_chart,
                           LV_CHART_AXIS_PRIMARY_Y,
                           6, 3, 5,
                           1,
                           true, 58);
    lv_obj_set_style_bg_color(g_sweep_chart,
                              lv_color_hex(0x050505u),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_sweep_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_sweep_chart,
                                  lv_color_hex(0xffffffu),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sweep_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_sweep_chart,
                                lv_color_hex(0xffffffu),
                                LV_PART_MAIN);
    lv_obj_set_style_line_width(g_sweep_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(g_sweep_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(g_sweep_chart, &lv_font_montserrat_16,
                               LV_PART_TICKS);
    lv_obj_set_style_text_color(g_sweep_chart, lv_color_hex(0xffffffu),
                                LV_PART_TICKS);
    lv_obj_set_style_line_color(g_sweep_chart, lv_color_hex(0xffffffu),
                                LV_PART_TICKS);
    lv_obj_set_style_line_opa(g_sweep_chart, LV_OPA_COVER,
                              LV_PART_TICKS);
    lv_obj_set_style_pad_left(g_sweep_chart, 5, LV_PART_TICKS);
    lv_obj_set_style_pad_bottom(g_sweep_chart, 4, LV_PART_TICKS);
    lv_obj_add_flag(g_sweep_chart, LV_OBJ_FLAG_CLICKABLE |
                                   LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(g_sweep_chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartDrawEvent,
                        LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartProbeDrawEvent,
                        LV_EVENT_DRAW_POST, NULL);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartTouchEvent,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartTouchEvent,
                        LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartTouchEvent,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_sweep_chart, UI_SweepChartTouchEvent,
                        LV_EVENT_PRESS_LOST, NULL);
    g_sweep_series[0] =
        lv_chart_add_series(g_sweep_chart,
                            lv_color_hex(g_sweep_series_colors[0]),
                            LV_CHART_AXIS_PRIMARY_Y);
    g_sweep_series[1] =
        lv_chart_add_series(g_sweep_chart,
                            lv_color_hex(g_sweep_series_colors[1]),
                            LV_CHART_AXIS_PRIMARY_Y);
    g_sweep_series[2] =
        lv_chart_add_series(g_sweep_chart,
                            lv_color_hex(g_sweep_series_colors[2]),
                            LV_CHART_AXIS_PRIMARY_Y);
    UI_CreateText(chart_panel,
                  "Vd / V (log)",
                  &lv_font_montserrat_16, 0xffffffu,
                  64, chart_height - 30, chart_width - 82, 22,
                  LV_TEXT_ALIGN_CENTER);

    g_sweep_progress_bar =
        UI_CreatePanel(g_sweep_curve_page,
                       8,
                       (lv_coord_t)LCD_PIXEL_HEIGHT -
                       UI_COMMON_HEADER_HEIGHT - 28,
                       (lv_coord_t)LCD_PIXEL_WIDTH - 16,
                       22,
                       0xadb8c2u,
                       1);
    lv_obj_set_style_border_color(g_sweep_progress_bar,
                                  lv_color_hex(0x4a5c6du),
                                  LV_PART_MAIN);
    g_sweep_progress_indicator = lv_obj_create(g_sweep_progress_bar);
    lv_obj_set_pos(g_sweep_progress_indicator, 1, 1);
    lv_obj_set_size(g_sweep_progress_indicator, 1, 20);
    lv_obj_clear_flag(g_sweep_progress_indicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_sweep_progress_indicator,
                              lv_color_hex(0x4f91c7u),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_sweep_progress_indicator, LV_OPA_COVER,
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sweep_progress_indicator, 0,
                                  LV_PART_MAIN);
    lv_obj_set_style_radius(g_sweep_progress_indicator, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_sweep_progress_indicator, 0, LV_PART_MAIN);
    UI_SetSweepProgress(0u);
    g_sweep_progress_label =
        UI_CreateText(g_sweep_curve_page,
                      "等待测量后端发送扫压数据",
                      &ui_font_cn_20, 0xffffffu,
                      16, (lv_coord_t)LCD_PIXEL_HEIGHT -
                      UI_COMMON_HEADER_HEIGHT - 27,
                      (lv_coord_t)LCD_PIXEL_WIDTH - 32, 20,
                      LV_TEXT_ALIGN_CENTER);

    g_sweep_control_panel = UI_CreatePanel(
        g_sweep_curve_page,
        (lv_coord_t)LCD_PIXEL_WIDTH - UI_SWEEP_DRAWER_WIDTH - 8,
        6,
        UI_SWEEP_DRAWER_WIDTH,
        chart_height,
        0x5f6464u,
        2);
    lv_obj_set_style_border_color(g_sweep_control_panel,
                                  lv_color_hex(UI_COLOR_PURPLE),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sweep_control_panel, 2, LV_PART_MAIN);
    UI_CreateText(g_sweep_control_panel, "扫压设置", &ui_font_cn_20,
                  UI_COLOR_TEXT, 12, 8, 112, 30,
                  LV_TEXT_ALIGN_LEFT);
    UI_CreateButton(g_sweep_control_panel,
                    "关闭",
                    UI_COLOR_SURFACE,
                    136,
                    6,
                    68,
                    36,
                    UI_SweepDrawerEvent,
                    (void *)(uintptr_t)0u);

    for (index = 0u; index < 4u; ++index)
    {
        g_sweep_path_buttons[index] =
            UI_CreateButton(g_sweep_control_panel,
                            path_names[index],
                            UI_COLOR_SURFACE,
                            18,
                            52 + (lv_coord_t)index * 48,
                            control_button_width,
                            42,
                            UI_SimpleActionEvent,
                            (void *)&g_sweep_path_actions[index]);
    }
    UI_UpdateSweepPathStyle();

    {
        lv_obj_t *bias_button =
            UI_CreateButton(g_sweep_control_panel,
                            "终止电压:0.1 V",
                            UI_COLOR_BLUE,
                            18,
                            252,
                            control_button_width,
                            52,
                            UI_BiasOpenEvent,
                            (void *)(uintptr_t)UI_BIAS_TARGET_SWEEP);

        g_sweep_bias_value_label = lv_obj_get_child(bias_button, 0);
        UI_UpdateSweepBiasLabel();
    }

    UI_CreateButton(g_sweep_control_panel,
                    "开始扫压",
                    UI_COLOR_GREEN,
                    18,
                    330,
                    control_button_width,
                    52,
                    UI_SimpleActionEvent,
                    (void *)&g_sweep_start_action);
    UI_CreateButton(g_sweep_control_panel,
                    "停止扫压",
                    UI_COLOR_RED,
                    18,
                    394,
                    control_button_width,
                    52,
                    UI_SimpleActionEvent,
                    (void *)&g_sweep_stop_action);
    UI_CreateText(g_sweep_control_panel,
                  "关闭设置后曲线全屏显示",
                  &ui_font_cn_20, UI_COLOR_STATUS,
                  14, 458, UI_SWEEP_DRAWER_WIDTH - 28, 42,
                  LV_TEXT_ALIGN_CENTER);

    g_sweep_settings_button =
        UI_CreateButton(g_sweep_curve_page,
                        "设置",
                        UI_COLOR_BLUE,
                        (lv_coord_t)LCD_PIXEL_WIDTH - 82,
                        12,
                        70,
                        40,
                        UI_SweepDrawerEvent,
                        (void *)(uintptr_t)1u);
    UI_RebuildSweepChart();
    UI_SetSweepDrawerOpen(false);

    g_page_built[UI_PAGE_SWEEP] = 1u;
    UI_ActivatePage(UI_PAGE_SWEEP);
}

uint32_t UI_Demo_Init(void)
{
    lv_obj_t *root_screen;
    uint32_t index;

    if (lv_scr_act() == NULL)
    {
        return UI_DEMO_ERROR;
    }
    if (g_sweep_chart_refresh_timer != NULL)
    {
        lv_timer_del(g_sweep_chart_refresh_timer);
        g_sweep_chart_refresh_timer = NULL;
    }

    g_ui_click_count = 0u;
    g_ui_last_button = 0u;
    g_ui_current_page = UI_PAGE_HOME;
    g_ui_reset_request_count = 0u;
    g_ui_single_path = UI_SINGLE_PATH_CISS;
    g_ui_sweep_path = 0u;
    g_ui_single_bias_tenths = UI_BIAS_MIN_TENTHS;
    g_ui_sweep_end_tenths = UI_BIAS_MIN_TENTHS;
    g_ui_analysis_voltage_tenths = UI_BIAS_MIN_TENTHS;
    g_ui_analysis_voltage_max_tenths = UI_BIAS_MIN_TENTHS;
    g_ui_display_bias_tenths = UI_BIAS_MIN_TENTHS;
    g_ui_display_path = UI_SINGLE_PATH_CISS;
    g_ui_immediate_refresh_enabled = 0u;
    g_keypad_target = UI_BIAS_TARGET_SINGLE;
    g_status_label = NULL;
    g_modal_overlay = NULL;
    g_common_header = NULL;
    g_header_back_button = NULL;
    g_header_title_label = NULL;
    g_header_connection_label = NULL;
    g_header_bias_label = NULL;
    g_header_path_label = NULL;
    g_header_gear_label = NULL;
    g_header_operation_label = NULL;
    g_keypad_overlay = NULL;
    g_keypad_title_label = NULL;
    g_single_bias_value_label = NULL;
    g_sweep_bias_value_label = NULL;
    g_single_result_label = NULL;
    g_single_result_note_label = NULL;
    g_single_result_panel = NULL;
    g_measure_all_result_panel = NULL;
    g_sweep_chart = NULL;
    g_sweep_progress_label = NULL;
    g_sweep_progress_bar = NULL;
    g_sweep_progress_indicator = NULL;
    g_sweep_control_panel = NULL;
    g_sweep_settings_button = NULL;
    g_sweep_curve_page = NULL;
    g_results_analysis_value_label = NULL;
    g_results_analysis_track = NULL;
    g_results_analysis_knob = NULL;
    g_results_probe_label = NULL;
    g_results_summary_label = NULL;
    (void)memset(g_results_parameter_value_labels, 0,
                 sizeof(g_results_parameter_value_labels));
    g_results_transfer_qrcode = NULL;
    g_results_transfer_status_label = NULL;
    g_results_transfer_payload_label = NULL;
    g_results_transfer_model_label = NULL;
    g_sweep_probe_chart = NULL;
    g_sweep_probe_owner = 0u;
    g_keypad_value_label = NULL;
    g_keypad_note_label = NULL;
    g_results_chart = NULL;
    (void)memset(g_result_cards, 0, sizeof(g_result_cards));
    (void)memset(g_result_value_labels, 0, sizeof(g_result_value_labels));
    (void)memset(g_result_state_labels, 0, sizeof(g_result_state_labels));
    (void)memset(g_results_series, 0, sizeof(g_results_series));
    UI_ResultSnapshotsClear();
    g_results_built = 0u;
    (void)memset(g_page_containers, 0, sizeof(g_page_containers));
    (void)memset(g_page_built, 0, sizeof(g_page_built));
    (void)memset(g_single_path_buttons, 0, sizeof(g_single_path_buttons));
    (void)memset(g_sweep_path_buttons, 0, sizeof(g_sweep_path_buttons));
    (void)memset(g_all_result_labels, 0, sizeof(g_all_result_labels));
    (void)memset(g_sweep_series, 0, sizeof(g_sweep_series));
    (void)memset(g_sweep_voltage, 0, sizeof(g_sweep_voltage));
    (void)memset(g_sweep_capacitance, 0, sizeof(g_sweep_capacitance));
    (void)memset(g_sweep_parallel_capacitance, 0,
                 sizeof(g_sweep_parallel_capacitance));
    (void)memset(g_sweep_dissipation, 0, sizeof(g_sweep_dissipation));
    (void)memset(g_sweep_model_valid, 0, sizeof(g_sweep_model_valid));
    (void)memset(g_sweep_valid, 0, sizeof(g_sweep_valid));
    (void)memset(g_sweep_total_points, 0, sizeof(g_sweep_total_points));
    (void)memset(g_sweep_cs_curve, 0, sizeof(g_sweep_cs_curve));
    (void)memset(g_sweep_cp_curve, 0, sizeof(g_sweep_cp_curve));
    (void)memset(g_sweep_d_curve, 0, sizeof(g_sweep_d_curve));
    (void)memset(g_sweep_interpolation_ready, 0,
                 sizeof(g_sweep_interpolation_ready));
    (void)memset(g_keypad_buffer, 0, sizeof(g_keypad_buffer));
    (void)strcpy(g_ui_actual_bias, "0.1V");
    (void)strcpy(g_ui_actual_path, "CISS");
    g_sweep_visible_mask = 0u;
    g_sweep_probe_path = 0u;
    g_sweep_probe_index = 0u;
    g_sweep_probe_voltage = 0.0f;
    g_sweep_probe_cs = 0.0f;
    g_sweep_probe_cp = 0.0f;
    g_sweep_probe_d = 0.0f;
    g_sweep_probe_model_valid = 0u;
    g_sweep_probe_interpolated = 0u;
    g_sweep_probe_valid = 0u;
    g_sweep_probe_tracking = 0u;
    g_sweep_inspection_ready = 0u;
    g_sweep_analysis_initialized = 0u;
    g_transfer_url[0] = '\0';
    g_transfer_acv1_nodes = 0u;
    g_transfer_scv1_points = 0u;
    g_transfer_has_payload = 0u;
    g_transfer_qr_dirty = 1u;
    g_sweep_last_tap_valid = 0u;
    g_sweep_single_touch_moved = 0u;
    g_sweep_last_tap_tick = 0u;
    g_sweep_touch_press_point.x = 0;
    g_sweep_touch_press_point.y = 0;
    g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
    g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
    g_sweep_y_min_log_milli = UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI;
    g_sweep_y_max_log_milli = UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI;
    g_sweep_full_y_min_log_milli = UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI;
    g_sweep_full_y_max_log_milli = UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI;
    (void)memset(&g_sweep_gesture, 0, sizeof(g_sweep_gesture));
    g_sweep_chart_dirty = 0u;
    g_latest_rg_ohm = 0.0f;
    g_latest_rg_valid = 0u;

    root_screen = UI_PrepareRootScreen();
    for (index = 0u; index < 5u; ++index)
    {
        g_page_containers[index] = UI_CreatePageContainer(root_screen);
    }
    UI_CreateBiasKeypad();

    UI_ShowInitialize();
    UI_ShowMeasure();
    UI_ShowSweep();
    UI_ShowHome();
    g_sweep_chart_refresh_timer =
        lv_timer_create(UI_SweepChartRefreshTimer,
                        UI_SWEEP_CHART_REFRESH_MS,
                        NULL);
    g_ui_immediate_refresh_enabled = 1u;
    return UI_DEMO_OK;
}

static void UI_RebuildSweepChart(void)
{
    uint32_t path;
    int32_t minimum_y = UI_SWEEP_DEFAULT_Y_MIN_LOG_MILLI;
    int32_t maximum_y = UI_SWEEP_DEFAULT_Y_MAX_LOG_MILLI;
    uint32_t found_value = 0u;

    if (g_sweep_chart == NULL)
    {
        return;
    }

    for (path = 0u; path < 3u; ++path)
    {
        uint32_t index;

        if ((g_sweep_visible_mask & (1u << path)) == 0u)
        {
            continue;
        }
        for (index = 0u;
             (index < g_sweep_total_points[path]) &&
             (index < UI_SWEEP_MAX_POINTS);
             ++index)
        {
            float capacitance = g_sweep_capacitance[path][index];
            int32_t log_value;

            if ((g_sweep_valid[path][index] == 0u) ||
                !isfinite(capacitance) || (capacitance <= 0.0f))
            {
                continue;
            }
            log_value = (int32_t)(log10f(capacitance) * 1000.0f);
            if (found_value == 0u)
            {
                minimum_y = log_value;
                maximum_y = log_value;
                found_value = 1u;
            }
            else
            {
                if (log_value < minimum_y)
                {
                    minimum_y = log_value;
                }
                if (log_value > maximum_y)
                {
                    maximum_y = log_value;
                }
            }
        }
    }

    if (found_value != 0u)
    {
        minimum_y =
            (int32_t)(floorf((float)minimum_y / 1000.0f) * 1000.0f);
        maximum_y =
            (int32_t)(ceilf((float)maximum_y / 1000.0f) * 1000.0f);
        if (maximum_y <= minimum_y)
        {
            maximum_y = minimum_y + 1000;
        }
    }
    g_sweep_full_y_min_log_milli = minimum_y;
    g_sweep_full_y_max_log_milli = maximum_y;
    if (g_sweep_inspection_ready == 0u)
    {
        g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
        g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
        g_sweep_y_min_log_milli = minimum_y;
        g_sweep_y_max_log_milli = maximum_y;
    }

    for (path = 0u; path < 3u; ++path)
    {
        uint32_t index;
        lv_chart_series_t *series = g_sweep_series[path];
        uint32_t visible = g_sweep_visible_mask & (1u << path);

        if (series == NULL)
        {
            continue;
        }
        lv_chart_hide_series(g_sweep_chart, series, visible == 0u);
        lv_chart_set_all_value(g_sweep_chart, series, LV_CHART_POINT_NONE);
        if (visible == 0u)
        {
            continue;
        }

        if (g_sweep_interpolation_ready[path] != 0u)
        {
            float minimum_log_voltage;
            float maximum_log_voltage;

            if (SweepInterpolation_GetLogVoltageRange(
                    &g_sweep_cs_curve[path],
                    &minimum_log_voltage,
                    &maximum_log_voltage))
            {
                uint32_t render_index;

                if (minimum_log_voltage <
                    ((float)UI_SWEEP_X_MIN_LOG_MILLI / 1000.0f))
                {
                    minimum_log_voltage =
                        (float)UI_SWEEP_X_MIN_LOG_MILLI / 1000.0f;
                }
                if (maximum_log_voltage >
                    ((float)UI_SWEEP_X_MAX_LOG_MILLI / 1000.0f))
                {
                    maximum_log_voltage =
                        (float)UI_SWEEP_X_MAX_LOG_MILLI / 1000.0f;
                }

                for (render_index = 0u;
                     (render_index < UI_SWEEP_RENDER_POINTS) &&
                     (maximum_log_voltage >= minimum_log_voltage);
                     ++render_index)
                {
                    float ratio =
                        (UI_SWEEP_RENDER_POINTS > 1u) ?
                        ((float)render_index /
                         (float)(UI_SWEEP_RENDER_POINTS - 1u)) : 0.0f;
                    float log_voltage =
                        minimum_log_voltage +
                        ratio * (maximum_log_voltage -
                                 minimum_log_voltage);
                    float log_capacitance;

                    if (!SweepInterpolation_EvaluateTransformed(
                            &g_sweep_cs_curve[path], log_voltage,
                            &log_capacitance))
                    {
                        continue;
                    }
                    lv_chart_set_next_value2(
                        g_sweep_chart, series,
                        (lv_coord_t)lroundf(log_voltage * 1000.0f),
                        (lv_coord_t)lroundf(log_capacitance * 1000.0f));
                }
                continue;
            }
        }

        for (index = 0u;
             (index < g_sweep_total_points[path]) &&
             (index < UI_SWEEP_MAX_POINTS);
             ++index)
        {
            float voltage = g_sweep_voltage[path][index];
            float capacitance = g_sweep_capacitance[path][index];
            lv_coord_t x_value;
            lv_coord_t y_value;

            if ((g_sweep_valid[path][index] == 0u) ||
                !isfinite(voltage) || !isfinite(capacitance) ||
                (capacitance <= 0.0f))
            {
                continue;
            }
            if (voltage < 0.1f)
            {
                voltage = 0.1f;
            }
            x_value = (lv_coord_t)(log10f(voltage) * 1000.0f);
            y_value = (lv_coord_t)(log10f(capacitance) * 1000.0f);
            lv_chart_set_next_value2(g_sweep_chart, series,
                                     x_value, y_value);
        }
    }
    UI_ApplySweepViewport();
    g_sweep_chart_dirty = 0u;
}

static void UI_RequestSweepChartRefresh(void)
{
    g_sweep_chart_dirty = 1u;
    if ((g_sweep_chart_refresh_timer == NULL) &&
        (g_ui_current_page == UI_PAGE_SWEEP))
    {
        UI_RebuildSweepChart();
    }
}

static void UI_SweepChartRefreshTimer(lv_timer_t *timer)
{
    (void)timer;
    if ((g_sweep_chart_dirty == 0u) ||
        (g_ui_current_page != UI_PAGE_SWEEP))
    {
        return;
    }

    UI_RebuildSweepChart();
}

void UI_Demo_SetConnection(uint32_t online)
{
    if (g_header_connection_label == NULL)
    {
        return;
    }
    lv_label_set_text(g_header_connection_label,
                      (online != 0u) ? "后端:在线" : "后端:未连接");
    lv_obj_set_style_text_color(g_header_connection_label,
                                lv_color_hex((online != 0u) ?
                                             UI_COLOR_GREEN :
                                             UI_COLOR_ORANGE),
                                LV_PART_MAIN);
}

void UI_Demo_SetStatusText(const char *text)
{
    if ((text != NULL) && (g_status_label != NULL))
    {
        lv_label_set_text(g_status_label, text);
    }
}

/* 🆕 2026-10-06:测量参数那一行(**「系统复位」页、复位按钮正下方**)。
 * 与上面那个不是一回事 —— 上面是顶栏的操作结果,这个是本页常驻的参数读数。
 * ⚠️ 页面还没构建过时 `g_init_status_label` 是 NULL ⇒ 静默跳过。
 *    这是**故意的**:上电时 MeasureConfig_Init() 就在 UI_Demo_Init() 之后立刻调,
 *    而「系统复位」页要到用户第一次点进去才构建。值为 NULL 时跳过,
 *    等页面构建时再用 UI_UpdateInitConfigStyle() + 一次主动刷新补上。 */
void UI_Demo_SetInitStatusText(const char *text)
{
    if ((text != NULL) && (g_init_status_label != NULL))
    {
        lv_label_set_text(g_init_status_label, text);
    }
}

void UI_Demo_SetActualBias(const char *bias_text)
{
    if (bias_text == NULL)
    {
        return;
    }
    (void)strncpy(g_ui_actual_bias, bias_text,
                  sizeof(g_ui_actual_bias) - 1u);
    g_ui_actual_bias[sizeof(g_ui_actual_bias) - 1u] = '\0';
    UI_UpdateCommonStatus();
}

void UI_Demo_SetActualPath(const char *path_text)
{
    if (path_text == NULL)
    {
        return;
    }
    (void)strncpy(g_ui_actual_path, path_text,
                  sizeof(g_ui_actual_path) - 1u);
    g_ui_actual_path[sizeof(g_ui_actual_path) - 1u] = '\0';
    UI_UpdateCommonStatus();
}

/* 🆕 2026-10-10:顶栏「档位:」栏。与上面 `SetActualPath` 逐行同构 ——
 * 同一条数据通路(存缓冲 → UI_UpdateCommonStatus → 刷标签),
 * 内容同样是 **PL 回读的已生效值**。调用方 = `app/relay_view.c`。
 *
 * 🆕 2026-10-15:多了 @p gear_code,**测量页那 4 颗挡位按钮的高亮也在这里刷**。
 *    ⇒ 顶栏文字与按钮高亮**同源、同一拍更新**,不可能出现"顶栏说 B、
 *      按钮高亮在 D"。这是个不变量:`g_ui_gear` 全工程**只有本函数写**。
 *    ⛔ 别在按键回调里顺手设一下 `g_ui_gear` "让点击响应更快"  ——
 *      写失败时按钮就会高亮一颗硬件根本没切的挡位,而 R0 读了旧值、
 *      四个读数一起错却没人报警。那正是本模块要消灭的那类谎话。 */
void UI_Demo_SetActualGear(const char *gear_text, uint32_t gear_code)
{
    if (gear_text == NULL)
    {
        return;
    }
    (void)strncpy(g_ui_actual_gear, gear_text,
                  sizeof(g_ui_actual_gear) - 1u);
    g_ui_actual_gear[sizeof(g_ui_actual_gear) - 1u] = '\0';
    g_ui_gear = gear_code & 0xFu;
    UI_UpdateGearStyle();
    UI_UpdateCommonStatus();
}

void UI_Demo_UpdateSingleCapacitance(uint32_t path,
                                     const char *cs_text,
                                     const char *cp_text,
                                     const char *d_text,
                                     uint32_t valid)
{
    if ((cs_text == NULL) || (cp_text == NULL) || (d_text == NULL))
    {
        return;
    }

    /* 🔴 「全部」模式:测量页显示的是三张卡(g_all_result_labels[0..2]),
     *    单卡 label 被隐藏。协议层的三腿序列是**逐腿**调用本函数
     *    (path = 0/1/2),不会调 UI_Demo_UpdateAllCapacitance ⇒ 原实现
     *    下面那句 `path != g_ui_single_path` 会把三腿全部挡掉,卡片永远是 --。
     *    这里按当前页面模式转交,而不是让协议层去猜 UI 处于哪个布局。 */
    if ((g_ui_single_path == UI_DISPLAY_PATH_ALL) && (path < 3u))
    {
        UI_Demo_UpdateAllCapacitance(path, cs_text, cp_text, d_text, valid);
        return;
    }

    /* 先记进结果页快照 —— 结果页与测量页共用同一份来源,不重复解析 */
    UI_ResultRecord(path, cs_text, cp_text, d_text, valid);

    if ((g_single_result_label == NULL) || (path != g_ui_single_path))
    {
        return;
    }

    lv_label_set_text_fmt(g_single_result_label,
                          "Cs  %s pF\nCp  %s pF\nD   %s",
                          cs_text, cp_text, d_text);
    if (g_single_result_note_label != NULL)
    {
        lv_label_set_text(g_single_result_note_label,
                          (valid != 0u) ?
                          "测量结果有效" : "测量结果无效");
        lv_obj_set_style_text_color(g_single_result_note_label,
                                    lv_color_hex((valid != 0u) ?
                                                 UI_COLOR_GREEN :
                                                 UI_COLOR_RED),
                                    LV_PART_MAIN);
    }
}

void UI_Demo_UpdateAllCapacitance(uint32_t path,
                                  const char *cs_text,
                                  const char *cp_text,
                                  const char *d_text,
                                  uint32_t valid)
{
    static const char *const names[] = {"CISS", "COSS", "CRSS"};
    lv_obj_t *label;

    if ((path >= 3u) || (cs_text == NULL) || (cp_text == NULL) ||
        (d_text == NULL))
    {
        return;
    }

    UI_ResultRecord(path, cs_text, cp_text, d_text, valid);

    label = g_all_result_labels[path];
    if (label == NULL)
    {
        return;
    }
    lv_label_set_text_fmt(label,
                          "%s\n\nCs  %s pF\nCp  %s pF\nD   %s%s",
                          names[path], cs_text, cp_text, d_text,
                          (valid != 0u) ? "" : "  无效");
    lv_obj_set_style_text_color(label,
                                lv_color_hex((valid != 0u) ?
                                             UI_COLOR_TEXT : UI_COLOR_RED),
                                LV_PART_MAIN);
}

void UI_Demo_UpdateResistance(const char *resistance_text, uint32_t valid)
{
    if (resistance_text == NULL)
    {
        return;
    }
    if (valid != 0u)
    {
        float resistance;

        if (UI_ParseNonnegativeDecimal(resistance_text, &resistance) &&
            isfinite(resistance) &&
            (resistance >= 0.0f))
        {
            g_latest_rg_ohm = resistance;
            g_latest_rg_valid = 1u;
            UI_ResultRecord(3u, resistance_text, NULL, NULL, valid);
            /* 结果页的 Rg 参数卡与曲线摘要立即跟上。 */
            UI_UpdateSweepAnalysisCard();
        }
    }
    if ((g_single_result_label == NULL) ||
        (g_ui_single_path != UI_SINGLE_PATH_RG))
    {
        return;
    }
    lv_label_set_text_fmt(g_single_result_label, "RG\n%s Ω", resistance_text);
    if (g_single_result_note_label != NULL)
    {
        lv_label_set_text(g_single_result_note_label,
                          (valid != 0u) ?
                          "电阻测量结果有效" : "电阻测量结果无效");
        lv_obj_set_style_text_color(g_single_result_note_label,
                                    lv_color_hex((valid != 0u) ?
                                                 UI_COLOR_GREEN :
                                                 UI_COLOR_RED),
                                    LV_PART_MAIN);
    }
}

void UI_Demo_ClearAllResults(void)
{
    static const char *const empty_text[] =
    {
        "CISS\n\nCs  -- pF\nCp  -- pF\nD   --",
        "COSS\n\nCs  -- pF\nCp  -- pF\nD   --",
        "CRSS\n\nCs  -- pF\nCp  -- pF\nD   --"
    };
    uint32_t path;

    for (path = 0u; path < 3u; ++path)
    {
        if (g_all_result_labels[path] != NULL)
        {
            lv_label_set_text(g_all_result_labels[path], empty_text[path]);
            lv_obj_set_style_text_color(g_all_result_labels[path],
                                        lv_color_hex(UI_COLOR_TEXT),
                                        LV_PART_MAIN);
        }
    }

    /* 结果页是投影视图:「清空」必须同时清掉它的数据来源快照,
       否则结果页会留着上一轮的读数,与源页面不一致。 */
    UI_ResultSnapshotsClear();
    if (g_single_result_label != NULL)
    {
        lv_label_set_text(g_single_result_label,
                          "Cs  -- pF\nCp  -- pF\nD   --");
    }
    UI_ResultsRefresh();
}

void UI_Demo_ClearSweep(uint32_t path_or_all)
{
    uint32_t first_path;
    uint32_t last_path;
    uint32_t path;

    if (path_or_all < 3u)
    {
        first_path = path_or_all;
        last_path = path_or_all;
        g_sweep_visible_mask = 1u << path_or_all;
    }
    else
    {
        first_path = 0u;
        last_path = 2u;
        g_sweep_visible_mask = 0x07u;
    }

    for (path = first_path; path <= last_path; ++path)
    {
        (void)memset(g_sweep_voltage[path], 0,
                     sizeof(g_sweep_voltage[path]));
        (void)memset(g_sweep_capacitance[path], 0,
                     sizeof(g_sweep_capacitance[path]));
        (void)memset(g_sweep_parallel_capacitance[path], 0,
                     sizeof(g_sweep_parallel_capacitance[path]));
        (void)memset(g_sweep_dissipation[path], 0,
                     sizeof(g_sweep_dissipation[path]));
        (void)memset(g_sweep_model_valid[path], 0,
                     sizeof(g_sweep_model_valid[path]));
        (void)memset(g_sweep_valid[path], 0,
                     sizeof(g_sweep_valid[path]));
        g_sweep_total_points[path] = 0u;
        UI_ResetSweepInterpolation(path);
    }
    g_sweep_inspection_ready = 0u;
    g_sweep_analysis_initialized = 0u;
    UI_UpdateSweepAnalysisCard();
    g_sweep_last_tap_valid = 0u;
    g_sweep_single_touch_moved = 0u;
    g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
    g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
    (void)memset(&g_sweep_gesture, 0, sizeof(g_sweep_gesture));
    UI_ResetSweepProbe();
    if (g_sweep_progress_label != NULL)
    {
        lv_label_set_text(g_sweep_progress_label,
                          "等待测量后端发送扫压数据");
    }
    if (g_sweep_progress_bar != NULL)
    {
        UI_SetSweepProgress(0u);
    }
    UI_RebuildSweepChart();
}

void UI_Demo_BeginSweep(uint32_t path, uint32_t total_points,
                        uint32_t multi_path)
{
    if (path >= 3u)
    {
        return;
    }
    (void)memset(g_sweep_voltage[path], 0,
                 sizeof(g_sweep_voltage[path]));
    (void)memset(g_sweep_capacitance[path], 0,
                 sizeof(g_sweep_capacitance[path]));
    (void)memset(g_sweep_parallel_capacitance[path], 0,
                 sizeof(g_sweep_parallel_capacitance[path]));
    (void)memset(g_sweep_dissipation[path], 0,
                 sizeof(g_sweep_dissipation[path]));
    (void)memset(g_sweep_model_valid[path], 0,
                 sizeof(g_sweep_model_valid[path]));
    (void)memset(g_sweep_valid[path], 0,
                 sizeof(g_sweep_valid[path]));
    UI_ResetSweepInterpolation(path);
    g_sweep_inspection_ready = 0u;
    g_sweep_analysis_initialized = 0u;
    UI_UpdateSweepAnalysisCard();
    g_sweep_last_tap_valid = 0u;
    g_sweep_single_touch_moved = 0u;
    g_sweep_x_min_log_milli = UI_SWEEP_X_MIN_LOG_MILLI;
    g_sweep_x_max_log_milli = UI_SWEEP_X_MAX_LOG_MILLI;
    (void)memset(&g_sweep_gesture, 0, sizeof(g_sweep_gesture));
    UI_ResetSweepProbe();
    g_sweep_total_points[path] =
        (total_points > UI_SWEEP_MAX_POINTS) ?
        UI_SWEEP_MAX_POINTS : total_points;
    g_sweep_visible_mask = (multi_path != 0u) ? 0x07u : (1u << path);
    if (g_sweep_progress_bar != NULL)
    {
        UI_SetSweepProgress(0u);
    }
    UI_RebuildSweepChart();
}

void UI_Demo_UpdateSweepPoint(uint32_t path,
                              uint32_t point_index,
                              uint32_t total_points,
                              float bias_voltage_v,
                              float series_capacitance_pf,
                              float parallel_capacitance_pf,
                              float dissipation_factor,
                              uint32_t model_values_valid,
                              const char *bias_text,
                              const char *capacitance_text,
                              uint32_t valid)
{
    static const char *const names[] = {"CISS", "COSS", "CRSS"};
    uint32_t array_index;
    uint8_t rebuild_interpolation;

    if ((path >= 3u) || (point_index == 0u) ||
        (point_index > UI_SWEEP_MAX_POINTS))
    {
        return;
    }
    array_index = point_index - 1u;
    rebuild_interpolation = g_sweep_interpolation_ready[path];
    if (rebuild_interpolation != 0u)
    {
        /* 收到补发或更新点时先使旧插值失效，防止显示旧曲线。 */
        UI_ResetSweepInterpolation(path);
    }
    g_sweep_voltage[path][array_index] = bias_voltage_v;
    g_sweep_capacitance[path][array_index] = series_capacitance_pf;
    g_sweep_parallel_capacitance[path][array_index] =
        parallel_capacitance_pf;
    g_sweep_dissipation[path][array_index] = dissipation_factor;
    g_sweep_model_valid[path][array_index] =
        ((model_values_valid != 0u) &&
         isfinite(parallel_capacitance_pf) &&
         isfinite(dissipation_factor)) ? 1u : 0u;
    g_sweep_valid[path][array_index] =
        ((valid != 0u) && isfinite(series_capacitance_pf) &&
         (series_capacitance_pf > 0.0f)) ? 1u : 0u;
    if (total_points > g_sweep_total_points[path])
    {
        g_sweep_total_points[path] =
            (total_points > UI_SWEEP_MAX_POINTS) ?
            UI_SWEEP_MAX_POINTS : total_points;
    }
    if (rebuild_interpolation != 0u)
    {
        UI_BuildSweepInterpolation(path);
    }

    if ((g_sweep_progress_label != NULL) &&
        (bias_text != NULL) && (capacitance_text != NULL))
    {
        lv_label_set_text_fmt(g_sweep_progress_label,
                              "%s  %lu/%lu   Vd=%s V   Cs=%s pF%s",
                              names[path],
                              (unsigned long)point_index,
                              (unsigned long)total_points,
                              bias_text, capacitance_text,
                              (valid != 0u) ? "" : "  无效");
    }
    if ((g_sweep_progress_bar != NULL) && (total_points != 0u))
    {
        uint32_t progress =
            (point_index >= total_points) ? 1000u :
            (point_index * 1000u) / total_points;

        UI_SetSweepProgress(progress);
    }
    UI_RequestSweepChartRefresh();
}

void UI_Demo_SetSweepInspectionReady(void)
{
    uint32_t path;

    for (path = 0u; path < 3u; ++path)
    {
        if (g_sweep_total_points[path] != 0u)
        {
            UI_BuildSweepInterpolation(path);
        }
    }
    UI_UpdateSweepAnalysisRange();
    g_sweep_inspection_ready = 1u;
    g_sweep_gesture.active = 0u;
    g_sweep_gesture.block_single_touch = 0u;
    UI_RebuildSweepChart();
    UI_UpdateSweepAnalysisCard();
    UI_ResultsRefresh();
    if (g_sweep_progress_label == NULL)
    {
        return;
    }
    if (g_sweep_probe_valid != 0u)
    {
        UI_UpdateSweepProbeLabel();
    }
    else
    {
        lv_label_set_text(
            g_sweep_progress_label,
            UI_IsSweepDataComplete() ?
            "扫压完成  结果汇总 > 扫压结果" :
            "当前回路完成  等待其他回路");
    }
}

/* 传输页数据入口:URL 由 cv_qr_live.c 生成。空 URL = 回到 DEMO 二维码。 */
void UI_Demo_SetTransferPayload(const char *url,
                                uint32_t acv1_nodes,
                                uint32_t scv1_points)
{
    size_t length = 0u;

    if (url != NULL)
    {
        length = strlen(url);
    }
    if ((url == NULL) || (length == 0u) ||
        (length >= sizeof(g_transfer_url)))
    {
        UI_Demo_ClearTransferPayload();
        return;
    }
    (void)memcpy(g_transfer_url, url, length + 1u);
    g_transfer_acv1_nodes = acv1_nodes;
    g_transfer_scv1_points = scv1_points;
    g_transfer_has_payload = 1u;
    g_transfer_qr_dirty = 1u;
    UI_UpdateTransferQRCode();
}

void UI_Demo_ClearTransferPayload(void)
{
    g_transfer_url[0] = '\0';
    g_transfer_acv1_nodes = 0u;
    g_transfer_scv1_points = 0u;
    g_transfer_has_payload = 0u;
    g_transfer_qr_dirty = 1u;
    UI_UpdateTransferQRCode();
}

uint32_t UI_Demo_CopySweepPath(uint32_t path,
                               float *voltage_v,
                               float *series_capacitance_pf,
                               uint8_t *valid,
                               uint32_t capacity)
{
    uint32_t count;

    if ((path >= UI_DEMO_SWEEP_PATH_COUNT) ||
        (voltage_v == NULL) ||
        (series_capacitance_pf == NULL) ||
        (valid == NULL) || (capacity == 0u))
    {
        return 0u;
    }
    count = g_sweep_total_points[path];
    if (count > capacity)
    {
        count = capacity;
    }
    if (count == 0u)
    {
        return 0u;
    }
    (void)memcpy(voltage_v, g_sweep_voltage[path],
                 count * sizeof(voltage_v[0]));
    (void)memcpy(series_capacitance_pf, g_sweep_capacitance[path],
                 count * sizeof(series_capacitance_pf[0]));
    (void)memcpy(valid, g_sweep_valid[path],
                 count * sizeof(valid[0]));
    return count;
}

uint32_t UI_Demo_GetLatestResistance(float *resistance_ohm)
{
    if ((resistance_ohm == NULL) || (g_latest_rg_valid == 0u) ||
        !isfinite(g_latest_rg_ohm) || (g_latest_rg_ohm <= 0.0f))
    {
        return 0u;
    }
    *resistance_ohm = g_latest_rg_ohm;
    return 1u;
}

void UI_Demo_GotoPage(uint32_t page)
{
    switch (page)
    {
        case (uint32_t)UI_PAGE_INITIALIZE:
            UI_ShowInitialize();
            break;

        case (uint32_t)UI_PAGE_MEASURE:
            UI_ShowMeasure();
            break;

        case (uint32_t)UI_PAGE_SWEEP:
            UI_ShowSweep();
            break;

        case (uint32_t)UI_PAGE_RESULTS:
            UI_ShowResults();
            break;

        case (uint32_t)UI_PAGE_HOME:
        default:
            UI_ShowHome();
            break;
    }
}

uint32_t UI_Demo_GetClickCount(void)
{
    return g_ui_click_count;
}

uint32_t UI_Demo_GetLastButton(void)
{
    return g_ui_last_button;
}

uint32_t UI_Demo_GetCurrentPage(void)
{
    return g_ui_current_page;
}

uint32_t UI_Demo_GetResetRequestCount(void)
{
    return g_ui_reset_request_count;
}

uint32_t UI_Demo_GetSinglePath(void)
{
    return g_ui_single_path;
}

uint32_t UI_Demo_GetSingleBiasTenths(void)
{
    return g_ui_single_bias_tenths;
}

uint32_t UI_Demo_GetSweepPath(void)
{
    return g_ui_sweep_path;
}

uint32_t UI_Demo_GetSweepEndTenths(void)
{
    return g_ui_sweep_end_tenths;
}
