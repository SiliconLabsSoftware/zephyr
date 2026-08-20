/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.h"

#include <string.h>
#include <sys/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/* Slab depth must exceed CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT
 * (driver's TX ring capacity, default 4) so write_timeout_when_no_dma_consumer
 * can fill the ring AND keep one extra block in hand to assert the
 * sem-timeout path on the next i2s_write(). 8 covers the default and
 * leaves headroom if someone bumps the Kconfig later. */
#define NUM_TX_BLOCKS 8
#define NUM_RX_BLOCKS 8

/* Sized for the worst case (32-bit stereo) so the same slab is reusable
 * across all tests regardless of word size. */
#define MEM_SLAB_BLOCK_SIZE TEST_BLOCK_SIZE_32BIT

K_MEM_SLAB_DEFINE(tx_mem_slab, MEM_SLAB_BLOCK_SIZE, NUM_TX_BLOCKS, 32);
K_MEM_SLAB_DEFINE(rx_mem_slab, MEM_SLAB_BLOCK_SIZE, NUM_RX_BLOCKS, 32);

const struct device *const dev_i2s = DEVICE_DT_GET_OR_NULL(I2S_EFR32_DEV_NODE);

void i2s_efr32_test_make_default_cfg(struct i2s_config *cfg, enum i2s_dir dir)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->word_size      = 16U;
	cfg->channels       = 2U;
	cfg->format         = I2S_FMT_DATA_FORMAT_I2S;
	cfg->frame_clk_freq = (uint32_t)TEST_FRAME_CLK_HZ;
	cfg->options        = I2S_OPT_BIT_CLK_CONT;
	cfg->block_size     = TEST_BLOCK_SIZE_16BIT;
	cfg->mem_slab       = (dir == I2S_DIR_RX) ? &rx_mem_slab : &tx_mem_slab;
	cfg->timeout        = TEST_TIMEOUT_MS;
}

void i2s_efr32_test_fill_pattern_16(int16_t *block, size_t block_bytes,
				    uint16_t seed)
{
	const size_t n = block_bytes / sizeof(int16_t);

	for (size_t i = 0; i < n; i++) {
		uint16_t v = seed + (uint16_t)i;

		block[i] = (int16_t)((i & 1U) ? (v ^ 0x5A5AU) : v);
	}
}

void i2s_efr32_test_fill_pattern_32(int32_t *block, size_t block_bytes,
				    uint16_t seed)
{
	const size_t n = block_bytes / sizeof(int32_t);

	/* W32D32 layout note (see driver header): the 16-bit audio payload
	 * lives in the LOW half; the HIGH half is zero-padded. We mirror the
	 * pattern in the low half so verify can compare with the same seed. */
	for (size_t i = 0; i < n; i++) {
		uint16_t v = seed + (uint16_t)i;
		uint16_t payload = (i & 1U) ? (v ^ 0x5A5AU) : v;

		block[i] = (int32_t)payload;
	}
}

int i2s_efr32_test_verify_pattern_16(const int16_t *block, size_t block_bytes,
				     uint16_t seed)
{
	const size_t n = block_bytes / sizeof(int16_t);

	for (size_t i = 0; i < n; i++) {
		uint16_t v = seed + (uint16_t)i;
		int16_t  expected = (int16_t)((i & 1U) ? (v ^ 0x5A5AU) : v);

		if (block[i] != expected) {
			printk("verify16: idx=%u expected=0x%04x got=0x%04x\n",
			       (unsigned int)i, (unsigned int)(uint16_t)expected,
			       (unsigned int)(uint16_t)block[i]);
			return -1;
		}
	}
	return 0;
}

int i2s_efr32_test_verify_pattern_32(const int32_t *block, size_t block_bytes,
				     uint16_t seed)
{
	const size_t n = block_bytes / sizeof(int32_t);

	for (size_t i = 0; i < n; i++) {
		uint16_t v = seed + (uint16_t)i;
		int32_t  expected = (int32_t)(uint16_t)((i & 1U)
				    ? (v ^ 0x5A5AU) : v);

		/* Compare only the low 16 bits. On the wire W32D16 carries 16
		 * payload bits zero-padded to a 32-bit slot, and the driver's
		 * rx widen sign-extends int16 -> int32. The high 16 bits of
		 * `block[i]` are therefore a sign-extension of the low 16,
		 * not part of the test payload. */
		if ((int16_t)block[i] != (int16_t)expected) {
			printk("verify32: idx=%u expected_lo=0x%04x got_lo=0x%04x\n",
			       (unsigned int)i,
			       (unsigned int)(uint16_t)expected,
			       (unsigned int)(uint16_t)block[i]);
			return -1;
		}
	}
	return 0;
}

void i2s_efr32_test_reset_device(void)
{
	struct i2s_config null_cfg = { 0 };

	/* Best-effort: the driver treats a NULL/zero cfg as a request to
	 * deconfigure both directions, returning the device to NOT_READY.
	 * Ignore the return code - it is harmless if the previous test
	 * already left the device in NOT_READY. */
	(void)i2s_configure(dev_i2s, I2S_DIR_BOTH, &null_cfg);
}
