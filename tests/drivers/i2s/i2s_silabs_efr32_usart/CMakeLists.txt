# Copyright (c) 2026 Silicon Laboratories Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2s_silabs_efr32_usart_unity)

# Unity is fetched by west into <workspace>/modules/lib/unity (submanifests/unity.yaml).
# Use the CMake variable ZEPHYR_BASE (set by find_package(Zephyr)), not $ENV{ZEPHYR_BASE}:
# west build often leaves the shell env unset, which breaks $ENV{ZEPHYR_BASE}/../...
if(DEFINED WEST_TOPDIR AND NOT "${WEST_TOPDIR}" STREQUAL "")
  set(UNITY_DIR "${WEST_TOPDIR}/modules/lib/unity/src")
else()
  get_filename_component(UNITY_DIR "${ZEPHYR_BASE}/../modules/lib/unity/src" ABSOLUTE)
endif()

if(NOT EXISTS "${UNITY_DIR}/unity.c")
  message(FATAL_ERROR
    "Unity not found at ${UNITY_DIR}/unity.c\n"
    "From your west workspace (e.g. projects/), run: west update")
endif()

target_include_directories(app PRIVATE ${UNITY_DIR} ${CMAKE_CURRENT_LIST_DIR}/src)
target_compile_definitions(app PRIVATE UNITY_INCLUDE_CONFIG_H)
target_sources(app PRIVATE
    ${UNITY_DIR}/unity.c
    src/test_i2s_efr32_common.c
    src/test_runner.c
    src/test_i2s_efr32_configure.c
    src/test_i2s_efr32_write.c
    src/test_i2s_efr32_read.c
    src/test_i2s_efr32_trigger.c
    src/test_i2s_efr32_sign_extension.c
    src/test_clock_measure.c
)
