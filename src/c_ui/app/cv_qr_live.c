#include "cv_qr_live.h"

#include <math.h>
#include <stdio.h>

#include "cv_qr_payload.h"
#include "measure_config.h"
#include "ui_demo.h"

#define CV_QR_LIVE_PATH_COUNT 3u
#define CV_QR_LIVE_POINTS     UI_DEMO_SWEEP_MAX_POINTS
/* 前序在 Rg 未知时回退 3.6 Ω;这里保持一致,但日志里会注明。 */
#define CV_QR_LIVE_RG_FALLBACK_OHM 3.6f
#define CV_QR_LIVE_EXCITATION_VPP  1.0f

static float g_cv_qr_live_voltage[CV_QR_LIVE_PATH_COUNT][CV_QR_LIVE_POINTS];
static float g_cv_qr_live_capacitance[CV_QR_LIVE_PATH_COUNT][CV_QR_LIVE_POINTS];
static uint8_t g_cv_qr_live_valid[CV_QR_LIVE_PATH_COUNT][CV_QR_LIVE_POINTS];
static uint32_t g_cv_qr_live_points[CV_QR_LIVE_PATH_COUNT];
static char g_cv_qr_live_url[CV_QR_PAYLOAD_URL_CAPACITY];

void CV_QR_Live_Reset(void)
{
    UI_Demo_ClearTransferPayload();
}

void CV_QR_Live_OnSweepComplete(void)
{
    const float *voltage_v[CV_QR_LIVE_PATH_COUNT];
    const float *capacitance_pf[CV_QR_LIVE_PATH_COUNT];
    const uint8_t *valid[CV_QR_LIVE_PATH_COUNT];
    CV_QR_PayloadInfo info;
    uint32_t frequency_hz;
    float rg_ohm;
    uint32_t path;

    for (path = 0u; path < CV_QR_LIVE_PATH_COUNT; ++path)
    {
        g_cv_qr_live_points[path] = UI_Demo_CopySweepPath(
            path,
            g_cv_qr_live_voltage[path],
            g_cv_qr_live_capacitance[path],
            g_cv_qr_live_valid[path],
            CV_QR_LIVE_POINTS);
        if (g_cv_qr_live_points[path] < 2u)
        {
            (void)printf("[qr] 跳过生成:路径 %lu 只有 %lu 点(需要三路完整)\n",
                         (unsigned long)path,
                         (unsigned long)g_cv_qr_live_points[path]);
            CV_QR_Live_Reset();
            return;
        }
        voltage_v[path] = g_cv_qr_live_voltage[path];
        capacitance_pf[path] = g_cv_qr_live_capacitance[path];
        valid[path] = g_cv_qr_live_valid[path];
    }

    frequency_hz = MeasureConfig_GetDdsHz();
    if ((UI_Demo_GetLatestResistance(&rg_ohm) == 0u) ||
        !isfinite(rg_ohm) || (rg_ohm <= 0.0f))
    {
        rg_ohm = CV_QR_LIVE_RG_FALLBACK_OHM;
        (void)printf("[qr] Rg 未知,回退 %.2f Ω(与前序一致)\n",
                     (double)rg_ohm);
    }

    if (!CV_QR_Payload_Build(voltage_v, capacitance_pf, valid,
                             g_cv_qr_live_points, frequency_hz,
                             CV_QR_LIVE_EXCITATION_VPP, rg_ohm,
                             g_cv_qr_live_url, sizeof(g_cv_qr_live_url),
                             &info))
    {
        (void)printf("[qr] 生成失败(路径点数 %lu/%lu/%lu, 频率 %lu Hz)\n",
                     (unsigned long)g_cv_qr_live_points[0],
                     (unsigned long)g_cv_qr_live_points[1],
                     (unsigned long)g_cv_qr_live_points[2],
                     (unsigned long)frequency_hz);
        CV_QR_Live_Reset();
        return;
    }

    UI_Demo_SetTransferPayload(g_cv_qr_live_url, info.acv1_nodes,
                               info.scv1_points);
    (void)printf("[qr] ACV1 %lu 节点 / SCV1 %lu 点 / URL %lu B / "
                 "CRC acv1=0x%08lx scv1=0x%08lx / %.4f-%.4f V\n",
                 (unsigned long)info.acv1_nodes,
                 (unsigned long)info.scv1_points,
                 (unsigned long)info.url_length,
                 (unsigned long)info.acv1_crc32,
                 (unsigned long)info.scv1_crc32,
                 (double)info.start_voltage_v,
                 (double)info.stop_voltage_v);
}
