/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-test printk lines for the on-target Unity runner. Unity still emits its
 * own ":PASS" / failure detail via UNITY_OUTPUT_CHAR (line-buffered in
 * unity_config.h); these macros add a readable index and a one-line verdict.
 */

#ifndef TEST_RUNNER_LOG_H_
#define TEST_RUNNER_LOG_H_

#include <stdint.h>

#include <zephyr/sys/printk.h>

#include "unity.h"
#include "unity_config.h"
#include "unity_internals.h"

#define TEST_FAIL_LOG_MAX    16
#define TEST_IGNORE_LOG_MAX  16

struct test_runner_log {
	uint8_t fail_count;
	uint8_t ignore_count;
	const char *fail_names[TEST_FAIL_LOG_MAX];
	const char *ignore_names[TEST_IGNORE_LOG_MAX];
};

static inline void test_runner_log_init(struct test_runner_log *log)
{
	log->fail_count = 0U;
	log->ignore_count = 0U;
}

static inline void test_runner_log_fail(struct test_runner_log *log, const char *name)
{
	if (log->fail_count < TEST_FAIL_LOG_MAX) {
		log->fail_names[log->fail_count++] = name;
	}
}

static inline void test_runner_log_ignore(struct test_runner_log *log, const char *name)
{
	if (log->ignore_count < TEST_IGNORE_LOG_MAX) {
		log->ignore_names[log->ignore_count++] = name;
	}
}

static inline void test_runner_log_print_summary(const struct test_runner_log *log,
						   int registered_count)
{
	uint8_t i;

	printk("\n========== TEST SUMMARY ==========\n");
	printk("Registered : %d\n", registered_count);
	printk("Executed   : %u\n", (unsigned int)Unity.NumberOfTests);
	printk("Failures   : %u\n", (unsigned int)Unity.TestFailures);
	printk("Ignored    : %u\n", (unsigned int)Unity.TestIgnores);

	if (log->fail_count > 0U) {
		printk("\nFailed tests:\n");
		for (i = 0U; i < log->fail_count; i++) {
			printk("  - %s\n", log->fail_names[i]);
		}
		if (Unity.TestFailures > log->fail_count) {
			printk("  (see Unity failure lines above for additional detail)\n");
		}
	}

	if (log->ignore_count > 0U) {
		printk("\nIgnored tests:\n");
		for (i = 0U; i < log->ignore_count; i++) {
			printk("  - %s\n", log->ignore_names[i]);
		}
	}

	printk("==================================\n\n");
	unity_output_flush();
}

#define RUN_TEST_LOG_COUNT(log, func, total)                                                       \
	do {                                                                                       \
		const uint16_t _idx = (uint16_t)(Unity.NumberOfTests + 1U);                        \
		const uint16_t _fail_before = (uint16_t)Unity.TestFailures;                        \
		const uint16_t _ign_before = (uint16_t)Unity.TestIgnores;                          \
                                                                                                   \
		printk("\n[%02u/%d] %s\n", _idx, (int)(total), #func);                               \
		unity_output_flush();                                                              \
		RUN_TEST(func);                                                                    \
		unity_output_flush();                                                              \
		if (Unity.TestIgnores > _ign_before) {                                             \
			printk("     => IGNORE\n");                                                \
			test_runner_log_ignore((log), #func);                                      \
		} else if (Unity.TestFailures > _fail_before) {                                    \
			printk("     => FAIL\n");                                                  \
			test_runner_log_fail((log), #func);                                        \
		} else {                                                                           \
			printk("     => PASS\n");                                                  \
		}                                                                                  \
		unity_output_flush();                                                              \
	} while (0)

#endif /* TEST_RUNNER_LOG_H_ */
