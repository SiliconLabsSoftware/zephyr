/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "unity.h"
#include "test_i2s_efr32_common.h"
#include "test_runner_log.h"

#define UNITY_TEST_COUNT 81

/* Wrapper so each RUN_TEST_LOG(func) picks up the registered total. */
#define RUN_TEST_LOG(func) RUN_TEST_LOG_COUNT(&log, func, UNITY_TEST_COUNT)

void tc_clock_teardown(void);

void setUp(void)
{
	test_reset_driver();
}

void tearDown(void)
{
	tc_clock_teardown();
	test_reset_driver();
}

/* --- configure / init --- */
void test_init__device_is_ready(void);
void test_init__i2s_tx_alias_resolves(void);
void test_configure__stop_with_null_cfg(void);
void test_configure__config_get_null_before_configure(void);
void test_configure__config_get_after_success__w16(void);
void test_configure__config_get_after_success__w32(void);
void test_configure__config_get_null_after_stop__w16(void);
void test_configure__rejects_word_size_zero(void);
void test_configure__accepts_mono_channels_one__w16(void);
void test_configure__rejects_channels_three__w16(void);
void test_configure__rejects_block_size_zero__w16(void);
void test_configure__rejects_block_size_not_frame_aligned__w16(void);
void test_configure__rejects_block_size_not_frame_aligned__w32(void);
void test_configure__rejects_dir_both__w16(void);
void test_configure__rejects_word_size_unsupported(void);
void test_configure__rejects_frame_clk_below_min__w16(void);
void test_configure__rejects_unsupported_options__w16(void);
void test_configure__rejects_bit_clk_target__w16(void);
void test_configure__rejects_frame_clk_target__w16(void);
void test_configure__rejects_format_right_justified__w16(void);
void test_configure__rejects_format_pcm_short__w16(void);
void test_configure__rejects_mismatched_tx_rx_freq__w16(void);
void test_configure__rejects_mismatched_tx_rx_word_size(void);
void test_configure__rejects_mismatched_tx_rx_format__w16(void);
void test_configure__success_sets_ready__w16(void);
void test_configure__success_sets_ready__w32(void);
void test_configure__left_justified_format_ok__w16(void);
void test_configure__configure_rx_then_tx_matching__w16(void);
void test_configure__good_board_expect_zero__w16(void);
void test_configure__good_board_expect_zero__w32(void);

/* --- write --- */
void test_write__no_configure_returns_eio(void);
void test_write__after_stop_returns_eio__w16(void);
void test_write__rejects_size_zero__w16(void);
void test_write__rejects_size_gt_block__w16(void);
void test_write__rejects_size_not_frame_aligned__w16(void);
void test_write__rejects_size_not_frame_aligned__w32(void);
void test_write__valid_in_ready_returns_zero__w16(void);
void test_write__valid_in_ready_returns_zero__w32(void);
void test_write__valid_in_running_returns_zero__w16(void);
void test_write__valid_in_running_returns_zero__w32(void);
void test_write__ring_full_no_wait_returns_ebusy__w16(void);
void test_write__ring_full_no_wait_returns_ebusy__w32(void);
void test_write__stopping_state_returns_eio__w16(void);
void test_write__stopping_state_returns_eio__w32(void);

/* --- read --- */
void test_read__no_configure_returns_eio(void);
void test_read__timeout_no_data_returns_ebusy__w16(void);
void test_read__timeout_no_data_returns_ebusy__w32(void);
void test_read__short_timeout_returns_eagain__w16(void);
void test_read__live_data_after_start(void);

/* --- trigger --- */
void test_trigger__start_from_ready__w16(void);
void test_trigger__start_from_ready__w32(void);
void test_trigger__start_without_configure_is_noop(void);
void test_trigger__stop_tx_empty_queue_immediate_ready__w16(void);
void test_trigger__stop_tx_empty_queue_immediate_ready__w32(void);
void test_trigger__stop_tx_nonempty_queue_drains__w16(void);
void test_trigger__stop_tx_nonempty_queue_drains__w32(void);
void test_trigger__stop_rx_immediate_ready__w16(void);
void test_trigger__stop_rx_immediate_ready__w32(void);
void test_trigger__drain_from_running__w16(void);
void test_trigger__drain_not_running_rejects__w16(void);
void test_trigger__drain_rx_rejects__w16(void);
void test_trigger__drop_with_cfg_returns_ready__w16(void);
void test_trigger__drop_without_cfg_returns_not_ready(void);
void test_trigger__prepare_not_error_rejects__w16(void);
void test_trigger__unknown_cmd_rejects__w16(void);
void test_trigger__stop_not_running_rejects__w16(void);

/* --- sign extension --- */
void test_sign_extension__sample_expand_zeros_positive__w32(void);
void test_sign_extension__sample_expand_differs_from_signed_widen(void);
void test_sign_extension__correct_sign_extend_reference(void);
void test_sign_extension__low16_mask_safe_compare(void);

/* --- clock measurement (test-image-only TIMER1 + PRS) --- */
void test_clock__mclk_routed_at_expected_ratio(void);
void test_clock__bclk_matches_configured_rate__w16(void);
void test_clock__bclk_matches_configured_rate__w32(void);

int main(void)
{
	struct test_runner_log log;
	int unity_rc;

	test_runner_log_init(&log);

	printk("\n");
	printk("=== I2S silabs,efr32-usart-i2s Unity tests ===\n");
	printk("device: %s\n", I2S_DEV->name);
	printk("TX ring depth: %u  RX ring depth: %u\n", TX_RING_DEPTH, RX_RING_DEPTH);
	printk("Unity v2.5.2  registered tests: %d  wiring: none required\n",
	       UNITY_TEST_COUNT);
	printk("==============================================\n");
	unity_output_flush();

	/* Short tag keeps Unity's per-test line prefix small (Eclipse format). */
	UnityBegin("i2s");

	/* init / configure */
	printk("\n--- Group: init & configure (34) ---\n");
	RUN_TEST_LOG(test_init__device_is_ready);
	RUN_TEST_LOG(test_init__i2s_tx_alias_resolves);
	RUN_TEST_LOG(test_configure__stop_with_null_cfg);
	RUN_TEST_LOG(test_configure__config_get_null_before_configure);
	RUN_TEST_LOG(test_configure__config_get_after_success__w16);
	RUN_TEST_LOG(test_configure__config_get_after_success__w32);
	RUN_TEST_LOG(test_configure__config_get_null_after_stop__w16);
	RUN_TEST_LOG(test_configure__rejects_word_size_zero);
	RUN_TEST_LOG(test_configure__accepts_mono_channels_one__w16);
	RUN_TEST_LOG(test_configure__rejects_channels_three__w16);
	RUN_TEST_LOG(test_configure__rejects_block_size_zero__w16);
	RUN_TEST_LOG(test_configure__rejects_block_size_not_frame_aligned__w16);
	RUN_TEST_LOG(test_configure__rejects_block_size_not_frame_aligned__w32);
	RUN_TEST_LOG(test_configure__rejects_dir_both__w16);
	RUN_TEST_LOG(test_configure__rejects_word_size_unsupported);
	RUN_TEST_LOG(test_configure__rejects_frame_clk_below_min__w16);
	RUN_TEST_LOG(test_configure__rejects_unsupported_options__w16);
	RUN_TEST_LOG(test_configure__rejects_bit_clk_target__w16);
	RUN_TEST_LOG(test_configure__rejects_frame_clk_target__w16);
	RUN_TEST_LOG(test_configure__rejects_format_right_justified__w16);
	RUN_TEST_LOG(test_configure__rejects_format_pcm_short__w16);
	RUN_TEST_LOG(test_configure__rejects_mismatched_tx_rx_freq__w16);
	RUN_TEST_LOG(test_configure__rejects_mismatched_tx_rx_word_size);
	RUN_TEST_LOG(test_configure__rejects_mismatched_tx_rx_format__w16);
	RUN_TEST_LOG(test_configure__success_sets_ready__w16);
	RUN_TEST_LOG(test_configure__success_sets_ready__w32);
	RUN_TEST_LOG(test_configure__left_justified_format_ok__w16);
	RUN_TEST_LOG(test_configure__configure_rx_then_tx_matching__w16);
	RUN_TEST_LOG(test_configure__good_board_expect_zero__w16);
	RUN_TEST_LOG(test_configure__good_board_expect_zero__w32);

	printk("\n--- Group: write (14) ---\n");
	RUN_TEST_LOG(test_write__no_configure_returns_eio);
	RUN_TEST_LOG(test_write__after_stop_returns_eio__w16);
	RUN_TEST_LOG(test_write__rejects_size_zero__w16);
	RUN_TEST_LOG(test_write__rejects_size_gt_block__w16);
	RUN_TEST_LOG(test_write__rejects_size_not_frame_aligned__w16);
	RUN_TEST_LOG(test_write__rejects_size_not_frame_aligned__w32);
	RUN_TEST_LOG(test_write__valid_in_ready_returns_zero__w16);
	RUN_TEST_LOG(test_write__valid_in_ready_returns_zero__w32);
	RUN_TEST_LOG(test_write__valid_in_running_returns_zero__w16);
	RUN_TEST_LOG(test_write__valid_in_running_returns_zero__w32);
	RUN_TEST_LOG(test_write__ring_full_no_wait_returns_ebusy__w16);
	RUN_TEST_LOG(test_write__ring_full_no_wait_returns_ebusy__w32);
	RUN_TEST_LOG(test_write__stopping_state_returns_eio__w16);
	RUN_TEST_LOG(test_write__stopping_state_returns_eio__w32);

	printk("\n--- Group: read (5) ---\n");
	RUN_TEST_LOG(test_read__no_configure_returns_eio);
	RUN_TEST_LOG(test_read__timeout_no_data_returns_ebusy__w16);
	RUN_TEST_LOG(test_read__timeout_no_data_returns_ebusy__w32);
	RUN_TEST_LOG(test_read__short_timeout_returns_eagain__w16);
	RUN_TEST_LOG(test_read__live_data_after_start);

	printk("\n--- Group: trigger & ISR (17) ---\n");
	RUN_TEST_LOG(test_trigger__start_from_ready__w16);
	RUN_TEST_LOG(test_trigger__start_from_ready__w32);
	RUN_TEST_LOG(test_trigger__start_without_configure_is_noop);
	RUN_TEST_LOG(test_trigger__stop_tx_empty_queue_immediate_ready__w16);
	RUN_TEST_LOG(test_trigger__stop_tx_empty_queue_immediate_ready__w32);
	RUN_TEST_LOG(test_trigger__stop_tx_nonempty_queue_drains__w16);
	RUN_TEST_LOG(test_trigger__stop_tx_nonempty_queue_drains__w32);
	RUN_TEST_LOG(test_trigger__stop_rx_immediate_ready__w16);
	RUN_TEST_LOG(test_trigger__stop_rx_immediate_ready__w32);
	RUN_TEST_LOG(test_trigger__drain_from_running__w16);
	RUN_TEST_LOG(test_trigger__drain_not_running_rejects__w16);
	RUN_TEST_LOG(test_trigger__drain_rx_rejects__w16);
	RUN_TEST_LOG(test_trigger__drop_with_cfg_returns_ready__w16);
	RUN_TEST_LOG(test_trigger__drop_without_cfg_returns_not_ready);
	RUN_TEST_LOG(test_trigger__prepare_not_error_rejects__w16);
	RUN_TEST_LOG(test_trigger__unknown_cmd_rejects__w16);
	RUN_TEST_LOG(test_trigger__stop_not_running_rejects__w16);

	printk("\n--- Group: sign extension (4) ---\n");
	RUN_TEST_LOG(test_sign_extension__sample_expand_zeros_positive__w32);
	RUN_TEST_LOG(test_sign_extension__sample_expand_differs_from_signed_widen);
	RUN_TEST_LOG(test_sign_extension__correct_sign_extend_reference);
	RUN_TEST_LOG(test_sign_extension__low16_mask_safe_compare);

	printk("\n--- Group: clock measurement (3) ---\n");
	RUN_TEST_LOG(test_clock__mclk_routed_at_expected_ratio);
	RUN_TEST_LOG(test_clock__bclk_matches_configured_rate__w16);
	RUN_TEST_LOG(test_clock__bclk_matches_configured_rate__w32);

	unity_output_flush();
	unity_rc = UnityEnd();
	test_runner_log_print_summary(&log, UNITY_TEST_COUNT);

	return unity_rc;
}
