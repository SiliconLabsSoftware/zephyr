/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

/*
 * Mirrors tas2505 sample SAMPLE_EXPAND for WORD_SIZE_BITS == 32.
 * The uint16_t cast keeps only the low 16 bits of (m >> 16); widening
 * through uint32_t zero-extends into the upper half of the int32 slot.
 */
#define SAMPLE_EXPAND_W32(m) \
	((int32_t)(uint32_t)(uint16_t)((int32_t)(m) >> 16))

static inline int32_t sample_expand_correct(int16_t mono)
{
	return (int32_t)mono;
}

void test_sign_extension__sample_expand_zeros_positive__w32(void)
{
	const int16_t mono = 100;
	const int32_t expanded = SAMPLE_EXPAND_W32(mono);

	/* (int32_t)100 >> 16 == 0, so the sample macro silences positive samples. */
	TEST_ASSERT_EQUAL_INT32(0, expanded);
	TEST_ASSERT_NOT_EQUAL(sample_expand_correct(mono), expanded);
}

void test_sign_extension__sample_expand_differs_from_signed_widen(void)
{
	const int16_t mono = (int16_t)-32768;
	const int32_t buggy = SAMPLE_EXPAND_W32(mono);
	const int32_t correct = sample_expand_correct(mono);

	TEST_ASSERT_NOT_EQUAL(correct, buggy);
	TEST_ASSERT_EQUAL_INT32(-32768, correct);
	/* Zero-extended low half: 0xFFFF -> 65535, not -32768. */
	TEST_ASSERT_EQUAL_INT32(65535, buggy);
}

void test_sign_extension__correct_sign_extend_reference(void)
{
	const int16_t samples[] = { 0, 1, -1, 16384, -16384, -32768, 32767 };
	const size_t count = sizeof(samples) / sizeof(samples[0]);

	for (size_t i = 0U; i < count; i++) {
		const int32_t widened = sample_expand_correct(samples[i]);

		TEST_ASSERT_EQUAL_INT32((int32_t)samples[i], widened);
		if (samples[i] < 0) {
			TEST_ASSERT_TRUE(widened < 0);
		}
	}
}

void test_sign_extension__low16_mask_safe_compare(void)
{
	const int16_t mono = (int16_t)-42;
	const int32_t correct = sample_expand_correct(mono);
	const uint32_t low16 = (uint32_t)correct & 0xFFFFU;

	/*
	 * When only the low 16 bits carry audio (W32D16), compare via mask —
	 * do not assume the high half is sign-extended unless production code
	 * casts through (int16_t) first.
	 */
	TEST_ASSERT_EQUAL_UINT32((uint32_t)(uint16_t)mono, low16);
}
