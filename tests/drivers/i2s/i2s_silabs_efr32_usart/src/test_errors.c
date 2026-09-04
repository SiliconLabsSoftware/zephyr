/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * ISR / error-recovery integration tests.
 *
 * The driver's ISR (i2s_efr32_isr) sets state=ERROR on USART TXUF (TX
 * underrun) or RXOF (RX overrun). The only documented path back to
 * READY is i2s_trigger(.., I2S_TRIGGER_PREPARE), which only succeeds
 * when at least one stream is in ERROR -- so PREPARE returning 0 is a
 * reliable witness that the ISR did set ERROR. Using PREPARE this way
 * also exercises the recovery branch in the same test.
 *
 * Why software-injection:
 *   On EFR32 Series 2 USART configured as I2S master, the controller
 *   only shifts (and only generates SCLK transitions) when the TX
 *   buffer has data. Starting TX with an empty queue therefore leaves
 *   the line idle and TXUF never asserts on its own. RXOF likewise
 *   needs another driver feeding SCLK + data, which we do not have
 *   without an external loopback. The reliable, hardware-independent
 *   way to verify the ISR error path is to write the IF flag directly
 *   via USART_IntSet(); that pends the same interrupt the hardware
 *   would have raised, and the ISR runs through the same code path.
 *
 * The injection takes effect within one IRQ latency (microseconds);
 * we sleep 5 ms after the write to give the ISR room to run before
 * we sample the state via PREPARE.
 */

#include "common.h"
#include "unity_fixture.h"

#include <em_usart.h>
#include <soc.h>

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>

#define I2S_BASE ((USART_TypeDef *)DT_REG_ADDR(I2S_EFR32_DEV_NODE))

#define ISR_SETTLE_MS 5

TEST_GROUP(i2s_efr32_errors);

TEST_SETUP(i2s_efr32_errors)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_errors)
{
	/* DROP first so any in-flight DMA on the previous test's channel
	 * is torn down before we deconfigure the USART out from under it. */
	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
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

TEST(i2s_efr32_errors, txuf_underrun_transitions_to_error)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure TX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"START TX with empty queue must succeed");

	/* configure already enabled USART_IF_TXUF in IEN, so pending the
	 * IF flag in software is enough to fire the ISR. */
	USART_IntSet(I2S_BASE, USART_IF_TXUF);
	k_sleep(K_MSEC(ISR_SETTLE_MS));

	/* PREPARE only returns 0 when at least one stream is in ERROR; a
	 * 0 here is the only outcome consistent with the ISR having
	 * latched state=ERROR. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE),
		"PREPARE must succeed -- expected state==ERROR after TXUF");
}

TEST(i2s_efr32_errors, rxof_overrun_transitions_to_error)
{
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_rx(),
		"configure RX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"START RX must succeed");

	USART_IntSet(I2S_BASE, USART_IF_RXOF);
	k_sleep(K_MSEC(ISR_SETTLE_MS));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_PREPARE),
		"PREPARE must succeed -- expected state==ERROR after RXOF");
}

TEST(i2s_efr32_errors, prepare_after_underrun__back_to_ready_then_runs)
{
	void *blk;

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_default_tx(),
		"configure TX failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"first START must succeed");

	USART_IntSet(I2S_BASE, USART_IF_TXUF);
	k_sleep(K_MSEC(ISR_SETTLE_MS));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE),
		"PREPARE must succeed (state==ERROR after the forced TXUF)");

	/* After PREPARE the stream is back in READY. A fresh START must
	 * accept and a follow-up write must enqueue cleanly: this is the
	 * full recovery loop end-to-end. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"second START after PREPARE must succeed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk, TEST_BLOCK_SIZE_16BIT),
		"write after recovery must succeed");

	/* Give the DMA a moment to consume so the test ends in a quiescent
	 * state regardless of whether DROP or another underrun fires
	 * first. */
	k_sleep(K_MSEC(50));

	(void)i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
}
