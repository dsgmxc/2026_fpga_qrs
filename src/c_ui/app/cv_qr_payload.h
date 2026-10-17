#ifndef QRS_CV_QR_PAYLOAD_H
#define QRS_CV_QR_PAYLOAD_H

/*
 * 实测扫压数据 → 前序网页可解码的二维码 URL。
 *
 * 输出形式与 `~/Desktop/qrs` 前序工程完全一致:
 *
 *   https://fox4327.github.io/ai-cv2spice-web/#m=<ACV1>&c=<SCV1>
 *
 *   - ACV1:LTspice 建模载荷(12~13 个电压节点 + Qgs/Qgd/Qds 电荷增量,
 *           Float16、CRC32/ISO-HDLC、Base64URL)。由
 *           `ai_cv2spice_acv1.c` 编码(前序逐字移植)。
 *   - SCV1:完整 C-V 曲线载荷(重采样到网页解码器隐式重建的电压网格,
 *           12-bit 量化、CRC32)。由 `ai_cv_sweep_payload.c` 编码。
 *
 * ⚠️ 本模块是**纯逻辑**,不碰 LVGL / 寄存器,可以在宿主上单测。
 * 静态工作区 ⇒ 不重入;只在 UI 主循环里调用。
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 实测最大约 1.3 kB;留足余量,避免节点数上限时溢出。 */
#define CV_QR_PAYLOAD_URL_CAPACITY 3072u

typedef struct
{
    uint32_t acv1_nodes;      /* ACV1 电压节点数 */
    uint32_t scv1_points;     /* SCV1 重采样点数(网页隐式网格长度) */
    uint32_t url_length;      /* 写入 url 的字符数(不含结尾 NUL) */
    uint32_t acv1_crc32;
    uint32_t scv1_crc32;
    bool scv1_present;        /* false ⇒ URL 只有 #m=,网页仍向后兼容 */
    float start_voltage_v;    /* 三条曲线公共有效区间(已按 float32 取整) */
    float stop_voltage_v;
} CV_QR_PayloadInfo;

/*
 * @param voltage_v       [3] 三条曲线的偏压数组(各自独立)
 * @param capacitance_pf  [3] CISS / COSS / CRSS(单位 pF)
 * @param valid           [3] 逐点有效标志
 * @param point_count     [3] 各路点数(2..201)
 * @param frequency_hz    激励频率(写入 ACV1)
 * @param vpp             激励峰峰值(V,写入 ACV1)
 * @param rg_ohm          栅极电阻(Ω,写入 ACV1)
 * @param url             输出缓冲
 * @param url_capacity    输出缓冲容量(建议 CV_QR_PAYLOAD_URL_CAPACITY)
 * @param info            可为 NULL
 * @return 至少 ACV1 编码成功返回 true;SCV1 失败时退化为只有 #m=。
 */
bool CV_QR_Payload_Build(
    const float *const voltage_v[3],
    const float *const capacitance_pf[3],
    const uint8_t *const valid[3],
    const uint32_t point_count[3],
    uint32_t frequency_hz, float vpp, float rg_ohm,
    char *url, size_t url_capacity,
    CV_QR_PayloadInfo *info);

#ifdef __cplusplus
}
#endif

#endif
