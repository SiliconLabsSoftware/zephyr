/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * MCLK (external master clock) verification for the EFR32 USART-I2S
 * stack.
 *
 * Scope
 * -----
 * Some external audio codecs (TAS2505, WM8731, ...) need a high-frequency
 * MCLK supplied by the MCU on a dedicated pin. The Silabs CMU clkout
 * CMU clock driver (`silabs,series-clock-clkout` child) is what programs that output. This
 * test is the only place the PRS + TIMER1 capture infrastructure that
 * was previously embedded in the tas2505 sample now lives -- the
 * production sample no longer pulls in em_timer / em_prs.
 *
 * What it asserts
 * ---------------
 *   1. If `silabs,series-clock-clkout` is present in DT (and enabled), the
 *      output frequency measured via PRS->TIMER1 capture sits within
 *      +/- 2 % of the nominal value derived from the DT source +
 *      prescaler. On the DK2602A test overlay this is
 *        EXPCLK = HFRCODPLL (76.8 MHz) / prescaler (4) = 19.2 MHz.
 *   2. If no clkout node is enabled, the test self-skips with
 *      TEST_IGNORE_MESSAGE so the rest of the Unity run completes
 *      cleanly (verbose UART: leading newline avoids glue to TEST name).
 */

#include "common.h"
#include "unity_fixture.h"

#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>

#include <em_cmu.h>
#include <em_prs.h>
#include <em_timer.h>

TEST_GROUP(i2s_efr32_mclk);

TEST_SETUP(i2s_efr32_mclk)
{
}

TEST_TEAR_DOWN(i2s_efr32_mclk)
{
}

#if DT_HAS_COMPAT_STATUS_OKAY(silabs_series_clock_clkout)

#define MCO_NODE       DT_COMPAT_GET_ANY_STATUS_OKAY(silabs_series_clock_clkout)
#define MCO_CLKOUT_IDX DT_REG_ADDR(MCO_NODE)
#define MCO_PRESCALER  DT_PROP_OR(MCO_NODE, clock_div, 1)

/* HFRCODPLL default is 76.8 MHz on xG27 series. The clkout source on
 * the test overlay is EXPCLK = HFRCODPLL, so the expected MCLK on the
 * pin is HFRCODPLL / prescaler. */
#define MCLK_HFRCODPLL_HZ 76800000U
#define MCLK_EXPECTED_HZ  (MCLK_HFRCODPLL_HZ / (uint32_t)MCO_PRESCALER)

/* PRS async producer: each CMU CLKOUT output has a fixed PRS signal. */
static uint32_t prs_cmul_clkout_signal(uint8_t clkout_idx)
{
	switch (clkout_idx) {
	case 0U:
		return PRS_CMUL_CLKOUT0;
	case 1U:
		return PRS_CMUL_CLKOUT1;
	case 2U:
		return PRS_CMUL_CLKOUT2;
	default:
		return 0U;
	}
}

/* Measure the actual MCLK frequency by routing the PRS clkout signal
 * to TIMER1 CC0 capture and timing one full period.
 */
static uint32_t mclk_measure_timer(void)
{
	CMU_ClockEnable(cmuClock_PRS, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);

	int prs_ch_i = PRS_GetFreeChannel(prsTypeAsync);

	if (prs_ch_i < 0) {
		printk("MCLK measure: no free PRS async channel\n");
		return 0U;
	}

	unsigned int prs_ch = (unsigned int)prs_ch_i;
	uint32_t prs_signal = prs_cmul_clkout_signal(MCO_CLKOUT_IDX);

	if (prs_signal == 0U) {
		printk("MCLK measure: invalid PRS signal for CLKOUT%u\n",
		       (unsigned int)MCO_CLKOUT_IDX);
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

	while ((TIMER_IntGet(TIMER1) & TIMER_IF_CC0) == 0U) {
	}
	uint32_t cap1 = TIMER_CaptureGet(TIMER1, 0);

	TIMER_IntClear(TIMER1, TIMER_IF_CC0);

	while ((TIMER_IntGet(TIMER1) & TIMER_IF_CC0) == 0U) {
	}
	uint32_t cap2 = TIMER_CaptureGet(TIMER1, 0);

	TIMER_Enable(TIMER1, false);

	uint32_t timer_clk = CMU_ClockFreqGet(cmuClock_TIMER1);
	uint32_t delta = cap2 - cap1;

	printk("[mclk] timer_clk=%u cap1=%u cap2=%u delta=%u\n",
	       (unsigned int)timer_clk, (unsigned int)cap1,
	       (unsigned int)cap2, (unsigned int)delta);

	return (delta > 0U) ? (timer_clk / delta) : 0U;
}

TEST(i2s_efr32_mclk, clkout_in_2pct_range)
{
	const uint32_t expected = MCLK_EXPECTED_HZ;
	const uint32_t tol = expected / 50U;  /* +/- 2 % */
	uint32_t mclk = mclk_measure_timer();

	printk("[mclk] expected=%u Hz measured=%u Hz (tol +/-%u Hz)\n",
	       (unsigned int)expected, (unsigned int)mclk,
	       (unsigned int)tol);

	TEST_ASSERT_TRUE_MESSAGE(mclk > 0U,
		"MCLK measurement returned 0 (PRS/TIMER setup failed)");
	TEST_ASSERT_UINT_WITHIN_MESSAGE(tol, expected, mclk,
		"MCLK out of +/- 2 % of expected = HFRCODPLL / prescaler");
}

#else  /* no silabs,series-clock-clkout in DT */

TEST(i2s_efr32_mclk, clkout_in_2pct_range)
{
	/* Verbose mode prints the TEST(...) name without a trailing newline;
	 * UnityIgnore() then prints file:line::IGNORE on the same UART line.
	 * Emit a newline first so the ignore reason stays readable on VCOM. */
	printk("\n");
	TEST_IGNORE_MESSAGE(
		"no silabs,series-clock-clkout in DT (add clkout node to cmu to run)");
}

#endif  /* DT_HAS_COMPAT_STATUS_OKAY(silabs_series_clock_clkout) */
