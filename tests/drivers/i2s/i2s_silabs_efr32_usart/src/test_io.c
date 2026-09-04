/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Validation paths of i2s_efr32_write() and i2s_efr32_read():
 *   - cfg_valid gating (-EIO when called before configure)
 *   - size predicates: zero, larger-than-block, frame-misaligned,
 *     not-a-multiple-of-DMA-data-size
 *   - happy-path queueing in READY state
 *   - read timeout behaviour (K_NO_WAIT and short ms timeout)
 *   - read after STOP must not surface stale ring contents
 *   - write rejected when TX stream is in ERROR state
 * The driver's data-path predicates are pure software checks that do not
 * require a wire, so this group runs in the no-wire build.
 */

#include "common.h"
#include "unity_fixture.h"

#include <em_usart.h>
#include <soc.h>

#include <errno.h>
#include <stddef.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define I2S_BASE_USART ((USART_TypeDef *)DT_REG_ADDR(I2S_EFR32_DEV_NODE))
#define ERROR_INJECT_SETTLE_MS 5

TEST_GROUP(i2s_efr32_io);

TEST_SETUP(i2s_efr32_io)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_io)
{
	i2s_efr32_test_reset_device();
}

static int configure_default_tx(void)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	return i2s_configure(dev_i2s, I2S_DIR_TX, &cfg);
}

static int configure_tx_mono_16(void)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.channels = 1U;
	cfg.block_size = TEST_BLOCK_SIZE_16BIT / 2U;
	/* Mono W16: one int16 per frame; block must align to 2-byte DMA slots. */
	return i2s_configure(dev_i2s, I2S_DIR_TX, &cfg);
}

TEST(i2s_efr32_io, write_without_configure__eio)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	int err = i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT);

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EIO, err,
		"write before configure must return -EIO");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_io, write_size_zero__einval)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, 0U),
		"size==0 must be rejected with -EINVAL");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_io, write_size_gt_block_size__einval)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT * 2U),
		"size > block_size must be rejected with -EINVAL");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_io, write_size_not_frame_aligned__einval)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	/* Frame size for 16-bit stereo is 4 bytes. 6 is neither a multiple
	 * of 4 (frame) nor of 4 (DMA), so both validators reject it. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, 6U),
		"frame-misaligned size must be rejected with -EINVAL");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_io, write_size_not_dma_aligned__einval)
{
	void *blk;

	if (configure_tx_mono_16() != 0) {
		printk("\n");
		TEST_IGNORE_MESSAGE("driver rejects mono configure -- skip");
		return;
	}

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	/* Mono W16: frame_bytes=2 and DMA slot=2 bytes. size=1 fails both. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, 1U),
		"odd byte size must be rejected with -EINVAL");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_io, write_in_ready_state__queues_ok)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	/* Driver allows enqueue while in READY (not just RUNNING); the
	 * block sits in the ring until START fires the DMA. There is no
	 * public way to inspect the ring count, so we just verify the
	 * accept-path returns 0 and rely on DROP to release the buffer. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT),
		"write in READY must accept and queue the block");

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_io, write_when_tx_in_error__eio)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START must succeed");
	USART_IntSet(I2S_BASE_USART, USART_IF_TXUF);
	k_sleep(K_MSEC(ERROR_INJECT_SETTLE_MS));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EIO,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT),
		"write while TX in ERROR must return -EIO");

	k_mem_slab_free(&tx_mem_slab, blk);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE),
		"PREPARE must recover from ERROR");
	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_io, read_without_configure__eio)
{
	void *blk = NULL;
	size_t size = 0U;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EIO,
		i2s_read(dev_i2s, &blk, &size),
		"read before configure must return -EIO");
}

TEST(i2s_efr32_io, read_no_data_no_wait__ebusy)
{
	struct i2s_config cfg;
	void *blk = NULL;
	size_t size = 0U;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_RX);
	cfg.timeout = 0; /* maps to K_NO_WAIT inside the driver */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_RX, &cfg),
		"RX configure with timeout=0 must succeed");

	/* k_sem_take(K_NO_WAIT) returns -EBUSY (immediate failure), not
	 * -EAGAIN. -EAGAIN is reserved for "timeout > 0 expired". */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EBUSY,
		i2s_read(dev_i2s, &blk, &size),
		"read with K_NO_WAIT and empty queue must return -EBUSY");
}

TEST(i2s_efr32_io, read_no_data_with_short_timeout__eagain)
{
	struct i2s_config cfg;
	void *blk = NULL;
	size_t size = 0U;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_RX);
	cfg.timeout = 10; /* ms */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_RX, &cfg),
		"RX configure with 10 ms timeout must succeed");

	int64_t t0 = k_uptime_get();
	int err = i2s_read(dev_i2s, &blk, &size);
	int64_t dt = k_uptime_get() - t0;

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EAGAIN, err,
		"read with 10 ms timeout and empty queue must return -EAGAIN");
	/* Generous lower bound (5 ms) to absorb tick rounding; generous
	 * upper bound (100 ms) to absorb scheduler / log-flush jitter. */
	TEST_ASSERT_TRUE_MESSAGE(dt >= 5 && dt <= 100,
		"read should block for ~10 ms, not return immediately or hang");
}

TEST(i2s_efr32_io, read_after_stop__no_garbage)
{
	struct i2s_config cfg;
	void *blk = NULL;
	size_t size = 0U;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_RX);
	cfg.timeout = 0; /* K_NO_WAIT so the assertion does not block */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_RX, &cfg),
		"RX configure with timeout=0 must succeed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_STOP),
		"RX STOP must succeed");

	/* STOP for RX runs rx_drop() synchronously: it frees any active /
	 * queued blocks AND drains every outstanding sem token. So a read
	 * with K_NO_WAIT immediately afterwards must fail fast with -EBUSY
	 * (k_sem_take(K_NO_WAIT) on an empty sem), not surface a stale
	 * block left over from before STOP. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EBUSY,
		i2s_read(dev_i2s, &blk, &size),
		"read after STOP must not surface stale ring data");
}
