/*
 * Copyright (C) 2026 Ilia Kliantsevich <iliawork112005@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"

#include <string.h>

#include "main.h"

#ifndef NUM_OF_CAN_BUSES
    #define NUM_OF_CAN_BUSES 1
#endif

#ifndef DRONECAN_FDCAN_PRIMARY
    #define DRONECAN_FDCAN_PRIMARY 1
#endif

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

typedef struct {
    FDCAN_HandleTypeDef* handler;
    FDCAN_TxHeaderTypeDef tx_header;
    uint8_t rx_buf[8];
    size_t err_counter;
    size_t tx_counter;
    size_t rx_counter;
} CanDriver;

static CanDriver driver[NUM_OF_CAN_BUSES] = {
#if DRONECAN_FDCAN_PRIMARY == 2
    {.handler = &hfdcan2, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0},
#if NUM_OF_CAN_BUSES >= 2
    {.handler = &hfdcan1, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0}
#endif
#else
    {.handler = &hfdcan1, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0},
#if NUM_OF_CAN_BUSES >= 2
    {.handler = &hfdcan2, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0}
#endif
#endif
};

void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

static int16_t canDriverInitPhysical(uint8_t physical_idx) {
    if (physical_idx >= NUM_OF_CAN_BUSES) {
        return -1;
    }

    driver[physical_idx].tx_header.IdType = FDCAN_EXTENDED_ID;
    driver[physical_idx].tx_header.TxFrameType = FDCAN_DATA_FRAME;
    driver[physical_idx].tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    driver[physical_idx].tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    driver[physical_idx].tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    driver[physical_idx].tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    driver[physical_idx].tx_header.MessageMarker = 0;

    HAL_StatusTypeDef res = HAL_FDCAN_ConfigGlobalFilter(
        driver[physical_idx].handler,
        FDCAN_ACCEPT_IN_RX_FIFO0,
        FDCAN_ACCEPT_IN_RX_FIFO0,
        FDCAN_REJECT_REMOTE,
        FDCAN_REJECT_REMOTE);
    if (res != HAL_OK) {
        driver[physical_idx].err_counter++;
        return -1;
    }

    res = HAL_FDCAN_Start(driver[physical_idx].handler);
    if (res != HAL_OK) {
        driver[physical_idx].err_counter++;
        return -1;
    }

    return 0;
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_speed;
    if (can_driver_idx != CAN_DRIVER_FIRST) {
        return -1;
    }

    for (uint8_t physical_idx = 0; physical_idx < NUM_OF_CAN_BUSES; physical_idx++) {
        const int16_t res = canDriverInitPhysical(physical_idx);
        if (res < 0) {
            return res;
        }
    }

    return 0;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    if (rx_frame == NULL || can_driver_idx != CAN_DRIVER_FIRST) {
        return 0;
    }

    static uint8_t next_physical_idx = 0;
    for (uint8_t attempt = 0; attempt < NUM_OF_CAN_BUSES; attempt++) {
        const uint8_t physical_idx = static_cast<uint8_t>((next_physical_idx + attempt) % NUM_OF_CAN_BUSES);
        FDCAN_RxHeaderTypeDef rx_header;

        HAL_StatusTypeDef res = HAL_FDCAN_GetRxMessage(driver[physical_idx].handler,
                                                       FDCAN_RX_FIFO0,
                                                       &rx_header,
                                                       driver[physical_idx].rx_buf);
        if (res != HAL_OK) {
            continue;
        }

        driver[physical_idx].rx_counter++;
        rx_frame->id = (CANARD_CAN_EXT_ID_MASK & (rx_header.Identifier)) | CANARD_CAN_FRAME_EFF;
        rx_frame->data_len = static_cast<uint8_t>(rx_header.DataLength);
        rx_frame->iface_id = physical_idx;
        memcpy(rx_frame->data, driver[physical_idx].rx_buf, rx_frame->data_len);
        next_physical_idx = static_cast<uint8_t>((physical_idx + 1) % NUM_OF_CAN_BUSES);
        return 1;
    }

    return 0;
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    if (tx_frame == NULL || can_driver_idx != CAN_DRIVER_FIRST) {
        return 0;
    }

    bool sent = false;
    for (uint8_t physical_idx = 0; physical_idx < NUM_OF_CAN_BUSES; physical_idx++) {
        driver[physical_idx].tx_header.Identifier = tx_frame->id & CANARD_CAN_EXT_ID_MASK;
        driver[physical_idx].tx_header.DataLength = tx_frame->data_len;

        HAL_StatusTypeDef res = HAL_FDCAN_AddMessageToTxFifoQ(
            driver[physical_idx].handler,
            &driver[physical_idx].tx_header,
            (uint8_t*)tx_frame->data);
        if (res == HAL_OK) {
            driver[physical_idx].tx_counter++;
            sent = true;
        } else {
            driver[physical_idx].err_counter++;
        }
    }

    return sent ? 1 : 0;
}

uint64_t canDriverGetErrorCount() {
    uint64_t errors = 0;
    for (uint8_t idx = 0; idx < NUM_OF_CAN_BUSES; idx++) {
        FDCAN_ProtocolStatusTypeDef protocol_status = {};
        FDCAN_ErrorCountersTypeDef error_counters = {};
        (void)HAL_FDCAN_GetProtocolStatus(driver[idx].handler, &protocol_status);
        (void)HAL_FDCAN_GetErrorCounters(driver[idx].handler, &error_counters);
        errors += driver[idx].err_counter;
        errors += protocol_status.BusOff;
        errors += protocol_status.Warning;
        errors += protocol_status.ErrorPassive;
        errors += error_counters.TxErrorCnt;
        errors += error_counters.RxErrorCnt;
    }
    return errors;
}

uint64_t canDriverGetRxOverflowCount() {
    return 0;
}
