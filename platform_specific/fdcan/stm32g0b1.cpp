/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
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
    size_t err_counter;
    size_t tx_counter;
    size_t rx_counter;
};

CanDriver drivers[MAX_INTERFACES]{};
size_t driver_count = 0U;
bool started = false;

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
    if (can_driver_idx >= driver_count) {
        return -1;
    }

    CanDriver& driver = drivers[can_driver_idx];
    driver.tx_header.IdType = FDCAN_EXTENDED_ID;
    driver.tx_header.TxFrameType = FDCAN_DATA_FRAME;
    driver.tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    driver.tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    driver.tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    driver.tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    driver.tx_header.MessageMarker = 0U;

    FDCAN_FilterTypeDef filter_config{};
    filter_config.IdType = FDCAN_EXTENDED_ID;
    filter_config.FilterIndex = 0U;
    filter_config.FilterType = FDCAN_FILTER_MASK;
    filter_config.FilterConfig = FDCAN_FILTER_DISABLE;

    if (HAL_FDCAN_ConfigFilter(driver.handler, &filter_config) != HAL_OK ||
        HAL_FDCAN_Start(driver.handler) != HAL_OK) {
        return -1;
    }

    started = true;
    return 0;
}

extern "C" int16_t canDriverReceive(CanardCANFrame* const rx_frame,
                                      uint8_t can_driver_idx) {
    if (rx_frame == nullptr || can_driver_idx >= driver_count) {
        return 0;
    }

    CanDriver& driver = drivers[can_driver_idx];
    FDCAN_RxHeaderTypeDef rx_header{};
    if (HAL_FDCAN_GetRxMessage(driver.handler,
                               FDCAN_RX_FIFO0,
                               &rx_header,
                               driver.rx_buf) != HAL_OK) {
        return 0;
    }

    driver.rx_counter++;
    rx_frame->id = (CANARD_CAN_EXT_ID_MASK & rx_header.Identifier) | CANARD_CAN_FRAME_EFF;
    rx_frame->data_len = static_cast<uint8_t>(rx_header.DataLength >> 16U);
    rx_frame->iface_id = driver.interface_id;
    memcpy(rx_frame->data, driver.rx_buf, rx_frame->data_len);
    return 1;
}

extern "C" int16_t canDriverTransmit(const CanardCANFrame* const tx_frame,
                                       uint8_t can_driver_idx) {
    if (tx_frame == nullptr || can_driver_idx >= driver_count) {
        return 0;
    }

    CanDriver& driver = drivers[can_driver_idx];
    driver.tx_header.Identifier = tx_frame->id;
    driver.tx_header.DataLength = static_cast<uint32_t>(tx_frame->data_len) << 16U;
    if (HAL_FDCAN_AddMessageToTxFifoQ(driver.handler,
                                      &driver.tx_header,
                                      const_cast<uint8_t*>(tx_frame->data)) != HAL_OK) {
        return 0;
    }

    driver.tx_counter++;
    return 1;
}

extern "C" uint64_t canDriverGetErrorCount() {
    return driver_count > 0U ? drivers[0].err_counter : 0U;
}

extern "C" uint64_t canDriverGetRxOverflowCount() {
    return 0U;
}
