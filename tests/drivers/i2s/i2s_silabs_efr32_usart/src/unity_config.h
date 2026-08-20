/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNITY_CONFIG_H_
#define UNITY_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/printk.h>

/* Shorter Unity pass/fail prefix than __FILE__ (see UnityBegin() in test_runner.c). */
#define UNITY_OUTPUT_FOR_ECLIPSE

#define UNITY_OUTPUT_CHAR(c) unity_putchar((char)(c))
#define UNITY_OUTPUT_FLUSH() unity_output_flush()

#define UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_DOUBLE

#define UNITY_LINE_BUF_SIZE 160

static char unity_line_buf[UNITY_LINE_BUF_SIZE];
static size_t unity_line_len;

static inline void unity_output_flush(void)
{
	if (unity_line_len > 0U) {
		unity_line_buf[unity_line_len] = '\0';
		printk("%s\n", unity_line_buf);
		unity_line_len = 0U;
	}
}

static inline void unity_putchar(char c)
{
	if (c == '\n') {
		unity_output_flush();
		return;
	}

	if (unity_line_len + 1U >= UNITY_LINE_BUF_SIZE) {
		unity_output_flush();
	}

	unity_line_buf[unity_line_len++] = c;
}

#endif /* UNITY_CONFIG_H_ */
