/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_I2S_EFR32_COMMON_H_
#define TEST_I2S_EFR32_COMMON_H_

#include <errno.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#if !DT_HAS_ALIAS(i2s_tx)
#error "Board overlay must define alias i2s-tx"
#endif

#define I2S_DEV          DEVICE_DT_GET(DT_ALIAS(i2s_tx))
#define TEST_BLOCK_SIZE  64U
#define TEST_FRAME_CLK   16000U

#define TX_RING_DEPTH    CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT
#define RX_RING_DEPTH    CONFIG_I2S_SILABS_EFR32_USART_RX_BLOCK_COUNT

/* Active/pending + full TX ring + margin for clock tests and recovery. */
#define TEST_NUM_BLOCKS  (TX_RING_DEPTH + 4U)

extern struct k_mem_slab tx_test_slab;
extern struct k_mem_slab rx_test_slab;

static inline uint32_t test_frame_bytes(uint8_t word_size)
{
	return ((uint32_t)word_size / 8U) * 2U;
}

static inline uint32_t test_dma_data_size(uint8_t word_size)
{
	return word_size == 16U ? 2U : 4U;
}

static inline void test_fill_cfg(struct i2s_config *cfg, uint8_t word_size, int32_t timeout_ms)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->word_size = word_size;
	cfg->channels = 2U;
	cfg->format = I2S_FMT_DATA_FORMAT_I2S;
	cfg->options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	cfg->frame_clk_freq = TEST_FRAME_CLK;
	cfg->block_size = TEST_BLOCK_SIZE;
	cfg->timeout = timeout_ms;
	cfg->mem_slab = &tx_test_slab;
}

static inline void test_fill_rx_cfg(struct i2s_config *cfg, uint8_t word_size, int32_t timeout_ms)
{
	test_fill_cfg(cfg, word_size, timeout_ms);
	cfg->mem_slab = &rx_test_slab;
}

static inline int test_configure_tx(uint8_t word_size, int32_t timeout_ms)
{
	struct i2s_config cfg;

	test_fill_cfg(&cfg, word_size, timeout_ms);
	return i2s_configure(I2S_DEV, I2S_DIR_TX, &cfg);
}

static inline int test_configure_rx(uint8_t word_size, int32_t timeout_ms)
{
	struct i2s_config cfg;

	test_fill_rx_cfg(&cfg, word_size, timeout_ms);
	return i2s_configure(I2S_DEV, I2S_DIR_RX, &cfg);
}

static inline int test_alloc_tx_block(void **blk)
{
	return k_mem_slab_alloc(&tx_test_slab, blk, K_NO_WAIT);
}

static inline int test_alloc_rx_block(void **blk)
{
	return k_mem_slab_alloc(&rx_test_slab, blk, K_NO_WAIT);
}

static inline void test_free_tx_block(void *blk)
{
	if (blk != NULL) {
		k_mem_slab_free(&tx_test_slab, blk);
	}
}

static inline void test_free_rx_block(void *blk)
{
	if (blk != NULL) {
		k_mem_slab_free(&rx_test_slab, blk);
	}
}

static inline int test_write_block(void *blk, size_t size)
{
	return i2s_write(I2S_DEV, blk, size);
}

/*
 * Wait until every TX test-slab block is free (driver returned all buffers).
 * Does not call i2s_write() — that can block forever when cfg.timeout is
 * SYS_FOREVER_MS and the TX semaphore is exhausted.
 */
static inline bool test_wait_tx_ready(int max_ms)
{
	for (int ms = 0; ms < max_ms; ms += 10) {
		if (k_mem_slab_num_free_get(&tx_test_slab) >= TEST_NUM_BLOCKS) {
			return true;
		}
		k_msleep(10);
	}
	return false;
}

static inline void test_recover_mem_slab(struct k_mem_slab *slab, uint32_t num_blocks)
{
	void *blocks[TEST_NUM_BLOCKS];
	uint32_t n;
	uint32_t i;

	if (num_blocks > TEST_NUM_BLOCKS) {
		num_blocks = TEST_NUM_BLOCKS;
	}

	n = 0U;
	while (n < num_blocks) {
		if (k_mem_slab_alloc(slab, &blocks[n], K_NO_WAIT) != 0) {
			break;
		}
		n++;
	}
	for (i = 0U; i < n; i++) {
		k_mem_slab_free(slab, blocks[i]);
	}
}

static inline void test_recover_mem_slabs(void)
{
	test_recover_mem_slab(&tx_test_slab, TEST_NUM_BLOCKS);
	test_recover_mem_slab(&rx_test_slab, TEST_NUM_BLOCKS);
}

static inline void test_reset_driver(void)
{
	i2s_trigger(I2S_DEV, I2S_DIR_TX, I2S_TRIGGER_DROP);
	i2s_trigger(I2S_DEV, I2S_DIR_RX, I2S_TRIGGER_DROP);
	i2s_configure(I2S_DEV, I2S_DIR_TX, NULL);
	i2s_configure(I2S_DEV, I2S_DIR_RX, NULL);
	test_recover_mem_slabs();
}

static inline int test_fill_tx_ring(uint8_t word_size, uint16_t count, void **held, size_t *held_sz)
{
	int ret;
	uint16_t i;

	for (i = 0U; i < count; i++) {
		void *blk;

		ret = test_alloc_tx_block(&blk);
		if (ret < 0) {
			return ret;
		}
		memset(blk, 0, TEST_BLOCK_SIZE);
		ret = test_write_block(blk, TEST_BLOCK_SIZE);
		if (ret < 0) {
			test_free_tx_block(blk);
			return ret;
		}
		if (held != NULL && held_sz != NULL && i == 0U) {
			*held = blk;
			*held_sz = TEST_BLOCK_SIZE;
		}
	}
	return 0;
}

#endif /* TEST_I2S_EFR32_COMMON_H_ */
