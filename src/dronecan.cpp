/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/dronecan.h"
#include <string.h>
#include "libdcnode/legacy/uavcan/protocol/get_transport_stats.h"
#include "libdcnode/legacy/uavcan/protocol/node_status.h"
#include "libdcnode/legacy/uavcan/protocol/restart_node.h"
#include "libdcnode/legacy/uavcan/protocol/param/execute_opcode.h"
#include "libdcnode/legacy/uavcan/protocol/param/getset.h"
#include "libdcnode/can_driver.h"
#include "libdcnode/platform.hpp"

#ifndef MAX_PARAM_NAME_LENGTH
#define MAX_PARAM_NAME_LENGTH 32
#endif

#ifndef CANARD_BUFFER_SIZE
#define CANARD_BUFFER_SIZE 1024
#endif

/**
 * @brief Encapsulate everything required for a subscriber
 */
typedef struct
{
    uint64_t signature;
    void (*callback)(CanardRxTransfer *transfer);
    uint16_t id;
} Subscriber_t;
#if UINTPTR_MAX == 0xFFFFFFFF
static_assert(sizeof(Subscriber_t) == 16, "Subscriber_t size mismatch on 32-bit");
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
static_assert(sizeof(Subscriber_t) == 24, "Subscriber_t size mismatch on 64-bit");
#else
#error "Unknown pointer size or unsupported platform"
#endif

typedef struct
{
    CanardInstance g_canard;
    uint8_t buffer[CANARD_BUFFER_SIZE];
    Subscriber_t subscribers[DRONECAN_MAX_SUBS_NUMBER];
    uint8_t number_of_subs;
    bool id_duplication_detected;

    // uavcan.protocol.NodeStatus
    NodeStatus_t node_status;
    uint64_t duplicate_deadline_ms;
    uint32_t node_status_last_send_time_ms;
    uint8_t node_status_transfer_id;

    // uavcan.protocol.GetNodeInfo
    SoftwareVersion sw_version;
    HardwareVersion hw_version;
    const char *node_name;

    // uavcan.protocol.GetTransportStats
    GetTransportStats_t iface_stats;
} DronecanNodeInstance;

#if UINTPTR_MAX == 0xFFFFFFFF
#define INSTANCE_SIZE (208 + CANARD_BUFFER_SIZE + DRONECAN_MAX_SUBS_NUMBER * sizeof(Subscriber_t))
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
#define INSTANCE_SIZE (256 + CANARD_BUFFER_SIZE + DRONECAN_MAX_SUBS_NUMBER * sizeof(Subscriber_t))
#else
#error "Unknown pointer size or unsupported platform"
#endif
static_assert(sizeof(DronecanNodeInstance) == INSTANCE_SIZE);

static DronecanNodeInstance node = {};
static ParamsApi params = {};
static PlatformApi platform = {};

static bool shouldAcceptTransfer(const CanardInstance *ins,
                                 uint64_t *out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id);
static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer);
static uint8_t uavcanProcessSending();
static bool uavcanProcessReceiving(uint32_t crnt_time_ms);
static void uavcanSpinNodeStatus(uint32_t now_ms);

static void uavcanProtocolGetNodeInfoHandle(CanardRxTransfer *transfer);
static void uavcanProtocolParamGetSetHandle(CanardRxTransfer *transfer);
static void uavcanParamExecuteOpcodeHandle(CanardRxTransfer *transfer);
static void uavcanProtocolRestartNodeHandle(CanardRxTransfer *transfer);
static void uavcanProtocolGetTransportStatHandle(CanardRxTransfer *transfer);
static void uavcanProtocolNodeStatusHandle(CanardRxTransfer *transfer);

int8_t uavcanSubscribe(uint64_t signature, uint16_t id, void (*callback)(CanardRxTransfer *))
{
    if (node.number_of_subs >= DRONECAN_MAX_SUBS_NUMBER || signature == 0 || id == 0 || callback == NULL)
    {
        return -1;
    }

    node.subscribers[node.number_of_subs].signature = signature;
    node.subscribers[node.number_of_subs].id = id;
    node.subscribers[node.number_of_subs].callback = callback;
    return node.number_of_subs++;
}

int16_t uavcanPublish(uint64_t data_type_signature,
                      uint16_t data_type_id,
                      uint8_t *inout_transfer_id,
                      uint8_t priority,
                      const void *payload,
                      uint16_t payload_len)
{
    return canardBroadcast(&node.g_canard,
                           data_type_signature,
                           data_type_id,
                           inout_transfer_id,
                           priority,
                           payload,
                           payload_len);
}

void uavcanRespond(CanardRxTransfer *transfer,
                   uint64_t data_type_signature,
                   uint16_t data_type_id,
                   const uint8_t *payload,
                   uint16_t len)
{
    if (!transfer || !payload || len == 0)
    {
        return;
    }

    canardRequestOrRespond(&node.g_canard,
                           transfer->source_node_id,
                           data_type_signature,
                           data_type_id,
                           &transfer->transfer_id,
                           transfer->priority,
                           CanardResponse,
                           payload,
                           len);
}

void uavcanConfigure(const SoftwareVersion *new_sw_vers, const HardwareVersion *new_hw_vers)
{
    node.sw_version.major = new_sw_vers->major;
    node.sw_version.minor = new_sw_vers->minor;
    node.sw_version.vcs_commit = new_sw_vers->vcs_commit;
    node.hw_version.major = new_hw_vers->major;
    node.hw_version.minor = new_hw_vers->minor;
}

void uavcanSetNodeName(const char *new_node_name)
{
    node.node_name = new_node_name;
}

void uavcanStatsIncreaseCanErrors()
{
    node.iface_stats.transfer_errors++;
}
void uavcanStatsIncreaseCanTx(uint8_t num_of_transfers)
{
    node.iface_stats.transfers_tx += num_of_transfers;
}
void uavcanStatsIncreaseCanRx()
{
    node.iface_stats.transfers_rx++;
}
void uavcanStatsIncreaseUartErrors()
{
    node.iface_stats.can_iface_stats[0].errors++;
}
void uavcanStatsIncreaseUartTx(uint32_t num)
{
    node.iface_stats.can_iface_stats[0].frames_tx += num;
}
void uavcanStatsIncreaseUartRx(uint32_t num)
{
    node.iface_stats.can_iface_stats[0].frames_rx += num;
}
uint64_t uavcanGetErrorCount()
{
    return platform.can.getErrorCount();
}

void uavcanSetNodeHealth(NodeStatusHealth_t health)
{
    // Not defined by the UAVCAN spec, but we treat CRITICAL state as persistent.
    // Once set, it can only be cleared by a reboot.
    if (node.node_status.health == NODE_STATUS_HEALTH_CRITICAL)
    {
        return;
    }

    node.node_status.health = health;
}
NodeStatusHealth_t uavcanGetNodeHealth()
{
    return node.node_status.health;
}

void uavcanSetNodeStatusMode(NodeStatusMode_t mode)
{
    node.node_status.mode = mode;
}

NodeStatusMode_t uavcanGetNodeStatusMode()
{
    return node.node_status.mode;
}

void uavcanSetVendorSpecificStatusCode(uint16_t vssc)
{
    node.node_status.vendor_specific_status_code = vssc;
}

const NodeStatus_t *uavcanGetNodeStatus()
{
    return &node.node_status;
}

/// ********************************* PRIVATE *********************************
/**
 * @brief Must have canard callback.
 * The library calls this function on each transfer.
 * @return true if data type is supported and fill signature, otherwise return false
 */
static bool shouldAcceptTransfer(__attribute__((unused)) const CanardInstance *ins,
                                 uint64_t *out_data_type_signature,
                                 uint16_t data_type_id,
                                 __attribute__((unused)) CanardTransferType transfer_type,
                                 __attribute__((unused)) uint8_t source_node_id)
{
    for (uint8_t sub_idx = 0; sub_idx < node.number_of_subs; sub_idx++)
    {
        if (data_type_id == node.subscribers[sub_idx].id)
        {
            *out_data_type_signature = node.subscribers[sub_idx].signature;
            return true;
        }
    }

    return false;
}

/**
 * @brief Must have canard callback.
 * The library calls this function only when shouldAcceptTransfer returns true
 */
static void onTransferReceived(__attribute__((unused)) CanardInstance *ins,
                               CanardRxTransfer *transfer)
{
    for (uint8_t sub_idx = 0; sub_idx < node.number_of_subs; sub_idx++)
    {
        if (transfer->data_type_id == node.subscribers[sub_idx].id)
        {
            transfer->sub_id = sub_idx;
            node.subscribers[sub_idx].callback(transfer);
        }
    }
}

static uint8_t uavcanProcessSending()
{
    const CanardCANFrame *txf = canardPeekTxQueue(&node.g_canard);
    uint8_t tx_attempt = 0;
    uint8_t tx_frames_counter = 0;
    while (txf)
    {
        const int tx_res = platform.can.send(txf, CAN_DRIVER_FIRST);
        if (tx_res > 0)
        {
            canardPopTxQueue(&node.g_canard);
            txf = canardPeekTxQueue(&node.g_canard);
            tx_frames_counter++;
        } else if (tx_res < 0) {
            break;
        }

        if ((tx_attempt++) > 20) {
            break;
        }
    }

    return tx_frames_counter;
}

static bool uavcanProcessReceiving(uint32_t crnt_time_ms)
{
    CanardCANFrame rx_frame;
    for (size_t idx = 0; idx < 10; idx++)
    {
        int16_t res = platform.can.recv(&rx_frame, CAN_DRIVER_FIRST);
        if (res)
        {
            uint64_t crnt_time_us = crnt_time_ms * 1000UL;
            canardHandleRxFrame(&node.g_canard, &rx_frame, crnt_time_us);
        } else {
            break;
        }
    }

    return false;
}

static void uavcanSpinNodeStatus(uint32_t now_ms)
{
    if (now_ms < node.node_status_last_send_time_ms + NODE_STATUS_SPIN_PERIOD_MS)
    {
        return;
    }
    node.node_status_last_send_time_ms = now_ms;

    node.node_status.uptime_sec = now_ms / 1000;
    if (node.duplicate_deadline_ms > now_ms && node.node_status.health == NODE_STATUS_HEALTH_OK)
    {
        node.node_status.health = NODE_STATUS_HEALTH_WARNING;
    }

    uint8_t node_status_buffer[UAVCAN_PROTOCOL_NODE_STATUS_MESSAGE_SIZE];
    uavcanEncodeNodeStatus(node_status_buffer, &node.node_status);
    uavcanPublish(UAVCAN_PROTOCOL_NODE_STATUS_SIGNATURE,
                  UAVCAN_PROTOCOL_NODE_STATUS_ID,
                  &node.node_status_transfer_id,
                  CANARD_TRANSFER_PRIORITY_LOW,
                  node_status_buffer,
                  UAVCAN_PROTOCOL_NODE_STATUS_MESSAGE_SIZE);
}

static void uavcanProtocolGetNodeInfoHandle(CanardRxTransfer *transfer)
{
    uint8_t buf[UAVCAN_GET_NODE_INFO_RESPONSE_MAX_SIZE];
    const NodeStatus_t *status = uavcanGetNodeStatus();
    uint16_t len = uavcanEncodeParamGetNodeInfo(buf, status, &node.sw_version, &node.hw_version, node.node_name);
    uavcanRespond(transfer, UAVCAN_GET_NODE_INFO_DATA_TYPE, buf, len);
}

static void uavcanProtocolParamGetSetHandle(CanardRxTransfer *transfer)
{
    // Value value
    uint8_t set_value_type_tag = uavcanParamGetSetDecodeValueTag(transfer);
    int64_t val_int64 = 0;
    uint8_t val_string[STRING_MAX_SIZE];
    int offset;
    uint8_t str_len = 0;
    switch (set_value_type_tag)
    {
    case PARAM_VALUE_INTEGER:
        val_int64 = uavcanParamGetSetDecodeInteger(transfer);
        offset = 80;
        break;
    case PARAM_VALUE_STRING:
        str_len = uavcanParamGetSetDecodeString(transfer, val_string);
        offset = 24 + 8 * str_len;
        break;
    default:
        offset = 16;
        break;
    }

    // uint8[<=92] recv_name
    uint8_t recv_name[MAX_PARAM_NAME_LENGTH] = "";
    uint16_t param_name_length = uavcanParamGetSetDecodeName(transfer, offset, recv_name);

    // uint13 index
    uint16_t param_idx;
    if (param_name_length)
    {
        param_idx = params.find(recv_name, param_name_length);
    } else {
        param_idx = uavcanParamGetSetDecodeIndex(transfer);
    }

    // @todo a response size must be carefully estimated because it may cause a segfault
    uint8_t resp[96] = "";
    uint16_t len;

    const char *name = params.getName(param_idx);
    if (params.isInteger(param_idx))
    {
        if (set_value_type_tag == PARAM_VALUE_INTEGER)
        {
            params.integer.setValue(param_idx, val_int64);
        }
        IntegerParamValue_t val = params.integer.getValue(param_idx);
        len = uavcanParamGetSetMakeIntResponse(
            resp,
            val,
            params.integer.getDef(param_idx),
            params.integer.getMin(param_idx),
            params.integer.getMax(param_idx),
            name);
    } else if (params.isString(param_idx)) {
        if (set_value_type_tag == PARAM_VALUE_STRING)
        {
            params.string.setValue(param_idx, str_len, val_string);
        }
        const char *str_value = (const char *)params.string.getValue(param_idx);
        len = uavcanParamGetSetMakeStringResponse(resp, str_value, name);
    } else {
        len = uavcanParamGetSetMakeEmptyResponse(resp);
    }

    uavcanRespond(transfer, UAVCAN_PROTOCOL_PARAM_GETSET, resp, len);
}

static void uavcanParamExecuteOpcodeHandle(CanardRxTransfer *transfer)
{
    uint8_t opcode = uavcanProtocolParamExecuteOpcodeDecode(transfer);

    uint8_t opcode_buffer[7];
    int8_t ok;
    switch (opcode)
    {
    case 0:
        ok = (params.save() == -1) ? 0 : 1;
        break;
    case 1:
        ok = (params.resetToDefault() < 0) ? 0 : 1;
        break;
    default:
        ok = -1;
        break;
    }
    uavcanProtocolParamExecuteOpcodeEncode(opcode_buffer, ok);
    uavcanRespond(transfer, UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE, opcode_buffer, 7);
}

static void uavcanProtocolRestartNodeHandle(__attribute__((unused)) CanardRxTransfer *transfer)
{
    uint8_t response_buffer = platform.requestRestart() ? 128 : 0;
    uavcanRespond(transfer, UAVCAN_PROTOCOL_RESTART_NODE, &response_buffer, 1);
}

static void uavcanProtocolGetTransportStatHandle(CanardRxTransfer *transfer)
{
    uint8_t transport_stats_buffer[UAVCAN_PROTOCOL_GET_TRANSPORT_STATS_MAX_SIZE];
    node.iface_stats.transfer_errors = platform.can.getErrorCount();

    uavcanEncodeTransportStats(transport_stats_buffer, &node.iface_stats);
    uavcanRespond(transfer, UAVCAN_PROTOCOL_GET_TRANSPORT_STATS, transport_stats_buffer, 72);
}

static void uavcanProtocolNodeStatusHandle(CanardRxTransfer *transfer)
{
    if (transfer->source_node_id == node.g_canard.node_id)
    {
        node.id_duplication_detected = true;
        node.duplicate_deadline_ms = transfer->timestamp_usec / 1000 + 2000;
    }
}

int16_t uavcanInitApplication(ParamsApi params_api, PlatformApi platform_api, const AppInfo *app_info)
{
    if (app_info)
    {
        uavcanSetNodeName(app_info->node_name);
        node.sw_version.vcs_commit = app_info->vcs_commit;
        node.sw_version.major = app_info->sw_version_major;
        node.sw_version.minor = app_info->sw_version_minor;
        node.hw_version.major = app_info->hw_version_major;
        node.hw_version.minor = app_info->hw_version_minor;
    }

    platform = platform_api;
    libdcnode::PlatformManager::instance().init(platform);

    params = params_api;

    int16_t res = platform.can.init(1000000, CAN_DRIVER_FIRST);
    if (res < 0)
    {
        return res;
    }

    canardInit(&node.g_canard,
               node.buffer,
               CANARD_BUFFER_SIZE,
               onTransferReceived,
               shouldAcceptTransfer,
               NULL);

    canardSetLocalNodeID(&node.g_canard, app_info ? app_info->node_id : 50);

    node.node_status.uptime_sec = 0;
    node.node_status.health = NODE_STATUS_HEALTH_OK;
    node.node_status.mode = NODE_STATUS_MODE_OPERATIONAL;
    node.node_status.sub_mode = 0;
    node.node_status.vendor_specific_status_code = 0;

    platform.readUniqueId(node.hw_version.unique_id);

    uavcanSubscribe(UAVCAN_GET_NODE_INFO_DATA_TYPE, uavcanProtocolGetNodeInfoHandle);
    uavcanSubscribe(UAVCAN_PROTOCOL_PARAM_GETSET, uavcanProtocolParamGetSetHandle);
    uavcanSubscribe(UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE, uavcanParamExecuteOpcodeHandle);
    uavcanSubscribe(UAVCAN_PROTOCOL_RESTART_NODE, uavcanProtocolRestartNodeHandle);
    uavcanSubscribe(UAVCAN_PROTOCOL_GET_TRANSPORT_STATS, uavcanProtocolGetTransportStatHandle);
    uavcanSubscribe(UAVCAN_PROTOCOL_NODE_STATUS, uavcanProtocolNodeStatusHandle);

    return 0;
}

void uavcanSetNodeId(uint8_t node_id)
{
    node.g_canard.node_id = node_id & 127;
}

uint8_t uavcanGetNodeId()
{
    return canardGetLocalNodeID(&node.g_canard);
}

void uavcanSpinOnce()
{
    uint32_t now_ms = platform.getTimeMs();
    uavcanProcessSending();
    uavcanProcessReceiving(now_ms);
    uavcanSpinNodeStatus(now_ms);
}
