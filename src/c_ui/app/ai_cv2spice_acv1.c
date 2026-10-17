/* ACV1 载荷编码器 —— 逐字移植自前序工程
 * `~/Desktop/qrs/LCD/XIP_Appli/Core/Src/ai_cv2spice_acv1.c`。 */

#include "ai_cv2spice_acv1.h"

#include <math.h>
#include <string.h>

static uint8_t g_ai_cv2spice_acv1_binary_workspace
    [AI_CV2SPICE_ACV1_MAX_BINARY_SIZE];

static void AI_CV2SPICE_ACV1_PutUint16(uint8_t *destination,
                                       uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void AI_CV2SPICE_ACV1_PutUint32(uint8_t *destination,
                                       uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
    destination[2] = (uint8_t)((value >> 16) & 0xffu);
    destination[3] = (uint8_t)((value >> 24) & 0xffu);
}

/* 与网页model-codec.js保持完全相同的Float32到Float16舍入语义。 */
static AI_CV2SPICE_ACV1_Status AI_CV2SPICE_ACV1_Float16(
    float value,
    uint16_t *result)
{
    uint32_t bits;
    uint32_t sign;
    uint32_t mantissa;
    int32_t exponent;

    if ((result == NULL) || !isfinite(value))
    {
        return AI_CV2SPICE_ACV1_ERROR_FLOAT16_RANGE;
    }
    (void)memcpy(&bits, &value, sizeof(bits));
    sign = (bits >> 16) & 0x8000u;
    exponent = (int32_t)((bits >> 23) & 0xffu) - 127 + 15;
    mantissa = bits & 0x7fffffu;

    if (exponent <= 0)
    {
        if (exponent < -10)
        {
            *result = (uint16_t)sign;
            return AI_CV2SPICE_ACV1_OK;
        }
        mantissa = (mantissa | 0x800000u) >> (uint32_t)(1 - exponent);
        if ((mantissa & 0x1000u) != 0u)
        {
            mantissa += 0x2000u;
        }
        *result = (uint16_t)(sign | (mantissa >> 13));
        return AI_CV2SPICE_ACV1_OK;
    }
    if (exponent >= 31)
    {
        return AI_CV2SPICE_ACV1_ERROR_FLOAT16_RANGE;
    }
    if ((mantissa & 0x1000u) != 0u)
    {
        mantissa += 0x2000u;
        if ((mantissa & 0x800000u) != 0u)
        {
            mantissa = 0u;
            ++exponent;
            if (exponent >= 31)
            {
                return AI_CV2SPICE_ACV1_ERROR_FLOAT16_RANGE;
            }
        }
    }
    *result = (uint16_t)(sign | ((uint32_t)exponent << 10) |
                         (mantissa >> 13));
    return AI_CV2SPICE_ACV1_OK;
}

uint32_t AI_CV2SPICE_ACV1_Crc32(const uint8_t *data, size_t length)
{
    uint32_t value = 0xffffffffu;
    size_t index;

    if ((data == NULL) && (length != 0u))
    {
        return 0u;
    }
    for (index = 0u; index < length; ++index)
    {
        uint32_t current = (value ^ data[index]) & 0xffu;
        uint32_t bit;

        for (bit = 0u; bit < 8u; ++bit)
        {
            current = ((current & 1u) != 0u) ?
                      ((current >> 1) ^ 0xedb88320u) : (current >> 1);
        }
        value = current ^ (value >> 8);
    }
    return value ^ 0xffffffffu;
}

AI_CV2SPICE_ACV1_Status AI_CV2SPICE_ACV1_EncodeBinary(
    const AI_CV2SPICE_ACV1_Model *model,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32)
{
    static const uint8_t magic[4] = {0x41u, 0x43u, 0x56u, 0x31u};
    size_t device_length;
    size_t part_length;
    size_t manufacturer_length;
    size_t required_length;
    size_t offset;
    uint32_t node;
    uint32_t component;
    uint32_t crc;
    float previous_voltage = 0.0f;
    uint16_t half;
    AI_CV2SPICE_ACV1_Status status;

    if ((model == NULL) || (output == NULL) || (output_length == NULL) ||
        (model->device_id == NULL) || (model->part_number == NULL) ||
        (model->manufacturer == NULL) ||
        (model->voltage_nodes_v == NULL))
    {
        return AI_CV2SPICE_ACV1_ERROR_ARGUMENT;
    }
    for (component = 0u;
         component < AI_CV2SPICE_ACV1_COMPONENT_COUNT;
         ++component)
    {
        if (model->delta_charge_nc[component] == NULL)
        {
            return AI_CV2SPICE_ACV1_ERROR_ARGUMENT;
        }
    }
    if ((model->node_count < AI_CV2SPICE_ACV1_MIN_NODE_COUNT) ||
        (model->node_count > AI_CV2SPICE_ACV1_MAX_NODE_COUNT))
    {
        return AI_CV2SPICE_ACV1_ERROR_NODE_COUNT;
    }
    device_length = strlen(model->device_id);
    part_length = strlen(model->part_number);
    manufacturer_length = strlen(model->manufacturer);
    if ((device_length == 0u) || (device_length > 255u) ||
        (part_length == 0u) || (part_length > 255u) ||
        (manufacturer_length == 0u) || (manufacturer_length > 255u) ||
        (model->frequency_hz == 0u) ||
        !isfinite(model->excitation_vpp) ||
        (model->excitation_vpp <= 0.0f) ||
        !isfinite(model->rg_ohm) || (model->rg_ohm <= 0.0f))
    {
        return AI_CV2SPICE_ACV1_ERROR_METADATA;
    }

    required_length = 24u + device_length + part_length +
                      manufacturer_length + 8u * model->node_count;
    if (output_capacity < required_length)
    {
        return AI_CV2SPICE_ACV1_ERROR_BUFFER;
    }
    for (node = 0u; node < model->node_count; ++node)
    {
        const float voltage = model->voltage_nodes_v[node];

        if (!isfinite(voltage) || (voltage <= previous_voltage))
        {
            return AI_CV2SPICE_ACV1_ERROR_VOLTAGE_NODE;
        }
        previous_voltage = voltage;
        for (component = 0u;
             component < AI_CV2SPICE_ACV1_COMPONENT_COUNT;
             ++component)
        {
            const float delta = model->delta_charge_nc[component][node];

            if (!isfinite(delta) || (delta <= 0.0f))
            {
                return AI_CV2SPICE_ACV1_ERROR_DELTA_CHARGE;
            }
        }
    }

    offset = 0u;
    (void)memcpy(&output[offset], magic, sizeof(magic));
    offset += sizeof(magic);
    output[offset++] = AI_CV2SPICE_ACV1_PROTOCOL_VERSION;
    output[offset++] = model->flags;
    output[offset++] = (uint8_t)model->node_count;
    output[offset++] = AI_CV2SPICE_ACV1_COMPONENT_COUNT;
    output[offset++] = (uint8_t)device_length;
    output[offset++] = (uint8_t)part_length;
    output[offset++] = (uint8_t)manufacturer_length;
    output[offset++] = 0u;
    AI_CV2SPICE_ACV1_PutUint32(&output[offset], model->frequency_hz);
    offset += 4u;

    status = AI_CV2SPICE_ACV1_Float16(model->excitation_vpp, &half);
    if (status != AI_CV2SPICE_ACV1_OK)
    {
        return status;
    }
    AI_CV2SPICE_ACV1_PutUint16(&output[offset], half);
    offset += 2u;
    status = AI_CV2SPICE_ACV1_Float16(model->rg_ohm, &half);
    if (status != AI_CV2SPICE_ACV1_OK)
    {
        return status;
    }
    AI_CV2SPICE_ACV1_PutUint16(&output[offset], half);
    offset += 2u;

    (void)memcpy(&output[offset], model->device_id, device_length);
    offset += device_length;
    (void)memcpy(&output[offset], model->part_number, part_length);
    offset += part_length;
    (void)memcpy(&output[offset], model->manufacturer,
                 manufacturer_length);
    offset += manufacturer_length;

    for (node = 0u; node < model->node_count; ++node)
    {
        status = AI_CV2SPICE_ACV1_Float16(model->voltage_nodes_v[node],
                                          &half);
        if (status != AI_CV2SPICE_ACV1_OK)
        {
            return status;
        }
        AI_CV2SPICE_ACV1_PutUint16(&output[offset], half);
        offset += 2u;
    }
    for (component = 0u;
         component < AI_CV2SPICE_ACV1_COMPONENT_COUNT;
         ++component)
    {
        for (node = 0u; node < model->node_count; ++node)
        {
            status = AI_CV2SPICE_ACV1_Float16(
                model->delta_charge_nc[component][node], &half);
            if (status != AI_CV2SPICE_ACV1_OK)
            {
                return status;
            }
            AI_CV2SPICE_ACV1_PutUint16(&output[offset], half);
            offset += 2u;
        }
    }

    crc = AI_CV2SPICE_ACV1_Crc32(output, offset);
    AI_CV2SPICE_ACV1_PutUint32(&output[offset], crc);
    offset += 4u;
    *output_length = offset;
    if (output_crc32 != NULL)
    {
        *output_crc32 = crc;
    }
    return AI_CV2SPICE_ACV1_OK;
}

static size_t AI_CV2SPICE_ACV1_Base64Url(
    const uint8_t *input,
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
        const uint32_t value =
            ((uint32_t)input[input_index] << 16) |
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
    if (output_index >= output_capacity)
    {
        return 0u;
    }
    output[output_index] = '\0';
    return output_index;
}

AI_CV2SPICE_ACV1_Status AI_CV2SPICE_ACV1_EncodePayload(
    const AI_CV2SPICE_ACV1_Model *model,
    char *output,
    size_t output_capacity,
    size_t *output_length,
    uint32_t *output_crc32)
{
    size_t binary_length;
    size_t payload_length;
    AI_CV2SPICE_ACV1_Status status;

    if ((output == NULL) || (output_length == NULL))
    {
        return AI_CV2SPICE_ACV1_ERROR_ARGUMENT;
    }
    status = AI_CV2SPICE_ACV1_EncodeBinary(
        model, g_ai_cv2spice_acv1_binary_workspace,
        sizeof(g_ai_cv2spice_acv1_binary_workspace),
        &binary_length, output_crc32);
    if (status != AI_CV2SPICE_ACV1_OK)
    {
        return status;
    }
    payload_length = AI_CV2SPICE_ACV1_Base64Url(
        g_ai_cv2spice_acv1_binary_workspace, binary_length,
        output, output_capacity);
    if (payload_length == 0u)
    {
        return AI_CV2SPICE_ACV1_ERROR_BUFFER;
    }
    *output_length = payload_length;
    return AI_CV2SPICE_ACV1_OK;
}
