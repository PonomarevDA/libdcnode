/*
 * Copyright (C) 2026 Ilia Kliantsevich <iliawork112005@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "libdcnode/pub.hpp"
#include "libdcnode/sub.hpp"

namespace
{
using Message = uavcan_protocol_Panic;
using Interface = Message::cxx_iface;

uint64_t subscribed_signature = 0;
uint16_t subscribed_id = 0;
void (*subscribed_callback)(CanardRxTransfer *) = nullptr;
int8_t next_subscription_id = 0;

std::array<uint8_t, 2> published_transfer_ids{};
std::array<uint8_t, Interface::MAX_SIZE> published_payload{};
uint64_t published_signature = 0;
uint16_t published_id = 0;
uint16_t published_payload_len = 0;
std::size_t publish_count = 0;

std::size_t callback_count = 0;
Message received_message{};

void message_callback(const Message& msg)
{
    callback_count++;
    received_message = msg;
}
} // namespace

extern "C" int8_t uavcanSubscribe(uint64_t signature,
                                   uint16_t id,
                                   void (*callback)(CanardRxTransfer *transfer))
{
    subscribed_signature = signature;
    subscribed_id = id;
    subscribed_callback = callback;
    return next_subscription_id++;
}

extern "C" int16_t uavcanPublish(uint64_t signature,
                                  uint16_t id,
                                  uint8_t *inout_transfer_id,
                                  uint8_t priority,
                                  const void *payload,
                                  uint16_t payload_len)
{
    assert(publish_count < published_transfer_ids.size());
    assert(priority == CANARD_TRANSFER_PRIORITY_MEDIUM);
    assert(payload_len <= published_payload.size());

    published_signature = signature;
    published_id = id;
    published_payload_len = payload_len;
    published_transfer_ids[publish_count++] = *inout_transfer_id;
    std::memcpy(published_payload.data(), payload, payload_len);

    *inout_transfer_id = static_cast<uint8_t>((*inout_transfer_id + 1U) & 31U);
    return 1;
}

int main()
{
    static_assert(Interface::ID == UAVCAN_PROTOCOL_PANIC_ID);
    static_assert(Interface::SIGNATURE == UAVCAN_PROTOCOL_PANIC_SIGNATURE);
    static_assert(Interface::MAX_SIZE == UAVCAN_PROTOCOL_PANIC_MAX_SIZE);

    libdcnode::DronecanSub<Message> sub;
    const auto subscription_id = sub.init(message_callback);
    assert(subscription_id >= 0);
    assert(subscribed_signature == Interface::SIGNATURE);
    assert(subscribed_id == Interface::ID);
    assert(subscribed_callback != nullptr);

    Message source{};
    source.reason_text.len = 3;
    source.reason_text.data[0] = 'b';
    source.reason_text.data[1] = 'a';
    source.reason_text.data[2] = 'd';

    std::array<uint8_t, Interface::MAX_SIZE> encoded{};
#if CANARD_ENABLE_CANFD || CANARD_ENABLE_TAO_OPTION
    const auto encoded_size = Interface::encode(&source, encoded.data(), true);
#else
    const auto encoded_size = Interface::encode(&source, encoded.data());
#endif
    assert(encoded_size > 0U);

    CanardRxTransfer transfer{};
    transfer.payload_head = encoded.data();
    transfer.payload_len = static_cast<uint16_t>(encoded_size);
    transfer.data_type_id = Interface::ID;
    transfer.sub_id = static_cast<uint8_t>(subscription_id);
    subscribed_callback(&transfer);

    assert(callback_count == 1U);
    assert(received_message.reason_text.len == source.reason_text.len);
    assert(std::memcmp(received_message.reason_text.data,
                       source.reason_text.data,
                       source.reason_text.len) == 0);

    transfer.payload_len = static_cast<uint16_t>(Interface::MAX_SIZE + 1U);
    subscribed_callback(&transfer);
    assert(callback_count == 1U);

    libdcnode::DronecanPub<Message> pub;
    pub.msg = source;
    pub.publish();
    pub.publish();

    assert(publish_count == 2U);
    assert(published_transfer_ids[0] == 0U);
    assert(published_transfer_ids[1] == 1U);
    assert(published_signature == Interface::SIGNATURE);
    assert(published_id == Interface::ID);
    assert(published_payload_len == encoded_size);
    assert(std::memcmp(published_payload.data(), encoded.data(), encoded_size) == 0);
    return 0;
}
