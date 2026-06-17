/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
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
    {.handler = &hfdcan1, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0},
#if NUM_OF_CAN_BUSES >= 2
    {.handler = &hfdcan2, .tx_header = {}, .rx_buf = {}, .err_counter = 0, .tx_counter = 0, .rx_counter = 0}
#endif
};

void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_speed;
    if (can_driver_idx >= NUM_OF_CAN_BUSES) {
        return -1;
    }

    driver[can_driver_idx].tx_header.IdType = FDCAN_EXTENDED_ID;
    driver[can_driver_idx].tx_header.TxFrameType = FDCAN_DATA_FRAME;
    driver[can_driver_idx].tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    driver[can_driver_idx].tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    driver[can_driver_idx].tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    driver[can_driver_idx].tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    driver[can_driver_idx].tx_header.MessageMarker = 0;

    FDCAN_FilterTypeDef sFilterConfig;
    sFilterConfig.IdType = FDCAN_EXTENDED_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    sFilterConfig.FilterConfig = FDCAN_FILTER_DISABLE;

    if (HAL_FDCAN_ConfigFilter(driver[can_driver_idx].handler, &sFilterConfig) != HAL_OK) {
        return -1;
    } else if (HAL_FDCAN_Start(driver[can_driver_idx].handler) != HAL_OK) {
        return -1;
    }

    return 0;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    if (rx_frame == NULL || can_driver_idx >= NUM_OF_CAN_BUSES) {
        return 0;
    }

    FDCAN_RxHeaderTypeDef rx_header;

    HAL_StatusTypeDef res = HAL_FDCAN_GetRxMessage(driver[can_driver_idx].handler,
                                                   FDCAN_RX_FIFO0,
                                                   &rx_header,
                                                   driver[can_driver_idx].rx_buf);
    if (res != HAL_OK) {
        return 0;
    }

    driver[can_driver_idx].rx_counter++;
    rx_frame->id = (CANARD_CAN_EXT_ID_MASK & (rx_header.Identifier)) | CANARD_CAN_FRAME_EFF;
    rx_frame->data_len = rx_header.DataLength >> 16U;
    rx_frame->iface_id = can_driver_idx;
    memcpy(rx_frame->data, driver[can_driver_idx].rx_buf, rx_frame->data_len);
    return 1;
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    if (tx_frame == NULL || can_driver_idx >= NUM_OF_CAN_BUSES) {
        return 0;
    }

    driver[can_driver_idx].tx_header.Identifier = tx_frame->id;
    driver[can_driver_idx].tx_header.DataLength = tx_frame->data_len << 16U;

    HAL_StatusTypeDef res = HAL_FDCAN_AddMessageToTxFifoQ(driver[can_driver_idx].handler,
                                                          &driver[can_driver_idx].tx_header,
                                                          (uint8_t*)tx_frame->data);
    if (res == HAL_OK) {
        driver[can_driver_idx].tx_counter++;
        return 1;
    } else {
        return 0;
    }
}

uint64_t canDriverGetErrorCount() {
    return driver[0].err_counter;
}

uint64_t canDriverGetRxOverflowCount() {
    return 0;
}
