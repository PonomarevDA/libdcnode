/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"
#include "canard_stm32.h"
#include "main.h"
#include "can_rx_irq.h"

#define RX_QUEUE_CAPACITY 64U
static CanardCANFrame rx_queue[RX_QUEUE_CAPACITY];
static volatile uint16_t rx_head, rx_tail, rx_count;
static volatile uint64_t rx_drops;
static volatile bool rx_interrupt_enabled;

__attribute__((weak)) bool canDriverConfigureRxInterrupt(bool enabled) {
    (void)enabled;
    return false;
}

static uint32_t lockInterrupts(void) {
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

void canDriverHandleRxInterrupt(void) {
    if (!rx_interrupt_enabled) {
        return;
    }
    // Bound ISR work even when more frames arrive while draining the hardware FIFOs.
    for (unsigned i = 0; i < 3U; ++i) {
        CanardCANFrame frame = {0};
        if (canardSTM32Receive(&frame) <= 0) {
            break;
        }
        if (rx_count == RX_QUEUE_CAPACITY) {
            ++rx_drops;
        } else {
            rx_queue[rx_head] = frame;
            rx_head = (rx_head + 1U) % RX_QUEUE_CAPACITY;
            ++rx_count;
        }
    }
}

void canDriverSetInterfaceName(const char* interface_name) {
    (void)interface_name;
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    uint32_t mask = lockInterrupts();
    canDriverConfigureRxInterrupt(false);
    rx_interrupt_enabled = false;
    rx_head = rx_tail = rx_count = 0;
    rx_drops = 0;
    __set_PRIMASK(mask);
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

    mask = lockInterrupts();
    rx_interrupt_enabled = canDriverConfigureRxInterrupt(true);
    __set_PRIMASK(mask);
    return 0;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    if (rx_frame == NULL) {
        return -CANARD_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t mask = lockInterrupts();
    if (rx_interrupt_enabled) {
        rx_interrupt_enabled = canDriverConfigureRxInterrupt(true);
    }
    const bool polling = !rx_interrupt_enabled;
    const int16_t result = rx_count != 0;
    if (result) {
        *rx_frame = rx_queue[rx_tail];
        rx_tail = (rx_tail + 1U) % RX_QUEUE_CAPACITY;
        --rx_count;
    }
    __set_PRIMASK(mask);
    // Preserve queued frames before reading hardware after a polling fallback.
    return (result || !polling) ? result : canardSTM32Receive(rx_frame);
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    // RX and TX both update the low-level error state; TX never waits for a mailbox.
    const uint32_t mask = lockInterrupts();
    const int16_t result = canardSTM32Transmit(tx_frame);
    __set_PRIMASK(mask);
    return result;
}

uint64_t canDriverGetErrorCount() {
    const uint32_t mask = lockInterrupts();
    const uint64_t result = canardSTM32GetStats().error_count;
    __set_PRIMASK(mask);
    return result;
}

uint64_t canDriverGetRxOverflowCount() {
    const uint32_t mask = lockInterrupts();
    const uint64_t result = canardSTM32GetStats().rx_overflow_count + rx_drops;
    __set_PRIMASK(mask);
    return result;
}
