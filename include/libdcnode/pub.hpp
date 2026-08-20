/*
 * Copyright (C) 2025 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef LIBDCNODE_PUB_HPP_
#define LIBDCNODE_PUB_HPP_

#include <cstdint>
#include <algorithm>
#include "libdcnode/dronecan.h"
#include "libdcnode/platform.hpp"
#include "dronecan_msgs.h"

// Initial delay before first publish after boot/reset (ms).
// Intended to reduce CAN bus flooding during rapid reboot loops (e.g., watchdog resets).
// Override via -DLIBDCNODE_INITIAL_PUB_DELAY_MS=...
#ifndef LIBDCNODE_INITIAL_PUB_DELAY_MS
#define LIBDCNODE_INITIAL_PUB_DELAY_MS 500U
#endif

namespace libdcnode
{
    template <typename MessageType>
    class DronecanPub
    {
    public:
        DronecanPub() = default;

        inline void publish()
        {
            using Interface = typename MessageType::cxx_iface;
            uint8_t buffer[Interface::MAX_SIZE];
#if CANARD_ENABLE_CANFD || CANARD_ENABLE_TAO_OPTION
            const auto bytes_needed = Interface::encode(&msg, buffer, true);
#else
            const auto bytes_needed = Interface::encode(&msg, buffer);
#endif
            if (bytes_needed == 0U || bytes_needed > Interface::MAX_SIZE)
            {
                return;
            }
            uavcanPublish(Interface::SIGNATURE,
                          Interface::ID,
                          &inout_transfer_id,
                          CANARD_TRANSFER_PRIORITY_MEDIUM,
                          buffer,
                          bytes_needed);
        }

        MessageType msg{};

    private:
        uint8_t inout_transfer_id{0};
    };

    template <typename MessageType>
    class DronecanPeriodicPub : public DronecanPub<MessageType>
    {
    public:
        explicit DronecanPeriodicPub(float frequency)
            : DronecanPub<MessageType>(),
              _pub_period_ms(static_cast<uint32_t>(
                  1000.0f / std::clamp(frequency, 0.001f, 1000.0f)))
        {}

        inline void spinOnce()
        {
            auto crnt_time_ms = libdcnode::getPlatformApi().getTimeMs();
            if (crnt_time_ms < _next_pub_time_ms)
            {
                return;
            }
            _next_pub_time_ms = crnt_time_ms + _pub_period_ms;

            this->publish();
        }

    private:
        const uint32_t _pub_period_ms;
        uint32_t _next_pub_time_ms{LIBDCNODE_INITIAL_PUB_DELAY_MS};
    };

} // namespace libdcnode

#endif // LIBDCNODE_PUB_HPP_
