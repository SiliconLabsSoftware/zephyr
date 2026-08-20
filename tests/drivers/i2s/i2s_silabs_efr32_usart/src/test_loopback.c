/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * End-to-end loopback test: write a deterministic block on TX, read it
 * back on RX, byte-compare. Requires a physical jumper on USART0 data:
 *
 *   xG27-DK2602A: PC0 (USART0 TX) <----wire----> PC1 (USART0_RX)
 *   xG27-RB4194A (BRD4194A): PA8 (TX) <----wire----> PA7 (RX); see
 *   boards/xg27_rb4194a.overlay (PA8 is on the Silabs exp-header pin 13).
 *
 * Compiled out unless CONFIG_I2S_EFR32_TEST_LOOPBACK=y is set so that
 * builds without the wire (CI smoke build, basic flashing) still pass.
 *
 * On failure, `verify16` / `verify32` in common.c still print the first
 * mismatch index. These tests printk a **snapshot** of the TX buffer taken
 * immediately after the pattern fill (before `i2s_write`; the driver
 * returns the TX mem_slab block to the free pool when DMA completes, so the
 * original `tx_block` pointer must not be logged after the transfer). RX
 * preview/hex are from the block returned by `i2s_read`.
 *
 * Loopback configures **left-justified** I2S (not Philips) so TX/RX slot
 * timing matches on a short wire; Philips mode can drop the first right
 * slot and mis-frame long blocks on this USART.
 *
 * -----------------------------------------------------------------------
 * RX scope note (product vs. test)
 * -----------------------------------------------------------------------
 * The driver's RX path (mem_slab fill from RXDOUBLE + optional W32D16
 * widen via `rx_widen_into_block`) is **not used by the product**: the
 * tas2505 playback sample is TX-only (no microphone). RX exists in the
 * driver solely to exercise the full
 * Zephyr I2S API surface against a known pattern via this loopback
 * harness and the state-machine / configure / IO unit tests.
 *
 * Implications:
 *   - `rx_widen_into_block` uses sign-extension `(int32_t)src[i]` so
 *     negative int16 samples remain negative when widened. The W32D16
 *     loopback comparator (`verify_pattern_32` in common.c) deliberately
 *     compares only the low 16 bits and is therefore unaffected.
 *   - If a future product variant needs microphone capture, this is the
 *     reference path: i2s_configure(I2S_DIR_RX) -> i2s_trigger(START) ->
 *     i2s_read() drains the queued blocks. The DMA target alternates per
 *     RXDATAV between L/R slots in stereo and a single slot in mono-hw.
 *   - To shave ~256 B RX scratch + one LDMA channel + the RX state
 *     machine code from a TX-only firmware image, see the discussion in
 *     i2s_silabs_efr32_usart.c (option to gate RX behind a Kconfig was
 *     intentionally not taken here to keep tests unchanged).
 */

#include "common.h"
#include "unity_fixture.h"

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK

#include <string.h>
#include <zephyr/autoconf.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define LOOPBACK_SEED 0xCAFEU

static void loopback_log_wire_hint(void)
{
#if defined(CONFIG_BOARD_XG27_RB4194A)
	printk("[loopback] board=RB4194A: short **PA8 (TX)** to **PA7 (RX)** "
	       "(exp-header pin 13 = PA8); not PC0/PC1\n");
#elif defined(CONFIG_BOARD_XG27_DK2602A)
	printk("[loopback] board=DK2602A: short **PC0 (TX)** to **PC1 (RX)**\n");
#else
	printk("[loopback] board=%s: short USART0 I2S TX->RX per this board overlay\n",
	       CONFIG_BOARD);
#endif
}

static void loopback_log_preview_16(const int16_t *tx, const int16_t *rx, size_t n_words)
{
	const unsigned m = (unsigned)MIN((size_t)8, n_words);

	printk("[loopback] stereo int16 preview (first %u words):", m);
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

static void loopback_log_preview_32(const int32_t *tx, const int32_t *rx, size_t n_words)
{
	const unsigned m = (unsigned)MIN((size_t)8, n_words);

	printk("[loopback] stereo int32 preview (first %u words, low 16b):", m);
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

static void loopback_log_hex_prefix(const char *lbl, const void *buf, size_t len, size_t max_n)
{
	const uint8_t *p = buf;
	const size_t n = MIN(len, max_n);

	printk("[loopback] %s hex (first %zu of %zu bytes):\n", lbl, n, len);
	for (size_t i = 0; i < n; i++) {
		if ((i & 15U) == 0U) {
			printk("  %04zx:", i);
		}
		printk(" %02x", p[i]);
		if ((i & 15U) == 15U || i + 1U == n) {
			printk("\n");
		}
	}
}

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_TRACE
static void loopback_trace_banner(void)
{
	printk("[loopback tr] dense trace on\n");
}

static void loopback_trace_raw_u32(const void *buf, size_t len, unsigned int max_words)
{
	const size_t nw = len / sizeof(uint32_t);
	const size_t n = MIN(nw, (size_t)max_words);

	printk("[loopback tr] raw u32 LE (first %zu of %zu words):\n", n, nw);
	for (size_t i = 0; i < n; i++) {
		uint32_t w;

		memcpy(&w, (const uint8_t *)buf + i * sizeof(uint32_t), sizeof(w));
		if ((i & 7U) == 0U) {
			printk("  %04zx:", i * sizeof(uint32_t));
		}
		printk(" %08x", (unsigned int)w);
		if ((i & 7U) == 7U || i + 1U == n) {
			printk("\n");
		}
	}
}

static void loopback_trace_byte_stride(const uint8_t *p, size_t len)
{
	printk("[loopback tr] even byte indices [0,2,4..] (32 max):");
	for (size_t i = 0; i < MIN(len, (size_t)32); i += 2U) {
		printk(" %02x", p[i]);
	}
	printk("\n[loopback tr] odd byte indices [1,3,5..] (31 max):");
	for (size_t i = 1; i < MIN(len, (size_t)32); i += 2U) {
		printk(" %02x", p[i]);
	}
	printk("\n");
}

static void loopback_trace_hex_long(const char *lbl, const void *buf, size_t len)
{
	loopback_log_hex_prefix(lbl, buf, len, MIN(len, (size_t)128U));
}
#endif

TEST_GROUP(i2s_efr32_loopback);

TEST_SETUP(i2s_efr32_loopback)
{
#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_TRACE
	loopback_trace_banner();
#endif
	i2s_efr32_test_reset_device();
}

TEST_TEAR_DOWN(i2s_efr32_loopback)
{
	i2s_efr32_test_reset_device();
}

static int configure_both_dirs(uint8_t word_size)
{
	struct i2s_config cfg;

	i2s_efr32_test_make_default_cfg(&cfg, I2S_DIR_TX);
	/* Philips I2S (delay=1) can mis-align the first RX slot vs TX on USART
	 * internal+pin loopback (first right sample may read as zero) and has
	 * been observed to corrupt the second half of a block in 32-bit mode
	 * on xG27. Left-justified (no MSB delay) keeps TX and RX slot timing
	 * identical for this wired test while remaining a supported driver mode.
	 */
	cfg.format = (cfg.format & ~I2S_FMT_DATA_FORMAT_MASK) |
		     I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
	cfg.word_size  = word_size;
	cfg.block_size = (word_size == 32U) ? TEST_BLOCK_SIZE_32BIT
					    : TEST_BLOCK_SIZE_16BIT;

	if (i2s_configure(dev_i2s, I2S_DIR_TX, &cfg) != 0) {
		return -1;
	}

	cfg.mem_slab = &rx_mem_slab;
	if (i2s_configure(dev_i2s, I2S_DIR_RX, &cfg) != 0) {
		return -1;
	}
	return 0;
}

TEST(i2s_efr32_loopback, loopback_16bit_short_buffer_matches)
{
	void *tx_block;
	void *rx_block;
	size_t rx_size = 0;
	int ret_rx, ret_wr, ret_tx, ret_rd;
	/* Driver returns the TX mem_slab block when DMA completes; do not read
	 * tx_block after the transfer for diagnostics. */
	uint8_t tx_snap[TEST_BLOCK_SIZE_32BIT];

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_both_dirs(16U),
		"configure both dirs failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &tx_block, K_NO_WAIT),
		"tx slab alloc");
	i2s_efr32_test_fill_pattern_16(tx_block, TEST_BLOCK_SIZE_16BIT,
				       LOOPBACK_SEED);
	memcpy(tx_snap, tx_block, TEST_BLOCK_SIZE_16BIT);

	/* RX must be started first so the receiver is armed before any bits
	 * appear on the wire. The driver does not buffer pre-START data. */
	ret_rx = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_rx, "RX START");
	ret_wr = i2s_write(dev_i2s, tx_block, TEST_BLOCK_SIZE_16BIT);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_wr, "tx write");
	ret_tx = i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_tx, "TX START");

	ret_rd = i2s_read(dev_i2s, &rx_block, &rx_size);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_rd, "rx read");
	TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)TEST_BLOCK_SIZE_16BIT, rx_size,
		"rx block size mismatch");

	printk("[loopback16] i2s ret: RX_START=%d write=%d TX_START=%d read=%d\n",
	       ret_rx, ret_wr, ret_tx, ret_rd);
	printk("[loopback16] tx lines = snapshot before i2s_write "
	       "(TX slab block is freed after TX DMA)\n");
	loopback_log_wire_hint();
	printk("[loopback16] block=%u bytes rx_size=%zu\n",
	       (unsigned int)TEST_BLOCK_SIZE_16BIT, rx_size);
	loopback_log_preview_16((const int16_t *)tx_snap, (const int16_t *)rx_block,
				TEST_BLOCK_SIZE_16BIT / sizeof(int16_t));
	loopback_log_hex_prefix("tx-snap", tx_snap, TEST_BLOCK_SIZE_16BIT, 32U);
	loopback_log_hex_prefix("rx", rx_block, rx_size, 32U);

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_TRACE
	loopback_trace_hex_long("rx-full", rx_block, rx_size);
	loopback_trace_raw_u32(rx_block, rx_size, 32U);
	loopback_trace_byte_stride(rx_block, rx_size);
#endif

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_DEBUG
	printk("[loopback dbg16] driver RX: RXDOUBLEX + 4-byte LDMA + unpack to int16 stereo "
	       "(byte striping fe 00 ca 00 => check LDMA CTRL SIZE=word and src=RXDOUBLEX)\n");
	{
		const uint32_t *w = (const uint32_t *)rx_block;
		const unsigned n = (unsigned)MIN((size_t)4, rx_size / sizeof(uint32_t));

		printk("[loopback dbg16] first %u RX u32 (raw memory):", n);
		for (unsigned i = 0; i < n; i++) {
			printk(" %08x", w[i]);
		}
		printk("\n");
	}
#endif

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_efr32_test_verify_pattern_16(rx_block,
						 TEST_BLOCK_SIZE_16BIT,
						 LOOPBACK_SEED),
		"16-bit loopback pattern mismatch");

	k_mem_slab_free(&rx_mem_slab, rx_block);
	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
}

TEST(i2s_efr32_loopback, loopback_32bit_short_buffer_matches)
{
	void *tx_block;
	void *rx_block;
	size_t rx_size = 0;
	int ret_rx, ret_wr, ret_tx, ret_rd;
	uint8_t tx_snap[TEST_BLOCK_SIZE_32BIT];

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, configure_both_dirs(32U),
		"configure both dirs failed");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		k_mem_slab_alloc(&tx_mem_slab, &tx_block, K_NO_WAIT),
		"tx slab alloc");
	i2s_efr32_test_fill_pattern_32(tx_block, TEST_BLOCK_SIZE_32BIT,
				       LOOPBACK_SEED);
	memcpy(tx_snap, tx_block, TEST_BLOCK_SIZE_32BIT);

	ret_rx = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_rx, "RX START");
	ret_wr = i2s_write(dev_i2s, tx_block, TEST_BLOCK_SIZE_32BIT);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_wr, "tx write");
	ret_tx = i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_tx, "TX START");

	ret_rd = i2s_read(dev_i2s, &rx_block, &rx_size);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret_rd, "rx read");
	TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)TEST_BLOCK_SIZE_32BIT, rx_size,
		"rx block size mismatch");

	printk("[loopback32] i2s ret: RX_START=%d write=%d TX_START=%d read=%d\n",
	       ret_rx, ret_wr, ret_tx, ret_rd);
	printk("[loopback32] tx lines = snapshot before i2s_write "
	       "(TX slab block is freed after TX DMA)\n");
	loopback_log_wire_hint();
	printk("[loopback32] block=%u bytes rx_size=%zu\n",
	       (unsigned int)TEST_BLOCK_SIZE_32BIT, rx_size);
	loopback_log_preview_32((const int32_t *)tx_snap, (const int32_t *)rx_block,
				TEST_BLOCK_SIZE_32BIT / sizeof(int32_t));
	loopback_log_hex_prefix("tx-snap", tx_snap, TEST_BLOCK_SIZE_32BIT, 32U);
	loopback_log_hex_prefix("rx", rx_block, rx_size, 32U);

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_TRACE
	loopback_trace_hex_long("rx-full", rx_block, rx_size);
	loopback_trace_raw_u32(rx_block, rx_size, 32U);
	loopback_trace_byte_stride(rx_block, rx_size);
#endif

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK_DEBUG
	printk("[loopback dbg32] driver RX: RXDOUBLEX + 4-byte LDMA + unpack to int32 stereo\n");
	{
		const uint32_t *w = (const uint32_t *)rx_block;
		const unsigned n = (unsigned)MIN((size_t)4, rx_size / sizeof(uint32_t));

		printk("[loopback dbg32] first %u RX u32 (raw memory):", n);
		for (unsigned i = 0; i < n; i++) {
			printk(" %08x", w[i]);
		}
		printk("\n");
	}
#endif

	TEST_ASSERT_EQUAL_INT_MESSAGE(0,
		i2s_efr32_test_verify_pattern_32(rx_block,
						 TEST_BLOCK_SIZE_32BIT,
						 LOOPBACK_SEED),
		"32-bit loopback pattern mismatch");

	k_mem_slab_free(&rx_mem_slab, rx_block);
	(void)i2s_trigger(dev_i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
}

#endif /* CONFIG_I2S_EFR32_TEST_LOOPBACK */
