/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr entry point for the Unity-driven i2s_silabs_efr32_usart on-target
 * test app.
 *
 * Test groups (stereo-only driver, channels=2):
 *   TEST_GROUP(i2s_efr32_configure)  - i2s_configure() accept/reject paths
 *   TEST_GROUP(i2s_efr32_config_get) - i2s_config_get() readback semantics
 *   TEST_GROUP(i2s_efr32_states)     - i2s_trigger() state machine
 *   TEST_GROUP(i2s_efr32_io)         - i2s_write()/i2s_read() validation
 *   TEST_GROUP(i2s_efr32_errors)     - ISR-driven ERROR + PREPARE recovery
 *   TEST_GROUP(i2s_efr32_clock)      - USART CLKDIV sanity (Silabs-specific)
 *   TEST_GROUP(i2s_efr32_mclk)       - external MCLK via CMU clkout + PRS
 *                                      capture (auto-skips if no clkout in DT)
 *   TEST_GROUP(i2s_efr32_loopback)   - end-to-end TX -> wire -> RX, channels=2
 *
 * Per-test cleanup (re-deconfigure the device before each test) is done in
 * each TEST_SETUP() / TEST_TEAR_DOWN() via i2s_efr32_test_reset_device().
 * The one-shot suite setup (verify device pointer / ready) runs here in
 * main() before UnityMain(), since Unity Fixture has no per-suite hook.
 */

#include "common.h"
#include "unity_fixture.h"

#include <stdio.h>
#include <zephyr/autoconf.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define UNITY_MAX_ARGV 12

static void RunAllTests(void)
{
	RUN_TEST_GROUP(i2s_efr32_configure);
	RUN_TEST_GROUP(i2s_efr32_config_get);
	RUN_TEST_GROUP(i2s_efr32_states);
	RUN_TEST_GROUP(i2s_efr32_io);
	RUN_TEST_GROUP(i2s_efr32_errors);
	RUN_TEST_GROUP(i2s_efr32_clock);
	RUN_TEST_GROUP(i2s_efr32_mclk);
#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK
	RUN_TEST_GROUP(i2s_efr32_loopback);
#endif
}

/* Translate the I2S_EFR32_TEST_* Kconfig knobs into the argv that
 * UnityMain's CLI parser expects. Returns argc. argv[0] is the program
 * name slot Unity ignores. */
static int build_unity_argv(const char **argv, char *repeat_buf,
			    size_t repeat_buf_sz)
{
	int argc = 0;

	argv[argc++] = "i2s_efr32_unity";

#ifdef CONFIG_I2S_EFR32_TEST_VERBOSE
	argv[argc++] = "-v";
#endif
#ifdef CONFIG_I2S_EFR32_TEST_SILENT
	argv[argc++] = "-s";
#endif

	if (sizeof(CONFIG_I2S_EFR32_TEST_GROUP_FILTER) > 1) {
		argv[argc++] = "-g";
		argv[argc++] = CONFIG_I2S_EFR32_TEST_GROUP_FILTER;
	}
	if (sizeof(CONFIG_I2S_EFR32_TEST_NAME_FILTER) > 1) {
		argv[argc++] = "-n";
		argv[argc++] = CONFIG_I2S_EFR32_TEST_NAME_FILTER;
	}
	if (CONFIG_I2S_EFR32_TEST_REPEAT > 1) {
		snprintf(repeat_buf, repeat_buf_sz, "%d",
			 CONFIG_I2S_EFR32_TEST_REPEAT);
		argv[argc++] = "-r";
		argv[argc++] = repeat_buf;
	}

	return argc;
}

static void print_runner_banner(int argc, const char **argv)
{
	printk("\n*** I2S Silabs EFR32 USART driver - Unity test runner ***\n");
	printk("Device: %s @ alias i2s-node0\n", dev_i2s->name);
	printk("Driver mode: STEREO (channels=2, I2SCTRL.MONO=0, DMASPLIT=0)\n");
	printk("Frame clock: %d Hz, slab depth: tx=8 rx=8\n",
	       (int)CONFIG_I2S_EFR32_TEST_FRAME_CLK_HZ);
#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK
#if defined(CONFIG_BOARD_XG27_RB4194A)
	printk("Loopback group: ENABLED (RB4194A: jumper USART0 I2S TX->RX **PA8->PA7**)\n");
#else
	printk("Loopback group: ENABLED (jumper USART0 TX->RX: PC0<->PC1 on DK2602A)\n");
#endif
#else
	printk("Loopback group: disabled (build with "
	       "CONFIG_I2S_EFR32_TEST_LOOPBACK=y to enable)\n");
#endif
#ifdef CONFIG_I2S_EFR32_TEST_VERBOSE
	printk("Output mode: VERBOSE (-v) - one line per test\n");
#elif defined(CONFIG_I2S_EFR32_TEST_SILENT)
	printk("Output mode: SILENT (-s) - failures + summary only\n");
#else
	printk("Output mode: default - one '.' per pass, 'F' per fail.\n"
	       "  Enable CONFIG_I2S_EFR32_TEST_VERBOSE=y for per-test names,\n"
	       "  or CONFIG_I2S_EFR32_TEST_SILENT=y to suppress dots.\n");
#endif
	if (sizeof(CONFIG_I2S_EFR32_TEST_GROUP_FILTER) > 1) {
		printk("Group filter (-g): \"%s\"\n",
		       CONFIG_I2S_EFR32_TEST_GROUP_FILTER);
	}
	if (sizeof(CONFIG_I2S_EFR32_TEST_NAME_FILTER) > 1) {
		printk("Name filter  (-n): \"%s\"\n",
		       CONFIG_I2S_EFR32_TEST_NAME_FILTER);
	}
	if (CONFIG_I2S_EFR32_TEST_REPEAT > 1) {
		printk("Repeat (-r): %d iterations\n",
		       (int)CONFIG_I2S_EFR32_TEST_REPEAT);
	}

	printk("Unity argv:");
	for (int i = 0; i < argc; i++) {
		printk(" %s", argv[i]);
	}
	printk("\n----------------------------------------\n");
}

int main(void)
{
	/* Give the kernel a moment to bring up the console before we start
	 * pumping Unity output through it -- otherwise the very first
	 * character can land before the UART driver is fully up on some
	 * boards. */
	k_sleep(K_MSEC(100));

	/* One-shot suite setup: if the device is missing/not-ready we abort
	 * up front rather than letting every TEST_SETUP fail with the same
	 * error. */
	if (dev_i2s == NULL) {
		printk("FATAL: I2S device pointer is NULL "
		       "(alias i2s-node0 missing?)\n");
		return 1;
	}
	if (!device_is_ready(dev_i2s)) {
		printk("FATAL: I2S device %s is not ready\n", dev_i2s->name);
		return 1;
	}

	const char *argv[UNITY_MAX_ARGV];
	char repeat_buf[8];
	int argc = build_unity_argv(argv, repeat_buf, sizeof(repeat_buf));

	print_runner_banner(argc, argv);

	int failures = UnityMain(argc, argv, RunAllTests);

	printk("\n----------------------------------------\n");
	printk("*** Unity run complete: %d failure(s) ***\n", failures);
	return failures;
}
