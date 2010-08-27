/*
 * Wildcard TDM2400P TDM FXS/FXO Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 * Support for TDM800P and VPM150M by Matthew Fredrickson <creslin@digium.com>
 *
 * Copyright (C) 2005-2010 Digium, Inc.
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
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#ifndef _WCTDM24XXP_H
#define _WCTDM24XXP_H

#include <dahdi/kernel.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#include "voicebus/voicebus.h"

#define NUM_FXO_REGS 60

#define WC_MAX_IFACES 128

/*!
 * \brief Default ringer debounce (in ms)
 */
#define DEFAULT_RING_DEBOUNCE	1024

#define POLARITY_DEBOUNCE	64		/* Polarity debounce (in ms) */

#define OHT_TIMER		6000	/* How long after RING to retain OHT */

#define FLAG_3215	(1 << 0)
#define FLAG_EXPRESS	(1 << 1)

#define EFRAME_SIZE 108L
#define EFRAME_GAP 20L
#define SFRAME_SIZE ((EFRAME_SIZE * DAHDI_CHUNKSIZE) + (EFRAME_GAP * (DAHDI_CHUNKSIZE - 1)))

#define MAX_ALARMS 10

#define MOD_TYPE_NONE		0
#define MOD_TYPE_FXS		1
#define MOD_TYPE_FXO		2
#define MOD_TYPE_FXSINIT	3
#define MOD_TYPE_VPM		4
#define MOD_TYPE_QRV		5
#define MOD_TYPE_VPM150M	6
#define MOD_TYPE_BRI		7

#define MINPEGTIME	10 * 8		/* 30 ms peak to peak gets us no more than 100 Hz */
#define PEGTIME		50 * 8		/* 50ms peak to peak gets us rings of 10 Hz or more */
#define PEGCOUNT	5		/* 5 cycles of pegging means RING */

#define SDI_CLK		(0x00010000)
#define SDI_DOUT	(0x00020000)
#define SDI_DREAD	(0x00040000)
#define SDI_DIN		(0x00080000)

#define __CMD_RD   (1 << 20)		/* Read Operation */
#define __CMD_WR   (1 << 21)		/* Write Operation */
#define __CMD_FIN  (1 << 22)		/* Has finished receive */
#define __CMD_TX   (1 << 23)		/* Has been transmitted */

#define CMD_WR(a,b) (((a) << 8) | (b) | __CMD_WR)
#define CMD_RD(a) (((a) << 8) | __CMD_RD)

#if 0
#define CMD_BYTE(card,bit,altcs) (((((card) & 0x3) * 3 + (bit)) * 7) \
			+ ((card) >> 2) + (altcs) + ((altcs) ? -21 : 0))
#endif
#define NUM_MODULES		24
#define NUM_EC			4
#define NUM_SLOTS		6
#define MAX_TDM_CHAN		31
#define MAX_SPANS		9

#define NUM_CAL_REGS		12

#define USER_COMMANDS		8
#define ISR_COMMANDS		2
#define QRV_DEBOUNCETIME	20

#define MAX_COMMANDS		(USER_COMMANDS + ISR_COMMANDS)

#define VPM150M_HPI_CONTROL	0x00
#define VPM150M_HPI_ADDRESS	0x02
#define VPM150M_HPI_DATA	0x03

#define VPM_SUPPORT

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define VPM150M_SUPPORT
#endif

#ifdef VPM_SUPPORT

/* Define to get more attention-grabbing but slightly more CPU using echocan status */
#define FANCY_ECHOCAN

#endif

#ifdef VPM150M_SUPPORT
#include "voicebus/GpakCust.h"
#endif

struct calregs {
	unsigned char vals[NUM_CAL_REGS];
};

struct cmdq {
	unsigned int cmds[MAX_COMMANDS];
	unsigned char isrshadow[ISR_COMMANDS];
};

enum battery_state {
	BATTERY_UNKNOWN = 0,
	BATTERY_PRESENT,
	BATTERY_LOST,
};

/**
 * struct wctdm_span -
 * @span:		dahdi_span to register.
 * @timing_priority:	What the priority of this span is relative to the other
 * 			spans.
 * @spanno:		Which span on the card this is.
 *
 * NOTE:  spanno would normally be taken care of by dahdi_span.offset, but
 * appears to have meaning in xhfc.c, and that needs to be audited before
 * changing. !!!TODO!!!
 *
 */
struct wctdm_span {
	struct dahdi_span span;
	int timing_priority;
	int spanno;
	struct wctdm *wc;
	struct b400m_span *bspan;
};

struct wctdm_chan {
	struct dahdi_chan chan;
	struct dahdi_echocan_state ec;
	int timeslot;
};

struct wctdm {
	const struct wctdm_desc *desc;
	char board_name[80];
	int usecount;
	int pos;				/* card number in system */

	spinlock_t frame_list_lock;
	struct list_head frame_list;

	unsigned int intcount;
	unsigned char txident;
	unsigned char rxident;

	int flags[NUM_MODULES];			/* bitmap of board-specific + module-specific flags */
	unsigned char ctlreg;			/* FIXME: something to do with VPM? */

	int mods_per_board;			/* maximum number of modules for this board */
	int digi_mods;				/* number of digital modules present */
	int avchannels;				/* active "voice" (voice, B and D) channels */
	int modmap;				/* Bit-map of present cards (1=present) */

	int altcs[NUM_MODULES + NUM_EC];

/* FIXME: why are all of these QRV-only members part of the main card structure? */
	char qrvhook[NUM_MODULES];
	unsigned short qrvdebtime[NUM_MODULES];
	int radmode[NUM_MODULES];
#define	RADMODE_INVERTCOR 1
#define	RADMODE_IGNORECOR 2
#define	RADMODE_EXTTONE 4
#define	RADMODE_EXTINVERT 8
#define	RADMODE_IGNORECT 16
#define	RADMODE_PREEMP	32
#define	RADMODE_DEEMP 64
	unsigned short debouncetime[NUM_MODULES];
	signed short rxgain[NUM_MODULES];
	signed short txgain[NUM_MODULES];

	spinlock_t reglock;			/* held when accessing anything affecting the module array */
	wait_queue_head_t regq;			/* for schluffen() */

	union {
		struct fxo {
			int wasringing;
			int lastrdtx;
			int lastrdtx_count;
			int ringdebounce;
			int offhook;
			int battdebounce;
			int battalarm;
			enum battery_state battery;
			int lastpol;
			int polarity;
			int polaritydebounce;
			int neonmwi_state;
			int neonmwi_last_voltage;
			unsigned int neonmwi_debounce;
			unsigned int neonmwi_offcounter;
		} fxo;
		struct fxs {
			int oldrxhook;
			int debouncehook;
			int lastrxhook;
			int debounce;
			int ohttimer;
			int idletxhookstate;	/* IDLE changing hook state */
	/* lasttxhook reflects the last value written to the proslic's reg
	* 64 (LINEFEED_CONTROL) in bits 0-2.  Bit 4 indicates if the last
	* write is pending i.e. it is in process of being written to the
	* register
	* NOTE: in order for this value to actually be written to the
	* proslic, the appropriate matching value must be written into the
	* sethook variable so that it gets queued and handled by the
	* voicebus ISR.
	*/
			int lasttxhook;
			int oppending_ms;
			spinlock_t lasttxhooklock;
			int palarms;
			struct dahdi_vmwi_info vmwisetting;
			int vmwi_active_messages;
			int vmwi_linereverse;
			int reversepolarity;	/* polarity reversal */
			struct calregs calregs;
		} fxs;
		struct b400m *bri;
	} mods[NUM_MODULES];

	struct cmdq cmdq[NUM_MODULES + NUM_EC];
	int modtype[NUM_MODULES + NUM_EC];		/* type of module (FXO/FXS/QRV/VPM/etc.) in this position */
	int sethook[NUM_MODULES + NUM_EC];		/* pending hook state command for each module */
	int dacssrc[NUM_MODULES];

	int vpm100;

	struct vpmadt032 *vpmadt032;
#ifdef FANCY_ECHOCAN
	int echocanpos;
	int blinktimer;
#endif
	struct voicebus vb;
	struct wctdm_span *aspan;			/* pointer to the spans[] holding the analog span */
	struct wctdm_span *spans[MAX_SPANS];
	struct wctdm_chan *chans[NUM_MODULES];
#ifdef CONFIG_VOICEBUS_ECREFERENCE
	struct dahdi_fifo *ec_reference[NUM_MODULES];
#endif

	/* Only care about digital spans here */
	/* int span_timing_prio[MAX_SPANS - 1]; */
	struct semaphore syncsem;
	int oldsync;

	int initialized;				/* =1 when the entire card is ready to go */
	unsigned long checkflag;			/* Internal state flags and task bits */
	int companding;
};

/* Atomic flag bits for checkflag field */
#define WCTDM_CHECK_TIMING	0

int schluffen(wait_queue_head_t *q);
void wait_just_a_bit(int foo);
int wctdm_getreg(struct wctdm *wc, int card, int addr);
int wctdm_setreg(struct wctdm *wc, int card, int addr, int val);

extern struct semaphore ifacelock;
extern struct wctdm *ifaces[WC_MAX_IFACES];

#endif
