/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef LIBDCNODE_CAN_DRIVER_H_
#define LIBDCNODE_CAN_DRIVER_H_

#include "libdcnode/internal.h"

#define CAN_DRIVER_FIRST 0
#define CAN_DRIVER_SECOND 1
// The extra cast to unsigned is needed to squelch warnings from clang-tidy
#define IS_START_OF_TRANSFER(x) ((bool)(((uint32_t)(x) >> 7U) & 0x1U))
#define IS_END_OF_TRANSFER(x) ((bool)(((uint32_t)(x) >> 6U) & 0x1U))
#define TOGGLE_BIT(x) ((bool)(((uint32_t)(x) >> 5U) & 0x1U))

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        CAN_PROTOCOL_DRONECAN = 0,
        CAN_PROTOCOL_CYPHAL = 1,
        CAN_PROTOCOL_UNKNOWN = -1
    } CanProtocol;

    int16_t canDriverInit(uint32_t can_speed, uint8_t can_driver_idx);

    void canDriverSetInterfaceName(const char *interface_name);

    int16_t canDriverReceive(CanardCANFrame *const rx_frame, uint8_t can_driver_idx);

    int16_t canDriverTransmit(const CanardCANFrame *const tx_frame, uint8_t can_driver_idx);

    /*
     * @brief Get protocol of the CAN driver
     * @return 0 if protocol is Dronecan, 1 if Cyphal, -1 if unknown
     */
    inline CanProtocol canDriverGetProtocol(uint8_t can_driver_idx)
    {
        CanardCANFrame rx_frame;
        if (canDriverReceive(&rx_frame, can_driver_idx) == 0)
        {
            const uint8_t tail_byte = rx_frame.data[rx_frame.data_len - 1];
            if (IS_START_OF_TRANSFER(tail_byte) && IS_END_OF_TRANSFER(tail_byte))
            {
                return TOGGLE_BIT(tail_byte) ? CAN_PROTOCOL_CYPHAL : CAN_PROTOCOL_DRONECAN;
            }
        }
        return CAN_PROTOCOL_UNKNOWN;
    }

    uint64_t canDriverGetRxOverflowCount();
    uint64_t canDriverGetErrorCount();

#ifdef USE_PLATFORM_NODE_V4
    uint64_t canDriverGetTxCount(uint8_t can_driver_idx);
    uint64_t canDriverGetRxCount(uint8_t can_driver_idx);
    int32_t canDriverGetLastTxStatus(uint8_t can_driver_idx);
    int32_t canDriverGetLastInitStatus(uint8_t can_driver_idx);
    uint32_t canDriverGetTxErrorCount(uint8_t can_driver_idx);
    uint32_t canDriverGetBusOff(uint8_t can_driver_idx);
    uint32_t canDriverGetTxFifoFreeLevel(uint8_t can_driver_idx);
    uint32_t canDriverGetLastErrorCode(uint8_t can_driver_idx);
    uint32_t canDriverGetActivity(uint8_t can_driver_idx);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBDCNODE_CAN_DRIVER_H_
