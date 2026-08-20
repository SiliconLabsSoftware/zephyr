/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * i2s_config_get() coverage. The driver returns a pointer into its internal
 * per-direction cfg struct iff that direction is currently configured, and
 * NULL otherwise. We verify both branches plus the round-trip through
 * deconfigure (NULL cfg) so the cfg_valid bookkeeping is exercised end to
 * end against the live API.
 */

#include "common.h"
#include "unity_fixture.h"

#include <stddef.h>
#include <zephyr/drivers/i2s.h>

TEST_GROUP(i2s_efr32_config_get);

TEST_SETUP(i2s_efr32_config_get)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_config_get)
{
	i2s_efr32_test_reset_device();
}

TEST(i2s_efr32_config_get, get_tx_after_configure__returns_same_fields)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"baseline TX configure must succeed");

	const struct i2s_config *got = i2s_config_get(dev_i2s, I2S_DIR_TX);

	TEST_ASSERT_NOT_NULL_MESSAGE(got,
		"config_get(TX) must return non-NULL after configure");
	TEST_ASSERT_EQUAL_UINT32_MESSAGE(cfg.frame_clk_freq, got->frame_clk_freq,
		"frame_clk_freq mismatch");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(cfg.word_size, got->word_size,
		"word_size mismatch");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(cfg.channels, got->channels,
		"channels mismatch");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(cfg.format, got->format,
		"format mismatch");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(cfg.block_size, got->block_size,
		"block_size mismatch");
}

TEST(i2s_efr32_config_get, get_rx_when_only_tx_configured__returns_null)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"TX configure must succeed");

	TEST_ASSERT_NULL_MESSAGE(i2s_config_get(dev_i2s, I2S_DIR_RX),
		"RX must report NULL when only TX has been configured");
}

TEST(i2s_efr32_config_get, get_tx_after_deconfigure__returns_null)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"TX configure must succeed");

	/* Driver treats NULL cfg as "deconfigure both directions": cfg_valid
	 * is cleared and state returns to NOT_READY. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, NULL),
		"deconfigure with NULL cfg must succeed");

	TEST_ASSERT_NULL_MESSAGE(i2s_config_get(dev_i2s, I2S_DIR_TX),
		"TX must report NULL after deconfigure");
}

TEST(i2s_efr32_config_get, get_rx_after_configure_both_dirs__returns_rx_cfg)
{
	struct i2s_config tx_cfg;
	struct i2s_config rx_cfg;

	i2s_efr32_test_make_default_cfg(&tx_cfg, I2S_DIR_TX);
	i2s_efr32_test_make_default_cfg(&rx_cfg, I2S_DIR_RX);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &tx_cfg),
		"TX configure must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_RX, &rx_cfg),
		"RX configure (matching TX) must succeed");

	const struct i2s_config *got = i2s_config_get(dev_i2s, I2S_DIR_RX);

	TEST_ASSERT_NOT_NULL_MESSAGE(got,
		"config_get(RX) must return non-NULL after RX configure");
	TEST_ASSERT_EQUAL_UINT32_MESSAGE(rx_cfg.frame_clk_freq,
		got->frame_clk_freq,
		"RX frame_clk_freq must match what we configured for RX");
}
