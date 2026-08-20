/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Integration tests for i2s_configure(): accept/reject predicates
 * exercised through the live Zephyr API on a real EFR32 USART. Covers
 * the mutex, cfg_valid bookkeeping, and the TX/RX cross-validation path.
 */

#include "common.h"
#include "unity_fixture.h"

#include <errno.h>
#include <zephyr/drivers/i2s.h>

TEST_GROUP(i2s_efr32_configure);

TEST_SETUP(i2s_efr32_configure)
{
	/* Blow away any leftover state from the previous test so each case
	 * starts in NOT_READY. */
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_configure)
{
	i2s_efr32_test_reset_device();
}

TEST(i2s_efr32_configure, valid_16bit_44k1__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.frame_clk_freq = 44100U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"valid 16-bit/44.1 kHz config should be accepted");
}

TEST(i2s_efr32_configure, sub_8k_frame_clk__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	/* Driver predicate: frame_clk_freq < 8000U is rejected. 4000 is
	 * unambiguously below that threshold; 8000 itself is on the
	 * accepted side. */
	cfg.frame_clk_freq = 4000U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"Silabs driver requires >= 8 kHz frame clock");
}

TEST(i2s_efr32_configure, word_24bit__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.word_size = 24U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"Silabs driver supports only 16/32-bit words");
}

TEST(i2s_efr32_configure, pcm_format__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.format = I2S_FMT_DATA_FORMAT_PCM_SHORT;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"Silabs driver does not support PCM formats");
}

TEST(i2s_efr32_configure, tx_rx_incompatible__rejected)
{
	struct i2s_config tx_cfg;
	struct i2s_config rx_cfg;

	i2s_efr32_test_make_default_cfg(&tx_cfg, I2S_DIR_TX);
	i2s_efr32_test_make_default_cfg(&rx_cfg, I2S_DIR_RX);
	rx_cfg.frame_clk_freq = 48000U; /* TX is 16 kHz -> mismatch */

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &tx_cfg),
		"TX config alone should be accepted");
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_RX, &rx_cfg),
		"RX config that conflicts with TX must be rejected");
}

TEST(i2s_efr32_configure, left_justified_stereo__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.format = (cfg.format & ~I2S_FMT_DATA_FORMAT_MASK) |
		     I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"LEFT_JUSTIFIED stereo must be accepted");
}

TEST(i2s_efr32_configure, left_justified_mono__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.format = (cfg.format & ~I2S_FMT_DATA_FORMAT_MASK) |
		     I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
	cfg.channels = 1U;
	cfg.block_size = TEST_BLOCK_SIZE_16BIT / 2U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"LEFT_JUSTIFIED mono TX must be accepted");
}

TEST(i2s_efr32_configure, clock_format_nf_ib__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.format = (cfg.format & ~I2S_FMT_CLK_FORMAT_MASK) |
		     I2S_FMT_CLK_NF_IB;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"I2S + NF_IB clock format must configure (clockMode1 path)");
}

TEST(i2s_efr32_configure, clock_format_if_ib__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.format = (cfg.format & ~I2S_FMT_CLK_FORMAT_MASK) |
		     I2S_FMT_CLK_IF_IB;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"I2S + IF_IB clock format must configure (clockMode1 path)");
}

TEST(i2s_efr32_configure, options_bit_clk_gated__accepted)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.options = I2S_OPT_BIT_CLK_GATED;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"I2S_OPT_BIT_CLK_GATED is within SUPPORTED_OPTIONS");
}

TEST(i2s_efr32_configure, unsupported_option_bit__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.options = I2S_OPT_BIT_CLK_CONT | I2S_OPT_LOOPBACK;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"options outside SUPPORTED_OPTIONS mask must be rejected");
}

TEST(i2s_efr32_configure, bit_clk_target_option__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.options = I2S_OPT_BIT_CLK_CONT | I2S_OPT_BIT_CLK_TARGET;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"I2S_OPT_BIT_CLK_TARGET must be rejected");
}

TEST(i2s_efr32_configure, frame_clk_target_option__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.options = I2S_OPT_BIT_CLK_CONT | I2S_OPT_FRAME_CLK_TARGET;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"I2S_OPT_FRAME_CLK_TARGET must be rejected");
}

TEST(i2s_efr32_configure, dir_both_valid_cfg__enosys)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);

	TEST_ASSERT_EQUAL_INT_MESSAGE(-ENOSYS,
		i2s_configure(dev_i2s, I2S_DIR_BOTH, &cfg),
		"I2S_DIR_BOTH with a non-deconfig cfg must return -ENOSYS");
}

TEST(i2s_efr32_configure, word_size_zero__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.word_size = 0U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"word_size==0 must be rejected");
}

TEST(i2s_efr32_configure, channels_zero__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.channels = 0U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"channels==0 must be rejected");
}

TEST(i2s_efr32_configure, block_size_zero__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.block_size = 0U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"block_size==0 must be rejected");
}

TEST(i2s_efr32_configure, block_size_not_frame_multiple__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	/* Stereo 16-bit: frame_bytes = 4. block_size = 6 is not a multiple. */
	cfg.block_size = 6U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"block_size must be a multiple of frame bytes");
}
