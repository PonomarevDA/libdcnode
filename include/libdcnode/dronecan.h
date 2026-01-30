/*
 * Copyright (C) 2023 Dmitry Ponomarev <ponomarevda96@gmail.com>
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef LIBDCNODE_DRONECAN_H_
#define LIBDCNODE_DRONECAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "libdcnode/internal.h"
#include "libdcnode/legacy/uavcan/protocol/node_status.h"
#include "libdcnode/legacy/uavcan/protocol/get_node_info.h"
#include "params.h"
#include "version.h"
#include "platform.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DRONECAN_MAX_SUBS_NUMBER
#define DRONECAN_MAX_SUBS_NUMBER 15
#endif

  typedef struct
  {
    uint8_t node_id;
    const char *node_name;
    uint64_t vcs_commit;
    uint8_t sw_version_major;
    uint8_t sw_version_minor;
    uint8_t hw_version_major;
    uint8_t hw_version_minor;
  } AppInfo;

  /**
   * @brief Initialize the node and minimal required services
   * @return 0 on success, otherwise negative error
   */
  int16_t uavcanInitApplication(ParamsApi params_api, PlatformApi platform_api, const AppInfo *app_info);

  void uavcanSetNodeId(uint8_t node_id);
  uint8_t uavcanGetNodeId();

  /**
   * @brief Functions below should be called periodically to handle the application.
   */
  void uavcanSpinOnce();

  /**
   * @brief Call this function once per each subscriber.
   * The application will automatically handle callbacks.
   * Callbacks should end ASAP.
   */
  int8_t uavcanSubscribe(uint64_t signature,
                         uint16_t id,
                         void(callback)(CanardRxTransfer *transfer));

  /**
   * @brief Broadcast a message.
   */
  int16_t uavcanPublish(uint64_t data_type_signature,
                        uint16_t data_type_id,
                        uint8_t *inout_transfer_id,
                        uint8_t priority,
                        const void *payload,
                        uint16_t payload_len);

  /**
   * @brief Respond on RPC-request.
   */
  void uavcanRespond(CanardRxTransfer *transfer,
                     uint64_t data_type_signature,
                     uint16_t data_type_id,
                     const uint8_t *payload,
                     uint16_t len);

  /**
   * @brief NodeInfo API
   */
  void uavcanConfigure(const SoftwareVersion *new_sw_vers, const HardwareVersion *new_hw_vers);
  void uavcanSetNodeName(const char *new_node_name);

  /**
   * @note TransportStats API
   */
  void uavcanStatsIncreaseCanErrors();
  void uavcanStatsIncreaseCanTx(uint8_t num_of_transfers);
  void uavcanStatsIncreaseCanRx();
  void uavcanStatsIncreaseUartErrors();
  void uavcanStatsIncreaseUartTx(uint32_t num);
  void uavcanStatsIncreaseUartRx(uint32_t num);
  uint64_t uavcanGetErrorCount();

  /**
   * @brief NodeStatus API
   */
  void uavcanSetNodeHealth(NodeStatusHealth_t health);
  NodeStatusHealth_t uavcanGetNodeHealth();

  NodeStatusMode_t uavcanGetNodeStatusMode();
  void uavcanSetNodeStatusMode(NodeStatusMode_t mode);

  void uavcanSetVendorSpecificStatusCode(uint16_t vssc);

  const NodeStatus_t *uavcanGetNodeStatus();

#ifdef __cplusplus
}
#endif

#endif // LIBDCNODE_DRONECAN_H_
