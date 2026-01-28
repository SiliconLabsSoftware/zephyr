# CMake integration for Simplicity SDK
#
# Copyright (c) 2025, Silicon Laboratories Inc.
#
# SPDX-License-Identifier: Apache-2.0

set(COMMON_DIR            ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/platform/common)
set(RADIO_DIR             ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/platform/radio)
set(EXTRA_RADIO_DIR       ${ZEPHYR_HAL_SILABS_EXTRA_MODULE_DIR}/simplicity_sdk/platform/radio)
set(OPENTHREAD_DIR        ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/protocol/openthread)
set(EXTRA_SILABS_CORE_DIR ${ZEPHYR_HAL_SILABS_EXTRA_MODULE_DIR}/simplicity_sdk/util/silicon_labs/silabs_core)

zephyr_library_named(openthread_platform_silabs)

dt_nodelabel(hfxo_path NODELABEL "hfxo")
dt_prop(hfxo_precision PATH ${hfxo_path} PROPERTY "precision")

zephyr_compile_definitions(
  # needed for alarm.c
  SL_OPENTHREAD_HFXO_ACCURACY=${hfxo_precision}
  # needed for radio_power_manager.c
  SL_OPENTHREAD_RAW_POWER_CALIBRATION_LENGTH=4
  SL_OPENTHREAD_FEM_SETTING_LENGTH=4
  # needed for radio.c
  SL_OPENTHREAD_CSL_TX_UNCERTAINTY=CONFIG_OPENTHREAD_PLATFORM_CSL_UNCERT
  SL_OPENTHREAD_RADIO_CCA_MODE=SL_RAIL_IEEE802154_CCA_MODE_RSSI
  SL_OPENTHREAD_RADIO_RX_BUFFER_COUNT=CONFIG_HEAP_MEM_POOL_SIZE
  MAX_QUEUE_LENGTH=CONFIG_HEAP_MEM_POOL_SIZE
)

zephyr_include_directories(
  ${COMMON_DIR}/inc
  ${EXTRA_RADIO_DIR}/mac/config
  ${RADIO_DIR}/rail_lib/protocol/ieee802154
  ${OPENTHREAD_DIR}/config
  ${OPENTHREAD_DIR}/platform-abstraction/efr32
  ${OPENTHREAD_DIR}/platform-abstraction/include
  ${ZEPHYR_OPENTHREAD_MODULE_DIR}/src/core
  ${ZEPHYR_OPENTHREAD_MODULE_DIR}/openthread/src/include
  ${ZEPHYR_OPENTHREAD_MODULE_DIR}/openthread
  ${ZEPHYR_BASE}/modules/openthread/platform
  ${EXTRA_SILABS_CORE_DIR}/queue
  ${ZEPHYR_MBEDTLS_MODULE_DIR}/include
  # Stub for syscalls/time_units.h when CONFIG_TIMER_READS_ITS_FREQUENCY_AT_RUNTIME is set
  ${OPENTHREAD_DIR}/stub_include
)

zephyr_library_sources(
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/radio.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/startup-gcc.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/radio_coex.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/radio_power_manager.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/alarm.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/ieee802154-packet-utils.cpp
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/soft_source_match_table.c
  ${OPENTHREAD_DIR}/platform-abstraction/efr32/sl_gp_interface.c
  ${EXTRA_SILABS_CORE_DIR}/queue/circular_queue.c
  zephyr.c
)

add_prebuilt_library(libsl_openthread_cm33_gcc protocol/openthread/platform-abstraction/efr32/libsl_openthread_cm33_gcc.a)

# Determine which libs should be linked in
set(ot_libs "")

list(APPEND ot_libs
  ot-config
  openthread-platform-utils-static
)

if(CONFIG_OPENTHREAD_FTD)
set(cli_lib openthread-cli-ftd)
elseif(CONFIG_OPENTHREAD_MTD)
set(cli_lib openthread-cli-mtd)
endif()

if(CONFIG_OPENTHREAD_SHELL)
list(APPEND ot_libs ${cli_lib})
endif()

if(CONFIG_OPENTHREAD_COPROCESSOR_RCP)
list(APPEND ot_libs openthread-rcp)
endif()

if(CONFIG_OPENTHREAD_COPROCESSOR_NCP)
if(CONFIG_OPENTHREAD_FTD)
list(APPEND ot_libs openthread-ncp-ftd)
elseif(CONFIG_OPENTHREAD_MTD)
list(APPEND ot_libs openthread-ncp-mtd)
endif()
endif()

if(NOT CONFIG_OPENTHREAD_COPROCESSOR_RCP)
if(CONFIG_OPENTHREAD_FTD)
list(APPEND ot_libs openthread-ftd)
elseif(CONFIG_OPENTHREAD_MTD)
list(APPEND ot_libs openthread-mtd)
endif()
endif()

if(CONFIG_HDLC_RCP_IF)
list(APPEND ot_libs
  openthread-platform
  openthread-radio-spinel
  openthread-spinel-ncp
  openthread-url
  openthread-hdlc
)
endif()

zephyr_link_libraries(${ot_libs})
