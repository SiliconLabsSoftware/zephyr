/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "test_i2s_efr32_common.h"

void test_init__device_is_ready(void)
{
	TEST_ASSERT_TRUE(device_is_ready(I2S_DEV));
}

void test_init__i2s_tx_alias_resolves(void)
{
	TEST_ASSERT_NOT_NULL(I2S_DEV);
	TEST_ASSERT_TRUE(DT_HAS_ALIAS(i2s_tx));
}

void test_configure__stop_with_null_cfg(void)
{
	struct i2s_config cfg;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	ret = i2s_configure(I2S_DEV, I2S_DIR_TX, NULL);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));

	memset(&cfg, 0, sizeof(cfg));
	cfg.frame_clk_freq = 0U;
	ret = i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
}

void test_configure__config_get_null_before_configure(void)
{
	TEST_ASSERT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
	TEST_ASSERT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_RX));
}

void test_configure__config_get_after_success__w16(void)
{
	const struct i2s_config *got;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	got = i2s_config_get(I2S_DEV, I2S_DIR_TX);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_EQUAL_UINT8(16, got->word_size);
	TEST_ASSERT_EQUAL_UINT8(2, got->channels);
	TEST_ASSERT_EQUAL_UINT32(TEST_FRAME_CLK, got->frame_clk_freq);
	TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_SIZE, got->block_size);
}

void test_configure__config_get_after_success__w32(void)
{
	const struct i2s_config *got;

	TEST_ASSERT_EQUAL(0, test_configure_rx(32, SYS_FOREVER_MS));
	got = i2s_config_get(I2S_DEV, I2S_DIR_RX);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_EQUAL_UINT8(32, got->word_size);
}

void test_configure__config_get_null_after_stop__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, NULL));
	TEST_ASSERT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
}

void test_configure__rejects_word_size_zero(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.word_size = 0U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__accepts_mono_channels_one__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.channels = 1U;
	cfg.block_size = TEST_BLOCK_SIZE / 2U;
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_channels_three__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.channels = 3U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_block_size_zero__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.block_size = 0U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_block_size_not_frame_aligned__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.block_size = 6U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_block_size_not_frame_aligned__w32(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 32, SYS_FOREVER_MS);
	cfg.block_size = 12U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_dir_both__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	TEST_ASSERT_EQUAL(-ENOSYS, i2s_configure(I2S_DEV, I2S_DIR_BOTH, &cfg));
}

void test_configure__rejects_word_size_unsupported(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 24, SYS_FOREVER_MS);
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_frame_clk_below_min__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.frame_clk_freq = 7999U;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_unsupported_options__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.options |= I2S_OPT_LOOPBACK;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_bit_clk_target__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.options |= I2S_OPT_BIT_CLK_TARGET;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_frame_clk_target__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.options |= I2S_OPT_FRAME_CLK_TARGET;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_format_right_justified__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.format = I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_format_pcm_short__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.format = I2S_FMT_DATA_FORMAT_PCM_SHORT;
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__rejects_mismatched_tx_rx_freq__w16(void)
{
	struct i2s_config tx;
	struct i2s_config rx;

	test_fill_cfg(&tx, 16, SYS_FOREVER_MS);
	test_fill_rx_cfg(&rx, 16, SYS_FOREVER_MS);
	rx.frame_clk_freq = 48000U;

	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &tx));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_RX, &rx));
}

void test_configure__rejects_mismatched_tx_rx_word_size(void)
{
	struct i2s_config tx;
	struct i2s_config rx;

	test_fill_cfg(&tx, 16, SYS_FOREVER_MS);
	test_fill_rx_cfg(&rx, 32, SYS_FOREVER_MS);

	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &tx));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_RX, &rx));
}

void test_configure__rejects_mismatched_tx_rx_format__w16(void)
{
	struct i2s_config tx;
	struct i2s_config rx;

	test_fill_cfg(&tx, 16, SYS_FOREVER_MS);
	test_fill_rx_cfg(&rx, 16, SYS_FOREVER_MS);
	rx.format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;

	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &tx));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_configure(I2S_DEV, I2S_DIR_RX, &rx));
}

void test_configure__success_sets_ready__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_NOT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
}

void test_configure__success_sets_ready__w32(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_NOT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
}

void test_configure__left_justified_format_ok__w16(void)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, 16, SYS_FOREVER_MS);
	cfg.format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg));
}

void test_configure__configure_rx_then_tx_matching__w16(void)
{
	struct i2s_config tx;
	struct i2s_config rx;

	test_fill_rx_cfg(&rx, 16, SYS_FOREVER_MS);
	test_fill_cfg(&tx, 16, SYS_FOREVER_MS);

	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_RX, &rx));
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, &tx));
	TEST_ASSERT_NOT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_RX));
	TEST_ASSERT_NOT_NULL(i2s_config_get(I2S_DEV, I2S_DIR_TX));
}

void test_configure__good_board_expect_zero__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
}

void test_configure__good_board_expect_zero__w32(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
}
