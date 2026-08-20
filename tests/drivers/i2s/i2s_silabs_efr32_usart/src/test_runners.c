/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Unity Fixture group runners. Unity does not auto-discover tests, so
 * each TEST(group, name) must be wired up here explicitly with
 * RUN_TEST_CASE(...). Keep this in sync with the TEST() definitions in
 * the per-group files.
 */

#include "unity_fixture.h"

#include <zephyr/autoconf.h>

TEST_GROUP_RUNNER(i2s_efr32_configure)
{
	RUN_TEST_CASE(i2s_efr32_configure, valid_16bit_44k1__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, sub_8k_frame_clk__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, word_24bit__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, pcm_format__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, tx_rx_incompatible__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, left_justified_stereo__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, left_justified_mono__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, clock_format_nf_ib__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, clock_format_if_ib__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, options_bit_clk_gated__accepted);
	RUN_TEST_CASE(i2s_efr32_configure, unsupported_option_bit__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, bit_clk_target_option__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, frame_clk_target_option__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, dir_both_valid_cfg__enosys);
	RUN_TEST_CASE(i2s_efr32_configure, word_size_zero__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, channels_zero__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, block_size_zero__rejected);
	RUN_TEST_CASE(i2s_efr32_configure, block_size_not_frame_multiple__rejected);
}

TEST_GROUP_RUNNER(i2s_efr32_config_get)
{
	RUN_TEST_CASE(i2s_efr32_config_get, get_tx_after_configure__returns_same_fields);
	RUN_TEST_CASE(i2s_efr32_config_get, get_rx_when_only_tx_configured__returns_null);
	RUN_TEST_CASE(i2s_efr32_config_get, get_tx_after_deconfigure__returns_null);
	RUN_TEST_CASE(i2s_efr32_config_get, get_rx_after_configure_both_dirs__returns_rx_cfg);
}

TEST_GROUP_RUNNER(i2s_efr32_states)
{
	RUN_TEST_CASE(i2s_efr32_states, start_in_not_ready__rejected);
	RUN_TEST_CASE(i2s_efr32_states, configure_then_start_running);
	RUN_TEST_CASE(i2s_efr32_states, stop_in_ready__rejected);
	RUN_TEST_CASE(i2s_efr32_states, prepare_when_not_error__rejected);
	RUN_TEST_CASE(i2s_efr32_states, write_timeout_when_no_dma_consumer);
	RUN_TEST_CASE(i2s_efr32_states, start_rx_in_ready__running);
	RUN_TEST_CASE(i2s_efr32_states, stop_tx_running_empty_queue__ready_immediately);
	RUN_TEST_CASE(i2s_efr32_states, stop_tx_running_pending_queue__stopping_then_ready);
	RUN_TEST_CASE(i2s_efr32_states, stop_rx_running__ready);
	RUN_TEST_CASE(i2s_efr32_states, drain_tx_running__ready_after_drain);
	RUN_TEST_CASE(i2s_efr32_states, drain_tx_in_ready__rejected);
	RUN_TEST_CASE(i2s_efr32_states, drain_rx__rejected);
	RUN_TEST_CASE(i2s_efr32_states, drop_running_tx__ready);
	RUN_TEST_CASE(i2s_efr32_states, drop_both_dirs__both_ready);
	RUN_TEST_CASE(i2s_efr32_states, drop_in_not_ready__no_state_change);
	RUN_TEST_CASE(i2s_efr32_states, unknown_trigger_cmd__rejected);
}

TEST_GROUP_RUNNER(i2s_efr32_io)
{
	RUN_TEST_CASE(i2s_efr32_io, write_without_configure__eio);
	RUN_TEST_CASE(i2s_efr32_io, write_size_zero__einval);
	RUN_TEST_CASE(i2s_efr32_io, write_size_gt_block_size__einval);
	RUN_TEST_CASE(i2s_efr32_io, write_size_not_frame_aligned__einval);
	RUN_TEST_CASE(i2s_efr32_io, write_size_not_dma_aligned__einval);
	RUN_TEST_CASE(i2s_efr32_io, write_in_ready_state__queues_ok);
	RUN_TEST_CASE(i2s_efr32_io, write_when_tx_in_error__eio);
	RUN_TEST_CASE(i2s_efr32_io, read_without_configure__eio);
	RUN_TEST_CASE(i2s_efr32_io, read_no_data_no_wait__ebusy);
	RUN_TEST_CASE(i2s_efr32_io, read_no_data_with_short_timeout__eagain);
	RUN_TEST_CASE(i2s_efr32_io, read_after_stop__no_garbage);
}

TEST_GROUP_RUNNER(i2s_efr32_errors)
{
	RUN_TEST_CASE(i2s_efr32_errors, txuf_underrun_transitions_to_error);
	RUN_TEST_CASE(i2s_efr32_errors, rxof_overrun_transitions_to_error);
	RUN_TEST_CASE(i2s_efr32_errors, prepare_after_underrun__back_to_ready_then_runs);
}

TEST_GROUP_RUNNER(i2s_efr32_clock)
{
	RUN_TEST_CASE(i2s_efr32_clock, nonzero_clkdiv_after_configure);
	RUN_TEST_CASE(i2s_efr32_clock, doubling_word_size_halves_divider);
	RUN_TEST_CASE(i2s_efr32_clock, halving_frame_clk_doubles_divider);
}

TEST_GROUP_RUNNER(i2s_efr32_mclk)
{
	RUN_TEST_CASE(i2s_efr32_mclk, clkout_in_2pct_range);
}

#ifdef CONFIG_I2S_EFR32_TEST_LOOPBACK
TEST_GROUP_RUNNER(i2s_efr32_loopback)
{
	RUN_TEST_CASE(i2s_efr32_loopback, loopback_16bit_short_buffer_matches);
	RUN_TEST_CASE(i2s_efr32_loopback, loopback_32bit_short_buffer_matches);
}
#endif
