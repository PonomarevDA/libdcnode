# libdcnode v0.5.x → v0.6.x Migration Guide

This guide summarizes the breaking changes between v0.5.x and v0.6.0 and provides concrete migration steps and code updates. The key theme is the **new standalone library build model** and **explicit platform/params hooks**.

## Summary

v0.6.0 turns libdcnode into a standalone CMake library with explicit integration points. The previous “source-include” model is removed. Your application must now:

- `add_subdirectory(libdcnode)` and link `libdcnode::libdcnode`.
- Provide a `ParamsApi` and `PlatformApi` at runtime.
- Include headers via the new `libdcnode/...` paths.

## Breaking Changes

1. **Build integration changed**
   v0.5.x expected you to include libdcnode sources directly in your app and set CMake variables. In v0.6.0, libdcnode is a standalone static library with its own `CMakeLists.txt` and exported target `libdcnode::libdcnode`.

2. **New `uavcanInitApplication` signature**
   v0.5.x:
   `int16_t uavcanInitApplication(uint8_t node_id);`

   v0.6.x:
   `int16_t uavcanInitApplication(ParamsApi params_api, PlatformApi platform_api, const AppInfo* app_info);`

3. **Explicit platform hooks**
   The platform functions are now provided via `PlatformApi` (function pointers for time, restart, UID, CAN driver). The weak default implementations that existed in `src/weak.c` are no longer linked.

4. **Explicit params API**
   Parameters are now accessed via a function table (`ParamsApi`) instead of global functions. The library no longer reaches into the app’s `params.*` globals directly.

5. **Header paths moved under `include/libdcnode`**
   All public headers are now under `include/libdcnode/...` and should be included as `#include "libdcnode/..."`.

## Migration Steps

1. **Update CMake integration**
   v0.5.x used a variable-based model; v0.6.0 uses `add_subdirectory` and linking:

```cmake
# libdcnode
add_subdirectory(${ROOT_DIR} ${CMAKE_BINARY_DIR}/libdcnode)

# platform config
set(CAN_PLATFORM socketcan) # bxcan, fdcan, socketcan
include(${ROOT_DIR}/platform_specific/${CAN_PLATFORM}/config.cmake)

# libparams
set(LIBPARAMS_PLATFORM ubuntu) # stm32f103, stm32g0b1, ubuntu
include(libparams.cmake)

# application target
add_executable(${PROJECT_NAME} ... ${DRONECAN_PLATFORM_SOURCES} ...)
target_include_directories(${PROJECT_NAME} PRIVATE ... ${DRONECAN_PLATFORM_HEADERS} ...)
target_link_libraries(${PROJECT_NAME} PRIVATE libdcnode::libdcnode)
```

2. **Update includes**

```cpp
// v0.5.x
#include "dronecan.h"
#include "publisher.hpp"
#include "subscriber.hpp"

// v0.6.x
#include "libdcnode/dronecan.h"
#include "libdcnode/publisher.hpp"
#include "libdcnode/subscriber.hpp"
#include "libdcnode/can_driver.h"
```

3. **Implement `ParamsApi` and `PlatformApi`**
   v0.6.0 requires explicit API structs at init time. The Ubuntu example in `examples/ubuntu/main.cpp` shows the full wiring.

```cpp
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

PlatformApi platform_api = {
    .getTimeMs = platformSpecificGetTimeMs,
    .requestRestart = platformSpecificRequestRestart,
    .readUniqueId = platformSpecificReadUniqueID,
    .can = {
        .init = canDriverInit,
        .recv = canDriverReceive,
        .send = canDriverTransmit,
        .getRxOverflowCount = canDriverGetRxOverflowCount,
        .getErrorCount = canDriverGetErrorCount,
    },
};

AppInfo app_info = {
    .node_id = node_id,
    .node_name = APP_NODE_NAME,
    .vcs_commit = GIT_HASH >> 32,
    .sw_version_major = APP_VERSION_MAJOR,
    .sw_version_minor = APP_VERSION_MINOR,
    .hw_version_major = HW_VERSION_MAJOR,
    .hw_version_minor = HW_VERSION_MINOR,
};

uavcanInitApplication(params_api, platform_api, &app_info);
```

4. **Provide platform callbacks**
   The app must implement:

```cpp
uint32_t platformSpecificGetTimeMs();
bool platformSpecificRequestRestart();
void platformSpecificReadUniqueID(uint8_t out_uid[16]);
```

No weak defaults are linked in v0.6.0.

## v0.6.0 Behavior Changes to Note

- Node info data (name, SW/HW versions, VCS commit) now comes from `AppInfo` rather than preprocessor macros in libdcnode.
- Param and platform access are delegated through function tables instead of direct function calls.

## Upgrade Checklist

- Convert your build to `add_subdirectory` + `target_link_libraries(... libdcnode::libdcnode)`.
- Update header include paths to `libdcnode/...`.
- Provide `ParamsApi` and `PlatformApi` at `uavcanInitApplication()`.
- Implement required platform callbacks (time, restart, UID).
- Ensure params wiring matches your existing `libparams` usage.
