#ifndef LIBDCNODE_PLATFORM_SPECIFIC_FDCAN_CONFIG_H_
#define LIBDCNODE_PLATFORM_SPECIFIC_FDCAN_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FDCAN_HandleTypeDef* handle;
    uint8_t interface_id;
} DronecanFdcanInterfaceConfig;

int16_t dronecanFdcanConfigure(const DronecanFdcanInterfaceConfig* interfaces,
                               size_t interface_count);

#ifdef __cplusplus
}
#endif

#endif  // LIBDCNODE_PLATFORM_SPECIFIC_FDCAN_CONFIG_H_
