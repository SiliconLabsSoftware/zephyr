/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mono test group for the i2s_silabs_efr32_usart driver. Only compiled in
 * when the driver is built with I2SCTRL.MONO=1 (default,
 * CONFIG_I2S_SILABS_EFR32_USART_STEREO=n).
 *
 * What is covered
 * ---------------
 *   - i2s_configure accepts channels=1 (W16D16 + W32D16) and rejects
 *     channels=2 (the stereo wire layout is not available in this build).
 *   - i2s_write alignment rule for mono is "size % frame_bytes == 0 AND
 *     size % 2 == 0". W16D16 frame=2 lets 2-byte writes through; W32D16
 *     frame=4 still rejects 2-byte writes via the %frame_bytes check.
 *   - Optional loopback over the BRD4194A PA8->PA7 jumper, channels=1,
 *     pattern-compares the round-trip block for both word sizes. Gated
 *     by CONFIG_I2S_EFR32_TEST_LOOPBACK, matching the stereo loopback
 *     group.
 *
 * The mono block layout the driver expects matches Zephyr mono I2S: a
 * contiguous [M0, M1, ...] sequence of int16 (W16D16) or int32 with
 * payload in the low 16 bits (W32D16). Block sizes are computed off
 * TEST_SAMPLES_PER_BLOCK from common.h so the slab (sized for stereo W32)
 * always has headroom.
 */

#include "common.h"
#include "unity_fixture.h"

#ifndef CONFIG_I2S_SILABS_EFR32_USART_STEREO

#include <string.h>
#include <zephyr/autoconf.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define MONO_BLOCK_SIZE_16BIT (TEST_SAMPLES_PER_BLOCK * 1 * (size_t)sizeof(int16_t))
#define MONO_BLOCK_SIZE_32BIT (TEST_SAMPLES_PER_BLOCK * 1 * (size_t)sizeof(int32_t))

#define MONO_LOOPBACK_SEED 0xCAFEU

TEST_GROUP(i2s_efr32_mono);

TEST_SETUP(i2s_efr32_mono)
{
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_mono)
{
	i2s_efr32_test_reset_device();
}

static void make_mono_cfg(struct i2s_config *cfg, enum i2s_dir dir,
			  uint8_t word_size)
{
	i2s_efr32_test_make_default_cfg(cfg, dir);
	cfg->channels   = 1U;
	cfg->word_size  = word_size;
	cfg->block_size = (word_size == 32U) ? MONO_BLOCK_SIZE_32BIT
					     : MONO_BLOCK_SIZE_16BIT;
}

TEST(i2s_efr32_mono, configure_mono_16__accepted)
{
	struct i2s_config cfg;

	make_mono_cfg(&cfg, I2S_DIR_TX, 16U);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"mono channels=1 W16D16 must be accepted by driver");
}

TEST(i2s_efr32_mono, configure_mono_32__accepted)
{
	struct i2s_config cfg;

	make_mono_cfg(&cfg, I2S_DIR_TX, 32U);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"mono channels=1 W32D16 must be accepted by driver");
}

TEST(i2s_efr32_mono, configure_stereo_in_mono_build__rejected)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	cfg.channels = 2U;
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"mono build must reject channels=2 with -EINVAL");
}

TEST(i2s_efr32_mono, write_w16_size_2B__accepted)
{
	struct i2s_config cfg;
	void *blk;

	make_mono_cfg(&cfg, I2S_DIR_TX, 16U);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"configure mono W16 failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc");

	/* W16D16 mono: frame_bytes=2, min_align=2. size=2 satisfies both. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, blk, 2U),
		"size=2 in mono W16 must be accepted");

	/* Driver freed blk on successful queue; do not free again. */
}

TEST(i2s_efr32_mono, write_w32_size_2B__einval)
{
	struct i2s_config cfg;
	void *blk;

	make_mono_cfg(&cfg, I2S_DIR_TX, 32U);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"configure mono W32 failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc");

	/* W32D16 mono: frame_bytes=4. size=2 fails the %frame_bytes check. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, 2U),
		"size=2 in mono W32 must be rejected");

	k_mem_slab_free(&tx_mem_slab, blk);
}

TEST(i2s_efr32_mono, write_size_zero__einval)
{
	struct i2s_config cfg;
	void *blk;

	make_mono_cfg(&cfg, I2S_DIR_TX, 16U);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_configure(dev_i2s, I2S_DIR_TX, &cfg),
		"configure mono W16 failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &blk, K_NO_WAIT),
		"tx slab alloc");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL,
		i2s_write(dev_i2s, blk, 0U),
		"size=0 must be rejected even in mono build");

	k_mem_slab_free(&tx_mem_slab, blk);
}

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK

static void mono_log_wire_hint(void)
{
#if defined(CONFIG_BOARD_XG27_RB4194A)
	printk("[loopback mono] board=RB4194A: short **PA8 (TX)** to "
	       "**PA7 (RX)** (exp-header pin 13 = PA8)\n");
#elif defined(CONFIG_BOARD_XG27_DK2602A)
	printk("[loopback mono] board=DK2602A: short **PC0 (TX)** to "
	       "**PC1 (RX)**\n");
#else
	printk("[loopback mono] board=%s: short USART0 I2S TX->RX per overlay\n",
	       CONFIG_BOARD);
#endif
}

static int configure_mono_both_dirs(uint8_t word_size)
{
	struct i2s_config cfg;

	make_mono_cfg(&cfg, I2S_DIR_TX, word_size);
	/* Left-justified avoids first-slot mis-framing on internal+pin
	 * loopback (same workaround the stereo loopback group uses). */
	cfg.format = (cfg.format & ~I2S_FMT_DATA_FORMAT_MASK) |
		     I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;

	if (i2s_configure(dev_i2s, I2S_DIR_TX, &cfg) != 0) {
		return -1;
	}
	cfg.mem_slab = &rx_mem_slab;
	if (i2s_configure(dev_i2s, I2S_DIR_RX, &cfg) != 0) {
		return -1;
	}
	return 0;
}

static void mono_log_preview_16(const int16_t *tx, const int16_t *rx,
				size_t n_samples)
{
	const unsigned m = (unsigned)MIN((size_t)8, n_samples);

	printk("[loopback mono16] preview (first %u samples):", m);
	printk("\n  tx:");
	for (unsigned i = 0; i < m; i++) {
		printk(" %04x", (unsigned int)(uint16_t)tx[i]);
	}
	printk("\n  rx:");
	for (unsigned i = 0; i < m; i++) {
		printk(" %04x", (unsigned int)(uint16_t)rx[i]);
	}
	printk("\n");
}

static void mono_log_preview_32(const int32_t *tx, const int32_t *rx,
				size_t n_samples)
{
	const unsigned m = (unsigned)MIN((size_t)8, n_samples);

	printk("[loopback mono32] preview (first %u samples, low 16b):", m);
	printk("\n  tx:");
	for (unsigned i = 0; i < m; i++) {
		printk(" %04x", (unsigned int)((uint32_t)tx[i] & 0xFFFFU));
	}
	printk("\n  rx:");
	for (unsigned i = 0; i < m; i++) {
		printk(" %04x", (unsigned int)((uint32_t)rx[i] & 0xFFFFU));
	}
	printk("\n");
}

TEST(i2s_efr32_mono, loopback_mono_16__matches)
{
	void *tx_block;
	void *rx_block = NULL;
	size_t rx_size = 0;
	uint8_t tx_snap[MONO_BLOCK_SIZE_16BIT];

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_mono_both_dirs(16U),
		"configure mono both dirs (W16) failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &tx_block, K_NO_WAIT),
		"tx slab alloc");
	i2s_efr32_test_fill_pattern_16(tx_block, MONO_BLOCK_SIZE_16BIT,
				       MONO_LOOPBACK_SEED);
	memcpy(tx_snap, tx_block, MONO_BLOCK_SIZE_16BIT);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, tx_block, MONO_BLOCK_SIZE_16BIT),
		"tx write");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"TX START");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_read(dev_i2s, &rx_block, &rx_size),
		"rx read");
	TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)MONO_BLOCK_SIZE_16BIT,
					 rx_size,
					 "rx block size mismatch");

	mono_log_wire_hint();
	mono_log_preview_16((const int16_t *)tx_snap,
			    (const int16_t *)rx_block,
			    MONO_BLOCK_SIZE_16BIT / sizeof(int16_t));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_efr32_test_verify_pattern_16(rx_block,
						 MONO_BLOCK_SIZE_16BIT,
						 MONO_LOOPBACK_SEED),
		"mono 16-bit loopback pattern mismatch");

	k_mem_slab_free(&rx_mem_slab, rx_block);
	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_mono, loopback_mono_32__matches)
{
	void *tx_block;
	void *rx_block = NULL;
	size_t rx_size = 0;
	uint8_t tx_snap[MONO_BLOCK_SIZE_32BIT];

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_mono_both_dirs(32U),
		"configure mono both dirs (W32) failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &tx_block, K_NO_WAIT),
		"tx slab alloc");
	i2s_efr32_test_fill_pattern_32(tx_block, MONO_BLOCK_SIZE_32BIT,
				       MONO_LOOPBACK_SEED);
	memcpy(tx_snap, tx_block, MONO_BLOCK_SIZE_32BIT);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START),
		"RX START");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_write(dev_i2s, tx_block, MONO_BLOCK_SIZE_32BIT),
		"tx write");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START),
		"TX START");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_read(dev_i2s, &rx_block, &rx_size),
		"rx read");
	TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)MONO_BLOCK_SIZE_32BIT,
					 rx_size,
					 "rx block size mismatch");

	mono_log_wire_hint();
	mono_log_preview_32((const int32_t *)tx_snap,
			    (const int32_t *)rx_block,
			    MONO_BLOCK_SIZE_32BIT / sizeof(int32_t));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_efr32_test_verify_pattern_32(rx_block,
						 MONO_BLOCK_SIZE_32BIT,
						 MONO_LOOPBACK_SEED),
		"mono 32-bit loopback pattern mismatch");

	k_mem_slab_free(&rx_mem_slab, rx_block);
	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
}

#endif /* CONFIG_I2S_EFR32_TEST_LOOPBACK */

#endif /* !CONFIG_I2S_SILABS_EFR32_USART_STEREO */
