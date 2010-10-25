/*
 * Wildcard TDM2400P TDM FXS/FXO Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 * Support for TDM800P and VPM150M by Matthew Fredrickson <creslin@digium.com>
 *
 * Support for Hx8 by Andrew Kohlsmith <akohlsmith@mixdown.ca> and Matthew
 * Fredrickson <creslin@digium.com>
 *
 * Copyright (C) 2005 - 2010 Digium, Inc.
 * All rights reserved.
 *
 * Sections for QRV cards written by Jim Dixon <jim@lambdatel.com>
 * Copyright (C) 2006, Jim Dixon and QRV Communications
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

/* For QRV DRI cards, gain is signed short, expressed in hundredths of
db (in reference to 1v Peak @ 1000Hz) , as follows:

Rx Gain: -11.99 to 15.52 db
Tx Gain - No Pre-Emphasis: -35.99 to 12.00 db
Tx Gain - W/Pre-Emphasis: -23.99 to 0.00 db
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include <linux/crc32.h>

#include <stdbool.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
/* Define this if you would like to load the modules in parallel.  While this
 * can speed up loads when multiple cards handled by this driver are installed,
 * it also makes it impossible to abort module loads with ctrl-c */
#undef USE_ASYNC_INIT
#include <linux/async.h>
#else
#undef USE_ASYNC_INIT
#endif

#include <dahdi/kernel.h>
#include <dahdi/wctdm_user.h>

#include "proslic.h"

#include "wctdm24xxp.h"
#include "xhfc.h"

#include "adt_lec.h"

#include "voicebus/GpakCust.h"
#include "voicebus/GpakApi.h"

#if VOICEBUS_SFRAME_SIZE != SFRAME_SIZE
#error SFRAME_SIZE must match the VOICEBUS_SFRAME_SIZE
#endif

/*
  Experimental max loop current limit for the proslic
  Loop current limit is from 20 mA to 41 mA in steps of 3
  (according to datasheet)
  So set the value below to:
  0x00 : 20mA (default)
  0x01 : 23mA
  0x02 : 26mA
  0x03 : 29mA
  0x04 : 32mA
  0x05 : 35mA
  0x06 : 37mA
  0x07 : 41mA
*/
static int loopcurrent = 20;

/* Following define is a logical exclusive OR to determine if the polarity of an fxs line is to be reversed.
 * 	The items taken into account are:
 * 	overall polarity reversal for the module,
 * 	polarity reversal for the port,
 * 	and the state of the line reversal MWI indicator
 */
#define POLARITY_XOR(card) ( (reversepolarity != 0) ^ (wc->mods[(card)].fxs.reversepolarity != 0) ^ (wc->mods[(card)].fxs.vmwi_linereverse != 0) )
static int reversepolarity = 0;

static alpha  indirect_regs[] =
{
{0,255,"DTMF_ROW_0_PEAK",0x55C2},
{1,255,"DTMF_ROW_1_PEAK",0x51E6},
{2,255,"DTMF_ROW2_PEAK",0x4B85},
{3,255,"DTMF_ROW3_PEAK",0x4937},
{4,255,"DTMF_COL1_PEAK",0x3333},
{5,255,"DTMF_FWD_TWIST",0x0202},
{6,255,"DTMF_RVS_TWIST",0x0202},
{7,255,"DTMF_ROW_RATIO_TRES",0x0198},
{8,255,"DTMF_COL_RATIO_TRES",0x0198},
{9,255,"DTMF_ROW_2ND_ARM",0x0611},
{10,255,"DTMF_COL_2ND_ARM",0x0202},
{11,255,"DTMF_PWR_MIN_TRES",0x00E5},
{12,255,"DTMF_OT_LIM_TRES",0x0A1C},
{13,0,"OSC1_COEF",0x7B30},
{14,1,"OSC1X",0x0063},
{15,2,"OSC1Y",0x0000},
{16,3,"OSC2_COEF",0x7870},
{17,4,"OSC2X",0x007D},
{18,5,"OSC2Y",0x0000},
{19,6,"RING_V_OFF",0x0000},
{20,7,"RING_OSC",0x7EF0},
{21,8,"RING_X",0x0160},
{22,9,"RING_Y",0x0000},
{23,255,"PULSE_ENVEL",0x2000},
{24,255,"PULSE_X",0x2000},
{25,255,"PULSE_Y",0x0000},
//{26,13,"RECV_DIGITAL_GAIN",0x4000},	// playback volume set lower
{26,13,"RECV_DIGITAL_GAIN",0x2000},	// playback volume set lower
{27,14,"XMIT_DIGITAL_GAIN",0x4000},
//{27,14,"XMIT_DIGITAL_GAIN",0x2000},
{28,15,"LOOP_CLOSE_TRES",0x1000},
{29,16,"RING_TRIP_TRES",0x3600},
{30,17,"COMMON_MIN_TRES",0x1000},
{31,18,"COMMON_MAX_TRES",0x0200},
{32,19,"PWR_ALARM_Q1Q2",0x07C0},
{33,20,"PWR_ALARM_Q3Q4", 0x4C00 /* 0x2600 */},
{34,21,"PWR_ALARM_Q5Q6",0x1B80},
{35,22,"LOOP_CLOSURE_FILTER",0x8000},
{36,23,"RING_TRIP_FILTER",0x0320},
{37,24,"TERM_LP_POLE_Q1Q2",0x008C},
{38,25,"TERM_LP_POLE_Q3Q4",0x0100},
{39,26,"TERM_LP_POLE_Q5Q6",0x0010},
{40,27,"CM_BIAS_RINGING",0x0C00},
{41,64,"DCDC_MIN_V",0x0C00},
{42,255,"DCDC_XTRA",0x1000},
{43,66,"LOOP_CLOSE_TRES_LOW",0x1000},
};

#ifdef FANCY_ECHOCAN
static char ectab[] = {
0, 0, 0, 1, 2, 3, 4, 6, 8, 9, 11, 13, 16, 18, 20, 22, 24, 25, 27, 28, 29, 30, 31, 31, 32, 
32, 32, 32, 32, 32, 32, 32, 32, 32, 32 ,32 ,32, 32,
32, 32, 32, 32, 32, 32, 32, 32, 32, 32 ,32 ,32, 32,
32, 32, 32, 32, 32, 32, 32, 32, 32, 32 ,32 ,32, 32,
31, 31, 30, 29, 28, 27, 25, 23, 22, 20, 18, 16, 13, 11, 9, 8, 6, 4, 3, 2, 1, 0, 0, 
};
static int ectrans[4] = { 0, 1, 3, 2 };
#define EC_SIZE (sizeof(ectab))
#define EC_SIZE_Q (sizeof(ectab) / 4)
#endif

/* Undefine to enable Power alarm / Transistor debug -- note: do not
   enable for normal operation! */
/* #define PAQ_DEBUG */

#define DEBUG_CARD (1 << 0)
#define DEBUG_ECHOCAN (1 << 1)

#include "fxo_modes.h"

struct wctdm_desc {
	const char *name;
	const int flags;
	const int ports;
};

static const struct wctdm_desc wctdm2400 = { "Wildcard TDM2400P", 0, 24 };
static const struct wctdm_desc wctdm800 = { "Wildcard TDM800P", 0, 8 };
static const struct wctdm_desc wctdm410 = { "Wildcard TDM410P", 0, 4 };
static const struct wctdm_desc wcaex2400 = { "Wildcard AEX2400", FLAG_EXPRESS, 24 };
static const struct wctdm_desc wcaex800 = { "Wildcard AEX800", FLAG_EXPRESS, 8 };
static const struct wctdm_desc wcaex410 = { "Wildcard AEX410", FLAG_EXPRESS, 4 };
static const struct wctdm_desc wcha80000 = { "HA8-0000", 0, 8 };
static const struct wctdm_desc wchb80000 = { "HB8-0000", FLAG_EXPRESS, 8 };

/**
 * Returns true if the card is one of the Hybrid Digital Analog Cards.
 */
static inline bool is_hx8(const struct wctdm *wc)
{
	return (&wcha80000 == wc->desc) || (&wchb80000 == wc->desc);
}

struct wctdm *ifaces[WC_MAX_IFACES];
DECLARE_MUTEX(ifacelock);

static void wctdm_release(struct wctdm *wc);

static int fxovoltage = 0;
static unsigned int battdebounce;
static unsigned int battalarm;
static unsigned int battthresh;
static int debug = 0;
#ifdef DEBUG
static int robust = 0;
static int digitalloopback;
#endif
static int lowpower = 0;
static int boostringer = 0;
static int fastringer = 0;
static int _opermode = 0;
static char *opermode = "FCC";
static int fxshonormode = 0;
static int alawoverride = 0;
static char *companding = "auto";
static int fxotxgain = 0;
static int fxorxgain = 0;
static int fxstxgain = 0;
static int fxsrxgain = 0;
static int nativebridge = 0;
static int ringdebounce = DEFAULT_RING_DEBOUNCE;
static int fwringdetect = 0;
static int latency = VOICEBUS_DEFAULT_LATENCY;
static unsigned int max_latency = VOICEBUS_DEFAULT_MAXLATENCY;
static int forceload;

#define MS_PER_HOOKCHECK	(1)
#define NEONMWI_ON_DEBOUNCE	(100/MS_PER_HOOKCHECK)
static int neonmwi_monitor = 0; 	/* Note: this causes use of full wave ring detect */
static int neonmwi_level = 75;		/* neon mwi trip voltage */
static int neonmwi_envelope = 10;
static int neonmwi_offlimit = 16000;  /* Time in milliseconds the monitor is checked before saying no message is waiting */
static int neonmwi_offlimit_cycles;  /* Time in milliseconds the monitor is checked before saying no message is waiting */

static int vpmsupport = 1;

static int vpmnlptype = DEFAULT_NLPTYPE;
static int vpmnlpthresh = DEFAULT_NLPTHRESH;
static int vpmnlpmaxsupp = DEFAULT_NLPMAXSUPP;

static void echocan_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);

static const struct dahdi_echocan_features vpm100m_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_features vpm150m_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_ops vpm100m_ec_ops = {
	.name = "VPM100M",
	.echocan_free = echocan_free,
};

static const struct dahdi_echocan_ops vpm150m_ec_ops = {
	.name = "VPM150M",
	.echocan_free = echocan_free,
};

static int wctdm_init_proslic(struct wctdm *wc, int card, int fast , int manual, int sane);

static inline int CMD_BYTE(int card, int bit, int altcs)
{
	/* Let's add some trickery to make the TDM410 work */
	if (altcs == 3) {
		if (card == 2) {
			card = 4;
			altcs = 0;
		} else if (card == 3) {
			card = 5;
			altcs = 2;
		}
	}

	return (((((card) & 0x3) * 3 + (bit)) * 7) \
			+ ((card) >> 2) + (altcs) + ((altcs) ? -21 : 0));
}

/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
int schluffen(wait_queue_head_t *q)
{
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(q, &wait);
	current->state = TASK_INTERRUPTIBLE;
	if (!signal_pending(current)) schedule();
	current->state = TASK_RUNNING;
	remove_wait_queue(q, &wait);
	if (signal_pending(current)) return -ERESTARTSYS;
	return(0);
}

static inline int empty_slot(struct wctdm *wc, int card)
{
	int x;
	for (x = 0; x < USER_COMMANDS; x++) {
		if (!wc->cmdq[card].cmds[x])
			return x;
	}
	return -1;
}

static void
setchanconfig_from_state(struct vpmadt032 *vpm, int channel,
			 GpakChannelConfig_t *chanconfig)
{
	const struct vpmadt032_options *options;
	GpakEcanParms_t *p;

	BUG_ON(!vpm);

	options = &vpm->options;

	chanconfig->PcmInPortA = 3;
	chanconfig->PcmInSlotA = channel;
	chanconfig->PcmOutPortA = SerialPortNull;
	chanconfig->PcmOutSlotA = channel;
	chanconfig->PcmInPortB = 2;
	chanconfig->PcmInSlotB = channel;
	chanconfig->PcmOutPortB = 3;
	chanconfig->PcmOutSlotB = channel;
	chanconfig->ToneTypesA = Null_tone;
	chanconfig->MuteToneA = Disabled;
	chanconfig->FaxCngDetA = Disabled;
	chanconfig->ToneTypesB = Null_tone;
	chanconfig->EcanEnableA = Enabled;
	chanconfig->EcanEnableB = Disabled;
	chanconfig->MuteToneB = Disabled;
	chanconfig->FaxCngDetB = Disabled;

	/* The software companding will be overridden on a channel by channel
	 * basis when the channel is enabled. */
	chanconfig->SoftwareCompand = cmpPCMU;

	chanconfig->FrameRate = rate2ms;
	p = &chanconfig->EcanParametersA;

	vpmadt032_get_default_parameters(p);

	p->EcanNlpType = vpm->curecstate[channel].nlp_type;
	p->EcanNlpThreshold = vpm->curecstate[channel].nlp_threshold;
	p->EcanNlpMaxSuppress = vpm->curecstate[channel].nlp_max_suppress;

	memcpy(&chanconfig->EcanParametersB,
		&chanconfig->EcanParametersA,
		sizeof(chanconfig->EcanParametersB));
}

static int config_vpmadt032(struct vpmadt032 *vpm, struct wctdm *wc)
{
	int res, i;
	GpakPortConfig_t portconfig = {0};
	gpakConfigPortStatus_t configportstatus;
	GPAK_PortConfigStat_t pstatus;
	GpakChannelConfig_t chanconfig;
	GPAK_ChannelConfigStat_t cstatus;
	GPAK_AlgControlStat_t algstatus;

	/* First Serial Port config */
	portconfig.SlotsSelect1 = SlotCfgNone;
	portconfig.FirstBlockNum1 = 0;
	portconfig.FirstSlotMask1 = 0x0000;
	portconfig.SecBlockNum1 = 1;
	portconfig.SecSlotMask1 = 0x0000;
	portconfig.SerialWordSize1 = SerWordSize8;
	portconfig.CompandingMode1 = cmpNone;
	portconfig.TxFrameSyncPolarity1 = FrameSyncActHigh;
	portconfig.RxFrameSyncPolarity1 = FrameSyncActHigh;
	portconfig.TxClockPolarity1 = SerClockActHigh;
	portconfig.RxClockPolarity1 = SerClockActHigh;
	portconfig.TxDataDelay1 = DataDelay0;
	portconfig.RxDataDelay1 = DataDelay0;
	portconfig.DxDelay1 = Disabled;
	portconfig.ThirdSlotMask1 = 0x0000;
	portconfig.FouthSlotMask1 = 0x0000;
	portconfig.FifthSlotMask1 = 0x0000;
	portconfig.SixthSlotMask1 = 0x0000;
	portconfig.SevenSlotMask1 = 0x0000;
	portconfig.EightSlotMask1 = 0x0000;

	/* Second Serial Port config */
	portconfig.SlotsSelect2 = SlotCfg2Groups;
	portconfig.FirstBlockNum2 = 0;
	portconfig.FirstSlotMask2 = 0xffff;
	portconfig.SecBlockNum2 = 1;
	portconfig.SecSlotMask2 = 0xffff;
	portconfig.SerialWordSize2 = SerWordSize8;
	portconfig.CompandingMode2 = cmpNone;
	portconfig.TxFrameSyncPolarity2 = FrameSyncActHigh;
	portconfig.RxFrameSyncPolarity2 = FrameSyncActHigh;
	portconfig.TxClockPolarity2 = SerClockActHigh;
	portconfig.RxClockPolarity2 = SerClockActLow;
	portconfig.TxDataDelay2 = DataDelay0;
	portconfig.RxDataDelay2 = DataDelay0;
	portconfig.DxDelay2 = Disabled;
	portconfig.ThirdSlotMask2 = 0x0000;
	portconfig.FouthSlotMask2 = 0x0000;
	portconfig.FifthSlotMask2 = 0x0000;
	portconfig.SixthSlotMask2 = 0x0000;
	portconfig.SevenSlotMask2 = 0x0000;
	portconfig.EightSlotMask2 = 0x0000;

	/* Third Serial Port Config */
	portconfig.SlotsSelect3 = SlotCfg2Groups;
	portconfig.FirstBlockNum3 = 0;
	portconfig.FirstSlotMask3 = 0xffff;
	portconfig.SecBlockNum3 = 1;
	portconfig.SecSlotMask3 = 0xffff;
	portconfig.SerialWordSize3 = SerWordSize8;
	portconfig.CompandingMode3 = cmpNone;
	portconfig.TxFrameSyncPolarity3 = FrameSyncActHigh;
	portconfig.RxFrameSyncPolarity3 = FrameSyncActHigh;
	portconfig.TxClockPolarity3 = SerClockActHigh;
	portconfig.RxClockPolarity3 = SerClockActLow;
	portconfig.TxDataDelay3 = DataDelay0;
	portconfig.RxDataDelay3 = DataDelay0;
	portconfig.DxDelay3 = Disabled;
	portconfig.ThirdSlotMask3 = 0x0000;
	portconfig.FouthSlotMask3 = 0x0000;
	portconfig.FifthSlotMask3 = 0x0000;
	portconfig.SixthSlotMask3 = 0x0000;
	portconfig.SevenSlotMask3 = 0x0000;
	portconfig.EightSlotMask3 = 0x0000;

	if ((configportstatus = gpakConfigurePorts(vpm->dspid, &portconfig, &pstatus))) {
		dev_notice(&wc->vb.pdev->dev, "Configuration of ports failed (%d)!\n", configportstatus);
		return -1;
	} else {
		if (vpm->options.debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "Configured McBSP ports successfully\n");
	}

	if ((res = gpakPingDsp(vpm->dspid, &vpm->version))) {
		dev_notice(&wc->vb.pdev->dev, "Error pinging DSP (%d)\n", res);
		return -1;
	}

	for (i = 0; i < vpm->options.channels; ++i) {
		vpm->curecstate[i].tap_length = 0;
		vpm->curecstate[i].nlp_type = vpm->options.vpmnlptype;
		vpm->curecstate[i].nlp_threshold = vpm->options.vpmnlpthresh;
		vpm->curecstate[i].nlp_max_suppress = vpm->options.vpmnlpmaxsupp;
		vpm->curecstate[i].companding = (wc->chans[i]->chan.span->deflaw == DAHDI_LAW_ALAW) ? ADT_COMP_ALAW : ADT_COMP_ULAW;
		/* set_vpmadt032_chanconfig_from_state(&vpm->curecstate[i], &vpm->options, i, &chanconfig); !!! */
		vpm->setchanconfig_from_state(vpm, i, &chanconfig);
		if ((res = gpakConfigureChannel(vpm->dspid, i, tdmToTdm, &chanconfig, &cstatus))) {
			dev_notice(&wc->vb.pdev->dev, "Unable to configure channel #%d (%d)", i, res);
			if (res == 1) {
				printk(KERN_CONT ", reason %d", cstatus);
			}
			printk(KERN_CONT "\n");
			return -1;
		}

		if ((res = gpakAlgControl(vpm->dspid, i, BypassEcanA, &algstatus))) {
			dev_notice(&wc->vb.pdev->dev, "Unable to disable echo can on channel %d (reason %d:%d)\n", i + 1, res, algstatus);
			return -1;
		}

		if ((res = gpakAlgControl(vpm->dspid, i, BypassSwCompanding, &algstatus))) {
			dev_notice(&wc->vb.pdev->dev, "Unable to disable echo can on channel %d (reason %d:%d)\n", i + 1, res, algstatus);
			return -1;
		}
	}

	if ((res = gpakPingDsp(vpm->dspid, &vpm->version))) {
		dev_notice(&wc->vb.pdev->dev, "Error pinging DSP (%d)\n", res);
		return -1;
	}

	set_bit(VPM150M_ACTIVE, &vpm->control);

	return 0;
}

/**
 * is_good_frame() - Whether the SFRAME received was one sent.
 *
 */
static inline bool is_good_frame(const u8 *sframe)
{
        const u8 a = sframe[0*(EFRAME_SIZE+EFRAME_GAP) + (EFRAME_SIZE+1)];
        const u8 b = sframe[1*(EFRAME_SIZE+EFRAME_GAP) + (EFRAME_SIZE+1)];
        return a != b;
}

static inline void cmd_dequeue_vpmadt032(struct wctdm *wc, u8 *writechunk)
{
	unsigned long flags;
	struct vpmadt032_cmd *curcmd = NULL;
	struct vpmadt032 *vpmadt032 = wc->vpmadt032;
	int x;
	unsigned char leds = ~((wc->intcount / 1000) % 8) & 0x7;

	/* Skip audio */
	writechunk += 24;

	if (test_bit(VPM150M_SPIRESET, &vpmadt032->control) || test_bit(VPM150M_HPIRESET, &vpmadt032->control)) {
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "HW Resetting VPMADT032...\n");
		spin_lock_irqsave(&wc->reglock, flags);
		for (x = 24; x < 28; x++) {
			if (x == 24) {
				if (test_and_clear_bit(VPM150M_SPIRESET, &vpmadt032->control))
					writechunk[CMD_BYTE(x, 0, 0)] = 0x08;
				else if (test_and_clear_bit(VPM150M_HPIRESET, &vpmadt032->control))
					writechunk[CMD_BYTE(x, 0, 0)] = 0x0b;
			} else
				writechunk[CMD_BYTE(x, 0, 0)] = 0x00 | leds;
			writechunk[CMD_BYTE(x, 1, 0)] = 0;
			writechunk[CMD_BYTE(x, 2, 0)] = 0x00;
		}
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}

	if ((curcmd = vpmadt032_get_ready_cmd(vpmadt032))) {
		curcmd->txident = wc->txident;
#if 0
		// if (printk_ratelimit()) 
			dev_info(&wc->vb.pdev->dev, "Transmitting txident = %d, desc = 0x%x, addr = 0x%x, data = 0x%x\n", curcmd->txident, curcmd->desc, curcmd->address, curcmd->data);
#endif
		if (curcmd->desc & __VPM150M_RWPAGE) {
			/* Set CTRL access to page*/
			writechunk[CMD_BYTE(24, 0, 0)] = (0x8 << 4);
			writechunk[CMD_BYTE(24, 1, 0)] = 0;
			writechunk[CMD_BYTE(24, 2, 0)] = 0x20;

			/* Do a page write */
			if (curcmd->desc & __VPM150M_WR)
				writechunk[CMD_BYTE(25, 0, 0)] = ((0x8 | 0x4) << 4);
			else
				writechunk[CMD_BYTE(25, 0, 0)] = ((0x8 | 0x4 | 0x1) << 4);
			writechunk[CMD_BYTE(25, 1, 0)] = 0;
			if (curcmd->desc & __VPM150M_WR)
				writechunk[CMD_BYTE(25, 2, 0)] = curcmd->data & 0xf;
			else
				writechunk[CMD_BYTE(25, 2, 0)] = 0;

			/* Clear XADD */
			writechunk[CMD_BYTE(26, 0, 0)] = (0x8 << 4);
			writechunk[CMD_BYTE(26, 1, 0)] = 0;
			writechunk[CMD_BYTE(26, 2, 0)] = 0;

			/* Fill in to buffer to size */
			writechunk[CMD_BYTE(27, 0, 0)] = 0;
			writechunk[CMD_BYTE(27, 1, 0)] = 0;
			writechunk[CMD_BYTE(27, 2, 0)] = 0;

		} else {
			/* Set address */
			writechunk[CMD_BYTE(24, 0, 0)] = ((0x8 | 0x4) << 4);
			writechunk[CMD_BYTE(24, 1, 0)] = (curcmd->address >> 8) & 0xff;
			writechunk[CMD_BYTE(24, 2, 0)] = curcmd->address & 0xff;

			/* Send/Get our data */
			writechunk[CMD_BYTE(25, 0, 0)] = (curcmd->desc & __VPM150M_WR) ?
				((0x8 | (0x3 << 1)) << 4) : ((0x8 | (0x3 << 1) | 0x1) << 4);
			writechunk[CMD_BYTE(25, 1, 0)] = (curcmd->data >> 8) & 0xff;
			writechunk[CMD_BYTE(25, 2, 0)] = curcmd->data & 0xff;
			
			writechunk[CMD_BYTE(26, 0, 0)] = 0;
			writechunk[CMD_BYTE(26, 1, 0)] = 0;
			writechunk[CMD_BYTE(26, 2, 0)] = 0;

			/* Fill in the rest */
			writechunk[CMD_BYTE(27, 0, 0)] = 0;
			writechunk[CMD_BYTE(27, 1, 0)] = 0;
			writechunk[CMD_BYTE(27, 2, 0)] = 0;
		}
	} else if (test_and_clear_bit(VPM150M_SWRESET, &vpmadt032->control)) {
		for (x = 24; x < 28; x++) {
			if (x == 24)
				writechunk[CMD_BYTE(x, 0, 0)] = (0x8 << 4);
			else
				writechunk[CMD_BYTE(x, 0, 0)] = 0x00;
			writechunk[CMD_BYTE(x, 1, 0)] = 0;
			if (x == 24)
				writechunk[CMD_BYTE(x, 2, 0)] = 0x01;
			else
				writechunk[CMD_BYTE(x, 2, 0)] = 0x00;
		}
	} else {
		for (x = 24; x < 28; x++) {
			writechunk[CMD_BYTE(x, 0, 0)] = 0x00;
			writechunk[CMD_BYTE(x, 1, 0)] = 0x00;
			writechunk[CMD_BYTE(x, 2, 0)] = 0x00;
		}
	}

	/* Add our leds in */
	for (x = 24; x < 28; x++) {
		writechunk[CMD_BYTE(x, 0, 0)] |= leds;
	}
}

static inline void cmd_dequeue(struct wctdm *wc, unsigned char *writechunk, int card, int pos)
{
	unsigned long flags;
	unsigned int curcmd=0;
	int x;
	int subaddr = card & 0x3;
#ifdef FANCY_ECHOCAN
	int ecval;
	ecval = wc->echocanpos;
	ecval += EC_SIZE_Q * ectrans[(card & 0x3)];
	ecval = ecval % EC_SIZE;
#endif

	/* QRV and BRI modules only use commands relating to the first channel */
	if ((card & 0x03) && (wc->modtype[card] ==  MOD_TYPE_QRV)) {
		return;
	}

	if (wc->altcs[card])
		subaddr = 0;

	/* Skip audio */
	writechunk += 24;
	spin_lock_irqsave(&wc->reglock, flags);
	/* Search for something waiting to transmit */
	if (pos) {
		for (x = 0; x < MAX_COMMANDS; x++) {
			if ((wc->cmdq[card].cmds[x] & (__CMD_RD | __CMD_WR)) && 
			   !(wc->cmdq[card].cmds[x] & (__CMD_TX | __CMD_FIN))) {
			   	curcmd = wc->cmdq[card].cmds[x];
				wc->cmdq[card].cmds[x] |= (wc->txident << 24) | __CMD_TX;
				break;
			}
		}
	}

	if (!curcmd) {
		/* If nothing else, use filler */
		if (wc->modtype[card] == MOD_TYPE_FXS)
			curcmd = CMD_RD(LINE_STATE);
		else if (wc->modtype[card] == MOD_TYPE_FXO)
			curcmd = CMD_RD(12);
		else if (wc->modtype[card] == MOD_TYPE_BRI)
			curcmd = 0x101010;
		else if (wc->modtype[card] == MOD_TYPE_QRV)
			curcmd = CMD_RD(3);
		else if (wc->modtype[card] == MOD_TYPE_VPM) {
#ifdef FANCY_ECHOCAN
			if (wc->blinktimer >= 0xf) {
				curcmd = CMD_WR(0x1ab, 0x0f);
			} else if (wc->blinktimer == (ectab[ecval] >> 1)) {
				curcmd = CMD_WR(0x1ab, 0x00);
			} else
#endif
			curcmd = CMD_RD(0x1a0);
		}
	}

	if (wc->modtype[card] == MOD_TYPE_FXS) {
		writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = (1 << (subaddr));
		if (curcmd & __CMD_WR)
			writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = (curcmd >> 8) & 0x7f;
		else
			writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0x80 | ((curcmd >> 8) & 0x7f);
		writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = curcmd & 0xff;

	} else if (wc->modtype[card] == MOD_TYPE_FXO) {
		static const int FXO_ADDRS[4] = { 0x00, 0x08, 0x04, 0x0c };
		int idx = CMD_BYTE(card, 0, wc->altcs[card]);
		if (curcmd & __CMD_WR)
			writechunk[idx] = 0x20 | FXO_ADDRS[subaddr];
		else
			writechunk[idx] = 0x60 | FXO_ADDRS[subaddr];
		writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = (curcmd >> 8) & 0xff;
		writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = curcmd & 0xff;

	} else if (wc->modtype[card] == MOD_TYPE_FXSINIT) {
		/* Special case, we initialize the FXS's into the three-byte command mode then
		   switch to the regular mode.  To send it into thee byte mode, treat the path as
		   6 two-byte commands and in the last one we initialize register 0 to 0x80. All modules
		   read this as the command to switch to daisy chain mode and we're done.  */
		writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = 0x00;
		writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0x00;
		if ((card & 0x1) == 0x1) 
			writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = 0x80;
		else
			writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = 0x00;

	} else if (wc->modtype[card] == MOD_TYPE_BRI) {

		if (unlikely((curcmd != 0x101010) && (curcmd & 0x1010) == 0x1010)) /* b400m CPLD */
			writechunk[CMD_BYTE(card, 0, 0)] = 0x55;
		else /* xhfc */
			writechunk[CMD_BYTE(card, 0, 0)] = 0x10;
		writechunk[CMD_BYTE(card, 1, 0)] = (curcmd >> 8) & 0xff;
		writechunk[CMD_BYTE(card, 2, 0)] = curcmd & 0xff;
	} else if (wc->modtype[card] == MOD_TYPE_VPM) {
		if (curcmd & __CMD_WR)
			writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = ((card & 0x3) << 4) | 0xc | ((curcmd >> 16) & 0x1);
		else
			writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = ((card & 0x3) << 4) | 0xa | ((curcmd >> 16) & 0x1);
		writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = (curcmd >> 8) & 0xff;
		writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = curcmd & 0xff;
	} else if (wc->modtype[card] == MOD_TYPE_QRV) {
 
		writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = 0x00;
		if (!curcmd) {
			writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0x00;
			writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = 0x00;
		} else {
			if (curcmd & __CMD_WR)
				writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0x40 | ((curcmd >> 8) & 0x3f);
			else
				writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0xc0 | ((curcmd >> 8) & 0x3f);
			writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = curcmd & 0xff;
		}
	} else if (wc->modtype[card] == MOD_TYPE_NONE) {
		writechunk[CMD_BYTE(card, 0, wc->altcs[card])] = 0x10;
		writechunk[CMD_BYTE(card, 1, wc->altcs[card])] = 0x10;
		writechunk[CMD_BYTE(card, 2, wc->altcs[card])] = 0x10;
	}
#if 0
	/* XXX */
	if (cmddesc < 40)
		dev_info(&wc->vb.pdev->dev, "Pass %d, card = %d (modtype=%d), pos = %d, CMD_BYTES = %d,%d,%d, (%02x,%02x,%02x) curcmd = %08x\n", cmddesc, card, wc->modtype[card], pos, CMD_BYTE(card, 0), CMD_BYTE(card, 1), CMD_BYTE(card, 2), writechunk[CMD_BYTE(card, 0)], writechunk[CMD_BYTE(card, 1)], writechunk[CMD_BYTE(card, 2)], curcmd);
#endif
	spin_unlock_irqrestore(&wc->reglock, flags);
#if 0
	/* XXX */
	cmddesc++;
#endif
}

static inline void cmd_decipher_vpmadt032(struct wctdm *wc, const u8 *readchunk)
{
	unsigned long flags;
	struct vpmadt032 *vpm = wc->vpmadt032;
	struct vpmadt032_cmd *cmd;

	BUG_ON(!vpm);

	/* If the hardware is not processing any commands currently, then
	 * there is nothing for us to do here. */
	if (list_empty(&vpm->active_cmds)) {
		return;
	}

	spin_lock_irqsave(&vpm->list_lock, flags);
	cmd = list_entry(vpm->active_cmds.next, struct vpmadt032_cmd, node);
	if (wc->rxident == cmd->txident) {
		list_del_init(&cmd->node);
	} else {
		cmd = NULL;
	}
	spin_unlock_irqrestore(&vpm->list_lock, flags);

	if (!cmd) {
		return;
	}

	/* Skip audio */
	readchunk += 24;

	/* Store result */
	cmd->data = (0xff & readchunk[CMD_BYTE(25, 1, 0)]) << 8;
	cmd->data |= readchunk[CMD_BYTE(25, 2, 0)];
	if (cmd->desc & __VPM150M_WR) {
		kfree(cmd);
	} else {
		cmd->desc |= __VPM150M_FIN;
		complete(&cmd->complete);
	}
}

static inline void cmd_decipher(struct wctdm *wc, const u8 *readchunk, int card)
{
	unsigned long flags;
	unsigned char ident;
	int x;

	/* QRV and BRI modules only use commands relating to the first channel */
	if ((card & 0x03) && (wc->modtype[card] ==  MOD_TYPE_QRV)) { /* || (wc->modtype[card] ==  MOD_TYPE_BRI))) { */
		return;
	}

	/* Skip audio */
	readchunk += 24;
	spin_lock_irqsave(&wc->reglock, flags);

	/* Search for any pending results */
	for (x=0;x<MAX_COMMANDS;x++) {
		if ((wc->cmdq[card].cmds[x] & (__CMD_RD | __CMD_WR)) && 
		    (wc->cmdq[card].cmds[x] & (__CMD_TX)) && 
		   !(wc->cmdq[card].cmds[x] & (__CMD_FIN))) {
		   	ident = (wc->cmdq[card].cmds[x] >> 24) & 0xff;
		   	if (ident == wc->rxident) {
				/* Store result */
				wc->cmdq[card].cmds[x] |= readchunk[CMD_BYTE(card, 2, wc->altcs[card])];
				wc->cmdq[card].cmds[x] |= __CMD_FIN;
/*
				if (card == 0 && wc->cmdq[card].cmds[x] & __CMD_RD) {
					dev_info(&wc->vb.pdev->dev, "decifer: got response %02x\n", wc->cmdq[card].cmds[x] & 0xff);
				}
*/
				if (wc->cmdq[card].cmds[x] & __CMD_WR) {
					/* Go ahead and clear out writes since they need no acknowledgement */
					wc->cmdq[card].cmds[x] = 0x00000000;
				} else if (x >= USER_COMMANDS) {
					/* Clear out ISR reads */
					wc->cmdq[card].isrshadow[x - USER_COMMANDS] = wc->cmdq[card].cmds[x] & 0xff;
					wc->cmdq[card].cmds[x] = 0x00000000;
				}
				break;
			}
		}
	}
#if 0
	/* XXX */
	if (!pos && (cmddesc < 256))
		dev_info(&wc->vb.pdev->dev, "Card %d: Command '%08x' => %02x\n",card,  wc->cmdq[card].lasttx[pos], wc->cmdq[card].lastrd[pos]);
#endif
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void cmd_checkisr(struct wctdm *wc, int card)
{
	if (!wc->cmdq[card].cmds[USER_COMMANDS + 0]) {
		if (wc->sethook[card]) {
			wc->cmdq[card].cmds[USER_COMMANDS + 0] = wc->sethook[card];
			wc->sethook[card] = 0;
		} else if (wc->modtype[card] == MOD_TYPE_FXS) {
			wc->cmdq[card].cmds[USER_COMMANDS + 0] = CMD_RD(68);	/* Hook state */
		} else if (wc->modtype[card] == MOD_TYPE_FXO) {
			wc->cmdq[card].cmds[USER_COMMANDS + 0] = CMD_RD(5);	/* Hook/Ring state */
		} else if (wc->modtype[card] == MOD_TYPE_QRV) {
			wc->cmdq[card & 0xfc].cmds[USER_COMMANDS + 0] = CMD_RD(3);	/* COR/CTCSS state */
		} else if (wc->modtype[card] == MOD_TYPE_BRI) {
			wc->cmdq[card].cmds[USER_COMMANDS + 0] = wctdm_bri_checkisr(wc, card, 0);
#ifdef VPM_SUPPORT
		} else if (wc->modtype[card] == MOD_TYPE_VPM) {
			wc->cmdq[card].cmds[USER_COMMANDS + 0] = CMD_RD(0xb9); /* DTMF interrupt */
#endif
		}
	}
	if (!wc->cmdq[card].cmds[USER_COMMANDS + 1]) {
		if (wc->modtype[card] == MOD_TYPE_FXS) {
#ifdef PAQ_DEBUG
			wc->cmdq[card].cmds[USER_COMMANDS + 1] = CMD_RD(19);	/* Transistor interrupts */
#else
			wc->cmdq[card].cmds[USER_COMMANDS + 1] = CMD_RD(LINE_STATE);
#endif
		} else if (wc->modtype[card] == MOD_TYPE_FXO) {
			wc->cmdq[card].cmds[USER_COMMANDS + 1] = CMD_RD(29);	/* Battery */
		} else if (wc->modtype[card] == MOD_TYPE_QRV) {
			wc->cmdq[card & 0xfc].cmds[USER_COMMANDS + 1] = CMD_RD(3);	/* Battery */
		} else if (wc->modtype[card] == MOD_TYPE_BRI) {
			wc->cmdq[card].cmds[USER_COMMANDS + 1] = wctdm_bri_checkisr(wc, card, 1);
#ifdef VPM_SUPPORT
		} else if (wc->modtype[card] == MOD_TYPE_VPM) {
			wc->cmdq[card].cmds[USER_COMMANDS + 1] = CMD_RD(0xbd); /* DTMF interrupt */
#endif
		}
	}
}

/**
 * insert_tdm_data() - Move TDM data from channels to sframe.
 *
 */
static void insert_tdm_data(const struct wctdm *wc, u8 *sframe)
{
	int i;
	register u8 *chanchunk;

	for (i = 0; i < wc->avchannels; i += 4) {
		chanchunk = &wc->chans[0 + i]->chan.writechunk[0];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*0] = chanchunk[0];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*1] = chanchunk[1];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*2] = chanchunk[2];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*3] = chanchunk[3];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*4] = chanchunk[4];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*5] = chanchunk[5];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*6] = chanchunk[6];
		sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*7] = chanchunk[7];

		chanchunk = &wc->chans[1 + i]->chan.writechunk[0];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*0] = chanchunk[0];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*1] = chanchunk[1];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*2] = chanchunk[2];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*3] = chanchunk[3];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*4] = chanchunk[4];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*5] = chanchunk[5];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*6] = chanchunk[6];
		sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*7] = chanchunk[7];

		chanchunk = &wc->chans[2 + i]->chan.writechunk[0];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*0] = chanchunk[0];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*1] = chanchunk[1];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*2] = chanchunk[2];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*3] = chanchunk[3];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*4] = chanchunk[4];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*5] = chanchunk[5];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*6] = chanchunk[6];
		sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*7] = chanchunk[7];

		chanchunk = &wc->chans[3 + i]->chan.writechunk[0];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*0] = chanchunk[0];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*1] = chanchunk[1];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*2] = chanchunk[2];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*3] = chanchunk[3];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*4] = chanchunk[4];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*5] = chanchunk[5];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*6] = chanchunk[6];
		sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*7] = chanchunk[7];
	}
}

static inline void wctdm_transmitprep(struct wctdm *wc, unsigned char *writechunk)
{
	int x,y;
	struct dahdi_span *s;

	/* Calculate Transmission */
	if (likely(wc->initialized)) {
		for (x = 0; x < MAX_SPANS; x++) {
			if (wc->spans[x]) {
				s = &wc->spans[x]->span;
				dahdi_transmit(s);
			}
		}
		insert_tdm_data(wc, writechunk);
#ifdef CONFIG_VOICEBUS_ECREFERENCE
		for (x = 0; x < wc->avchannels; ++x) {
			__dahdi_fifo_put(wc->ec_reference[x],
					 wc->chans[x]->chan.writechunk,
					 DAHDI_CHUNKSIZE);
		}
#endif
	}

	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		/* Send a sample, as a 32-bit word */

		/* TODO: ABK: hmm, this was originally mods_per_board, but we
		 * need to worry about all the active "voice" timeslots, since
		 * BRI modules have a different number of TDM channels than
		 * installed modules. */
		for (y = 0; y < wc->avchannels; y++) {
			if (!x && y < wc->mods_per_board) {
				cmd_checkisr(wc, y);
			}

			if (y < wc->mods_per_board)
				cmd_dequeue(wc, writechunk, y, x);
		}
		if (!x)
			wc->blinktimer++;
		if (wc->vpm100) {
			for (y = NUM_MODULES; y < NUM_MODULES + NUM_EC; y++) {
				if (!x)
					cmd_checkisr(wc, y);
				cmd_dequeue(wc, writechunk, y, x);
			}
#ifdef FANCY_ECHOCAN
			if (wc->vpm100 && wc->blinktimer >= 0xf) {
				wc->blinktimer = -1;
				wc->echocanpos++;
			}
#endif			
		} else if (wc->vpmadt032) {
			cmd_dequeue_vpmadt032(wc, writechunk);
		}

		if (x < DAHDI_CHUNKSIZE - 1) {
			writechunk[EFRAME_SIZE] = wc->ctlreg;
			writechunk[EFRAME_SIZE + 1] = wc->txident++;

			if ((wc->desc->ports == 4) && ((wc->ctlreg & 0x10) || (wc->modtype[NUM_MODULES] == MOD_TYPE_NONE))) {
				writechunk[EFRAME_SIZE + 2] = 0;
				for (y = 0; y < 4; y++) {
					if (wc->modtype[y] == MOD_TYPE_NONE)
						writechunk[EFRAME_SIZE + 2] |= (1 << y);
				}
			} else
				writechunk[EFRAME_SIZE + 2] = 0xf;
		}
		writechunk += (EFRAME_SIZE + EFRAME_GAP);
	}
}

static inline int wctdm_setreg_full(struct wctdm *wc, int card, int addr, int val, int inisr)
{
	unsigned long flags;
	int hit=0;
	int ret;

	/* QRV and BRI cards are only addressed at their first "port" */
	if ((card & 0x03) && ((wc->modtype[card] ==  MOD_TYPE_QRV) ||
	    (wc->modtype[card] ==  MOD_TYPE_BRI)))
		return 0;

	do {
		spin_lock_irqsave(&wc->reglock, flags);
		hit = empty_slot(wc, card);
		if (hit > -1) {
			wc->cmdq[card].cmds[hit] = CMD_WR(addr, val);
		}
		spin_unlock_irqrestore(&wc->reglock, flags);
		if (inisr)
			break;
		if (hit < 0) {
			if ((ret = schluffen(&wc->regq)))
				return ret;
		}
	} while (hit < 0);
	return (hit > -1) ? 0 : -1;
}

static inline int wctdm_setreg_intr(struct wctdm *wc, int card, int addr, int val)
{
	return wctdm_setreg_full(wc, card, addr, val, 1);
}
inline int wctdm_setreg(struct wctdm *wc, int card, int addr, int val)
{
	return wctdm_setreg_full(wc, card, addr, val, 0);
}

inline int wctdm_getreg(struct wctdm *wc, int card, int addr)
{
	unsigned long flags;
	int hit;
	int ret=0;

	/* if a QRV card, use only its first channel */  
	if (wc->modtype[card] ==  MOD_TYPE_QRV)
	{
		if (card & 3) return(0);
	}
	do {
		spin_lock_irqsave(&wc->reglock, flags);
		hit = empty_slot(wc, card);
		if (hit > -1) {
			wc->cmdq[card].cmds[hit] = CMD_RD(addr);
		}
		spin_unlock_irqrestore(&wc->reglock, flags);
		if (hit < 0) {
			if ((ret = schluffen(&wc->regq)))
				return ret;
		}
	} while (hit < 0);
	do {
		spin_lock_irqsave(&wc->reglock, flags);
		if (wc->cmdq[card].cmds[hit] & __CMD_FIN) {
			ret = wc->cmdq[card].cmds[hit] & 0xff;
			wc->cmdq[card].cmds[hit] = 0x00000000;
			hit = -1;
		}
		spin_unlock_irqrestore(&wc->reglock, flags);
		if (hit > -1) {
			if ((ret = schluffen(&wc->regq)))
				return ret;
		}
	} while (hit > -1);
	return ret;
}


static inline unsigned char wctdm_vpm_in(struct wctdm *wc, int unit, const unsigned int addr)
{
	return wctdm_getreg(wc, unit + NUM_MODULES, addr);
}

static inline void wctdm_vpm_out(struct wctdm *wc, int unit, const unsigned int addr, const unsigned char val)
{
	wctdm_setreg(wc, unit + NUM_MODULES, addr, val);
}

static inline void cmd_retransmit(struct wctdm *wc)
{
	int x,y;
	unsigned long flags;
	/* Force retransmissions */
	spin_lock_irqsave(&wc->reglock, flags);
	for (x=0;x<MAX_COMMANDS;x++) {
		for (y = 0; y < wc->mods_per_board; y++) {
			if (wc->modtype[y] != MOD_TYPE_BRI) {
				if (!(wc->cmdq[y].cmds[x] & __CMD_FIN))
					wc->cmdq[y].cmds[x] &= ~(__CMD_TX | (0xff << 24));
			}
		}
	}
	spin_unlock_irqrestore(&wc->reglock, flags);
#ifdef VPM_SUPPORT
	if (wc->vpmadt032)
		vpmadt032_resend(wc->vpmadt032);
#endif
}

/**
 * extract_tdm_data() - Move TDM data from sframe to channels.
 *
 */
static void extract_tdm_data(struct wctdm *wc, const u8 *sframe)
{
	int i;
	register u8 *chanchunk;

	for (i = 0; i < wc->avchannels; i += 4) {
		chanchunk = &wc->chans[0 + i]->chan.readchunk[0];
		chanchunk[0] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*0];
		chanchunk[1] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*1];
		chanchunk[2] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*2];
		chanchunk[3] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*3];
		chanchunk[4] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*4];
		chanchunk[5] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*5];
		chanchunk[6] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*6];
		chanchunk[7] = sframe[0 + i + (EFRAME_SIZE + EFRAME_GAP)*7];

		chanchunk = &wc->chans[1 + i]->chan.readchunk[0];
		chanchunk[0] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*0];
		chanchunk[1] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*1];
		chanchunk[2] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*2];
		chanchunk[3] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*3];
		chanchunk[4] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*4];
		chanchunk[5] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*5];
		chanchunk[6] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*6];
		chanchunk[7] = sframe[1 + i + (EFRAME_SIZE + EFRAME_GAP)*7];

		chanchunk = &wc->chans[2 + i]->chan.readchunk[0];
		chanchunk[0] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*0];
		chanchunk[1] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*1];
		chanchunk[2] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*2];
		chanchunk[3] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*3];
		chanchunk[4] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*4];
		chanchunk[5] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*5];
		chanchunk[6] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*6];
		chanchunk[7] = sframe[2 + i + (EFRAME_SIZE + EFRAME_GAP)*7];

		chanchunk = &wc->chans[3 + i]->chan.readchunk[0];
		chanchunk[0] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*0];
		chanchunk[1] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*1];
		chanchunk[2] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*2];
		chanchunk[3] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*3];
		chanchunk[4] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*4];
		chanchunk[5] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*5];
		chanchunk[6] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*6];
		chanchunk[7] = sframe[3 + i + (EFRAME_SIZE + EFRAME_GAP)*7];
	}
}

static inline void wctdm_receiveprep(struct wctdm *wc, const u8 *readchunk)
{
	int x,y;
	bool irqmiss = 0;
	unsigned char expected;

	if (unlikely(!is_good_frame(readchunk)))
		return;

	if (likely(wc->initialized))
		extract_tdm_data(wc, readchunk);

	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		if (x < DAHDI_CHUNKSIZE - 1) {
			expected = wc->rxident+1;
			wc->rxident = readchunk[EFRAME_SIZE + 1];
			if (wc->rxident != expected) {
				irqmiss = 1;
				cmd_retransmit(wc);
			}
		}
		for (y = 0; y < wc->avchannels; y++) {
			cmd_decipher(wc, readchunk, y);
		}
		if (wc->vpm100) {
			for (y = NUM_MODULES; y < NUM_MODULES + NUM_EC; y++)
				cmd_decipher(wc, readchunk, y);
		} else if (wc->vpmadt032)
			cmd_decipher_vpmadt032(wc, readchunk);

		readchunk += (EFRAME_SIZE + EFRAME_GAP);
	}

	/* XXX We're wasting 8 taps.  We should get closer :( */
	if (likely(wc->initialized)) {
		for (x = 0; x < wc->avchannels; x++) {
			struct dahdi_chan *c = &wc->chans[x]->chan;
#ifdef CONFIG_VOICEBUS_ECREFERENCE
			unsigned char buffer[DAHDI_CHUNKSIZE];
			__dahdi_fifo_get(wc->ec_reference[x], buffer,
				    ARRAY_SIZE(buffer));
			dahdi_ec_chunk(c, c->readchunk, buffer);
#else
			dahdi_ec_chunk(c, c->readchunk, c->writechunk);
#endif
		}

		for (x = 0; x < MAX_SPANS; x++) {
			if (wc->spans[x]) {
				struct dahdi_span *s = &wc->spans[x]->span;
#if 1
				/* Check for digital spans */
				if (s->ops->chanconfig == b400m_chanconfig) {
					BUG_ON(!is_hx8(wc));
					if (s->flags & DAHDI_FLAG_RUNNING)
						b400m_dchan(s);

				}
#endif
				dahdi_receive(s);
				if (unlikely(irqmiss))
					++s->irqmisses;
			}
		}
	}

	/* Wake up anyone sleeping to read/write a new register */
	wake_up_interruptible_all(&wc->regq);
}

static int wait_access(struct wctdm *wc, int card)
{
    unsigned char data=0;
    int count = 0;

    #define MAX 10 /* attempts */


    /* Wait for indirect access */
    while (count++ < MAX)
	 {
		data = wctdm_getreg(wc, card, I_STATUS);

		if (!data)
			return 0;

	 }

    if (count > (MAX-1))
	    dev_notice(&wc->vb.pdev->dev, " ##### Loop error (%02x) #####\n", data);

	return 0;
}

static unsigned char translate_3215(unsigned char address)
{
	int x;
	for (x = 0; x < ARRAY_SIZE(indirect_regs); x++) {
		if (indirect_regs[x].address == address) {
			address = indirect_regs[x].altaddr;
			break;
		}
	}
	return address;
}

static int wctdm_proslic_setreg_indirect(struct wctdm *wc, int card, unsigned char address, unsigned short data)
{
	int res = -1;
	/* Translate 3215 addresses */
	if (wc->flags[card] & FLAG_3215) {
		address = translate_3215(address);
		if (address == 255)
			return 0;
	}
	if (!wait_access(wc, card)) {
		wctdm_setreg(wc, card, IDA_LO,(unsigned char)(data & 0xFF));
		wctdm_setreg(wc, card, IDA_HI,(unsigned char)((data & 0xFF00)>>8));
		wctdm_setreg(wc, card, IAA,address);
		res = 0;
	};
	return res;
}

static int wctdm_proslic_getreg_indirect(struct wctdm *wc, int card, unsigned char address)
{ 
	int res = -1;
	char *p=NULL;
	/* Translate 3215 addresses */
	if (wc->flags[card] & FLAG_3215) {
		address = translate_3215(address);
		if (address == 255)
			return 0;
	}
	if (!wait_access(wc, card)) {
		wctdm_setreg(wc, card, IAA, address);
		if (!wait_access(wc, card)) {
			unsigned char data1, data2;
			data1 = wctdm_getreg(wc, card, IDA_LO);
			data2 = wctdm_getreg(wc, card, IDA_HI);
			res = data1 | (data2 << 8);
		} else
			p = "Failed to wait inside\n";
	} else
		p = "failed to wait\n";
	if (p)
		dev_notice(&wc->vb.pdev->dev, "%s", p);
	return res;
}

static int wctdm_proslic_init_indirect_regs(struct wctdm *wc, int card)
{
	unsigned char i;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) {
		if (wctdm_proslic_setreg_indirect(wc, card,
				indirect_regs[i].address,
				indirect_regs[i].initial))
			return -1;
	}

	return 0;
}

static int wctdm_proslic_verify_indirect_regs(struct wctdm *wc, int card)
{ 
	int passed = 1;
	unsigned short i, initial;
	int j;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) 
	{
		j = wctdm_proslic_getreg_indirect(wc, card, (unsigned char) indirect_regs[i].address);
		if (j < 0) {
			dev_notice(&wc->vb.pdev->dev, "Failed to read indirect register %d\n", i);
			return -1;
		}
		initial= indirect_regs[i].initial;

		if ( j != initial && (!(wc->flags[card] & FLAG_3215) || (indirect_regs[i].altaddr != 255)))
		{
			 dev_notice(&wc->vb.pdev->dev, "!!!!!!! %s  iREG %X = %X  should be %X\n",
				indirect_regs[i].name,indirect_regs[i].address,j,initial );
			 passed = 0;
		}	
	}

    if (passed) {
		if (debug & DEBUG_CARD)
			dev_info(&wc->vb.pdev->dev, "Init Indirect Registers completed successfully.\n");
    } else {
		dev_notice(&wc->vb.pdev->dev, " !!!!! Init Indirect Registers UNSUCCESSFULLY.\n");
		return -1;
    }
    return 0;
}

/* 1ms interrupt */
static inline void wctdm_proslic_check_oppending(struct wctdm *wc, int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;
	int res;

	/* Monitor the Pending LF state change, for the next 100ms */
	if (fxs->lasttxhook & SLIC_LF_OPPENDING) {
		spin_lock(&fxs->lasttxhooklock);

		if (!(fxs->lasttxhook & SLIC_LF_OPPENDING)) {
			spin_unlock(&fxs->lasttxhooklock);
			return;
		}

		res = wc->cmdq[card].isrshadow[1];
		if ((res & SLIC_LF_SETMASK) == (fxs->lasttxhook & SLIC_LF_SETMASK)) {
			fxs->lasttxhook &= SLIC_LF_SETMASK;
			fxs->oppending_ms = 0;
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev, "SLIC_LF OK: card=%d shadow=%02x lasttxhook=%02x intcount=%d \n", card, res, fxs->lasttxhook, wc->intcount);
			}
		} else if (fxs->oppending_ms) { /* if timing out */
			if (--fxs->oppending_ms == 0) {
				/* Timed out, resend the linestate */
				wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev, "SLIC_LF RETRY: card=%d shadow=%02x lasttxhook=%02x intcount=%d \n", card, res, fxs->lasttxhook, wc->intcount);
				}
			}
		} else { /* Start 100ms Timeout */
			fxs->oppending_ms = 100;
		}
		spin_unlock(&fxs->lasttxhooklock);
	}
}

/* 256ms interrupt */
static inline void wctdm_proslic_recheck_sanity(struct wctdm *wc, int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;
	int res;
	unsigned long flags;
#ifdef PAQ_DEBUG
	res = wc->cmdq[card].isrshadow[1];
	res &= ~0x3;
	if (res) {
		wc->cmdq[card].isrshadow[1]=0;
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->vb.pdev->dev, "Power alarm (%02x) on module %d, resetting!\n", res, card + 1);
			wc->sethook[card] = CMD_WR(19, res);
			/* Update shadow register to avoid extra power alarms until next read */
			wc->cmdq[card].isrshadow[1] = 0;
		} else {
			if (fxs->palarms == MAX_ALARMS)
				dev_notice(&wc->vb.pdev->dev, "Too many power alarms on card %d, NOT resetting!\n", card + 1);
		}
	}
#else
	spin_lock_irqsave(&fxs->lasttxhooklock, flags);
	res = wc->cmdq[card].isrshadow[1];

#if 0
	/* This makes sure the lasthook was put in reg 64 the linefeed reg */
	if (fxs->lasttxhook & SLIC_LF_OPPENDING) {
		if ((res & SLIC_LF_SETMASK) == (fxs->lasttxhook & SLIC_LF_SETMASK)) {
			fxs->lasttxhook &= SLIC_LF_SETMASK;
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev, "SLIC_LF OK: intcount=%d channel=%d shadow=%02x lasttxhook=%02x\n", wc->intcount, card, res, fxs->lasttxhook);
			}
		} else if (!(wc->intcount & 0x03)) {
			wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev, "SLIC_LF RETRY: intcount=%d channel=%d shadow=%02x lasttxhook=%02x\n", wc->intcount, card, res, fxs->lasttxhook);
			}
		}
	}
	if (debug & DEBUG_CARD) {
		if (!(wc->intcount % 100)) {
			dev_info(&wc->vb.pdev->dev, "SLIC_LF DEBUG: intcount=%d channel=%d shadow=%02x lasttxhook=%02x\n", wc->intcount, card, res, fxs->lasttxhook);
		}
	}
#endif

	res = !res &&    /* reg 64 has to be zero at last isr read */
		!(fxs->lasttxhook & SLIC_LF_OPPENDING) && /* not a transition */
		fxs->lasttxhook; /* not an intended zero */
	spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
	
	if (res) {
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->vb.pdev->dev, "Power alarm on module %d, resetting!\n", card + 1);
			spin_lock_irqsave(&fxs->lasttxhooklock, flags);
			if (fxs->lasttxhook == SLIC_LF_RINGING) {
				fxs->lasttxhook = POLARITY_XOR(card) ?
							SLIC_LF_ACTIVE_REV :
							SLIC_LF_ACTIVE_FWD;;
			}
			fxs->lasttxhook |= SLIC_LF_OPPENDING;
			wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
			spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);

			/* Update shadow register to avoid extra power alarms until next read */
			wc->cmdq[card].isrshadow[1] = fxs->lasttxhook;
		} else {
			if (fxs->palarms == MAX_ALARMS)
				dev_notice(&wc->vb.pdev->dev, "Too many power alarms on card %d, NOT resetting!\n", card + 1);
		}
	}
#endif
}

static inline void wctdm_qrvdri_check_hook(struct wctdm *wc, int card)
{
	signed char b,b1;
	int qrvcard = card & 0xfc;

	
	if (wc->qrvdebtime[card] >= 2) wc->qrvdebtime[card]--;
	b = wc->cmdq[qrvcard].isrshadow[0];	/* Hook/Ring state */
	b &= 0xcc; /* use bits 3-4 and 6-7 only */

	if (wc->radmode[qrvcard] & RADMODE_IGNORECOR) b &= ~4;
	else if (!(wc->radmode[qrvcard] & RADMODE_INVERTCOR)) b ^= 4;
	if (wc->radmode[qrvcard + 1] | RADMODE_IGNORECOR) b &= ~0x40;
	else if (!(wc->radmode[qrvcard + 1] | RADMODE_INVERTCOR)) b ^= 0x40;

	if ((wc->radmode[qrvcard] & RADMODE_IGNORECT) || 
		(!(wc->radmode[qrvcard] & RADMODE_EXTTONE))) b &= ~8;
	else if (!(wc->radmode[qrvcard] & RADMODE_EXTINVERT)) b ^= 8;
	if ((wc->radmode[qrvcard + 1] & RADMODE_IGNORECT) || 
		(!(wc->radmode[qrvcard + 1] & RADMODE_EXTTONE))) b &= ~0x80;
	else if (!(wc->radmode[qrvcard + 1] & RADMODE_EXTINVERT)) b ^= 0x80;
	/* now b & MASK should be zero, if its active */
	/* check for change in chan 0 */
	if ((!(b & 0xc)) != wc->qrvhook[qrvcard + 2])
	{
		wc->qrvdebtime[qrvcard] = wc->debouncetime[qrvcard];
		wc->qrvhook[qrvcard + 2] = !(b & 0xc);
	} 
	/* if timed-out and ready */
	if (wc->qrvdebtime[qrvcard] == 1)
	{
		b1 = wc->qrvhook[qrvcard + 2];
if (debug) dev_info(&wc->vb.pdev->dev, "QRV channel %d rx state changed to %d\n",qrvcard,wc->qrvhook[qrvcard + 2]);
		dahdi_hooksig(wc->aspan->span.chans[qrvcard],
			(b1) ? DAHDI_RXSIG_OFFHOOK : DAHDI_RXSIG_ONHOOK);
		wc->qrvdebtime[card] = 0;
	}
	/* check for change in chan 1 */
	if ((!(b & 0xc0)) != wc->qrvhook[qrvcard + 3])
	{
		wc->qrvdebtime[qrvcard + 1] = QRV_DEBOUNCETIME;
		wc->qrvhook[qrvcard + 3] = !(b & 0xc0);
	}
	if (wc->qrvdebtime[qrvcard + 1] == 1)
	{
		b1 = wc->qrvhook[qrvcard + 3];
if (debug) dev_info(&wc->vb.pdev->dev, "QRV channel %d rx state changed to %d\n",qrvcard + 1,wc->qrvhook[qrvcard + 3]);
		dahdi_hooksig(wc->aspan->span.chans[qrvcard + 1],
			(b1) ? DAHDI_RXSIG_OFFHOOK : DAHDI_RXSIG_ONHOOK);
		wc->qrvdebtime[card] = 0;
	}
	return;
}

static inline void wctdm_voicedaa_check_hook(struct wctdm *wc, int card)
{
#define MS_PER_CHECK_HOOK 1

	unsigned char res;
	signed char b;
	unsigned int abs_voltage;
	struct fxo *fxo = &wc->mods[card].fxo;

	/* Try to track issues that plague slot one FXO's */
	b = wc->cmdq[card].isrshadow[0];	/* Hook/Ring state */
	b &= 0x9b;
	if (fxo->offhook) {
		if (b != 0x9)
			wctdm_setreg_intr(wc, card, 5, 0x9);
	} else {
		if (b != 0x8)
			wctdm_setreg_intr(wc, card, 5, 0x8);
	}
	if (!fxo->offhook) {
		if (fwringdetect || neonmwi_monitor) {
			/* Look for ring status bits (Ring Detect Signal Negative and
			* Ring Detect Signal Positive) to transition back and forth
			* some number of times to indicate that a ring is occurring.
			* Provide some number of samples to allow for the transitions
			* to occur before ginving up.
			* NOTE: neon mwi voltages will trigger one of these bits to go active
			* but not to have transitions between the two bits (i.e. no negative
			* to positive or positive to negative transversals )
			*/
			res =  wc->cmdq[card].isrshadow[0] & 0x60;
			if (0 == wc->mods[card].fxo.wasringing) {
				if (res) {
					/* Look for positive/negative crossings in ring status reg */
					fxo->wasringing = 2;
					fxo->ringdebounce = ringdebounce /16;
					fxo->lastrdtx = res;
					fxo->lastrdtx_count = 0;
				}
			} else if (2 == fxo->wasringing) {
				/* If ring detect signal has transversed */
				if (res && res != fxo->lastrdtx) {
					/* if there are at least 3 ring polarity transversals */
					if (++fxo->lastrdtx_count >= 2) {
						fxo->wasringing = 1;
						if (debug)
							dev_info(&wc->vb.pdev->dev, "FW RING on %d/%d!\n", wc->aspan->span.spanno, card + 1);
						dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_RING);
						fxo->ringdebounce = ringdebounce / 16;
					} else {
						fxo->lastrdtx = res;
						fxo->ringdebounce = ringdebounce / 16;
					}
					/* ring indicator (positve or negative) has not transitioned, check debounce count */
				} else if (--fxo->ringdebounce == 0) {
					fxo->wasringing = 0;
				}
			} else {  /* I am in ring state */
				if (res) { /* If any ringdetect bits are still active */
					fxo->ringdebounce = ringdebounce / 16;
				} else if (--fxo->ringdebounce == 0) {
					fxo->wasringing = 0;
					if (debug)
						dev_info(&wc->vb.pdev->dev, "FW NO RING on %d/%d!\n", wc->aspan->span.spanno, card + 1);
					dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_OFFHOOK);
				}
			}
		} else {
			res =  wc->cmdq[card].isrshadow[0];
			if ((res & 0x60) && (fxo->battery == BATTERY_PRESENT)) {
				fxo->ringdebounce += (DAHDI_CHUNKSIZE * 16);
				if (fxo->ringdebounce >= DAHDI_CHUNKSIZE * ringdebounce) {
					if (!fxo->wasringing) {
						fxo->wasringing = 1;
						dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_RING);
						if (debug)
							dev_info(&wc->vb.pdev->dev, "RING on %d/%d!\n", wc->aspan->span.spanno, card + 1);
					}
					fxo->ringdebounce = DAHDI_CHUNKSIZE * ringdebounce;
				}
			} else {
				fxo->ringdebounce -= DAHDI_CHUNKSIZE * 4;
				if (fxo->ringdebounce <= 0) {
					if (fxo->wasringing) {
						fxo->wasringing = 0;
						dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_OFFHOOK);
						if (debug)
							dev_info(&wc->vb.pdev->dev, "NO RING on %d/%d!\n", wc->aspan->span.spanno, card + 1);
					}
					fxo->ringdebounce = 0;
				}
					
			}
		}
	}

	b = wc->cmdq[card].isrshadow[1]; /* Voltage */
	abs_voltage = abs(b);

	if (fxovoltage) {
		if (!(wc->intcount % 100)) {
			dev_info(&wc->vb.pdev->dev, "Port %d: Voltage: %d  Debounce %d\n", card + 1, b, fxo->battdebounce);
		}
	}

	if (unlikely(DAHDI_RXSIG_INITIAL == wc->aspan->span.chans[card]->rxhooksig)) {
		/*
		 * dahdi-base will set DAHDI_RXSIG_INITIAL after a
		 * DAHDI_STARTUP or DAHDI_CHANCONFIG ioctl so that new events
		 * will be queued on the channel with the current received
		 * hook state.  Channels that use robbed-bit signalling always
		 * report the current received state via the dahdi_rbsbits
		 * call. Since we only call dahdi_hooksig when we've detected
		 * a change to report, let's forget our current state in order
		 * to force us to report it again via dahdi_hooksig.
		 *
		 */
		fxo->battery = BATTERY_UNKNOWN;
	}

	if (abs_voltage < battthresh) {
		/* possible existing states:
		   battery lost, no debounce timer
		   battery lost, debounce timer (going to battery present)
		   battery present or unknown, no debounce timer
		   battery present or unknown, debounce timer (going to battery lost)
		*/

		if (fxo->battery == BATTERY_LOST) {
			if (fxo->battdebounce) {
				/* we were going to BATTERY_PRESENT, but battery was lost again,
				   so clear the debounce timer */
				fxo->battdebounce = 0;
			}
		} else {
			if (fxo->battdebounce) {
				/* going to BATTERY_LOST, see if we are there yet */
				if (--fxo->battdebounce == 0) {
					fxo->battery = BATTERY_LOST;
					if (debug)
						dev_info(&wc->vb.pdev->dev, "NO BATTERY on %d/%d!\n", wc->aspan->span.spanno, card + 1);
#ifdef	JAPAN
					if (!wc->ohdebounce && wc->offhook) {
						dahdi_hooksig(wc->aspan->chans[card], DAHDI_RXSIG_ONHOOK);
						if (debug)
							dev_info(&wc->vb.pdev->dev, "Signalled On Hook\n");
#ifdef	ZERO_BATT_RING
						wc->onhook++;
#endif
					}
#else
					dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_ONHOOK);
					/* set the alarm timer, taking into account that part of its time
					   period has already passed while debouncing occurred */
					fxo->battalarm = (battalarm - battdebounce) / MS_PER_CHECK_HOOK;
#endif
				}
			} else {
				/* start the debounce timer to verify that battery has been lost */
				fxo->battdebounce = battdebounce / MS_PER_CHECK_HOOK;
			}
		}
	} else {
		/* possible existing states:
		   battery lost or unknown, no debounce timer
		   battery lost or unknown, debounce timer (going to battery present)
		   battery present, no debounce timer
		   battery present, debounce timer (going to battery lost)
		*/

		if (fxo->battery == BATTERY_PRESENT) {
			if (fxo->battdebounce) {
				/* we were going to BATTERY_LOST, but battery appeared again,
				   so clear the debounce timer */
				fxo->battdebounce = 0;
			}
		} else {
			if (fxo->battdebounce) {
				/* going to BATTERY_PRESENT, see if we are there yet */
				if (--fxo->battdebounce == 0) {
					fxo->battery = BATTERY_PRESENT;
					if (debug) {
						dev_info(&wc->vb.pdev->dev,
							 "BATTERY on %d/%d (%s)!\n",
							 wc->aspan->span.spanno,
							 card + 1,
							 (b < 0) ? "-" : "+");
					}
#ifdef	ZERO_BATT_RING
					if (wc->onhook) {
						wc->onhook = 0;
						dahdi_hooksig(wc->aspan->chans[card], DAHDI_RXSIG_OFFHOOK);
						if (debug)
							dev_info(&wc->vb.pdev->dev, "Signalled Off Hook\n");
					}
#else
					dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_OFFHOOK);
#endif
					/* set the alarm timer, taking into account that part of its time
					   period has already passed while debouncing occurred */
					fxo->battalarm = (battalarm - battdebounce) / MS_PER_CHECK_HOOK;
				}
			} else {
				/* start the debounce timer to verify that battery has appeared */
				fxo->battdebounce = battdebounce / MS_PER_CHECK_HOOK;
			}
		}

		if (fxo->lastpol >= 0) {
			if (b < 0) {
				fxo->lastpol = -1;
				fxo->polaritydebounce = POLARITY_DEBOUNCE / MS_PER_CHECK_HOOK;
			}
		} 
		if (fxo->lastpol <= 0) {
			if (b > 0) {
				fxo->lastpol = 1;
				fxo->polaritydebounce = POLARITY_DEBOUNCE / MS_PER_CHECK_HOOK;
			}
		}
	}

	if (fxo->battalarm) {
		if (--fxo->battalarm == 0) {
			/* the alarm timer has expired, so update the battery alarm state
			   for this channel */
			dahdi_alarm_channel(wc->aspan->span.chans[card], fxo->battery == BATTERY_LOST ? DAHDI_ALARM_RED : DAHDI_ALARM_NONE);
		}
	}

	if (fxo->polaritydebounce) {
	        fxo->polaritydebounce--;
		if (fxo->polaritydebounce < 1) {
		    if (fxo->lastpol != fxo->polarity) {
			if (debug & DEBUG_CARD)
				dev_info(&wc->vb.pdev->dev, "%lu Polarity reversed (%d -> %d)\n", jiffies, 
				       fxo->polarity, 
				       fxo->lastpol);
			if (fxo->polarity)
				dahdi_qevent_lock(wc->aspan->span.chans[card], DAHDI_EVENT_POLARITY);
			fxo->polarity = fxo->lastpol;
		    }
		}
	}
	/* Look for neon mwi pulse */
	if (neonmwi_monitor && !wc->mods[card].fxo.offhook) {
		/* Look for 4 consecutive voltage readings
		* where the voltage is over the neon limit but
		* does not vary greatly from the last reading
		*/
		if (fxo->battery == 1 &&
				  abs_voltage > neonmwi_level &&
				  (0 == fxo->neonmwi_last_voltage ||
				  (b >= fxo->neonmwi_last_voltage - neonmwi_envelope &&
				  b <= fxo->neonmwi_last_voltage + neonmwi_envelope ))) {
			fxo->neonmwi_last_voltage = b;
			if (NEONMWI_ON_DEBOUNCE == fxo->neonmwi_debounce) {
				fxo->neonmwi_offcounter = neonmwi_offlimit_cycles;
				if (0 == fxo->neonmwi_state) {
					dahdi_qevent_lock(wc->aspan->span.chans[card], DAHDI_EVENT_NEONMWI_ACTIVE);
					fxo->neonmwi_state = 1;
					if (debug)
						dev_info(&wc->vb.pdev->dev, "NEON MWI active for card %d\n", card+1);
				}
				fxo->neonmwi_debounce++;  /* terminate the processing */
			} else if (NEONMWI_ON_DEBOUNCE > fxo->neonmwi_debounce) {
				fxo->neonmwi_debounce++;
			} else { /* Insure the count gets reset */
				fxo->neonmwi_offcounter = neonmwi_offlimit_cycles;
			}
		} else {
			fxo->neonmwi_debounce = 0;
			fxo->neonmwi_last_voltage = 0;
		}
		/* If no neon mwi pulse for given period of time, indicte no neon mwi state */
		if (fxo->neonmwi_state && 0 < fxo->neonmwi_offcounter ) {
			fxo->neonmwi_offcounter--;
			if (0 == fxo->neonmwi_offcounter) {
				dahdi_qevent_lock(wc->aspan->span.chans[card], DAHDI_EVENT_NEONMWI_INACTIVE);
				fxo->neonmwi_state = 0;
				if (debug)
					dev_info(&wc->vb.pdev->dev, "NEON MWI cleared for card %d\n", card+1);
			}
		}
	}
#undef MS_PER_CHECK_HOOK
}

static void wctdm_fxs_hooksig(struct wctdm *wc, const int card, enum dahdi_txsig txsig)
{
	int x = 0;
	unsigned long flags;
	struct fxs *const fxs = &wc->mods[card].fxs;
	spin_lock_irqsave(&fxs->lasttxhooklock, flags);
	switch (txsig) {
	case DAHDI_TXSIG_ONHOOK:
		switch (wc->aspan->span.chans[card]->sig) {
		case DAHDI_SIG_EM:
		case DAHDI_SIG_FXOKS:
		case DAHDI_SIG_FXOLS:
			x = fxs->idletxhookstate;
			break;
		case DAHDI_SIG_FXOGS:
			x = (POLARITY_XOR(card)) ?
					SLIC_LF_RING_OPEN :
					SLIC_LF_TIP_OPEN;
			break;
		}
		break;
	case DAHDI_TXSIG_OFFHOOK:
		switch (wc->aspan->span.chans[card]->sig) {
		case DAHDI_SIG_EM:
			x = (POLARITY_XOR(card)) ?
					SLIC_LF_ACTIVE_FWD :
					SLIC_LF_ACTIVE_REV;
			break;
		default:
			x = fxs->idletxhookstate;
			break;
		}
		break;
	case DAHDI_TXSIG_START:
		x = SLIC_LF_RINGING;
		break;
	case DAHDI_TXSIG_KEWL:
		x = SLIC_LF_OPEN;
		break;
	default:
		spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
		dev_notice(&wc->vb.pdev->dev,
			"wctdm24xxp: Can't set tx state to %d\n", txsig);
		return;
	}

	if (x != fxs->lasttxhook) {
		fxs->lasttxhook = x | SLIC_LF_OPPENDING;
		wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
		spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);

		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev, "Setting FXS hook state "
				 "to %d (%02x) intcount=%d\n", txsig, x,
				 wc->intcount);
		}
	} else {
		spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
	}
}

static void wctdm_fxs_off_hook(struct wctdm *wc, const int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;

	if (debug & DEBUG_CARD)
		dev_info(&wc->vb.pdev->dev,
			"fxs_off_hook: Card %d Going off hook\n", card);
	switch (fxs->lasttxhook) {
	case SLIC_LF_RINGING:		/* Ringing */
	case SLIC_LF_OHTRAN_FWD:	/* Forward On Hook Transfer */
	case SLIC_LF_OHTRAN_REV:	/* Reverse On Hook Transfer */
		/* just detected OffHook, during Ringing or OnHookTransfer */
		fxs->idletxhookstate = POLARITY_XOR(card) ?
						SLIC_LF_ACTIVE_REV :
						SLIC_LF_ACTIVE_FWD;
		break;
	}
	wctdm_fxs_hooksig(wc, card, DAHDI_TXSIG_OFFHOOK);
	dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_OFFHOOK);

#ifdef DEBUG
	if (robust)
		wctdm_init_proslic(wc, card, 1, 0, 1);
#endif
	fxs->oldrxhook = 1;
}

static void wctdm_fxs_on_hook(struct wctdm *wc, const int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;
	if (debug & DEBUG_CARD)
		dev_info(&wc->vb.pdev->dev,
			"fxs_on_hook: Card %d Going on hook\n", card);
	wctdm_fxs_hooksig(wc, card, DAHDI_TXSIG_ONHOOK);
	dahdi_hooksig(wc->aspan->span.chans[card], DAHDI_RXSIG_ONHOOK);
	fxs->oldrxhook = 0;
}

static inline void wctdm_proslic_check_hook(struct wctdm *wc, const int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;
	char res;
	int hook;

	/* For some reason we have to debounce the
	   hook detector.  */

	res = wc->cmdq[card].isrshadow[0];	/* Hook state */
	hook = (res & 1);
	
	if (hook != fxs->lastrxhook) {
		/* Reset the debounce (must be multiple of 4ms) */
		fxs->debounce = 8 * (4 * 8);
#if 0
		dev_info(&wc->vb.pdev->dev, "Resetting debounce card %d hook %d, %d\n",
		       card, hook, fxs->debounce);
#endif
	} else {
		if (fxs->debounce > 0) {
			fxs->debounce -= 4 * DAHDI_CHUNKSIZE;
#if 0
			dev_info(&wc->vb.pdev->dev, "Sustaining hook %d, %d\n",
			       hook, fxs->debounce);
#endif
			if (!fxs->debounce) {
#if 0
				dev_info(&wc->vb.pdev->dev, "Counted down debounce, newhook: %d...\n", hook);
#endif
				fxs->debouncehook = hook;
			}

			if (!fxs->oldrxhook && fxs->debouncehook)
				wctdm_fxs_off_hook(wc, card);
			else if (fxs->oldrxhook && !fxs->debouncehook)
				wctdm_fxs_on_hook(wc, card);
		}
	}
	fxs->lastrxhook = hook;
}

static inline void wctdm_vpm_check(struct wctdm *wc, int x)
{
	if (wc->cmdq[x].isrshadow[0]) {
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "VPM: Detected dtmf ON channel %02x on chip %d!\n", wc->cmdq[x].isrshadow[0], x - NUM_MODULES);
		wc->sethook[x] = CMD_WR(0xb9, wc->cmdq[x].isrshadow[0]);
		wc->cmdq[x].isrshadow[0] = 0;
		/* Cancel most recent lookup, if there is one */
		wc->cmdq[x].cmds[USER_COMMANDS+0] = 0x00000000; 
	} else if (wc->cmdq[x].isrshadow[1]) {
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "VPM: Detected dtmf OFF channel %02x on chip %d!\n", wc->cmdq[x].isrshadow[1], x - NUM_MODULES);
		wc->sethook[x] = CMD_WR(0xbd, wc->cmdq[x].isrshadow[1]);
		wc->cmdq[x].isrshadow[1] = 0;
		/* Cancel most recent lookup, if there is one */
		wc->cmdq[x].cmds[USER_COMMANDS+1] = 0x00000000; 
	}
}

static int wctdm_echocan_create(struct dahdi_chan *chan,
				struct dahdi_echocanparams *ecp,
				struct dahdi_echocanparam *p,
				struct dahdi_echocan_state **ec)
{
	struct wctdm *wc = chan->pvt;
	struct wctdm_chan *wchan = container_of(chan, struct wctdm_chan, chan);
	const struct dahdi_echocan_ops *ops;
	const struct dahdi_echocan_features *features;

#ifdef VPM_SUPPORT
	if (!vpmsupport)
		return -ENODEV;
#endif
	if (!wc->vpm100 && !wc->vpmadt032)
		return -ENODEV;

	if (wc->vpmadt032) {
		ops = &vpm150m_ec_ops;
		features = &vpm150m_ec_features;
	} else {
		ops = &vpm100m_ec_ops;
		features = &vpm100m_ec_features;
	}

	if (wc->vpm100 && (ecp->param_count > 0)) {
		dev_warn(&wc->vb.pdev->dev, "%s echo canceller does not support parameters; failing request\n", ops->name);
		return -EINVAL;
	}

	*ec = &wchan->ec;
	(*ec)->ops = ops;
	(*ec)->features = *features;

	if (wc->vpm100) {
		int channel;
		int unit;

		channel = wchan->timeslot;
		unit = wchan->timeslot & 0x3;
		if (wc->vpm100 < 2)
			channel >>= 2;
	
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "echocan: Unit is %d, Channel is %d length %d\n", unit, channel, ecp->tap_length);

		wctdm_vpm_out(wc, unit, channel, 0x3e);
		return 0;
	} else if (wc->vpmadt032) {
		enum adt_companding comp;

		comp = (DAHDI_LAW_ALAW == chan->span->deflaw) ?
					ADT_COMP_ALAW : ADT_COMP_ULAW;

		return vpmadt032_echocan_create(wc->vpmadt032,
						wchan->timeslot, comp, ecp, p);
	} else {
		return -ENODEV;
	}
}

static void echocan_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct wctdm *wc = chan->pvt;
	struct wctdm_chan *wchan = container_of(chan, struct wctdm_chan, chan);

	memset(ec, 0, sizeof(*ec));
	if (wc->vpm100) {
		int channel;
		int unit;

		channel = wchan->timeslot;
		unit = wchan->timeslot & 0x3;
		if (wc->vpm100 < 2)
			channel >>= 2;

		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "echocan: Unit is %d, Channel is %d length 0\n",
			       unit, channel);
		wctdm_vpm_out(wc, unit, channel, 0x01);
	} else if (wc->vpmadt032) {
		vpmadt032_echocan_free(wc->vpmadt032, wchan->timeslot, ec);
	}
}

/* 1ms interrupt */
static void wctdm_isr_misc_fxs(struct wctdm *wc, int card)
{
	struct fxs *const fxs = &wc->mods[card].fxs;
	unsigned long flags;

	if (!(wc->intcount % 10000)) {
		/* Accept an alarm once per 10 seconds */
		if (fxs->palarms)
			fxs->palarms--;
	}
	wctdm_proslic_check_hook(wc, card);

	wctdm_proslic_check_oppending(wc, card);

	if (!(wc->intcount & 0xfc))	/* every 256ms */
		wctdm_proslic_recheck_sanity(wc, card);
	if (SLIC_LF_RINGING == fxs->lasttxhook) {
		/* RINGing, prepare for OHT */
		fxs->ohttimer = OHT_TIMER << 3;
		/* OHT mode when idle */
		fxs->idletxhookstate = POLARITY_XOR(card) ? SLIC_LF_OHTRAN_REV :
							    SLIC_LF_OHTRAN_FWD;
	} else if (fxs->ohttimer) {
		 /* check if still OnHook */
		if (!fxs->oldrxhook) {
			fxs->ohttimer -= DAHDI_CHUNKSIZE;
			if (fxs->ohttimer)
				return;

			/* Switch to active */
			fxs->idletxhookstate = POLARITY_XOR(card) ? SLIC_LF_ACTIVE_REV :
								    SLIC_LF_ACTIVE_FWD;
			spin_lock_irqsave(&fxs->lasttxhooklock, flags);
			if (SLIC_LF_OHTRAN_FWD == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING | SLIC_LF_ACTIVE_FWD;
				/* Data enqueued here */
				wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d OnHookTransfer "
						 "stop\n", card);
				}
			} else if (SLIC_LF_OHTRAN_REV == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING | SLIC_LF_ACTIVE_REV;
				/* Data enqueued here */
				wc->sethook[card] = CMD_WR(LINE_STATE, fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d OnHookTransfer "
						 "stop\n", card);
				}
			}
			spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
		} else {
			fxs->ohttimer = 0;
			/* Switch to active */
			fxs->idletxhookstate = POLARITY_XOR(card) ? SLIC_LF_ACTIVE_REV : SLIC_LF_ACTIVE_FWD;
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "Channel %d OnHookTransfer abort\n",
					 card);
			}
		}

	}
}

/* 1ms interrupt */
static inline void wctdm_isr_misc(struct wctdm *wc)
{
	int x;

	if (unlikely(!wc->initialized)) {
		return;
	}

	for (x = 0; x < wc->mods_per_board; x++) {
		if (wc->modmap & (1 << x)) {
			if (wc->modtype[x] == MOD_TYPE_FXS) {
				wctdm_isr_misc_fxs(wc, x);
			} else if (wc->modtype[x] == MOD_TYPE_FXO) {
				wctdm_voicedaa_check_hook(wc, x);
			} else if (wc->modtype[x] == MOD_TYPE_QRV) {
				wctdm_qrvdri_check_hook(wc, x);
			}
		}
	}
	if (wc->vpm100 > 0) {
		for (x = NUM_MODULES; x < NUM_MODULES+NUM_EC; x++)
			wctdm_vpm_check(wc, x);
	}
}

static void handle_receive(struct voicebus *vb, struct list_head *buffers)
{
	struct wctdm *wc = container_of(vb, struct wctdm, vb);
	struct vbb *vbb;
	list_for_each_entry(vbb, buffers, entry)
		wctdm_receiveprep(wc, vbb->data);
}

static void handle_transmit(struct voicebus *vb, struct list_head *buffers)
{
	struct wctdm *wc = container_of(vb, struct wctdm, vb);
	struct vbb *vbb;

	list_for_each_entry(vbb, buffers, entry) {
		memset(vbb->data, 0, sizeof(vbb->data));
		wctdm_transmitprep(wc, vbb->data);
		wctdm_isr_misc(wc);
		wc->intcount++;
	}
}

struct sframe_packet {
	struct list_head node;
	u8 sframe[SFRAME_SIZE];
};

/**
 * handle_hx8_bootmode_receive() - queue up the receive packet for later...
 *
 * This function is called from interrupt context and isn't optimal, but it's
 * not the main code path.
 */
static void handle_hx8_bootmode_receive(struct wctdm *wc, const void *vbb)
{
	struct sframe_packet *frame;

	frame = kzalloc(sizeof(*frame), GFP_ATOMIC);
	if (unlikely(!frame)) {
		WARN_ON(1);
		return;
	}

	memcpy(frame->sframe, vbb, sizeof(frame->sframe));
	spin_lock(&wc->frame_list_lock);
	list_add_tail(&frame->node, &wc->frame_list);
	spin_unlock(&wc->frame_list_lock);

	/* Wake up anyone waiting for a new packet. */
	wake_up(&wc->regq);
	return;
}

static void handle_hx8_receive(struct voicebus *vb, struct list_head *buffers)
{
	struct wctdm *wc = container_of(vb, struct wctdm, vb);
	struct vbb *vbb;
	list_for_each_entry(vbb, buffers, entry)
		handle_hx8_bootmode_receive(wc, vbb->data);
}

static void handle_hx8_transmit(struct voicebus *vb, struct list_head *buffers)
{
	struct vbb *vbb, *n;

	list_for_each_entry_safe(vbb, n, buffers, entry) {
		list_del(&vbb->entry);
		dma_pool_free(vb->pool, vbb, vbb->dma_addr);
	}
}

static int wctdm_voicedaa_insane(struct wctdm *wc, int card)
{
	int blah;
	blah = wctdm_getreg(wc, card, 2);
	if (blah != 0x3)
		return -2;
	blah = wctdm_getreg(wc, card, 11);
	if (debug & DEBUG_CARD)
		dev_info(&wc->vb.pdev->dev, "VoiceDAA System: %02x\n", blah & 0xf);
	return 0;
}

static int wctdm_proslic_insane(struct wctdm *wc, int card)
{
	int blah, reg1, insane_report;
	insane_report=0;

	blah = wctdm_getreg(wc, card, 0);
	if (blah != 0xff && (debug & DEBUG_CARD))
		dev_info(&wc->vb.pdev->dev, "ProSLIC on module %d, product %d, version %d\n", card, (blah & 0x30) >> 4, (blah & 0xf));

#if 0
	if ((blah & 0x30) >> 4) {
		dev_info(&wc->vb.pdev->dev, "ProSLIC on module %d is not a 3210.\n", card);
		return -1;
	}
#endif
	if (((blah & 0xf) == 0) || ((blah & 0xf) == 0xf)) {
		/* SLIC not loaded */
		return -1;
	}

	/* let's be really sure this is an FXS before we continue */
	reg1 = wctdm_getreg(wc, card, 1);
	if ((0x80 != (blah & 0xf0)) || ((0x88 != reg1) && (0x08 != reg1))) {
		if (debug & DEBUG_CARD)
			dev_info(&wc->vb.pdev->dev, "DEBUG: not FXS b/c reg0=%x or reg1 != 0x88 (%x).\n", blah, reg1);
		return -1;
	}

	if ((blah & 0xf) < 2) {
		dev_info(&wc->vb.pdev->dev, "ProSLIC 3210 version %d is too old\n", blah & 0xf);
		return -1;
	}
	if (wctdm_getreg(wc, card, 1) & 0x80)
		/* ProSLIC 3215, not a 3210 */
		wc->flags[card] |= FLAG_3215;

	blah = wctdm_getreg(wc, card, 8);
	if (blah != 0x2) {
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d insane (1) %d should be 2\n", card, blah);
		return -1;
	} else if ( insane_report)
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d Reg 8 Reads %d Expected is 0x2\n",card,blah);

	blah = wctdm_getreg(wc, card, 64);
	if (blah != 0x0) {
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d insane (2)\n", card);
		return -1;
	} else if ( insane_report)
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d Reg 64 Reads %d Expected is 0x0\n",card,blah);

	blah = wctdm_getreg(wc, card, 11);
	if (blah != 0x33) {
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d insane (3)\n", card);
		return -1;
	} else if ( insane_report)
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d Reg 11 Reads %d Expected is 0x33\n",card,blah);

	/* Just be sure it's setup right. */
	wctdm_setreg(wc, card, 30, 0);

	if (debug & DEBUG_CARD) 
		dev_info(&wc->vb.pdev->dev, "ProSLIC on module %d seems sane.\n", card);
	return 0;
}

static int wctdm_proslic_powerleak_test(struct wctdm *wc, int card)
{
	unsigned long origjiffies;
	unsigned char vbat;

	/* Turn off linefeed */
	wctdm_setreg(wc, card, LINE_STATE, 0);

	/* Power down */
	wctdm_setreg(wc, card, 14, 0x10);

	/* Wait for one second */
	origjiffies = jiffies;

	while ((vbat = wctdm_getreg(wc, card, 82)) > 0x6) {
		if ((jiffies - origjiffies) >= (HZ/2))
			break;;
	}

	if (vbat < 0x06) {
		dev_notice(&wc->vb.pdev->dev, "Excessive leakage detected on module %d: %d volts (%02x) after %d ms\n", card,
		       376 * vbat / 1000, vbat, (int)((jiffies - origjiffies) * 1000 / HZ));
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev, "Post-leakage voltage: %d volts\n", 376 * vbat / 1000);
	}
	return 0;
}

static int wctdm_powerup_proslic(struct wctdm *wc, int card, int fast)
{
	unsigned char vbat;
	unsigned long origjiffies;
	int lim;

	/* Set period of DC-DC converter to 1/64 khz */
	wctdm_setreg(wc, card, 92, 0xc0 /* was 0xff */);

	/* Wait for VBat to powerup */
	origjiffies = jiffies;

	/* Disable powerdown */
	wctdm_setreg(wc, card, 14, 0);

	/* If fast, don't bother checking anymore */
	if (fast)
		return 0;

	while ((vbat = wctdm_getreg(wc, card, 82)) < 0xc0) {
		/* Wait no more than 500ms */
		if ((jiffies - origjiffies) > HZ/2) {
			break;
		}
	}

	if (vbat < 0xc0) {
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d failed to powerup within %d ms (%d mV only)\n\n -- DID YOU REMEMBER TO PLUG IN THE HD POWER CABLE TO THE TDM CARD??\n",
		       card, (int)(((jiffies - origjiffies) * 1000 / HZ)),
			vbat * 375);
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev, "ProSLIC on module %d powered up to -%d volts (%02x) in %d ms\n",
		       card, vbat * 376 / 1000, vbat, (int)(((jiffies - origjiffies) * 1000 / HZ)));
	}

        /* Proslic max allowed loop current, reg 71 LOOP_I_LIMIT */
        /* If out of range, just set it to the default value     */
        lim = (loopcurrent - 20) / 3;
        if ( loopcurrent > 41 ) {
                lim = 0;
                if (debug & DEBUG_CARD)
                        dev_info(&wc->vb.pdev->dev, "Loop current out of range! Setting to default 20mA!\n");
        }
        else if (debug & DEBUG_CARD)
                        dev_info(&wc->vb.pdev->dev, "Loop current set to %dmA!\n",(lim*3)+20);
        wctdm_setreg(wc,card,LOOP_I_LIMIT,lim);

	/* Engage DC-DC converter */
	wctdm_setreg(wc, card, 93, 0x19 /* was 0x19 */);
	return 0;

}

static int wctdm_proslic_manual_calibrate(struct wctdm *wc, int card)
{
	unsigned long origjiffies;
	unsigned char i;

	wctdm_setreg(wc, card, 21, 0);//(0)  Disable all interupts in DR21
	wctdm_setreg(wc, card, 22, 0);//(0)Disable all interupts in DR21
	wctdm_setreg(wc, card, 23, 0);//(0)Disable all interupts in DR21
	wctdm_setreg(wc, card, 64, 0);//(0)

	wctdm_setreg(wc, card, 97, 0x18); //(0x18)Calibrations without the ADC and DAC offset and without common mode calibration.
	wctdm_setreg(wc, card, 96, 0x47); //(0x47)	Calibrate common mode and differential DAC mode DAC + ILIM

	origjiffies=jiffies;
	while (wctdm_getreg(wc, card, 96) != 0) {
		if ((jiffies-origjiffies) > 80)
			return -1;
	}
//Initialized DR 98 and 99 to get consistant results.
// 98 and 99 are the results registers and the search should have same intial conditions.

/*******************************The following is the manual gain mismatch calibration****************************/
/*******************************This is also available as a function *******************************************/
	msleep(10);
	wctdm_proslic_setreg_indirect(wc, card, 88,0);
	wctdm_proslic_setreg_indirect(wc,card,89,0);
	wctdm_proslic_setreg_indirect(wc,card,90,0);
	wctdm_proslic_setreg_indirect(wc,card,91,0);
	wctdm_proslic_setreg_indirect(wc,card,92,0);
	wctdm_proslic_setreg_indirect(wc,card,93,0);

	wctdm_setreg(wc, card, 98,0x10); // This is necessary if the calibration occurs other than at reset time
	wctdm_setreg(wc, card, 99,0x10);

	for ( i=0x1f; i>0; i--)
	{
		wctdm_setreg(wc, card, 98,i);
		msleep(40);
		if ((wctdm_getreg(wc, card, 88)) == 0)
			break;
	} // for

	for ( i=0x1f; i>0; i--)
	{
		wctdm_setreg(wc, card, 99,i);
		msleep(40);
		if ((wctdm_getreg(wc, card, 89)) == 0)
			break;
	}//for

/*******************************The preceding is the manual gain mismatch calibration****************************/
/**********************************The following is the longitudinal Balance Cal***********************************/
	wctdm_setreg(wc,card,64,1);
	msleep(100);

	wctdm_setreg(wc, card, 64, 0);
	wctdm_setreg(wc, card, 23, 0x4);  // enable interrupt for the balance Cal
	wctdm_setreg(wc, card, 97, 0x1); // this is a singular calibration bit for longitudinal calibration
	wctdm_setreg(wc, card, 96,0x40);

	wctdm_getreg(wc,card,96); /* Read Reg 96 just cause */

	wctdm_setreg(wc, card, 21, 0xFF);
	wctdm_setreg(wc, card, 22, 0xFF);
	wctdm_setreg(wc, card, 23, 0xFF);

	/**The preceding is the longitudinal Balance Cal***/
	return(0);

}

static int wctdm_proslic_calibrate(struct wctdm *wc, int card)
{
	unsigned long origjiffies;
	int x;
	/* Perform all calibrations */
	wctdm_setreg(wc, card, 97, 0x1f);
	
	/* Begin, no speedup */
	wctdm_setreg(wc, card, 96, 0x5f);

	/* Wait for it to finish */
	origjiffies = jiffies;
	while (wctdm_getreg(wc, card, 96)) {
		if (time_after(jiffies, (origjiffies + (2*HZ)))) {
			dev_notice(&wc->vb.pdev->dev, "Timeout waiting for calibration of module %d\n", card);
			return -1;
		}
	}
	
	if (debug & DEBUG_CARD) {
		/* Print calibration parameters */
		dev_info(&wc->vb.pdev->dev, "Calibration Vector Regs 98 - 107: \n");
		for (x=98;x<108;x++) {
			dev_info(&wc->vb.pdev->dev, "%d: %02x\n", x, wctdm_getreg(wc, card, x));
		}
	}
	return 0;
}

void wait_just_a_bit(int foo)
{
	long newjiffies;
	newjiffies = jiffies + foo;
	while (jiffies < newjiffies)
		;
}

/*********************************************************************
 * Set the hwgain on the analog modules
 *
 * card = the card position for this module (0-23)
 * gain = gain in dB x10 (e.g. -3.5dB  would be gain=-35)
 * tx = (0 for rx; 1 for tx)
 *
 *******************************************************************/
static int wctdm_set_hwgain(struct wctdm *wc, int card, __s32 gain, __u32 tx)
{
	if (!(wc->modtype[card] == MOD_TYPE_FXO)) {
		dev_notice(&wc->vb.pdev->dev, "Cannot adjust gain.  Unsupported module type!\n");
		return -1;
	}
	if (tx) {
		if (debug)
			dev_info(&wc->vb.pdev->dev, "setting FXO tx gain for card=%d to %d\n", card, gain);
		if (gain >=  -150 && gain <= 0) {
			wctdm_setreg(wc, card, 38, 16 + (gain/-10));
			wctdm_setreg(wc, card, 40, 16 + (-gain%10));
		} else if (gain <= 120 && gain > 0) {
			wctdm_setreg(wc, card, 38, gain/10);
			wctdm_setreg(wc, card, 40, (gain%10));
		} else {
			dev_notice(&wc->vb.pdev->dev, "FXO tx gain is out of range (%d)\n", gain);
			return -1;
		}
	} else { /* rx */
		if (debug)
			dev_info(&wc->vb.pdev->dev, "setting FXO rx gain for card=%d to %d\n", card, gain);
		if (gain >=  -150 && gain <= 0) {
			wctdm_setreg(wc, card, 39, 16+ (gain/-10));
			wctdm_setreg(wc, card, 41, 16 + (-gain%10));
		} else if (gain <= 120 && gain > 0) {
			wctdm_setreg(wc, card, 39, gain/10);
			wctdm_setreg(wc, card, 41, (gain%10));
		} else {
			dev_notice(&wc->vb.pdev->dev, "FXO rx gain is out of range (%d)\n", gain);
			return -1;
		}
	}

	return 0;
}

static int set_lasttxhook_interruptible(struct fxs *fxs, unsigned newval, int * psethook)
{
	int res = 0;
	unsigned long flags;
	int timeout = 0;

	do {
		spin_lock_irqsave(&fxs->lasttxhooklock, flags);
		if (SLIC_LF_OPPENDING & fxs->lasttxhook) {
			spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
			if (timeout++ > 100)
				return -1;
			msleep(1);
		} else {
			fxs->lasttxhook = (newval & SLIC_LF_SETMASK) | SLIC_LF_OPPENDING;
			*psethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
			spin_unlock_irqrestore(&fxs->lasttxhooklock, flags);
			break;
		}
	} while (1);

	return res;
}

/* Must be called from within an interruptible context */
static int set_vmwi(struct wctdm *wc, int chan_idx)
{
	int x;
	struct fxs *const fxs = &wc->mods[chan_idx].fxs;

	/* Presently only supports line reversal MWI */
	if ((fxs->vmwi_active_messages) &&
	    (fxs->vmwisetting.vmwi_type & DAHDI_VMWI_LREV))
		fxs->vmwi_linereverse = 1;
	else
		fxs->vmwi_linereverse = 0;

	/* Set line polarity for new VMWI state */
	if (POLARITY_XOR(chan_idx)) {
		fxs->idletxhookstate |= SLIC_LF_OPPENDING | SLIC_LF_REVMASK;
		/* Do not set while currently ringing or open */
		if (((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_RINGING)  &&
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)) {
			x = fxs->lasttxhook;
			x |= SLIC_LF_REVMASK;
			set_lasttxhook_interruptible(fxs, x, &wc->sethook[chan_idx]);
		}
	} else {
		fxs->idletxhookstate &= ~SLIC_LF_REVMASK;
		/* Do not set while currently ringing or open */
		if (((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_RINGING) &&
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)) {
			x = fxs->lasttxhook;
			x &= ~SLIC_LF_REVMASK;
			set_lasttxhook_interruptible(fxs, x, &wc->sethook[chan_idx]);
		}
	}
	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "Setting VMWI on channel %d, messages=%d, lrev=%d\n",
			 chan_idx, fxs->vmwi_active_messages,
			 fxs->vmwi_linereverse);
	}
	return 0;
}

static void wctdm_voicedaa_set_ts(struct wctdm *wc, int card, int ts)
{
	wctdm_setreg(wc, card, 34, (ts * 8) & 0xff);
	wctdm_setreg(wc, card, 35, (ts * 8) >> 8);
	wctdm_setreg(wc, card, 36, (ts * 8) & 0xff);
	wctdm_setreg(wc, card, 37, (ts * 8) >> 8);

	if (debug)
		dev_info(&wc->vb.pdev->dev, "voicedaa: card %d new timeslot: %d\n", card + 1, ts);
}

static int wctdm_init_voicedaa(struct wctdm *wc, int card, int fast, int manual, int sane)
{
	unsigned char reg16=0, reg26=0, reg30=0, reg31=0;
	unsigned long flags;
	long newjiffies;

	if ((wc->modtype[card & 0xfc] == MOD_TYPE_QRV) ||
	    (wc->modtype[card & 0xfc] == MOD_TYPE_BRI))
		return -2;

	spin_lock_irqsave(&wc->reglock, flags);
	wc->modtype[card] = MOD_TYPE_NONE;
	spin_unlock_irqrestore(&wc->reglock, flags);
	/* Wait just a bit */
	wait_just_a_bit(HZ/10);

	spin_lock_irqsave(&wc->reglock, flags);
	wc->modtype[card] = MOD_TYPE_FXO;
	spin_unlock_irqrestore(&wc->reglock, flags);
	wait_just_a_bit(HZ/10);

	if (!sane && wctdm_voicedaa_insane(wc, card))
		return -2;

	/* Software reset */
	wctdm_setreg(wc, card, 1, 0x80);

	/* Wait just a bit */
	wait_just_a_bit(HZ/10);

	/* Set On-hook speed, Ringer impedence, and ringer threshold */
	reg16 |= (fxo_modes[_opermode].ohs << 6);
	reg16 |= (fxo_modes[_opermode].rz << 1);
	reg16 |= (fxo_modes[_opermode].rt);
	wctdm_setreg(wc, card, 16, reg16);

	/* Enable ring detector full-wave rectifier mode */
	wctdm_setreg(wc, card, 18, 2);
	wctdm_setreg(wc, card, 24, 0);
	
	/* Set DC Termination:
	   Tip/Ring voltage adjust, minimum operational current, current limitation */
	reg26 |= (fxo_modes[_opermode].dcv << 6);
	reg26 |= (fxo_modes[_opermode].mini << 4);
	reg26 |= (fxo_modes[_opermode].ilim << 1);
	wctdm_setreg(wc, card, 26, reg26);

	/* Set AC Impedence */
	reg30 = (fxo_modes[_opermode].acim);
	wctdm_setreg(wc, card, 30, reg30);

	/* Misc. DAA parameters */
	reg31 = 0xa3;
	reg31 |= (fxo_modes[_opermode].ohs2 << 3);
	wctdm_setreg(wc, card, 31, reg31);

	wctdm_voicedaa_set_ts(wc, card, card);

	/* Enable ISO-Cap */
	wctdm_setreg(wc, card, 6, 0x00);

	/* Wait 1000ms for ISO-cap to come up */
	newjiffies = jiffies;
	newjiffies += 2 * HZ;
	while ((jiffies < newjiffies) && !(wctdm_getreg(wc, card, 11) & 0xf0))
		wait_just_a_bit(HZ/10);

	if (!(wctdm_getreg(wc, card, 11) & 0xf0)) {
		dev_notice(&wc->vb.pdev->dev, "VoiceDAA did not bring up ISO link properly!\n");
		return -1;
	}
	if (debug & DEBUG_CARD)
		dev_info(&wc->vb.pdev->dev, "ISO-Cap is now up, line side: %02x rev %02x\n", 
		       wctdm_getreg(wc, card, 11) >> 4,
		       (wctdm_getreg(wc, card, 13) >> 2) & 0xf);
	/* Enable on-hook line monitor */
	wctdm_setreg(wc, card, 5, 0x08);
	
	/* Take values for fxotxgain and fxorxgain and apply them to module */
	wctdm_set_hwgain(wc, card, fxotxgain, 1);
	wctdm_set_hwgain(wc, card, fxorxgain, 0);

#ifdef DEBUG
	if (digitalloopback) {
		dev_info(&wc->vb.pdev->dev,
			 "Turning on digital loopback for port %d.\n",
			 card + 1);
		wctdm_setreg(wc, card, 10, 0x01);
	}
#endif

	if (debug)
		dev_info(&wc->vb.pdev->dev, "DEBUG fxotxgain:%i.%i fxorxgain:%i.%i\n", (wctdm_getreg(wc, card, 38)/16) ? -(wctdm_getreg(wc, card, 38) - 16) : wctdm_getreg(wc, card, 38), (wctdm_getreg(wc, card, 40)/16) ? -(wctdm_getreg(wc, card, 40) - 16) : wctdm_getreg(wc, card, 40), (wctdm_getreg(wc, card, 39)/16) ? -(wctdm_getreg(wc, card, 39) - 16): wctdm_getreg(wc, card, 39), (wctdm_getreg(wc, card, 41)/16)?-(wctdm_getreg(wc, card, 41) - 16) : wctdm_getreg(wc, card, 41));
	
	return 0;
		
}

static void wctdm_proslic_set_ts(struct wctdm *wc, int card, int ts)
{
	wctdm_setreg(wc, card, 2, (ts * 8) & 0xff);		/* Tx Start count low byte  0 */
	wctdm_setreg(wc, card, 3, (ts * 8) >> 8);		/* Tx Start count high byte 0 */
	wctdm_setreg(wc, card, 4, (ts * 8) & 0xff);		/* Rx Start count low byte  0 */
	wctdm_setreg(wc, card, 5, (ts * 8) >> 8);		/* Rx Start count high byte 0 */

	if (debug)
		dev_info(&wc->vb.pdev->dev, "proslic: card %d new timeslot: %d\n", card + 1, ts);
}

static int wctdm_init_proslic(struct wctdm *wc, int card, int fast, int manual, int sane)
{

	unsigned short tmp[5];
	unsigned long flags;
	unsigned char r19,r9;
	int x;
	int fxsmode=0;

	if (wc->modtype[card & 0xfc] == MOD_TYPE_QRV) return -2;

	spin_lock_irqsave(&wc->reglock, flags);
	wc->modtype[card] = MOD_TYPE_FXS;
	spin_unlock_irqrestore(&wc->reglock, flags);

	wait_just_a_bit(HZ/10);

	/* Sanity check the ProSLIC */
	if (!sane && wctdm_proslic_insane(wc, card))
		return -2;

	/* Initialize VMWI settings */
	memset(&(wc->mods[card].fxs.vmwisetting), 0, sizeof(wc->mods[card].fxs.vmwisetting));
	wc->mods[card].fxs.vmwi_linereverse = 0;

	/* By default, don't send on hook */
	if (!reversepolarity != !wc->mods[card].fxs.reversepolarity) {
		wc->mods[card].fxs.idletxhookstate = SLIC_LF_ACTIVE_REV;
	} else {
		wc->mods[card].fxs.idletxhookstate = SLIC_LF_ACTIVE_FWD;
	}

	if (sane) {
		/* Make sure we turn off the DC->DC converter to prevent anything from blowing up */
		wctdm_setreg(wc, card, 14, 0x10);
	}

	if (wctdm_proslic_init_indirect_regs(wc, card)) {
		dev_info(&wc->vb.pdev->dev, "Indirect Registers failed to initialize on module %d.\n", card);
		return -1;
	}

	/* Clear scratch pad area */
	wctdm_proslic_setreg_indirect(wc, card, 97,0);

	/* Clear digital loopback */
	wctdm_setreg(wc, card, 8, 0);

	/* Revision C optimization */
	wctdm_setreg(wc, card, 108, 0xeb);

	/* Disable automatic VBat switching for safety to prevent
	   Q7 from accidently turning on and burning out. */
	wctdm_setreg(wc, card, 67, 0x07); /* If pulse dialing has trouble at high REN
					     loads change this to 0x17 */

	/* Turn off Q7 */
	wctdm_setreg(wc, card, 66, 1);

	/* Flush ProSLIC digital filters by setting to clear, while
	   saving old values */
	for (x=0;x<5;x++) {
		tmp[x] = wctdm_proslic_getreg_indirect(wc, card, x + 35);
		wctdm_proslic_setreg_indirect(wc, card, x + 35, 0x8000);
	}

	/* Power up the DC-DC converter */
	if (wctdm_powerup_proslic(wc, card, fast)) {
		dev_notice(&wc->vb.pdev->dev, "Unable to do INITIAL ProSLIC powerup on module %d\n", card);
		return -1;
	}

	if (!fast) {
		spin_lock_init(&wc->mods[card].fxs.lasttxhooklock);

		/* Check for power leaks */
		if (wctdm_proslic_powerleak_test(wc, card)) {
			dev_notice(&wc->vb.pdev->dev, "ProSLIC module %d failed leakage test.  Check for short circuit\n", card);
		}
		/* Power up again */
		if (wctdm_powerup_proslic(wc, card, fast)) {
			dev_notice(&wc->vb.pdev->dev, "Unable to do FINAL ProSLIC powerup on module %d\n", card);
			return -1;
		}
#ifndef NO_CALIBRATION
		/* Perform calibration */
		if (manual) {
			if (wctdm_proslic_manual_calibrate(wc, card)) {
				//dev_notice(&wc->vb.pdev->dev, "Proslic failed on Manual Calibration\n");
				if (wctdm_proslic_manual_calibrate(wc, card)) {
					dev_notice(&wc->vb.pdev->dev, "Proslic Failed on Second Attempt to Calibrate Manually. (Try -DNO_CALIBRATION in Makefile)\n");
					return -1;
				}
				dev_info(&wc->vb.pdev->dev, "Proslic Passed Manual Calibration on Second Attempt\n");
			}
		}
		else {
			if (wctdm_proslic_calibrate(wc, card))  {
				//dev_notice(&wc->vb.pdev->dev, "ProSlic died on Auto Calibration.\n");
				if (wctdm_proslic_calibrate(wc, card)) {
					dev_notice(&wc->vb.pdev->dev, "Proslic Failed on Second Attempt to Auto Calibrate\n");
					return -1;
				}
				dev_info(&wc->vb.pdev->dev, "Proslic Passed Auto Calibration on Second Attempt\n");
			}
		}
		/* Perform DC-DC calibration */
		wctdm_setreg(wc, card, 93, 0x99);
		r19 = wctdm_getreg(wc, card, 107);
		if ((r19 < 0x2) || (r19 > 0xd)) {
			dev_notice(&wc->vb.pdev->dev, "DC-DC cal has a surprising direct 107 of 0x%02x!\n", r19);
			wctdm_setreg(wc, card, 107, 0x8);
		}

		/* Save calibration vectors */
		for (x=0;x<NUM_CAL_REGS;x++)
			wc->mods[card].fxs.calregs.vals[x] = wctdm_getreg(wc, card, 96 + x);
#endif

	} else {
		/* Restore calibration registers */
		for (x=0;x<NUM_CAL_REGS;x++)
			wctdm_setreg(wc, card, 96 + x, wc->mods[card].fxs.calregs.vals[x]);
	}
	/* Calibration complete, restore original values */
	for (x=0;x<5;x++) {
		wctdm_proslic_setreg_indirect(wc, card, x + 35, tmp[x]);
	}

	if (wctdm_proslic_verify_indirect_regs(wc, card)) {
		dev_info(&wc->vb.pdev->dev, "Indirect Registers failed verification.\n");
		return -1;
	}


#if 0
    /* Disable Auto Power Alarm Detect and other "features" */
    wctdm_setreg(wc, card, 67, 0x0e);
    blah = wctdm_getreg(wc, card, 67);
#endif

#if 0
    if (wctdm_proslic_setreg_indirect(wc, card, 97, 0x0)) { // Stanley: for the bad recording fix
		 dev_info(&wc->vb.pdev->dev, "ProSlic IndirectReg Died.\n");
		 return -1;
	}
#endif

	/* U-Law 8-bit interface */
    wctdm_proslic_set_ts(wc, card, card);

    wctdm_setreg(wc, card, 18, 0xff);     // clear all interrupt
    wctdm_setreg(wc, card, 19, 0xff);
    wctdm_setreg(wc, card, 20, 0xff);
    wctdm_setreg(wc, card, 22, 0xff);
    wctdm_setreg(wc, card, 73, 0x04);
	if (fxshonormode) {
		static const int ACIM2TISS[16] = { 0x0, 0x1, 0x4, 0x5, 0x7,
						   0x0, 0x0, 0x6, 0x0, 0x0,
						   0x0, 0x2, 0x0, 0x3 };
		fxsmode = ACIM2TISS[fxo_modes[_opermode].acim];
		wctdm_setreg(wc, card, 10, 0x08 | fxsmode);
		if (fxo_modes[_opermode].ring_osc)
			wctdm_proslic_setreg_indirect(wc, card, 20, fxo_modes[_opermode].ring_osc);
		if (fxo_modes[_opermode].ring_x)
			wctdm_proslic_setreg_indirect(wc, card, 21, fxo_modes[_opermode].ring_x);
	}
    if (lowpower)
    	wctdm_setreg(wc, card, 72, 0x10);

#if 0
    wctdm_setreg(wc, card, 21, 0x00); 	// enable interrupt
    wctdm_setreg(wc, card, 22, 0x02); 	// Loop detection interrupt
    wctdm_setreg(wc, card, 23, 0x01); 	// DTMF detection interrupt
#endif

#if 0
    /* Enable loopback */
    wctdm_setreg(wc, card, 8, 0x2);
    wctdm_setreg(wc, card, 14, 0x0);
    wctdm_setreg(wc, card, 64, 0x0);
    wctdm_setreg(wc, card, 1, 0x08);
#endif

	if (fastringer) {
		/* Speed up Ringer */
		wctdm_proslic_setreg_indirect(wc, card, 20, 0x7e6d);
		wctdm_proslic_setreg_indirect(wc, card, 21, 0x01b9);
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wctdm_setreg(wc, card, 74, 0x3f);
			if (wctdm_proslic_setreg_indirect(wc, card, 21, 0x247)) 
				return -1;
			dev_info(&wc->vb.pdev->dev, "Boosting fast ringer on slot %d (89V peak)\n", card + 1);
		} else if (lowpower) {
			if (wctdm_proslic_setreg_indirect(wc, card, 21, 0x14b)) 
				return -1;
			dev_info(&wc->vb.pdev->dev, "Reducing fast ring power on slot %d (50V peak)\n", card + 1);
		} else
			dev_info(&wc->vb.pdev->dev, "Speeding up ringer on slot %d (25Hz)\n", card + 1);
	} else {
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wctdm_setreg(wc, card, 74, 0x3f);
			if (wctdm_proslic_setreg_indirect(wc, card, 21, 0x1d1)) 
				return -1;
			dev_info(&wc->vb.pdev->dev, "Boosting ringer on slot %d (89V peak)\n", card + 1);
		} else if (lowpower) {
			if (wctdm_proslic_setreg_indirect(wc, card, 21, 0x108)) 
				return -1;
			dev_info(&wc->vb.pdev->dev, "Reducing ring power on slot %d (50V peak)\n", card + 1);
		}
	}

	if (fxstxgain || fxsrxgain) {
		r9 = wctdm_getreg(wc, card, 9);
		switch (fxstxgain) {
		
			case 35:
				r9+=8;
				break;
			case -35:
				r9+=4;
				break;
			case 0: 
				break;
		}
	
		switch (fxsrxgain) {
			
			case 35:
				r9+=2;
				break;
			case -35:
				r9+=1;
				break;
			case 0:
				break;
		}
		wctdm_setreg(wc, card, 9, r9);
	}

	if (debug)
		dev_info(&wc->vb.pdev->dev, "DEBUG: fxstxgain:%s fxsrxgain:%s\n",((wctdm_getreg(wc, card, 9)/8) == 1)?"3.5":(((wctdm_getreg(wc,card,9)/4) == 1)?"-3.5":"0.0"),((wctdm_getreg(wc, card, 9)/2) == 1)?"3.5":((wctdm_getreg(wc,card,9)%2)?"-3.5":"0.0"));

	wc->mods[card].fxs.lasttxhook = wc->mods[card].fxs.idletxhookstate;
	wctdm_setreg(wc, card, LINE_STATE, wc->mods[card].fxs.lasttxhook);
	return 0;
}

static void wctdm_qrvdri_set_ts(struct wctdm *wc, int card, int ts)
{
	wctdm_setreg(wc, card, 0x13, ts + 0x80);	/* codec 2 tx, ts0 */
	wctdm_setreg(wc, card, 0x17, ts + 0x80);	/* codec 0 rx, ts0 */
	wctdm_setreg(wc, card, 0x14, ts + 0x81);	/* codec 1 tx, ts1 */
	wctdm_setreg(wc, card, 0x18, ts + 0x81);	/* codec 1 rx, ts1 */

	if (debug)
		dev_info(&wc->vb.pdev->dev, "qrvdri: card %d new timeslot: %d\n", card + 1, ts);
}

static int wctdm_init_qrvdri(struct wctdm *wc, int card)
{
	unsigned char x,y;

	if (MOD_TYPE_BRI == wc->modtype[card & 0xfc])
		return -2;

	/* have to set this, at least for now */
	wc->modtype[card] = MOD_TYPE_QRV;
	if (!(card & 3)) /* if at base of card, reset and write it */
	{
		wctdm_setreg(wc,card,0,0x80); 
		wctdm_setreg(wc,card,0,0x55);
		wctdm_setreg(wc,card,1,0x69);
		wc->qrvhook[card] = wc->qrvhook[card + 1] = 0;
		wc->qrvhook[card + 2] = wc->qrvhook[card + 3] = 0xff;
		wc->debouncetime[card] = wc->debouncetime[card + 1] = QRV_DEBOUNCETIME;
		wc->qrvdebtime[card] = wc->qrvdebtime[card + 1] = 0;
		wc->radmode[card] = wc->radmode[card + 1] = 0;
		wc->txgain[card] = wc->txgain[card + 1] = 3599;
		wc->rxgain[card] = wc->rxgain[card + 1] = 1199;
	} else { /* channel is on same card as base, no need to test */
		if (wc->modtype[card & 0x7c] == MOD_TYPE_QRV) 
		{
			/* only lower 2 are valid */
			if (!(card & 2)) return 0;
		}
		wc->modtype[card] = MOD_TYPE_NONE;
		return 1;
	}
	x = wctdm_getreg(wc,card,0);
	y = wctdm_getreg(wc,card,1);
	/* if not a QRV card, return as such */
	if ((x != 0x55) || (y != 0x69))
	{
		wc->modtype[card] = MOD_TYPE_NONE;
		return 1;
	}
	for (x = 0; x < 0x30; x++)
	{
		if ((x >= 0x1c) && (x <= 0x1e)) wctdm_setreg(wc,card,x,0xff);
		else wctdm_setreg(wc,card,x,0);
	}
	wctdm_setreg(wc,card,0,0x80); 
	msleep(100);
	wctdm_setreg(wc,card,0,0x10); 
	wctdm_setreg(wc,card,0,0x10); 
	msleep(100);
	/* set up modes */
	wctdm_setreg(wc,card,0,0x1c); 
	/* set up I/O directions */
	wctdm_setreg(wc,card,1,0x33); 
	wctdm_setreg(wc,card,2,0x0f); 
	wctdm_setreg(wc,card,5,0x0f); 
	/* set up I/O to quiescent state */
	wctdm_setreg(wc,card,3,0x11);  /* D0-7 */
	wctdm_setreg(wc,card,4,0xa);  /* D8-11 */
	wctdm_setreg(wc,card,7,0);  /* CS outputs */

	wctdm_qrvdri_set_ts(wc, card, card);

	/* set up for max gains */
	wctdm_setreg(wc,card,0x26,0x24); 
	wctdm_setreg(wc,card,0x27,0x24); 
	wctdm_setreg(wc,card,0x0b,0x01);  /* "Transmit" gain codec 0 */
	wctdm_setreg(wc,card,0x0c,0x01);  /* "Transmit" gain codec 1 */
	wctdm_setreg(wc,card,0x0f,0xff);  /* "Receive" gain codec 0 */
	wctdm_setreg(wc,card,0x10,0xff);  /* "Receive" gain codec 1 */
	return 0;
}

static void qrv_dosetup(struct dahdi_chan *chan,struct wctdm *wc)
{
	int qrvcard;
	unsigned char r;
	long l;

	/* actually do something with the values */
	qrvcard = (chan->chanpos - 1) & 0xfc;
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ radmodes: %d,%d  rxgains: %d,%d   txgains: %d,%d\n",
	wc->radmode[qrvcard],wc->radmode[qrvcard + 1],
		wc->rxgain[qrvcard],wc->rxgain[qrvcard + 1],
			wc->txgain[qrvcard],wc->txgain[qrvcard + 1]);
	r = 0;
	if (wc->radmode[qrvcard] & RADMODE_DEEMP) r |= 4;		
	if (wc->radmode[qrvcard + 1] & RADMODE_DEEMP) r |= 8;		
	if (wc->rxgain[qrvcard] < 1200) r |= 1;
	if (wc->rxgain[qrvcard + 1] < 1200) r |= 2;
	wctdm_setreg(wc, qrvcard, 7, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 7 to %02x hex\n",r);
	r = 0;
	if (wc->radmode[qrvcard] & RADMODE_PREEMP) r |= 3;
	else if (wc->txgain[qrvcard] >= 3600) r |= 1;
	else if (wc->txgain[qrvcard] >= 1200) r |= 2;
	if (wc->radmode[qrvcard + 1] & RADMODE_PREEMP) r |= 0xc;
	else if (wc->txgain[qrvcard + 1] >= 3600) r |= 4;
	else if (wc->txgain[qrvcard + 1] >= 1200) r |= 8;
	wctdm_setreg(wc, qrvcard, 4, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 4 to %02x hex\n",r);
	r = 0;
	if (wc->rxgain[qrvcard] >= 2400) r |= 1; 
	if (wc->rxgain[qrvcard + 1] >= 2400) r |= 2; 
	wctdm_setreg(wc, qrvcard, 0x25, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x25 to %02x hex\n",r);
	r = 0;
	if (wc->txgain[qrvcard] < 2400) r |= 1; else r |= 4;
	if (wc->txgain[qrvcard + 1] < 2400) r |= 8; else r |= 0x20;
	wctdm_setreg(wc, qrvcard, 0x26, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x26 to %02x hex\n",r);
	l = ((long)(wc->rxgain[qrvcard] % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	if (wc->rxgain[qrvcard] >= 2400) l += 181;
	wctdm_setreg(wc, qrvcard, 0x0b, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0b to %02x hex\n",(unsigned char)l);
	l = ((long)(wc->rxgain[qrvcard + 1] % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	if (wc->rxgain[qrvcard + 1] >= 2400) l += 181;
	wctdm_setreg(wc, qrvcard, 0x0c, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0c to %02x hex\n",(unsigned char)l);
	l = ((long)(wc->txgain[qrvcard] % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	wctdm_setreg(wc, qrvcard, 0x0f, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0f to %02x hex\n", (unsigned char)l);
	l = ((long)(wc->txgain[qrvcard + 1] % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	wctdm_setreg(wc, qrvcard, 0x10,(unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x10 to %02x hex\n",(unsigned char)l);
	return;
}


static int wctdm_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct wctdm_stats stats;
	struct wctdm_regs regs;
	struct wctdm_regop regop;
	struct wctdm_echo_coefs echoregs;
	struct dahdi_hwgain hwgain;
	struct wctdm *wc = chan->pvt;
	int x;
	union {
		struct dahdi_radio_stat s;
		struct dahdi_radio_param p;
	} stack;
	struct fxs *const fxs = &wc->mods[chan->chanpos - 1].fxs;

	switch (cmd) {
	case DAHDI_ONHOOKTRANSFER:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
			return -EINVAL;
		if (get_user(x, (__user int *) data))
			return -EFAULT;
		fxs->ohttimer = x << 3;

		/* Active mode when idle */
		fxs->idletxhookstate = POLARITY_XOR(chan->chanpos - 1) ?
						SLIC_LF_ACTIVE_REV :
						SLIC_LF_ACTIVE_FWD;

		if (((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_ACTIVE_FWD) ||
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_ACTIVE_REV)) {

			x = set_lasttxhook_interruptible(fxs,
				(POLARITY_XOR(chan->chanpos - 1) ?
				SLIC_LF_OHTRAN_REV : SLIC_LF_OHTRAN_FWD),
				&wc->sethook[chan->chanpos - 1]);

			if (debug & DEBUG_CARD) {
				if (x) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d TIMEOUT: "
						 "OnHookTransfer start\n",
						 chan->chanpos - 1);
				} else {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d OnHookTransfer "
						 "start\n", chan->chanpos - 1);
				}
			}

		}
		break;
	case DAHDI_VMWI_CONFIG:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
			return -EINVAL;
		if (copy_from_user(&(fxs->vmwisetting),
				   (__user void *)data,
				   sizeof(fxs->vmwisetting)))
			return -EFAULT;
		set_vmwi(wc, chan->chanpos - 1);
		break;
	case DAHDI_VMWI:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
			return -EINVAL;
		if (get_user(x, (__user int *) data))
			return -EFAULT;
		if (0 > x)
			return -EFAULT;
		fxs->vmwi_active_messages = x;
		set_vmwi(wc, chan->chanpos - 1);
		break;
	case WCTDM_GET_STATS:
		if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXS) {
			stats.tipvolt = wctdm_getreg(wc, chan->chanpos - 1, 80) * -376;
			stats.ringvolt = wctdm_getreg(wc, chan->chanpos - 1, 81) * -376;
			stats.batvolt = wctdm_getreg(wc, chan->chanpos - 1, 82) * -376;
		} else if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXO) {
			stats.tipvolt = (signed char)wctdm_getreg(wc, chan->chanpos - 1, 29) * 1000;
			stats.ringvolt = (signed char)wctdm_getreg(wc, chan->chanpos - 1, 29) * 1000;
			stats.batvolt = (signed char)wctdm_getreg(wc, chan->chanpos - 1, 29) * 1000;
		} else 
			return -EINVAL;
		if (copy_to_user((__user void *) data, &stats, sizeof(stats)))
			return -EFAULT;
		break;
	case WCTDM_GET_REGS:
		if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXS) {
			for (x=0;x<NUM_INDIRECT_REGS;x++)
				regs.indirect[x] = wctdm_proslic_getreg_indirect(wc, chan->chanpos -1, x);
			for (x=0;x<NUM_REGS;x++)
				regs.direct[x] = wctdm_getreg(wc, chan->chanpos - 1, x);
		} else if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_QRV) {
			memset(&regs, 0, sizeof(regs));
			for (x=0;x<0x32;x++)
				regs.direct[x] = wctdm_getreg(wc, chan->chanpos - 1, x);
		} else {
			memset(&regs, 0, sizeof(regs));
			for (x=0;x<NUM_FXO_REGS;x++)
				regs.direct[x] = wctdm_getreg(wc, chan->chanpos - 1, x);
		}
		if (copy_to_user((__user void *)data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
	case WCTDM_SET_REG:
		if (copy_from_user(&regop, (__user void *) data, sizeof(regop)))
			return -EFAULT;
		if (regop.indirect) {
			if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
				return -EINVAL;
			dev_info(&wc->vb.pdev->dev, "Setting indirect %d to 0x%04x on %d\n", regop.reg, regop.val, chan->chanpos);
			wctdm_proslic_setreg_indirect(wc, chan->chanpos - 1, regop.reg, regop.val);
		} else {
			regop.val &= 0xff;
			if (regop.reg == LINE_STATE) {
				/* Set feedback register to indicate the new state that is being set */
				fxs->lasttxhook = (regop.val & 0x0f) |  SLIC_LF_OPPENDING;
			}
			dev_info(&wc->vb.pdev->dev, "Setting direct %d to %04x on %d\n", regop.reg, regop.val, chan->chanpos);
			wctdm_setreg(wc, chan->chanpos - 1, regop.reg, regop.val);
		}
		break;
	case WCTDM_SET_ECHOTUNE:
		dev_info(&wc->vb.pdev->dev, "-- Setting echo registers: \n");
		if (copy_from_user(&echoregs, (__user void *) data, sizeof(echoregs)))
			return -EFAULT;

		if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXO) {
			/* Set the ACIM register */
			wctdm_setreg(wc, chan->chanpos - 1, 30, echoregs.acim);

			/* Set the digital echo canceller registers */
			wctdm_setreg(wc, chan->chanpos - 1, 45, echoregs.coef1);
			wctdm_setreg(wc, chan->chanpos - 1, 46, echoregs.coef2);
			wctdm_setreg(wc, chan->chanpos - 1, 47, echoregs.coef3);
			wctdm_setreg(wc, chan->chanpos - 1, 48, echoregs.coef4);
			wctdm_setreg(wc, chan->chanpos - 1, 49, echoregs.coef5);
			wctdm_setreg(wc, chan->chanpos - 1, 50, echoregs.coef6);
			wctdm_setreg(wc, chan->chanpos - 1, 51, echoregs.coef7);
			wctdm_setreg(wc, chan->chanpos - 1, 52, echoregs.coef8);

			dev_info(&wc->vb.pdev->dev, "-- Set echo registers successfully\n");

			break;
		} else {
			return -EINVAL;

		}
		break;
	case DAHDI_SET_HWGAIN:
		if (copy_from_user(&hwgain, (__user void *) data, sizeof(hwgain)))
			return -EFAULT;

		wctdm_set_hwgain(wc, chan->chanpos-1, hwgain.newgain, hwgain.tx);

		if (debug)
			dev_info(&wc->vb.pdev->dev, "Setting hwgain on channel %d to %d for %s direction\n", 
				chan->chanpos-1, hwgain.newgain, hwgain.tx ? "tx" : "rx");
		break;
#ifdef VPM_SUPPORT
	case DAHDI_TONEDETECT:
		/* Hardware DTMF detection is not supported. */
		return -ENOSYS;
#endif
	case DAHDI_SETPOLARITY:
		if (get_user(x, (__user int *) data))
			return -EFAULT;
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
			return -EINVAL;
		/* Can't change polarity while ringing or when open */
		if (((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_RINGING) ||
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_OPEN)) {
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "Channel %d Unable to Set Polarity\n", chan->chanpos - 1);
			}
			return -EINVAL;
		}

		fxs->reversepolarity = (x) ? 1 : 0;

		if (POLARITY_XOR(chan->chanpos - 1)) {
			fxs->idletxhookstate |= SLIC_LF_REVMASK;
			x = fxs->lasttxhook & SLIC_LF_SETMASK;
			x |= SLIC_LF_REVMASK;
			if (x != fxs->lasttxhook) { 
				x = set_lasttxhook_interruptible(fxs, x, &wc->sethook[chan->chanpos - 1]);
				if ((debug & DEBUG_CARD) && x) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d TIMEOUT: Set Reverse "
						 "Polarity\n", chan->chanpos - 1);
				} else if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d Set Reverse Polarity\n",
						 chan->chanpos - 1);
				}
			}
		} else {
			fxs->idletxhookstate &= ~SLIC_LF_REVMASK;
			x = fxs->lasttxhook & SLIC_LF_SETMASK;
			x &= ~SLIC_LF_REVMASK;
			if (x != fxs->lasttxhook) { 
				x = set_lasttxhook_interruptible(fxs, x, &wc->sethook[chan->chanpos - 1]);
				if ((debug & DEBUG_CARD) & x) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d TIMEOUT: Set Normal "
						 "Polarity\n", chan->chanpos - 1);
				} else if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d Set Normal Polarity\n",
						 chan->chanpos - 1);
				}
			}
		}
		break;
	case DAHDI_RADIO_GETPARAM:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_QRV) 
			return -ENOTTY;
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p)))
			return -EFAULT;
		stack.p.data = 0; /* start with 0 value in output */
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (wc->radmode[chan->chanpos - 1] & RADMODE_INVERTCOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECOR:
			if (wc->radmode[chan->chanpos - 1] & RADMODE_IGNORECOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECT:
			if (wc->radmode[chan->chanpos - 1] & RADMODE_IGNORECT)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_EXTRXTONE:
			stack.p.data = 0;
			if (wc->radmode[chan->chanpos - 1] & RADMODE_EXTTONE)
			{
				stack.p.data = 1;
				if (wc->radmode[chan->chanpos - 1] & RADMODE_EXTINVERT)
				{
					stack.p.data = 2;
				}
			}
			break;
		case DAHDI_RADPAR_DEBOUNCETIME:
			stack.p.data = wc->debouncetime[chan->chanpos - 1];
			break;
		case DAHDI_RADPAR_RXGAIN:
			stack.p.data = wc->rxgain[chan->chanpos - 1] - 1199;
			break;
		case DAHDI_RADPAR_TXGAIN:
			stack.p.data = wc->txgain[chan->chanpos - 1] - 3599;
			break;
		case DAHDI_RADPAR_DEEMP:
			stack.p.data = 0;
			if (wc->radmode[chan->chanpos - 1] & RADMODE_DEEMP)
			{
				stack.p.data = 1;
			}
			break;
		case DAHDI_RADPAR_PREEMP:
			stack.p.data = 0;
			if (wc->radmode[chan->chanpos - 1] & RADMODE_PREEMP)
			{
				stack.p.data = 1;
			}
			break;
		default:
			return -EINVAL;
		}
		if (copy_to_user((__user void *) data, &stack.p, sizeof(stack.p)))
		    return -EFAULT;
		break;
	case DAHDI_RADIO_SETPARAM:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_QRV) 
			return -ENOTTY;
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p)))
			return -EFAULT;
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_INVERTCOR;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_INVERTCOR;
			return 0;
		case DAHDI_RADPAR_IGNORECOR:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_IGNORECOR;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_IGNORECOR;
			return 0;
		case DAHDI_RADPAR_IGNORECT:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_IGNORECT;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_IGNORECT;
			return 0;
		case DAHDI_RADPAR_EXTRXTONE:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_EXTTONE;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_EXTTONE;
			if (stack.p.data > 1)
				wc->radmode[chan->chanpos - 1] |= RADMODE_EXTINVERT;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_EXTINVERT;
			return 0;
		case DAHDI_RADPAR_DEBOUNCETIME:
			wc->debouncetime[chan->chanpos - 1] = stack.p.data;
			return 0;
		case DAHDI_RADPAR_RXGAIN:
			/* if out of range */
			if ((stack.p.data <= -1200) || (stack.p.data > 1552))
			{
				return -EINVAL;
			}
			wc->rxgain[chan->chanpos - 1] = stack.p.data + 1199;
			break;
		case DAHDI_RADPAR_TXGAIN:
			/* if out of range */
			if (wc->radmode[chan->chanpos -1] & RADMODE_PREEMP)
			{
				if ((stack.p.data <= -2400) || (stack.p.data > 0))
				{
					return -EINVAL;
				}
			}
			else
			{
				if ((stack.p.data <= -3600) || (stack.p.data > 1200))
				{
					return -EINVAL;
				}
			}
			wc->txgain[chan->chanpos - 1] = stack.p.data + 3599;
			break;
		case DAHDI_RADPAR_DEEMP:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_DEEMP;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_DEEMP;
			wc->rxgain[chan->chanpos - 1] = 1199;
			break;
		case DAHDI_RADPAR_PREEMP:
			if (stack.p.data)
				wc->radmode[chan->chanpos - 1] |= RADMODE_PREEMP;
			else
				wc->radmode[chan->chanpos - 1] &= ~RADMODE_PREEMP;
			wc->txgain[chan->chanpos - 1] = 3599;
			break;
		default:
			return -EINVAL;
		}
		qrv_dosetup(chan,wc);
		return 0;				
	default:
		return -ENOTTY;
	}
	return 0;
}

static int wctdm_open(struct dahdi_chan *chan)
{
	struct wctdm *wc;
	int channo;
	unsigned long flags;

	wc = chan->pvt;
	channo = chan->chanpos - 1;

#if 0
	if (!(wc->modmap & (1 << (chan->chanpos - 1))))
		return -ENODEV;
	if (wc->dead)
		return -ENODEV;
#endif
	wc->usecount++;
	
	if (wc->modtype[channo] == MOD_TYPE_FXO) {
		/* Reset the mwi indicators */
		spin_lock_irqsave(&wc->reglock, flags);
		wc->mods[channo].fxo.neonmwi_debounce = 0;
		wc->mods[channo].fxo.neonmwi_offcounter = 0;
		wc->mods[channo].fxo.neonmwi_state = 0;
		spin_unlock_irqrestore(&wc->reglock, flags);
	}

	return 0;
}

static inline struct wctdm *span_to_wctdm(struct dahdi_span *span)
{
	struct wctdm_span *s = container_of(span, struct wctdm_span, span);
	return s->wc;
}

static int wctdm_watchdog(struct dahdi_span *span, int event)
{
	struct wctdm *wc = span_to_wctdm(span);
	dev_info(&wc->vb.pdev->dev, "TDM: Called watchdog\n");
	return 0;
}

static int wctdm_close(struct dahdi_chan *chan)
{
	struct wctdm *wc;
	int x;
	signed char reg;

	wc = chan->pvt;
	for (x = 0; x < wc->mods_per_board; x++) {
		if (MOD_TYPE_FXS == wc->modtype[x]) {
			wc->mods[x].fxs.idletxhookstate =
				POLARITY_XOR(x) ? SLIC_LF_ACTIVE_REV :
						  SLIC_LF_ACTIVE_FWD;
		} else if (MOD_TYPE_QRV == wc->modtype[x]) {
			int qrvcard = x & 0xfc;

			wc->qrvhook[x] = 0;
			wc->qrvhook[x + 2] = 0xff;
			wc->debouncetime[x] = QRV_DEBOUNCETIME;
			wc->qrvdebtime[x] = 0;
			wc->radmode[x] = 0;
			wc->txgain[x] = 3599;
			wc->rxgain[x] = 1199;
			reg = 0;
			if (!wc->qrvhook[qrvcard])
				reg |= 1;
			if (!wc->qrvhook[qrvcard + 1])
				reg |= 0x10;
			wc->sethook[qrvcard] = CMD_WR(3, reg);
			qrv_dosetup(chan,wc);
		}
	}

	return 0;
}

static int wctdm_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct wctdm *wc = chan->pvt;
	int reg = 0, qrvcard;

	if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_QRV) {
		qrvcard = (chan->chanpos - 1) & 0xfc;
		switch(txsig) {
		case DAHDI_TXSIG_START:
		case DAHDI_TXSIG_OFFHOOK:
			wc->qrvhook[chan->chanpos - 1] = 1;
			break;
		case DAHDI_TXSIG_ONHOOK:
			wc->qrvhook[chan->chanpos - 1] = 0;
			break;
		default:
			dev_notice(&wc->vb.pdev->dev, "wctdm24xxp: Can't set tx state to %d\n", txsig);
		}
		reg = 0;
		if (!wc->qrvhook[qrvcard]) reg |= 1;
		if (!wc->qrvhook[qrvcard + 1]) reg |= 0x10;
		wc->sethook[qrvcard] = CMD_WR(3, reg);
		/* wctdm_setreg(wc, qrvcard, 3, reg); */
	} else if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXO) {
		switch(txsig) {
		case DAHDI_TXSIG_START:
		case DAHDI_TXSIG_OFFHOOK:
			wc->mods[chan->chanpos - 1].fxo.offhook = 1;
			wc->sethook[chan->chanpos - 1] = CMD_WR(5, 0x9);
			/* wctdm_setreg(wc, chan->chanpos - 1, 5, 0x9); */
			break;
		case DAHDI_TXSIG_ONHOOK:
			wc->mods[chan->chanpos - 1].fxo.offhook = 0;
			wc->sethook[chan->chanpos - 1] = CMD_WR(5, 0x8);
			/* wctdm_setreg(wc, chan->chanpos - 1, 5, 0x8); */
			break;
		default:
			dev_notice(&wc->vb.pdev->dev, "wctdm24xxp: Can't set tx state to %d\n", txsig);
		}
	} else if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXS) {
		wctdm_fxs_hooksig(wc, chan->chanpos - 1, txsig);
	}
	return 0;
}

static void wctdm_dacs_connect(struct wctdm *wc, int srccard, int dstcard)
{

	if (wc->dacssrc[dstcard] > - 1) {
		dev_notice(&wc->vb.pdev->dev, "wctdm_dacs_connect: Can't have double sourcing yet!\n");
		return;
	}
	if (!((wc->modtype[srccard] == MOD_TYPE_FXS)||(wc->modtype[srccard] == MOD_TYPE_FXO))){
		dev_notice(&wc->vb.pdev->dev, "wctdm_dacs_connect: Unsupported modtype for card %d\n", srccard);
		return;
	}
	if (!((wc->modtype[dstcard] == MOD_TYPE_FXS)||(wc->modtype[dstcard] == MOD_TYPE_FXO))){
		dev_notice(&wc->vb.pdev->dev, "wctdm_dacs_connect: Unsupported modtype for card %d\n", dstcard);
		return;
	}
	if (debug)
		dev_info(&wc->vb.pdev->dev, "connect %d => %d\n", srccard, dstcard);
	wc->dacssrc[dstcard] = srccard;

	/* make srccard transmit to srccard+24 on the TDM bus */
	if (wc->modtype[srccard] == MOD_TYPE_FXS) {
		/* proslic */
		wctdm_setreg(wc, srccard, PCM_XMIT_START_COUNT_LSB, ((srccard+24) * 8) & 0xff); 
		wctdm_setreg(wc, srccard, PCM_XMIT_START_COUNT_MSB, ((srccard+24) * 8) >> 8);
	} else if (wc->modtype[srccard] == MOD_TYPE_FXO) {
		/* daa */
		wctdm_setreg(wc, srccard, 34, ((srccard+24) * 8) & 0xff); /* TX */
		wctdm_setreg(wc, srccard, 35, ((srccard+24) * 8) >> 8);   /* TX */
	}

	/* have dstcard receive from srccard+24 on the TDM bus */
	if (wc->modtype[dstcard] == MOD_TYPE_FXS) {
		/* proslic */
    	wctdm_setreg(wc, dstcard, PCM_RCV_START_COUNT_LSB,  ((srccard+24) * 8) & 0xff);
		wctdm_setreg(wc, dstcard, PCM_RCV_START_COUNT_MSB,  ((srccard+24) * 8) >> 8);
	} else if (wc->modtype[dstcard] == MOD_TYPE_FXO) {
		/* daa */
		wctdm_setreg(wc, dstcard, 36, ((srccard+24) * 8) & 0xff); /* RX */
		wctdm_setreg(wc, dstcard, 37, ((srccard+24) * 8) >> 8);   /* RX */
	}

}

static void wctdm_dacs_disconnect(struct wctdm *wc, int card)
{
	if (wc->dacssrc[card] > -1) {
		if (debug)
			dev_info(&wc->vb.pdev->dev, "wctdm_dacs_disconnect: restoring TX for %d and RX for %d\n",wc->dacssrc[card], card);

		/* restore TX (source card) */
		if (wc->modtype[wc->dacssrc[card]] == MOD_TYPE_FXS) {
			wctdm_setreg(wc, wc->dacssrc[card], PCM_XMIT_START_COUNT_LSB, (wc->dacssrc[card] * 8) & 0xff);
			wctdm_setreg(wc, wc->dacssrc[card], PCM_XMIT_START_COUNT_MSB, (wc->dacssrc[card] * 8) >> 8);
		} else if (wc->modtype[wc->dacssrc[card]] == MOD_TYPE_FXO) {
			wctdm_setreg(wc, card, 34, (card * 8) & 0xff);
			wctdm_setreg(wc, card, 35, (card * 8) >> 8);
		} else {
			dev_warn(&wc->vb.pdev->dev, "WARNING: wctdm_dacs_disconnect() called on unsupported modtype\n");
		}

		/* restore RX (this card) */
		if (MOD_TYPE_FXS == wc->modtype[card]) {
			wctdm_setreg(wc, card, PCM_RCV_START_COUNT_LSB, (card * 8) & 0xff);
			wctdm_setreg(wc, card, PCM_RCV_START_COUNT_MSB, (card * 8) >> 8);
		} else if (MOD_TYPE_FXO == wc->modtype[card]) {
			wctdm_setreg(wc, card, 36, (card * 8) & 0xff);
			wctdm_setreg(wc, card, 37, (card * 8) >> 8);
		} else {
			dev_warn(&wc->vb.pdev->dev, "WARNING: wctdm_dacs_disconnect() called on unsupported modtype\n");
		}

		wc->dacssrc[card] = -1;
	}
}

static int wctdm_dacs(struct dahdi_chan *dst, struct dahdi_chan *src)
{
	struct wctdm *wc;

	if (!nativebridge)
		return 0; /* should this return -1 since unsuccessful? */

	wc = dst->pvt;

	if (src) {
		wctdm_dacs_connect(wc, src->chanpos - 1, dst->chanpos - 1);
		if (debug)
			dev_info(&wc->vb.pdev->dev, "dacs connecct: %d -> %d!\n\n", src->chanpos, dst->chanpos);
	} else {
		wctdm_dacs_disconnect(wc, dst->chanpos - 1);
		if (debug)
			dev_info(&wc->vb.pdev->dev, "dacs disconnect: %d!\n", dst->chanpos);
	}
	return 0;
}

static const struct dahdi_span_ops wctdm24xxp_analog_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = wctdm_hooksig,
	.open = wctdm_open,
	.close = wctdm_close,
	.ioctl = wctdm_ioctl,
	.watchdog = wctdm_watchdog,
	.dacs = wctdm_dacs,
#ifdef VPM_SUPPORT
	.echocan_create = wctdm_echocan_create,
#endif
};

static const struct dahdi_span_ops wctdm24xxp_digital_span_ops = {
	.owner = THIS_MODULE,
	.open = wctdm_open,
	.close = wctdm_close,
	.ioctl = wctdm_ioctl,
	.watchdog = wctdm_watchdog,
	.hdlc_hard_xmit = wctdm_hdlc_hard_xmit,
	.spanconfig = b400m_spanconfig,
	.chanconfig = b400m_chanconfig,
	.dacs = wctdm_dacs,
#ifdef VPM_SUPPORT
	.echocan_create = wctdm_echocan_create,
#endif
};

static inline bool dahdi_is_digital_span(const struct dahdi_span *s)
{
	return (s->linecompat > 0);
}

static struct wctdm_chan *wctdm_init_chan(struct wctdm *wc, struct wctdm_span *s, int chanoffset, int channo)
{
	struct wctdm_chan *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return NULL;

	/* Do not change the procfs representation for non-hx8 cards. */
	if (dahdi_is_digital_span(&s->span)) {
		sprintf(c->chan.name, "WCBRI/%d/%d/%d", wc->pos, s->spanno,
			channo);
	} else {
		sprintf(c->chan.name, "WCTDM/%d/%d", wc->pos, channo);
	}

	c->chan.chanpos = channo+1;
	c->chan.span = &s->span;
	c->chan.pvt = wc;
	c->timeslot = chanoffset + channo;
	return c;
}

#if 0
/**
 * wctdm_span_count() - Return the number of spans exported by this board.
 *
 * This is only called during initialization so let's just count the spans each
 * time we need this information as opposed to storing another variable in the
 * wctdm structure.
 */
static int wctdm_span_count(const struct wctdm *wc)
{
	int i;
	int count = 0;
	for (i = 0; i < MAX_SPANS; ++i) {
		if (wc->spans[i])
			++count;
	}
	return count;
}
#endif

static struct wctdm_span *wctdm_init_span(struct wctdm *wc, int spanno, int chanoffset, int chancount, int digital_span)
{
	int x;
	struct pci_dev *pdev = wc->vb.pdev;
	struct wctdm_chan *c;
	struct wctdm_span *s;
	static int spancount;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	/* DAHDI stuff */
	s->span.offset = spanno;

	s->spanno = spancount++;
	s->wc = wc;

	/* Do not change the procfs representation for non-hx8 cards. */
	if (digital_span)
		sprintf(s->span.name, "WCBRI/%d/%d", wc->pos, s->spanno);
	else
		sprintf(s->span.name, "WCTDM/%d", wc->pos);

	snprintf(s->span.desc, sizeof(s->span.desc) - 1, "%s Board %d", wc->desc->name, wc->pos + 1);
	snprintf(s->span.location, sizeof(s->span.location) - 1,
		 "PCI%s Bus %02d Slot %02d", (wc->flags[0] & FLAG_EXPRESS) ? " Express" : "",
		 pdev->bus->number, PCI_SLOT(pdev->devfn) + 1);
	s->span.manufacturer = "Digium";
	strncpy(s->span.devicetype, wc->desc->name, sizeof(s->span.devicetype) - 1);

	if (wc->companding == DAHDI_LAW_DEFAULT) {
		if (wc->digi_mods || digital_span)
			/* If we have a BRI module, Auto set to alaw */
			s->span.deflaw = DAHDI_LAW_ALAW;
		else
			/* Auto set to ulaw */
			s->span.deflaw = DAHDI_LAW_MULAW;
	} else if (wc->companding == DAHDI_LAW_ALAW) {
		/* Force everything to alaw */
		s->span.deflaw = DAHDI_LAW_ALAW;
	} else {
		/* Auto set to ulaw */
		s->span.deflaw = DAHDI_LAW_MULAW;
	}

	if (digital_span) {
		s->span.ops = &wctdm24xxp_digital_span_ops;
		s->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS | DAHDI_CONFIG_D4;
		s->span.linecompat |= DAHDI_CONFIG_ESF | DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
		s->span.linecompat |= DAHDI_CONFIG_NTTE | DAHDI_CONFIG_TERM;
		s->span.spantype = "TE";
	} else {
		s->span.ops = &wctdm24xxp_analog_span_ops;
		s->span.flags = DAHDI_FLAG_RBS;
		/* analog sigcap handled in fixup_analog_span() */
	}

	s->span.chans = kmalloc(sizeof(struct dahdi_chan *) * chancount, GFP_KERNEL);
	if (!s->span.chans)
		return NULL;

	/* allocate channels for the span */
	for (x = 0; x < chancount; x++) {
		c = wctdm_init_chan(wc, s, chanoffset, x);
		if (!c)
			return NULL;
		wc->chans[chanoffset + x] = c;
		s->span.chans[x] = &c->chan;
	}

	s->span.channels = chancount;
	s->span.irq = pdev->irq;

	if (digital_span) {
		wc->chans[chanoffset + 0]->chan.sigcap = DAHDI_SIG_CLEAR;
		wc->chans[chanoffset + 1]->chan.sigcap = DAHDI_SIG_CLEAR;
		wc->chans[chanoffset + 2]->chan.sigcap = DAHDI_SIG_HARDHDLC;
	}

	init_waitqueue_head(&s->span.maintq);
	wc->spans[spanno] = s;
	return s;
}

/**
 * should_set_alaw() - Should be called after all the spans are initialized.
 *
 * Returns true if the module companding should be set to alaw, otherwise
 * false.
 */
static bool should_set_alaw(const struct wctdm *wc)
{
	if (DAHDI_LAW_DEFAULT == wc->companding)
		return (wc->digi_mods > 0);
	else if (DAHDI_LAW_ALAW == wc->companding)
		return true;
	else
		return false;
}

static void wctdm_fixup_analog_span(struct wctdm *wc, int spanno)
{
	struct dahdi_span *s;
	int x, y;

	/* Finalize signalling  */
	y = 0;
	s = &wc->spans[spanno]->span;

	for (x = 0; x < wc->desc->ports; x++) {
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "fixup_analog: x=%d, y=%d modtype=%d, "
				 "s->chans[%d]=%p\n", x, y, wc->modtype[x],
				 y, s->chans[y]);
		}
		if (wc->modtype[x] == MOD_TYPE_FXO) {
			int val;
			s->chans[y++]->sigcap = DAHDI_SIG_FXSKS | DAHDI_SIG_FXSLS | DAHDI_SIG_SF | DAHDI_SIG_CLEAR;
			val = should_set_alaw(wc) ? 0x20 : 0x28;
#ifdef DEBUG
			val = (digitalloopback) ? 0x30 : val;
#endif
			wctdm_setreg(wc, x, 33, val);
		} else if (wc->modtype[x] == MOD_TYPE_FXS) {
			s->chans[y++]->sigcap = DAHDI_SIG_FXOKS | DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS | DAHDI_SIG_SF | DAHDI_SIG_EM | DAHDI_SIG_CLEAR;
			wctdm_setreg(wc, x, 1,
				     (should_set_alaw(wc) ? 0x20 : 0x28));
		} else if (wc->modtype[x] == MOD_TYPE_QRV) {
			s->chans[y++]->sigcap = DAHDI_SIG_SF | DAHDI_SIG_EM | DAHDI_SIG_CLEAR;
		} else {
			s->chans[y++]->sigcap = 0;
		}
	}

	for (x = 0; x < MAX_SPANS; x++) {
		if (!wc->spans[x])
			continue;
		if (wc->vpm100)
			strncat(wc->spans[x]->span.devicetype, " (VPM100M)", sizeof(wc->spans[x]->span.devicetype) - 1);
		else if (wc->vpmadt032)
			strncat(wc->spans[x]->span.devicetype, " (VPMADT032)", sizeof(wc->spans[x]->span.devicetype) - 1);
	}
}

static int wctdm_vpm_init(struct wctdm *wc)
{
	unsigned char reg;
	unsigned int mask;
	unsigned int ver;
	unsigned char vpmver=0;
	unsigned int i, x, y;

	for (x=0;x<NUM_EC;x++) {
		ver = wctdm_vpm_in(wc, x, 0x1a0); /* revision */
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "VPM100: Chip %d: ver %02x\n", x, ver);
		if (ver != 0x33) {
			if (x)
				dev_info(&wc->vb.pdev->dev,
						"VPM100: Inoperable\n");
			wc->vpm100 = 0;
			return -ENODEV;
		}	

		if (!x) {
			vpmver = wctdm_vpm_in(wc, x, 0x1a6) & 0xf;
			dev_info(&wc->vb.pdev->dev, "VPM Revision: %02x\n", vpmver);
		}


		/* Setup GPIO's */
		for (y=0;y<4;y++) {
			wctdm_vpm_out(wc, x, 0x1a8 + y, 0x00); /* GPIO out */
			if (y == 3)
				wctdm_vpm_out(wc, x, 0x1ac + y, 0x00); /* GPIO dir */
			else
				wctdm_vpm_out(wc, x, 0x1ac + y, 0xff); /* GPIO dir */
			wctdm_vpm_out(wc, x, 0x1b0 + y, 0x00); /* GPIO sel */
		}

		/* Setup TDM path - sets fsync and tdm_clk as inputs */
		reg = wctdm_vpm_in(wc, x, 0x1a3); /* misc_con */
		wctdm_vpm_out(wc, x, 0x1a3, reg & ~2);

		/* Setup Echo length (256 taps) */
		wctdm_vpm_out(wc, x, 0x022, 0);

		/* Setup timeslots */
		if (vpmver == 0x01) {
			wctdm_vpm_out(wc, x, 0x02f, 0x00); 
			wctdm_vpm_out(wc, x, 0x023, 0xff);
			mask = 0x11111111 << x;
		} else {
			wctdm_vpm_out(wc, x, 0x02f, 0x20  | (x << 3)); 
			wctdm_vpm_out(wc, x, 0x023, 0x3f);
			mask = 0x0000003f;
		}

		/* Setup the tdm channel masks for all chips*/
		for (i = 0; i < 4; i++)
			wctdm_vpm_out(wc, x, 0x33 - i, (mask >> (i << 3)) & 0xff);

		/* Setup convergence rate */
		reg = wctdm_vpm_in(wc,x,0x20);
		reg &= 0xE0;

		if (wc->companding == DAHDI_LAW_DEFAULT) {
			if (wc->digi_mods)
				/* If we have a BRI module, Auto set to alaw */
				reg |= 0x01;
			else
				/* Auto set to ulaw */
				reg &= ~0x01;
		} else if (wc->companding == DAHDI_LAW_ALAW) {
			/* Force everything to alaw */
			reg |= 0x01;
		} else {
			/* Auto set to ulaw */
			reg &= ~0x01;
		}

		wctdm_vpm_out(wc,x,0x20,(reg | 0x20));

		/* Initialize echo cans */
		for (i = 0 ; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				wctdm_vpm_out(wc,x,i,0x00);
		}

		for (i=0;i<30;i++) 
			schluffen(&wc->regq);

		/* Put in bypass mode */
		for (i = 0 ; i < MAX_TDM_CHAN ; i++) {
			if (mask & (0x00000001 << i)) {
				wctdm_vpm_out(wc,x,i,0x01);
			}
		}

		/* Enable bypass */
		for (i = 0 ; i < MAX_TDM_CHAN ; i++) {
			if (mask & (0x00000001 << i))
				wctdm_vpm_out(wc,x,0x78 + i,0x01);
		}
      
		/* Enable DTMF detectors (always DTMF detect all spans) */
		for (i = 0; i < 6; i++) {
			if (vpmver == 0x01) 
				wctdm_vpm_out(wc, x, 0x98 + i, 0x40 | (i << 2) | x);
			else
				wctdm_vpm_out(wc, x, 0x98 + i, 0x40 | i);
		}

		for (i = 0xB8; i < 0xC0; i++)
			wctdm_vpm_out(wc, x, i, 0xFF);
		for (i = 0xC0; i < 0xC4; i++)
			wctdm_vpm_out(wc, x, i, 0xff);

	} 
	
	/* TODO: What do the different values for vpm100 mean? */
	if (vpmver == 0x01) {
		wc->vpm100 = 2;
	} else {
		wc->vpm100 = 1;
	}

	dev_info(&wc->vb.pdev->dev, "Enabling VPM100 gain adjustments on any FXO ports found\n");
	for (i = 0; i < wc->desc->ports; i++) {
		if (wc->modtype[i] == MOD_TYPE_FXO) {
			/* Apply negative Tx gain of 4.5db to DAA */
			wctdm_setreg(wc, i, 38, 0x14);	/* 4db */
			wctdm_setreg(wc, i, 40, 0x15);	/* 0.5db */

			/* Apply negative Rx gain of 4.5db to DAA */
			wctdm_setreg(wc, i, 39, 0x14);	/* 4db */
			wctdm_setreg(wc, i, 41, 0x15);	/* 0.5db */
		}
	}

	return 0;
}

static void get_default_portconfig(GpakPortConfig_t *portconfig)
{
	memset(portconfig, 0, sizeof(GpakPortConfig_t));

	/* First Serial Port config */
	portconfig->SlotsSelect1 = SlotCfgNone;
	portconfig->FirstBlockNum1 = 0;
	portconfig->FirstSlotMask1 = 0x0000;
	portconfig->SecBlockNum1 = 1;
	portconfig->SecSlotMask1 = 0x0000;
	portconfig->SerialWordSize1 = SerWordSize8;
	portconfig->CompandingMode1 = cmpNone;
	portconfig->TxFrameSyncPolarity1 = FrameSyncActHigh;
	portconfig->RxFrameSyncPolarity1 = FrameSyncActHigh;
	portconfig->TxClockPolarity1 = SerClockActHigh;
	portconfig->RxClockPolarity1 = SerClockActHigh;
	portconfig->TxDataDelay1 = DataDelay0;
	portconfig->RxDataDelay1 = DataDelay0;
	portconfig->DxDelay1 = Disabled;
	portconfig->ThirdSlotMask1 = 0x0000;
	portconfig->FouthSlotMask1 = 0x0000;
	portconfig->FifthSlotMask1 = 0x0000;
	portconfig->SixthSlotMask1 = 0x0000;
	portconfig->SevenSlotMask1 = 0x0000;
	portconfig->EightSlotMask1 = 0x0000;

	/* Second Serial Port config */
	portconfig->SlotsSelect2 = SlotCfg2Groups;
	portconfig->FirstBlockNum2 = 0;
	portconfig->FirstSlotMask2 = 0xffff;
	portconfig->SecBlockNum2 = 1;
	portconfig->SecSlotMask2 = 0xffff;
	portconfig->SerialWordSize2 = SerWordSize8;
	portconfig->CompandingMode2 = cmpNone;
	portconfig->TxFrameSyncPolarity2 = FrameSyncActHigh;
	portconfig->RxFrameSyncPolarity2 = FrameSyncActHigh;
	portconfig->TxClockPolarity2 = SerClockActHigh;
	portconfig->RxClockPolarity2 = SerClockActLow;
	portconfig->TxDataDelay2 = DataDelay0;
	portconfig->RxDataDelay2 = DataDelay0;
	portconfig->DxDelay2 = Disabled;
	portconfig->ThirdSlotMask2 = 0x0000;
	portconfig->FouthSlotMask2 = 0x0000;
	portconfig->FifthSlotMask2 = 0x0000;
	portconfig->SixthSlotMask2 = 0x0000;
	portconfig->SevenSlotMask2 = 0x0000;
	portconfig->EightSlotMask2 = 0x0000;

	/* Third Serial Port Config */
	portconfig->SlotsSelect3 = SlotCfg2Groups;
	portconfig->FirstBlockNum3 = 0;
	portconfig->FirstSlotMask3 = 0xffff;
	portconfig->SecBlockNum3 = 1;
	portconfig->SecSlotMask3 = 0xffff;
	portconfig->SerialWordSize3 = SerWordSize8;
	portconfig->CompandingMode3 = cmpNone;
	portconfig->TxFrameSyncPolarity3 = FrameSyncActHigh;
	portconfig->RxFrameSyncPolarity3 = FrameSyncActHigh;
	portconfig->TxClockPolarity3 = SerClockActHigh;
	portconfig->RxClockPolarity3 = SerClockActLow;
	portconfig->TxDataDelay3 = DataDelay0;
	portconfig->RxDataDelay3 = DataDelay0;
	portconfig->DxDelay3 = Disabled;
	portconfig->ThirdSlotMask3 = 0x0000;
	portconfig->FouthSlotMask3 = 0x0000;
	portconfig->FifthSlotMask3 = 0x0000;
	portconfig->SixthSlotMask3 = 0x0000;
	portconfig->SevenSlotMask3 = 0x0000;
	portconfig->EightSlotMask3 = 0x0000;
}

static int wctdm_initialize_vpmadt032(struct wctdm *wc)
{
	int x;
	int res;
	unsigned long flags;
	struct vpmadt032_options options;

	GpakPortConfig_t portconfig;

	spin_lock_irqsave(&wc->reglock, flags);
	for (x = NUM_MODULES; x < NUM_MODULES + NUM_EC; x++)
		wc->modtype[x] = MOD_TYPE_NONE;
	spin_unlock_irqrestore(&wc->reglock, flags);

	options.debug = debug;
	options.vpmnlptype = vpmnlptype;
	options.vpmnlpthresh = vpmnlpthresh;
	options.vpmnlpmaxsupp = vpmnlpmaxsupp;
	options.channels = wc->avchannels;

	wc->vpmadt032 = vpmadt032_alloc(&options, wc->board_name);
	if (!wc->vpmadt032)
		return -ENOMEM;

	wc->vpmadt032->setchanconfig_from_state = setchanconfig_from_state;
	/* wc->vpmadt032->context = wc; */
	/* Pull the configuration information from the span holding
	 * the analog channels. */
	get_default_portconfig(&portconfig);
	res = vpmadt032_init(wc->vpmadt032, &wc->vb);
	if (res) {
		vpmadt032_free(wc->vpmadt032);
		wc->vpmadt032 = NULL;
		return res;
	}

	/* Now we need to configure the VPMADT032 module for this
	 * particular board. */
	res = config_vpmadt032(wc->vpmadt032, wc);
	if (res) {
		vpmadt032_free(wc->vpmadt032);
		wc->vpmadt032 = NULL;
		return res;
	}

	return 0;
}

static int wctdm_initialize_vpm(struct wctdm *wc)
{
	int res = 0;

	if (!vpmsupport) {
		dev_notice(&wc->vb.pdev->dev, "VPM: Support Disabled\n");
	} else if (!wctdm_vpm_init(wc)) {
		dev_info(&wc->vb.pdev->dev, "VPM: Present and operational (Rev %c)\n",
		       'A' + wc->vpm100 - 1);
		wc->ctlreg |= 0x10;
	} else {
		res = wctdm_initialize_vpmadt032(wc);
		if (!res)
			wc->ctlreg |= 0x10;
	}
	return res;
}

static int wctdm_identify_modules(struct wctdm *wc)
{
	int x;
	unsigned long flags;
	wc->ctlreg = 0x00;

	/* Make sure all units go into daisy chain mode */
	spin_lock_irqsave(&wc->reglock, flags);

/*
 * This looks a little weird.
 *
 * There are only 8 physical ports on the TDM/AEX800, but the code immediately
 * below sets 24 modules up.  This has to do with the altcs magic that allows us
 * to have single-port and quad-port modules on these products.
 * The variable "mods_per_board" is set to the appropriate value just below the
 * next code block.
 *
 * Now why this is important:
 * The FXS modules come out of reset in a two-byte, non-chainable SPI mode.
 * This is currently incompatible with how we do things, so we need to set
 * them to a chained, 3-byte command mode.  This is done by setting the module
 * type to MOD_TYPE_FXSINIT for a little while so that cmd_dequeue will
 * initialize the SLIC into the appropriate mode.
 *
 * This "go to 3-byte chained mode" command, however, wreaks havoc with HybridBRI.
 *
 * The solution:
 * Since HybridBRI is only designed to work in an 8-port card, and since the single-port
 * modules "show up" in SPI slots >= 8 in these cards, we only set SPI slots 8-23 to
 * MOD_TYPE_FXSINIT.  The HybridBRI will never see the command that causes it to freak
 * out and the single-port FXS cards get what they need so that when we probe with altcs
 * we see them.
 */

	for (x = 0; x < wc->mods_per_board; x++)
		wc->modtype[x] = MOD_TYPE_FXSINIT;

	wc->vpm100 = -1;
	for (x = wc->mods_per_board; x < wc->mods_per_board+NUM_EC; x++)
		wc->modtype[x] = MOD_TYPE_VPM;

	spin_unlock_irqrestore(&wc->reglock, flags);

/* Wait just a bit; this makes sure that cmd_dequeue is emitting SPI commands in the appropriate mode(s). */
	for (x = 0; x < 10; x++)
		schluffen(&wc->regq);

/* Now that all the cards have been reset, we can stop checking them all if there aren't as many */
	spin_lock_irqsave(&wc->reglock, flags);
	wc->mods_per_board = wc->desc->ports;
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* Reset modules */
	for (x = 0; x < wc->mods_per_board; x++) {
		int sane = 0, ret = 0, readi = 0;

		if (fatal_signal_pending(current))
			break;
retry:
		if (!(ret = wctdm_init_proslic(wc, x, 0, 0, sane))) {
			wc->modmap |= (1 << x);
			if (debug & DEBUG_CARD) {
				readi = wctdm_getreg(wc,x,LOOP_I_LIMIT);
				dev_info(&wc->vb.pdev->dev, "Proslic module %d loop current is %dmA\n", x, ((readi*3)+20));
			}
			dev_info(&wc->vb.pdev->dev, "Port %d: Installed -- AUTO FXS/DPO\n", x + 1);
		} else {
			if (ret != -2) {
				sane = 1;
				/* Init with Manual Calibration */
				if (!wctdm_init_proslic(wc, x, 0, 1, sane)) {
					wc->modmap |= (1 << x);

					if (debug & DEBUG_CARD) {
						readi = wctdm_getreg(wc, x, LOOP_I_LIMIT);
						dev_info(&wc->vb.pdev->dev, "Proslic module %d loop current is %dmA\n", x, ((readi*3)+20));
					}

					dev_info(&wc->vb.pdev->dev, "Port %d: Installed -- MANUAL FXS\n",x + 1);
				} else {
					dev_notice(&wc->vb.pdev->dev, "Port %d: FAILED FXS (%s)\n", x + 1, fxshonormode ? fxo_modes[_opermode].name : "FCC");
				}

			} else if (!(ret = wctdm_init_voicedaa(wc, x, 0, 0, sane))) {
				wc->modmap |= (1 << x);
				dev_info(&wc->vb.pdev->dev,
					 "Port %d: Installed -- AUTO FXO "
					 "(%s mode)\n", x + 1,
					 fxo_modes[_opermode].name);
			} else if (!wctdm_init_qrvdri(wc, x)) {
				wc->modmap |= 1 << x;
				dev_info(&wc->vb.pdev->dev,
					 "Port %d: Installed -- QRV DRI card\n", x + 1);
			} else if (is_hx8(wc) && !wctdm_init_b400m(wc, x)) {
				wc->modmap |= (1 << x);
				dev_info(&wc->vb.pdev->dev,
					 "Port %d: Installed -- BRI "
					 "quad-span module\n", x + 1);
			} else {
				if ((wc->desc->ports != 24) && ((x & 0x3) == 1) && !wc->altcs[x]) {
					spin_lock_irqsave(&wc->reglock, flags);
					wc->altcs[x] = 2;

					if (wc->desc->ports == 4) {
						wc->altcs[x+1] = 3;
						wc->altcs[x+2] = 3;
					}

					wc->modtype[x] = MOD_TYPE_FXSINIT;
					spin_unlock_irqrestore(&wc->reglock, flags);

					schluffen(&wc->regq);
					schluffen(&wc->regq);

					spin_lock_irqsave(&wc->reglock, flags);
					wc->modtype[x] = MOD_TYPE_FXS;
					spin_unlock_irqrestore(&wc->reglock, flags);

					if (debug & DEBUG_CARD)
						dev_info(&wc->vb.pdev->dev, "Trying port %d with alternate chip select\n", x + 1);
					goto retry;

				} else {
					dev_info(&wc->vb.pdev->dev,
						 "Port %d: Not installed\n",
						 x + 1);
					wc->modtype[x] = MOD_TYPE_NONE;
				}
			}
		}
	}	/* for (x...) */

	return 0;
}

static struct pci_driver wctdm_driver;

static void wctdm_back_out_gracefully(struct wctdm *wc)
{
	int i;
	unsigned long flags;
	struct sframe_packet *frame;
	LIST_HEAD(local_list);

	voicebus_release(&wc->vb);
#ifdef CONFIG_VOICEBUS_ECREFERENCE
	for (i = 0; i < ARRAY_SIZE(wc->ec_reference); ++i) {
		if (wc->ec_reference[i])
			dahdi_fifo_free(wc->ec_reference[i]);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(wc->spans); ++i) {
		if (wc->spans[i] && wc->spans[i]->span.chans)
			kfree(wc->spans[i]->span.chans);

		kfree(wc->spans[i]);
		wc->spans[i] = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(wc->chans); ++i) {
		kfree(wc->chans[i]);
		wc->chans[i] = NULL;
	}

	spin_lock_irqsave(&wc->frame_list_lock, flags);
	list_splice(&wc->frame_list, &local_list);
	spin_unlock_irqrestore(&wc->frame_list_lock, flags);

	while (!list_empty(&local_list)) {
		frame = list_entry(local_list.next,
				   struct sframe_packet, node);
		list_del(&frame->node);
		kfree(frame);
	}

	kfree(wc);
}

static const struct voicebus_operations voicebus_operations = {
	.handle_receive = handle_receive,
	.handle_transmit = handle_transmit,
};

static const struct voicebus_operations hx8_voicebus_operations = {
	.handle_receive = handle_hx8_receive,
	.handle_transmit = handle_hx8_transmit,
};

struct cmd_results {
	u8 results[8];
};

static int hx8_send_command(struct wctdm *wc, const u8 *command,
				     size_t count, int checksum,
				     int application, int bootloader,
				     struct cmd_results *results)
{
	int ret = 0;
	struct vbb *vbb;
	struct sframe_packet *frame;
	const int MAX_COMMAND_LENGTH = 264 + 4;
	unsigned long flags;
	dma_addr_t dma_addr;

	might_sleep();

	/* can't boot both into the application and the bootloader at once. */
	WARN_ON((application > 0) && (bootloader > 0));
	if ((application > 0) && (bootloader > 0))
		return -EINVAL;

	WARN_ON(count > MAX_COMMAND_LENGTH);
	if (count > MAX_COMMAND_LENGTH)
		return -EINVAL;

	vbb = dma_pool_alloc(wc->vb.pool, GFP_KERNEL, &dma_addr);
	WARN_ON(!vbb);
	if (!vbb)
		return -ENOMEM;

	vbb->dma_addr = dma_addr;
	memset(vbb->data, 0, SFRAME_SIZE);
	memcpy(&vbb->data[EFRAME_SIZE + EFRAME_GAP], command, count);

	vbb->data[EFRAME_SIZE] = 0x80 | ((application) ? 0 : 0x40) |
		((checksum) ? 0x20 : 0) | ((count & 0x100) >> 4);
	vbb->data[EFRAME_SIZE + 1] = count & 0xff;

	if (bootloader)
		vbb->data[EFRAME_SIZE + 3] = 0xAA;

	spin_lock_irqsave(&wc->vb.lock, flags);
	voicebus_transmit(&wc->vb, vbb);
	spin_unlock_irqrestore(&wc->vb.lock, flags);

	/* Do not wait for the response if the caller doesn't care about the
	 * results. */
	if (NULL == results)
		return 0;

	if (!wait_event_timeout(wc->regq, !list_empty(&wc->frame_list), 2*HZ)) {
		dev_err(&wc->vb.pdev->dev, "Timeout waiting "
			"for receive frame.\n");
		ret = -EIO;
	}

	/* We only want the last packet received.  Throw away anything else on
	 * the list */
	frame = NULL;
	spin_lock_irqsave(&wc->frame_list_lock, flags);
	while (!list_empty(&wc->frame_list)) {
		frame = list_entry(wc->frame_list.next,
				   struct sframe_packet, node);
		list_del(&frame->node);
		if (!list_empty(&wc->frame_list)) {
			kfree(frame);
			frame = NULL;
		}
	}
	spin_unlock_irqrestore(&wc->frame_list_lock, flags);

	if (frame) {
		memcpy(results->results, &frame->sframe[EFRAME_SIZE],
		       sizeof(results->results));
	} else {
		ret = -EIO;
	}

	return ret;

}

static int hx8_get_fpga_version(struct wctdm *wc, u8 *major, u8 *minor)
{
	int ret;
	struct cmd_results results;
	u8 command[] = {0xD7, 0x00};

	ret = hx8_send_command(wc, command, ARRAY_SIZE(command),
					0, 0, 0, &results);
	if (ret)
		return ret;

	*major = results.results[0];
	*minor = results.results[2];
	return 0;
}

static void hx8_cleanup_frame_list(struct wctdm *wc)
{
	unsigned long flags;
	LIST_HEAD(local_list);
	struct sframe_packet *frame;

	spin_lock_irqsave(&wc->frame_list_lock, flags);
	list_splice_init(&wc->frame_list, &local_list);
	spin_unlock_irqrestore(&wc->frame_list_lock, flags);

	while (!list_empty(&local_list)) {
		frame = list_entry(local_list.next, struct sframe_packet, node);
		list_del(&frame->node);
		kfree(frame);
	}
}

static int hx8_switch_to_application(struct wctdm *wc)
{
	int ret;
	u8 command[] = {0xD7, 0x00};

	ret = hx8_send_command(wc, command, ARRAY_SIZE(command),
					0, 1, 0, NULL);
	if (ret)
		return ret;

	msleep(1000);
	hx8_cleanup_frame_list(wc);

	return 0;
}

/**
 * hx8_switch_to_bootloader() - Send packet to switch hx8 into bootloader
 *
 */
static int hx8_switch_to_bootloader(struct wctdm *wc)
{
	int ret;
	u8 command[] = {0xD7, 0x00};

	ret = hx8_send_command(wc, command, ARRAY_SIZE(command),
					0, 0, 1, NULL);
	if (ret)
		return ret;

	/* It takes some time for the FPGA to reload and switch it's
	 * configuration. */
	msleep(300);
	hx8_cleanup_frame_list(wc);

	return 0;
}

struct ha80000_firmware {
	u8	header[6];
	u8	major_ver;
	u8	minor_ver;
	u8	data[54648];
	u32	chksum;
} __attribute__((packed));

static void hx8_send_dummy(struct wctdm *wc)
{
	u8 command[] = {0xD7, 0x00};

	hx8_send_command(wc, command, ARRAY_SIZE(command),
				  0, 0, 0, NULL);
}

static int hx8_read_status_register(struct wctdm *wc, u8 *status)
{
	int ret;
	struct cmd_results results;
	u8 command[] = {0xD7, 0x00};

	ret = hx8_send_command(wc, command, ARRAY_SIZE(command),
					0, 0, 0, &results);
	if (ret)
		return ret;

	*status = results.results[3];
	return 0;
}

static const unsigned int HYBRID_PAGE_SIZE = 264;

static int hx8_write_buffer(struct wctdm *wc, const u8 *buffer, size_t size)
{
	int ret = 0;
	struct cmd_results results;
	int padding_bytes = 0;
	u8 *local_data;
	u8 command[] = {0x84, 0, 0, 0};

	if (size > HYBRID_PAGE_SIZE)
		return -EINVAL;

	if (size < HYBRID_PAGE_SIZE)
		padding_bytes = HYBRID_PAGE_SIZE - size;

	local_data = kmalloc(sizeof(command) + size + padding_bytes, GFP_KERNEL);
	if (!local_data)
		return -ENOMEM;

	memcpy(local_data, command, sizeof(command));
	memcpy(&local_data[sizeof(command)], buffer, size);
	memset(&local_data[sizeof(command) + size], 0xff, padding_bytes);

	ret = hx8_send_command(wc, local_data,
					sizeof(command) + size + padding_bytes,
					1, 0, 0, &results);
	if (ret)
		goto cleanup;

cleanup:
	kfree(local_data);
	return ret;
}

static int hx8_buffer_to_page(struct wctdm *wc, const unsigned int page)
{
	int ret;
	struct cmd_results results;
	u8 command[] = {0x83, (page & 0x180) >> 7, (page & 0x7f) << 1, 0x00};

	ret = hx8_send_command(wc, command, sizeof(command),
					1, 0, 0, &results);
	if (ret)
		return ret;

	return 0;
}

static int hx8_wait_for_ready(struct wctdm *wc, const int timeout)
{
	int ret;
	u8 status;
	unsigned long local_timeout = jiffies + timeout;

	do {
		ret = hx8_read_status_register(wc, &status);
		if (ret)
			return ret;
		if ((status & 0x80) > 0)
			break;
	} while (time_after(local_timeout, jiffies));

	if (time_after(jiffies, local_timeout))
		return -EIO;

	return 0;
}

/**
 * hx8_reload_application - reload the application firmware
 *
 * NOTE: The caller should ensure that the board is in bootloader mode before
 * calling this function.
 */
static int hx8_reload_application(struct wctdm *wc, const struct ha80000_firmware *ha8_fw)
{
	unsigned int cur_page;
	const u8 *data;
	u8 status;
	int ret = 0;
	const int HYBRID_PAGE_COUNT = (sizeof(ha8_fw->data)) / HYBRID_PAGE_SIZE;

	dev_info(&wc->vb.pdev->dev, "Reloading firmware. Do not power down "
			"the system until the process is complete.\n");

	BUG_ON(!ha8_fw);
	might_sleep();

	data = &ha8_fw->data[0];
	ret = hx8_read_status_register(wc, &status);
	if (ret)
		return ret;

	for (cur_page = 0; cur_page < HYBRID_PAGE_COUNT; ++cur_page) {
		ret = hx8_write_buffer(wc, data, HYBRID_PAGE_SIZE);
		if (ret)
			return ret;
		/* The application starts out at page 0x100 */
		ret = hx8_buffer_to_page(wc, 0x100 + cur_page);
		if (ret)
			return ret;

		/* wait no more than a second for the write to the page to
		 * finish */
		ret = hx8_wait_for_ready(wc, HZ);
		if (ret)
			return ret;

		data += HYBRID_PAGE_SIZE;
	}

	return ret;
}

static void print_hx8_recovery_message(struct device *dev)
{
	dev_warn(dev, "The firmware may be corrupted.  Please completely "
		 "power off your system, power on, and then reload the driver "
		 "with the 'forceload' module parameter set to 1 to attempt "
		 "recovery.\n");
}

/**
 * hx8_check_firmware - Check the firmware version and load a new one possibly.
 *
 */
static int hx8_check_firmware(struct wctdm *wc)
{
	int ret;
	u8 major;
	u8 minor;
	const struct firmware *fw;
	const struct ha80000_firmware *ha8_fw;
	struct device *dev = &wc->vb.pdev->dev;
	int retries = 10;

	BUG_ON(!is_hx8(wc));

	might_sleep();

	do {
		hx8_send_dummy(wc);
		ret = hx8_get_fpga_version(wc, &major, &minor);
		if (!ret)
			break;
		if (fatal_signal_pending(current))
			return -EINTR;
	} while (--retries);

	if (ret) {
		print_hx8_recovery_message(dev);
		return ret;
	}

	/* If we're in the bootloader, try to jump into the application. */
	if ((1 == major) && (0x80 == minor) && !forceload) {
		dev_dbg(dev, "Switching to application.\n");
		hx8_switch_to_application(wc);
		ret = hx8_get_fpga_version(wc, &major, &minor);
		if (ret) {
			print_hx8_recovery_message(dev);
			return ret;
		}
	}

	dev_dbg(dev, "FPGA VERSION: %02x.%02x\n", major, minor);

	ret = request_firmware(&fw, "dahdi-fw-hx8.bin", dev);
	if (ret) {
		dev_warn(dev, "Failed to load firmware from userspace, skipping "
			 "check. (%d)\n", ret);
		return 0;
	}
	ha8_fw = (const struct ha80000_firmware *)fw->data;

	if ((fw->size != sizeof(*ha8_fw)) ||
	    (0 != memcmp("DIGIUM", ha8_fw->header, sizeof(ha8_fw->header))) ||
	    ((crc32(~0, (void *)ha8_fw, sizeof(*ha8_fw) - sizeof(u32)) ^ ~0) !=
	      ha8_fw->chksum)) {
		dev_warn(dev, "Firmware file is invalid. Skipping load.\n");
		ret = 0;
		goto cleanup;
	}

	dev_dbg(dev, "FIRMWARE: %02x.%02x\n", ha8_fw->major_ver, ha8_fw->minor_ver);

	if (ha8_fw->major_ver == major &&
	    ha8_fw->minor_ver == minor) {
		dev_dbg(dev, "Firmware versions match, skipping load.\n");
		ret = 0;
		goto cleanup;
	}

	if (2 == major) {
		hx8_switch_to_bootloader(wc);
		ret = hx8_get_fpga_version(wc, &major, &minor);
		if (ret)
			goto cleanup;
	}

	/* so now we're in boot loader mode, ready to load the new firmware. */
	ret = hx8_reload_application(wc, ha8_fw);
	if (ret)
		goto cleanup;

	dev_dbg(dev, "Firmware reloaded.  Booting into application.\n");

	hx8_switch_to_application(wc);
	ret = hx8_get_fpga_version(wc, &major, &minor);
	if (ret)
		goto cleanup;

	dev_dbg(dev, "FPGA VERSION AFTER LOAD: %02x.%02x\n", major, minor);

	if (forceload) {
		dev_warn(dev, "Please unset forceload if your card is able to "
			 "detect the installed modules.\n");
	}

cleanup:
	release_firmware(fw);
	dev_info(dev, "Hx8 firmware version: %d.%02d\n", major, minor);
	return ret;
}

#ifdef CONFIG_VOICEBUS_SYSFS
static ssize_t
voicebus_current_latency_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	struct wctdm *wc = dev_get_drvdata(dev);
	unsigned int current_latency;
	spin_lock_irqsave(&wc->vb.lock, flags);
	current_latency = wc->vb.min_tx_buffer_count;
	spin_unlock_irqrestore(&wc->vb.lock, flags);
	return sprintf(buf, "%d\n", current_latency);
}

static DEVICE_ATTR(voicebus_current_latency, 0400,
		   voicebus_current_latency_show, NULL);

static ssize_t vpm_firmware_version_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int res;
	u16 version = 0;
	struct wctdm *wc = dev_get_drvdata(dev);

	if (wc->vpmadt032) {
		res = gpakPingDsp(wc->vpmadt032->dspid, &version);
		if (res) {
			dev_info(&wc->vb.pdev->dev, "Failed gpakPingDsp %d\n", res);
			version = -1;
		}
	}

	return sprintf(buf, "%x.%02x\n", (version & 0xff00) >> 8, (version & 0xff));
}

static DEVICE_ATTR(vpm_firmware_version, 0400,
		   vpm_firmware_version_show, NULL);

static void create_sysfs_files(struct wctdm *wc)
{
	int ret;
	ret = device_create_file(&wc->vb.pdev->dev,
				 &dev_attr_voicebus_current_latency);
	if (ret) {
		dev_info(&wc->vb.pdev->dev,
			"Failed to create device attributes.\n");
	}

	ret = device_create_file(&wc->vb.pdev->dev,
				 &dev_attr_vpm_firmware_version);
	if (ret) {
		dev_info(&wc->vb.pdev->dev,
			"Failed to create device attributes.\n");
	}
}

static void remove_sysfs_files(struct wctdm *wc)
{
	device_remove_file(&wc->vb.pdev->dev,
			   &dev_attr_vpm_firmware_version);

	device_remove_file(&wc->vb.pdev->dev,
			   &dev_attr_voicebus_current_latency);
}

#else

static inline void create_sysfs_files(struct wctdm *wc) { return; }
static inline void remove_sysfs_files(struct wctdm *wc) { return; }

#endif /* CONFIG_VOICEBUS_SYSFS */

#ifdef USE_ASYNC_INIT
struct async_data {
	struct pci_dev *pdev;
	const struct pci_device_id *ent;
};
static int __devinit
__wctdm_init_one(struct pci_dev *pdev, const struct pci_device_id *ent,
		 async_cookie_t cookie)
#else
static int __devinit
__wctdm_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
#endif
{
	struct wctdm *wc;
	int i, ret;

	int anamods, digimods, curchan, curspan;
	
	neonmwi_offlimit_cycles = neonmwi_offlimit / MS_PER_HOOKCHECK;

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	down(&ifacelock);
	/* \todo this is a candidate for removal... */
	for (i = 0; i < WC_MAX_IFACES; ++i) {
		if (!ifaces[i]) {
			ifaces[i] = wc;
			break;
		}
	}
	up(&ifacelock);

#ifdef CONFIG_VOICEBUS_ECREFERENCE
	for (i = 0; i < ARRAY_SIZE(wc->ec_reference); ++i) {
		/* 256 is the smallest power of 2 that will contains the
		 * maximum possible amount of latency. */
		wc->ec_reference[i] = dahdi_fifo_alloc(256, GFP_KERNEL);

		if (IS_ERR(wc->ec_reference[i])) {
			ret = PTR_ERR(wc->ec_reference[i]);
			wc->ec_reference[i] = NULL;
			wctdm_back_out_gracefully(wc);
			return ret;
		}
	}
#endif


	wc->desc = (struct wctdm_desc *)ent->driver_data;

	/* This is to insure that the analog span is given lowest priority */
	wc->oldsync = -1;
	init_MUTEX(&wc->syncsem);
	INIT_LIST_HEAD(&wc->frame_list);
	spin_lock_init(&wc->frame_list_lock);

	snprintf(wc->board_name, sizeof(wc->board_name)-1, "%s%d", wctdm_driver.name, i);

	pci_set_drvdata(pdev, wc);
	wc->vb.ops = &voicebus_operations;
	wc->vb.pdev = pdev;
	wc->vb.debug = &debug;

	if (is_hx8(wc)) {
		wc->vb.ops = &hx8_voicebus_operations;
		ret = voicebus_boot_init(&wc->vb, wc->board_name);
	} else {
		wc->vb.ops = &voicebus_operations;
		ret = voicebus_init(&wc->vb, wc->board_name);
		voicebus_set_minlatency(&wc->vb, latency);
		voicebus_set_maxlatency(&wc->vb, max_latency);
	}

	if (ret) {
		kfree(wc);
		return ret;
	}

	create_sysfs_files(wc);

	voicebus_lock_latency(&wc->vb);

	init_waitqueue_head(&wc->regq);

	spin_lock_init(&wc->reglock);
	wc->mods_per_board = NUM_MODULES;
	wc->pos = i;
	wc->txident = 1;

	if (alawoverride) {
		companding = "alaw";
		dev_info(&wc->vb.pdev->dev, "The module parameter alawoverride"\
					" has been deprecated. Please use the "\
					"parameter companding=alaw instead");
	}

	if (!strcasecmp(companding, "alaw"))
		/* Force this card's companding to alaw */
		wc->companding = DAHDI_LAW_ALAW;
	else if (!strcasecmp(companding, "ulaw"))
		/* Force this card's companding to ulaw */
		wc->companding = DAHDI_LAW_MULAW;
	else
		/* Auto detect this card's companding */
		wc->companding = DAHDI_LAW_DEFAULT;

	for (i = 0; i < NUM_MODULES; i++) {
		wc->flags[i] = wc->desc->flags;
		wc->dacssrc[i] = -1;
	}

	/* Start the hardware processing. */
	if (voicebus_start(&wc->vb)) {
		BUG_ON(1);
	}

	if (is_hx8(wc)) {
		ret = hx8_check_firmware(wc);
		if (ret) {
			voicebus_release(&wc->vb);
			kfree(wc);
			return -EIO;
		}

		/* Switch to the normal operating mode for this card. */
		voicebus_stop(&wc->vb);
		wc->vb.ops = &voicebus_operations;
		voicebus_set_minlatency(&wc->vb, latency);
		voicebus_set_maxlatency(&wc->vb, max_latency);
		voicebus_set_hx8_mode(&wc->vb);
		if (voicebus_start(&wc->vb))
			BUG_ON(1);
	}

/* first we have to make sure that we process all module data, we'll fine-tune it later in this routine. */
	wc->avchannels = NUM_MODULES;

	/* Now track down what modules are installed */
	wctdm_identify_modules(wc);

	if (fatal_signal_pending(current)) {
		wctdm_back_out_gracefully(wc);
		return -EINTR;
	}
/*
 * Walk the module list and create a 3-channel span for every BRI module found.
 * Empty and analog modules get a common span which is allocated outside of this loop.
 */
	anamods = digimods = 0;
	curchan = curspan = 0;
	
	for (i = 0; i < wc->mods_per_board; i++) {
		struct b400m *b4;

		if (wc->modtype[i] == MOD_TYPE_NONE) {
			++curspan;
			continue;
		} else if (wc->modtype[i] == MOD_TYPE_BRI) {
			if (!is_hx8(wc)) {
				dev_info(&wc->vb.pdev->dev, "Digital modules "
					"detected on a non-hybrid card. "
					"This is unsupported.\n");
				wctdm_back_out_gracefully(wc);
				return -EIO;
			}
			wc->spans[curspan] = wctdm_init_span(wc, curspan, curchan, 3, 1);
			if (!wc->spans[curspan]) {
				wctdm_back_out_gracefully(wc);
				return -EIO;
			}
			b4 = wc->mods[i].bri;
			b400m_set_dahdi_span(b4, i & 0x03, wc->spans[curspan]);

			++curspan;
			curchan += 3;
			if (!(i & 0x03)) {
				b400m_post_init(b4);
				++digimods;
			}
		} else {
/*
 * FIXME: ABK:
 * create a wctdm_chan for every analog module and link them into a span of their own down below.
 * then evaluate all of the callbacks and hard-code whether they are receiving a dahdi_chan or wctdm_chan *.
 * Finally, move the union from the wctdm structure to the dahdi_chan structure, and we should have something
 * resembling a clean dynamic # of channels/dynamic # of spans driver.
 */
			++curspan;
			++anamods;
		}

		if (digimods > 2) {
			dev_info(&wc->vb.pdev->dev, "More than two digital modules detected. This is unsupported.\n");
			wctdm_back_out_gracefully(wc);
			return -EIO;
		}
	}

	wc->digi_mods = digimods;

/* create an analog span if there are analog modules, or if there are no digital ones. */
	if (anamods || !digimods) {
		if (!digimods) {
			curspan = 0;
		}
		wctdm_init_span(wc, curspan, curchan, wc->desc->ports, 0);
		wctdm_fixup_analog_span(wc, curspan);
		wc->aspan = wc->spans[curspan];
		curchan += wc->desc->ports;
		++curspan;
	}

	/* Now fix up the timeslots for the analog modules, since the digital
	 * modules are always first */
	for (i = 0; i < wc->mods_per_board; i++) {
		if (wc->modtype[i] == MOD_TYPE_FXS) {
			wctdm_proslic_set_ts(wc, i, (digimods * 12) + i);
		} else if (wc->modtype[i] == MOD_TYPE_FXO) {
			wctdm_voicedaa_set_ts(wc, i, (digimods * 12) + i);
		} else if (wc->modtype[i] == MOD_TYPE_QRV) {
			wctdm_qrvdri_set_ts(wc, i, (digimods * 12) + i);
		}
	}


	/* This shouldn't ever occur, but if we don't try to trap it, the driver
	 * will be scribbling into memory it doesn't own. */
	BUG_ON(curchan > 24);

	wc->avchannels = curchan;

	wctdm_initialize_vpm(wc);

#ifdef USE_ASYNC_INIT
		async_synchronize_cookie(cookie);
#endif
	/* We should be ready for DAHDI to come in now. */
	for (i = 0; i < MAX_SPANS; ++i) {
		if (!wc->spans[i])
			continue;

		if (dahdi_register(&wc->spans[i]->span, 0)) {
			dev_notice(&wc->vb.pdev->dev, "Unable to register span %d with DAHDI\n", i);
			while (i)
				dahdi_unregister(&wc->spans[i--]->span);
			wctdm_back_out_gracefully(wc);
			return -1;
		}
	}

	wc->initialized = 1;

	dev_info(&wc->vb.pdev->dev,
		 "Found a %s: %s (%d BRI spans, %d analog %s)\n",
		 (is_hx8(wc)) ? "Hybrid card" : "Wildcard TDM",
		 wc->desc->name, digimods*4, anamods,
		 (anamods == 1) ? "channel" : "channels");
	ret = 0;

	voicebus_unlock_latency(&wc->vb);
	return 0;
}

#ifdef USE_ASYNC_INIT
static __devinit void
wctdm_init_one_async(void *data, async_cookie_t cookie)
{
	struct async_data *dat = data;
	__wctdm_init_one(dat->pdev, dat->ent, cookie);
	kfree(dat);
}

static int __devinit
wctdm_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct async_data *dat;

	dat = kmalloc(sizeof(*dat), GFP_KERNEL);
	/* If we can't allocate the memory for the async_data, odds are we won't
	 * be able to initialize the device either, but let's try synchronously
	 * anyway... */
	if (!dat)
		return __wctdm_init_one(pdev, ent, 0);

	dat->pdev = pdev;
	dat->ent = ent;
	async_schedule(wctdm_init_one_async, dat);
	return 0;
}
#else
static int __devinit
wctdm_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return __wctdm_init_one(pdev, ent);
}
#endif


static void wctdm_release(struct wctdm *wc)
{
	int i;

	if (wc->initialized) {
		for (i = 0; i < MAX_SPANS; i++) {
			if (wc->spans[i])
				dahdi_unregister(&wc->spans[i]->span);
		}
	}

	down(&ifacelock);
	for (i = 0; i < WC_MAX_IFACES; i++)
		if (ifaces[i] == wc)
			break;
	ifaces[i] = NULL;
	up(&ifacelock);
	
	wctdm_back_out_gracefully(wc);
}

static void __devexit wctdm_remove_one(struct pci_dev *pdev)
{
	struct wctdm *wc = pci_get_drvdata(pdev);
	struct vpmadt032 *vpm = wc->vpmadt032;
	int i;


	if (wc) {

		remove_sysfs_files(wc);

		if (vpm) {
			clear_bit(VPM150M_DTMFDETECT, &vpm->control);
			clear_bit(VPM150M_ACTIVE, &vpm->control);
			flush_scheduled_work();
		}

		/* shut down any BRI modules */
		for (i = 0; i < wc->mods_per_board; i += 4) {
			if (wc->modtype[i] == MOD_TYPE_BRI)
				wctdm_unload_b400m(wc, i);
		}

		voicebus_stop(&wc->vb);

		if (vpm) {
			vpmadt032_free(wc->vpmadt032);
			wc->vpmadt032 = NULL;
		}

		dev_info(&wc->vb.pdev->dev, "Freed a %s\n",
				(is_hx8(wc)) ? "Hybrid card" : "Wildcard");
		/* Release span */
		wctdm_release(wc);
	}
}

static struct pci_device_id wctdm_pci_tbl[] = {
	{ 0xd161, 0x2400, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wctdm2400 },
	{ 0xd161, 0x0800, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wctdm800 },
	{ 0xd161, 0x8002, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcaex800 },
	{ 0xd161, 0x8003, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcaex2400 },
	{ 0xd161, 0x8005, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wctdm410 },
	{ 0xd161, 0x8006, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcaex410 },
	{ 0xd161, 0x8007, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcha80000 },
	{ 0xd161, 0x8008, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wchb80000 },
	{ 0 }
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
static void wctdm_shutdown(struct pci_dev *pdev)
{
	struct wctdm *wc = pci_get_drvdata(pdev);
	voicebus_stop(&wc->vb);
}
#endif

MODULE_DEVICE_TABLE(pci, wctdm_pci_tbl);

static struct pci_driver wctdm_driver = {
	.name = "wctdm24xxp",
	.probe = wctdm_init_one,
	.remove = __devexit_p(wctdm_remove_one),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
	.shutdown = wctdm_shutdown,
#endif
	.id_table = wctdm_pci_tbl,
};

static int __init wctdm_init(void)
{
	int res;
	int x;

	for (x = 0; x < ARRAY_SIZE(fxo_modes); x++) {
		if (!strcmp(fxo_modes[x].name, opermode))
			break;
	}
	if (x < ARRAY_SIZE(fxo_modes)) {
		_opermode = x;
	} else {
		printk(KERN_NOTICE "Invalid/unknown operating mode '%s' "
		       "specified.  Please choose one of:\n", opermode);
		for (x = 0; x < ARRAY_SIZE(fxo_modes); x++)
			printk(KERN_CONT "  %s\n", fxo_modes[x].name);
		printk(KERN_NOTICE "Note this option is CASE SENSITIVE!\n");
		return -ENODEV;
	}

	if (!strcmp(opermode, "AUSTRALIA")) {
		boostringer = 1;
		fxshonormode = 1;
	}

	/* for the voicedaa_check_hook defaults, if the user has not overridden
	   them by specifying them as module parameters, then get the values
	   from the selected operating mode
	*/
	if (battdebounce == 0) {
		battdebounce = fxo_modes[_opermode].battdebounce;
	}
	if (battalarm == 0) {
		battalarm = fxo_modes[_opermode].battalarm;
	}
	if (battthresh == 0) {
		battthresh = fxo_modes[_opermode].battthresh;
	}

	b400m_module_init();

	res = dahdi_pci_module(&wctdm_driver);
	if (res)
		return -ENODEV;

#ifdef USE_ASYNC_INIT
	async_synchronize_full();
#endif
	return 0;
}

static void __exit wctdm_cleanup(void)
{
	pci_unregister_driver(&wctdm_driver);
}


module_param(debug, int, 0600);
module_param(fxovoltage, int, 0600);
module_param(loopcurrent, int, 0600);
module_param(reversepolarity, int, 0600);
#ifdef DEBUG
module_param(robust, int, 0600);
module_param(digitalloopback, int, 0400);
MODULE_PARM_DESC(digitalloopback, "Set to 1 to place FXO modules into " \
		 "loopback mode for troubleshooting.");
#endif
module_param(opermode, charp, 0600);
module_param(lowpower, int, 0600);
module_param(boostringer, int, 0600);
module_param(fastringer, int, 0600);
module_param(fxshonormode, int, 0600);
module_param(battdebounce, uint, 0600);
module_param(battalarm, uint, 0600);
module_param(battthresh, uint, 0600);
module_param(nativebridge, int, 0600);
module_param(fxotxgain, int, 0600);
module_param(fxorxgain, int, 0600);
module_param(fxstxgain, int, 0600);
module_param(fxsrxgain, int, 0600);
module_param(ringdebounce, int, 0600);
module_param(fwringdetect, int, 0600);
module_param(latency, int, 0400);
module_param(max_latency, int, 0400);
module_param(neonmwi_monitor, int, 0600);
module_param(neonmwi_level, int, 0600);
module_param(neonmwi_envelope, int, 0600);
module_param(neonmwi_offlimit, int, 0600);
#ifdef VPM_SUPPORT
module_param(vpmsupport, int, 0400);
module_param(vpmnlptype, int, 0400);
module_param(vpmnlpthresh, int, 0400);
module_param(vpmnlpmaxsupp, int, 0400);
#endif

/* Module parameters backed by code in xhfc.c */
module_param(bri_debug, int, 0600);
MODULE_PARM_DESC(bri_debug, "bitmap: 1=general 2=dtmf 4=regops 8=fops 16=ec 32=st state 64=hdlc 128=alarm");

module_param(bri_spanfilter, int, 0600);
MODULE_PARM_DESC(bri_spanfilter, "debug filter for spans. bitmap: 1=port 1, 2=port 2, 4=port 3, 8=port 4");

module_param(bri_alarmdebounce, int, 0600);
MODULE_PARM_DESC(bri_alarmdebounce, "msec to wait before set/clear alarm condition");

module_param(bri_teignorered, int, 0600);
MODULE_PARM_DESC(bri_teignorered, "1=ignore (do not inform DAHDI) if a red alarm exists in TE mode");

module_param(bri_persistentlayer1, int, 0600);
MODULE_PARM_DESC(bri_persistentlayer1, "Set to 0 for disabling automatic layer 1 reactivation (when other end deactivates it)");

module_param(timingcable, int, 0600);
MODULE_PARM_DESC(timingcable, "Set to 1 for enabling timing cable.  This means that *all* cards in the system are linked together with a single timing cable");

module_param(forceload, int, 0600);
MODULE_PARM_DESC(forceload, "Set to 1 in order to force an FPGA reload after power on (currently only for HA8/HB8 cards).");

module_param(alawoverride, int, 0400);
MODULE_PARM_DESC(alawoverride, "This option has been deprecated. Please use "\
			     "the parameter \"companding\" instead");

module_param(companding, charp, 0400);
MODULE_PARM_DESC(companding, "Change the companding to \"auto\" or \"alaw\" " \
		"or \"ulaw\". Auto (default) will set everything to ulaw " \
		"unless a BRI module is installed. It will use alaw in that "
		"case.");

MODULE_DESCRIPTION("VoiceBus Driver for Wildcard Analog and Hybrid Cards");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_ALIAS("wctdm8xxp");
MODULE_ALIAS("wctdm4xxp");
MODULE_ALIAS("wcaex24xx");
MODULE_ALIAS("wcaex8xx");
MODULE_ALIAS("wcaex4xx");
MODULE_LICENSE("GPL v2");

module_init(wctdm_init);
module_exit(wctdm_cleanup);
