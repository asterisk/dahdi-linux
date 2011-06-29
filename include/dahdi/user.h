/*
 * DAHDI Telephony Interface
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001 - 2008 Digium, Inc.
 *
 * All rights reserved.
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
 * the GNU Lesser General Public License Version 2.1 as published
 * by the Free Software Foundation. See the LICENSE.LGPL file
 * included with this program for more details.
 *
 * In addition, when this program is distributed with Asterisk in
 * any form that would qualify as a 'combined work' or as a
 * 'derivative work' (but not mere aggregation), you can redistribute
 * and/or modify the combination under the terms of the license
 * provided with that copy of Asterisk, instead of the license
 * terms granted here.
 */

#ifndef _DAHDI_USER_H
#define _DAHDI_USER_H

#include <linux/types.h>
#include <linux/ioctl.h>
#include <dahdi/dahdi_config.h>

#ifndef ELAST
#define ELAST 500
#endif

/* Per-span configuration values */
#define DAHDI_CONFIG_TXLEVEL	7				/* bits 0-2 are tx level */

/* Line configuration */
/* These apply to T1 */
#define DAHDI_CONFIG_D4	 	(1 << 4)
#define DAHDI_CONFIG_ESF	(1 << 5)
#define DAHDI_CONFIG_AMI	(1 << 6)
#define DAHDI_CONFIG_B8ZS	(1 << 7)
/* These apply to E1 */
#define DAHDI_CONFIG_CCS	(1 << 8)			/* CCS (ISDN) instead of CAS (Robbed Bit) */
#define DAHDI_CONFIG_HDB3	(1 << 9)			/* HDB3 instead of AMI (line coding) */
#define DAHDI_CONFIG_CRC4	(1 << 10)			/* CRC4 framing */
#define DAHDI_CONFIG_NOTOPEN	(1 << 16)
/* These apply to BRI */
#define DAHDI_CONFIG_NTTE	(1 << 11)			/* To enable NT mode, set this bit to 1, for TE this should be 0 */
#define DAHDI_CONFIG_TERM	(1 << 12)			/* To enable Termination resistance set this bit to 1 */

/* Signalling types */
#define DAHDI_SIG_BROKEN	(1 << 31)			/* The port is broken and/or failed initialization */

#define __DAHDI_SIG_FXO		(1 << 12)			/* Never use directly */
#define __DAHDI_SIG_FXS		(1 << 13)			/* Never use directly */

#define DAHDI_SIG_NONE		(0)				/* Channel not configured */
#define DAHDI_SIG_FXSLS		((1 << 0) | __DAHDI_SIG_FXS)	/* FXS, Loopstart */
#define DAHDI_SIG_FXSGS		((1 << 1) | __DAHDI_SIG_FXS)	/* FXS, Groundstart */
#define DAHDI_SIG_FXSKS		((1 << 2) | __DAHDI_SIG_FXS)	/* FXS, Kewlstart */

#define DAHDI_SIG_FXOLS		((1 << 3) | __DAHDI_SIG_FXO)	/* FXO, Loopstart */
#define DAHDI_SIG_FXOGS		((1 << 4) | __DAHDI_SIG_FXO)	/* FXO, Groupstart */
#define DAHDI_SIG_FXOKS		((1 << 5) | __DAHDI_SIG_FXO)	/* FXO, Kewlstart */

#define DAHDI_SIG_EM		(1 << 6)			/* Ear & Mouth (E&M) */

/* The following are all variations on clear channel */

#define __DAHDI_SIG_DACS	(1 << 16)

#define DAHDI_SIG_CLEAR		(1 << 7)				/* Clear channel */
#define DAHDI_SIG_HDLCRAW	((1 << 8)  | DAHDI_SIG_CLEAR)		/* Raw unchecked HDLC */
#define DAHDI_SIG_HDLCFCS	((1 << 9)  | DAHDI_SIG_HDLCRAW)		/* HDLC with FCS calculation */
#define DAHDI_SIG_HDLCNET	((1 << 10) | DAHDI_SIG_HDLCFCS)		/* HDLC Network */
#define DAHDI_SIG_SLAVE		(1 << 11) 				/* Slave to another channel */
#define DAHDI_SIG_SF		(1 << 14)				/* Single Freq. tone only, no sig bits */
#define DAHDI_SIG_CAS		(1 << 15)				/* Just get bits */
#define DAHDI_SIG_DACS		(__DAHDI_SIG_DACS | DAHDI_SIG_CLEAR)	/* Cross connect */
#define DAHDI_SIG_EM_E1		(1 << 17)				/* E1 E&M Variation */
#define DAHDI_SIG_DACS_RBS	((1 << 18) | __DAHDI_SIG_DACS)		/* Cross connect w/ RBS */
#define DAHDI_SIG_HARDHDLC	((1 << 19) | DAHDI_SIG_CLEAR)
#define DAHDI_SIG_MTP2		((1 << 20) | DAHDI_SIG_HDLCFCS)		/* MTP2 support  Need HDLC bitstuff and FCS calcuation too */

/* tone flag values */
#define DAHDI_REVERSE_RXTONE	1  /* reverse polarity rx tone logic */
#define DAHDI_REVERSE_TXTONE	2  /* reverse polarity tx tone logic */

#define DAHDI_ABIT		(1 << 3)
#define DAHDI_BBIT		(1 << 2)
#define DAHDI_CBIT		(1 << 1)
#define DAHDI_DBIT		(1 << 0)

#define DAHDI_BITS_ABCD (DAHDI_ABIT | DAHDI_BBIT | DAHDI_CBIT | DAHDI_DBIT)
#define DAHDI_BITS_ABD (DAHDI_ABIT | DAHDI_BBIT | DAHDI_DBIT)
#define DAHDI_BITS_ACD (DAHDI_ABIT | DAHDI_CBIT | DAHDI_DBIT)
#define DAHDI_BITS_BCD (DAHDI_BBIT | DAHDI_CBIT | DAHDI_DBIT)
#define DAHDI_BITS_AC (DAHDI_ABIT | DAHDI_CBIT)
#define DAHDI_BITS_BD (DAHDI_BBIT | DAHDI_DBIT)

#define DAHDI_MAJOR		196

#define DAHDI_MAX_BLOCKSIZE	8192
#define DAHDI_DEFAULT_NUM_BUFS	2
#define DAHDI_MAX_NUM_BUFS	32
#define DAHDI_MAX_BUF_SPACE	32768

#define DAHDI_DEFAULT_BLOCKSIZE 1024
#define DAHDI_DEFAULT_MTR_MRU	2048

/*! Define the default network block size */
#define DAHDI_DEFAULT_MTU_MRU	2048

#define DAHDI_POLICY_IMMEDIATE	0		/* Start play/record immediately */
#define DAHDI_POLICY_WHEN_FULL	1		/* Start play/record when buffer is full */
#define DAHDI_POLICY_HALF_FULL	2		/* Start play/record when buffer is half full.
						   Note -- This policy only works on tx buffers */

#define DAHDI_GET_PARAMS_RETURN_MASTER 0x40000000

#define DAHDI_TONE_ZONE_MAX		128

#define DAHDI_TONE_ZONE_DEFAULT 	-1	/* To restore default */

#define DAHDI_TONE_STOP		-1
#define DAHDI_TONE_DIALTONE	0
#define DAHDI_TONE_BUSY		1
#define DAHDI_TONE_RINGTONE	2
#define DAHDI_TONE_CONGESTION	3
#define DAHDI_TONE_CALLWAIT	4
#define DAHDI_TONE_DIALRECALL	5
#define DAHDI_TONE_RECORDTONE	6
#define DAHDI_TONE_INFO		7
#define DAHDI_TONE_CUST1		8
#define DAHDI_TONE_CUST2		9
#define DAHDI_TONE_STUTTER		10
#define DAHDI_TONE_MAX		16

#define DAHDI_TONE_DTMF_BASE	64
#define DAHDI_TONE_MFR1_BASE	80
#define DAHDI_TONE_MFR2_FWD_BASE	96
#define DAHDI_TONE_MFR2_REV_BASE	112

enum {
	DAHDI_TONE_DTMF_0 = DAHDI_TONE_DTMF_BASE,
	DAHDI_TONE_DTMF_1,
	DAHDI_TONE_DTMF_2,
	DAHDI_TONE_DTMF_3,
	DAHDI_TONE_DTMF_4,
	DAHDI_TONE_DTMF_5,
	DAHDI_TONE_DTMF_6,
	DAHDI_TONE_DTMF_7,
	DAHDI_TONE_DTMF_8,
	DAHDI_TONE_DTMF_9,
	DAHDI_TONE_DTMF_s,
	DAHDI_TONE_DTMF_p,
	DAHDI_TONE_DTMF_A,
	DAHDI_TONE_DTMF_B,
	DAHDI_TONE_DTMF_C,
	DAHDI_TONE_DTMF_D
};

#define DAHDI_TONE_DTMF_MAX DAHDI_TONE_DTMF_D

enum {
	DAHDI_TONE_MFR1_0 = DAHDI_TONE_MFR1_BASE,
	DAHDI_TONE_MFR1_1,
	DAHDI_TONE_MFR1_2,
	DAHDI_TONE_MFR1_3,
	DAHDI_TONE_MFR1_4,
	DAHDI_TONE_MFR1_5,
	DAHDI_TONE_MFR1_6,
	DAHDI_TONE_MFR1_7,
	DAHDI_TONE_MFR1_8,
	DAHDI_TONE_MFR1_9,
	DAHDI_TONE_MFR1_KP,
	DAHDI_TONE_MFR1_ST,
	DAHDI_TONE_MFR1_STP,
	DAHDI_TONE_MFR1_ST2P,
	DAHDI_TONE_MFR1_ST3P,
};

#define DAHDI_TONE_MFR1_MAX DAHDI_TONE_MFR1_ST3P

enum {
	DAHDI_TONE_MFR2_FWD_1 = DAHDI_TONE_MFR2_FWD_BASE,
	DAHDI_TONE_MFR2_FWD_2,
	DAHDI_TONE_MFR2_FWD_3,
	DAHDI_TONE_MFR2_FWD_4,
	DAHDI_TONE_MFR2_FWD_5,
	DAHDI_TONE_MFR2_FWD_6,
	DAHDI_TONE_MFR2_FWD_7,
	DAHDI_TONE_MFR2_FWD_8,
	DAHDI_TONE_MFR2_FWD_9,
	DAHDI_TONE_MFR2_FWD_10,
	DAHDI_TONE_MFR2_FWD_11,
	DAHDI_TONE_MFR2_FWD_12,
	DAHDI_TONE_MFR2_FWD_13,
	DAHDI_TONE_MFR2_FWD_14,
	DAHDI_TONE_MFR2_FWD_15,
};

#define DAHDI_TONE_MFR2_FWD_MAX DAHDI_TONE_MFR2_FWD_15

enum {
	DAHDI_TONE_MFR2_REV_1 = DAHDI_TONE_MFR2_REV_BASE,
	DAHDI_TONE_MFR2_REV_2,
	DAHDI_TONE_MFR2_REV_3,
	DAHDI_TONE_MFR2_REV_4,
	DAHDI_TONE_MFR2_REV_5,
	DAHDI_TONE_MFR2_REV_6,
	DAHDI_TONE_MFR2_REV_7,
	DAHDI_TONE_MFR2_REV_8,
	DAHDI_TONE_MFR2_REV_9,
	DAHDI_TONE_MFR2_REV_10,
	DAHDI_TONE_MFR2_REV_11,
	DAHDI_TONE_MFR2_REV_12,
	DAHDI_TONE_MFR2_REV_13,
	DAHDI_TONE_MFR2_REV_14,
	DAHDI_TONE_MFR2_REV_15,
};

#define DAHDI_TONE_MFR2_REV_MAX DAHDI_TONE_MFR2_REV_15

#define DAHDI_LAW_DEFAULT	0	/* Default law for span */
#define DAHDI_LAW_MULAW		1	/* Mu-law */
#define DAHDI_LAW_ALAW		2	/* A-law */

#define DAHDI_DIAL_OP_APPEND	1
#define DAHDI_DIAL_OP_REPLACE	2
#define DAHDI_DIAL_OP_CANCEL	3

#define DAHDI_MAX_CADENCE		16

#define DAHDI_TONEDETECT_ON	(1 << 0)		/* Detect tones */
#define DAHDI_TONEDETECT_MUTE	(1 << 1)		/* Mute audio in received channel */

/* Define the max # of outgoing DTMF, MFR1 or MFR2 digits to queue */
#define DAHDI_MAX_DTMF_BUF 256

#define DAHDI_MAX_EVENTSIZE	64	/* 64 events max in buffer */

/* Value for DAHDI_HOOK, set to ON hook */
#define DAHDI_ONHOOK	0

/* Value for DAHDI_HOOK, set to OFF hook */
#define DAHDI_OFFHOOK	1

/* Value for DAHDI_HOOK, wink (off hook momentarily) */
#define DAHDI_WINK		2

/* Value for DAHDI_HOOK, flash (on hook momentarily) */
#define DAHDI_FLASH	3

/* Value for DAHDI_HOOK, start line */
#define DAHDI_START	4

/* Value for DAHDI_HOOK, ring line (same as start line) */
#define DAHDI_RING		5

/* Value for DAHDI_HOOK, turn ringer off */
#define DAHDI_RINGOFF  6

/* Flush and stop the read (input) process */
#define DAHDI_FLUSH_READ		1

/* Flush and stop the write (output) process */
#define DAHDI_FLUSH_WRITE		2

/* Flush and stop both (input and output) processes */
#define DAHDI_FLUSH_BOTH		(DAHDI_FLUSH_READ | DAHDI_FLUSH_WRITE)

/* Flush the event queue */
#define DAHDI_FLUSH_EVENT		4

/* Flush everything */
#define DAHDI_FLUSH_ALL			(DAHDI_FLUSH_BOTH | DAHDI_FLUSH_EVENT)

#define DAHDI_MAX_SPANS			128	/* Max, 128 spans */
#define DAHDI_MAX_CHANNELS		1024	/* Max, 1024 channels */
#define DAHDI_MAX_CONF			1024	/* Max, 1024 conferences */

/* Conference modes */
#define DAHDI_CONF_MODE_MASK		0xFF		/* mask for modes */
#define DAHDI_CONF_NORMAL		0		/* normal mode */
#define DAHDI_CONF_MONITOR		1		/* monitor mode (rx of other chan) */
#define DAHDI_CONF_MONITORTX		2		/* monitor mode (tx of other chan) */
#define DAHDI_CONF_MONITORBOTH		3		/* monitor mode (rx & tx of other chan) */
#define DAHDI_CONF_CONF			4		/* conference mode */
#define DAHDI_CONF_CONFANN		5		/* conference announce mode */
#define DAHDI_CONF_CONFMON		6		/* conference monitor mode */
#define DAHDI_CONF_CONFANNMON		7		/* conference announce/monitor mode */
#define DAHDI_CONF_REALANDPSEUDO	8		/* real and pseudo port both on conf */
#define DAHDI_CONF_DIGITALMON		9		/* Do not decode or interpret */
#define DAHDI_CONF_MONITOR_RX_PREECHO	10		/* monitor mode (rx of other chan) - before echo can is done */
#define DAHDI_CONF_MONITOR_TX_PREECHO	11		/* monitor mode (tx of other chan) - before echo can is done */
#define DAHDI_CONF_MONITORBOTH_PREECHO	12		/* monitor mode (rx & tx of other chan) - before echo can is done */
#define DAHDI_CONF_FLAG_MASK		0xFF00		/* mask for flags */
#define DAHDI_CONF_LISTENER		0x100		/* is a listener on the conference */
#define DAHDI_CONF_TALKER		0x200		/* is a talker on the conference */
#define DAHDI_CONF_PSEUDO_LISTENER	0x400		/* pseudo is a listener on the conference */
#define DAHDI_CONF_PSEUDO_TALKER	0x800		/* pseudo is a talker on the conference */

/* Alarm Condition bits */
#define DAHDI_ALARM_NONE		0	 /* No alarms */
#define DAHDI_ALARM_RECOVER		(1 << 0) /* Recovering from alarm */
#define DAHDI_ALARM_LOOPBACK		(1 << 1) /* In loopback */
#define DAHDI_ALARM_YELLOW		(1 << 2) /* Yellow Alarm */
#define DAHDI_ALARM_RED			(1 << 3) /* Red Alarm */
#define DAHDI_ALARM_BLUE		(1 << 4) /* Blue Alarm */
#define DAHDI_ALARM_NOTOPEN		(1 << 5)
/* Verbose alarm states (upper byte) */
#define DAHDI_ALARM_LOS			(1 << 8) /* Loss of Signal */
#define DAHDI_ALARM_LFA			(1 << 9) /* Loss of Frame Alignment */
#define DAHDI_ALARM_LMFA		(1 << 10)/* Loss of Multi-Frame Align */

/* Maintenance modes */
#define DAHDI_MAINT_NONE		0	/* Normal Mode */
#define DAHDI_MAINT_LOCALLOOP		1	/* Local Loopback */
#define DAHDI_MAINT_REMOTELOOP		2	/* Remote Loopback */
#define DAHDI_MAINT_NETWORKLINELOOP	2	/* Remote Loopback */
#define DAHDI_MAINT_LOOPUP		3	/* send loopup code */
#define DAHDI_MAINT_LOOPDOWN		4	/* send loopdown code */
#define DAHDI_MAINT_FAS_DEFECT		6	/* insert a FAS defect */
#define DAHDI_MAINT_MULTI_DEFECT	7	/* insert a Multiframe defect */
#define DAHDI_MAINT_CRC_DEFECT		8	/* insert a FAS defect */
#define DAHDI_MAINT_CAS_DEFECT		9	/* insert a FAS defect */
#define DAHDI_MAINT_PRBS_DEFECT		10	/* insert a FAS defect */
#define DAHDI_MAINT_BIPOLAR_DEFECT	11	/* insert a FAS defect */
#define DAHDI_MAINT_PRBS		12	/* enable the PRBS gen/mon */
#define DAHDI_MAINT_NETWORKPAYLOADLOOP	13	/* Remote Loopback */
#define DAHDI_RESET_COUNTERS		14	/* Clear the error counters */
#define DAHDI_MAINT_ALARM_SIM		15	/* Simulate alarms */

/* Flag Value for IOMUX, read avail */
#define DAHDI_IOMUX_READ	1

/* Flag Value for IOMUX, write avail */
#define DAHDI_IOMUX_WRITE	2

/* Flag Value for IOMUX, write done */
#define DAHDI_IOMUX_WRITEEMPTY	4

/* Flag Value for IOMUX, signalling event avail */
#define DAHDI_IOMUX_SIGEVENT	8

/* Flag Value for IOMUX, Do Not Wait if nothing to report */
#define DAHDI_IOMUX_NOWAIT	0x100

/* Ret. Value for GET/WAIT Event, no event */
#define DAHDI_EVENT_NONE		0

/* Ret. Value for GET/WAIT Event, Went Onhook */
#define DAHDI_EVENT_ONHOOK		1

/* Ret. Value for GET/WAIT Event, Went Offhook or got Ring */
#define DAHDI_EVENT_RINGOFFHOOK		2

/* Ret. Value for GET/WAIT Event, Got Wink or Flash */
#define DAHDI_EVENT_WINKFLASH		3

/* Ret. Value for GET/WAIT Event, Got Alarm */
#define DAHDI_EVENT_ALARM		4

/* Ret. Value for GET/WAIT Event, Got No Alarm (after alarm) */
#define DAHDI_EVENT_NOALARM		5

/* Ret. Value for GET/WAIT Event, HDLC Abort frame */
#define DAHDI_EVENT_ABORT		6

/* Ret. Value for GET/WAIT Event, HDLC Frame overrun */
#define DAHDI_EVENT_OVERRUN		7

/* Ret. Value for GET/WAIT Event, Bad FCS */
#define DAHDI_EVENT_BADFCS		8

/* Ret. Value for dial complete */
#define DAHDI_EVENT_DIALCOMPLETE	9

/* Ret Value for ringer going on */
#define DAHDI_EVENT_RINGERON		10

/* Ret Value for ringer going off */
#define DAHDI_EVENT_RINGEROFF		11

/* Ret Value for hook change complete */
#define DAHDI_EVENT_HOOKCOMPLETE	12

/* Ret Value for bits changing on a CAS / User channel */
#define DAHDI_EVENT_BITSCHANGED		13

/* Ret value for the beginning of a pulse coming on its way */
#define DAHDI_EVENT_PULSE_START		14

/* Timer event -- timer expired */
#define DAHDI_EVENT_TIMER_EXPIRED	15

/* Timer event -- ping ready */
#define DAHDI_EVENT_TIMER_PING		16

/* Polarity reversal event */
#define DAHDI_EVENT_POLARITY		17

/* Ring Begin event */
#define DAHDI_EVENT_RINGBEGIN		18

/* Echo can disabled event */
#define DAHDI_EVENT_EC_DISABLED		19

/* Channel was disconnected. Hint user to close channel */
#define DAHDI_EVENT_REMOVED		20

/* A neon MWI pulse was detected */
#define DAHDI_EVENT_NEONMWI_ACTIVE	21

/* No neon MWI pulses were detected over some period of time */
#define DAHDI_EVENT_NEONMWI_INACTIVE	22

/* A CED tone was detected on the channel in the transmit direction */
#define DAHDI_EVENT_TX_CED_DETECTED	23

/* A CED tone was detected on the channel in the receive direction */
#define DAHDI_EVENT_RX_CED_DETECTED	24

/* A CNG tone was detected on the channel in the transmit direction */
#define DAHDI_EVENT_TX_CNG_DETECTED	25

/* A CNG tone was detected on the channel in the receive direction */
#define DAHDI_EVENT_RX_CNG_DETECTED	26

/* The echo canceler's NLP (only) was disabled */
#define DAHDI_EVENT_EC_NLP_DISABLED	27

/* The echo canceler's NLP (only) was enabled */
#define DAHDI_EVENT_EC_NLP_ENABLED	28

/* The channel's read buffer encountered an overrun condition */
#define DAHDI_EVENT_READ_OVERRUN	29

/* The channel's write buffer encountered an underrun condition */
#define DAHDI_EVENT_WRITE_UNDERRUN	30

#define DAHDI_EVENT_PULSEDIGIT		(1 << 16)	/* This is OR'd with the digit received */
#define DAHDI_EVENT_DTMFDOWN		(1 << 17)	/* Ditto for DTMF key down event */
#define DAHDI_EVENT_DTMFUP		(1 << 18)	/* Ditto for DTMF key up event */

/* Transcoder related definitions */

struct dahdi_transcoder_formats {
	__u32	srcfmt;
	__u32	dstfmt;
};
struct dahdi_transcoder_info {
	__u32 tcnum;
	char name[80];
	__u32 numchannels;
	__u32 dstfmts;
	__u32 srcfmts;
};

#define DAHDI_MAX_ECHOCANPARAMS 8

/* ioctl definitions */
#define DAHDI_CODE		0xDA

/*
 * Get/Set Transfer Block Size.
 */
#define DAHDI_GET_BLOCKSIZE		_IOR(DAHDI_CODE, 1, int)
#define DAHDI_SET_BLOCKSIZE		_IOW(DAHDI_CODE, 1, int)

/*
 * Flush Buffer(s) and stop I/O
 */
#define DAHDI_FLUSH			_IOW(DAHDI_CODE, 3, int)

/*
 * Wait for Write to Finish
 */
#define DAHDI_SYNC			_IO(DAHDI_CODE, 4)

/*
 * Get/set channel parameters
 */

struct dahdi_params {
	int channo;		/* Channel number */
	int spanno;		/* Span itself */
	int chanpos;		/* Channel number in span */
	int sigtype;		/* read-only */
	int sigcap;		/* read-only */
	int rxisoffhook;	/* read-only */
	int rxbits;		/* read-only */
	int txbits;		/* read-only */
	int txhooksig;		/* read-only */
	int rxhooksig;		/* read-only */
	int curlaw;		/* read-only  -- one of DAHDI_LAW_MULAW or DAHDI_LAW_ALAW */
	int idlebits;		/* read-only  -- What is considered the idle state */
	char name[40];		/* Name of channel */
	int prewinktime;
	int preflashtime;
	int winktime;
	int flashtime;
	int starttime;
	int rxwinktime;
	int rxflashtime;
	int debouncetime;
	int pulsebreaktime;
	int pulsemaketime;
	int pulseaftertime;
	__u32 chan_alarms;	/* alarms on this channel */
};

#define DAHDI_GET_PARAMS_V1		_IOR(DAHDI_CODE,  5, struct dahdi_params)
#define DAHDI_GET_PARAMS		_IOWR(DAHDI_CODE, 5, struct dahdi_params)
#define DAHDI_SET_PARAMS		_IOW(DAHDI_CODE,  5, struct dahdi_params)

/*
 * Set Hookswitch Status
 */
#define DAHDI_HOOK			_IOW(DAHDI_CODE, 7, int)

/*
 * Get Signalling Event
 */
#define DAHDI_GETEVENT			_IOR(DAHDI_CODE, 8, int)

/*
 * Wait for something to happen (IO Mux)
 */
#define DAHDI_IOMUX			_IOWR(DAHDI_CODE, 9, int)

/*
 * Get Span Status
 */
struct dahdi_spaninfo {
	int	spanno;		/* span number */
	char	name[20];	/* Name */
	char	desc[40];	/* Description */
	int	alarms;		/* alarms status */
	int	txlevel;	/* what TX level is set to */
	int	rxlevel;	/* current RX level */

	int	bpvcount;	/* current BPV count */
	int	crc4count;	/* current CRC4 error count */
	int	ebitcount;	/* current E-bit error count */
	int	fascount;	/* current FAS error count */
	__u32	fecount;	/* Framing error counter */
	__u32	cvcount;	/* Coding violations counter */
	__u32	becount;	/* current bit error count */
	__u32	prbs;		/* current PRBS detected pattern */
	__u32	errsec;		/* errored seconds */

	int	irqmisses;	/* current IRQ misses */
	int	syncsrc;	/* span # of current sync source,
				   or 0 for free run */
	int	numchans;	/* number of configured channels on this span */
	int	totalchans;	/* total number of channels on the span */
	int	totalspans;	/* total number of spans in entire system */
	int	lbo;		/* line build out */
	int	lineconfig;	/* framing/coding */
	char 	lboname[40];	/* line build out in text form */
	char	location[40];	/* span's device location in system */
	char	manufacturer[40]; /* manufacturer of span's device */
	char	devicetype[40];	/* span's device type */
	int	irq;		/* span's device IRQ */
	int	linecompat;	/* span global signaling or 0 for
				   analog spans.*/
	char	spantype[6];	/* type of span in text form */
} __attribute__((packed));

struct dahdi_spaninfo_v1 {
	int	spanno;		/* span number */
	char	name[20];	/* Name */
	char	desc[40];	/* Description */
	int	alarms;		/* alarms status */
	int	txlevel;	/* what TX level is set to */
	int	rxlevel;	/* current RX level */
	int	bpvcount;	/* current BPV count */
	int	crc4count;	/* current CRC4 error count */
	int	ebitcount;	/* current E-bit error count */
	int	fascount;	/* current FAS error count */
	int	irqmisses;	/* current IRQ misses */
	int	syncsrc;	/* span # of current sync source, or 0 for free run  */
	int	numchans;	/* number of configured channels on this span */
	int	totalchans;	/* total number of channels on the span */
	int	totalspans;	/* total number of spans in entire system */
	int	lbo;		/* line build out */
	int	lineconfig;	/* framing/coding */
	char 	lboname[40];	/* line build out in text form */
	char	location[40];	/* span's device location in system */
	char	manufacturer[40]; /* manufacturer of span's device */
	char	devicetype[40];	/* span's device type */
	int	irq;		/* span's device IRQ */
	int	linecompat;	/* signaling modes possible on this span */
	char	spantype[6];	/* type of span in text form */
};
#define DAHDI_SPANSTAT	   _IOWR(DAHDI_CODE, 10, struct dahdi_spaninfo)
#define DAHDI_SPANSTAT_V1  _IOWR(DAHDI_CODE, 10, struct dahdi_spaninfo_v1)

/*
 * Set Maintenance Mode
 */
struct dahdi_maintinfo {
	int	spanno;		/* span number */
	int	command;	/* command */
};

#define DAHDI_MAINT			_IOW(DAHDI_CODE, 11, struct dahdi_maintinfo)

/*
 * Get/Set Conference Mode
 */
struct dahdi_confinfo {
	int	chan;		/* channel number, 0 for current */
	int	confno;		/* conference number */
	int	confmode;	/* conferencing mode */
};

#define DAHDI_GETCONF_V1		_IOR(DAHDI_CODE,   12, struct dahdi_confinfo)
#define DAHDI_GETCONF			_IOWR(DAHDI_CODE,  12, struct dahdi_confinfo)

#define DAHDI_SETCONF_V1		_IOW(DAHDI_CODE,  12, struct dahdi_confinfo)
#define DAHDI_SETCONF			_IOWR(DAHDI_CODE, 13, struct dahdi_confinfo)

/*
 * Display Conference Diagnostic Information on Console
 */
#define DAHDI_CONFDIAG_V1		_IOR(DAHDI_CODE, 15, int)
#define DAHDI_CONFDIAG			_IOW(DAHDI_CODE, 15, int)

/*
 * Get/Set Channel audio gains
 */
struct dahdi_gains {
	int	chan;			/* channel number, 0 for current */
	unsigned char rxgain[256];	/* Receive gain table */
	unsigned char txgain[256];	/* Transmit gain table */
};

#define DAHDI_GETGAINS_V1		_IOR(DAHDI_CODE,  16, struct dahdi_gains)
#define DAHDI_GETGAINS			_IOWR(DAHDI_CODE, 16, struct dahdi_gains)
#define DAHDI_SETGAINS			_IOW(DAHDI_CODE,  16, struct dahdi_gains)

/*
 * Set Line (T1) Configurations
 */
struct dahdi_lineconfig {
	int span;		/* Which span number (0 to use name) */
	char name[20];		/* Name of span to use */
	int	lbo;		/* line build-outs */
	int	lineconfig;	/* line config parameters (framing, coding) */
	int	sync;		/* what level of sync source we are */
};

#define DAHDI_SPANCONFIG		_IOW(DAHDI_CODE, 18, struct dahdi_lineconfig)

/*
 * Set Channel Configuration
 */
struct dahdi_chanconfig {
	int	chan;		/* Channel we're applying this to (0 to use name) */
	char	name[40];	/* Name of channel to use */
	int	sigtype;	/* Signal type */
	int	deflaw;		/* Default law (DAHDI_LAW_DEFAULT, DAHDI_LAW_MULAW, or DAHDI_LAW_ALAW) */
	int	master;		/* Master channel if sigtype is DAHDI_SLAVE */
	int	idlebits;	/* Idle bits (if this is a CAS channel) or
				   channel to monitor (if this is DACS channel) */
	char	netdev_name[16];/* name for the hdlc network device*/
};

#define DAHDI_CHANCONFIG		_IOW(DAHDI_CODE, 19, struct dahdi_chanconfig)

/*
 * Set Conference to mute mode
 */
#define DAHDI_CONFMUTE			_IOW(DAHDI_CODE, 20, int)

/*
 * Send a particular tone (see DAHDI_TONE_*)
 */
#define DAHDI_SENDTONE			_IOW(DAHDI_CODE, 21, int)

/*
 * Get/Set your region for tones
 */
#define DAHDI_GETTONEZONE		_IOR(DAHDI_CODE, 22, int)
#define DAHDI_SETTONEZONE		_IOW(DAHDI_CODE, 22, int)

/*
 * Master unit only -- set default zone (see DAHDI_TONE_ZONE_*)
 */
#define DAHDI_DEFAULTZONE		_IOW(DAHDI_CODE, 24, int)

/*
 * Load a tone zone from a dahdi_tone_def_header
 */
struct dahdi_tone_def {
	int tone;		/* See DAHDI_TONE_* */
	int next;		/* What the next position in the cadence is
				   (They're numbered by the order the appear here) */
	int samples;		/* How many samples to play for this cadence */
	int shift;		/* How much to scale down the volume (2 is nice) */

	/* Now come the constants we need to make tones */

	/* 
		Calculate the next 6 factors using the following equations:
		l = <level in dbm>, f1 = <freq1>, f2 = <freq2>
		gain = pow(10.0, (l - 3.14) / 20.0) * 65536.0 / 2.0;

		// Frequency factor 1 
		fac_1 = 2.0 * cos(2.0 * M_PI * (f1/8000.0)) * 32768.0;
		// Last previous two samples 
		init_v2_1 = sin(-4.0 * M_PI * (f1/8000.0)) * gain;
		init_v3_1 = sin(-2.0 * M_PI * (f1/8000.0)) * gain;

		// Frequency factor 2 
		fac_2 = 2.0 * cos(2.0 * M_PI * (f2/8000.0)) * 32768.0;
		// Last previous two samples 
		init_v2_2 = sin(-4.0 * M_PI * (f2/8000.0)) * gain;
		init_v3_2 = sin(-2.0 * M_PI * (f2/8000.0)) * gain;
	*/
	int fac1;		
	int init_v2_1;		
	int init_v3_1;		
	int fac2;		
	int init_v2_2;		
	int init_v3_2;
	int modulate;
};

struct dahdi_tone_def_header {
	int count;				/* How many samples follow */
	int zone;				/* Which zone we are loading */
	int ringcadence[DAHDI_MAX_CADENCE];	/* Ring cadence in ms (0=on, 1=off, ends with 0 value) */
	char name[40];				/* Informational name of zone */
	/* immediately follow this structure with dahdi_tone_def structures */
	struct dahdi_tone_def tones[0];
};

#define DAHDI_LOADZONE			_IOW(DAHDI_CODE, 25, struct dahdi_tone_def_header)

/*
 * Free a tone zone 
 */
#define DAHDI_FREEZONE			_IOW(DAHDI_CODE, 26, int)

/*
 * Get/Set buffer policy 
 */
struct dahdi_bufferinfo {
	int txbufpolicy;	/* Policy for handling receive buffers */
	int rxbufpolicy;	/* Policy for handling receive buffers */
	int numbufs;		/* How many buffers to use */
	int bufsize;		/* How big each buffer is */
	int readbufs;		/* How many read buffers are full (read-only) */
	int writebufs;		/* How many write buffers are full (read-only) */
};

#define DAHDI_GET_BUFINFO		_IOR(DAHDI_CODE, 27, struct dahdi_bufferinfo)
#define DAHDI_SET_BUFINFO		_IOW(DAHDI_CODE, 27, struct dahdi_bufferinfo)

/*
 * Get/Set dialing parameters
 */
struct dahdi_dialparams {
	int mfv1_tonelen;	/* MF R1 tone length for digits */
	int dtmf_tonelen;	/* DTMF tone length */
	int mfr2_tonelen;	/* MF R2 tone length */
	int reserved[3];	/* Reserved for future expansion -- always set to 0 */
};

#define DAHDI_GET_DIALPARAMS		_IOR(DAHDI_CODE, 29, struct dahdi_dialparams)
#define DAHDI_SET_DIALPARAMS		_IOW(DAHDI_CODE, 29, struct dahdi_dialparams)

/*
 * Append, replace, or cancel a dial string
 */
struct dahdi_dialoperation {
	int op;
	char dialstr[DAHDI_MAX_DTMF_BUF];
};

#define DAHDI_DIAL			_IOW(DAHDI_CODE, 31, struct dahdi_dialoperation)

/*
 * Set a clear channel into audio mode
 */
#define DAHDI_AUDIOMODE			_IOW(DAHDI_CODE, 32, int)

/*
 * Enable or disable echo cancellation on a channel 
 *
 * For ECHOCANCEL:
 * The number is zero to disable echo cancellation and non-zero
 * to enable echo cancellation.  If the number is between 32
 * and 1024, it will also set the number of taps in the echo canceller
 *
 * For ECHOCANCEL_PARAMS:
 * The structure contains parameters that should be passed to the
 * echo canceler instance for the selected channel.
 */
#define DAHDI_ECHOCANCEL		_IOW(DAHDI_CODE, 33, int)

struct dahdi_echocanparam {
	char name[16];
	__s32 value;
};

struct dahdi_echocanparams {
	/* 8 taps per millisecond */
	__u32 tap_length;
	/* number of parameters supplied */
	__u32 param_count;
	/* immediately follow this structure with dahdi_echocanparam structures */
	struct dahdi_echocanparam params[0];
};

#define DAHDI_ECHOCANCEL_PARAMS		_IOW(DAHDI_CODE, 33, struct dahdi_echocanparams)

/*
 * Return a channel's channel number
 */
#define DAHDI_CHANNO			_IOR(DAHDI_CODE, 34, int)

/*
 * Return a flag indicating whether channel is currently dialing
 */
#define DAHDI_DIALING			_IOR(DAHDI_CODE, 35, int)

/*
 * Set a clear channel into HDLC w/out FCS checking/calculation mode
 */
#define DAHDI_HDLCRAWMODE		_IOW(DAHDI_CODE, 36, int)

/*
 * Set a clear channel into HDLC w/ FCS mode
 */
#define DAHDI_HDLCFCSMODE		_IOW(DAHDI_CODE, 37, int)

/* 
 * Specify a channel on generic channel selector - must be done before
 * performing any other ioctls
 */
#define DAHDI_SPECIFY			_IOW(DAHDI_CODE, 38, int)

/*
 * Temporarily set the law on a channel to 
 * DAHDI_LAW_DEFAULT, DAHDI_LAW_ALAW, or DAHDI_LAW_MULAW.  Is reset on close.  
 */
#define DAHDI_SETLAW			_IOW(DAHDI_CODE, 39, int)

/*
 * Temporarily set the channel to operate in linear mode when non-zero
 * or default law if 0
 */
#define DAHDI_SETLINEAR			_IOW(DAHDI_CODE, 40, int)

/*
 * Set a clear channel into HDLC w/ PPP interface mode
 */
#define DAHDI_HDLCPPP			_IOW(DAHDI_CODE, 41, int)

/*
 * Set the ring cadence for FXS interfaces
 */
struct dahdi_ring_cadence {
	int ringcadence[DAHDI_MAX_CADENCE];
};

#define DAHDI_SETCADENCE		_IOW(DAHDI_CODE, 42, struct dahdi_ring_cadence)

/*
 * Get/Set the signaling bits for CAS interface
 */
#define DAHDI_GETRXBITS 		_IOR(DAHDI_CODE, 43, int)
#define DAHDI_SETTXBITS			_IOW(DAHDI_CODE, 43, int)

/*
 * Display Channel Diagnostic Information on Console
 */
#define DAHDI_CHANDIAG_V1		_IOR(DAHDI_CODE, 44, int)
#define DAHDI_CHANDIAG			_IOW(DAHDI_CODE, 44, int)

/*
 * Set Channel's SF Tone Configuration
 */
struct dahdi_sfconfig {
	int	chan;		/* Channel we're applying this to (0 to use name) */
	char	name[40];	/* Name of channel to use */
	long	rxp1;		/* receive tone det. p1 */
	long	rxp2;		/* receive tone det. p2 */
	long	rxp3;		/* receive tone det. p3 */
	int	txtone;		/* Tx tone factor */
	int	tx_v2;		/* initial v2 value */
	int	tx_v3;		/* initial v3 value */
	int	toneflag;	/* Tone flags */
};

#define DAHDI_SFCONFIG			_IOW(DAHDI_CODE, 46, struct dahdi_sfconfig)

/*
 * Set timer expiration (in samples)
 */
#define DAHDI_TIMERCONFIG		_IOW(DAHDI_CODE, 47, int)

/*
 * Acknowledge timer expiration (number to acknowledge, or -1 for all)
 */
#define DAHDI_TIMERACK 			_IOW(DAHDI_CODE, 48, int)

/*
 * Get Conference to mute mode
 */
#define DAHDI_GETCONFMUTE		_IOR(DAHDI_CODE, 49, int)

/*
 * Request echo training in some number of ms (with muting in the mean time)
 */
#define DAHDI_ECHOTRAIN			_IOW(DAHDI_CODE, 50, int)

/*
 * Set on hook transfer for n number of ms -- implemented by low level driver
 */
#define DAHDI_ONHOOKTRANSFER		_IOW(DAHDI_CODE, 51, int)

/*
 * Queue Ping
 */
#define DAHDI_TIMERPING 		_IO(DAHDI_CODE, 52)

/*
 * Acknowledge ping
 */
#define DAHDI_TIMERPONG 		_IO(DAHDI_CODE, 53)

/*
 * Get/set signalling freeze
 */
#define DAHDI_GETSIGFREEZE 		_IOR(DAHDI_CODE, 54, int)
#define DAHDI_SETSIGFREEZE 		_IOW(DAHDI_CODE, 54, int)

/*
 * Perform an indirect ioctl (on a specified channel via master interface)
 */
struct dahdi_indirect_data {
	int	chan;
	int	op;
	void	*data;
};

#define DAHDI_INDIRECT 			_IOWR(DAHDI_CODE, 56, struct dahdi_indirect_data)


/*
 * Get the version of DAHDI that is running, and a description
 * of the compiled-in echo cancellers (if any)
 */
struct dahdi_versioninfo {
	char version[80];
	char echo_canceller[80];
};

#define DAHDI_GETVERSION		_IOR(DAHDI_CODE, 57, struct dahdi_versioninfo)

/*
 * Put the channel in loopback mode (receive from the channel is
 * transmitted back on the interface)
 */
#define DAHDI_LOOPBACK 			_IOW(DAHDI_CODE, 58, int)

/*
  Attach the desired echo canceler module (or none) to a channel in an
  audio-supporting mode, so that when the channel needs an echo canceler
  that module will be used to supply one.
 */
struct dahdi_attach_echocan {
	int	chan;		/* Channel we're applying this to */
	char	echocan[16];	/* Name of echo canceler to attach to this channel
				   (leave empty to have no echocan attached */
};

#define DAHDI_ATTACH_ECHOCAN 		_IOW(DAHDI_CODE, 59, struct dahdi_attach_echocan)


/*
 *  60-80 are reserved for private drivers
 *  80-85 are reserved for dynamic span stuff
 */

/*
 * Create a dynamic span
 */
struct dahdi_dynamic_span {
	char driver[20];	/* Which low-level driver to use */
	char addr[40];		/* Destination address */
	int numchans;		/* Number of channels */
	int timing;		/* Timing source preference */
	int spanno;		/* Span number (filled in by DAHDI) */
};

#define DAHDI_DYNAMIC_CREATE		_IOWR(DAHDI_CODE, 80, struct dahdi_dynamic_span)

/* 
 * Destroy a dynamic span 
 */
#define DAHDI_DYNAMIC_DESTROY		_IOW(DAHDI_CODE, 81, struct dahdi_dynamic_span)

/*
 * Set the HW gain for a device
 */
struct dahdi_hwgain {
	__s32 newgain;	/* desired gain in dB but x10.  -3.5dB would be -35 */
	__u32 tx:1;	/* 0=rx; 1=tx */
};
#define DAHDI_SET_HWGAIN		_IOW(DAHDI_CODE, 86, struct dahdi_hwgain)

/*
 * Enable tone detection -- implemented by low level driver
 */
#define DAHDI_TONEDETECT		_IOW(DAHDI_CODE, 91, int)

/*
 * Set polarity -- implemented by individual driver.  0 = forward, 1 = reverse
 */
#define DAHDI_SETPOLARITY		_IOW(DAHDI_CODE, 92, int)

/*
 * Transcoder operations
 */

/* DAHDI_TRANSCODE_OP is an older interface that is deprecated and no longer
 * supported.
 */
#define DAHDI_TRANSCODE_OP		_IOWR(DAHDI_CODE, 93, int)

#define DAHDI_TC_CODE			'T'
#define DAHDI_TC_ALLOCATE		_IOW(DAHDI_TC_CODE, 1, struct dahdi_transcoder_formats)
#define DAHDI_TC_GETINFO		_IOWR(DAHDI_TC_CODE, 2, struct dahdi_transcoder_info)

/*
 * VMWI Specification 
 */
struct dahdi_vmwi_info {
	unsigned int vmwi_type;
};

#define DAHDI_VMWI_LREV	(1 << 0)	/* Line Reversal */
#define DAHDI_VMWI_HVDC	(1 << 1)	/* HV 90VDC */
#define DAHDI_VMWI_HVAC	(1 << 2)	/* HV 90VAC Neon lamp */

/*
 * VoiceMail Waiting Indication (VMWI) -- implemented by low-level driver.
 * Value: number of waiting messages (hence 0: switch messages off).
 */
#define DAHDI_VMWI			_IOWR(DAHDI_CODE, 94, int)
#define DAHDI_VMWI_CONFIG		_IOW(DAHDI_CODE, 95, struct dahdi_vmwi_info)

/*
 * Startup or Shutdown a span
 */
#define DAHDI_STARTUP			_IOW(DAHDI_CODE, 99, int)
#define DAHDI_SHUTDOWN			_IOW(DAHDI_CODE, 100, int)

#define DAHDI_HDLC_RATE			_IOW(DAHDI_CODE, 101, int)

/* Put a channel's echo canceller into 'FAX mode' if possible */

#define DAHDI_ECHOCANCEL_FAX_MODE	_IOW(DAHDI_CODE, 102, int)

/*
 * Defines which channel to receive mirrored traffic from
 */
#ifdef CONFIG_DAHDI_MIRROR
#define DAHDI_RXMIRROR			_IOW(DAHDI_CODE, 103, int)
#define DAHDI_TXMIRROR			_IOW(DAHDI_CODE, 104, int)
#endif /* CONFIG_DAHDI_MIRROR */

/*
  Set the desired state for channel buffer event generation which is disabled
  by default to allow for backwards compatibility for dumb users of channels
  such as pattern utilities.
 */
#define DAHDI_BUFFER_EVENTS		_IOW(DAHDI_CODE, 105, int)

/* Get current status IOCTL */
/* Defines for Radio Status (dahdi_radio_stat.radstat) bits */

#define DAHDI_RADSTAT_RX	1	/* currently "receiving " */
#define DAHDI_RADSTAT_TX	2	/* currently "transmitting" */
#define DAHDI_RADSTAT_RXCT	4	/* currently receiving continuous tone with 
				   current settings */
#define DAHDI_RADSTAT_RXCOR	8	/* currently receiving COR (irrelevant of COR
				   ignore) */
#define DAHDI_RADSTAT_IGNCOR	16	/* currently ignoring COR */
#define DAHDI_RADSTAT_IGNCT	32	/* currently ignoring CTCSS/DCS decode */
#define DAHDI_RADSTAT_NOENCODE 64	/* currently blocking CTCSS/DCS encode */

struct dahdi_radio_stat {
	unsigned short ctcode_rx;	/* code of currently received CTCSS 
					   or DCS, 0 for none */
	unsigned short ctclass;		/* class of currently received CTCSS or
					    DCS code */
	unsigned short ctcode_tx;	/* code of currently encoded CTCSS or
					   DCS, 0 for none */
	unsigned char radstat;		/* status bits of radio */
};

#define DAHDI_RADIO_GETSTAT		_IOR(DAHDI_CODE, 57, struct dahdi_radio_stat)

/* Get/Set a radio channel parameter */
/* Defines for Radio Parameters (dahdi_radio_param.radpar) */
#define DAHDI_RADPAR_INVERTCOR 1	/* invert the COR signal (0/1) */
#define DAHDI_RADPAR_IGNORECOR 2	/* ignore the COR signal (0/1) */
#define DAHDI_RADPAR_IGNORECT 3	/* ignore the CTCSS/DCS decode (0/1) */
#define DAHDI_RADPAR_NOENCODE 4	/* block the CTCSS/DCS encode (0/1) */
#define DAHDI_RADPAR_CORTHRESH 5	/* COR trigger threshold (0-7) */

#define DAHDI_RADPAR_EXTRXTONE 6	/* 0 means use internal decoder, 1 means UIOA
				   logic true is CT decode, 2 means UIOA logic
				   false is CT decode */
#define DAHDI_RADPAR_NUMTONES	7	/* returns maximum tone index (curently 15) */
#define DAHDI_RADPAR_INITTONE	8	/* init all tone indexes to 0 (no tones) */
#define DAHDI_RADPAR_RXTONE	9	/* CTCSS tone, (1-32) or DCS tone (1-777),
				   or 0 meaning no tone, set index also (1-15) */
#define DAHDI_RADPAR_RXTONECLASS 10	/* Tone class (0-65535), set index also (1-15) */
#define DAHDI_RADPAR_TXTONE 11	/* CTCSS tone (1-32) or DCS tone (1-777) or 0
				   to indicate no tone, to transmit 
				   for this tone index (0-32, 0 disables
				   transmit CTCSS), set index also (0-15) */
#define DAHDI_RADPAR_DEBOUNCETIME 12	/* receive indication debounce time, 
				   milliseconds (1-999) */
#define DAHDI_RADPAR_BURSTTIME 13	/* end of transmit with no CT tone in
				   milliseconds (0-999) */


#define DAHDI_RADPAR_UIODATA 14	/* read/write UIOA and UIOB data. Bit 0 is
				   UIOA, bit 1 is UIOB */
#define DAHDI_RADPAR_UIOMODE 15	/* 0 means UIOA and UIOB are both outputs, 1
				   means UIOA is input, UIOB is output, 2 
				   means UIOB is input and UIOA is output,
				   3 means both UIOA and UIOB are inputs. Note
				   mode for UIOA is overridden when in
				   EXTRXTONE mode. */

#define DAHDI_RADPAR_REMMODE 16	/* Remote control data mode */
	#define DAHDI_RADPAR_REM_NONE 0 	/* no remote control data mode */
	#define DAHDI_RADPAR_REM_RBI1 1	/* Doug Hall RBI-1 data mode */
	#define DAHDI_RADPAR_REM_SERIAL 2	/* Serial Data, 9600 BPS */
	#define DAHDI_RADPAR_REM_SERIAL_ASCII 3	/* Serial Ascii Data, 9600 BPS */

#define DAHDI_RADPAR_REMCOMMAND 17	/* Remote conrtol write data block & do cmd */

#define DAHDI_RADPAR_DEEMP 18 /* Audio De-empahsis (on or off) */ 

#define DAHDI_RADPAR_PREEMP 19 /* Audio Pre-empahsis (on or off) */ 

#define DAHDI_RADPAR_RXGAIN 20 /* Audio (In to system) Rx Gain */ 

#define DAHDI_RADPAR_TXGAIN 21 /* Audio (Out from system) Tx Gain */ 

#define RAD_SERIAL_BUFLEN 128

struct dahdi_radio_param {
	unsigned short radpar;	/* param identifier */
	unsigned short index;	/* tone number */
	int data;		/* param */
	int data2;		/* param 2 */
	unsigned char buf[RAD_SERIAL_BUFLEN];
};
#define DAHDI_RADIO_GETPARAM		_IOR(DAHDI_CODE, 58, struct dahdi_radio_param)
#define DAHDI_RADIO_SETPARAM		_IOW(DAHDI_CODE, 58, struct dahdi_radio_param)


/*!
	\brief Size-limited null-terminating string copy.
	\param dst The destination buffer
	\param src The source string
	\param size The size of the destination buffer
	\return Nothing.

	This is similar to \a strncpy, with two important differences:
	- the destination buffer will \b always be null-terminated
	- the destination buffer is not filled with zeros past the copied string length
	These differences make it slightly more efficient, and safer to use since it will
	not leave the destination buffer unterminated. There is no need to pass an artificially
	reduced buffer size to this function (unlike \a strncpy), and the buffer does not need
	to be initialized to zeroes prior to calling this function.
*/
static inline void dahdi_copy_string(char *dst, const char *src, unsigned int size)
{
	while (*src && size) {
		*dst++ = *src++;
		size--;
	}
	if (__builtin_expect(!size, 0))
		dst--;
	*dst = '\0';
}

#endif /* _DAHDI_USER_H */
