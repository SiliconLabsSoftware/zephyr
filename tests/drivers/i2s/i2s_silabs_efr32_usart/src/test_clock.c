/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Silabs-specific tests that confirm i2s_configure() actually programmed
 * the EFR32 USART CLKDIV register with a value consistent with the
 * requested frame clock. These check the emlib USART_InitI2s() side
 * effects directly on real registers.
 *
 * The tests are intentionally relative (they compare CLKDIV ratios for
 * two different frame clocks) so they do not need to know the exact
 * HFPERCLK rate at compile time. The driver writes CLKDIV via
 * USART_InitI2s with baudrate = (frame_clk * 2 * word_size) / 2, and the
 * EFR32 USART formula is approximately
 *   CLKDIV ~ 256 * (HFPERCLK / (2 * baudrate) - 1)
 * Halving baudrate (e.g. 16 kHz -> 8 kHz, were the latter accepted) would
 * roughly double CLKDIV. We exploit this monotonic relation below.
 */

#include "common.h"
#include "unity_fixture.h"

#include <em_usart.h>
#include <soc.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>

/* Pull the USART base from devicetree the same way the driver does
 * (DT_REG_ADDR on the i2s alias node). */
#define I2S_BASE ((USART_TypeDef *)DT_REG_ADDR(I2S_EFR32_DEV_NODE))

TEST_GROUP(i2s_efr32_clock);

TEST_SETUP(i2s_efr32_clock)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_clock)
{
	i2s_efr32_test_reset_device();
}

static uint32_t configure_and_read_clkdiv(uint32_t frame_clk_hz,
					  uint8_t word_size)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.frame_clk_freq = frame_clk_hz;
	cfg.word_size      = word_size;
	cfg.block_size     = (word_size == 32U) ? TEST_BLOCK_SIZE_32BIT
					        : TEST_BLOCK_SIZE_16BIT;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"configure(<freq>, <word_size>) failed");

	return I2S_BASE->CLKDIV;
}

TEST(i2s_efr32_clock, nonzero_clkdiv_after_configure)
{
	uint32_t clkdiv = configure_and_read_clkdiv(16000U, 16U);

	TEST_ASSERT_NOT_EQUAL_MESSAGE(0U, clkdiv,
		"CLKDIV must be programmed (non-zero) after configure");
}

TEST(i2s_efr32_clock, doubling_word_size_halves_divider)
{
	uint32_t clkdiv_w16 = configure_and_read_clkdiv(16000U, 16U);

	/* Bring the device back to NOT_READY before reconfiguring with a
	 * different word size. Without this, the second configure would
	 * collide with the still-valid TX cfg and, depending on driver
	 * mood, either reject the new word size outright or stall on the
	 * cfg_valid mutex. */
	i2s_efr32_test_reset_device();

	uint32_t clkdiv_w32 = configure_and_read_clkdiv(16000U, 32U);

	/* 32-bit word at the same frame rate needs DOUBLE the bit clock,
	 * so CLKDIV must be roughly HALF. Allow +/- 1 LSB rounding error
	 * in the integer divisor part (CLKDIV[16:8]). */
	uint32_t int_w16 = clkdiv_w16 >> 8;
	uint32_t int_w32 = clkdiv_w32 >> 8;

	TEST_ASSERT_TRUE_MESSAGE(int_w16 > 0U && int_w32 > 0U,
		"both dividers must be > 0");
	TEST_ASSERT_UINT_WITHIN_MESSAGE(2U, int_w16, int_w32 * 2U,
		"32-bit CLKDIV should be ~half of 16-bit CLKDIV");
}

TEST(i2s_efr32_clock, halving_frame_clk_doubles_divider)
{
	uint32_t clkdiv_44k = configure_and_read_clkdiv(44100U, 16U);

	i2s_efr32_test_reset_device();

	uint32_t clkdiv_22k = configure_and_read_clkdiv(22050U, 16U);

	uint32_t int_44 = clkdiv_44k >> 8;
	uint32_t int_22 = clkdiv_22k >> 8;

	TEST_ASSERT_TRUE_MESSAGE(int_44 > 0U && int_22 > 0U,
		"both dividers must be > 0");
	TEST_ASSERT_UINT_WITHIN_MESSAGE(2U, int_22, int_44 * 2U,
		"22.05k CLKDIV should be ~double of 44.1k CLKDIV");
}
