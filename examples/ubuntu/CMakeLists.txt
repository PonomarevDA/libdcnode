# Copyright (c) 2023-2024 Dmitry Ponomarev
# Distributed under the MPL v2.0 License, available in the file LICENSE.
# Author: Dmitry Ponomarev <ponomarevda96@gmail.com>

cmake_minimum_required(VERSION 3.15.3)
project(ubuntu CXX C ASM)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH EXAMPLES_DIR)
cmake_path(GET EXAMPLES_DIR PARENT_PATH ROOT_DIR)

# 1. libdcnode
add_subdirectory(${ROOT_DIR} ${CMAKE_BINARY_DIR}/libdcnode)

# 2. libcanver
set(CAN_PLATFORM socketcan) # Options: bxcan, fdcan or socketcan
include(${ROOT_DIR}/platform_specific/${CAN_PLATFORM}/config.cmake)

# 3. libparams
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${ROOT_DIR}/build/external")
FetchContent_Declare(
    libparams
    GIT_REPOSITORY https://github.com/PonomarevDA/libparams.git
    GIT_TAG v0.8.4
    GIT_SHALLOW TRUE
)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(libparams)
if(NOT libparams_POPULATED)
    FetchContent_Populate(libparams)
endif()
set(LIBPARAMS_PLATFORM    ubuntu)
include(${ROOT_DIR}/build/external/libparams-src/CMakeLists.txt)

# 4. Create application
execute_process(
    COMMAND git rev-parse --short=16 HEAD
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_VARIABLE GIT_HASH_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(GIT_HASH "0x${GIT_HASH_SHORT}")
add_definitions(-DGIT_HASH=${GIT_HASH})
add_definitions(-DAPP_VERSION_MAJOR=0)
add_definitions(-DAPP_VERSION_MINOR=1)
add_definitions(-DHW_VERSION_MAJOR=0)
add_definitions(-DHW_VERSION_MINOR=0)
add_definitions(-DAPP_NODE_NAME="com.example.libdcnode.sitl")

add_executable(${PROJECT_NAME}
    main.cpp
    ${DRONECAN_PLATFORM_SOURCES}
    ${libparamsSrc}
)
target_include_directories(${PROJECT_NAME} PRIVATE
    .
    ${libparamsHeaders}
)
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wfloat-equal
    -Werror
    -Wundef
    -Wshadow
    -Wpointer-arith
    -Wunreachable-code
    -Wstrict-overflow=5
    -Wwrite-strings
    -Wswitch-default
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    libdcnode::libdcnode
)
