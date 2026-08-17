/*
 * Copyright (C) 2024 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <assert.h>
#include <iostream>
#include <chrono>
#include <string.h>
#include "storage.h"
#include "libdcnode/dronecan.h"
#include "libdcnode/can_driver.h"
// #include "libdcnode/subscriber.hpp"
// #include "libdcnode/publisher.hpp"

// Hack
// It's ok to keep some undefs for now, but maybe there is a better solution;
// This hack won't be necessary after removing legacy serialization
#undef UAVCAN_EQUIPMENT_ACTUATOR_COMMAND_SIGNATURE
#undef UAVCAN_EQUIPMENT_ACTUATOR_STATUS_SIGNATURE
#undef UAVCAN_EQUIPMENT_AHRS_SOLUTION_SIGNATURE
#undef UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_SIGNATURE
#undef UAVCAN_EQUIPMENT_HARDPOINT_COMMAND_SIGNATURE
#undef UAVCAN_EQUIPMENT_INDICATION_BEEPCOMMAND_SIGNATURE
#undef UAVCAN_EQUIPMENT_DEVICE_TEMPERATURE_SIGNATURE
#undef UAVCAN_EQUIPMENT_ESC_STATUS_SIGNATURE
#undef UAVCAN_EQUIPMENT_GNSS_FIX2_SIGNATURE
#undef UAVCAN_EQUIPMENT_HARDPOINT_STATUS_SIGNATURE
#undef UAVCAN_EQUIPMENT_ICE_RECIPROCATING_STATUS_SIGNATURE
#undef UAVCAN_EQUIPMENT_RANGE_SENSOR_MEASUREMENT_SIGNATURE
#include "libdcnode/pub.hpp"

#include "libdcnode/sub.hpp"

IntegerDesc_t __attribute__((weak)) integer_desc_pool[] = {
    {"uavcan.node.id", 0, 100, 50, true, false},
};
IntegerParamValue_t integer_values_pool[sizeof(integer_desc_pool) / sizeof(IntegerDesc_t)];

StringDesc_t __attribute__((weak)) string_desc_pool[] = {
    {"uavcan.node.description", "", true},
    {"system.name", "dronecan_application", true},
};
StringParamValue_t string_values_pool[sizeof(string_desc_pool) / sizeof(StringDesc_t)];

bool paramsIsInteger(ParamIndex_t param_idx)
{
    return paramsGetType(param_idx) == PARAM_TYPE_INTEGER;
}
bool paramsIsString(ParamIndex_t param_idx)
{
    return paramsGetType(param_idx) == PARAM_TYPE_STRING;
}
IntegerParamValue_t paramsGetIntegerMin(ParamIndex_t param_idx)
{
    auto desc = paramsGetIntegerDesc(param_idx);
    return desc != nullptr ? desc->min : 0;
}
IntegerParamValue_t paramsGetIntegerMax(ParamIndex_t param_idx)
{
    auto desc = paramsGetIntegerDesc(param_idx);
    return desc != nullptr ? desc->max : 0;
}
IntegerParamValue_t paramsGetIntegerDef(ParamIndex_t param_idx)
{
    auto desc = paramsGetIntegerDesc(param_idx);
    return desc != nullptr ? desc->def : 0;
}

/**
 * @brief Platform specific functions which should be provided by a user
 */
uint32_t platformSpecificGetTimeMs()
{
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto crnt_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(crnt_time - start_time).count();
}
bool platformSpecificRequestRestart()
{
    return false;
}
void platformSpecificReadUniqueID(uint8_t out_uid[16])
{
    memset(out_uid, 0x00, 16);
}

/**
 * @brief Application specific functions
 */
void rc1_callback(const uavcan_equipment_esc_RawCommand &msg)
{
    std::cout << "Get RawCommand 1 with " << (int)msg.cmd.len << " commands." << std::endl;
}
void rc2_callback(const uavcan_equipment_esc_RawCommand &msg)
{
    std::cout << "Get RawCommand 2 with " << (int)msg.cmd.len << " commands." << std::endl;
}

void ac1_callback(const uavcan_equipment_actuator_ArrayCommand &msg)
{
    std::cout << "Get ArrayCommand_t1 with " << msg.commands.len << "commands." << std::endl;
}
void ac2_callback(const uavcan_equipment_actuator_ArrayCommand &msg)
{
    std::cout << "Get ArrayCommand_t2 with " << msg.commands.len << "commands." << std::endl;
}
bool ac1_filter(const uavcan_equipment_actuator_ArrayCommand &msg)
{
    for (size_t idx = 0; idx < msg.commands.len; idx++)
    {
        if (msg.commands.data[idx].actuator_id == 0)
        {
            return true;
        }
    }
    return false;
}
bool ac2_filter(const uavcan_equipment_actuator_ArrayCommand &msg)
{
    for (size_t idx = 0; idx < msg.commands.len; idx++)
    {
        if (msg.commands.data[idx].actuator_id == 1)
        {
            return true;
        }
    }
    return false;
}

void lights_callback(const uavcan_equipment_indication_LightsCommand &msg)
{
    std::cout << "Get LightsCommand_t with " << msg.commands.len << " commands." << std::endl;
}

/**
 * @brief Main application entry point
 */
int main()
{

    paramsInit(1, 2, -1, 1);
    paramsResetToDefault();
    ParamsApi params_api = {
        .getName = paramsGetName,
        .isInteger = paramsIsInteger,
        .isString = paramsIsString,
        .find = paramsFind,
        .save = paramsSave,
        .resetToDefault = paramsResetToDefault,
        .integer = {
            .setValue = paramsSetIntegerValue,
            .getValue = paramsGetIntegerValue,
            .getMin = paramsGetIntegerMin,
            .getMax = paramsGetIntegerMax,
            .getDef = paramsGetIntegerDef,
        },
        .string = {
            .setValue = paramsSetStringValue,
            .getValue = paramsGetStringValue,
        },
    };

    PlatformApi platform_api{
        .getTimeMs = platformSpecificGetTimeMs,
        .requestRestart = platformSpecificRequestRestart,
        .readUniqueId = platformSpecificReadUniqueID,
        .can = {
            .init = canDriverInit,
            .recv = canDriverReceive,
            .send = canDriverTransmit,
            .getRxOverflowCount = canDriverGetRxOverflowCount,
            .getErrorCount = canDriverGetErrorCount,
        }};

    AppInfo app_info{
        .node_id = static_cast<uint8_t>(paramsGetIntegerValue(0)),
        .node_name = APP_NODE_NAME,
        .vcs_commit = GIT_HASH >> 32,
        .sw_version_major = APP_VERSION_MAJOR,
        .sw_version_minor = APP_VERSION_MINOR,
        .hw_version_major = HW_VERSION_MAJOR,
        .hw_version_minor = HW_VERSION_MINOR,
    };

    auto init_res = uavcanInitApplication(params_api, platform_api, &app_info);
    if (init_res < 0)
    {
        std::cout << "CAN interface could not be found. Exit with code " << init_res << std::endl;
        return init_res;
    }
    uint8_t sub_init = 0;
    uint8_t sub_fail = 0; 
    libdcnode::DronecanSub<uavcan_equipment_indication_LightsCommand> lights_command_sub;
    lights_command_sub.init(&lights_callback) >= 0 ? sub_init++ : sub_fail++;

    libdcnode::DronecanSub<uavcan_equipment_actuator_ArrayCommand> array_command_sub1;
    array_command_sub1.init(&ac1_callback, &ac1_filter) >= 0 ? sub_init++ : sub_fail++;

    libdcnode::DronecanSub<uavcan_equipment_actuator_ArrayCommand> array_command_sub2;
    array_command_sub2.init(&ac2_callback, &ac2_filter)>= 0 ? sub_init++ : sub_fail++;

    libdcnode::DronecanSub<uavcan_equipment_esc_RawCommand> raw_command_sub1;
    raw_command_sub1.init(&rc1_callback)>= 0 ? sub_init++ : sub_fail++;

    libdcnode::DronecanSub<uavcan_equipment_esc_RawCommand> raw_command_sub2;
    raw_command_sub2.init(&rc2_callback) >= 0 ? sub_init++ : sub_fail++; 
    std::cout<< "Subs initialized: " << sub_init << ". Subs failed: "<<sub_fail;
    libdcnode::DronecanPeriodicPub<uavcan_equipment_power_CircuitStatus> circuit_status(2.0f);
    libdcnode::DronecanPeriodicPub<uavcan_equipment_power_BatteryInfo> battery_info(1.0f);

    while (platformSpecificGetTimeMs() < 50000)
    {
        circuit_status.msg.voltage = 5.0;
        battery_info.msg.voltage = 5.1;

        circuit_status.spinOnce();

        battery_info.spinOnce();
        uavcanSpinOnce();
    }

    std::cout << "Good. Enough." << std::endl;
    return 0;
}
