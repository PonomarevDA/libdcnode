# Copyright (c) 2025 Dmitry Ponomarev
# Distributed under the MPL v2.0 License, available in the file LICENSE.
# Author: Dmitry Ponomarev <ponomarevda96@gmail.com>

cmake_minimum_required(VERSION 3.15.3)

set(_run_dsdlc FALSE)

if(NOT IS_DIRECTORY "${DSDL_OUT_DIR}")
    set(_run_dsdlc TRUE)
else()
    file(GLOB _dsdl_out_contents
        LIST_DIRECTORIES TRUE
        RELATIVE "${DSDL_OUT_DIR}"
        "${DSDL_OUT_DIR}/*")
    if(_dsdl_out_contents STREQUAL "")
        set(_run_dsdlc TRUE)
    endif()
endif()

if(_run_dsdlc)
    message(STATUS "Generating DSDL sources into: ${DSDL_OUT_DIR}")
    execute_process(
        COMMAND ${DSDL_COMPILER} -O ${DSDL_OUT_DIR} ${DSDL_IN_DIR}
        RESULT_VARIABLE ret
    )
    if(NOT ret EQUAL 0)
        message( FATAL_ERROR "DSDL Generator failed. Abort.")
    endif()
else()
    message(STATUS "DSDL output exists and is not empty, skipping generation: ${DSDL_OUT_DIR}")
endif()

# Fix includes to use local libcanard_v0 path
file(GLOB_RECURSE _dc_gen_headers "${DSDL_OUT_DIR}/include/*.h")
foreach(_h ${_dc_gen_headers})
    file(READ "${_h}" _txt)
    string(REPLACE "#include <canard.h>" "#include \"libcanard_v0/canard.h\"" _new_txt "${_txt}")
    if(NOT "${_txt}" STREQUAL "${_new_txt}")
        file(WRITE "${_h}" "${_new_txt}")
    endif()
endforeach()

