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

namespace libdcnode
{
    template <typename MessageType>
    class DronecanSub
    {
    public:
        DronecanSub() = default;
        /*
        * @brief User must process result of function to ensure that callback is properly registered. 
        * @param callback User-defined function to be called upon message reception.
        * @param filter Optional user-defined function to filter messages.
        * @return Subscription ID on success, negative error code on failure.
        */
        [[nodiscard]] int8_t init(void (*callback)(const MessageType &), bool (*filter_)(const MessageType &) = nullptr)
        {
            user_callback = callback;
            filter = filter_;
            using Interface = typename MessageType::cxx_iface;
            auto sub_id = uavcanSubscribe(Interface::SIGNATURE, Interface::ID, transfer_callback);
            if (sub_id < 0) {
                return sub_id;
            }
            instances[sub_id] = this;
            return sub_id;
        }

        static inline void transfer_callback(CanardRxTransfer *transfer)
        {
            using Interface = typename MessageType::cxx_iface;
            if (Interface::decode(transfer, &msg))
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

#endif // LIBDCNODE_SUB_HPP_
