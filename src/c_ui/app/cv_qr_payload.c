/*
 * 实测扫压数据 → 网页可解码二维码 URL(ACV1 + SCV1)。
 *
 * 协议与物理积分口径全部沿用前序工程 `~/Desktop/qrs`:
 *   - 载荷编码:`ai_cv2spice_acv1.c` / `ai_cv_sweep_payload.c`(逐字移植)
 *   - 电荷积分:`ai_cv2spice_physics.c`(分量映射 + 梯形积分 + 0V→首点恒定)
 *   - 隐式电压网格:`AI_Model_Web/js/sweep-codec.js::reconstructVoltageGrid`
 *
 * ⚠️ 与网页解码器的两个硬约束(改了就对不上,别顺手"优化"):
 *   1. SCV1 只存首/末电压,中间点由解码器按固定规则重建 —— C 侧必须生成
 *      **同一组浮点值**,而不是按自己的扫压点集直接编码。
 *   2. start/stop 先按 float32 取整再参与网格生成,保证解码器读到的值
 *      与 C 侧生成时逐位一致。
 */

#include "cv_qr_payload.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai_cv2spice_acv1.h"
#include "ai_cv_sweep_payload.h"
#include "sweep_interpolation.h"

#define CV_QR_PATH_COUNT 3u
#define CV_QR_BASE_URL "https://fox4327.github.io/ai-cv2spice-web/"
#define CV_QR_MINIMUM_COMPONENT_PF 0.001f
/* 前序用 1e-8 nC,但该值在 Float16 下舍入为 0 → 编码器判为非法。
 * 1e-6 nC 是 Float16 次正规数可表示的最小安全值(≈17 ulp)。 */
#define CV_QR_MINIMUM_DELTA_NC 1.0e-6f
#define CV_QR_GRID_MAX_POINTS 201u
#define CV_QR_ACV1_MAX_NODES 13u
#define CV_QR_INTEGRATION_MAX_POINTS (SWEEP_INTERPOLATION_MAX_POINTS + 2u)

static SweepInterpolationCurve g_cv_qr_curves[CV_QR_PATH_COUNT];
static float g_cv_qr_sample_v[CV_QR_INTEGRATION_MAX_POINTS];
static float g_cv_qr_component_pf[CV_QR_PATH_COUNT]
                                [CV_QR_INTEGRATION_MAX_POINTS];
static float g_cv_qr_node_v[CV_QR_ACV1_MAX_NODES];
static float g_cv_qr_delta_nc[CV_QR_PATH_COUNT][CV_QR_ACV1_MAX_NODES];
static float g_cv_qr_grid_v[CV_QR_GRID_MAX_POINTS];
static float g_cv_qr_grid_cap[CV_QR_PATH_COUNT][CV_QR_GRID_MAX_POINTS];
static uint8_t g_cv_qr_grid_valid[CV_QR_PATH_COUNT][CV_QR_GRID_MAX_POINTS];
static char g_cv_qr_acv1[AI_CV2SPICE_ACV1_MAX_PAYLOAD_SIZE];
static char g_cv_qr_scv1[AI_CV_SWEEP_PAYLOAD_MAX_TEXT_SIZE];

/* 前序 12 个标准 ACV1 电压节点。 */
static const float g_cv_qr_standard_nodes[] =
{
    0.1f, 0.494f, 1.176f, 2.379f, 4.559f, 7.627f,
    10.0f, 13.11f, 20.78f, 33.84f, 56.62f, 100.0f
};

static float CV_QR_Maximum(float lhs, float rhs)
{
    return (lhs > rhs) ? lhs : rhs;
}

static float CV_QR_Minimum(float lhs, float rhs)
{
    return (lhs < rhs) ? lhs : rhs;
}

/* 与 `ai_cv2spice_physics.c::AI_CV2SPICE_PhysicsIntegrateInterval` 同构:
 * 在 [left, right] 上对分段线性电容做梯形积分;第一个采样点以下按首值恒定。 */
static float CV_QR_IntegrateInterval(const float *component_pf,
                                     uint32_t sample_count,
                                     float interval_left_v,
                                     float interval_right_v)
{
    float area_pf_v = 0.0f;
    float active_left_v = interval_left_v;
    uint32_t sample;

    if ((component_pf == NULL) || (sample_count == 0u) ||
        !(interval_right_v > interval_left_v))
    {
        return 0.0f;
    }
    if (active_left_v < g_cv_qr_sample_v[0])
    {
        const float constant_right_v = CV_QR_Minimum(
            interval_right_v, g_cv_qr_sample_v[0]);

        if (constant_right_v > active_left_v)
        {
            area_pf_v += (constant_right_v - active_left_v) *
                         component_pf[0];
            active_left_v = constant_right_v;
        }
    }

    for (sample = 0u; sample + 1u < sample_count; ++sample)
    {
        const float segment_left_v = g_cv_qr_sample_v[sample];
        const float segment_right_v = g_cv_qr_sample_v[sample + 1u];
        const float overlap_left_v = CV_QR_Maximum(active_left_v,
                                                   segment_left_v);
        const float overlap_right_v = CV_QR_Minimum(interval_right_v,
                                                    segment_right_v);

        if (overlap_right_v > overlap_left_v)
        {
            const float inverse_width =
                1.0f / (segment_right_v - segment_left_v);
            const float slope =
                (component_pf[sample + 1u] - component_pf[sample]) *
                inverse_width;
            const float left_capacitance = component_pf[sample] +
                slope * (overlap_left_v - segment_left_v);
            const float right_capacitance = component_pf[sample] +
                slope * (overlap_right_v - segment_left_v);

            area_pf_v += 0.5f * (left_capacitance + right_capacitance) *
                         (overlap_right_v - overlap_left_v);
        }
        if (segment_right_v >= interval_right_v)
        {
            break;
        }
    }
    return area_pf_v;
}

/* 生成 ACV1 节点:标准节点截到 stop,末项不是 stop 时补上。 */
static uint32_t CV_QR_BuildAcv1Nodes(float stop_v)
{
    uint32_t count = 0u;
    size_t index;

    for (index = 0u;
         index < (sizeof(g_cv_qr_standard_nodes) /
                  sizeof(g_cv_qr_standard_nodes[0]));
         ++index)
    {
        if (g_cv_qr_standard_nodes[index] <= stop_v)
        {
            g_cv_qr_node_v[count++] = g_cv_qr_standard_nodes[index];
        }
    }
    if ((count == 0u) ||
        (fabsf(g_cv_qr_node_v[count - 1u] - stop_v) > 1.0e-6f))
    {
        g_cv_qr_node_v[count++] = stop_v;
    }
    return count;
}

/* 复用 `sweep-codec.js::reconstructVoltageGrid` 的规则。
 * 必须在 double 下算,再转 float —— 解码器是在 double 下重建的。 */
static uint32_t CV_QR_BuildWebGrid(float start_v, float stop_v,
                                   float *grid, uint32_t capacity)
{
    double generated[CV_QR_GRID_MAX_POINTS];
    const double start = (double)start_v;
    const double stop = (double)stop_v;
    const double low_step = 20.0 / 60.0;
    uint32_t count = 0u;
    int index;

    if ((grid == NULL) || !(stop > start) || (stop <= 0.0))
    {
        return 0u;
    }
    if (capacity < 2u)
    {
        return 0u;
    }

    generated[count++] = 0.0;
    for (index = 1; index <= 60; ++index)
    {
        const double voltage = index * low_step;

        if (voltage <= (stop + 1.0e-5))
        {
            if (count >= capacity)
            {
                return 0u;
            }
            generated[count++] = voltage;
        }
    }
    if (stop > 20.0)
    {
        double voltage;

        for (voltage = 21.0; voltage <= floor(stop + 1.0e-5);
             voltage += 1.0)
        {
            if (count >= capacity)
            {
                return 0u;
            }
            generated[count++] = voltage;
        }
    }
    if (fabs(generated[count - 1u] - stop) > 1.0e-4)
    {
        if (count >= capacity)
        {
            return 0u;
        }
        generated[count++] = stop;
    }

    generated[0] = (start > 0.1) ? start : 0.1;
    for (index = 0; index < (int)count; ++index)
    {
        grid[index] = (float)generated[index];
    }
    return count;
}

/* 取三路中点数最多的一路偏压数组,截到公共区间并补端点。 */
static uint32_t CV_QR_BuildIntegrationSamples(
    const float *const voltage_v[CV_QR_PATH_COUNT],
    const uint32_t point_count[CV_QR_PATH_COUNT],
    float start_v, float stop_v)
{
    uint32_t source = 0u;
    uint32_t path;
    uint32_t point;
    uint32_t count = 0u;

    for (path = 1u; path < CV_QR_PATH_COUNT; ++path)
    {
        if (point_count[path] > point_count[source])
        {
            source = path;
        }
    }

    if (start_v < stop_v)
    {
        g_cv_qr_sample_v[count++] = start_v;
    }
    for (point = 0u; point < point_count[source]; ++point)
    {
        const float voltage = voltage_v[source][point];

        if (!isfinite(voltage) || (voltage <= start_v) ||
            (voltage >= stop_v))
        {
            continue;
        }
        if (count >= CV_QR_INTEGRATION_MAX_POINTS)
        {
            return 0u;
        }
        g_cv_qr_sample_v[count++] = voltage;
    }
    if ((count == 0u) ||
        (fabsf(g_cv_qr_sample_v[count - 1u] - stop_v) > 1.0e-6f))
    {
        if (count >= CV_QR_INTEGRATION_MAX_POINTS)
        {
            return 0u;
        }
        g_cv_qr_sample_v[count++] = stop_v;
    }
    return (count >= 2u) ? count : 0u;
}

bool CV_QR_Payload_Build(
    const float *const voltage_v[3],
    const float *const capacitance_pf[3],
    const uint8_t *const valid[3],
    const uint32_t point_count[3],
    uint32_t frequency_hz, float vpp, float rg_ohm,
    char *url, size_t url_capacity,
    CV_QR_PayloadInfo *info)
{
    float minimum_log_voltage = -INFINITY;
    float maximum_log_voltage = INFINITY;
    float start_v;
    float stop_v;
    uint32_t path;
    uint32_t sample_count;
    uint32_t node_count;
    uint32_t grid_count = 0u;
    size_t acv1_length = 0u;
    uint32_t acv1_crc32 = 0u;
    size_t scv1_length = 0u;
    uint32_t scv1_crc32 = 0u;
    int written;
    size_t url_length;
    AI_CV2SPICE_ACV1_Model model;
    AI_CV2SPICE_ACV1_Status acv1_status;
    AI_CV_SWEEP_PAYLOAD_Status scv1_status = AI_CV_SWEEP_PAYLOAD_ERROR_ARGUMENT;

    if ((voltage_v == NULL) || (capacitance_pf == NULL) ||
        (valid == NULL) || (point_count == NULL) || (url == NULL) ||
        (url_capacity == 0u) || (frequency_hz == 0u))
    {
        return false;
    }
    url[0] = '\0';

    for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
    {
        float curve_min_log;
        float curve_max_log;

        if ((voltage_v[path] == NULL) || (capacitance_pf[path] == NULL) ||
            (valid[path] == NULL) || (point_count[path] < 2u) ||
            (point_count[path] > SWEEP_INTERPOLATION_MAX_POINTS))
        {
            return false;
        }
        (void)SweepInterpolation_Build(&g_cv_qr_curves[path],
                                       voltage_v[path],
                                       capacitance_pf[path],
                                       valid[path],
                                       point_count[path], true);
        if (!SweepInterpolation_GetLogVoltageRange(&g_cv_qr_curves[path],
                                                   &curve_min_log,
                                                   &curve_max_log))
        {
            return false;
        }
        if (curve_min_log > minimum_log_voltage)
        {
            minimum_log_voltage = curve_min_log;
        }
        if (curve_max_log < maximum_log_voltage)
        {
            maximum_log_voltage = curve_max_log;
        }
    }
    if (!(maximum_log_voltage > minimum_log_voltage))
    {
        return false;
    }

    /* 先转 float32 再参与后续生成:网页从载荷里读到的就是这两个值。 */
    start_v = powf(10.0f, minimum_log_voltage);
    stop_v = powf(10.0f, maximum_log_voltage);
    if (!isfinite(start_v) || !isfinite(stop_v) ||
        !(stop_v > start_v) || (start_v <= 0.0f) ||
        (stop_v > 1.0e6f) || (frequency_hz == 0u) ||
        !isfinite(vpp) || (vpp <= 0.0f) ||
        !isfinite(rg_ohm) || (rg_ohm <= 0.0f))
    {
        return false;
    }

    /* ---- ACV1:节点 + 电荷增量 ------------------------------------- */
    node_count = CV_QR_BuildAcv1Nodes(stop_v);
    if ((node_count < AI_CV2SPICE_ACV1_MIN_NODE_COUNT) ||
        (node_count > CV_QR_ACV1_MAX_NODES))
    {
        return false;
    }
    sample_count = CV_QR_BuildIntegrationSamples(voltage_v, point_count,
                                                 start_v, stop_v);
    if (sample_count == 0u)
    {
        return false;
    }
    for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
    {
        uint32_t sample;

        for (sample = 0u; sample < sample_count; ++sample)
        {
            float capacitance;

            if (!SweepInterpolation_Evaluate(&g_cv_qr_curves[path],
                                             g_cv_qr_sample_v[sample],
                                             &capacitance) ||
                !isfinite(capacitance) || (capacitance <= 0.0f))
            {
                return false;
            }
            g_cv_qr_component_pf[path][sample] = capacitance;
        }
    }
    {
        uint32_t sample;

        for (sample = 0u; sample < sample_count; ++sample)
        {
            const float ciss = g_cv_qr_component_pf[0][sample];
            const float coss = g_cv_qr_component_pf[1][sample];
            const float crss = g_cv_qr_component_pf[2][sample];

            /* 前序映射:[0]=Ciss−Crss, [1]=Crss, [2]=Coss−Crss。 */
            g_cv_qr_component_pf[0][sample] =
                CV_QR_Maximum(ciss - crss, CV_QR_MINIMUM_COMPONENT_PF);
            g_cv_qr_component_pf[1][sample] =
                CV_QR_Maximum(crss, CV_QR_MINIMUM_COMPONENT_PF);
            g_cv_qr_component_pf[2][sample] =
                CV_QR_Maximum(coss - crss, CV_QR_MINIMUM_COMPONENT_PF);
        }
    }
    {
        float previous_node_v = 0.0f;
        uint32_t node;

        for (node = 0u; node < node_count; ++node)
        {
            const float node_v = g_cv_qr_node_v[node];

            if (!(node_v > previous_node_v))
            {
                return false;
            }
            for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
            {
                float delta_nc = CV_QR_IntegrateInterval(
                    g_cv_qr_component_pf[path], sample_count,
                    previous_node_v, node_v) / 1000.0f;

                if (!isfinite(delta_nc) || (delta_nc < 0.0f))
                {
                    return false;
                }
                if (delta_nc < CV_QR_MINIMUM_DELTA_NC)
                {
                    delta_nc = CV_QR_MINIMUM_DELTA_NC;
                }
                g_cv_qr_delta_nc[path][node] = delta_nc;
            }
            previous_node_v = node_v;
        }
    }

    (void)memset(&model, 0, sizeof(model));
    model.flags = (uint8_t)AI_CV2SPICE_ACV1_FLAG_MEASURED_PHYSICAL;
    model.device_id = "LIVE";
    model.part_number = "MEASURED_MOSFET";
    model.manufacturer = "Measured";
    model.frequency_hz = frequency_hz;
    model.excitation_vpp = vpp;
    model.rg_ohm = rg_ohm;
    model.node_count = node_count;
    model.voltage_nodes_v = g_cv_qr_node_v;
    for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
    {
        model.delta_charge_nc[path] = g_cv_qr_delta_nc[path];
    }
    acv1_status = AI_CV2SPICE_ACV1_EncodePayload(
        &model, g_cv_qr_acv1, sizeof(g_cv_qr_acv1),
        &acv1_length, &acv1_crc32);
    if (acv1_status != AI_CV2SPICE_ACV1_OK)
    {
        return false;
    }

    /* ---- SCV1:重采样到网页隐式网格 ------------------------------- */
    grid_count = CV_QR_BuildWebGrid(start_v, stop_v, g_cv_qr_grid_v,
                                    CV_QR_GRID_MAX_POINTS);
    if (grid_count >= 2u)
    {
        for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
        {
            uint32_t point;
            bool any_valid = false;

            for (point = 0u; point < grid_count; ++point)
            {
                float capacitance = 0.0f;

                g_cv_qr_grid_valid[path][point] =
                    (SweepInterpolation_Evaluate(&g_cv_qr_curves[path],
                                                 g_cv_qr_grid_v[point],
                                                 &capacitance) &&
                     isfinite(capacitance) && (capacitance > 0.0f)) ? 1u : 0u;
                g_cv_qr_grid_cap[path][point] = capacitance;
                if (g_cv_qr_grid_valid[path][point] != 0u)
                {
                    any_valid = true;
                }
            }
            if (!any_valid)
            {
                grid_count = 0u;
                break;
            }
        }
    }
    if (grid_count >= 2u)
    {
        const float *capacity_curves[AI_CV_SWEEP_PAYLOAD_PATH_COUNT];
        const uint8_t *valid_masks[AI_CV_SWEEP_PAYLOAD_PATH_COUNT];

        for (path = 0u; path < CV_QR_PATH_COUNT; ++path)
        {
            capacity_curves[path] = g_cv_qr_grid_cap[path];
            valid_masks[path] = g_cv_qr_grid_valid[path];
        }
        scv1_status = AI_CV_SWEEP_PAYLOAD_Encode(
            g_cv_qr_grid_v, capacity_curves, valid_masks,
            grid_count, g_cv_qr_scv1, sizeof(g_cv_qr_scv1),
            &scv1_length, &scv1_crc32);
    }
    if (scv1_status != AI_CV_SWEEP_PAYLOAD_OK)
    {
        grid_count = 0u;
        scv1_length = 0u;
        scv1_crc32 = 0u;
    }

    /* ---- URL ------------------------------------------------------- */
    if (scv1_length != 0u)
    {
        written = snprintf(url, url_capacity, CV_QR_BASE_URL "#m=%s&c=%s",
                           g_cv_qr_acv1, g_cv_qr_scv1);
    }
    else
    {
        written = snprintf(url, url_capacity, CV_QR_BASE_URL "#m=%s",
                           g_cv_qr_acv1);
    }
    if ((written < 0) || ((size_t)written >= url_capacity))
    {
        url[0] = '\0';
        return false;
    }
    url_length = (size_t)written;

    if (info != NULL)
    {
        (void)memset(info, 0, sizeof(*info));
        info->acv1_nodes = node_count;
        info->scv1_points = grid_count;
        info->url_length = (uint32_t)url_length;
        info->acv1_crc32 = acv1_crc32;
        info->scv1_crc32 = scv1_crc32;
        info->scv1_present = (scv1_length != 0u);
        info->start_voltage_v = start_v;
        info->stop_voltage_v = stop_v;
    }
    return true;
}
