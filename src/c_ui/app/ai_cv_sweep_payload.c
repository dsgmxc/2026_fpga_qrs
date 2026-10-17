/* SCV1 载荷编码器 —— 逐字移植自前序工程
 * `~/Desktop/qrs/LCD/XIP_Appli/Core/Src/ai_cv_sweep_payload.c`。 */

#include "ai_cv_sweep_payload.h"

#include <math.h>
#include <string.h>

#include "ai_cv2spice_acv1.h"

static uint8_t g_ai_cv_sweep_binary[AI_CV_SWEEP_PAYLOAD_MAX_BINARY_SIZE];

static void AI_CV_SWEEP_PutUint16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void AI_CV_SWEEP_PutUint32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
    destination[2] = (uint8_t)((value >> 16) & 0xffu);
    destination[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void AI_CV_SWEEP_PutFloat32(uint8_t *destination, float value)
{
    uint32_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    AI_CV_SWEEP_PutUint32(destination, bits);
}

static void AI_CV_SWEEP_Put12(uint8_t *destination,
                              uint32_t bit_offset,
                              uint16_t value)
{
    uint32_t bit;

    for (bit = 0u; bit < AI_CV_SWEEP_PAYLOAD_BITS_PER_VALUE; ++bit)
    {
        if ((value & (uint16_t)(1u << bit)) != 0u)
        {
            uint32_t output_bit = bit_offset + bit;
            destination[output_bit >> 3] |=
                (uint8_t)(1u << (output_bit & 7u));
        }
    }
}

static size_t AI_CV_SWEEP_Base64Url(const uint8_t *input,
                                    size_t input_length,
                                    char *output,
                                    size_t output_capacity)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t input_index = 0u;
    size_t output_index = 0u;

    while ((input_index + 3u) <= input_length)
    {
        uint32_t value = ((uint32_t)input[input_index] << 16) |
                         ((uint32_t)input[input_index + 1u] << 8) |
                         input[input_index + 2u];

        if ((output_index + 4u) >= output_capacity)
        {
            return 0u;
        }
        output[output_index++] = alphabet[(value >> 18) & 0x3fu];
        output[output_index++] = alphabet[(value >> 12) & 0x3fu];
        output[output_index++] = alphabet[(value >> 6) & 0x3fu];
        output[output_index++] = alphabet[value & 0x3fu];
        input_index += 3u;
    }
    if (input_index < input_length)
    {
        uint32_t value = (uint32_t)input[input_index] << 16;
        size_t remaining = input_length - input_index;

        if (remaining == 2u)
        {
            value |= (uint32_t)input[input_index + 1u] << 8;
        }
        if ((output_index + ((remaining == 2u) ? 3u : 2u)) >=
            output_capacity)
        {
            return 0u;
        }
        output[output_index++] = alphabet[(value >> 18) & 0x3fu];
        output[output_index++] = alphabet[(value >> 12) & 0x3fu];
        if (remaining == 2u)
        {
            output[output_index++] = alphabet[(value >> 6) & 0x3fu];
        }
    }
    output[output_index] = '\0';
    return output_index;
}

AI_CV_SWEEP_PAYLOAD_Status AI_CV_SWEEP_PAYLOAD_Encode(
    const float *voltage_v,
    const float *capacitance_pf[AI_CV_SWEEP_PAYLOAD_PATH_COUNT],
    const uint8_t *valid[AI_CV_SWEEP_PAYLOAD_PATH_COUNT],
    uint32_t point_count,
    char *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32)
{
    static const uint8_t magic[4] = {0x53u, 0x43u, 0x56u, 0x31u};
    float minimum_log[AI_CV_SWEEP_PAYLOAD_PATH_COUNT];
    float maximum_log[AI_CV_SWEEP_PAYLOAD_PATH_COUNT];
    uint32_t valid_count[AI_CV_SWEEP_PAYLOAD_PATH_COUNT] = {0u, 0u, 0u};
    size_t curve_bytes;
    size_t valid_bytes;
    size_t binary_length;
    size_t offset;
    uint32_t path;
    uint32_t point;
    uint32_t crc;
    size_t payload_length;

    if ((voltage_v == NULL) || (capacitance_pf == NULL) ||
        (valid == NULL) || (output == NULL) || (output_length == NULL))
    {
        return AI_CV_SWEEP_PAYLOAD_ERROR_ARGUMENT;
    }
    if ((point_count < 2u) ||
        (point_count > AI_CV_SWEEP_PAYLOAD_MAX_POINTS))
    {
        return AI_CV_SWEEP_PAYLOAD_ERROR_POINT_COUNT;
    }
    if (!isfinite(voltage_v[0]) ||
        !isfinite(voltage_v[point_count - 1u]) ||
        (voltage_v[0] < 0.0f) ||
        (voltage_v[point_count - 1u] <= voltage_v[0]))
    {
        return AI_CV_SWEEP_PAYLOAD_ERROR_VOLTAGE;
    }

    for (path = 0u; path < AI_CV_SWEEP_PAYLOAD_PATH_COUNT; ++path)
    {
        if ((capacitance_pf[path] == NULL) || (valid[path] == NULL))
        {
            return AI_CV_SWEEP_PAYLOAD_ERROR_ARGUMENT;
        }
        minimum_log[path] = 0.0f;
        maximum_log[path] = 0.0f;
        for (point = 0u; point < point_count; ++point)
        {
            float capacitance = capacitance_pf[path][point];

            if ((valid[path][point] != 0u) && isfinite(capacitance) &&
                (capacitance > 0.0f))
            {
                float log_value = log10f(capacitance);

                if (valid_count[path] == 0u)
                {
                    minimum_log[path] = log_value;
                    maximum_log[path] = log_value;
                }
                else
                {
                    if (log_value < minimum_log[path])
                    {
                        minimum_log[path] = log_value;
                    }
                    if (log_value > maximum_log[path])
                    {
                        maximum_log[path] = log_value;
                    }
                }
                ++valid_count[path];
            }
        }
        if (valid_count[path] == 0u)
        {
            return AI_CV_SWEEP_PAYLOAD_ERROR_NO_VALID_CURVE;
        }
    }

    curve_bytes = (point_count * AI_CV_SWEEP_PAYLOAD_BITS_PER_VALUE + 7u) / 8u;
    valid_bytes = (point_count + 7u) / 8u;
    binary_length = 40u + AI_CV_SWEEP_PAYLOAD_PATH_COUNT *
                    (curve_bytes + valid_bytes) + 4u;
    if (binary_length > sizeof(g_ai_cv_sweep_binary))
    {
        return AI_CV_SWEEP_PAYLOAD_ERROR_BUFFER;
    }
    (void)memset(g_ai_cv_sweep_binary, 0, binary_length);
    offset = 0u;
    (void)memcpy(&g_ai_cv_sweep_binary[offset], magic, sizeof(magic));
    offset += sizeof(magic);
    g_ai_cv_sweep_binary[offset++] = 1u;
    g_ai_cv_sweep_binary[offset++] = AI_CV_SWEEP_PAYLOAD_PATH_COUNT;
    AI_CV_SWEEP_PutUint16(&g_ai_cv_sweep_binary[offset],
                          (uint16_t)point_count);
    offset += 2u;
    AI_CV_SWEEP_PutFloat32(&g_ai_cv_sweep_binary[offset], voltage_v[0]);
    offset += 4u;
    AI_CV_SWEEP_PutFloat32(&g_ai_cv_sweep_binary[offset],
                           voltage_v[point_count - 1u]);
    offset += 4u;
    for (path = 0u; path < AI_CV_SWEEP_PAYLOAD_PATH_COUNT; ++path)
    {
        AI_CV_SWEEP_PutFloat32(&g_ai_cv_sweep_binary[offset],
                               minimum_log[path]);
        offset += 4u;
        AI_CV_SWEEP_PutFloat32(&g_ai_cv_sweep_binary[offset],
                               maximum_log[path]);
        offset += 4u;
    }

    for (path = 0u; path < AI_CV_SWEEP_PAYLOAD_PATH_COUNT; ++path)
    {
        uint8_t *packed = &g_ai_cv_sweep_binary[offset];
        float span = maximum_log[path] - minimum_log[path];

        for (point = 0u; point < point_count; ++point)
        {
            float capacitance = capacitance_pf[path][point];
            uint16_t quantized = 0u;

            if ((valid[path][point] != 0u) && isfinite(capacitance) &&
                (capacitance > 0.0f) && (span > 0.0f))
            {
                float normalized =
                    (log10f(capacitance) - minimum_log[path]) / span;
                float scaled = fminf(fmaxf(normalized, 0.0f), 1.0f) *
                               4095.0f;
                quantized = (uint16_t)floorf(scaled + 0.5f);
            }
            AI_CV_SWEEP_Put12(packed,
                              point * AI_CV_SWEEP_PAYLOAD_BITS_PER_VALUE,
                              quantized);
        }
        offset += curve_bytes;
    }
    for (path = 0u; path < AI_CV_SWEEP_PAYLOAD_PATH_COUNT; ++path)
    {
        uint8_t *mask = &g_ai_cv_sweep_binary[offset];

        for (point = 0u; point < point_count; ++point)
        {
            float capacitance = capacitance_pf[path][point];

            if ((valid[path][point] != 0u) && isfinite(capacitance) &&
                (capacitance > 0.0f))
            {
                mask[point >> 3] |= (uint8_t)(1u << (point & 7u));
            }
        }
        offset += valid_bytes;
    }

    crc = AI_CV2SPICE_ACV1_Crc32(g_ai_cv_sweep_binary, offset);
    AI_CV_SWEEP_PutUint32(&g_ai_cv_sweep_binary[offset], crc);
    offset += 4u;
    payload_length = AI_CV_SWEEP_Base64Url(
        g_ai_cv_sweep_binary, offset, output, output_capacity);
    if (payload_length == 0u)
    {
        return AI_CV_SWEEP_PAYLOAD_ERROR_BUFFER;
    }
    *output_length = payload_length;
    if (output_crc32 != NULL)
    {
        *output_crc32 = crc;
    }
    return AI_CV_SWEEP_PAYLOAD_OK;
}
