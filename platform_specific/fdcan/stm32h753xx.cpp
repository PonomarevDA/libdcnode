/*
 * Copyright (C) 2026 Ilia Kliantsevich <iliawork112005@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"
#include "fdcan_config.h"

#include <string.h>

namespace {

constexpr size_t MAX_INTERFACES = 2U;

struct CanDriver {
    FDCAN_HandleTypeDef* handler;
    FDCAN_TxHeaderTypeDef tx_header;
    uint8_t interface_id;
    uint8_t rx_buf[8];
    uint32_t err_counter;
    uint32_t tx_counter;
    uint32_t rx_counter;
};

CanDriver drivers[MAX_INTERFACES]{};
size_t driver_count = 0U;
bool started = false;
size_t next_receive = 0U;

bool isConfigurationValid(const DronecanFdcanInterfaceConfig* interfaces,
                          const size_t interface_count) {
    if (interfaces == nullptr || interface_count == 0U || interface_count > MAX_INTERFACES) {
        return false;
    }
    for (size_t idx = 0U; idx < interface_count; idx++) {
        if (interfaces[idx].handle == nullptr) {
            return false;
        }
        for (size_t other = 0U; other < idx; other++) {
            if (interfaces[idx].handle == interfaces[other].handle ||
                interfaces[idx].interface_id == interfaces[other].interface_id) {
                return false;
            }
        }
    }
    return true;
}

void initializeTxHeader(FDCAN_TxHeaderTypeDef& header) {
    header = {};
    header.IdType = FDCAN_EXTENDED_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
}

}  // namespace

extern "C" int16_t dronecanFdcanConfigure(
    const DronecanFdcanInterfaceConfig* interfaces,
    const size_t interface_count) {
    if (started || !isConfigurationValid(interfaces, interface_count)) {
        return -1;
    }
    drivers[0] = {};
    drivers[1] = {};
    driver_count = interface_count;
    next_receive = 0U;
    for (size_t idx = 0U; idx < interface_count; idx++) {
        drivers[idx].handler = interfaces[idx].handle;
        drivers[idx].interface_id = interfaces[idx].interface_id;
    }
    return 0;
}

extern "C" void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

extern "C" int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_speed;
    if (can_driver_idx != CAN_DRIVER_FIRST || driver_count == 0U || started) {
        return -1;
    }
    for (size_t idx = 0U; idx < driver_count; idx++) {
        initializeTxHeader(drivers[idx].tx_header);
        if (HAL_FDCAN_ConfigGlobalFilter(drivers[idx].handler,
                                         FDCAN_ACCEPT_IN_RX_FIFO0,
                                         FDCAN_ACCEPT_IN_RX_FIFO0,
                                         FDCAN_REJECT_REMOTE,
                                         FDCAN_REJECT_REMOTE) != HAL_OK ||
            HAL_FDCAN_Start(drivers[idx].handler) != HAL_OK) {
            drivers[idx].err_counter++;
            return -1;
        }
    }
    started = true;
    return 0;
}

extern "C" int16_t canDriverReceive(CanardCANFrame* const rx_frame,
                                      uint8_t can_driver_idx) {
    if (rx_frame == nullptr || can_driver_idx != CAN_DRIVER_FIRST || !started) {
        return 0;
    }
    for (size_t attempt = 0U; attempt < driver_count; attempt++) {
        const size_t idx = (next_receive + attempt) % driver_count;
        FDCAN_RxHeaderTypeDef header{};
        if (HAL_FDCAN_GetRxMessage(drivers[idx].handler,
                                   FDCAN_RX_FIFO0,
                                   &header,
                                   drivers[idx].rx_buf) != HAL_OK) {
            continue;
        }
        if (header.IdType != FDCAN_EXTENDED_ID || header.RxFrameType != FDCAN_DATA_FRAME ||
            header.FDFormat != FDCAN_CLASSIC_CAN) {
            drivers[idx].err_counter++;
            continue;
        }
        const uint8_t data_len = static_cast<uint8_t>(header.DataLength);
        if (data_len > sizeof(rx_frame->data)) {
            drivers[idx].err_counter++;
            continue;
        }
        rx_frame->id = (header.Identifier & CANARD_CAN_EXT_ID_MASK) | CANARD_CAN_FRAME_EFF;
        rx_frame->data_len = data_len;
        rx_frame->iface_id = drivers[idx].interface_id;
        memcpy(rx_frame->data, drivers[idx].rx_buf, data_len);
        drivers[idx].rx_counter++;
        next_receive = (idx + 1U) % driver_count;
        return 1;
    }
    return 0;
}

extern "C" int16_t canDriverTransmit(const CanardCANFrame* const tx_frame,
                                       uint8_t can_driver_idx) {
    if (tx_frame == nullptr || can_driver_idx != CAN_DRIVER_FIRST || !started ||
        tx_frame->data_len > sizeof(tx_frame->data)) {
        return 0;
    }
    bool sent = false;
    for (size_t idx = 0U; idx < driver_count; idx++) {
        drivers[idx].tx_header.Identifier = tx_frame->id & CANARD_CAN_EXT_ID_MASK;
        drivers[idx].tx_header.DataLength = tx_frame->data_len;
        if (HAL_FDCAN_AddMessageToTxFifoQ(drivers[idx].handler,
                                          &drivers[idx].tx_header,
                                          const_cast<uint8_t*>(tx_frame->data)) == HAL_OK) {
            drivers[idx].tx_counter++;
            sent = true;
        } else {
            drivers[idx].err_counter++;
        }
    }
    return sent ? 1 : 0;
}

extern "C" uint64_t canDriverGetErrorCount() {
    uint64_t errors = 0U;
    for (size_t idx = 0U; idx < driver_count; idx++) {
        FDCAN_ProtocolStatusTypeDef protocol{};
        FDCAN_ErrorCountersTypeDef counters{};
        (void)HAL_FDCAN_GetProtocolStatus(drivers[idx].handler, &protocol);
        (void)HAL_FDCAN_GetErrorCounters(drivers[idx].handler, &counters);
        errors += drivers[idx].err_counter + protocol.BusOff + protocol.Warning +
                  protocol.ErrorPassive + counters.TxErrorCnt + counters.RxErrorCnt;
    }
    return errors;
}

extern "C" uint64_t canDriverGetRxOverflowCount() {
    return 0U;
}
