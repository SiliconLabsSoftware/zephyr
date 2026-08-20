/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared helpers for the Unity-driven i2s_silabs_efr32_usart on-target
 * test app: device handle, mem-slab plumbing, default-cfg builder, pattern
 * fill/verify, and a setup helper that test groups call from TEST_SETUP().
 */

#ifndef I2S_EFR32_TEST_COMMON_H
#define I2S_EFR32_TEST_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>

#define I2S_EFR32_DEV_NODE DT_ALIAS(i2s_node0)

#define TEST_FRAME_CLK_HZ  CONFIG_I2S_EFR32_TEST_FRAME_CLK_HZ
#define TEST_TIMEOUT_MS    1000

/* 32 stereo frames per block. At 16 kHz / 16-bit stereo this is 2 ms of
 * audio per block, small enough that DRAIN/STOP test cases finish quickly
 * but big enough that a single LDMA transfer is not degenerate. */
#define TEST_SAMPLES_PER_BLOCK 32
#define TEST_BLOCK_SIZE_16BIT  (TEST_SAMPLES_PER_BLOCK * 2 * sizeof(int16_t))
#define TEST_BLOCK_SIZE_32BIT  (TEST_SAMPLES_PER_BLOCK * 2 * sizeof(int32_t))

extern struct k_mem_slab tx_mem_slab;
extern struct k_mem_slab rx_mem_slab;

extern const struct device *const dev_i2s;

/* Build a baseline-valid i2s_config for the Silabs driver: 16 kHz, 16-bit,
 * I2S format, controller TX+RX, no PINGPONG/LOOPBACK/PCM, mem-slab plumbing
 * already filled in. Caller may mutate fields before passing to
 * i2s_configure(). */
void i2s_efr32_test_make_default_cfg(struct i2s_config *cfg, enum i2s_dir dir);

/* Fill `block` with a deterministic ramp pattern keyed on `seed` so that
 * verify_pattern() can detect missing/duplicated samples. Callers ensure
 * `block_bytes` matches the configured word size and is a multiple of
 * 2*word_size (one stereo frame). */
void i2s_efr32_test_fill_pattern_16(int16_t *block, size_t block_bytes,
				    uint16_t seed);
void i2s_efr32_test_fill_pattern_32(int32_t *block, size_t block_bytes,
				    uint16_t seed);

/* Returns 0 on match, -1 on first divergence. Print mismatch via printk. */
int  i2s_efr32_test_verify_pattern_16(const int16_t *block, size_t block_bytes,
				      uint16_t seed);
int  i2s_efr32_test_verify_pattern_32(const int32_t *block, size_t block_bytes,
				      uint16_t seed);

/* Common per-test cleanup: blow away any leftover state from the previous
 * test by passing NULL cfg (the driver treats this as "deconfigure both
 * directions"). Call from each TEST_SETUP() / TEST_TEAR_DOWN(). */
void i2s_efr32_test_reset_device(void);

#endif /* I2S_EFR32_TEST_COMMON_H */
