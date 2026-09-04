/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "test_i2s_efr32_common.h"

void test_write__no_configure_returns_eio(void)
{
	void *blk;
	int ret;

	ret = test_alloc_tx_block(&blk);
	TEST_ASSERT_EQUAL(0, ret);
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_write__after_stop_returns_eio__w16(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_configure(I2S_DEV, I2S_DIR_TX, NULL));

	ret = test_alloc_tx_block(&blk);
	TEST_ASSERT_EQUAL(0, ret);
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_write__rejects_size_zero__w16(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_write(I2S_DEV, blk, 0U));
	test_free_tx_block(blk);
}

void test_write__rejects_size_gt_block__w16(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE + 4U));
	test_free_tx_block(blk);
}

void test_write__rejects_size_not_frame_aligned__w16(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_write(I2S_DEV, blk, 2U));
	test_free_tx_block(blk);
}

void test_write__rejects_size_not_frame_aligned__w32(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(-EINVAL, i2s_write(I2S_DEV, blk, 4U));
	test_free_tx_block(blk);
}

void test_write__valid_in_ready_returns_zero__w16(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__valid_in_ready_returns_zero__w32(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__valid_in_running_returns_zero__w16(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP);
	test_wait_tx_ready(2000);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__valid_in_running_returns_zero__w32(void)
{
	void *blk;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP);
	test_wait_tx_ready(2000);
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__ring_full_no_wait_returns_ebusy__w16(void)
{
	void *blk;
	int ret;
	uint16_t i;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, 0));

	for (i = 0U; i < TX_RING_DEPTH; i++) {
		TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
		TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	}

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EBUSY, ret);

	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__ring_full_no_wait_returns_ebusy__w32(void)
{
	void *blk;
	int ret;
	uint16_t i;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, 0));

	for (i = 0U; i < TX_RING_DEPTH; i++) {
		TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
		TEST_ASSERT_EQUAL(0, i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE));
	}

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EBUSY, ret);

	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

void test_write__stopping_state_returns_eio__w16(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(16, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_fill_tx_ring(16, 1, NULL, NULL));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);

	TEST_ASSERT_TRUE(test_wait_tx_ready(3000));
}

void test_write__stopping_state_returns_eio__w32(void)
{
	void *blk;
	int ret;

	TEST_ASSERT_EQUAL(0, test_configure_tx(32, SYS_FOREVER_MS));
	TEST_ASSERT_EQUAL(0, test_fill_tx_ring(32, 1, NULL, NULL));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_START));
	TEST_ASSERT_EQUAL(0, i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_STOP));

	TEST_ASSERT_EQUAL(0, test_alloc_tx_block(&blk));
	ret = i2s_write(I2S_DEV, blk, TEST_BLOCK_SIZE);
	test_free_tx_block(blk);
	TEST_ASSERT_EQUAL(-EIO, ret);

	TEST_ASSERT_TRUE(test_wait_tx_ready(3000));
}
