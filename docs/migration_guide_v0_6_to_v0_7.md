# libdcnode v0.6.x → v0.7.x Migration Guide

This guide covers changes introduced in v0.7.0 and verifies that v0.7.1 and v0.7.2 do not add additional breaking API changes. It focuses on upgrade steps, build impacts, and code updates needed to move from v0.6.x to v0.7.x safely.

## Summary

v0.7.x introduces build-time DSDL code generation and reorganizes DSDL headers. The core C API is still present, but the library now requires a C++ toolchain because `src/dronecan.c` became `src/dronecan.cpp` and `include/libdcnode/dronecan.h` now includes `include/libdcnode/platform.hpp` (C++). Existing serialization headers were moved under `include/libdcnode/legacy`, and new generated message types are provided via `dronecan_msgs.h`.

## Breaking Changes

1. C++ is now required
   The core implementation is compiled as C++ (`src/dronecan.cpp`) and `dronecan.h` includes `platform.hpp`, which uses C++ constructs. Pure C-only builds that previously included `dronecan.h` will fail. Your application must be compiled as C++ (or at least any translation unit including `dronecan.h`).

2. DSDL headers moved to legacy paths
   All manually maintained DSDL headers previously under `include/libdcnode/uavcan/...` and `include/libdcnode/dronecan/...` were moved under `include/libdcnode/legacy/...`. Any includes that use the old paths must be updated.

3. New generated DSDL types and field layout
   v0.7.x generates DSDL messages at configure time via `Libs/dronecan_dsdlc`. The generated types use different names and struct layouts (e.g., dynamic arrays become `{ .len, .data }`). If you switch to generated types, you must update message types and field access.

4. Macro name collisions if you mix legacy and generated headers
   Some generated headers define the same macro names as legacy headers. If you include both, you can get redefinition conflicts. Choose one serialization path per translation unit. The Ubuntu example uses `#undef` as a temporary workaround; you should avoid mixing where possible.

## Migration Steps

1. Update submodules
   The DSDL source tree and compiler are now submodules. Ensure they are present:

```bash
git submodule update --init --recursive
```

2. Build system updates
   v0.7.x generates serialization during CMake configure. `CMakeLists.txt` already wires this up when you add the library via `add_subdirectory(libdcnode)`. Make sure Python 3 is available and `requirements.txt` is installed if you run tests or regenerate code.

3. Pick a serialization path

Option A. Use generated DSDL (recommended for v0.7.x)
- Include `dronecan_msgs.h` (generated in the build tree and added to include paths by CMake).
- Use the new C++ wrappers in `include/libdcnode/pub.hpp` and `include/libdcnode/sub.hpp`.
- Update types and field access to generated layout.

Option B. Keep legacy serialization (minimal code changes)
- Update includes from `libdcnode/uavcan/...` to `libdcnode/legacy/uavcan/...`.
- Update includes from `libdcnode/dronecan/...` to `libdcnode/legacy/dronecan/...`.
- Continue to use `include/libdcnode/publisher.hpp` and `include/libdcnode/subscriber.hpp`.
- Do not include `dronecan_msgs.h` in the same translation unit to avoid macro conflicts.

4. Update platform API includes
   The platform API types now live in `include/libdcnode/platform.hpp`. This header is included by `dronecan.h`, so any file that includes `dronecan.h` must be compiled as C++.

## Code Update Examples

### Example 1: ArrayCommand fields and types

v0.6.x (legacy types)

```cpp
ArrayCommand_t msg;
for (size_t i = 0; i < msg.size; i++) {
    if (msg.commads[i].actuator_id == 0) {
        // ...
    }
}
```

v0.7.x (generated types)

```cpp
uavcan_equipment_actuator_ArrayCommand msg;
for (size_t i = 0; i < msg.commands.len; i++) {
    if (msg.commands.data[i].actuator_id == 0) {
        // ...
    }
}
```

### Example 2: RawCommand size access

v0.6.x

```cpp
void rc_callback(const RawCommand_t& msg) {
    std::cout << msg.size << std::endl;
}
```

v0.7.x

```cpp
void rc_callback(const uavcan_equipment_esc_RawCommand& msg) {
    std::cout << msg.cmd.len << std::endl;
}
```

### Example 3: New pub/sub wrappers

v0.6.x

```cpp
DronecanSubscriber<RawCommand_t> raw_command_sub;
raw_command_sub.init(&rc_callback);

DronecanPeriodicPublisher<CircuitStatus_t> circuit_status(2.0f);
```

v0.7.x

```cpp
libdcnode::DronecanSub<uavcan_equipment_esc_RawCommand> raw_command_sub;
raw_command_sub.init(&rc_callback);

libdcnode::DronecanPeriodicPub<uavcan_equipment_power_CircuitStatus> circuit_status(2.0f);
```

## v0.7.1 and v0.7.2 Notes

No additional breaking API changes were introduced after v0.7.0. Changes in v0.7.1 and v0.7.2 are build and safety improvements:
- DSDL generation now avoids rewriting files unnecessarily and fixes the `canard.h` include path.
- `DRONECAN_MAX_SUBS_NUMBER` default increased to 15.
- New pub/sub wrappers (`pub.hpp`, `sub.hpp`) gained minor improvements and safer subscription handling.
- README and example updates only.

## Upgrade Checklist

- Ensure a C++ compiler is used for any translation unit including `libdcnode/dronecan.h`.
- Update submodules and ensure the DSDL generator is present.
- Decide on generated DSDL vs legacy headers and update includes accordingly.
- If using generated DSDL, update message types and field access for dynamic arrays.
- Avoid mixing legacy headers and generated headers in the same translation unit.
- Rebuild and run the Ubuntu example or your tests to validate NodeStatus/GetNodeInfo behavior.
