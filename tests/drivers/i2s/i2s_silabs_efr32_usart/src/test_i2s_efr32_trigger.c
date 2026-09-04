/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "test_i2s_efr32_common.h"

void test_trigger__start_from_ready__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP);
}

void test_trigger__start_from_ready__w32(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP);
}

void test_trigger__start_without_configure_is_noop(void)
{
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
}

void test_trigger__stop_tx_empty_queue_immediate_ready__w16(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	TEST_ASSERT_EQUAL(0, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_trigger__stop_tx_empty_queue_immediate_ready__w32(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	TEST_ASSERT_EQUAL(0, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_trigger__stop_tx_nonempty_queue_drains__w16(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	/* Queue while READY so STOP always drains a full ring. */
	TEST_ASSERT_EQUAL(0, test_fill_tx_ring(16, TX_RING_DEPTH, NULL, NULL));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));
	TEST_ASSERT_TRUE(test_wait_tx_ready(5000));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	TEST_ASSERT_EQUAL(0, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);

	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));
}

void test_trigger__stop_tx_nonempty_queue_drains__w32(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_fill_tx_ring(32, TX_RING_DEPTH, NULL, NULL));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));
	TEST_ASSERT_TRUE(test_wait_tx_ready(5000));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	TEST_ASSERT_EQUAL(0, ret);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_trigger__stop_rx_immediate_ready__w16(void)
{
	void *blk = NULL;
	size_t size = 0U;

	TEST_ASSERT_EQUAL(0, test_configure_rx(16, 0));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP));
	TEST_ASSERT_EQUAL(-EBUSY, i2s_read(I2S_DEV, &blk, &size));
}

void test_trigger__stop_rx_immediate_ready__w32(void)
{
	void *blk = NULL;
	size_t size = 0U;

	TEST_ASSERT_EQUAL(0, test_configure_rx(32, 0));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_STOP));
	TEST_ASSERT_EQUAL(-EBUSY, i2s_read(I2S_DEV, &blk, &size));
}

void test_trigger__drain_from_running__w16(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_fill_tx_ring(16, 1, NULL, NULL));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DRAIN));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);

	TEST_ASSERT_TRUE(test_wait_tx_ready(3000));
}

void test_trigger__drain_not_running_rejects__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DRAIN));
}

void test_trigger__drain_rx_rejects__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_rx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_DRAIN));
}

void test_trigger__drop_with_cfg_returns_ready__w16(void)
{
	void *blk1;
	void *blk2;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk1));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk1, TEST_BLOCK_SIZE));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk2));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk2, TEST_BLOCK_SIZE));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_trigger__drop_without_cfg_returns_not_ready(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_trigger__prepare_not_error_rejects__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_PREPARE));
}

void test_trigger__unknown_cmd_rejects__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, (enum i2s_trigger_cmd)99));
}

void test_trigger__stop_not_running_rejects__w16(void)
{
	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));
}
