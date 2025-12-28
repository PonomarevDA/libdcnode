/*
 * Copyright (C) 2024 Anastasiia Stepanova <asiiapine@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libdcnode/logger.hpp"
#include "libdcnode/legacy/uavcan/protocol/debug/LogMessage.h"

DronecanLogger::DronecanLogger(const char* source) {
    init(source);
}

void DronecanLogger::init(const char* source) {
    dronecan_protocol_debug_log_message_set_source(&_msg, source);
}

void DronecanLogger::log_debug(const char* text) {
#ifdef NDEBUG
    (void)text;
#else
    log(LOG_LEVEL_DEBUG, text);
#endif
}

void DronecanLogger::log_info(const char* text) {
    log(LOG_LEVEL_INFO, text);
}
void DronecanLogger::log_warn(const char* text) {
    log(LOG_LEVEL_WARNING, text);
}
void DronecanLogger::log_error(const char* text) {
    log(LOG_LEVEL_ERROR, text);
}

void DronecanLogger::log(uint8_t severity, const char* text) {
#ifdef NDEBUG
    if (severity == 0) {
        return;
    }
#endif
    if (dronecan_protocol_debug_log_message_set_severity_level(&_msg, severity) < 0) {
        return;
    }

    if (dronecan_protocol_debug_log_message_set_text(&_msg, text)) {
        return;
    }

    publish(_msg);
}

void DronecanLogger::log_global(uint8_t severity, const char* source, const char* text) {
#ifdef NDEBUG
    if (severity == 0) {
        return;
    }
#endif

    DebugLogMessage_t msg;

    if (dronecan_protocol_debug_log_message_set_severity_level(&msg, severity) < 0) {
        return;
    }

    if (dronecan_protocol_debug_log_message_set_source(&msg, source) < 0) {
        return;
    }

    if (dronecan_protocol_debug_log_message_set_text(&msg, text)) {
        return;
    }

    publish(msg);
}


void DronecanLogger::publish(const DebugLogMessage_t& msg) {
    dronecan_protocol_debug_log_message_publish(&msg, &_transfer_id);
    _transfer_id++;
}
