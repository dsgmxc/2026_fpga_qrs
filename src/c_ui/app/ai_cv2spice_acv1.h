#ifndef QRS_AI_CV2SPICE_ACV1_H
#define QRS_AI_CV2SPICE_ACV1_H

/* ACV1 载荷编码器 —— 逐字移植自前序工程
 * `~/Desktop/qrs/LCD/XIP_Appli/Core/Inc|Src/ai_cv2spice_acv1.{h,c}`,
 * 仅改了 include guard。协议细节必须与网页 `js/model-codec.js` 保持一致,
 * 所以这里**不要**顺手改格式或舍入方式。
 * 使用方:`app/cv_qr_payload.c`。 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define AI_CV2SPICE_ACV1_PROTOCOL_VERSION 1u
#define AI_CV2SPICE_ACV1_FLAG_AI_GENERATED (1u << 0)
#define AI_CV2SPICE_ACV1_FLAG_SINGLE_DEVICE_OVERFIT (1u << 1)
#define AI_CV2SPICE_ACV1_FLAG_MEASURED_PHYSICAL (1u << 2)
#define AI_CV2SPICE_ACV1_COMPONENT_COUNT 3u
#define AI_CV2SPICE_ACV1_MIN_NODE_COUNT  2u
#define AI_CV2SPICE_ACV1_MAX_NODE_COUNT  32u
#define AI_CV2SPICE_ACV1_MAX_BINARY_SIZE \
    (24u + 3u * 255u + 8u * AI_CV2SPICE_ACV1_MAX_NODE_COUNT)
#define AI_CV2SPICE_ACV1_MAX_PAYLOAD_SIZE \
    (((AI_CV2SPICE_ACV1_MAX_BINARY_SIZE + 2u) / 3u) * 4u)

typedef enum
{
    AI_CV2SPICE_ACV1_OK = 0u,
    AI_CV2SPICE_ACV1_ERROR_ARGUMENT = 1u,
    AI_CV2SPICE_ACV1_ERROR_METADATA = 2u,
    AI_CV2SPICE_ACV1_ERROR_NODE_COUNT = 3u,
    AI_CV2SPICE_ACV1_ERROR_VOLTAGE_NODE = 4u,
    AI_CV2SPICE_ACV1_ERROR_DELTA_CHARGE = 5u,
    AI_CV2SPICE_ACV1_ERROR_FLOAT16_RANGE = 6u,
    AI_CV2SPICE_ACV1_ERROR_BUFFER = 7u
} AI_CV2SPICE_ACV1_Status;

typedef struct
{
    uint8_t flags;
    const char *device_id;
    const char *part_number;
    const char *manufacturer;
    uint32_t frequency_hz;
    float excitation_vpp;
    float rg_ohm;
    uint32_t node_count;
    const float *voltage_nodes_v;
    const float *delta_charge_nc[AI_CV2SPICE_ACV1_COMPONENT_COUNT];
} AI_CV2SPICE_ACV1_Model;

AI_CV2SPICE_ACV1_Status AI_CV2SPICE_ACV1_EncodeBinary(
    const AI_CV2SPICE_ACV1_Model *model,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32);

AI_CV2SPICE_ACV1_Status AI_CV2SPICE_ACV1_EncodePayload(
    const AI_CV2SPICE_ACV1_Model *model,
    char *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32);

uint32_t AI_CV2SPICE_ACV1_Crc32(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
