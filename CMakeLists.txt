# Copyright (c) 2023 Dmitry Ponomarev
# Distributed under the MPL v2.0 License, available in the file LICENSE.
# Author: Dmitry Ponomarev <ponomarevda96@gmail.com>

cmake_minimum_required(VERSION 3.15.3)
project(libdcnode VERSION 0.6.0 LANGUAGES C CXX)

#
# 1. Generate DSDL
#
set(DSDL_IN_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/Libs/DSDL/uavcan"
    "${CMAKE_CURRENT_LIST_DIR}/Libs/DSDL/ardupilot"
)
set(DSDL_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/libdcnode/serialization")
set(DSDL_COMPILER "${CMAKE_CURRENT_LIST_DIR}/Libs/dronecan_dsdlc/dronecan_dsdlc.py")
include(${CMAKE_CURRENT_LIST_DIR}/cmake/generate_dsdl.cmake)

#
# 2. Create library
#
file(GLOB SERIALIZATION_SOURCES CONFIGURE_DEPENDS
  "${DSDL_OUT_DIR}/src/*.c"
)
add_library(${PROJECT_NAME} STATIC
    ${CMAKE_CURRENT_LIST_DIR}/Libs/libcanard_v0/canard.c
    ${CMAKE_CURRENT_LIST_DIR}/src/dronecan.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/logger.cpp
    ${SERIALIZATION_SOURCES}
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})



target_include_directories(libdcnode PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/Libs
    ${DSDL_OUT_DIR}/include
)


set_source_files_properties(${SERIALIZATION_SOURCES}
    PROPERTIES
        LANGUAGE C
        COMPILE_FLAGS "-Wno-switch-default -fno-strict-aliasing -Wno-undef"
)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-address-of-packed-member>
)
