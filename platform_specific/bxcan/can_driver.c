/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"
#include "canard_stm32.h"
#include "main.h"

void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    CanardSTM32CANTimings timings;
    int16_t res;

    res = canardSTM32ComputeCANTimings(HAL_RCC_GetPCLK1Freq(), can_speed, &timings);
    if (res < CANARD_OK) {
        return res;
    }

    res = canardSTM32Init(&timings, CanardSTM32IfaceModeNormal);
    if (res < CANARD_OK) {
        return res;
    }

    return 0;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    return canardSTM32Receive(rx_frame);
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    return canardSTM32Transmit(tx_frame);
}

uint64_t canDriverGetErrorCount() {
    return canardSTM32GetStats().error_count;
}

uint64_t canDriverGetRxOverflowCount() {
    return canardSTM32GetStats().rx_overflow_count;
}
