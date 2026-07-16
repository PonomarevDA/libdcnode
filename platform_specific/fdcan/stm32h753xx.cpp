/*
 * Copyright (C) 2026 Ilia Kliantsevich <iliawork112005@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"

#include <string.h>

#include "main.h"

#ifndef DRONECAN_FDCAN_PRIMARY
    #define DRONECAN_FDCAN_PRIMARY 1
#endif

#ifndef CANOPEN_FDCAN_PRIMARY
    #define CANOPEN_FDCAN_PRIMARY 2
#endif

static_assert(DRONECAN_FDCAN_PRIMARY == 1 || DRONECAN_FDCAN_PRIMARY == 2,
              "DRONECAN_FDCAN_PRIMARY must be 1 or 2");
static_assert(CANOPEN_FDCAN_PRIMARY == 1 || CANOPEN_FDCAN_PRIMARY == 2,
              "CANOPEN_FDCAN_PRIMARY must be 1 or 2");
static_assert(DRONECAN_FDCAN_PRIMARY != CANOPEN_FDCAN_PRIMARY,
              "DroneCAN and CANopen cannot own the same FDCAN controller");

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

namespace {

struct CanDriver {
    FDCAN_HandleTypeDef* handler;
    FDCAN_TxHeaderTypeDef tx_header;
    uint8_t rx_buf[8];
    size_t err_counter;
    size_t tx_counter;
    size_t rx_counter;
};

#if DRONECAN_FDCAN_PRIMARY == 2
CanDriver driver{.handler = &hfdcan2};
#else
CanDriver driver{.handler = &hfdcan1};
#endif

}  // namespace

void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_speed;
    if (can_driver_idx != CAN_DRIVER_FIRST) {
        return -1;
    }

    driver.tx_header.IdType = FDCAN_EXTENDED_ID;
    driver.tx_header.TxFrameType = FDCAN_DATA_FRAME;
    driver.tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    driver.tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    driver.tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    driver.tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    driver.tx_header.MessageMarker = 0U;

    if (HAL_FDCAN_ConfigGlobalFilter(driver.handler,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK ||
        HAL_FDCAN_Start(driver.handler) != HAL_OK) {
        driver.err_counter++;
        return -1;
    }
    return 0;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    if (rx_frame == nullptr || can_driver_idx != CAN_DRIVER_FIRST) {
        return 0;
    }

    FDCAN_RxHeaderTypeDef header{};
    if (HAL_FDCAN_GetRxMessage(driver.handler, FDCAN_RX_FIFO0, &header, driver.rx_buf) !=
        HAL_OK) {
        return 0;
    }
    if (header.IdType != FDCAN_EXTENDED_ID || header.RxFrameType != FDCAN_DATA_FRAME ||
        header.FDFormat != FDCAN_CLASSIC_CAN) {
        driver.err_counter++;
        return 0;
    }

    const uint8_t data_len = static_cast<uint8_t>(header.DataLength);
    if (data_len > sizeof(rx_frame->data)) {
        driver.err_counter++;
        return 0;
    }
    rx_frame->id = (header.Identifier & CANARD_CAN_EXT_ID_MASK) | CANARD_CAN_FRAME_EFF;
    rx_frame->data_len = data_len;
    rx_frame->iface_id = 0U;
    memcpy(rx_frame->data, driver.rx_buf, data_len);
    driver.rx_counter++;
    return 1;
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    if (tx_frame == nullptr || can_driver_idx != CAN_DRIVER_FIRST ||
        tx_frame->data_len > sizeof(tx_frame->data)) {
        return 0;
    }

    driver.tx_header.Identifier = tx_frame->id & CANARD_CAN_EXT_ID_MASK;
    driver.tx_header.DataLength = tx_frame->data_len;
    if (HAL_FDCAN_AddMessageToTxFifoQ(driver.handler,
                                      &driver.tx_header,
                                      const_cast<uint8_t*>(tx_frame->data)) != HAL_OK) {
        driver.err_counter++;
        return 0;
    }
    driver.tx_counter++;
    return 1;
}

uint64_t canDriverGetErrorCount() {
    FDCAN_ProtocolStatusTypeDef protocol_status{};
    FDCAN_ErrorCountersTypeDef error_counters{};
    (void)HAL_FDCAN_GetProtocolStatus(driver.handler, &protocol_status);
    (void)HAL_FDCAN_GetErrorCounters(driver.handler, &error_counters);
    return driver.err_counter + protocol_status.BusOff + protocol_status.Warning +
           protocol_status.ErrorPassive + error_counters.TxErrorCnt + error_counters.RxErrorCnt;
}

uint64_t canDriverGetRxOverflowCount() {
    return 0U;
}
