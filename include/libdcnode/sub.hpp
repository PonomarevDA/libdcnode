/*
 * Copyright (C) 2025 Ilia Kliantsevich <iliawork112005@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef LIBDCNODE_SUB_HPP_
#define LIBDCNODE_SUB_HPP_

#include <stdint.h>
#include <array>
#include "libdcnode/dronecan.h"
#include "dronecan_msgs.h"

// Max encoded message size (bytes) for subscriber stack buffer.
// Override via -DLIBDCNODE_MAX_SUB_MESSAGE_SIZE=... if your DSDL set requires more.
#ifndef LIBDCNODE_MAX_SUB_MESSAGE_SIZE
#define LIBDCNODE_MAX_SUB_MESSAGE_SIZE 250
#endif

// Initial delay before first subscribe after boot/reset (ms).
// Intended to reduce CAN bus flooding during rapid reboot loops (e.g., watchdog resets).
// Override via -DLIBDCNODE_INITIAL_SUB_DELAY_MS=...
#ifndef LIBDCNODE_INITIAL_SUB_DELAY_MS
#define LIBDCNODE_INITIAL_SUB_DELAY_MS 500U
#endif

namespace libdcnode
{

    template <typename MessageType>
    struct DronecanSubscriberTraits;

#define LIBDCNODE_DEFINE_SUB_TRAITS(MessageType, Prefix)                               \
    template <>                                                                        \
    struct DronecanSubscriberTraits<MessageType>                                       \
    {                                                                                  \
        static inline int8_t subscribe(void (*callback)(CanardRxTransfer *))           \
        {                                                                              \
            return uavcanSubscribe(Prefix##_SIGNATURE, Prefix##_ID, callback);         \
        }                                                                              \
        static inline int8_t deserialize(CanardRxTransfer *transfer, MessageType *msg) \
        {                                                                              \
            return MessageType##_decode(transfer, msg);                                \
        }                                                                              \
    };

    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_hardpoint_Command,
                                UAVCAN_EQUIPMENT_HARDPOINT_COMMAND)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_indication_LightsCommand, UAVCAN_EQUIPMENT_INDICATION_LIGHTSCOMMAND)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_actuator_ArrayCommand, UAVCAN_EQUIPMENT_ACTUATOR_ARRAYCOMMAND)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_camera_gimbal_AngularCommand,
                                UAVCAN_EQUIPMENT_CAMERA_GIMBAL_ANGULARCOMMAND)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_ahrs_Solution, UAVCAN_EQUIPMENT_AHRS_SOLUTION)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_esc_RawCommand, UAVCAN_EQUIPMENT_ESC_RAWCOMMAND)
    LIBDCNODE_DEFINE_SUB_TRAITS(::uavcan_equipment_safety_ArmingStatus, UAVCAN_EQUIPMENT_SAFETY_ARMINGSTATUS)

    template <typename MessageType>
    class DronecanSub
    {
    public:
        DronecanSub() = default;

        int8_t init(void (*callback)(const MessageType &), bool (*filter_)(const MessageType &) = nullptr)
        {
            user_callback = callback;
            filter = filter_;
            auto sub_id = DronecanSubscriberTraits<MessageType>::subscribe(transfer_callback);
            instances[sub_id] = this;
            return sub_id;
        }

        static inline void transfer_callback(CanardRxTransfer *transfer)
        {
            int8_t res = DronecanSubscriberTraits<MessageType>::deserialize(transfer, &msg);
            if (res < 0)
            {
                return;
            }

            auto instance = static_cast<DronecanSub *>(instances[transfer->sub_id]);
            if (instance == nullptr)
            {
                return;
            }

            if (instance->filter != nullptr && !instance->filter(msg))
            {
                return;
            }

            instance->user_callback(msg);
        }

        static inline std::array<void *, DRONECAN_MAX_SUBS_NUMBER> instances{};
        static inline MessageType msg = {};
        void (*user_callback)(const MessageType &) = nullptr;
        bool (*filter)(const MessageType &) = nullptr;
    };

} // namespace libdcnode

// Undef internal macros
#undef LIBDCNODE_DEFINE_SUB_TRAITS

#endif // LIBDCNODE_SUB_HPP_
