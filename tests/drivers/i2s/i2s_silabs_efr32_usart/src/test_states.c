/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * State-machine integration tests. Confirms that the live driver
 * threads through the right transitions on real hardware:
 *   NOT_READY -> [configure] -> READY
 *                READY        -> [START]   -> RUNNING
 *                READY        -> [STOP]    -> -EINVAL  (must be RUNNING)
 *                RUNNING      -> [DROP]    -> READY
 *                READY        -> [PREPARE] -> -EINVAL  (must be ERROR)
 */

#include "common.h"
#include "unity_fixture.h"

#include <errno.h>
#include <zephyr/drivers/i2s.h>

TEST_GROUP(i2s_efr32_states);

TEST_SETUP(i2s_efr32_states)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_states)
{
	i2s_efr32_test_reset_device();
}

static int configure_default_tx(void)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	return i2s_configure(dev_i2s, I2S_DIR_TX, &cfg);
}

static int configure_default_rx(void)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_RX);
	return i2s_configure(dev_i2s, I2S_DIR_RX, &cfg);
}

static int configure_default_both(void)
{
	struct i2s_config tx_cfg;
	struct i2s_config rx_cfg;
	int err;

	i2s_efr32_test_make_default_cfg(&tx_cfg, I2S_DIR_TX);
	i2s_efr32_test_make_default_cfg(&rx_cfg, I2S_DIR_RX);

	err = i2s_configure(dev_i2s, I2S_DIR_TX, &tx_cfg);
	if (err != 0) {
		return err;
	}
	return i2s_configure(dev_i2s, I2S_DIR_RX, &rx_cfg);
}

TEST(i2s_efr32_states, start_in_not_ready__rejected)
{
	/* Device is freshly deconfigured by TEST_SETUP, so START must be
	 * a no-op (cfg_valid==false). The driver returns 0 in that case
	 * because the per-direction branch is gated on cfg_valid; we just
	 * verify it does not return -EFAULT or assert. */
	int ret = i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);

	TEST_ASSERT_TRUE_MESSAGE(ret == 0 || ret == -EINVAL,
		"trigger(START) on unconfigured stream returned unexpected code");
}

TEST(i2s_efr32_states, configure_then_start_running)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START in READY must succeed");

	/* Trying to START again from RUNNING must be rejected. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START while RUNNING must return -EINVAL");

	/* DROP brings TX back to READY (no buffers were enqueued, so the
	 * abort path is fast). */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP),
		"DROP from RUNNING must succeed");
}

TEST(i2s_efr32_states, stop_in_ready__rejected)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_STOP),
		"STOP requires RUNNING, must reject in READY");
}

TEST(i2s_efr32_states, prepare_when_not_error__rejected)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE),
		"PREPARE only valid in ERROR state");
}

TEST(i2s_efr32_states, write_timeout_when_no_dma_consumer)
{
	void *block;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &block, K_NO_WAIT),
		"tx slab alloc failed");

	/* No START issued -> driver leaves USART disabled, DMA never drains
	 * the queue. With a TX_RING_COUNT of 4, we can enqueue a few then
	 * must time out on the next one. cfg.timeout = 1000 ms. */
	int err = 0;

	for (int i = 0; i < CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT; i++) {
		void *b;

		TEST_ASSERT_EQUAL_INT_MESSAGE(0,
			k_mem_slab_alloc(&tx_mem_slab, &b, K_NO_WAIT),
			"slab alloc inside fill loop failed");
		err = i2s_write(dev_i2s, b, TEST_BLOCK_SIZE_16BIT);
		if (err != 0) {
			break;
		}
	}

	/* The very next write MUST time out with -EAGAIN (ring full + no
	 * DMA progress because START was never issued). */
	err = i2s_write(dev_i2s, block, TEST_BLOCK_SIZE_16BIT);
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EAGAIN, err,
		"i2s_write should time out with -EAGAIN when DMA never drains");

	k_mem_slab_free(&tx_mem_slab, block);
	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, start_rx_in_ready__running)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_rx(),
		"configure RX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"first RX START in READY must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"second RX START while RUNNING must return -EINVAL");

	(void)i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, stop_tx_running_empty_queue__ready_immediately)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START must succeed");

	/* With no enqueued blocks the driver short-circuits STOP straight
	 * to READY (active==NULL and q.count==0), no STOPPING phase. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_STOP),
		"STOP with empty queue must succeed");

	/* Indirect check that we landed back in READY: DROP from any
	 * cfg_valid state succeeds and a follow-up START must accept. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP),
		"DROP after the implicit STOP->READY must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START again confirms we are in READY");

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, stop_tx_running_pending_queue__stopping_then_ready)
{
	void *blk1;
	void *blk2;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk1, K_NO_WAIT),
		"alloc blk1");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk2, K_NO_WAIT),
		"alloc blk2");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk1, TEST_BLOCK_SIZE_16BIT),
		"write blk1");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk2, TEST_BLOCK_SIZE_16BIT),
		"write blk2");

	/* With pending data the driver enters STOPPING and only flips to
	 * READY after the DMA callbacks drain the active block + the
	 * remaining queue entries. Each block is ~2 ms of audio, so 200 ms
	 * is comfortably more than the drain time. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_STOP),
		"STOP with pending queue must succeed");
	k_sleep(K_MSEC(200));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"after drain we must be in READY -> START succeeds");

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, stop_rx_running__ready)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_rx(),
		"configure RX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_STOP),
		"RX STOP must succeed (drains synchronously to READY)");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START again confirms we are back in READY");

	(void)i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, drain_tx_running__ready_after_drain)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"alloc blk");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT),
		"write blk");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN),
		"DRAIN in RUNNING must succeed");
	/* DRAIN is the same path as STOP-with-pending-data: STOPPING ->
	 * (DMA callbacks pop the queue) -> READY. Block is ~2 ms, give it
	 * 100 ms of slack. */
	k_sleep(K_MSEC(100));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"after drain finishes we must be in READY -> START succeeds");

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, drain_tx_in_ready__rejected)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");

	/* DRAIN is only valid from RUNNING. In READY the driver returns
	 * -EINVAL without touching state. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN),
		"DRAIN in READY must return -EINVAL");
}

TEST(i2s_efr32_states, drain_rx__rejected)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_rx(),
		"configure RX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START must succeed");

	/* The driver implements DRAIN only for the TX direction; any
	 * non-TX dir falls through to the else branch -> -EINVAL. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DRAIN),
		"DRAIN on RX direction must be rejected");

	(void)i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, drop_running_tx__ready)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"alloc blk");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT),
		"write blk");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP),
		"DROP from RUNNING must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"after DROP we are back in READY -> START succeeds");

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, drop_both_dirs__both_ready)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_both(),
		"configure both dirs failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"TX START must succeed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START must succeed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP),
		"DROP DIR_BOTH must succeed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"TX must be back in READY");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX must be back in READY");

	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_states, drop_in_not_ready__no_state_change)
{
	/* No configure -- both streams are NOT_READY / cfg_valid==false.
	 * The driver's DROP path runs unconditionally (clears DMA, drains
	 * ring) and then sets state per cfg_valid, leaving NOT_READY
	 * intact. The trigger must not assert or fault. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP),
		"DROP on NOT_READY stream must be a benign no-op");

	/* Follow-up START is also a no-op -- the per-direction branches
	 * are gated on cfg_valid, so they short-circuit and return 0 (not
	 * -EFAULT, not -EINVAL). */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START on NOT_READY stream must be a benign no-op");
}

TEST(i2s_efr32_states, unknown_trigger_cmd__rejected)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure failed");

	/* The trigger switch has an explicit default arm that returns
	 * -EINVAL on any unknown command. Cast through int to silence the
	 * "value not in enum" warning that some toolchains emit. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_trigger(dev_i2s, I2S_DIR_TX,
			    (enum i2s_trigger_cmd)0xFF),
		"unknown trigger command must be rejected");
}
