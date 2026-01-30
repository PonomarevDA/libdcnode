# libdcnode [![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=PonomarevDA_libdcnode&metric=code_smells)](https://sonarcloud.io/summary/new_code?id=PonomarevDA_libdcnode) [![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=PonomarevDA_libdcnode&metric=ncloc)](https://sonarcloud.io/summary/new_code?id=PonomarevDA_libdcnode) [![build](https://github.com/PonomarevDA/libdcnode/actions/workflows/build.yml/badge.svg)](https://github.com/PonomarevDA/libdcnode/actions/workflows/build.yml)

Lightweight [DroneCAN / UAVCAN v0](https://dronecan.github.io/) node toolkit built on top of [libcanard](https://github.com/dronecan/libcanard), [platform_specific](platform_specific) and [libparams](https://github.com/PonomarevDA/libparams). It wraps the transport, exposes a small C API for node setup/spin, and offers C++ helpers for typed publishers/subscribers and logging.

## Features

- Implements core DroneCAN services out of the box: [NodeStatus](https://legacy.uavcan.org/Specification/7._List_of_standard_data_types/#nodestatus), [GetNodeInfo](https://legacy.uavcan.org/Specification/7._List_of_standard_data_types/#getnodeinfo), [Param.GetSet and Param.ExecuteOpcode](https://legacy.uavcan.org/Specification/7._List_of_standard_data_types/#uavcanprotocolparam), [RestartNode](https://legacy.uavcan.org/Specification/7._List_of_standard_data_types/#restartnode), and [GetTransportStats](https://legacy.uavcan.org/Specification/7._List_of_standard_data_types/#gettransportstats).
- Platform abstraction (`include/libdcnode/platform.hpp`) so you can plug in your own timing, reset, unique-ID, and CAN driver callbacks.
- Templated C++ wrappers for publishing/subscribing (`include/libdcnode/pub.hpp`, `include/libdcnode/sub.hpp`) with optional filters and periodic senders; legacy interfaces remain in `include/libdcnode/publisher.hpp` and `include/libdcnode/subscriber.hpp`.
- Debug logger (`include/libdcnode/logger.hpp`) that publishes `uavcan.protocol.debug.LogMessage` with severity helpers.
- DSDL compiler baked into the build (`Libs/dronecan_dsdlc`) so message code is generated during CMake configure.

## Prerequisites

- C/C++ toolchain with C++17, CMake >= 3.15, Python 3 (for tests/tools).
- Linux SocketCAN stack and `ip` utilities (used by `scripts/vcan.sh`).
- `can-utils` (for monitoring) and `dronecan_gui_tool` are handy when playing with the examples.

## Getting started

Set up the Python deps (used by the DSDL generator and tests):

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

Then wire the library into your project:

```cmake
add_subdirectory(libdcnode)
target_link_libraries(my_app PRIVATE libdcnode::libdcnode)
```

During configure, DSDL code is generated into `build/generated/libdcnode/serialization` using `Libs/dronecan_dsdlc/dronecan_dsdlc.py`. If you add custom DSDL namespaces, point `DSDL_IN_DIR` (see `CMakeLists.txt`) to your set or rerun `scripts/code_generation.sh` with your paths.

Provide the platform and params hooks from `include/libdcnode/platform.hpp` and `include/libdcnode/params.hpp` in your application; the Ubuntu example shows minimal implementations.

Number of subscribers is limited by parameter `DRONECAN_MAX_SUBS_NUMBER` in file `include/libdcnode/dronecan.h` to 15. You may modify this parameter for further development, however note that size of each subscriber is limited to 16 or 24 bytes depending on 32 or 64 bit device.
### Ubuntu example (build, link, run)

The `examples/ubuntu` target demonstrates the C API plus the modern C++ pub/sub wrappers end-to-end.

```bash
# Prepare a virtual CAN iface once (uses sudo):
scripts/vcan.sh slcan0

# Build and run the example:
make ubuntu
```

What it does: sets `uavcan.node.id` (default 50) and `system.name`, wires platform callbacks (time, restart request, UID, CAN driver) in `examples/ubuntu/main.cpp`, subscribes to `esc.RawCommand`/`actuator.ArrayCommand`/`indication.LightsCommand`, and publishes periodic `equipment.power.CircuitStatus` + `equipment.power.BatteryInfo` while serving NodeStatus/GetNodeInfo.

In gui_tool you will see:

<img src="https://raw.githubusercontent.com/wiki/PonomarevDA/dronecan_application/assets/ubuntu_minimal.gif" alt="drawing">


<img src="https://raw.githubusercontent.com/wiki/PonomarevDA/dronecan_application/assets/ubuntu_publisher.gif" alt="drawing">


## Testing

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements-dev.txt
scripts/vcan.sh slcan0  # once, requires sudo
pytest tests/smoke_socketcan.py
```

The smoke tests expect the Ubuntu example (node ID 50 on `slcan0`) to be running; they verify `NodeStatus` and `GetNodeInfo` responses via the `dronecan` Python client.

## Changelog

| Version | ReleaseDate | Note |
| ------- | ------------ | ---- |
| [v0.7.0](https://github.com/PonomarevDA/libdcnode/tree/v0.7.0) | 2025-12-28 | Added DSDL ser/des generator |
| [v0.6.0](https://github.com/PonomarevDA/libdcnode/tree/v0.6.0) | 2025-10-17 | Build model changed: standalone library; platform hooks provided by user (no more source-include mode). |
| [v0.5.0](https://github.com/PonomarevDA/libdcnode/tree/v0.5.0) | 2024-09-26 | Decoupled the platform specific functions from the library |
| [v0.4.0](https://github.com/PonomarevDA/libdcnode/tree/v0.4.0) | 2024-07-29 | Added macro helpers for pub and sub traits |
| [v0.3.*](https://github.com/PonomarevDA/libdcnode/tree/v0.3.0) | 2024-01-06 | Incremental serialization additions |
| [v0.2.0](https://github.com/PonomarevDA/libdcnode/tree/v0.2.0) | 2024-01-06 | Add bxcan and fdcan drivers |
| [v0.1.0](https://github.com/PonomarevDA/libdcnode/tree/v0.1.0) | 2023-12-22 | First public drop; pure C, manual serialization, and source-include usage (include the C sources into your app). |

## License

The software is distributed under term of MPL v2.0 license.
