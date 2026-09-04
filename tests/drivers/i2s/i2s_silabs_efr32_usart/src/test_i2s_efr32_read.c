/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "test_i2s_efr32_common.h"

void test_read__no_configure_returns_eio(void)
{
	void *blk = NULL;
	size_t size = 0U;

	TEST_ASSERT_EQUAL(-EIO, i2s_read(I2S_DEV, &blk, &size));
}

void test_read__timeout_no_data_returns_ebusy__w16(void)
{
	void *blk = NULL;
	size_t size = 0U;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_rx(16, 0));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	ret = i2s_read(I2S_DEV, &blk, &size);
	TEST_ASSERT_EQUAL(-EBUSY, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP);
}

void test_read__timeout_no_data_returns_ebusy__w32(void)
{
	void *blk = NULL;
	size_t size = 0U;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_rx(32, 0));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	ret = i2s_read(I2S_DEV, &blk, &size);
	TEST_ASSERT_EQUAL(-EBUSY, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP);
}

void test_read__short_timeout_returns_eagain__w16(void)
{
	void *blk = NULL;
	size_t size = 0U;
	int ret;
	struct i2s_config cfg;

	test_fill_rx_cfg(&cfg, 16, 1);
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_RX, &cfg));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	ret = i2s_read(I2S_DEV, &blk, &size);
	TEST_ASSERT_EQUAL(-EAGAIN, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP);
}

void test_read__live_data_after_start(void)
{
	void *tx_blk;
	void *rx_blk = NULL;
	uint8_t expected[TEST_BLOCK_SIZE];
	size_t rx_size = 0U;
	int16_t *samples;
	unsigned int i;
	int ret;

	/*
	 * Zephyr I2S loopback pattern (see tests/drivers/i2s/i2s_api):
	 *  - configure RX then TX with matching params
	 *  - queue TX block(s) while READY
	 *  - START RX, then START TX (BCLK/LRCLK from TX controller)
	 *  - i2s_read() receives a driver-owned RX slab block (do not pre-alloc)
	 * Wire: USART0 TX (PC0) -> USART0 RX (PC1), GND common.
	 */
	TEST_ASSERT_EQUAL(0, test_configure_rx(16, 1000));
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&tx_blk));

	samples = (int16_t *)tx_blk;
	for (i = 0U; i < TEST_BLOCK_SIZE / sizeof(int16_t); i++) {
		samples[i] = (int16_t)(0x5500 + (int16_t)i);
	}
	memcpy(expected, tx_blk, TEST_BLOCK_SIZE);

	/* Prefill TX queue before clocks run (same as i2s_api loopback tests). */
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, tx_blk, TEST_BLOCK_SIZE));

	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));

	ret = i2s_read(I2S_DEV, &rx_blk, &rx_size);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(TEST_BLOCK_SIZE, rx_size);
	TEST_ASSERT_EQUAL_MEMORY(expected, rx_blk, TEST_BLOCK_SIZE);

	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP);
	i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP);
	test_free_rx_block(rx_blk);
}
