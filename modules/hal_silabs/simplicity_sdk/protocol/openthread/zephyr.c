/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Silicon Laboratories Inc.
 *
 * This file implements the OpenThread platform abstraction for radio
 * communication on Silicon Labs EFR32.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/net/net_pkt.h>

#include "alarm.h"

#include "platform-zephyr.h"
#include "platform-efr32.h"

/* Redirect efr32 Alarm APIs to platform-specific implementations
 */

OT_TOOL_WEAK void sl_openthread_init(void)
{
	/* Placeholder for enabling Silabs specific features available only
	 * through Simplicity Studio.
	 */
}

void platformRadioInit(void)
{
	sl_openthread_init();
	efr32RadioInit();
}

void platformRadioProcess(otInstance *aInstance)
{
	efr32RadioProcess(aInstance);
}

void platformAlarmInit(void)
{
	efr32AlarmInit();
}

void platformAlarmProcess(otInstance *aInstance)
{
	efr32AlarmProcess(aInstance);
}

/* Openthread APIs called from Zephyr's Thread stack (openthread.c)
 */

int notify_new_rx_frame(struct net_pkt *pkt)
{
	return 0;
}

int notify_new_tx_frame(struct net_pkt *pkt)
{
	return 0;
}
