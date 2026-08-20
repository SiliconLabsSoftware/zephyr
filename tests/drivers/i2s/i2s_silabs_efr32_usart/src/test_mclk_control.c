/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * MCO driver API tests (silabs_mco_enable / silabs_mco_disable).
 *
 * Uses the same Unity runner as test_i2s_efr32_*.c (test_runner.c + RUN_TEST_LOG).
 * Pin-frequency checks reuse PRS + TIMER1 capture (shared idea with test_mclk.c).
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control/clock_silabs_efr32_mco.h>
#include <zephyr/sys/printk.h>

#include <em_cmu.h>

#include "unity.h"
#include "unity_config.h"

#if DT_HAS_COMPAT_STATUS_OKAY(silabs_efr32_cmu_clkout)
#include <em_prs.h>
#include <em_timer.h>

#define MCO_NODE              DT_COMPAT_GET_ANY_STATUS_OKAY(silabs_efr32_cmu_clkout)
#define MCO_DEV               DEVICE_DT_GET(DT_NODELABEL(silabs_cmu_clkout))
#define MCO_GPIO_PORT         DT_PROP(MCO_NODE, silabs_clkout_gpio_port)
#define MCO_PRESCALER         DT_PROP(MCO_NODE, silabs_clkout_prescaler)
#define HAS_MCLK_HW           1

#define MCLK_HFRCODPLL_HZ     76800000U
#define MCLK_EXPECTED_HZ      (MCLK_HFRCODPLL_HZ / (uint32_t)MCO_PRESCALER)
/* Max busy-wait iterations for TIMER1 CC0 capture when CLKOUT is running (limit avoids infinite hang). */
#define MCLK_CAPTURE_SPIN_MAX 2000000U
/* Shorter timeout when CLKOUT should be off after silabs_mco_disable() */
#define MCLK_ABSENT_SPIN_MAX  50000U
#else
#define HAS_MCLK_HW           0
#endif

#if HAS_MCLK_HW

static uint32_t prs_cmul_clkout_signal(uint8_t gpio_port)
{
	switch (gpio_port) {
	case 0U:
	case 1U:
		return PRS_CMUL_CLKOUT2;
	case 2U:
		return PRS_CMUL_CLKOUT0;
	case 3U:
		return PRS_CMUL_CLKOUT1;
	default:
		return 0U;
	}
}

/* Poll TIMER1 CC0 until a capture edge is seen or spin_max is exhausted. */
static bool timer_wait_cc0(uint32_t spin_max)
{
	uint32_t spins = 0U;

	while (spins < spin_max) {
		if ((TIMER_IntGet(TIMER1) & TIMER_IF_CC0) != 0U) {
			return true;
		}
		spins++;
	}

	return false;
}

static uint32_t mclk_measure_hz(uint32_t spin_max)
{
	CMU_ClockEnable(cmuClock_PRS, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);

	int prs_ch_i = PRS_GetFreeChannel(prsTypeAsync);

	if (prs_ch_i < 0) {
		return 0U;
	}

	unsigned int prs_ch = (unsigned int)prs_ch_i;
	uint32_t prs_signal = prs_cmul_clkout_signal(MCO_GPIO_PORT);

	if (prs_signal == 0U) {
		return 0U;
	}

	PRS_ConnectSignal(prs_ch, prsTypeAsync, (PRS_Signal_t)prs_signal);
	PRS_ConnectConsumer(prs_ch, prsTypeAsync, prsConsumerTIMER1_CC0);

	TIMER_Init_TypeDef tim_init = TIMER_INIT_DEFAULT;

	tim_init.prescale = timerPrescale1;
	tim_init.enable = false;
	TIMER_Init(TIMER1, &tim_init);
	TIMER_TopSet(TIMER1, 0xFFFFFFFFU);

	TIMER_InitCC_TypeDef cc_init = TIMER_INITCC_DEFAULT;

	cc_init.mode = timerCCModeCapture;
	cc_init.edge = timerEdgeRising;
	cc_init.prsInput = true;
	cc_init.prsSel = prs_ch;
	cc_init.prsInputType = timerPrsInputAsyncLevel;
	TIMER_InitCC(TIMER1, 0, &cc_init);

	TIMER_IntClear(TIMER1, TIMER_IF_CC0);
	TIMER_Enable(TIMER1, true);

	if (!timer_wait_cc0(spin_max)) {
		TIMER_Enable(TIMER1, false);
		return 0U;
	}

	uint32_t cap1 = TIMER_CaptureGet(TIMER1, 0);

	TIMER_IntClear(TIMER1, TIMER_IF_CC0);

	if (!timer_wait_cc0(spin_max)) {
		TIMER_Enable(TIMER1, false);
		return 0U;
	}

	uint32_t cap2 = TIMER_CaptureGet(TIMER1, 0);

	TIMER_Enable(TIMER1, false);

	uint32_t timer_clk = CMU_ClockFreqGet(cmuClock_TIMER1);
	uint32_t delta = cap2 - cap1;

	return (delta > 0U) ? (timer_clk / delta) : 0U;
}

/*
 * Force CLKOUT off via silabs_mco_disable() so each test starts and ends in a
 * known state. silabs_mco_init() may already have enabled MCLK at boot; without
 * this, enable/disable tests could see a clock that was never gated off.
 */
static void mco_leave_disabled(void)
{
	if (device_is_ready(MCO_DEV)) {
		(void)silabs_mco_disable(MCO_DEV);
	}
}

#endif /* HAS_MCLK_HW */

void test_mclk_enable_fails(void)
{
	TEST_ASSERT_EQUAL_INT(-EINVAL, silabs_mco_enable(NULL));
}

void test_mclk_disable_fails(void)
{
	TEST_ASSERT_EQUAL_INT(-EINVAL, silabs_mco_disable(NULL));
}

void test_mclk_control__mco_enable_outputs_expected_hz(void)
{
#if !HAS_MCLK_HW
	TEST_IGNORE_MESSAGE("requires silabs,efr32-cmu-clkout in board overlay");
#else
	const uint32_t expected = MCLK_EXPECTED_HZ;
	const uint32_t tol = expected / 50U; /* +/- 2 % */
	uint32_t measured;

	mco_leave_disabled();

	TEST_ASSERT_TRUE(device_is_ready(MCO_DEV));
	TEST_ASSERT_EQUAL_INT(0, silabs_mco_enable(MCO_DEV));

	measured = mclk_measure_hz(MCLK_CAPTURE_SPIN_MAX);

	printk("[mclk_ctrl] enable: expected=%u Hz measured=%u Hz (tol +/-%u)\n",
	       (unsigned int)expected, (unsigned int)measured, (unsigned int)tol);
	unity_output_flush();

	mco_leave_disabled();

	TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0U, measured,
		"no CLKOUT edges after silabs_mco_enable()");
	TEST_ASSERT_UINT_WITHIN_MESSAGE(tol, expected, measured,
		"MCLK after enable outside +/- 2 %");
#endif
}

void test_mclk_control__mco_disable_stops_clock(void)
{
#if !HAS_MCLK_HW
	TEST_IGNORE_MESSAGE("requires silabs,efr32-cmu-clkout in board overlay");
#else
	uint32_t measured;

	mco_leave_disabled();
	TEST_ASSERT_EQUAL_INT(0, silabs_mco_enable(MCO_DEV));

	measured = mclk_measure_hz(MCLK_CAPTURE_SPIN_MAX);
	TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0U, measured,
		"CLKOUT not running before disable");

	TEST_ASSERT_EQUAL_INT(0, silabs_mco_disable(MCO_DEV));

	measured = mclk_measure_hz(MCLK_ABSENT_SPIN_MAX);
	printk("[mclk_ctrl] disable: measured=%u Hz (expect 0)\n",
	       (unsigned int)measured);
	unity_output_flush();

	mco_leave_disabled();

	TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, measured,
		"CLKOUT still toggling after silabs_mco_disable()");
#endif
}
