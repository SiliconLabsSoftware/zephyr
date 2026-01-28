/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Silicon Laboratories Inc.
 *
 * Stub for OpenThread/SiLabs build when Zephyr generated syscalls are not
 * available (e.g. OpenThread sources built with different include context).
 * Silicon Labs HAL provides this so compilation succeeds without modifying
 * Zephyr's include/zephyr/sys/time_units.h or the OpenThread module.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef Z_INCLUDE_SYSCALLS_TIME_UNITS_H
#define Z_INCLUDE_SYSCALLS_TIME_UNITS_H

#ifndef _ASMLANGUAGE

#include <zephyr/autoconf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * When CONFIG_TIMER_READS_ITS_FREQUENCY_AT_RUNTIME is set, time_units.h
 * includes this header and expects sys_clock_hw_cycles_per_sec_runtime_get().
 * In OpenThread build context we use the compile-time constant.
 */
static inline unsigned int sys_clock_hw_cycles_per_sec_runtime_get(void)
{
	return (unsigned int)SL_CLOCK_SYS_HZ;
}

#ifdef __cplusplus
}
#endif

#endif /* _ASMLANGUAGE */
#endif /* Z_INCLUDE_SYSCALLS_TIME_UNITS_H */
