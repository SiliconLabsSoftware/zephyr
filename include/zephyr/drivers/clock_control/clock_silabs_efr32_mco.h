/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CLOCK_SILABS_EFR32_MCO_H_
#define CLOCK_SILABS_EFR32_MCO_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

int silabs_mco_enable(const struct device *dev);
int silabs_mco_disable(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_SILABS_EFR32_MCO_H_ */
