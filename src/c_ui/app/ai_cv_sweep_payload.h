#ifndef QRS_AI_CV_SWEEP_PAYLOAD_H
#define QRS_AI_CV_SWEEP_PAYLOAD_H

/* SCV1 载荷编码器 —— 逐字移植自前序工程
 * `~/Desktop/qrs/LCD/XIP_Appli/Core/Inc|Src/ai_cv_sweep_payload.{h,c}`,
 * 仅改了 include guard。解码端是网页 `js/sweep-codec.js`,
 * 格式(尤其隐式电压网格)必须与其完全一致。
 * 使用方:`app/cv_qr_payload.c`。 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define AI_CV_SWEEP_PAYLOAD_PATH_COUNT 3u
#define AI_CV_SWEEP_PAYLOAD_MAX_POINTS 201u
#define AI_CV_SWEEP_PAYLOAD_BITS_PER_VALUE 12u
#define AI_CV_SWEEP_PAYLOAD_MAX_CURVE_BYTES \
    ((AI_CV_SWEEP_PAYLOAD_MAX_POINTS * \
      AI_CV_SWEEP_PAYLOAD_BITS_PER_VALUE + 7u) / 8u)
#define AI_CV_SWEEP_PAYLOAD_MAX_VALID_BYTES \
    ((AI_CV_SWEEP_PAYLOAD_MAX_POINTS + 7u) / 8u)
#define AI_CV_SWEEP_PAYLOAD_MAX_BINARY_SIZE \
    (44u + AI_CV_SWEEP_PAYLOAD_PATH_COUNT * \
     (AI_CV_SWEEP_PAYLOAD_MAX_CURVE_BYTES + \
      AI_CV_SWEEP_PAYLOAD_MAX_VALID_BYTES))
#define AI_CV_SWEEP_PAYLOAD_MAX_TEXT_SIZE \
    (((AI_CV_SWEEP_PAYLOAD_MAX_BINARY_SIZE + 2u) / 3u) * 4u)

typedef enum
{
    AI_CV_SWEEP_PAYLOAD_OK = 0u,
    AI_CV_SWEEP_PAYLOAD_ERROR_ARGUMENT = 1u,
    AI_CV_SWEEP_PAYLOAD_ERROR_POINT_COUNT = 2u,
    AI_CV_SWEEP_PAYLOAD_ERROR_VOLTAGE = 3u,
    AI_CV_SWEEP_PAYLOAD_ERROR_NO_VALID_CURVE = 4u,
    AI_CV_SWEEP_PAYLOAD_ERROR_BUFFER = 5u
} AI_CV_SWEEP_PAYLOAD_Status;

AI_CV_SWEEP_PAYLOAD_Status AI_CV_SWEEP_PAYLOAD_Encode(
    const float *voltage_v,
    const float *capacitance_pf[AI_CV_SWEEP_PAYLOAD_PATH_COUNT],
    const uint8_t *valid[AI_CV_SWEEP_PAYLOAD_PATH_COUNT],
    uint32_t point_count,
    char *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32);

#ifdef __cplusplus
}
#endif

#endif
