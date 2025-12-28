/*
 * Copyright (C) 2023-2024 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef LIBDCNODE_PUBLISHER_HPP_
#define LIBDCNODE_PUBLISHER_HPP_

#include <stdint.h>
#include <algorithm>
#include "libdcnode/dronecan.h"
#include "libdcnode/legacy/uavcan/equipment/actuator/Status.h"
#include "libdcnode/legacy/uavcan/equipment/ahrs/MagneticFieldStrength2.h"
#include "libdcnode/legacy/uavcan/equipment/ahrs/RawImu.h"
#include "libdcnode/legacy/uavcan/equipment/ahrs/Solution.h"
#include "libdcnode/legacy/uavcan/equipment/air_data/IndicatedAirspeed.h"
#include "libdcnode/legacy/uavcan/equipment/air_data/RawAirData.h"
#include "libdcnode/legacy/uavcan/equipment/air_data/StaticPressure.h"
#include "libdcnode/legacy/uavcan/equipment/air_data/StaticTemperature.h"
#include "libdcnode/legacy/uavcan/equipment/air_data/TrueAirspeed.h"
#include "libdcnode/legacy/uavcan/equipment/esc/Status.h"
#include "libdcnode/legacy/uavcan/equipment/gnss/Fix2.h"
#include "libdcnode/legacy/uavcan/equipment/hardpoint/Status.h"
#include "libdcnode/legacy/uavcan/equipment/ice/FuelTankStatus.h"
#include "libdcnode/legacy/uavcan/equipment/ice/Status.h"
#include "libdcnode/legacy/uavcan/equipment/power/CircuitStatus.h"
#include "libdcnode/legacy/uavcan/equipment/power/BatteryInfo.h"
#include "libdcnode/legacy/uavcan/equipment/device/Temperature.h"
#include "libdcnode/legacy/dronecan/sensors/hygrometer/Hygrometer.h"
#include "libdcnode/legacy/uavcan/equipment/indication/LightsCommand.h"
#include "libdcnode/legacy/uavcan/equipment/range_sensor/Measurement.h"
#include "libdcnode/legacy/uavcan/com/rl/vibration/Measurement.h"

template <typename MessageType>
struct DronecanPublisherTraits;

#define DEFINE_PUBLISHER_TRAITS(MessageType, PublishFunction)                                 \
    template <>                                                                               \
    struct DronecanPublisherTraits<MessageType>                                               \
    {                                                                                         \
        static inline int8_t publish_once(const MessageType &msg, uint8_t *inout_transfer_id) \
        {                                                                                     \
            return PublishFunction(&msg, inout_transfer_id);                                  \
        }                                                                                     \
    };

DEFINE_PUBLISHER_TRAITS(ActuatorStatus_t, dronecan_equipment_actuator_status_publish)
DEFINE_PUBLISHER_TRAITS(MagneticFieldStrength2, dronecan_equipment_ahrs_magnetic_field_2_publish)
DEFINE_PUBLISHER_TRAITS(AhrsRawImu, dronecan_equipment_ahrs_raw_imu_publish)
DEFINE_PUBLISHER_TRAITS(AhrsSolution_t, dronecan_equipment_ahrs_solution_publish)
DEFINE_PUBLISHER_TRAITS(IndicatedAirspeed, dronecan_equipment_air_data_indicated_airspeed_publish)
DEFINE_PUBLISHER_TRAITS(RawAirData_t, dronecan_equipment_air_data_raw_air_data_publish)
DEFINE_PUBLISHER_TRAITS(StaticPressure, dronecan_equipment_air_data_static_pressure_publish)
DEFINE_PUBLISHER_TRAITS(StaticTemperature, dronecan_equipment_air_data_static_temperature_publish)
DEFINE_PUBLISHER_TRAITS(TrueAirspeed, dronecan_equipment_air_data_true_airspeed_publish)
DEFINE_PUBLISHER_TRAITS(EscStatus_t, dronecan_equipment_esc_status_publish)
DEFINE_PUBLISHER_TRAITS(GnssFix2, dronecan_equipment_gnss_fix2_publish)
DEFINE_PUBLISHER_TRAITS(HardpointStatus, dronecan_equipment_hardpoint_status_publish)
DEFINE_PUBLISHER_TRAITS(FuelTankStatus_t, dronecan_equipment_ice_fuel_tank_status_publish)
DEFINE_PUBLISHER_TRAITS(IceReciprocatingStatus, dronecan_equipment_ice_status_publish)
DEFINE_PUBLISHER_TRAITS(CircuitStatus_t, dronecan_equipment_circuit_status_publish)
DEFINE_PUBLISHER_TRAITS(Temperature_t, dronecan_equipment_temperature_publish)
DEFINE_PUBLISHER_TRAITS(BatteryInfo_t, dronecan_equipment_battery_info_publish)
DEFINE_PUBLISHER_TRAITS(Hygrometer, dronecan_sensors_hygrometer_hygrometer_publish)
DEFINE_PUBLISHER_TRAITS(LightsCommand_t, dronecan_equipment_indication_lights_command_publish)
DEFINE_PUBLISHER_TRAITS(RangeSensorMeasurement_t,
                        dronecan_equipment_range_sensor_measurement_publish)
DEFINE_PUBLISHER_TRAITS(Mpu_vibration,
                        dronecan_mpu_vibration_publish)

template <typename MessageType>
class DronecanPublisher
{
public:
    DronecanPublisher() = default;

    inline void publish()
    {
        DronecanPublisherTraits<MessageType>::publish_once(msg, &inout_transfer_id);
        inout_transfer_id++;
    }

    MessageType msg;

private:
    uint8_t inout_transfer_id;
};

template <typename MessageType>
class DronecanPeriodicPublisher : public DronecanPublisher<MessageType>
{
public:
    explicit DronecanPeriodicPublisher(float frequency) :
        DronecanPublisher<MessageType>(),
        PUB_PERIOD_MS(static_cast<uint32_t>(1000.0f / std::clamp(frequency, 0.001f, 1000.0f))) {};

    inline void spinOnce()
    {
        auto crnt_time_ms = libdcnode::getPlatformApi().getTimeMs();
        if (crnt_time_ms < next_pub_time_ms)
        {
            return;
        }
        next_pub_time_ms = crnt_time_ms + PUB_PERIOD_MS;

        this->publish();
    }

private:
    const uint32_t PUB_PERIOD_MS;
    uint32_t next_pub_time_ms{500};
};

#endif // LIBDCNODE_PUBLISHER_HPP_
