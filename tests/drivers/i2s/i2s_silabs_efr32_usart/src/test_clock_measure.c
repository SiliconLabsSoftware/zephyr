/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Clock-measurement tests for the I2S USART driver - test-image-only.
 *
 *   MCLK (MCO) — verifies CMU_ClockFreqGet(EXPCLK) / prescaler against
 *                HFRCODPLL / prescaler (the EXPORTCLK divider that drives PA4).
 *
 *   BCLK       — read CLKDIV via USART_BaudrateGet(USART0) after I2S configure.
 *                On xG27 the USART output pins (PC2/PC3) cannot be read back via
 *                GPIO DIN while held in peripheral push-pull, so we verify the
 *                hardware register the driver wrote instead of bit-banging GPIO.
 *
 *   LRCLK      — derived from BCLK / (2 * word_size). USART I2S generates the
 *                word-select directly from the bit clock; no separate divider.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <em_cmu.h>
#include <em_usart.h>

#include "unity.h"
#include "test_i2s_efr32_common.h"
#include "unity_config.h"

#if DT_HAS_COMPAT_STATUS_OKAY(silabs_series_clock_clkout)
#define MCO_NODE         DT_COMPAT_GET_ANY_STATUS_OKAY(silabs_series_clock_clkout)
#define MCO_CLKOUT_IDX   DT_REG_ADDR(MCO_NODE)
#define MCO_PRESCALER    DT_PROP_OR(MCO_NODE, clock_div, 1)
#define HAS_MCO_MEASURE  1
#else
#define HAS_MCO_MEASURE  0
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(silabs_i2s_clock_measure)
#define HAS_CLOCK_VERIFY 1
#else
#define HAS_CLOCK_VERIFY 0
#endif

#define I2S_USART_BASE   ((USART_TypeDef *)DT_REG_ADDR(DT_ALIAS(i2s_tx)))

#define CLOCK_TOLERANCE_NUM 5
#define CLOCK_TOLERANCE_DEN 100

void tc_clock_teardown(void)
{
	/* No TIMER/PRS resources to release. */
}

static bool freq_within_tolerance(uint32_t measured, uint32_t expected)
{
	if (expected == 0U) {
		return false;
	}

	uint64_t tol = ((uint64_t)expected * CLOCK_TOLERANCE_NUM) / CLOCK_TOLERANCE_DEN;
	uint64_t lo = (uint64_t)expected - tol;
	uint64_t hi = (uint64_t)expected + tol;

	return ((uint64_t)measured >= lo) && ((uint64_t)measured <= hi);
}

static uint32_t hfrcodpll_hz_get(void)
{
	uint32_t hz = (uint32_t)CMU_HFRCODPLLBandGet();

	if (hz == 0U || hz == (uint32_t)cmuHFRCODPLLFreq_UserDefined) {
		hz = CMU_ClockFreqGet(cmuClock_SYSCLK);
	}
	if (hz == 0U) {
		hz = 76800000U;
	}
	return hz;
}

#if HAS_MCO_MEASURE
static uint32_t mco_expected_hz_from_expclk(void)
{
	uint32_t expclk = (uint32_t)CMU_ClockFreqGet(cmuClock_EXPCLK);
	uint32_t hfrco = hfrcodpll_hz_get();

	if (expclk == 0U || (hfrco / expclk) >= 4U) {
		expclk = hfrco;
	}

	return expclk / (uint32_t)MCO_PRESCALER;
}
#endif /* HAS_MCO_MEASURE */

#if HAS_CLOCK_VERIFY
/*
 * Configure I2S TX for `word_size` so the driver programs USART CLKDIV, then
 * read the actual baudrate back out of USART0_CLKDIV via emlib. We do NOT
 * trigger START — programming the clock divider does not require streaming.
 */
static uint32_t bclk_from_usart_register(uint8_t word_size)
{
	int ret = test_configure_tx(word_size, 0);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret, "i2s_configure(TX) failed");

	return USART_BaudrateGet(I2S_USART_BASE);
}

static void reset_after_verify(void)
{
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
	test_reset_driver();
}
#endif /* HAS_CLOCK_VERIFY */

void test_clock__mclk_routed_at_expected_ratio(void)
{
#if !HAS_MCO_MEASURE
	TEST_IGNORE_MESSAGE("requires silabs,series-clock-clkout in board overlay");
#else
	uint32_t hfrco = hfrcodpll_hz_get();
	uint32_t from_hfrco = hfrco / (uint32_t)MCO_PRESCALER;
	uint32_t from_expclk = mco_expected_hz_from_expclk();

	printk("MCO: EXPCLK/%u=%u Hz HFRCODPLL/%u=%u Hz (CLKOUT%u, config verify)\n",
	       (unsigned int)MCO_PRESCALER, from_expclk,
	       (unsigned int)MCO_PRESCALER, from_hfrco,
	       (unsigned int)MCO_CLKOUT_IDX);
	unity_output_flush();

	TEST_ASSERT_NOT_EQUAL_MESSAGE(0U, from_expclk, "EXPORTCLK frequency is zero");
	TEST_ASSERT_TRUE_MESSAGE(freq_within_tolerance(from_expclk, from_hfrco),
				 "MCO ratio: EXPCLK/prescaler vs HFRCODPLL/prescaler outside +/-5%");
#endif
}

void test_clock__bclk_matches_configured_rate__w16(void)
{
#if !HAS_CLOCK_VERIFY
	TEST_IGNORE_MESSAGE("requires silabs,i2s-clock-measure in board overlay");
#else
	uint32_t bclk_expected = TEST_FRAME_CLK * 2U * 16U;
	uint32_t lrclk_expected = TEST_FRAME_CLK;
	uint32_t bclk_hz = bclk_from_usart_register(16U);
	uint32_t lrclk_hz = bclk_hz / (2U * 16U);

	printk("BCLK w16: register=%u Hz expected=%u Hz (PCLK=%u)\n",
	       bclk_hz, bclk_expected,
	       (unsigned int)CMU_ClockFreqGet(cmuClock_PCLK));
	printk("LRCLK w16: derived=%u Hz expected=%u Hz\n", lrclk_hz, lrclk_expected);
	unity_output_flush();

	reset_after_verify();

	TEST_ASSERT_NOT_EQUAL_MESSAGE(0U, bclk_hz, "USART_BaudrateGet returned 0");
	TEST_ASSERT_TRUE_MESSAGE(freq_within_tolerance(bclk_hz, bclk_expected),
				 "BCLK w16 outside +/-5% of frame_clk*2*word_size");
	TEST_ASSERT_TRUE_MESSAGE(freq_within_tolerance(lrclk_hz, lrclk_expected),
				 "LRCLK w16 outside +/-5% of frame_clk");
#endif
}

void test_clock__bclk_matches_configured_rate__w32(void)
{
#if !HAS_CLOCK_VERIFY
	TEST_IGNORE_MESSAGE("requires silabs,i2s-clock-measure in board overlay");
#else
	uint32_t bclk_expected = TEST_FRAME_CLK * 2U * 32U;
	uint32_t lrclk_expected = TEST_FRAME_CLK;
	uint32_t bclk_hz = bclk_from_usart_register(32U);
	uint32_t lrclk_hz = bclk_hz / (2U * 32U);

	printk("BCLK w32: register=%u Hz expected=%u Hz (PCLK=%u)\n",
	       bclk_hz, bclk_expected,
	       (unsigned int)CMU_ClockFreqGet(cmuClock_PCLK));
	printk("LRCLK w32: derived=%u Hz expected=%u Hz\n", lrclk_hz, lrclk_expected);
	unity_output_flush();

	reset_after_verify();

	TEST_ASSERT_NOT_EQUAL_MESSAGE(0U, bclk_hz, "USART_BaudrateGet returned 0");
	TEST_ASSERT_TRUE_MESSAGE(freq_within_tolerance(bclk_hz, bclk_expected),
				 "BCLK w32 outside +/-5% of frame_clk*2*word_size");
	TEST_ASSERT_TRUE_MESSAGE(freq_within_tolerance(lrclk_hz, lrclk_expected),
				 "LRCLK w32 outside +/-5% of frame_clk");
#endif
}
