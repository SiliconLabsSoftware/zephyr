/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Unity configuration for the Zephyr / xG27-DK2602A on-target test app.
 *
 * - Route every Unity character through printk() so the test report lands
 *   on the on-board VCOM UART without any extra wiring.
 * - Disable floating point support; none of the I2S driver assertions need
 *   it and excluding it shrinks the binary on the small EFR32 part.
 */

#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#include <zephyr/sys/printk.h>

/* Unity hands us an int (the C standard putchar prototype). printk's "%c"
 * conversion expects an int as well, so a plain forward is safe.
 *
 * UNITY_OUTPUT_CHAR is intentionally the only output hook we override --
 * we leave UNITY_OUTPUT_CHAR_HEADER_DECLARATION undefined so unity.c does
 * NOT emit an extern prototype for a non-existent helper symbol. */
#define UNITY_OUTPUT_CHAR(a)                  printk("%c", (int)(a))

/* Trim binary size: this driver's test plan never touches floats/doubles. */
#define UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_DOUBLE

#endif /* UNITY_CONFIG_H */
