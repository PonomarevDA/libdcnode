/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef UAVCAN_PROTOCOL_PARAM_GETSET_H_
#define UAVCAN_PROTOCOL_PARAM_GETSET_H_

#include <string.h>
#include "libdcnode/internal.h"
#include "libdcnode/serialization_internal.h"

#define STRING_MAX_SIZE 56

#define UAVCAN_PROTOCOL_PARAM_GETSET_ID             11
#define UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE      0xa7b622f939d1a4d5
#define UAVCAN_PROTOCOL_PARAM_GETSET                UAVCAN_EXPAND(UAVCAN_PROTOCOL_PARAM_GETSET)


typedef enum {
    PARAM_VALUE_UNDEFINED,
    PARAM_VALUE_INTEGER,
    PARAM_VALUE_REAL,
    PARAM_VALUE_BOOLEAN,
    PARAM_VALUE_STRING,
} UavcanProtocolParamValue_t;

#ifdef __cplusplus
extern "C" {
#endif

static inline void uavcanEncodeParamValueInt32(uint8_t* buffer, uint16_t offset, int32_t value)
{
    const uint8_t tag_integer = PARAM_VALUE_INTEGER;
    int64_t value_i64 = value;
    canardEncodeScalar(buffer,  offset, 3, &tag_integer);
    canardEncodeScalar(buffer, offset + 3, 64, &value_i64);
}

static inline void uavcanEncodeParamNumericValueInt32(uint8_t* buffer, uint16_t offset, int32_t value)
{
    const uint8_t tag_integer = PARAM_VALUE_INTEGER;
    int64_t value_i64 = value;
    canardEncodeScalar(buffer,  offset, 2, &tag_integer);
    canardEncodeScalar(buffer, offset + 2, 64, &value_i64);
}

uint16_t uavcanParamGetSetMakeIntResponse(
    uint8_t* buffer,
    int64_t val,
    int64_t def,
    int64_t min,
    int64_t max,
    const char* param_name)
{
    buffer[0 / 8] = 0;
    uavcanEncodeParamValueInt32(buffer,  5, val);
    buffer[72 / 8] = 0;
    uavcanEncodeParamValueInt32(buffer,  77, def);
    buffer[144 / 8] = 0;
    uavcanEncodeParamNumericValueInt32(buffer,  150, max);
    buffer[216 / 8] = 0;
    uavcanEncodeParamNumericValueInt32(buffer,  222, min);

    // uint8[<=92] name
    size_t param_name_length = strlenSafely(param_name, 92);
    memcpy(&buffer[288 / 8], param_name, param_name_length);

    return 288/8 + param_name_length;
}

uint16_t uavcanParamGetSetMakeStringResponse(
    uint8_t* buffer,
    const char* str_val,
    const char* param_name)
{
    if (buffer == NULL || str_val == NULL || param_name == NULL) {
        return 0;
    }

    const uint8_t tag_empty = PARAM_VALUE_UNDEFINED;
    const uint8_t tag_string = PARAM_VALUE_STRING;

    uint8_t dummy_zero_byte = 0;
    canardEncodeScalar(buffer, 0, 5, &dummy_zero_byte);

    // Value value (uint8[<=128] string_value)
    canardEncodeScalar(buffer, 5, 3, &tag_string);
    uint8_t string_size = strlenSafely(str_val, STRING_MAX_SIZE);
    uint16_t string_bit_offset = string_size * 8;
    canardEncodeScalar(buffer, 8, 8, &string_size);
    memcpy(&buffer[2], str_val, string_size);

    // void6, NumericValue default_value is Optional
    canardEncodeScalar(buffer, string_bit_offset + 16, 5, &dummy_zero_byte);
    canardEncodeScalar(buffer, string_bit_offset + 29, 3, &tag_empty);

    // void6, NumericValue max_value not applicable for bool/string
    canardEncodeScalar(buffer, string_bit_offset + 32, 6, &dummy_zero_byte);
    canardEncodeScalar(buffer, string_bit_offset + 38, 2, &tag_empty);

    // void6, NumericValue min_value not applicable for bool/string
    canardEncodeScalar(buffer, string_bit_offset + 40, 6, &dummy_zero_byte);
    canardEncodeScalar(buffer, string_bit_offset + 46, 2, &tag_empty);

    // uint8[<=92] name
    uint8_t param_name_length = strlenSafely(param_name, 92);
    memcpy(&buffer[5 + string_size], param_name, param_name_length);
    return 5 + string_size + param_name_length;
}

uint16_t uavcanParamGetSetMakeEmptyResponse(uint8_t* buffer)
{
    memset(buffer, 0x00, 4);
    return 4;
}

uint16_t uavcanParamGetSetDecodeIndex(const CanardRxTransfer* transfer)
{
    uint16_t param_idx = 0xFFFF;
    canardDecodeScalar(transfer, 0,  13, false, &param_idx);
    return param_idx;
}

uint8_t uavcanParamGetSetDecodeValueTag(const CanardRxTransfer* transfer)
{
    uint8_t value_type_tag = PARAM_VALUE_UNDEFINED;
    canardDecodeScalar(transfer, 13, 3, false, &value_type_tag);
    return value_type_tag;
}

uint64_t uavcanParamGetSetDecodeInteger(const CanardRxTransfer* transfer)
{
    uint64_t val_int64 = PARAM_VALUE_UNDEFINED;
    canardDecodeScalar(transfer, 16, 64, false, &val_int64);
    return val_int64;
}

uint8_t uavcanParamGetSetDecodeString(
    const CanardRxTransfer* transfer,
    uint8_t* val_string)
{
    uint8_t str_len = 0;
    canardDecodeScalar(transfer, 16, 8, false, &str_len);

    int offset = 24;
    uint32_t len = (str_len < STRING_MAX_SIZE) ? str_len : STRING_MAX_SIZE;
    for (uint_fast8_t idx = 0; idx < len; idx++) {
        canardDecodeScalar(transfer, offset, 8, false, &val_string[idx]);
        offset += 8;
    }

    return str_len;
}

uint8_t uavcanParamGetSetDecodeName(
    const CanardRxTransfer* transfer,
    int offset,
    uint8_t* name)
{
    uint16_t param_name_length = transfer->payload_len - offset / 8;
    for (int idx = 0; idx < param_name_length; idx++) {
        canardDecodeScalar(transfer, offset, 8, false, &name[idx]);
        offset += 8;
    }
    return param_name_length;
}

#ifdef __cplusplus
}
#endif

#endif  // UAVCAN_PROTOCOL_PARAM_GETSET_H_
