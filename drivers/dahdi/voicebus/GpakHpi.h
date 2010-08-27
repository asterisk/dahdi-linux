/*
 * Copyright (c) 2001, Adaptive Digital Technologies, Inc.
 *
 * File Name: GpakHpi.h
 *
 * Description:
 *   This file contains common definitions related to the G.PAK interface
 *   between a host processor and a DSP processor via the Host Port Interface.
 *
 * Version: 1.0
 *
 * Revision History:
 *   10/17/01 - Initial release.
 *
 * This program has been released under the terms of the GPL version 2 by
 * permission of Adaptive Digital Technologies, Inc.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#ifndef _GPAKHPI_H  /* prevent multiple inclusion */
#define _GPAKHPI_H


/* Definition of G.PAK Command/Reply message type codes. */
#define MSG_NULL_REPLY 0            /* Null Reply (unsupported Command) */
#define MSG_SYS_CONFIG_RQST 1       /* System Configuration Request */
#define MSG_SYS_CONFIG_REPLY 2      /* System Configuration Reply */
#define MSG_READ_SYS_PARMS 3        /* Read System Parameters */
#define MSG_READ_SYS_PARMS_REPLY 4  /* Read System Parameters Reply */
#define MSG_WRITE_SYS_PARMS 5       /* Write System Parameters */
#define MSG_WRITE_SYS_PARMS_REPLY 6 /* Write System Parameters Reply */
#define MSG_CONFIGURE_PORTS 7       /* Configure Serial Ports */
#define MSG_CONFIG_PORTS_REPLY 8    /* Configure Serial Ports Reply */
#define MSG_CONFIGURE_CHANNEL 9     /* Configure Channel */
#define MSG_CONFIG_CHAN_REPLY 10    /* Configure Channel Reply */
#define MSG_TEAR_DOWN_CHANNEL 11    /* Tear Down Channel */
#define MSG_TEAR_DOWN_REPLY 12      /* Tear Down Channel Reply */
#define MSG_CHAN_STATUS_RQST 13     /* Channel Status Request */
#define MSG_CHAN_STATUS_REPLY 14    /* Channel Status Reply */

#define MSG_TEST_MODE 17            /* Configure/Perform Test Mode */
#define MSG_TEST_REPLY 18           /* Configure/Perform Test Mode Reply */

#define MSG_ALG_CONTROL 27             /* algorithm control */
#define MSG_ALG_CONTROL_REPLY 28       /* algorithm control reply */
#define MSG_GET_TXCID_ADDRESS 29       /* get tx cid buffer start address */
#define MSG_GET_TXCID_ADDRESS_REPLY 30 /* get tx cid buffer start addr reply */

#define MSG_PING 35                    /* ping command */
#define MSG_PING_REPLY 36              /* ping command reply */
#define MSG_SERIAL_TXVAL 37            /* transmit serial fixed value */
#define MSG_SERIAL_TXVAL_REPLY 38      /* transmit serial fixed value reply */
#define MSG_TDM_LOOPBACK 39            /* tdm loopback control */
#define MSG_TDM_LOOPBACK_REPLY 40      /* tdm loopback control reply */
#define MSG_RESET_USAGE_STATS 41       /* reset cpu usage stats */
#define MSG_RESET_USAGE_STATS_REPLY 42 /* reset cpu usage stats reply */

#define MSG_RESET_FRAME_STATS 47       /* reset framing stats */
#define MSG_RESET_FRAME_STATS_REPLY 48 /* reset framing stats reply */

#define MSG_READ_DSP_MEMORY         49 /* read small section of DSP's memory */
#define MSG_READ_DSP_MEMORY_REPLY   50 /* read memory reply */

#define MSG_ACCESSGPIO              51
#define MSG_ACCESSGPIO_REPLY        52
#endif  /* prevent multiple inclusion */
