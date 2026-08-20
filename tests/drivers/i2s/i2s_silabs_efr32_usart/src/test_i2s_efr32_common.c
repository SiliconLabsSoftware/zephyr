/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_i2s_efr32_common.h"

K_MEM_SLAB_DEFINE(tx_test_slab, WB_UP(TEST_BLOCK_SIZE), TEST_NUM_BLOCKS, WB_UP(32));
K_MEM_SLAB_DEFINE(rx_test_slab, WB_UP(TEST_BLOCK_SIZE), TEST_NUM_BLOCKS, WB_UP(32));
