/*
 * Copyright (c) 2002 - 2004, Adaptive Digital Technologies, Inc.
 *
 * File Name: GpakErrs.h
 *
 * Description:
 *   This file contains DSP reply status codes used by G.PAK API functions to
 *   indicate specific errors.
 *
 * Version: 1.0
 *
 * Revision History:
 *   10/17/01 - Initial release.
 *   07/03/02 - Updates for conferencing.
 *   06/15/04 - Tone type updates.
 *
 * This program has been released under the terms of the GPL version 2 by
 * permission of Adaptive Digital Technologies, Inc.
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

#ifndef _GPAKERRS_H  /* prevent multiple inclusion */
#define _GPAKERRS_H

/* Configure Serial Ports reply status codes. */
typedef enum
{
    Pc_Success = 0,             /* serial ports configured successfully */
    Pc_ChannelsActive = 1,      /* unable to configure while channels active  */
    Pc_TooManySlots1 = 2,       /* too many slots selected for port 1 */
    Pc_InvalidBlockCombo1 = 3,  /* invalid combination of blocks for port 1 */
    Pc_NoSlots1 = 4,            /* no slots selected for port 1 */
    Pc_InvalidSlots1 = 5,       /* invalid slot (> max) selected for port 1 */
    Pc_TooManySlots2 = 6,       /* too many slots selected for port 2 */
    Pc_InvalidBlockCombo2 = 7,  /* invalid combination of blocks for port 2 */
    Pc_NoSlots2 = 8,            /* no slots selected for port 2 */
    Pc_InvalidSlots2 = 9,       /* invalid slot (> max) selected for port 2 */
    Pc_TooManySlots3 = 10,      /* too many slots selected for port 3 */
    Pc_InvalidBlockCombo3 = 11, /* invalid combination of blocks for port 3 */
    Pc_NoSlots3 = 12,           /* no slots selected for port 3 */
    Pc_InvalidSlots3 = 13       /* invalid slot (> max) selected for port 3 */
} GPAK_PortConfigStat_t;

/* Configure Channel reply status codes. */
typedef enum
{
    Cc_Success = 0,                 /* channel configured successfully */
    Cc_InvalidChannelType = 1,      /* invalid Channel Type */
    Cc_InvalidChannel  = 2,         /* invalid Channel A Id */
    Cc_ChannelActiveA = 3,          /* Channel A is currently active */
    Cc_InvalidInputPortA = 4,       /* invalid Input A Port */
    Cc_InvalidInputSlotA = 5,       /* invalid Input A Slot */
    Cc_BusyInputSlotA = 6,          /* busy Input A Slot */
    Cc_InvalidOutputPortA = 7,      /* invalid Output A Port */
    Cc_InvalidOutputSlotA = 8,      /* invalid Output A Slot */
    Cc_BusyOutputSlotA = 9,         /* busy Output A Slot */
    Cc_InvalidInputPortB = 10,      /* invalid Input B Port */
    Cc_InvalidInputSlotB = 11,      /* invalid Input B Slot */
    Cc_BusyInputSlotB = 12,         /* busy Input B Slot */
    Cc_InvalidPktInCodingA = 13,    /* invalid Packet In A Coding */
    Cc_InvalidPktOutCodingA = 14,   /* invalid Packet Out A Coding */
    Cc_InvalidPktInSizeA = 15,      /* invalid Packet In A Frame Size */
    Cc_InvalidPktOutSizeA = 16,     /* invalid Packet Out A Frame Size */

    Cc_ChanTypeNotConfigured = 21,  /* channel type was not configured */
    Cc_InsuffECResources = 22,      /* insufficient ecan resources avail. */
    Cc_InsuffTDMResources = 23,     /* insufficient tdm block resources avail. */

    Cc_InsuffPktBufResources = 25,  /* insufficient pkt buffer resources avail. */
    Cc_InsuffPcmBufResources = 26,  /* insufficient pcm buffer resources avail. */

    Cc_BadPcmEcNlpType = 30,        /* invalid EC Nlp type */
    Cc_BadPcmEcTapLength = 31,      /* invalid EC tap length */
    Cc_BadPcmEcDblTalkThresh = 32,  /* invalid EC double-talk threshold */
    Cc_BadPcmEcNlpThreshold = 33,   /* invalid EC Nlp threshold */
    Cc_BadPcmEcCngThreshold = 34,   /* invalid EC Cng threshold */
    Cc_BadPcmEcAdaptLimit = 35,     /* invalid EC Adapt Limit */
    Cc_BadPcmEcCrossCorrLim = 36,   /* invalid EC Cross Correlation Limit */
    Cc_BadPcmEcNumFirSegs = 37,     /* invalid EC Number of FirSegments */
    Cc_BadPcmEcFirSegLen = 38,      /* invalid EC Fir Segment Length */

    /*Cc_InvalidNumEcsEnabled = 48, */   /* more than 1 Ec enabled on channel */
    Cc_InvalidFrameRate = 49,        /* invalid gpak frame rate */
    Cc_InvalidSoftCompand = 50,      /* invalid softCompanding type */

	Cc_InvalidMuteToneA = 51,        /* invalid MuteToneA set, no detector */
	Cc_InvalidMuteToneB = 52,         /* invalid MuteToneB set, no detector */
	Cc_InsuffFaxCngDetResources = 53,    /* insufficient tdm block resources avail. */
	Cc_PortDmaNotStarted = 54,	/* SerialPort not ready  */
	Cc_ChannelDebugActive = 55,	/* Debug Channel is not active  */
	Cc_ChannelDebugEnabled = 56	/* Channel already been debugged  */
} GPAK_ChannelConfigStat_t;

/* Tear Down Channel reply status codes. */
typedef enum
{
    Td_Success = 0,                 /* channel torn down successfully */
    Td_InvalidChannel = 1,          /* invalid Channel Id */
    Td_ChannelNotActive = 2         /* channel is not active */
} GPAK_TearDownChanStat_t;


typedef enum
{
    Ac_Success = 0,           /* algorithm control is successfull */
    Ac_InvalidChannel = 1,    /* invalid channel identifier */
    Ac_InvalidCode = 2,       /* invalid algorithm control code */
    Ac_ECNotEnabled = 3,      /* echo canceller was not allocated */ 
    Ac_InvalidSoftComp = 4,   /* invalid softcompanding, 'cause serial port not in companding mode */
	Ac_InvalidDTMFMuteA = 5,  /* A side invalid Mute, since no dtmf detector */
	Ac_InvalidDTMFMuteB = 6,  /* B side invalid Mute, since no dtmf detector */
	Ac_InvalidFaxCngA = 7,    /* A side FAXCNG detector not available */
	Ac_InvalidFaxCngB = 8,     /* B side FAXCNG detector not available */
	Ac_InvalidSysConfig = 9    /* No new system parameters (DTMF config) wrriten yet */
} GPAK_AlgControlStat_t;

/* Write System Parameters reply status codes. */
typedef enum
{
    Sp_Success = 0,             /* System Parameters written successfully */
    Sp_BadTwistThresh = 29        /* invalid twist threshold */

} GPAK_SysParmsStat_t;

#endif  /* prevent multiple inclusion */



















