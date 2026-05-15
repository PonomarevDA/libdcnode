/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/can_driver.h"
#include "socketcan.h"

#ifndef SOCKETCAN_INTERFACE_NAME
    #define SOCKETCAN_INTERFACE_NAME "slcan0"
#endif

SocketCANInstance socket_can_instance;
const char* can_iface_name = SOCKETCAN_INTERFACE_NAME;

void canDriverSetInterfaceName(const char* interface_name) {
    if (interface_name != NULL && interface_name[0] != '\0') {
        can_iface_name = interface_name;
    }
}

int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx) {
    (void)can_speed;
    (void)can_driver_idx;
    return socketcanInit(&socket_can_instance, can_iface_name);;
}

int16_t canDriverReceive(CanardCANFrame* const rx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    rx_frame->iface_id = 0;
    return socketcanReceive(&socket_can_instance, rx_frame, 0);
}

int16_t canDriverTransmit(const CanardCANFrame* const tx_frame, uint8_t can_driver_idx) {
    (void)can_driver_idx;
    return socketcanTransmit(&socket_can_instance, tx_frame, 0);
}

uint64_t canDriverGetErrorCount() {
    return 0;
}

uint64_t canDriverGetRxOverflowCount() {
    return 0;
}
