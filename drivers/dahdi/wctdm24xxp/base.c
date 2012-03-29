/*
 * Wildcard TDM2400P TDM FXS/FXO Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 * Support for TDM800P and VPM150M by Matthew Fredrickson <creslin@digium.com>
 *
 * Support for Hx8 by Andrew Kohlsmith <akohlsmith@mixdown.ca> and Matthew
 * Fredrickson <creslin@digium.com>
 *
 * Copyright (C) 2005 - 2011 Digium, Inc.
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
#define POLARITY_XOR(fxs) \
	((reversepolarity != 0) ^ ((fxs)->reversepolarity != 0) ^ \
	((fxs)->vmwi_linereverse != 0))

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

/* names of HWEC modules */
static const char *vpmadt032_name = "VPMADT032";
static const char *vpmoct_name = "VPMOCT032";

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

static inline bool is_pcie(const struct wctdm *wc)
{
	return (wc->desc->flags & FLAG_EXPRESS) > 0;
}

/**
 * Returns true if the card is one of the Hybrid Digital Analog Cards.
 */
static inline bool is_hx8(const struct wctdm *wc)
{
	return (&wcha80000 == wc->desc) || (&wchb80000 == wc->desc);
}

static inline struct dahdi_chan *
get_dahdi_chan(const struct wctdm *wc, struct wctdm_module *const mod)
{
	return wc->aspan->span.chans[mod->card];
}

static inline void
mod_hooksig(struct wctdm *wc, struct wctdm_module *mod, enum dahdi_rxsig rxsig)
{
	dahdi_hooksig(get_dahdi_chan(wc, mod), rxsig);
}

struct wctdm *ifaces[WC_MAX_IFACES];
DEFINE_SEMAPHORE(ifacelock);

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
static int fastpickup = -1; /* -1 auto, 0 no, 1 yes */
static int fxotxgain = 0;
static int fxorxgain = 0;
static int fxstxgain = 0;
static int fxsrxgain = 0;
static int nativebridge = 0;
static int ringdebounce = DEFAULT_RING_DEBOUNCE;
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

static const struct dahdi_echocan_features vpm_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_ops vpm_ec_ops = {
	.echocan_free = echocan_free,
};

static int
wctdm_init_proslic(struct wctdm *wc, struct wctdm_module *const mod, int fast,
		   int manual, int sane);

static void set_offsets(struct wctdm_module *const mod, int altcs)
{
	int card = mod->card;
	int bit;

	mod->subaddr = (altcs) ? 0 : (mod->card & 0x3);

	for (bit = 0; bit < ARRAY_SIZE(mod->offsets); ++bit) {
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
		mod->offsets[bit] = ((((card & 0x3) * 3 + bit) * 7) +
					(card >> 2) + altcs +
					((altcs) ? -21 : 0));
	}
}

static inline __attribute_const__ int
CMD_BYTE(const struct wctdm_module *const mod, const int bit)
{
	return mod->offsets[bit];
}

static inline __attribute_const__ int VPM_CMD_BYTE(int timeslot, int bit)
{
	return ((((timeslot) & 0x3) * 3 + (bit)) * 7) + ((timeslot) >> 2);
}

typedef int (*bg_work_func_t)(struct wctdm *wc, unsigned long data);

struct bg {
	struct workqueue_struct *wq;
	struct work_struct	work;
	struct completion	complete;
	struct wctdm		*wc;
	bg_work_func_t		fn;
	unsigned long		param;
	int			ret;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void bg_work_func(void *data)
{
	struct bg *bg = data;
#else
static void bg_work_func(struct work_struct *work)
{
	struct bg *bg = container_of(work, struct bg, work);
#endif
	bg->ret = bg->fn(bg->wc, bg->param);
	complete(&bg->complete);
}

/**
 * bg_create - Call a function running in a background thread.
 * @wc:		The board structure passed to fn
 * @fn:		The function to run in it's own thread.
 * @parma:	An extra parameter to pass to the fn.
 *
 * Returns NULL if the thread could not be created, otherwise a pointer to be
 * passed to bg_join in order to get the return value.
 *
 * The function 'fn' will be run in a new thread. The return value is the
 * return from the bg_join function.
 *
 * This would probably be best served by concurrency managed workqueues before
 * merging, but this will at least work on the older kernels tht DAHDI
 * supports.
 */
static struct bg *
bg_create(struct wctdm *wc, bg_work_func_t fn, unsigned long param)
{
	struct bg *bg;

	bg = kzalloc(sizeof(*bg), GFP_KERNEL);
	if (!bg)
		return NULL;

	bg->wq = create_singlethread_workqueue("wctdm_bg");
	if (!bg->wq) {
		kfree(bg);
		return NULL;
	}

	init_completion(&bg->complete);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&bg->work, bg_work_func, bg);
#else
	INIT_WORK(&bg->work, bg_work_func);
#endif

	bg->wc = wc;
	bg->fn = fn;
	bg->param = param;

	queue_work(bg->wq, &bg->work);

	return bg;
}

/**
 * bg_join - Wait for a background function to complete and get the result.
 * @bg:		Pointer returned from the bg_create call.
 *
 * Returns the result of the function passed to bg_create.
 */
static int bg_join(struct bg *bg)
{
	int ret = -ERESTARTSYS;

	if (unlikely(!bg))
		return -EINVAL;

	while (ret)
		ret = wait_for_completion_interruptible(&bg->complete);

	ret = bg->ret;
	destroy_workqueue(bg->wq);
	kfree(bg);

	return ret;
}

static void
setchanconfig_from_state(struct vpmadt032 *vpm, int channel,
			 GpakChannelConfig_t *chanconfig)
{
	GpakEcanParms_t *p;

	BUG_ON(!vpm);

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

struct vpmadt032_channel_setup {
	struct work_struct	work;
	struct wctdm		*wc;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void vpm_setup_work_func(void *data)
{
	struct vpmadt032_channel_setup *setup = data;
#else
static void vpm_setup_work_func(struct work_struct *work)
{
	struct vpmadt032_channel_setup *setup =
		container_of(work, struct vpmadt032_channel_setup, work);
#endif
	int i;
	int res;
	GpakChannelConfig_t chanconfig;
	GPAK_ChannelConfigStat_t cstatus;
	GPAK_AlgControlStat_t algstatus;
	GpakPortConfig_t portconfig = {0};
	gpakConfigPortStatus_t configportstatus;
	GPAK_PortConfigStat_t pstatus;
	struct vpmadt032 *vpm;
	struct wctdm *const wc = setup->wc;

	WARN_ON(!wc);
	WARN_ON(!wc->vpmadt032);
	if (unlikely(!wc || !wc->vpmadt032))
		return;
	vpm = wc->vpmadt032;

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
		return;
	} else {
		if (vpm->options.debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "Configured McBSP ports successfully\n");
	}

	res = gpakPingDsp(vpm->dspid, &vpm->version);
	if (res) {
		dev_notice(&wc->vb.pdev->dev, "Error pinging DSP (%d)\n", res);
		return;
	}

	for (i = 0; i < vpm->options.channels; ++i) {
		vpm->curecstate[i].tap_length = 0;
		vpm->curecstate[i].nlp_type = vpm->options.vpmnlptype;
		vpm->curecstate[i].nlp_threshold = vpm->options.vpmnlpthresh;
		vpm->curecstate[i].nlp_max_suppress =
						vpm->options.vpmnlpmaxsupp;
		vpm->curecstate[i].companding = ADT_COMP_ULAW;
		vpm->setchanconfig_from_state(vpm, i, &chanconfig);

		res = gpakConfigureChannel(vpm->dspid, i, tdmToTdm,
					   &chanconfig, &cstatus);
		if (res) {
			dev_notice(&wc->vb.pdev->dev,
				   "Unable to configure channel #%d (%d)",
				   i, res);
			if (res == 1)
				printk(KERN_CONT ", reason %d", cstatus);
			printk(KERN_CONT "\n");
			goto exit;
		}

		res = gpakAlgControl(vpm->dspid, i, BypassEcanA, &algstatus);
		if (res) {
			dev_notice(&wc->vb.pdev->dev,
				   "Unable to disable echo can on channel %d "
				   "(reason %d:%d)\n", i + 1, res, algstatus);
			goto exit;
		}

		res = gpakAlgControl(vpm->dspid, i,
				     BypassSwCompanding, &algstatus);
		if (res) {
			dev_notice(&wc->vb.pdev->dev,
				   "Unable to disable echo can on channel %d "
				   "(reason %d:%d)\n", i + 1, res, algstatus);
			goto exit;
		}
	}

	res = gpakPingDsp(vpm->dspid, &vpm->version);
	if (res) {
		dev_notice(&wc->vb.pdev->dev, "Error pinging DSP (%d)\n", res);
		goto exit;
	}

	set_bit(VPM150M_ACTIVE, &vpm->control);

exit:
	kfree(setup);
}

static int config_vpmadt032(struct vpmadt032 *vpm, struct wctdm *wc)
{
	struct vpmadt032_channel_setup *setup;

	/* Because the channel configuration can take such a long time, let's
	 * move this out onto the VPM workqueue so the system can proceeded
	 * with startup. */

	setup = kzalloc(sizeof(*setup), GFP_KERNEL);
	if (!setup)
		return -ENOMEM;

	setup->wc = wc;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&setup->work, vpm_setup_work_func, setup);
#else
	INIT_WORK(&setup->work, vpm_setup_work_func);
#endif

	queue_work(vpm->wq, &setup->work);

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

static inline void cmd_dequeue_vpmoct(struct wctdm *wc, u8 *eframe)
{
	struct vpmoct *vpm = wc->vpmoct;
	struct vpmoct_cmd *cmd;
	u8 i;

	/* Pop a command off pending list */
	spin_lock(&vpm->list_lock);
	if (list_empty(&vpm->pending_list)) {
		spin_unlock(&vpm->list_lock);
		return;
	}

	cmd = list_entry(vpm->pending_list.next, struct vpmoct_cmd, node);
	if (is_vpmoct_cmd_read(cmd))
		list_move_tail(&cmd->node, &vpm->active_list);
	else
		list_del_init(&cmd->node);

	/* Skip audio (24 bytes) and ignore first 6 timeslots */
	eframe += 30;

	/* Save ident so we can match the return eframe */
	cmd->txident = wc->txident;

	/* We have four timeslots to work with for a regular spi packet */
	/* TODO: Create debug flag for this in dev */

	/* The vpmoct requires a "sync" spi command as the first three bytes
	 * of an eframe */
	eframe[7*0] = 0x12;
	eframe[7*1] = 0x34;
	eframe[7*2] = 0x56;
	eframe[7*3] = cmd->command;
	eframe[7*4] = cmd->address;
	eframe[7*5] = cmd->data[0];
	for (i = 1; i < cmd->chunksize; i++)
		eframe[(7*5)+7*i] = cmd->data[i];

	/* Clean up fire-and-forget messages from memory */
	if (list_empty(&cmd->node))
		kfree(cmd);

	spin_unlock(&vpm->list_lock);
}

static inline void cmd_dequeue_vpmadt032(struct wctdm *wc, u8 *eframe)
{
	struct vpmadt032_cmd *curcmd = NULL;
	struct vpmadt032 *vpmadt032 = wc->vpmadt032;
	int x;
	unsigned char leds = ~((wc->framecount / 1000) % 8) & 0x7;

	/* Skip audio */
	eframe += 24;

	if (test_bit(VPM150M_HPIRESET, &vpmadt032->control)) {
		if (debug & DEBUG_ECHOCAN)
			dev_info(&wc->vb.pdev->dev, "HW Resetting VPMADT032...\n");
		for (x = 24; x < 28; x++) {
			if (x == 24) {
				if (test_and_clear_bit(VPM150M_HPIRESET,
						       &vpmadt032->control)) {
					eframe[VPM_CMD_BYTE(x, 0)] = 0x0b;
				} else {
					eframe[VPM_CMD_BYTE(x, 0)] = leds;
				}
			} else {
				eframe[VPM_CMD_BYTE(x, 0)] = leds;
			}
			eframe[VPM_CMD_BYTE(x, 1)] = 0;
			eframe[VPM_CMD_BYTE(x, 2)] = 0x00;
		}
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
			eframe[VPM_CMD_BYTE(24, 0)] = (0x8 << 4);
			eframe[VPM_CMD_BYTE(24, 1)] = 0;
			eframe[VPM_CMD_BYTE(24, 2)] = 0x20;

			/* Do a page write */
			if (curcmd->desc & __VPM150M_WR)
				eframe[VPM_CMD_BYTE(25, 0)] = ((0x8 | 0x4) << 4);
			else
				eframe[VPM_CMD_BYTE(25, 0)] = ((0x8 | 0x4 | 0x1) << 4);
			eframe[VPM_CMD_BYTE(25, 1)] = 0;
			if (curcmd->desc & __VPM150M_WR)
				eframe[VPM_CMD_BYTE(25, 2)] = curcmd->data & 0xf;
			else
				eframe[VPM_CMD_BYTE(25, 2)] = 0;

			/* Clear XADD */
			eframe[VPM_CMD_BYTE(26, 0)] = (0x8 << 4);
			eframe[VPM_CMD_BYTE(26, 1)] = 0;
			eframe[VPM_CMD_BYTE(26, 2)] = 0;

			/* Fill in to buffer to size */
			eframe[VPM_CMD_BYTE(27, 0)] = 0;
			eframe[VPM_CMD_BYTE(27, 1)] = 0;
			eframe[VPM_CMD_BYTE(27, 2)] = 0;

		} else {
			/* Set address */
			eframe[VPM_CMD_BYTE(24, 0)] = ((0x8 | 0x4) << 4);
			eframe[VPM_CMD_BYTE(24, 1)] = (curcmd->address >> 8) & 0xff;
			eframe[VPM_CMD_BYTE(24, 2)] = curcmd->address & 0xff;

			/* Send/Get our data */
			eframe[VPM_CMD_BYTE(25, 0)] = (curcmd->desc & __VPM150M_WR) ?
				((0x8 | (0x3 << 1)) << 4) : ((0x8 | (0x3 << 1) | 0x1) << 4);
			eframe[VPM_CMD_BYTE(25, 1)] = (curcmd->data >> 8) & 0xff;
			eframe[VPM_CMD_BYTE(25, 2)] = curcmd->data & 0xff;
			
			eframe[VPM_CMD_BYTE(26, 0)] = 0;
			eframe[VPM_CMD_BYTE(26, 1)] = 0;
			eframe[VPM_CMD_BYTE(26, 2)] = 0;

			/* Fill in the rest */
			eframe[VPM_CMD_BYTE(27, 0)] = 0;
			eframe[VPM_CMD_BYTE(27, 1)] = 0;
			eframe[VPM_CMD_BYTE(27, 2)] = 0;
		}
	} else if (test_and_clear_bit(VPM150M_SWRESET, &vpmadt032->control)) {
		for (x = 24; x < 28; x++) {
			if (x == 24)
				eframe[VPM_CMD_BYTE(x, 0)] = (0x8 << 4);
			else
				eframe[VPM_CMD_BYTE(x, 0)] = 0x00;
			eframe[VPM_CMD_BYTE(x, 1)] = 0;
			if (x == 24)
				eframe[VPM_CMD_BYTE(x, 2)] = 0x01;
			else
				eframe[VPM_CMD_BYTE(x, 2)] = 0x00;
		}
	} else {
		for (x = 24; x < 28; x++) {
			eframe[VPM_CMD_BYTE(x, 0)] = 0x00;
			eframe[VPM_CMD_BYTE(x, 1)] = 0x00;
			eframe[VPM_CMD_BYTE(x, 2)] = 0x00;
		}
	}

	/* Add our leds in */
	for (x = 24; x < 28; x++) {
		eframe[VPM_CMD_BYTE(x, 0)] |= leds;
	}
}

/* Call with wc->reglock held and local interrupts disabled */
static void _cmd_dequeue(struct wctdm *wc, u8 *eframe, int card, int pos)
{
	struct wctdm_module *const mod = &wc->mods[card];
	unsigned int curcmd=0;

	/* QRV only use commands relating to the first channel */
	if ((card & 0x03) && (mod->type == QRV))
		return;

	/* Skip audio */
	eframe += 24;
	/* Search for something waiting to transmit */
	if (pos) {
		if (!list_empty(&mod->pending_cmds)) {
			struct wctdm_cmd *const cmd =
				list_entry(mod->pending_cmds.next,
					   struct wctdm_cmd, node);
			curcmd = cmd->cmd;
			cmd->ident = wc->txident;
			list_move_tail(&cmd->node, &mod->active_cmds);
		}
	}

	if (!curcmd) {
		/* If nothing else, use filler */
		switch (mod->type) {
		case FXS:
			curcmd = CMD_RD(LINE_STATE);
			break;
		case FXO:
			curcmd = CMD_RD(12);
			break;
		case BRI:
			curcmd = 0x101010;
			break;
		case QRV:
			curcmd = CMD_RD(3);
			break;
		default:
			break;
		}
	}

	switch (mod->type) {
	case FXS:
		eframe[CMD_BYTE(mod, 0)] = (1 << (mod->subaddr));
		if (curcmd & __CMD_WR)
			eframe[CMD_BYTE(mod, 1)] = (curcmd >> 8) & 0x7f;
		else
			eframe[CMD_BYTE(mod, 1)] = 0x80 | ((curcmd >> 8) & 0x7f);
		eframe[CMD_BYTE(mod, 2)] = curcmd & 0xff;
		break;

	case FXO:
	{
		static const int ADDRS[4] = {0x00, 0x08, 0x04, 0x0c};
		if (curcmd & __CMD_WR)
			eframe[CMD_BYTE(mod, 0)] = 0x20 | ADDRS[mod->subaddr];
		else
			eframe[CMD_BYTE(mod, 0)] = 0x60 | ADDRS[mod->subaddr];
		eframe[CMD_BYTE(mod, 1)] = (curcmd >> 8) & 0xff;
		eframe[CMD_BYTE(mod, 2)] = curcmd & 0xff;
		break;
	}
	case FXSINIT:
		/* Special case, we initialize the FXS's into the three-byte command mode then
		   switch to the regular mode.  To send it into thee byte mode, treat the path as
		   6 two-byte commands and in the last one we initialize register 0 to 0x80. All modules
		   read this as the command to switch to daisy chain mode and we're done.  */
		eframe[CMD_BYTE(mod, 0)] = 0x00;
		eframe[CMD_BYTE(mod, 1)] = 0x00;
		if ((card & 0x1) == 0x1) 
			eframe[CMD_BYTE(mod, 2)] = 0x80;
		else
			eframe[CMD_BYTE(mod, 2)] = 0x00;
		break;

	case BRI:
		if (unlikely((curcmd != 0x101010) && (curcmd & 0x1010) == 0x1010)) /* b400m CPLD */
			eframe[CMD_BYTE(mod, 0)] = 0x55;
		else /* xhfc */
			eframe[CMD_BYTE(mod, 0)] = 0x10;
		eframe[CMD_BYTE(mod, 1)] = (curcmd >> 8) & 0xff;
		eframe[CMD_BYTE(mod, 2)] = curcmd & 0xff;
		break;

	case QRV:
		eframe[CMD_BYTE(mod, 0)] = 0x00;
		if (!curcmd) {
			eframe[CMD_BYTE(mod, 1)] = 0x00;
			eframe[CMD_BYTE(mod, 2)] = 0x00;
		} else {
			if (curcmd & __CMD_WR)
				eframe[CMD_BYTE(mod, 1)] = 0x40 | ((curcmd >> 8) & 0x3f);
			else
				eframe[CMD_BYTE(mod, 1)] = 0xc0 | ((curcmd >> 8) & 0x3f);
			eframe[CMD_BYTE(mod, 2)] = curcmd & 0xff;
		}
		break;

	case NONE:
		eframe[CMD_BYTE(mod, 0)] = 0x10;
		eframe[CMD_BYTE(mod, 1)] = 0x10;
		eframe[CMD_BYTE(mod, 2)] = 0x10;
		break;
	}
}

static inline void cmd_decipher_vpmoct(struct wctdm *wc, const u8 *eframe)
{
	struct vpmoct *vpm = wc->vpmoct;
	struct vpmoct_cmd *cmd;
	int i;

	/* Skip audio and first 6 timeslots */
	eframe += 30;

	spin_lock(&vpm->list_lock);
	/* No command to handle, just exit */
	if (list_empty(&vpm->active_list)) {
		spin_unlock(&vpm->list_lock);
		return;
	}

	cmd = list_entry(vpm->active_list.next, struct vpmoct_cmd, node);
	if (wc->rxident == cmd->txident)
		list_del_init(&cmd->node);
	else
		cmd = NULL;
	spin_unlock(&vpm->list_lock);

	if (!cmd)
		return;

	/* Store result, Ignoring the first "sync spi command" bytes */
	cmd->command = eframe[7*3];
	cmd->address = eframe[7*4];
	for (i = 0; i < cmd->chunksize; ++i)
		cmd->data[i] = eframe[7*(5+i)];
	complete(&cmd->complete);
}

static inline void cmd_decipher_vpmadt032(struct wctdm *wc, const u8 *eframe)
{
	struct vpmadt032 *const vpm = wc->vpmadt032;
	struct vpmadt032_cmd *cmd;

	BUG_ON(!vpm);

	/* If the hardware is not processing any commands currently, then
	 * there is nothing for us to do here. */
	if (list_empty(&vpm->active_cmds)) {
		return;
	}

	spin_lock(&vpm->list_lock);
	cmd = list_entry(vpm->active_cmds.next, struct vpmadt032_cmd, node);
	if (wc->rxident == cmd->txident) {
		list_del_init(&cmd->node);
	} else {
		cmd = NULL;
	}
	spin_unlock(&vpm->list_lock);

	if (!cmd)
		return;

	/* Skip audio */
	eframe += 24;

	/* Store result */
	cmd->data = (0xff & eframe[VPM_CMD_BYTE(25, 1)]) << 8;
	cmd->data |= eframe[VPM_CMD_BYTE(25, 2)];
	if (cmd->desc & __VPM150M_WR) {
		kfree(cmd);
	} else {
		cmd->desc |= __VPM150M_FIN;
		complete(&cmd->complete);
	}
}

/**
 * Call with the reglock held and local interrupts disabled
 */
static void _cmd_decipher(struct wctdm *wc, const u8 *eframe, int card)
{
	enum { TDM_BYTES = 24, };
	struct wctdm_module *const mod = &wc->mods[card];
	struct wctdm_cmd *cmd;
	u8 address;
	u8 value;

	if (list_empty(&mod->active_cmds))
		return;

	cmd = list_entry(mod->active_cmds.next, struct wctdm_cmd, node);
	if (cmd->ident != wc->rxident)
		return;

	list_del(&cmd->node);

	if (cmd->cmd & __CMD_WR) {
		kfree(cmd);
		return;
	}

	address = (cmd->cmd >> 8) & 0xff;

	cmd->cmd = eframe[TDM_BYTES + CMD_BYTE(mod, 2)];

	value = (cmd->cmd & 0xff);

	if (cmd->complete) {
		complete(cmd->complete);
		return;
	}

	list_add(&cmd->node, &wc->free_isr_commands);

	switch (mod->type) {
	case FXS:
		if (68 == address)
			mod->mod.fxs.hook_state_shadow = value;
		else
			mod->mod.fxs.linefeed_control_shadow = value;
		break;
	case FXO:
		if (5 == address)
			mod->mod.fxo.hook_ring_shadow = value;
		else
			mod->mod.fxo.line_voltage_status = value;
		break;
	case QRV:
		/* wctdm_isr_getreg(wc, mod, 3); */ /* COR/CTCSS state */
		/* TODO: This looks broken to me, but I have no way to
		 * resolved it. */
		/* wc->mods[card & 0xfc].cmds[USER_COMMANDS + 1] = CMD_RD(3); */
		break;
	default:
		break;
	}
}

/* Call with wc.reglock held and local interrupts disabled. */
static void
wctdm_isr_getreg(struct wctdm *wc, struct wctdm_module *const mod, u8 address)
{
	struct wctdm_cmd *cmd;

	if (!list_empty(&wc->free_isr_commands)) {
		cmd = list_entry(wc->free_isr_commands.next,
				 struct wctdm_cmd, node);
		list_del(&cmd->node);
	} else {
		cmd = kmalloc(sizeof(*cmd), GFP_ATOMIC);
		if (unlikely(!cmd))
			return;
	}

	cmd->cmd = CMD_RD(address);
	cmd->complete = NULL;

	list_add(&cmd->node, &mod->pending_cmds);
}

/* Must be called with wc.reglock held and local interrupts disabled */
static inline void
wctdm_setreg_intr(struct wctdm *wc, struct wctdm_module *mod, int addr, int val)
{
	struct wctdm_cmd *cmd;

	cmd = kmalloc(sizeof(*cmd), GFP_ATOMIC);
	if (unlikely(!cmd))
		return;

	cmd->complete = NULL;
	cmd->cmd = CMD_WR(addr, val);

	list_add_tail(&cmd->node, &mod->pending_cmds);
}


static void cmd_checkisr(struct wctdm *wc, struct wctdm_module *const mod)
{
	if (mod->sethook) {
		wctdm_setreg_intr(wc, mod, ((mod->sethook >> 8) & 0xff),
				  mod->sethook & 0xff);
		mod->sethook = 0;
		return;
	}

	switch (mod->type) {
	case FXS:
		wctdm_isr_getreg(wc, mod, 68); /* Hook state */
#ifdef PAQ_DEBUG
		wctdm_isr_getreg(wc, mod, 19); /* Transistor interrupts */
#else
		wctdm_isr_getreg(wc, mod, LINE_STATE);
#endif
		break;
	case FXO:
		wctdm_isr_getreg(wc, mod, 5);  /* Hook/Ring state */
		wctdm_isr_getreg(wc, mod, 29); /* Battery */
		break;
	case QRV:
		wctdm_isr_getreg(wc, mod, 3); /* COR/CTCSS state */
		/* TODO: This looks broken to me, but I have no way to
		 * resolved it. */
		/* wc->mods[card & 0xfc].cmds[USER_COMMANDS + 1] = CMD_RD(3); */
		break;
	case BRI:
		/* TODO: Two calls needed here? */
		wctdm_bri_checkisr(wc, mod, 0);
		wctdm_bri_checkisr(wc, mod, 1);
		break;
	default:
		break;
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

static inline void wctdm_transmitprep(struct wctdm *wc, unsigned char *sframe)
{
	int x, y;
	struct dahdi_span *s;
	unsigned char *eframe = sframe;

	/* Calculate Transmission */
	if (likely(is_initialized(wc))) {
		for (x = 0; x < MAX_SPANS; x++) {
			if (wc->spans[x]) {
				s = &wc->spans[x]->span;
				_dahdi_transmit(s);
			}
		}
		insert_tdm_data(wc, sframe);
#ifdef CONFIG_VOICEBUS_ECREFERENCE
		for (x = 0; x < wc->avchannels; ++x) {
			__dahdi_fifo_put(wc->ec_reference[x],
					 wc->chans[x]->chan.writechunk,
					 DAHDI_CHUNKSIZE);
		}
#endif
	}

	spin_lock(&wc->reglock);
	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		/* Send a sample, as a 32-bit word */

		/* TODO: ABK: hmm, this was originally mods_per_board, but we
		 * need to worry about all the active "voice" timeslots, since
		 * BRI modules have a different number of TDM channels than
		 * installed modules. */
		for (y = 0; y < wc->avchannels; y++) {
			if (y < wc->mods_per_board)
				_cmd_dequeue(wc, eframe, y, x);
		}

		if (wc->vpmadt032)
			cmd_dequeue_vpmadt032(wc, eframe);
		else if (wc->vpmoct)
			cmd_dequeue_vpmoct(wc, eframe);

		if (x < DAHDI_CHUNKSIZE - 1) {
			eframe[EFRAME_SIZE] = wc->ctlreg;
			eframe[EFRAME_SIZE + 1] = wc->txident++;
			if (4 == wc->desc->ports)
				eframe[EFRAME_SIZE + 2] = wc->tdm410leds;
		}
		eframe += (EFRAME_SIZE + EFRAME_GAP);
	}
	spin_unlock(&wc->reglock);
}

int wctdm_setreg(struct wctdm *wc, struct wctdm_module *mod, int addr, int val)
{
	struct wctdm_cmd *cmd;
	unsigned long flags;

#if 0 /* TODO */
	/* QRV and BRI cards are only addressed at their first "port" */
	if ((card & 0x03) && ((wc->mods[card].type ==  QRV) ||
	    (wc->mods[card].type ==  BRI)))
		return 0;
#endif

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (unlikely(!cmd))
		return -ENOMEM;

	cmd->complete = NULL;
	cmd->cmd = CMD_WR(addr, val);

	spin_lock_irqsave(&wc->reglock, flags);
	list_add_tail(&cmd->node, &mod->pending_cmds);
	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}

int wctdm_getreg(struct wctdm *wc, struct wctdm_module *const mod, int addr)
{
	unsigned long flags;
	struct wctdm_cmd *cmd;
	int val;

#if 0 /* TODO */
	/* if a QRV card, use only its first channel */  
	if (wc->mods[card].type ==  QRV) {
		if (card & 3)
			return 0;
	}
#endif

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->complete = kmalloc(sizeof(*cmd->complete), GFP_KERNEL);
	if (!cmd->complete) {
		kfree(cmd);
		return -ENOMEM;
	}

	init_completion(cmd->complete);

	cmd->cmd = CMD_RD(addr);

	spin_lock_irqsave(&wc->reglock, flags);
	list_add_tail(&cmd->node, &mod->pending_cmds);
	spin_unlock_irqrestore(&wc->reglock, flags);

	wait_for_completion(cmd->complete);
	val = cmd->cmd & 0xff;

	kfree(cmd->complete);
	kfree(cmd);

	return val;
}

static int wctdm_getregs(struct wctdm *wc, struct wctdm_module *const mod,
			 int *const addresses, const size_t count)
{
	int x;
	unsigned long flags;
	struct wctdm_cmd *cmd;
	struct wctdm_cmd **cmds = kmalloc(sizeof(cmd) * count, GFP_KERNEL);

	if (!cmds)
		return -ENOMEM;

	for (x = 0; x < count; ++x) {
		cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
		if (!cmd) {
			kfree(cmds);
			return -ENOMEM;
		}

		cmd->complete = kmalloc(sizeof(*cmd->complete), GFP_KERNEL);
		if (!cmd->complete) {
			kfree(cmd);
			kfree(cmds);
			return -ENOMEM;
		}

		init_completion(cmd->complete);

		cmd->cmd = CMD_RD(addresses[x]);

		spin_lock_irqsave(&wc->reglock, flags);
		list_add_tail(&cmd->node, &mod->pending_cmds);
		spin_unlock_irqrestore(&wc->reglock, flags);

		cmds[x] = cmd;
	}

	for (x = count - 1; x >= 0; --x) {
		cmd = cmds[x];
		wait_for_completion(cmd->complete);
		addresses[x] = cmd->cmd & 0xff;
		kfree(cmd->complete);
		kfree(cmd);
	}

	kfree(cmds);
	return 0;
}

/**
 * call with wc->reglock held and interrupts disabled.
 */
static void cmd_retransmit(struct wctdm *wc)
{
	int x;

	for (x = 0; x < wc->mods_per_board; x++) {
		struct wctdm_module *const mod = &wc->mods[x];
		if (mod->type == BRI)
			continue;
		list_splice_init(&mod->active_cmds, &mod->pending_cmds);
	}

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

	/* Pre-echo with the vpmoct overwrites the 24th timeslot with the
	 * specified channel's pre-echo audio stream. This data is ignored
	 * on all but the 24xx card, so we store it in a temporary buffer.
	 */
	if (wc->vpmoct && wc->vpmoct->preecho_enabled) {
		chanchunk = &wc->vpmoct->preecho_buf[0];
		chanchunk[0] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*0];
		chanchunk[1] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*1];
		chanchunk[2] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*2];
		chanchunk[3] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*3];
		chanchunk[4] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*4];
		chanchunk[5] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*5];
		chanchunk[6] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*6];
		chanchunk[7] = sframe[23 + (EFRAME_SIZE + EFRAME_GAP)*7];
	}
}

static inline void wctdm_receiveprep(struct wctdm *wc, const u8 *sframe)
{
	int x, y;
	bool irqmiss = false;
	unsigned char expected;
	const u8 *eframe = sframe;

	if (unlikely(!is_good_frame(sframe)))
		return;

	spin_lock(&wc->reglock);

	if (likely(is_initialized(wc)))
		extract_tdm_data(wc, sframe);

	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		if (x < DAHDI_CHUNKSIZE - 1) {
			expected = wc->rxident + 1;
			wc->rxident = eframe[EFRAME_SIZE + 1];
			if (wc->rxident != expected) {
				irqmiss = true;
				cmd_retransmit(wc);
			}
		}

		for (y = 0; y < wc->avchannels; y++)
			_cmd_decipher(wc, eframe, y);

		if (wc->vpmadt032)
			cmd_decipher_vpmadt032(wc, eframe);
		else if (wc->vpmoct)
			cmd_decipher_vpmoct(wc, eframe);

		eframe += (EFRAME_SIZE + EFRAME_GAP);
	}
	spin_unlock(&wc->reglock);

	/* XXX We're wasting 8 taps.  We should get closer :( */
	if (likely(is_initialized(wc))) {
		for (x = 0; x < wc->avchannels; x++) {
			struct wctdm_chan *const wchan = wc->chans[x];
			struct dahdi_chan *const c = &wchan->chan;
#ifdef CONFIG_VOICEBUS_ECREFERENCE
			unsigned char buffer[DAHDI_CHUNKSIZE];
			__dahdi_fifo_get(wc->ec_reference[x], buffer,
				    ARRAY_SIZE(buffer));
			_dahdi_ec_chunk(c, c->readchunk, buffer);
#else
			if ((wc->vpmoct) &&
			    (wchan->timeslot == wc->vpmoct->preecho_timeslot) &&
			    (wc->vpmoct->preecho_enabled)) {
				__dahdi_ec_chunk(c, c->readchunk,
						 wc->vpmoct->preecho_buf,
						 c->writechunk);
			} else {
				__dahdi_ec_chunk(c, c->readchunk, c->readchunk,
						 c->writechunk);
			}
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
				_dahdi_receive(s);
				if (unlikely(irqmiss))
					wc->ddev->irqmisses++;
			}
		}
	}
}

static int wait_access(struct wctdm *wc, struct wctdm_module *const mod)
{
	unsigned char data = 0;
	int count = 0;

	#define MAX 10 /* attempts */

	/* Wait for indirect access */
	while (count++ < MAX) {
		data = wctdm_getreg(wc, mod, I_STATUS);
		if (!data)
			return 0;
	}

	if (count > (MAX-1)) {
		dev_notice(&wc->vb.pdev->dev,
			   " ##### Loop error (%02x) #####\n", data);
	}

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

static int
wctdm_proslic_setreg_indirect(struct wctdm *wc, struct wctdm_module *const mod,
			      unsigned char address, unsigned short data)
{
	int res = -1;

	address = translate_3215(address);
	if (address == 255)
		return 0;

	if (!wait_access(wc, mod)) {
		wctdm_setreg(wc, mod, IDA_LO, (u8)(data & 0xFF));
		wctdm_setreg(wc, mod, IDA_HI, (u8)((data & 0xFF00)>>8));
		wctdm_setreg(wc, mod, IAA, address);
		res = 0;
	};
	return res;
}

static int
wctdm_proslic_getreg_indirect(struct wctdm *wc, struct wctdm_module *const mod,
			      unsigned char address)
{ 
	int res = -1;
	char *p=NULL;

	address = translate_3215(address);
	if (address == 255)
		return 0;

	if (!wait_access(wc, mod)) {
		wctdm_setreg(wc, mod, IAA, address);
		if (!wait_access(wc, mod)) {
			int addresses[2] = {IDA_LO, IDA_HI};
			wctdm_getregs(wc, mod, addresses,
				      ARRAY_SIZE(addresses));
			res = addresses[0] | (addresses[1] << 8);
		} else
			p = "Failed to wait inside\n";
	} else
		p = "failed to wait\n";
	if (p)
		dev_notice(&wc->vb.pdev->dev, "%s", p);
	return res;
}

static int
wctdm_proslic_init_indirect_regs(struct wctdm *wc, struct wctdm_module *mod)
{
	unsigned char i;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) {
		if (wctdm_proslic_setreg_indirect(wc, mod,
				indirect_regs[i].address,
				indirect_regs[i].initial))
			return -1;
	}

	return 0;
}

static int
wctdm_proslic_verify_indirect_regs(struct wctdm *wc, struct wctdm_module *mod)
{ 
	int passed = 1;
	unsigned short i, initial;
	int j;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) {
		j = wctdm_proslic_getreg_indirect(wc, mod,
						(u8)indirect_regs[i].address);
		if (j < 0) {
			dev_notice(&wc->vb.pdev->dev, "Failed to read indirect register %d\n", i);
			return -1;
		}
		initial = indirect_regs[i].initial;

		if ((j != initial) && (indirect_regs[i].altaddr != 255)) {
			dev_notice(&wc->vb.pdev->dev,
				   "!!!!!!! %s  iREG %X = %X  should be %X\n",
				   indirect_regs[i].name,
				   indirect_regs[i].address, j, initial);
			 passed = 0;
		}	
	}

	if (passed) {
		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev,
			 "Init Indirect Registers completed successfully.\n");
		}
	} else {
		dev_notice(&wc->vb.pdev->dev,
			" !!!!! Init Indirect Registers UNSUCCESSFULLY.\n");
		return -1;
	}
	return 0;
}

/**
 * wctdm_proslic_check_oppending -
 *
 * Ensures that a write to the line feed register on the SLIC has been
 * processed. If it hasn't after the timeout value, then it will resend the
 * command and wait for another timeout period.
 *
 */
static void
wctdm_proslic_check_oppending(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;

	if (!(fxs->lasttxhook & SLIC_LF_OPPENDING))
		return;

	/* Monitor the Pending LF state change, for the next 100ms */
	spin_lock(&wc->reglock);


	if (!(fxs->lasttxhook & SLIC_LF_OPPENDING)) {
		spin_unlock(&wc->reglock);
		return;
	}

	if ((fxs->linefeed_control_shadow & SLIC_LF_SETMASK) ==
	    (fxs->lasttxhook & SLIC_LF_SETMASK)) {
		fxs->lasttxhook &= SLIC_LF_SETMASK;
		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev,
				 "SLIC_LF OK: card=%d shadow=%02x "
				 "lasttxhook=%02x framecount=%ld\n", mod->card,
				 fxs->linefeed_control_shadow,
				 fxs->lasttxhook, wc->framecount);
		}
	} else if (time_after(wc->framecount, fxs->oppending_timeout)) {
		/* Check again in 100 ms */
		fxs->oppending_timeout = wc->framecount + 100;

		wctdm_setreg_intr(wc, mod, LINE_STATE, fxs->lasttxhook);
		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev,
				 "SLIC_LF RETRY: card=%d shadow=%02x "
				 "lasttxhook=%02x framecount=%ld\n", mod->card,
				 fxs->linefeed_control_shadow,
				 fxs->lasttxhook, wc->framecount);
		}
	}

	spin_unlock(&wc->reglock);
}

/* 256ms interrupt */
static void
wctdm_proslic_recheck_sanity(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;
	int res;
#ifdef PAQ_DEBUG
	res = mod->isrshadow[1];
	res &= ~0x3;
	if (res) {
		mod->isrshadow[1] = 0;
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->vb.pdev->dev, "Power alarm (%02x) on module %d, resetting!\n", res, card + 1);
			mod->sethook = CMD_WR(19, res);
			/* Update shadow register to avoid extra power alarms until next read */
			mod->isrshadow[1] = 0;
		} else {
			if (fxs->palarms == MAX_ALARMS)
				dev_notice(&wc->vb.pdev->dev, "Too many power alarms on card %d, NOT resetting!\n", card + 1);
		}
	}
#else
	spin_lock(&wc->reglock);

	/* reg 64 has to be zero at last isr read */
	res = !fxs->linefeed_control_shadow &&
		!(fxs->lasttxhook & SLIC_LF_OPPENDING) && /* not a transition */
		fxs->lasttxhook; /* not an intended zero */
	
	if (res) {
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->vb.pdev->dev,
				   "Power alarm on module %d, resetting!\n",
				   mod->card + 1);
			if (fxs->lasttxhook == SLIC_LF_RINGING) {
				fxs->lasttxhook = POLARITY_XOR(fxs) ?
							SLIC_LF_ACTIVE_REV :
							SLIC_LF_ACTIVE_FWD;;
			}
			fxs->lasttxhook |= SLIC_LF_OPPENDING;
			mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
			fxs->oppending_timeout = wc->framecount + 100;

			/* Update shadow register to avoid extra power alarms
			 * until next read */
			fxs->linefeed_control_shadow = fxs->lasttxhook;
		} else {
			if (fxs->palarms == MAX_ALARMS) {
				dev_notice(&wc->vb.pdev->dev,
					   "Too many power alarms on card %d, "
					   "NOT resetting!\n", mod->card + 1);
			}
		}
	}
	spin_unlock(&wc->reglock);
#endif
}

static void wctdm_qrvdri_check_hook(struct wctdm *wc, int card)
{
	signed char b,b1;
	int qrvcard = card & 0xfc;

	if (wc->mods[card].mod.qrv.debtime >= 2)
		wc->mods[card].mod.qrv.debtime--;
	b = wc->mods[qrvcard].mod.qrv.isrshadow[0]; /* Hook/Ring state */
	b &= 0xcc; /* use bits 3-4 and 6-7 only */

	if (wc->mods[qrvcard].mod.qrv.radmode & RADMODE_IGNORECOR)
		b &= ~4;
	else if (!(wc->mods[qrvcard].mod.qrv.radmode & RADMODE_INVERTCOR))
		b ^= 4;
	if (wc->mods[qrvcard + 1].mod.qrv.radmode | RADMODE_IGNORECOR)
		b &= ~0x40;
	else if (!(wc->mods[qrvcard + 1].mod.qrv.radmode | RADMODE_INVERTCOR))
		b ^= 0x40;

	if ((wc->mods[qrvcard].mod.qrv.radmode & RADMODE_IGNORECT) ||
	    (!(wc->mods[qrvcard].mod.qrv.radmode & RADMODE_EXTTONE)))
		b &= ~8;
	else if (!(wc->mods[qrvcard].mod.qrv.radmode & RADMODE_EXTINVERT))
		b ^= 8;
	if ((wc->mods[qrvcard + 1].mod.qrv.radmode & RADMODE_IGNORECT) ||
	    (!(wc->mods[qrvcard + 1].mod.qrv.radmode & RADMODE_EXTTONE)))
		b &= ~0x80;
	else if (!(wc->mods[qrvcard + 1].mod.qrv.radmode & RADMODE_EXTINVERT))
		b ^= 0x80;
	/* now b & MASK should be zero, if its active */
	/* check for change in chan 0 */
	if ((!(b & 0xc)) != wc->mods[qrvcard + 2].mod.qrv.hook)
	{
		wc->mods[qrvcard].mod.qrv.debtime = wc->mods[qrvcard].mod.qrv.debouncetime;
		wc->mods[qrvcard + 2].mod.qrv.hook = !(b & 0xc);
	} 
	/* if timed-out and ready */
	if (wc->mods[qrvcard].mod.qrv.debtime == 1) {
		b1 = wc->mods[qrvcard + 2].mod.qrv.hook;
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "QRV channel %d rx state changed to %d\n",
				 qrvcard, wc->mods[qrvcard + 2].mod.qrv.hook);
		}
		dahdi_hooksig(wc->aspan->span.chans[qrvcard],
			(b1) ? DAHDI_RXSIG_OFFHOOK : DAHDI_RXSIG_ONHOOK);
		wc->mods[card].mod.qrv.debtime = 0;
	}
	/* check for change in chan 1 */
	if ((!(b & 0xc0)) != wc->mods[qrvcard + 3].mod.qrv.hook)
	{
		wc->mods[qrvcard + 1].mod.qrv.debtime = QRV_DEBOUNCETIME;
		wc->mods[qrvcard + 3].mod.qrv.hook = !(b & 0xc0);
	}
	if (wc->mods[qrvcard + 1].mod.qrv.debtime == 1) {
		b1 = wc->mods[qrvcard + 3].mod.qrv.hook;
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "QRV channel %d rx state changed to %d\n",
				 qrvcard + 1, wc->mods[qrvcard + 3].mod.qrv.hook);
		}
		dahdi_hooksig(wc->aspan->span.chans[qrvcard + 1],
			(b1) ? DAHDI_RXSIG_OFFHOOK : DAHDI_RXSIG_ONHOOK);
		wc->mods[card].mod.qrv.debtime = 0;
	}
	return;
}

static inline bool is_fxo_ringing(const struct fxo *const fxo)
{
	return ((fxo->hook_ring_shadow & 0x60) &&
		(fxo->battery_state == BATTERY_PRESENT));
}

static inline bool is_fxo_ringing_positive(const struct fxo *const fxo)
{
	return (((fxo->hook_ring_shadow & 0x60) == 0x20) &&
		(fxo->battery_state == BATTERY_PRESENT));
}

static inline bool is_fxo_ringing_negative(const struct fxo *const fxo)
{
	return (((fxo->hook_ring_shadow & 0x60) == 0x40) &&
		(fxo->battery_state == BATTERY_PRESENT));
}

static inline void set_ring(struct fxo *fxo, enum ring_detector_state new)
{
	fxo->ring_state = new;
}

static void wctdm_fxo_ring_detect(struct wctdm *wc, struct wctdm_module *mod)
{
	struct fxo *const fxo = &mod->mod.fxo;

	/* Look for ring status bits (Ring Detect Signal Negative and Ring
	 * Detect Signal Positive) to transition back and forth some number of
	 * times to indicate that a ring is occurring.  Provide some number of
	 * samples to allow for the transitions to occur before giving up.
	 * NOTE: neon mwi voltages will trigger one of these bits to go active
	 * but not to have transitions between the two bits (i.e. no negative
	 * to positive or positive to negative traversals) */

	switch (fxo->ring_state) {
	case DEBOUNCING_RINGING_POSITIVE:
		if (is_fxo_ringing_negative(fxo)) {
			if (++fxo->ring_polarity_change_count > 4) {
				mod_hooksig(wc, mod, DAHDI_RXSIG_RING);
				set_ring(fxo, RINGING);
				if (debug) {
					dev_info(&wc->vb.pdev->dev,
						 "RING on %s!\n",
						 get_dahdi_chan(wc, mod)->name);
				}
			} else {
				set_ring(fxo, DEBOUNCING_RINGING_NEGATIVE);
			}
		} else if (time_after(wc->framecount,
				      fxo->ringdebounce_timer)) {
			set_ring(fxo, RINGOFF);
		}
		break;
	case DEBOUNCING_RINGING_NEGATIVE:
		if (is_fxo_ringing_positive(fxo)) {
			if (++fxo->ring_polarity_change_count > 4) {
				mod_hooksig(wc, mod, DAHDI_RXSIG_RING);
				set_ring(fxo, RINGING);
				if (debug) {
					dev_info(&wc->vb.pdev->dev,
						 "RING on %s!\n",
						 get_dahdi_chan(wc, mod)->name);
				}
			} else {
				set_ring(fxo, DEBOUNCING_RINGING_POSITIVE);
			}
		} else if (time_after(wc->framecount,
				      fxo->ringdebounce_timer)) {
			set_ring(fxo, RINGOFF);
		}
		break;
	case RINGING:
		if (!is_fxo_ringing(fxo)) {
			set_ring(fxo, DEBOUNCING_RINGOFF);
			fxo->ringdebounce_timer =
					wc->framecount + ringdebounce / 8;
		}
		break;
	case DEBOUNCING_RINGOFF:
		if (!is_fxo_ringing(fxo)) {
			if (time_after(wc->framecount,
				       fxo->ringdebounce_timer)) {
				if (debug) {
					dev_info(&wc->vb.pdev->dev,
						 "NO RING on %s!\n",
						 get_dahdi_chan(wc, mod)->name);
				}
				mod_hooksig(wc, mod, DAHDI_RXSIG_OFFHOOK);
				set_ring(fxo, RINGOFF);
			}
		} else {
			set_ring(fxo, RINGING);
		}
		break;
	case RINGOFF:
		if (is_fxo_ringing(fxo)) {
			/* Look for positive/negative crossings in ring status
			 * reg */
			if (is_fxo_ringing_positive(fxo))
				set_ring(fxo, DEBOUNCING_RINGING_POSITIVE);
			else
				set_ring(fxo, DEBOUNCING_RINGING_NEGATIVE);
			fxo->ringdebounce_timer =
					wc->framecount + ringdebounce / 8;
			fxo->ring_polarity_change_count = 0;
		}
		break;
	}
}

#define MS_PER_CHECK_HOOK 1

static void
wctdm_check_battery_lost(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxo *const fxo = &mod->mod.fxo;

	/* possible existing states:
	   battery lost, no debounce timer
	   battery lost, debounce timer (going to battery present)
	   battery present or unknown, no debounce timer
	   battery present or unknown, debounce timer (going to battery lost)
	*/
	switch (fxo->battery_state) {
	case BATTERY_DEBOUNCING_PRESENT:
		/* we were going to BATTERY_PRESENT, but
		 * battery was lost again. */
		fxo->battery_state = BATTERY_LOST;
		break;
	case BATTERY_UNKNOWN:
		mod_hooksig(wc, mod, DAHDI_RXSIG_ONHOOK);
	case BATTERY_DEBOUNCING_PRESENT_ALARM: /* intentional drop through */
	case BATTERY_PRESENT:
		fxo->battery_state = BATTERY_DEBOUNCING_LOST;
		fxo->battdebounce_timer = wc->framecount + battdebounce;
		break;
	case BATTERY_DEBOUNCING_LOST:
		if (time_after(wc->framecount, fxo->battdebounce_timer)) {
			if (debug) {
				dev_info(&wc->vb.pdev->dev,
					 "NO BATTERY on %d/%d!\n",
					 wc->aspan->span.spanno,
					 mod->card + 1);
			}
#ifdef	JAPAN
			if (!wc->ohdebounce && wc->offhook) {
				dahdi_hooksig(wc->aspan->chans[card],
					      DAHDI_RXSIG_ONHOOK);
				if (debug) {
					dev_info(&wc->vb.pdev->dev,
						 "Signalled On Hook\n");
				}
#ifdef	ZERO_BATT_RING
				wc->onhook++;
#endif
			}
#else
			mod_hooksig(wc, mod, DAHDI_RXSIG_ONHOOK);
#endif
			/* set the alarm timer, taking into account that part
			 * of its time period has already passed while
			 * debouncing occurred */
			fxo->battery_state = BATTERY_DEBOUNCING_LOST_ALARM;
			fxo->battdebounce_timer = wc->framecount +
						   battalarm - battdebounce;
		}
		break;
	case BATTERY_DEBOUNCING_LOST_ALARM:
		if (time_after(wc->framecount, fxo->battdebounce_timer)) {
			fxo->battery_state = BATTERY_LOST;
			dahdi_alarm_channel(get_dahdi_chan(wc, mod),
					    DAHDI_ALARM_RED);
		}
		break;
	case BATTERY_LOST:
		break;
	}
}

static void
wctdm_check_battery_present(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxo *const fxo = &mod->mod.fxo;

	switch (fxo->battery_state) {
	case BATTERY_DEBOUNCING_PRESENT:
		if (time_after(jiffies, fxo->battdebounce_timer)) {
			if (debug) {
				dev_info(&wc->vb.pdev->dev,
					 "BATTERY on %d/%d (%s)!\n",
					 wc->aspan->span.spanno, mod->card + 1,
					 (fxo->line_voltage_status < 0) ?
						"-" : "+");
			}
#ifdef	ZERO_BATT_RING
			if (wc->onhook) {
				wc->onhook = 0;
				dahdi_hooksig(wc->aspan->chans[card],
					      DAHDI_RXSIG_OFFHOOK);
				if (debug) {
					dev_info(&wc->vb.pdev->dev,
						 "Signalled Off Hook\n");
				}
			}
#else
			mod_hooksig(wc, mod, DAHDI_RXSIG_OFFHOOK);
#endif
			/* set the alarm timer, taking into account that part
			 * of its time period has already passed while
			 * debouncing occurred */
			fxo->battery_state = BATTERY_DEBOUNCING_PRESENT_ALARM;
			fxo->battdebounce_timer = jiffies +
				msecs_to_jiffies(battalarm - battdebounce);
		}
		break;
	case BATTERY_DEBOUNCING_PRESENT_ALARM:
		if (time_after(jiffies, fxo->battdebounce_timer)) {
			fxo->battery_state = BATTERY_PRESENT;
			dahdi_alarm_channel(get_dahdi_chan(wc, mod),
					    DAHDI_ALARM_NONE);
		}
		break;
	case BATTERY_PRESENT:
		break;
	case BATTERY_DEBOUNCING_LOST:
		/* we were going to BATTERY_LOST, but battery appeared again,
		 * so clear the debounce timer */
		fxo->battery_state = BATTERY_PRESENT;
		break;
	case BATTERY_UNKNOWN:
		mod_hooksig(wc, mod, DAHDI_RXSIG_OFFHOOK);
	case BATTERY_LOST: /* intentional drop through */
	case BATTERY_DEBOUNCING_LOST_ALARM:
		fxo->battery_state = BATTERY_DEBOUNCING_PRESENT;
		fxo->battdebounce_timer = jiffies +
						msecs_to_jiffies(battdebounce);
		break;
	}
}

static void
wctdm_fxo_stop_debouncing_polarity(struct wctdm *wc,
				   struct wctdm_module *const mod)
{
	struct fxo *const fxo = &mod->mod.fxo;
	switch (fxo->polarity_state) {
	case UNKNOWN_POLARITY:
		break;
	case POLARITY_DEBOUNCE_POSITIVE:
		fxo->polarity_state = POLARITY_NEGATIVE;
		break;
	case POLARITY_POSITIVE:
		break;
	case POLARITY_DEBOUNCE_NEGATIVE:
		fxo->polarity_state = POLARITY_POSITIVE;
		break;
	case POLARITY_NEGATIVE:
		break;
	};
}

static void
wctdm_fxo_check_polarity(struct wctdm *wc, struct wctdm_module *const mod,
			 const bool positive_polarity)
{
	struct fxo *const fxo = &mod->mod.fxo;

	switch (fxo->polarity_state) {
	case UNKNOWN_POLARITY:
		fxo->polarity_state = (positive_polarity) ? POLARITY_POSITIVE :
							    POLARITY_NEGATIVE;
		break;
	case POLARITY_DEBOUNCE_POSITIVE:
		if (!positive_polarity) {
			fxo->polarity_state = POLARITY_NEGATIVE;
		} else if (time_after(wc->framecount, fxo->poldebounce_timer)) {
			fxo->polarity_state = POLARITY_POSITIVE;
			dahdi_qevent_lock(get_dahdi_chan(wc, mod),
					  DAHDI_EVENT_POLARITY);
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "%s: Polarity NEGATIVE -> POSITIVE\n",
					 get_dahdi_chan(wc, mod)->name);
			}
		}
		break;
	case POLARITY_POSITIVE:
		if (!positive_polarity) {
			fxo->polarity_state = POLARITY_DEBOUNCE_NEGATIVE;
			fxo->poldebounce_timer = wc->framecount +
							POLARITY_DEBOUNCE;
		}
		break;
	case POLARITY_DEBOUNCE_NEGATIVE:
		if (positive_polarity) {
			fxo->polarity_state = POLARITY_POSITIVE;
		} else if (time_after(wc->framecount, fxo->poldebounce_timer)) {
			dahdi_qevent_lock(get_dahdi_chan(wc, mod),
					  DAHDI_EVENT_POLARITY);
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "%s: Polarity POSITIVE -> NEGATIVE\n",
					 get_dahdi_chan(wc, mod)->name);
			}
			fxo->polarity_state = POLARITY_NEGATIVE;
		}
		break;
	case POLARITY_NEGATIVE:
		if (positive_polarity) {
			fxo->polarity_state = POLARITY_DEBOUNCE_POSITIVE;
			fxo->poldebounce_timer = wc->framecount +
							POLARITY_DEBOUNCE;
		}
		break;
	};
}

static void
wctdm_voicedaa_check_hook(struct wctdm *wc, struct wctdm_module *const mod)
{
	signed char b;
	u8 abs_voltage;
	struct fxo *const fxo = &mod->mod.fxo;

	/* Try to track issues that plague slot one FXO's */
	b = fxo->hook_ring_shadow & 0x9b;

	if (fxo->offhook) {
		if (b != 0x9)
			wctdm_setreg_intr(wc, mod, 5, 0x9);
	} else {
		if (b != 0x8)
			wctdm_setreg_intr(wc, mod, 5, 0x8);

		wctdm_fxo_ring_detect(wc, mod);
	}

	abs_voltage = abs(fxo->line_voltage_status);

	if (fxovoltage && time_after(wc->framecount, fxo->display_fxovoltage)) {
		/* Every 100 ms */
		fxo->display_fxovoltage = wc->framecount + 100;
		dev_info(&wc->vb.pdev->dev,
			 "Port %d: Voltage: %d\n",
			 mod->card + 1, fxo->line_voltage_status);
	}

	if (unlikely(DAHDI_RXSIG_INITIAL == get_dahdi_chan(wc, mod)->rxhooksig)) {
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
		fxo->battery_state = BATTERY_UNKNOWN;
	}

	if (abs_voltage < battthresh) {
		wctdm_fxo_stop_debouncing_polarity(wc, mod);
		wctdm_check_battery_lost(wc, mod);
	} else {
		wctdm_check_battery_present(wc, mod);
		wctdm_fxo_check_polarity(wc, mod,
					 (fxo->line_voltage_status > 0));
	}

	/* Look for neon mwi pulse */
	if (neonmwi_monitor && !fxo->offhook) {
		/* Look for 4 consecutive voltage readings
		* where the voltage is over the neon limit but
		* does not vary greatly from the last reading
		*/
		if (fxo->battery_state == BATTERY_PRESENT &&
				  abs_voltage > neonmwi_level &&
				  (0 == fxo->neonmwi_last_voltage ||
				  (fxo->line_voltage_status >= fxo->neonmwi_last_voltage - neonmwi_envelope &&
				  fxo->line_voltage_status <= fxo->neonmwi_last_voltage + neonmwi_envelope))) {
			fxo->neonmwi_last_voltage = fxo->line_voltage_status;
			if (NEONMWI_ON_DEBOUNCE == fxo->neonmwi_debounce) {
				fxo->neonmwi_offcounter = neonmwi_offlimit_cycles;
				if (0 == fxo->neonmwi_state) {
					dahdi_qevent_lock(get_dahdi_chan(wc, mod), DAHDI_EVENT_NEONMWI_ACTIVE);
					fxo->neonmwi_state = 1;
					if (debug)
						dev_info(&wc->vb.pdev->dev, "NEON MWI active for card %d\n", mod->card+1);
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
				dahdi_qevent_lock(get_dahdi_chan(wc, mod), DAHDI_EVENT_NEONMWI_INACTIVE);
				fxo->neonmwi_state = 0;
				if (debug)
					dev_info(&wc->vb.pdev->dev, "NEON MWI cleared for card %d\n", mod->card+1);
			}
		}
	}
#undef MS_PER_CHECK_HOOK
}

static void
wctdm_fxs_hooksig(struct wctdm *wc, struct wctdm_module *const mod,
		  enum dahdi_txsig txsig)
{
	int x = 0;
	unsigned long flags;
	struct fxs *const fxs = &mod->mod.fxs;

	spin_lock_irqsave(&wc->reglock, flags);
	switch (txsig) {
	case DAHDI_TXSIG_ONHOOK:
		switch (get_dahdi_chan(wc, mod)->sig) {
		case DAHDI_SIG_FXOGS:
			x = (POLARITY_XOR(fxs)) ?
					SLIC_LF_RING_OPEN :
					SLIC_LF_TIP_OPEN;
			break;
		case DAHDI_SIG_EM:
		case DAHDI_SIG_FXOKS:
		case DAHDI_SIG_FXOLS:
		default:
			x = fxs->idletxhookstate;
			break;
		}
		break;
	case DAHDI_TXSIG_OFFHOOK:
		switch (get_dahdi_chan(wc, mod)->sig) {
		case DAHDI_SIG_EM:
			x = (POLARITY_XOR(fxs)) ?
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
		spin_unlock_irqrestore(&wc->reglock, flags);
		dev_notice(&wc->vb.pdev->dev,
			"wctdm24xxp: Can't set tx state to %d\n", txsig);
		return;
	}

	if (x != fxs->lasttxhook) {
		fxs->lasttxhook = x | SLIC_LF_OPPENDING;
		mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
		fxs->oppending_timeout = wc->framecount + 100;
		spin_unlock_irqrestore(&wc->reglock, flags);

		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev, "Setting FXS hook state "
				 "to %d (%02x) framecount=%ld\n", txsig, x,
				 wc->framecount);
		}
	} else {
		spin_unlock_irqrestore(&wc->reglock, flags);
	}
}

static void wctdm_fxs_off_hook(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;

	if (debug & DEBUG_CARD)
		dev_info(&wc->vb.pdev->dev,
			"fxs_off_hook: Card %d Going off hook\n", mod->card);
	switch (fxs->lasttxhook) {
	case SLIC_LF_RINGING:		/* Ringing */
	case SLIC_LF_OHTRAN_FWD:	/* Forward On Hook Transfer */
	case SLIC_LF_OHTRAN_REV:	/* Reverse On Hook Transfer */
		/* just detected OffHook, during Ringing or OnHookTransfer */
		fxs->idletxhookstate = POLARITY_XOR(fxs) ?
						SLIC_LF_ACTIVE_REV :
						SLIC_LF_ACTIVE_FWD;
		break;
	}
	if ((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)
		wctdm_fxs_hooksig(wc, mod, DAHDI_TXSIG_OFFHOOK);
	dahdi_hooksig(get_dahdi_chan(wc, mod), DAHDI_RXSIG_OFFHOOK);

#ifdef DEBUG
	if (robust)
		wctdm_init_proslic(wc, mod, 1, 0, 1);
#endif
}

/**
 * wctdm_fxs_on_hook - Report on hook to DAHDI.
 * @wc:		Board hosting the module.
 * @card:	Index of the module / port to place on hook.
 *
 * If we are intentionally dropping battery to signal a forward
 * disconnect we do not want to place the line "On-Hook". In this
 * case, the core of DAHDI will place us on hook when one of the RBS
 * timers expires.
 *
 */
static void wctdm_fxs_on_hook(struct wctdm *wc, struct wctdm_module *const mod)
{
	if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev,
			"fxs_on_hook: Card %d Going on hook\n", mod->card);
	}

	if ((mod->mod.fxs.lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)
		wctdm_fxs_hooksig(wc, mod, DAHDI_TXSIG_ONHOOK);
	dahdi_hooksig(get_dahdi_chan(wc, mod), DAHDI_RXSIG_ONHOOK);
}

static const char *wctdm_echocan_name(const struct dahdi_chan *chan)
{
	struct wctdm *wc = chan->pvt;
	if (wc->vpmadt032)
		return vpmadt032_name;
	else if (wc->vpmoct)
		return vpmoct_name;

	return NULL;
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
	enum adt_companding comp;


#ifdef VPM_SUPPORT
	if (!vpmsupport)
		return -ENODEV;
#endif
	if (wc->vpmadt032) {
		ops = &vpm_ec_ops;
		features = &vpm_ec_features;

		*ec = &wchan->ec;
		(*ec)->ops = ops;
		(*ec)->features = *features;

		comp = (DAHDI_LAW_ALAW == chan->span->deflaw) ?
					ADT_COMP_ALAW : ADT_COMP_ULAW;

		return vpmadt032_echocan_create(wc->vpmadt032, wchan->timeslot,
						comp, ecp, p);
	} else if (wc->vpmoct) {
		ops = &vpm_ec_ops;
		features = &vpm_ec_features;

		*ec = &wchan->ec;
		(*ec)->ops = ops;
		(*ec)->features = *features;
		return vpmoct_echocan_create(wc->vpmoct, wchan->timeslot,
					     chan->span->deflaw);
	} else {
		return -ENODEV;
	}
}

static void echocan_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct wctdm *wc = chan->pvt;
	struct wctdm_chan *wchan = container_of(chan, struct wctdm_chan, chan);

	if (wc->vpmadt032) {
		memset(ec, 0, sizeof(*ec));
		vpmadt032_echocan_free(wc->vpmadt032, wchan->timeslot, ec);
	} else if (wc->vpmoct) {
		memset(ec, 0, sizeof(*ec));
		vpmoct_echocan_free(wc->vpmoct, wchan->timeslot);
	}
}

/* 1ms interrupt */
static void wctdm_isr_misc_fxs(struct wctdm *wc, struct wctdm_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;

	if (time_after(wc->framecount, fxs->check_alarm)) {
		/* Accept an alarm once per 10 seconds */
		fxs->check_alarm = wc->framecount + (1000*10);
		if (fxs->palarms)
			fxs->palarms--;
	}

	if (fxs->off_hook && !(fxs->hook_state_shadow & 1)) {
		wctdm_fxs_on_hook(wc, mod);
		fxs->off_hook = 0;
	} else if (!fxs->off_hook && (fxs->hook_state_shadow & 1)) {
		wctdm_fxs_off_hook(wc, mod);
		fxs->off_hook = 1;
	}

	wctdm_proslic_check_oppending(wc, mod);

	if (time_after(wc->framecount, fxs->check_proslic)) {
		fxs->check_proslic = wc->framecount + 250; /* every 250ms */
		wctdm_proslic_recheck_sanity(wc, mod);
	}

	if (SLIC_LF_RINGING == fxs->lasttxhook) {
		/* RINGing, prepare for OHT */
		fxs->ohttimer = wc->framecount + OHT_TIMER;
		/* OHT mode when idle */
		fxs->idletxhookstate = POLARITY_XOR(fxs) ? SLIC_LF_OHTRAN_REV :
							    SLIC_LF_OHTRAN_FWD;
	} else if (fxs->oht_active) {
		/* check if still OnHook */
		if (!fxs->off_hook) {
			if (time_before(wc->framecount, fxs->ohttimer))
				return;

			/* Switch to active */
			fxs->idletxhookstate = POLARITY_XOR(fxs) ? SLIC_LF_ACTIVE_REV :
								    SLIC_LF_ACTIVE_FWD;
			spin_lock(&wc->reglock);
			if (SLIC_LF_OHTRAN_FWD == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING | SLIC_LF_ACTIVE_FWD;
				/* Data enqueued here */
				mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d OnHookTransfer "
						 "stop\n", mod->card);
				}
			} else if (SLIC_LF_OHTRAN_REV == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING | SLIC_LF_ACTIVE_REV;
				/* Data enqueued here */
				mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->vb.pdev->dev,
						 "Channel %d OnHookTransfer "
						 "stop\n", mod->card);
				}
			}
			spin_unlock(&wc->reglock);
		} else {
			fxs->oht_active = 0;
			/* Switch to active */
			fxs->idletxhookstate = POLARITY_XOR(fxs) ? SLIC_LF_ACTIVE_REV : SLIC_LF_ACTIVE_FWD;
			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "Channel %d OnHookTransfer abort\n",
					 mod->card);
			}
		}

	}
}

/* 1ms interrupt */
static inline void wctdm_isr_misc(struct wctdm *wc)
{
	int x;

	if (unlikely(!is_initialized(wc)))
		return;

	for (x = 0; x < wc->mods_per_board; x++) {
		struct wctdm_module *const mod = &wc->mods[x];

		spin_lock(&wc->reglock);
		cmd_checkisr(wc, mod);
		spin_unlock(&wc->reglock);

		switch (mod->type) {
		case FXS:
			wctdm_isr_misc_fxs(wc, mod);
			break;
		case FXO:
			wctdm_voicedaa_check_hook(wc, mod);
			break;
		case QRV:
			wctdm_qrvdri_check_hook(wc, x);
			break;
		default:
			break;
		}
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
		wc->framecount++;
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

static int wctdm_voicedaa_insane(struct wctdm *wc, struct wctdm_module *mod)
{
	int blah;
	blah = wctdm_getreg(wc, mod, 2);
	if (blah != 0x3)
		return -2;
	blah = wctdm_getreg(wc, mod, 11);
	if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev,
			 "VoiceDAA System: %02x\n", blah & 0xf);
	}
	return 0;
}

static int
wctdm_proslic_insane(struct wctdm *wc, struct wctdm_module *const mod)
{
	int blah, reg1, insane_report;
	insane_report=0;

	blah = wctdm_getreg(wc, mod, 0);
	if (blah != 0xff && (debug & DEBUG_CARD)) {
		dev_info(&wc->vb.pdev->dev,
			 "ProSLIC on module %d, product %d, "
			 "version %d\n", mod->card, (blah & 0x30) >> 4,
			 (blah & 0xf));
	}

#if 0
	if ((blah & 0x30) >> 4) {
		dev_info(&wc->vb.pdev->dev,
			 "ProSLIC on module %d is not a 3210.\n", mod->card);
		return -1;
	}
#endif
	if (((blah & 0xf) == 0) || ((blah & 0xf) == 0xf)) {
		/* SLIC not loaded */
		return -1;
	}

	/* let's be really sure this is an FXS before we continue */
	reg1 = wctdm_getreg(wc, mod, 1);
	if ((0x80 != (blah & 0xf0)) || (0x88 != reg1)) {
		if (debug & DEBUG_CARD) {
			dev_info(&wc->vb.pdev->dev,
				 "DEBUG: not FXS b/c reg0=%x or "
				 "reg1 != 0x88 (%x).\n", blah, reg1);
		}
		return -1;
	}

	blah = wctdm_getreg(wc, mod, 8);
	if (blah != 0x2) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d insane (1) %d should be 2\n",
			   mod->card, blah);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d Reg 8 Reads %d Expected "
			   "is 0x2\n", mod->card, blah);
	}

	blah = wctdm_getreg(wc, mod, 64);
	if (blah != 0x0) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d insane (2)\n",
			   mod->card);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d Reg 64 Reads %d Expected "
			   "is 0x0\n", mod->card, blah);
	}

	blah = wctdm_getreg(wc, mod, 11);
	if (blah != 0x33) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d insane (3)\n", mod->card);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->vb.pdev->dev,
			   "ProSLIC on module %d Reg 11 Reads %d "
			   "Expected is 0x33\n", mod->card, blah);
	}

	/* Just be sure it's setup right. */
	wctdm_setreg(wc, mod, 30, 0);

	if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev,
			 "ProSLIC on module %d seems sane.\n", mod->card);
	}
	return 0;
}

static int
wctdm_proslic_powerleak_test(struct wctdm *wc, struct wctdm_module *const mod)
{
	unsigned long start;
	unsigned char vbat;

	/* Turn off linefeed */
	wctdm_setreg(wc, mod, LINE_STATE, 0);

	/* Power down */
	wctdm_setreg(wc, mod, 14, 0x10);

	start = jiffies;

	/* TODO: Why is this sleep necessary.  Without it, the first read
	 * comes back with a 0 value. */
	msleep(20);

	while ((vbat = wctdm_getreg(wc, mod, 82)) > 0x6) {
		if (time_after(jiffies, start + HZ/4))
			break;
	}

	if (vbat < 0x06) {
		dev_notice(&wc->vb.pdev->dev,
			   "Excessive leakage detected on module %d: %d "
			   "volts (%02x) after %d ms\n", mod->card,
			   376 * vbat / 1000, vbat,
			   (int)((jiffies - start) * 1000 / HZ));
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev,
			 "Post-leakage voltage: %d volts\n", 376 * vbat / 1000);
	}
	return 0;
}

static int wctdm_powerup_proslic(struct wctdm *wc,
				 struct wctdm_module *mod, int fast)
{
	unsigned char vbat;
	unsigned long origjiffies;
	int lim;

	/* Set period of DC-DC converter to 1/64 khz */
	wctdm_setreg(wc, mod, 92, 0xc0 /* was 0xff */);

	/* Wait for VBat to powerup */
	origjiffies = jiffies;

	/* Disable powerdown */
	wctdm_setreg(wc, mod, 14, 0);

	/* If fast, don't bother checking anymore */
	if (fast)
		return 0;

	while ((vbat = wctdm_getreg(wc, mod, 82)) < 0xc0) {
		/* Wait no more than 500ms */
		if ((jiffies - origjiffies) > HZ/2) {
			break;
		}
	}

	if (vbat < 0xc0) {
		dev_notice(&wc->vb.pdev->dev, "ProSLIC on module %d failed to powerup within %d ms (%d mV only)\n\n -- DID YOU REMEMBER TO PLUG IN THE HD POWER CABLE TO THE TDM CARD??\n",
		       mod->card, (int)(((jiffies - origjiffies) * 1000 / HZ)),
			vbat * 375);
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev,
			 "ProSLIC on module %d powered up to -%d volts (%02x) "
			 "in %d ms\n", mod->card, vbat * 376 / 1000, vbat,
			 (int)(((jiffies - origjiffies) * 1000 / HZ)));
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
	wctdm_setreg(wc, mod, LOOP_I_LIMIT, lim);

	/* Engage DC-DC converter */
	wctdm_setreg(wc, mod, 93, 0x19 /* was 0x19 */);
	return 0;

}

static int
wctdm_proslic_manual_calibrate(struct wctdm *wc, struct wctdm_module *const mod)
{
	unsigned long origjiffies;
	unsigned char i;

	/* Disable all interupts in DR21-23 */
	wctdm_setreg(wc, mod, 21, 0);
	wctdm_setreg(wc, mod, 22, 0);
	wctdm_setreg(wc, mod, 23, 0);

	wctdm_setreg(wc, mod, 64, 0);

	/* (0x18) Calibrations without the ADC and DAC offset and without
	 * common mode calibration. */
	wctdm_setreg(wc, mod, 97, 0x18);

	/* (0x47) Calibrate common mode and differential DAC mode DAC + ILIM */
	wctdm_setreg(wc, mod, 96, 0x47);

	origjiffies=jiffies;
	while (wctdm_getreg(wc, mod, 96) != 0) {
		if ((jiffies-origjiffies) > 80)
			return -1;
	}
//Initialized DR 98 and 99 to get consistant results.
// 98 and 99 are the results registers and the search should have same intial conditions.

/*******************************The following is the manual gain mismatch calibration****************************/
/*******************************This is also available as a function *******************************************/
	msleep(10);
	wctdm_proslic_setreg_indirect(wc, mod, 88, 0);
	wctdm_proslic_setreg_indirect(wc, mod, 89, 0);
	wctdm_proslic_setreg_indirect(wc, mod, 90, 0);
	wctdm_proslic_setreg_indirect(wc, mod, 91, 0);
	wctdm_proslic_setreg_indirect(wc, mod, 92, 0);
	wctdm_proslic_setreg_indirect(wc, mod, 93, 0);

	/* This is necessary if the calibration occurs other than at reset */
	wctdm_setreg(wc, mod, 98, 0x10);
	wctdm_setreg(wc, mod, 99, 0x10);

	for ( i=0x1f; i>0; i--)
	{
		wctdm_setreg(wc, mod, 98, i);
		msleep(40);
		if ((wctdm_getreg(wc, mod, 88)) == 0)
			break;
	} // for

	for ( i=0x1f; i>0; i--)
	{
		wctdm_setreg(wc, mod, 99, i);
		msleep(40);
		if ((wctdm_getreg(wc, mod, 89)) == 0)
			break;
	}//for

/*******************************The preceding is the manual gain mismatch calibration****************************/
/**********************************The following is the longitudinal Balance Cal***********************************/
	wctdm_setreg(wc, mod, 64, 1);
	msleep(100);

	wctdm_setreg(wc, mod, 64, 0);

	/* enable interrupt for the balance Cal */
	wctdm_setreg(wc, mod, 23, 0x4);

	/* this is a singular calibration bit for longitudinal calibration */
	wctdm_setreg(wc, mod, 97, 0x1);
	wctdm_setreg(wc, mod, 96, 0x40);

	wctdm_getreg(wc, mod, 96); /* Read Reg 96 just cause */

	wctdm_setreg(wc, mod, 21, 0xFF);
	wctdm_setreg(wc, mod, 22, 0xFF);
	wctdm_setreg(wc, mod, 23, 0xFF);

	/**The preceding is the longitudinal Balance Cal***/
	return(0);

}

static int
wctdm_proslic_calibrate(struct wctdm *wc, struct wctdm_module *mod)
{
	unsigned long origjiffies;
	int x;

	/* Perform all calibrations */
	wctdm_setreg(wc, mod, 97, 0x1f);
	
	/* Begin, no speedup */
	wctdm_setreg(wc, mod, 96, 0x5f);

	/* Wait for it to finish */
	origjiffies = jiffies;
	while (wctdm_getreg(wc, mod, 96)) {
		if (time_after(jiffies, (origjiffies + (2*HZ)))) {
			dev_notice(&wc->vb.pdev->dev,
				   "Timeout waiting for calibration of "
				   "module %d\n", mod->card);
			return -1;
		}
	}
	
	if (debug & DEBUG_CARD) {
		/* Print calibration parameters */
		dev_info(&wc->vb.pdev->dev,
			 "Calibration Vector Regs 98 - 107:\n");
		for (x=98;x<108;x++) {
			dev_info(&wc->vb.pdev->dev,
				 "%d: %02x\n", x, wctdm_getreg(wc, mod, x));
		}
	}
	return 0;
}

/*********************************************************************
 * Set the hwgain on the analog modules
 *
 * card = the card position for this module (0-23)
 * gain = gain in dB x10 (e.g. -3.5dB  would be gain=-35)
 * tx = (0 for rx; 1 for tx)
 *
 *******************************************************************/
static int
wctdm_set_hwgain(struct wctdm *wc, struct wctdm_module *mod,
		 __s32 gain, __u32 tx)
{
	if (mod->type != FXO) {
		dev_notice(&wc->vb.pdev->dev,
			   "Cannot adjust gain. Unsupported module type!\n");
		return -1;
	}

	if (tx) {
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "setting FXO tx gain for card=%d to %d\n",
				 mod->card, gain);
		}
		if (gain >=  -150 && gain <= 0) {
			wctdm_setreg(wc, mod, 38, 16 + (gain / -10));
			wctdm_setreg(wc, mod, 40, 16 + (-gain % 10));
		} else if (gain <= 120 && gain > 0) {
			wctdm_setreg(wc, mod, 38, gain/10);
			wctdm_setreg(wc, mod, 40, (gain%10));
		} else {
			dev_notice(&wc->vb.pdev->dev,
				   "FXO tx gain is out of range (%d)\n", gain);
			return -1;
		}
	} else { /* rx */
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "setting FXO rx gain for card=%d to %d\n",
				 mod->card, gain);
		}
		if (gain >=  -150 && gain <= 0) {
			wctdm_setreg(wc, mod, 39, 16 + (gain / -10));
			wctdm_setreg(wc, mod, 41, 16 + (-gain % 10));
		} else if (gain <= 120 && gain > 0) {
			wctdm_setreg(wc, mod, 39, gain/10);
			wctdm_setreg(wc, mod, 41, (gain%10));
		} else {
			dev_notice(&wc->vb.pdev->dev,
				   "FXO rx gain is out of range (%d)\n", gain);
			return -1;
		}
	}

	return 0;
}

static int set_lasttxhook_interruptible(struct wctdm *wc, struct fxs *fxs,
					unsigned newval, int *psethook)
{
	int res = 0;
	unsigned long flags;
	int timeout = 0;

	do {
		spin_lock_irqsave(&wc->reglock, flags);
		if (SLIC_LF_OPPENDING & fxs->lasttxhook) {
			spin_unlock_irqrestore(&wc->reglock, flags);
			if (timeout++ > 100)
				return -1;
			msleep(1);
		} else {
			fxs->lasttxhook = (newval & SLIC_LF_SETMASK) | SLIC_LF_OPPENDING;
			*psethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		}
	} while (1);

	return res;
}

/* Must be called from within an interruptible context */
static int set_vmwi(struct wctdm *wc, struct wctdm_module *const mod)
{
	int x;
	struct fxs *const fxs = &mod->mod.fxs;

	/* Presently only supports line reversal MWI */
	if ((fxs->vmwi_active_messages) &&
	    (fxs->vmwisetting.vmwi_type & DAHDI_VMWI_LREV))
		fxs->vmwi_linereverse = 1;
	else
		fxs->vmwi_linereverse = 0;

	/* Set line polarity for new VMWI state */
	if (POLARITY_XOR(fxs)) {
		fxs->idletxhookstate |= SLIC_LF_REVMASK;
		/* Do not set while currently ringing or open */
		if (((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_RINGING)  &&
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)) {
			x = fxs->lasttxhook;
			x |= SLIC_LF_REVMASK;
			set_lasttxhook_interruptible(wc, fxs, x, &mod->sethook);
		}
	} else {
		fxs->idletxhookstate &= ~SLIC_LF_REVMASK;
		/* Do not set while currently ringing or open */
		if (((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_RINGING) &&
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)) {
			x = fxs->lasttxhook;
			x &= ~SLIC_LF_REVMASK;
			set_lasttxhook_interruptible(wc, fxs, x, &mod->sethook);
		}
	}
	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "Setting VMWI on channel %d, messages=%d, lrev=%d\n",
			 mod->card, fxs->vmwi_active_messages,
			 fxs->vmwi_linereverse);
	}
	return 0;
}

static void
wctdm_voicedaa_set_ts(struct wctdm *wc, struct wctdm_module *mod, int ts)
{
	wctdm_setreg(wc, mod, 34, (ts * 8) & 0xff);
	wctdm_setreg(wc, mod, 35, (ts * 8) >> 8);
	wctdm_setreg(wc, mod, 36, (ts * 8) & 0xff);
	wctdm_setreg(wc, mod, 37, (ts * 8) >> 8);

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "voicedaa: card %d new timeslot: %d\n",
			 mod->card + 1, ts);
	}
}

static int
wctdm_init_voicedaa(struct wctdm *wc, struct wctdm_module *mod,
		    int fast, int manual, int sane)
{
	unsigned char reg16=0, reg26=0, reg30=0, reg31=0;
	unsigned long flags;
	long newjiffies;

#if 0	/* TODO */
	if ((wc->mods[card & 0xfc].type == QRV) ||
	    (wc->mods[card & 0xfc].type == BRI))
		return -2;
#endif

	spin_lock_irqsave(&wc->reglock, flags);
	mod->type = NONE;
	spin_unlock_irqrestore(&wc->reglock, flags);
	msleep(20);

	spin_lock_irqsave(&wc->reglock, flags);
	mod->type = FXO;
	spin_unlock_irqrestore(&wc->reglock, flags);
	msleep(20);

	if (!sane && wctdm_voicedaa_insane(wc, mod))
		return -2;

	/* Software reset */
	wctdm_setreg(wc, mod, 1, 0x80);
	msleep(100);

	/* Set On-hook speed, Ringer impedence, and ringer threshold */
	reg16 |= (fxo_modes[_opermode].ohs << 6);
	reg16 |= (fxo_modes[_opermode].rz << 1);
	reg16 |= (fxo_modes[_opermode].rt);
	wctdm_setreg(wc, mod, 16, reg16);

	/* Enable ring detector full-wave rectifier mode */
	wctdm_setreg(wc, mod, 18, 2);
	wctdm_setreg(wc, mod, 24, 0);
	
	/* Set DC Termination:
	   Tip/Ring voltage adjust, minimum operational current, current limitation */
	reg26 |= (fxo_modes[_opermode].dcv << 6);
	reg26 |= (fxo_modes[_opermode].mini << 4);
	reg26 |= (fxo_modes[_opermode].ilim << 1);
	wctdm_setreg(wc, mod, 26, reg26);

	/* Set AC Impedence */
	reg30 = (fxo_modes[_opermode].acim);
	wctdm_setreg(wc, mod, 30, reg30);

	/* Misc. DAA parameters */

	/* If fast pickup is set, then the off hook counter will be set to 8
	 * ms, otherwise 128 ms. */
	reg31 = (fastpickup) ? 0xe3 : 0xa3;

	reg31 |= (fxo_modes[_opermode].ohs2 << 3);
	wctdm_setreg(wc, mod, 31, reg31);

	wctdm_voicedaa_set_ts(wc, mod, mod->card);

	/* Enable ISO-Cap */
	wctdm_setreg(wc, mod, 6, 0x00);

	/* Turn off the calibration delay when fastpickup is enabled. */
	if (fastpickup)
		wctdm_setreg(wc, mod, 17, wctdm_getreg(wc, mod, 17) | 0x20);

	/* Wait 1000ms for ISO-cap to come up */
	newjiffies = jiffies;
	newjiffies += 2 * HZ;

	while ((jiffies < newjiffies) && !(wctdm_getreg(wc, mod, 11) & 0xf0))
		msleep(100);

	if (!(wctdm_getreg(wc, mod, 11) & 0xf0)) {
		dev_notice(&wc->vb.pdev->dev, "VoiceDAA did not bring up ISO link properly!\n");
		return -1;
	}

	if (debug & DEBUG_CARD) {
		dev_info(&wc->vb.pdev->dev, "ISO-Cap is now up, line side: %02x rev %02x\n", 
		       wctdm_getreg(wc, mod, 11) >> 4,
		       (wctdm_getreg(wc, mod, 13) >> 2) & 0xf);
	}

	/* Enable on-hook line monitor */
	wctdm_setreg(wc, mod, 5, 0x08);
	
	/* Take values for fxotxgain and fxorxgain and apply them to module */
	wctdm_set_hwgain(wc, mod, fxotxgain, 1);
	wctdm_set_hwgain(wc, mod, fxorxgain, 0);

#ifdef DEBUG
	if (digitalloopback) {
		dev_info(&wc->vb.pdev->dev,
			 "Turning on digital loopback for port %d.\n",
			 mod->card + 1);
		wctdm_setreg(wc, mod, 10, 0x01);
	}
#endif

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "DEBUG fxotxgain:%i.%i fxorxgain:%i.%i\n",
			 (wctdm_getreg(wc, mod, 38)/16) ?
				-(wctdm_getreg(wc, mod, 38) - 16) :
				wctdm_getreg(wc, mod, 38),
			 (wctdm_getreg(wc, mod, 40)/16) ?
				-(wctdm_getreg(wc, mod, 40) - 16) :
				wctdm_getreg(wc, mod, 40),
			 (wctdm_getreg(wc, mod, 39)/16) ?
				-(wctdm_getreg(wc, mod, 39) - 16) :
				wctdm_getreg(wc, mod, 39),
			 (wctdm_getreg(wc, mod, 41)/16) ?
				-(wctdm_getreg(wc, mod, 41) - 16) :
				wctdm_getreg(wc, mod, 41));
	}
	
	return 0;
}

static void
wctdm_proslic_set_ts(struct wctdm *wc, struct wctdm_module *mod, int ts)
{
	wctdm_setreg(wc, mod, 2, (ts * 8) & 0xff); /* Tx Start low byte  0 */
	wctdm_setreg(wc, mod, 3, (ts * 8) >> 8);   /* Tx Start high byte 0 */
	wctdm_setreg(wc, mod, 4, (ts * 8) & 0xff); /* Rx Start low byte  0 */
	wctdm_setreg(wc, mod, 5, (ts * 8) >> 8);   /* Rx Start high byte 0 */

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "proslic: card %d new timeslot: %d\n",
			 mod->card + 1, ts);
	}
}

static int
wctdm_init_proslic(struct wctdm *wc, struct wctdm_module *const mod,
		   int fast, int manual, int sane)
{

	struct fxs *const fxs = &mod->mod.fxs;
	unsigned short tmp[5];
	unsigned long flags;
	unsigned char r19,r9;
	int x;
	int fxsmode=0;
	int addresses[NUM_CAL_REGS];

#if 0 /* TODO */
	if (wc->mods[mod->card & 0xfc].type == QRV)
		return -2;
#endif

	spin_lock_irqsave(&wc->reglock, flags);
	mod->type = FXS;
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* msleep(100); */

	/* Sanity check the ProSLIC */
	if (!sane && wctdm_proslic_insane(wc, mod))
		return -2;

	/* Initialize VMWI settings */
	memset(&(fxs->vmwisetting), 0, sizeof(fxs->vmwisetting));
	fxs->vmwi_linereverse = 0;

	/* By default, don't send on hook */
	if (!reversepolarity != !fxs->reversepolarity)
		fxs->idletxhookstate = SLIC_LF_ACTIVE_REV;
	else
		fxs->idletxhookstate = SLIC_LF_ACTIVE_FWD;

	if (sane) {
		/* Make sure we turn off the DC->DC converter to prevent anything from blowing up */
		wctdm_setreg(wc, mod, 14, 0x10);
	}

	if (wctdm_proslic_init_indirect_regs(wc, mod)) {
		dev_info(&wc->vb.pdev->dev,
			 "Indirect Registers failed to initialize on "
			 "module %d.\n", mod->card);
		return -1;
	}

	/* Clear scratch pad area */
	wctdm_proslic_setreg_indirect(wc, mod, 97, 0);

	/* Clear digital loopback */
	wctdm_setreg(wc, mod, 8, 0);

	/* Revision C optimization */
	wctdm_setreg(wc, mod, 108, 0xeb);

	/* Disable automatic VBat switching for safety to prevent
	 * Q7 from accidently turning on and burning out.
	 * If pulse dialing has trouble at high REN loads change this to 0x17 */
	wctdm_setreg(wc, mod, 67, 0x07);

	/* Turn off Q7 */
	wctdm_setreg(wc, mod, 66, 1);

	/* Flush ProSLIC digital filters by setting to clear, while
	   saving old values */
	for (x=0;x<5;x++) {
		tmp[x] = wctdm_proslic_getreg_indirect(wc, mod, x + 35);
		wctdm_proslic_setreg_indirect(wc, mod, x + 35, 0x8000);
	}

	/* Power up the DC-DC converter */
	if (wctdm_powerup_proslic(wc, mod, fast)) {
		dev_notice(&wc->vb.pdev->dev,
			   "Unable to do INITIAL ProSLIC powerup on "
			   "module %d\n", mod->card);
		return -1;
	}

	if (!fast) {
		/* Check for power leaks */
		if (wctdm_proslic_powerleak_test(wc, mod)) {
			dev_notice(&wc->vb.pdev->dev,
				   "ProSLIC module %d failed leakage test. "
				   "Check for short circuit\n", mod->card);
		}
		/* Power up again */
		if (wctdm_powerup_proslic(wc, mod, fast)) {
			dev_notice(&wc->vb.pdev->dev,
				   "Unable to do FINAL ProSLIC powerup on "
				   "module %d\n", mod->card);
			return -1;
		}
#ifndef NO_CALIBRATION
		/* Perform calibration */
		if (manual) {
			if (wctdm_proslic_manual_calibrate(wc, mod)) {
				//dev_notice(&wc->vb.pdev->dev, "Proslic failed on Manual Calibration\n");
				if (wctdm_proslic_manual_calibrate(wc, mod)) {
					dev_notice(&wc->vb.pdev->dev, "Proslic Failed on Second Attempt to Calibrate Manually. (Try -DNO_CALIBRATION in Makefile)\n");
					return -1;
				}
				dev_info(&wc->vb.pdev->dev, "Proslic Passed Manual Calibration on Second Attempt\n");
			}
		}
		else {
			if (wctdm_proslic_calibrate(wc, mod))  {
				//dev_notice(&wc->vb.pdev->dev, "ProSlic died on Auto Calibration.\n");
				if (wctdm_proslic_calibrate(wc, mod)) {
					dev_notice(&wc->vb.pdev->dev, "Proslic Failed on Second Attempt to Auto Calibrate\n");
					return -1;
				}
				dev_info(&wc->vb.pdev->dev, "Proslic Passed Auto Calibration on Second Attempt\n");
			}
		}
		/* Perform DC-DC calibration */
		wctdm_setreg(wc, mod, 93, 0x99);
		r19 = wctdm_getreg(wc, mod, 107);
		if ((r19 < 0x2) || (r19 > 0xd)) {
			dev_notice(&wc->vb.pdev->dev, "DC-DC cal has a surprising direct 107 of 0x%02x!\n", r19);
			wctdm_setreg(wc, mod, 107, 0x8);
		}

		/* Save calibration vectors */
		for (x = 0; x < NUM_CAL_REGS; x++)
			addresses[x] = 96 + x;
		wctdm_getregs(wc, mod, addresses, ARRAY_SIZE(addresses));
		for (x = 0; x < NUM_CAL_REGS; x++)
			fxs->calregs.vals[x] = addresses[x];
#endif

	} else {
		/* Restore calibration registers */
		for (x = 0; x < NUM_CAL_REGS; x++)
			wctdm_setreg(wc, mod, 96 + x, fxs->calregs.vals[x]);
	}
	/* Calibration complete, restore original values */
	for (x=0;x<5;x++) {
		wctdm_proslic_setreg_indirect(wc, mod, x + 35, tmp[x]);
	}

	if (wctdm_proslic_verify_indirect_regs(wc, mod)) {
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
	wctdm_proslic_set_ts(wc, mod, mod->card);

	wctdm_setreg(wc, mod, 18, 0xff); /* clear all interrupt */
	wctdm_setreg(wc, mod, 19, 0xff);
	wctdm_setreg(wc, mod, 20, 0xff);
	wctdm_setreg(wc, mod, 22, 0xff);
	wctdm_setreg(wc, mod, 73, 0x04);

	wctdm_setreg(wc, mod, 69, 0x4);

	if (fxshonormode) {
		static const int ACIM2TISS[16] = { 0x0, 0x1, 0x4, 0x5, 0x7,
						   0x0, 0x0, 0x6, 0x0, 0x0,
						   0x0, 0x2, 0x0, 0x3 };
		fxsmode = ACIM2TISS[fxo_modes[_opermode].acim];
		wctdm_setreg(wc, mod, 10, 0x08 | fxsmode);
		if (fxo_modes[_opermode].ring_osc) {
			wctdm_proslic_setreg_indirect(wc, mod, 20,
					fxo_modes[_opermode].ring_osc);
		}
		if (fxo_modes[_opermode].ring_x) {
			wctdm_proslic_setreg_indirect(wc, mod, 21,
					fxo_modes[_opermode].ring_x);
		}
	}
	if (lowpower)
		wctdm_setreg(wc, mod, 72, 0x10);

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
		wctdm_proslic_setreg_indirect(wc, mod, 20, 0x7e6d);
		wctdm_proslic_setreg_indirect(wc, mod, 21, 0x01b9);
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wctdm_setreg(wc, mod, 74, 0x3f);
			if (wctdm_proslic_setreg_indirect(wc, mod, 21, 0x247))
				return -1;
			dev_info(&wc->vb.pdev->dev,
				 "Boosting fast ringer on slot %d (89V peak)\n",
				 mod->card + 1);
		} else if (lowpower) {
			if (wctdm_proslic_setreg_indirect(wc, mod, 21, 0x14b))
				return -1;
			dev_info(&wc->vb.pdev->dev,
				 "Reducing fast ring power on slot %d "
				 "(50V peak)\n", mod->card + 1);
		} else
			dev_info(&wc->vb.pdev->dev,
				 "Speeding up ringer on slot %d (25Hz)\n",
				 mod->card + 1);
	} else {
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wctdm_setreg(wc, mod, 74, 0x3f);
			if (wctdm_proslic_setreg_indirect(wc, mod, 21, 0x1d1))
				return -1;
			dev_info(&wc->vb.pdev->dev,
				 "Boosting ringer on slot %d (89V peak)\n",
				 mod->card + 1);
		} else if (lowpower) {
			if (wctdm_proslic_setreg_indirect(wc, mod, 21, 0x108))
				return -1;
			dev_info(&wc->vb.pdev->dev,
				 "Reducing ring power on slot %d "
				 "(50V peak)\n", mod->card + 1);
		}
	}

	if (fxstxgain || fxsrxgain) {
		r9 = wctdm_getreg(wc, mod, 9);
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
		wctdm_setreg(wc, mod, 9, r9);
	}

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "DEBUG: fxstxgain:%s fxsrxgain:%s\n",
			 ((wctdm_getreg(wc, mod, 9) / 8) == 1) ?
				"3.5" : (((wctdm_getreg(wc, mod, 9) / 4) == 1) ?
							"-3.5" : "0.0"),
			 ((wctdm_getreg(wc, mod, 9) / 2) == 1) ?
				"3.5" : ((wctdm_getreg(wc, mod, 9) % 2) ?
							"-3.5" : "0.0"));
	}

	fxs->lasttxhook = fxs->idletxhookstate;
	wctdm_setreg(wc, mod, LINE_STATE, fxs->lasttxhook);

	/* Preset the shadow register so that we won't get a power alarm when
	 * we finish initialization, otherwise the line state register may not
	 * have been read yet. */
	fxs->linefeed_control_shadow = fxs->lasttxhook;
	return 0;
}

static void
wctdm_qrvdri_set_ts(struct wctdm *wc, struct wctdm_module *mod, int ts)
{
	wctdm_setreg(wc, mod, 0x13, ts + 0x80);	/* codec 2 tx, ts0 */
	wctdm_setreg(wc, mod, 0x17, ts + 0x80);	/* codec 0 rx, ts0 */
	wctdm_setreg(wc, mod, 0x14, ts + 0x81);	/* codec 1 tx, ts1 */
	wctdm_setreg(wc, mod, 0x18, ts + 0x81);	/* codec 1 rx, ts1 */

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "qrvdri: card %d new timeslot: %d\n",
			 mod->card + 1, ts);
	}
}

static int wctdm_init_qrvdri(struct wctdm *wc, int card)
{
	struct wctdm_module *const mod = &wc->mods[card];
	unsigned char x,y;

	if (BRI == wc->mods[card & 0xfc].type)
		return -2;

	/* have to set this, at least for now */
	mod->type = QRV;
	if (!(card & 3)) { /* if at base of card, reset and write it */
		struct qrv *const qrv = &mod->mod.qrv;
		struct qrv *const qrv1 = &wc->mods[card + 1].mod.qrv;
		struct qrv *const qrv2 = &wc->mods[card + 2].mod.qrv;
		struct qrv *const qrv3 = &wc->mods[card + 3].mod.qrv;

		wctdm_setreg(wc, mod, 0, 0x80);
		wctdm_setreg(wc, mod, 0, 0x55);
		wctdm_setreg(wc, mod, 1, 0x69);
		qrv->hook = qrv1->hook = 0;
		qrv2->hook = qrv3->hook = 0xff;
		qrv->debouncetime = qrv1->debouncetime = QRV_DEBOUNCETIME;
		qrv->debtime = qrv1->debtime = 0;
		qrv->radmode = qrv1->radmode = 0;
		qrv->txgain = qrv1->txgain = 3599;
		qrv->rxgain = qrv1->rxgain = 1199;
	} else { /* channel is on same card as base, no need to test */
		if (wc->mods[card & 0x7c].type == QRV) {
			/* only lower 2 are valid */
			if (!(card & 2))
				return 0;
		}
		mod->type = NONE;
		return 1;
	}
	x = wctdm_getreg(wc, mod, 0);
	y = wctdm_getreg(wc, mod, 1);
	/* if not a QRV card, return as such */
	if ((x != 0x55) || (y != 0x69))
	{
		mod->type = NONE;
		return 1;
	}
	for (x = 0; x < 0x30; x++) {
		if ((x >= 0x1c) && (x <= 0x1e))
			wctdm_setreg(wc, mod, x, 0xff);
		else
			wctdm_setreg(wc, mod, x, 0);
	}
	wctdm_setreg(wc, mod, 0, 0x80);
	msleep(100);
	wctdm_setreg(wc, mod, 0, 0x10);
	wctdm_setreg(wc, mod, 0, 0x10);
	msleep(100);
	/* set up modes */
	wctdm_setreg(wc, mod, 0, 0x1c);
	/* set up I/O directions */
	wctdm_setreg(wc, mod, 1, 0x33);
	wctdm_setreg(wc, mod, 2, 0x0f);
	wctdm_setreg(wc, mod, 5, 0x0f);
	/* set up I/O to quiescent state */
	wctdm_setreg(wc, mod, 3, 0x11);  /* D0-7 */
	wctdm_setreg(wc, mod, 4, 0xa);  /* D8-11 */
	wctdm_setreg(wc, mod, 7, 0);  /* CS outputs */

	wctdm_qrvdri_set_ts(wc, mod, card);

	/* set up for max gains */
	wctdm_setreg(wc, mod, 0x26, 0x24);
	wctdm_setreg(wc, mod, 0x27, 0x24);
	wctdm_setreg(wc, mod, 0x0b, 0x01);  /* "Transmit" gain codec 0 */
	wctdm_setreg(wc, mod, 0x0c, 0x01);  /* "Transmit" gain codec 1 */
	wctdm_setreg(wc, mod, 0x0f, 0xff);  /* "Receive" gain codec 0 */
	wctdm_setreg(wc, mod, 0x10, 0xff);  /* "Receive" gain codec 1 */
	return 0;
}

static void qrv_dosetup(struct dahdi_chan *chan, struct wctdm *wc)
{
	struct wctdm_module *qrvmod;
	struct wctdm_module *nextqrvmod;
	int qrvcard;
	unsigned char r;
	long l;

	/* actually do something with the values */
	qrvcard = (chan->chanpos - 1) & 0xfc;
	qrvmod = &wc->mods[qrvcard];
	nextqrvmod = &wc->mods[qrvcard + 1];
	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "@@@@@ radmodes: %d,%d  rxgains: %d,%d   "
			 "txgains: %d,%d\n", wc->mods[qrvcard].mod.qrv.radmode,
			 nextqrvmod->mod.qrv.radmode,
			 wc->mods[qrvcard].mod.qrv.rxgain,
			 nextqrvmod->mod.qrv.rxgain,
			 wc->mods[qrvcard].mod.qrv.txgain,
			 nextqrvmod->mod.qrv.txgain);
	}
	r = 0;
	if (qrvmod->mod.qrv.radmode & RADMODE_DEEMP)
		r |= 4;
	if (nextqrvmod->mod.qrv.radmode & RADMODE_DEEMP)
		r |= 8;
	if (qrvmod->mod.qrv.rxgain < 1200)
		r |= 1;
	if (nextqrvmod->mod.qrv.rxgain < 1200)
		r |= 2;
	wctdm_setreg(wc, qrvmod, 7, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 7 to %02x hex\n",r);
	r = 0;
	if (qrvmod->mod.qrv.radmode & RADMODE_PREEMP)
		r |= 3;
	else if (qrvmod->mod.qrv.txgain >= 3600)
		r |= 1;
	else if (qrvmod->mod.qrv.txgain >= 1200)
		r |= 2;
	if (nextqrvmod->mod.qrv.radmode & RADMODE_PREEMP)
		r |= 0xc;
	else if (nextqrvmod->mod.qrv.txgain >= 3600)
		r |= 4;
	else if (nextqrvmod->mod.qrv.txgain >= 1200)
		r |= 8;
	wctdm_setreg(wc, qrvmod, 4, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 4 to %02x hex\n",r);
	r = 0;
	if (qrvmod->mod.qrv.rxgain >= 2400)
		r |= 1;
	if (nextqrvmod->mod.qrv.rxgain >= 2400)
		r |= 2;
	wctdm_setreg(wc, qrvmod, 0x25, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x25 to %02x hex\n",r);
	r = 0;
	if (qrvmod->mod.qrv.txgain < 2400)
		r |= 1;
	else
		r |= 4;
	if (nextqrvmod->mod.qrv.txgain < 2400)
		r |= 8;
	else
		r |= 0x20;
	wctdm_setreg(wc, qrvmod, 0x26, r);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x26 to %02x hex\n",r);
	l = ((long)(qrvmod->mod.qrv.rxgain % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	if (qrvmod->mod.qrv.rxgain >= 2400)
		l += 181;
	wctdm_setreg(wc, qrvmod, 0x0b, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0b to %02x hex\n",(unsigned char)l);
	l = ((long)(nextqrvmod->mod.qrv.rxgain % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	if (nextqrvmod->mod.qrv.rxgain >= 2400)
		l += 181;
	wctdm_setreg(wc, qrvmod, 0x0c, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0c to %02x hex\n",(unsigned char)l);
	l = ((long)(qrvmod->mod.qrv.txgain % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	wctdm_setreg(wc, qrvmod, 0x0f, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x0f to %02x hex\n", (unsigned char)l);
	l = ((long)(nextqrvmod->mod.qrv.txgain % 1200) * 10000) / 46875;
	if (l == 0) l = 1;
	wctdm_setreg(wc, qrvmod, 0x10, (unsigned char)l);
	if (debug) dev_info(&wc->vb.pdev->dev, "@@@@@ setting reg 0x10 to %02x hex\n",(unsigned char)l);
	return;
}

static void wctdm24xxp_get_fxs_regs(struct wctdm *wc, struct wctdm_module *mod,
				    struct wctdm_regs *regs)
{
	int  x;

	for (x = 0; x < NUM_INDIRECT_REGS; x++)
		regs->indirect[x] = wctdm_proslic_getreg_indirect(wc, mod, x);

	for (x = 0; x < NUM_REGS; x++)
		regs->direct[x] = wctdm_getreg(wc, mod, x);
}

static void wctdm24xxp_get_fxo_regs(struct wctdm *wc, struct wctdm_module *mod,
				    struct wctdm_regs *regs)
{
	int  x;
	for (x = 0; x < NUM_FXO_REGS; x++)
		regs->direct[x] = wctdm_getreg(wc, mod, x);
}

static void wctdm24xxp_get_qrv_regs(struct wctdm *wc, struct wctdm_module *mod,
				    struct wctdm_regs *regs)
{
	int  x;
	for (x = 0; x < 0x32; x++)
		regs->direct[x] = wctdm_getreg(wc, mod, x);
}

static int wctdm_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct wctdm_stats stats;
	struct wctdm_regop regop;
	struct wctdm_echo_coefs echoregs;
	struct dahdi_hwgain hwgain;
	struct wctdm *wc = chan->pvt;
	int x;
	union {
		struct dahdi_radio_stat s;
		struct dahdi_radio_param p;
	} stack;
	struct wctdm_module *const mod = &wc->mods[chan->chanpos - 1];
	struct fxs *const fxs = &mod->mod.fxs;

	switch (cmd) {
	case DAHDI_ONHOOKTRANSFER:
		if (mod->type != FXS)
			return -EINVAL;
		if (get_user(x, (__user int *) data))
			return -EFAULT;

		/* Active mode when idle */
		fxs->idletxhookstate = POLARITY_XOR(fxs) ?
						SLIC_LF_ACTIVE_REV :
						SLIC_LF_ACTIVE_FWD;

		if (((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_ACTIVE_FWD) ||
		    ((fxs->lasttxhook & SLIC_LF_SETMASK) == SLIC_LF_ACTIVE_REV)) {
			int res;

			res = set_lasttxhook_interruptible(wc, fxs,
				(POLARITY_XOR(fxs) ?
				SLIC_LF_OHTRAN_REV : SLIC_LF_OHTRAN_FWD),
				&mod->sethook);

			if (debug & DEBUG_CARD) {
				if (res) {
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

		fxs->ohttimer = wc->framecount + x;
		fxs->oht_active = 1;

		break;
	case DAHDI_VMWI_CONFIG:
		if (mod->type != FXS)
			return -EINVAL;
		if (copy_from_user(&(fxs->vmwisetting),
				   (__user void *)data,
				   sizeof(fxs->vmwisetting)))
			return -EFAULT;
		set_vmwi(wc, mod);
		break;
	case DAHDI_VMWI:
		if (mod->type != FXS)
			return -EINVAL;
		if (get_user(x, (__user int *) data))
			return -EFAULT;
		if (0 > x)
			return -EFAULT;
		fxs->vmwi_active_messages = x;
		set_vmwi(wc, mod);
		break;
	case WCTDM_GET_STATS:
		if (mod->type == FXS) {
			stats.tipvolt = wctdm_getreg(wc, mod, 80) * -376;
			stats.ringvolt = wctdm_getreg(wc, mod, 81) * -376;
			stats.batvolt = wctdm_getreg(wc, mod, 82) * -376;
		} else if (mod->type == FXO) {
			stats.tipvolt = (s8)wctdm_getreg(wc, mod, 29) * 1000;
			stats.ringvolt = (s8)wctdm_getreg(wc, mod, 29) * 1000;
			stats.batvolt = (s8)wctdm_getreg(wc, mod, 29) * 1000;
		} else 
			return -EINVAL;
		if (copy_to_user((__user void *) data, &stats, sizeof(stats)))
			return -EFAULT;
		break;
	case WCTDM_GET_REGS:
	{
		struct wctdm_regs *regs = kzalloc(sizeof(*regs), GFP_KERNEL);
		if (!regs)
			return -ENOMEM;

		if (mod->type == FXS)
			wctdm24xxp_get_fxs_regs(wc, mod, regs);
		else if (mod->type == QRV)
			wctdm24xxp_get_qrv_regs(wc, mod, regs);
		else
			wctdm24xxp_get_fxo_regs(wc, mod, regs);

		if (copy_to_user((__user void *)data, regs, sizeof(*regs))) {
			kfree(regs);
			return -EFAULT;
		}

		kfree(regs);
		break;
	}
	case WCTDM_SET_REG:
		if (copy_from_user(&regop, (__user void *) data, sizeof(regop)))
			return -EFAULT;
		if (regop.indirect) {
			if (mod->type != FXS)
				return -EINVAL;
			dev_info(&wc->vb.pdev->dev, "Setting indirect %d to 0x%04x on %d\n", regop.reg, regop.val, chan->chanpos);
			wctdm_proslic_setreg_indirect(wc, mod, regop.reg,
						      regop.val);
		} else {
			regop.val &= 0xff;
			if (regop.reg == LINE_STATE) {
				/* Set feedback register to indicate the new state that is being set */
				fxs->lasttxhook = (regop.val & 0x0f) |  SLIC_LF_OPPENDING;
			}
			dev_info(&wc->vb.pdev->dev, "Setting direct %d to %04x on %d\n", regop.reg, regop.val, chan->chanpos);
			wctdm_setreg(wc, mod, regop.reg, regop.val);
		}
		break;
	case WCTDM_SET_ECHOTUNE:
		dev_info(&wc->vb.pdev->dev, "-- Setting echo registers: \n");
		if (copy_from_user(&echoregs, (__user void *) data, sizeof(echoregs)))
			return -EFAULT;

		if (mod->type == FXO) {
			/* Set the ACIM register */
			wctdm_setreg(wc, mod, 30, echoregs.acim);

			/* Set the digital echo canceller registers */
			wctdm_setreg(wc, mod, 45, echoregs.coef1);
			wctdm_setreg(wc, mod, 46, echoregs.coef2);
			wctdm_setreg(wc, mod, 47, echoregs.coef3);
			wctdm_setreg(wc, mod, 48, echoregs.coef4);
			wctdm_setreg(wc, mod, 49, echoregs.coef5);
			wctdm_setreg(wc, mod, 50, echoregs.coef6);
			wctdm_setreg(wc, mod, 51, echoregs.coef7);
			wctdm_setreg(wc, mod, 52, echoregs.coef8);

			dev_info(&wc->vb.pdev->dev, "-- Set echo registers successfully\n");

			break;
		} else {
			return -EINVAL;

		}
		break;
	case DAHDI_SET_HWGAIN:
		if (copy_from_user(&hwgain, (__user void *) data, sizeof(hwgain)))
			return -EFAULT;

		wctdm_set_hwgain(wc, mod, hwgain.newgain, hwgain.tx);

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
		if (mod->type != FXS)
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

		if (POLARITY_XOR(fxs)) {
			fxs->idletxhookstate |= SLIC_LF_REVMASK;
			x = fxs->lasttxhook & SLIC_LF_SETMASK;
			x |= SLIC_LF_REVMASK;
			if (x != fxs->lasttxhook) { 
				x = set_lasttxhook_interruptible(wc, fxs, x,
								 &mod->sethook);
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
				x = set_lasttxhook_interruptible(wc, fxs, x,
								 &mod->sethook);
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
		if (mod->type != QRV)
			return -ENOTTY;
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p)))
			return -EFAULT;
		stack.p.data = 0; /* start with 0 value in output */
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (mod->mod.qrv.radmode & RADMODE_INVERTCOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECOR:
			if (mod->mod.qrv.radmode & RADMODE_IGNORECOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECT:
			if (mod->mod.qrv.radmode & RADMODE_IGNORECT)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_EXTRXTONE:
			stack.p.data = 0;
			if (mod->mod.qrv.radmode & RADMODE_EXTTONE) {
				stack.p.data = 1;
				if (mod->mod.qrv.radmode & RADMODE_EXTINVERT)
					stack.p.data = 2;
			}
			break;
		case DAHDI_RADPAR_DEBOUNCETIME:
			stack.p.data = mod->mod.qrv.debouncetime;
			break;
		case DAHDI_RADPAR_RXGAIN:
			stack.p.data = mod->mod.qrv.rxgain - 1199;
			break;
		case DAHDI_RADPAR_TXGAIN:
			stack.p.data = mod->mod.qrv.txgain - 3599;
			break;
		case DAHDI_RADPAR_DEEMP:
			stack.p.data = 0;
			if (mod->mod.qrv.radmode & RADMODE_DEEMP)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_PREEMP:
			stack.p.data = 0;
			if (mod->mod.qrv.radmode & RADMODE_PREEMP)
				stack.p.data = 1;
			break;
		default:
			return -EINVAL;
		}
		if (copy_to_user((__user void *) data, &stack.p, sizeof(stack.p)))
		    return -EFAULT;
		break;
	case DAHDI_RADIO_SETPARAM:
		if (mod->type != QRV)
			return -ENOTTY;
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p)))
			return -EFAULT;
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_INVERTCOR;
			else
				mod->mod.qrv.radmode &= ~RADMODE_INVERTCOR;
			return 0;
		case DAHDI_RADPAR_IGNORECOR:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_IGNORECOR;
			else
				mod->mod.qrv.radmode &= ~RADMODE_IGNORECOR;
			return 0;
		case DAHDI_RADPAR_IGNORECT:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_IGNORECT;
			else
				mod->mod.qrv.radmode &= ~RADMODE_IGNORECT;
			return 0;
		case DAHDI_RADPAR_EXTRXTONE:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_EXTTONE;
			else
				mod->mod.qrv.radmode &= ~RADMODE_EXTTONE;
			if (stack.p.data > 1)
				mod->mod.qrv.radmode |= RADMODE_EXTINVERT;
			else
				mod->mod.qrv.radmode &= ~RADMODE_EXTINVERT;
			return 0;
		case DAHDI_RADPAR_DEBOUNCETIME:
			mod->mod.qrv.debouncetime = stack.p.data;
			return 0;
		case DAHDI_RADPAR_RXGAIN:
			/* if out of range */
			if ((stack.p.data <= -1200) || (stack.p.data > 1552))
			{
				return -EINVAL;
			}
			mod->mod.qrv.rxgain = stack.p.data + 1199;
			break;
		case DAHDI_RADPAR_TXGAIN:
			/* if out of range */
			if (mod->mod.qrv.radmode & RADMODE_PREEMP) {
				if ((stack.p.data <= -2400) ||
				    (stack.p.data > 0))
					return -EINVAL;
			} else {
				if ((stack.p.data <= -3600) ||
				    (stack.p.data > 1200))
					return -EINVAL;
			}
			mod->mod.qrv.txgain = stack.p.data + 3599;
			break;
		case DAHDI_RADPAR_DEEMP:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_DEEMP;
			else
				mod->mod.qrv.radmode &= ~RADMODE_DEEMP;
			mod->mod.qrv.rxgain = 1199;
			break;
		case DAHDI_RADPAR_PREEMP:
			if (stack.p.data)
				mod->mod.qrv.radmode |= RADMODE_PREEMP;
			else
				mod->mod.qrv.radmode &= ~RADMODE_PREEMP;
			mod->mod.qrv.txgain = 3599;
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
	struct wctdm *const wc = chan->pvt;
	unsigned long flags;
	struct wctdm_module *const mod = &wc->mods[chan->chanpos - 1];

#if 0
	if (wc->dead)
		return -ENODEV;
#endif
	if (mod->type == FXO) {
		/* Reset the mwi indicators */
		spin_lock_irqsave(&wc->reglock, flags);
		mod->mod.fxo.neonmwi_debounce = 0;
		mod->mod.fxo.neonmwi_offcounter = 0;
		mod->mod.fxo.neonmwi_state = 0;
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
		struct wctdm_module *const mod = &wc->mods[x];
		if (FXS == mod->type) {
			mod->mod.fxs.idletxhookstate =
			    POLARITY_XOR(&mod->mod.fxs) ? SLIC_LF_ACTIVE_REV :
							  SLIC_LF_ACTIVE_FWD;
		} else if (QRV == mod->type) {
			int qrvcard = x & 0xfc;

			mod->mod.qrv.hook = 0;
			wc->mods[x + 2].mod.qrv.hook = 0xff;
			mod->mod.qrv.debouncetime = QRV_DEBOUNCETIME;
			mod->mod.qrv.debtime = 0;
			mod->mod.qrv.radmode = 0;
			mod->mod.qrv.txgain = 3599;
			mod->mod.qrv.rxgain = 1199;
			reg = 0;
			if (!wc->mods[qrvcard].mod.qrv.hook)
				reg |= 1;
			if (!wc->mods[qrvcard + 1].mod.qrv.hook)
				reg |= 0x10;
			wc->mods[qrvcard].sethook = CMD_WR(3, reg);
			qrv_dosetup(chan,wc);
		}
	}

	return 0;
}

static int wctdm_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct wctdm *wc = chan->pvt;
	int reg = 0;
	struct wctdm_module *const mod = &wc->mods[chan->chanpos - 1];

	if (mod->type == QRV) {
		const int qrvcard = (chan->chanpos - 1) & 0xfc;

		switch(txsig) {
		case DAHDI_TXSIG_START:
		case DAHDI_TXSIG_OFFHOOK:
			mod->mod.qrv.hook = 1;
			break;
		case DAHDI_TXSIG_ONHOOK:
			mod->mod.qrv.hook = 0;
			break;
		default:
			dev_notice(&wc->vb.pdev->dev, "wctdm24xxp: Can't set tx state to %d\n", txsig);
		}
		reg = 0;
		if (!wc->mods[qrvcard].mod.qrv.hook)
			reg |= 1;
		if (!wc->mods[qrvcard + 1].mod.qrv.hook)
			reg |= 0x10;
		wc->mods[qrvcard].sethook = CMD_WR(3, reg);
		/* wctdm_setreg(wc, qrvcard, 3, reg); */
	} else if (mod->type == FXO) {
		switch(txsig) {
		case DAHDI_TXSIG_START:
		case DAHDI_TXSIG_OFFHOOK:
			mod->mod.fxo.offhook = 1;
			mod->sethook = CMD_WR(5, 0x9);
			/* wctdm_setreg(wc, chan->chanpos - 1, 5, 0x9); */
			break;
		case DAHDI_TXSIG_ONHOOK:
			mod->mod.fxo.offhook = 0;
			mod->sethook = CMD_WR(5, 0x8);
			/* wctdm_setreg(wc, chan->chanpos - 1, 5, 0x8); */
			break;
		default:
			dev_notice(&wc->vb.pdev->dev, "wctdm24xxp: Can't set tx state to %d\n", txsig);
		}
	} else if (mod->type == FXS) {
		wctdm_fxs_hooksig(wc, mod, txsig);
	}
	return 0;
}

static void wctdm_dacs_connect(struct wctdm *wc, int srccard, int dstcard)
{
	struct wctdm_module *const srcmod = &wc->mods[srccard];
	struct wctdm_module *const dstmod = &wc->mods[dstcard];
	unsigned int type;

	if (wc->mods[dstcard].dacssrc > -1) {
		dev_notice(&wc->vb.pdev->dev, "wctdm_dacs_connect: Can't have double sourcing yet!\n");
		return;
	}
	type = wc->mods[srccard].type;
	if ((type == FXS) || (type == FXO)) {
		dev_notice(&wc->vb.pdev->dev,
			   "wctdm_dacs_connect: Unsupported modtype for "
			   "card %d\n", srccard);
		return;
	}
	type = wc->mods[dstcard].type;
	if ((type != FXS) && (type != FXO)) {
		dev_notice(&wc->vb.pdev->dev,
			   "wctdm_dacs_connect: Unsupported modtype "
			   "for card %d\n", dstcard);
		return;
	}

	if (debug) {
		dev_info(&wc->vb.pdev->dev,
			 "connect %d => %d\n", srccard, dstcard);
	}

	dstmod->dacssrc = srccard;

	/* make srccard transmit to srccard+24 on the TDM bus */
	if (srcmod->type == FXS) {
		/* proslic */
		wctdm_setreg(wc, srcmod, PCM_XMIT_START_COUNT_LSB,
			     ((srccard+24) * 8) & 0xff);
		wctdm_setreg(wc, srcmod, PCM_XMIT_START_COUNT_MSB,
			     ((srccard+24) * 8) >> 8);
	} else if (srcmod->type == FXO) {
		/* daa TX */
		wctdm_setreg(wc, srcmod, 34, ((srccard+24) * 8) & 0xff);
		wctdm_setreg(wc, srcmod, 35, ((srccard+24) * 8) >> 8);
	}

	/* have dstcard receive from srccard+24 on the TDM bus */
	if (dstmod->type == FXS) {
		/* proslic */
		wctdm_setreg(wc, dstmod, PCM_RCV_START_COUNT_LSB,
			     ((srccard+24) * 8) & 0xff);
		wctdm_setreg(wc, dstmod, PCM_RCV_START_COUNT_MSB,
			     ((srccard+24) * 8) >> 8);
	} else if (dstmod->type == FXO) {
		/* daa RX */
		wctdm_setreg(wc, dstmod, 36, ((srccard+24) * 8) & 0xff);
		wctdm_setreg(wc, dstmod, 37, ((srccard+24) * 8) >> 8);
	}
}

static void wctdm_dacs_disconnect(struct wctdm *wc, int card)
{
	struct wctdm_module *const mod = &wc->mods[card];
	struct wctdm_module *dacssrc;

	if (mod->dacssrc <= -1)
		return;

	dacssrc = &wc->mods[mod->dacssrc];

	if (debug) {
		dev_info(&wc->vb.pdev->dev, "wctdm_dacs_disconnect: "
			 "restoring TX for %d and RX for %d\n",
			 mod->dacssrc, card);
	}

	/* restore TX (source card) */
	if (dacssrc->type == FXS) {
		wctdm_setreg(wc, dacssrc, PCM_XMIT_START_COUNT_LSB,
			     (mod->dacssrc * 8) & 0xff);
		wctdm_setreg(wc, dacssrc, PCM_XMIT_START_COUNT_MSB,
			     (mod->dacssrc * 8) >> 8);
	} else if (dacssrc->type == FXO) {
		wctdm_setreg(wc, mod, 34, (card * 8) & 0xff);
		wctdm_setreg(wc, mod, 35, (card * 8) >> 8);
	} else {
		dev_warn(&wc->vb.pdev->dev,
			 "WARNING: wctdm_dacs_disconnect() called "
			 "on unsupported modtype\n");
	}

	/* restore RX (this card) */
	if (FXS == mod->type) {
		wctdm_setreg(wc, mod, PCM_RCV_START_COUNT_LSB,
			     (card * 8) & 0xff);
		wctdm_setreg(wc, mod, PCM_RCV_START_COUNT_MSB,
			     (card * 8) >> 8);
	} else if (FXO == mod->type) {
		wctdm_setreg(wc, mod, 36, (card * 8) & 0xff);
		wctdm_setreg(wc, mod, 37, (card * 8) >> 8);
	} else {
		dev_warn(&wc->vb.pdev->dev,
			 "WARNING: wctdm_dacs_disconnect() called "
			 "on unsupported modtype\n");
	}

	mod->dacssrc = -1;
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

/**
 * wctdm_wait_for_ready
 *
 * Check if the board has finished any setup and is ready to start processing
 * calls.
 */
int wctdm_wait_for_ready(struct wctdm *wc)
{
	while (!is_initialized(wc)) {
		if (fatal_signal_pending(current))
			return -EIO;
		msleep_interruptible(250);
	}
	return 0;
}

static int wctdm_enable_hw_preechocan(struct dahdi_chan *chan)
{
	struct wctdm *wc = chan->pvt;
	struct wctdm_chan *wchan = container_of(chan, struct wctdm_chan, chan);

	if (!wc->vpmoct)
		return 0;

	return vpmoct_preecho_enable(wc->vpmoct, wchan->timeslot);
}

static void wctdm_disable_hw_preechocan(struct dahdi_chan *chan)
{
	struct wctdm *wc = chan->pvt;
	struct wctdm_chan *wchan = container_of(chan, struct wctdm_chan, chan);

	if (!wc->vpmoct)
		return;

	vpmoct_preecho_disable(wc->vpmoct, wchan->timeslot);
}

/**
 * wctdm_chanconfig - Called when the channels are being configured.
 *
 * Ensure that the card is completely ready to go before we allow the channels
 * to be completely configured. This is to allow lengthy initialization
 * actions to take place in background on driver load and ensure we're synced
 * up by the time dahdi_cfg is run.
 *
 */
static int
wctdm_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	struct wctdm *wc = chan->pvt;

	if ((file->f_flags & O_NONBLOCK) && !is_initialized(wc))
		return -EAGAIN;

	return wctdm_wait_for_ready(wc);
}

/*
 * wctdm24xxp_assigned - Called when span is assigned.
 * @span:	The span that is now assigned.
 *
 * This function is called by the core of DAHDI after the span number and
 * channel numbers have been assigned.
 *
 */
static void wctdm24xxp_assigned(struct dahdi_span *span)
{
	struct dahdi_span *s;
	struct dahdi_device *ddev = span->parent;
	struct wctdm *wc = NULL;

	list_for_each_entry(s, &ddev->spans, device_node) {
		wc = (container_of(s, struct wctdm_span, span))->wc;
		if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &s->flags))
			return;
	}

	if (wc) {
		WARN_ON(0 == wc->not_ready);
		--wc->not_ready;
	}
}

static const struct dahdi_span_ops wctdm24xxp_analog_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = wctdm_hooksig,
	.open = wctdm_open,
	.close = wctdm_close,
	.ioctl = wctdm_ioctl,
	.watchdog = wctdm_watchdog,
	.chanconfig = wctdm_chanconfig,
	.dacs = wctdm_dacs,
	.assigned = wctdm24xxp_assigned,
#ifdef VPM_SUPPORT
	.enable_hw_preechocan = wctdm_enable_hw_preechocan,
	.disable_hw_preechocan = wctdm_disable_hw_preechocan,
	.echocan_create = wctdm_echocan_create,
	.echocan_name = wctdm_echocan_name,
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
	.assigned = wctdm24xxp_assigned,
#ifdef VPM_SUPPORT
	.enable_hw_preechocan = wctdm_enable_hw_preechocan,
	.disable_hw_preechocan = wctdm_disable_hw_preechocan,
	.echocan_create = wctdm_echocan_create,
	.echocan_name = wctdm_echocan_name,
#endif
};

static struct wctdm_chan *
wctdm_init_chan(struct wctdm *wc, struct wctdm_span *s, int chanoffset,
		int channo, unsigned int card_position)
{
	struct wctdm_chan *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return NULL;

	/* Do not change the procfs representation for non-hx8 cards. */
	if (dahdi_is_digital_span(&s->span)) {
		sprintf(c->chan.name, "WCBRI/%d/%d/%d", card_position,
			s->spanno, channo);
	} else {
		sprintf(c->chan.name, "WCTDM/%d/%d", card_position, channo);
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

static struct wctdm_span *
wctdm_init_span(struct wctdm *wc, int spanno, int chanoffset, int chancount,
		int digital_span, unsigned int card_position)
{
	int x;
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
		sprintf(s->span.name, "WCBRI/%d/%d", card_position, s->spanno);
	else
		sprintf(s->span.name, "WCTDM/%d", card_position);

	snprintf(s->span.desc, sizeof(s->span.desc) - 1, "%s", wc->desc->name);

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
		c = wctdm_init_chan(wc, s, chanoffset, x, card_position);
		if (!c)
			return NULL;
		wc->chans[chanoffset + x] = c;
		s->span.chans[x] = &c->chan;
	}

	s->span.channels = chancount;

	if (digital_span) {
		wc->chans[chanoffset + 0]->chan.sigcap = DAHDI_SIG_CLEAR;
		wc->chans[chanoffset + 1]->chan.sigcap = DAHDI_SIG_CLEAR;
		wc->chans[chanoffset + 2]->chan.sigcap = DAHDI_SIG_HARDHDLC;
	}

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
		struct wctdm_module *const mod = &wc->mods[x];
		if (debug) {
			dev_info(&wc->vb.pdev->dev,
				 "fixup_analog: x=%d, y=%d modtype=%d, "
				 "s->chans[%d]=%p\n", x, y, mod->type,
				 y, s->chans[y]);
		}
		if (mod->type == FXO) {
			int val;
			s->chans[y++]->sigcap = DAHDI_SIG_FXSKS | DAHDI_SIG_FXSLS | DAHDI_SIG_SF | DAHDI_SIG_CLEAR;
			val = should_set_alaw(wc) ? 0x20 : 0x28;
#ifdef DEBUG
			val = (digitalloopback) ? 0x30 : val;
#endif
			wctdm_setreg(wc, mod, 33, val);
		} else if (mod->type == FXS) {
			s->chans[y++]->sigcap = DAHDI_SIG_FXOKS | DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS | DAHDI_SIG_SF | DAHDI_SIG_EM | DAHDI_SIG_CLEAR;
			wctdm_setreg(wc, mod, 1,
				     (should_set_alaw(wc) ? 0x20 : 0x28));
		} else if (mod->type == QRV) {
			s->chans[y++]->sigcap = DAHDI_SIG_SF | DAHDI_SIG_EM | DAHDI_SIG_CLEAR;
		} else {
			s->chans[y++]->sigcap = 0;
		}
	}
}

static int wctdm_initialize_vpmadt032(struct wctdm *wc)
{
	int res;
	struct vpmadt032_options options;
	struct vpmadt032 *vpm;
	unsigned long flags;

	options.debug = debug;
	options.vpmnlptype = vpmnlptype;
	options.vpmnlpthresh = vpmnlpthresh;
	options.vpmnlpmaxsupp = vpmnlpmaxsupp;
	if (is_hx8(wc)) {
		/* Hybrid cards potentially have 3 channels of EC on their
		 * ports since they may be BRI spans. */
		options.channels = 3 * wc->desc->ports;
	} else {
		options.channels = wc->desc->ports;
	}

	BUG_ON(options.channels > 24);

	wc->vpmadt032 = vpmadt032_alloc(&options);
	if (!wc->vpmadt032)
		return -ENOMEM;

	wc->vpmadt032->setchanconfig_from_state = setchanconfig_from_state;

	/* Pull the configuration information from the span holding
	 * the analog channels. */
	res = vpmadt032_test(wc->vpmadt032, &wc->vb);
	if (!res)
		res = vpmadt032_init(wc->vpmadt032);
	if (res) {
		vpm = wc->vpmadt032;
		spin_lock_irqsave(&wc->reglock, flags);
		wc->vpmadt032 = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);
		vpmadt032_free(vpm);
		return res;
	}

	/* Now we need to configure the VPMADT032 module for this
	 * particular board. */
	res = config_vpmadt032(wc->vpmadt032, wc);
	if (res) {
		vpm = wc->vpmadt032;
		spin_lock_irqsave(&wc->reglock, flags);
		wc->vpmadt032 = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);
		vpmadt032_free(vpm);
		return res;
	}

	return 0;
}

static void wctdm_vpm_load_complete(struct device *dev, bool operational)
{
	unsigned long flags;
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct wctdm *wc = pci_get_drvdata(pdev);
	struct vpmoct *vpm = NULL;

	WARN_ON(!wc || !wc->not_ready);
	if (!wc || !wc->not_ready)
		return;

	spin_lock_irqsave(&wc->reglock, flags);
	wc->not_ready--;
	if (operational) {
		wc->ctlreg |= 0x10;
	} else {
		vpm = wc->vpmoct;
		wc->vpmoct = NULL;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (vpm)
		vpmoct_free(vpm);
}

static int wctdm_initialize_vpm(struct wctdm *wc, unsigned long unused)
{
	int res = 0;

	if (!vpmsupport) {
		dev_notice(&wc->vb.pdev->dev, "VPM: Support Disabled\n");
		return 0;
	}

	res = wctdm_initialize_vpmadt032(wc);
	if (!res) {
		wc->ctlreg |= 0x10;
		return 0;
	} else {
		struct vpmoct *vpm;
		unsigned long flags;

		vpm = vpmoct_alloc();
		if (!vpm) {
			dev_info(&wc->vb.pdev->dev,
			    "Unable to allocate memory for struct vpmoct\n");
			return -ENOMEM;
		}

		vpm->dev = &wc->vb.pdev->dev;

		spin_lock_irqsave(&wc->reglock, flags);
		wc->vpmoct = vpm;
		wc->not_ready++;
		spin_unlock_irqrestore(&wc->reglock, flags);

		res = vpmoct_init(vpm, wctdm_vpm_load_complete);
		if (-EINVAL == res) {
			spin_lock_irqsave(&wc->reglock, flags);
			wc->vpmoct = NULL;
			wc->not_ready--;
			spin_unlock_irqrestore(&wc->reglock, flags);
			vpmoct_free(vpm);
		}
	}
	return 0;
}

static int __wctdm_identify_module_group(struct wctdm *wc, unsigned long base)
{
	int x;
	unsigned long flags;

	for (x = base; x < base + 4; ++x) {
		struct wctdm_module *const mod = &wc->mods[x];
		enum {SANE = 1, UNKNOWN = 0};
		int ret = 0, readi = 0;
		bool altcs = false;

		if (fatal_signal_pending(current))
			break;
retry:
		ret = wctdm_init_proslic(wc, mod, 0, 0, UNKNOWN);
		if (!ret) {
			if (debug & DEBUG_CARD) {
				readi = wctdm_getreg(wc, mod, LOOP_I_LIMIT);
				dev_info(&wc->vb.pdev->dev,
					 "Proslic module %d loop current "
					 "is %dmA\n", x, ((readi*3) + 20));
			}
			continue;
		}

		if (ret != -2) {
			/* Init with Manual Calibration */
			if (!wctdm_init_proslic(wc, mod, 0, 1, SANE)) {

				if (debug & DEBUG_CARD) {
					readi = wctdm_getreg(wc, mod,
							     LOOP_I_LIMIT);
					dev_info(&wc->vb.pdev->dev,
						 "Proslic module %d loop "
						 "current is %dmA\n", x,
						 ((readi*3)+20));
				}
			} else {
				dev_notice(&wc->vb.pdev->dev,
					   "Port %d: FAILED FXS (%s)\n",
					   x + 1, fxshonormode ?
					   fxo_modes[_opermode].name : "FCC");
			}
			continue;
		}

		ret = wctdm_init_voicedaa(wc, mod, 0, 0, UNKNOWN);
		if (!ret)
			continue;

		if (!wctdm_init_qrvdri(wc, x))
			continue;

		if (is_hx8(wc) && !wctdm_init_b400m(wc, x))
			continue;

		if ((wc->desc->ports != 24) && ((x&0x3) == 1) && !altcs) {

			spin_lock_irqsave(&wc->reglock, flags);
			set_offsets(mod, 2);
			altcs = true;

			if (wc->desc->ports == 4) {
				set_offsets(&wc->mods[x+1], 3);
				set_offsets(&wc->mods[x+2], 3);
			}

			mod->type = FXSINIT;
			spin_unlock_irqrestore(&wc->reglock, flags);

			udelay(1000);
			udelay(1000);

			spin_lock_irqsave(&wc->reglock, flags);
			mod->type = FXS;
			spin_unlock_irqrestore(&wc->reglock, flags);

			if (debug & DEBUG_CARD) {
				dev_info(&wc->vb.pdev->dev,
					 "Trying port %d with alternate chip "
					 "select\n", x + 1);
			}
			goto retry;
		}

		mod->type = NONE;
	}
	return 0;
}

/**
 * wctdm_print_moule_configuration - Print the configuration to the kernel log
 * @wc:		The card we're interested in.
 *
 * This is to ensure that the module configuration from each card shows up
 * sequentially in the kernel log, as opposed to interleaved with one another.
 *
 */
static void wctdm_print_module_configuration(const struct wctdm *const wc)
{
	int i;
	static DEFINE_MUTEX(print);

	mutex_lock(&print);
	for (i = 0; i < wc->mods_per_board; ++i) {
		const struct wctdm_module *const mod = &wc->mods[i];

		switch (mod->type) {
		case FXO:
			dev_info(&wc->vb.pdev->dev, "Port %d: Installed -- "
				 "AUTO FXO (%s mode)\n", i + 1,
				 fxo_modes[_opermode].name);
			break;
		case FXS:
			dev_info(&wc->vb.pdev->dev,
				 "Port %d: Installed -- AUTO FXS/DPO\n", i + 1);
			break;
		case BRI:
			dev_info(&wc->vb.pdev->dev, "Port %d: Installed -- BRI "
				 "quad-span module\n", i + 1);
			break;
		case QRV:
			dev_info(&wc->vb.pdev->dev,
				 "Port %d: Installed -- QRV DRI card\n", i + 1);
			break;
		case NONE:
			dev_info(&wc->vb.pdev->dev,
				 "Port %d: Not installed\n", i + 1);
			break;
		case FXSINIT:
			break;
		}
	}
	mutex_unlock(&print);
}

static void wctdm_identify_modules(struct wctdm *wc)
{
	int x;
	unsigned long flags;
	struct bg *bg_work[ARRAY_SIZE(wc->mods)/4 + 1] = {NULL, };

	wc->ctlreg = 0x00;

	/*
	 * This looks a little weird.
	 *
	 * There are only 8 physical ports on the TDM/AEX800, but the code
	 * immediately below sets 24 modules up.  This has to do with the
	 * altcs magic that allows us to have single-port and quad-port
	 * modules on these products.  The variable "mods_per_board" is set to
	 * the appropriate value just below the next code block.
	 *
	 * Now why this is important: The FXS modules come out of reset in a
	 * two-byte, non-chainable SPI mode.  This is currently incompatible
	 * with how we do things, so we need to set them to a chained, 3-byte
	 * command mode.  This is done by setting the module type to FXSINIT
	 * for a little while so that cmd_dequeue will initialize the SLIC
	 * into the appropriate mode.
	 *
	 * This "go to 3-byte chained mode" command, however, wreaks havoc
	 * with HybridBRI.
	 *
	 * The solution: Since HybridBRI is only designed to work in an 8-port
	 * card, and since the single-port modules "show up" in SPI slots >= 8
	 * in these cards, we only set SPI slots 8-23 to FXSINIT.  The
	 * HybridBRI will never see the command that causes it to freak out
	 * and the single-port FXS cards get what they need so that when we
	 * probe with altcs we see them.
	 */

	/* Make sure all units go into daisy chain mode */
	spin_lock_irqsave(&wc->reglock, flags);
	for (x = 0; x < ARRAY_SIZE(wc->mods); x++)
		wc->mods[x].type = FXSINIT;
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* Wait just a bit; this makes sure that cmd_dequeue is emitting SPI
	 * commands in the appropriate mode(s). */
	msleep(20);

	/* Now that all the cards have been reset, we can stop checking them
	 * all if there aren't as many */
	spin_lock_irqsave(&wc->reglock, flags);
	wc->mods_per_board = wc->desc->ports;
	spin_unlock_irqrestore(&wc->reglock, flags);

	BUG_ON(wc->desc->ports % 4);

	/* Detecting and configuring the modules over voicebus takes a
	 * significant amount of time. We can speed things up by performing
	 * this in parallel for each group of four modules. */
	for (x = 0; x < wc->desc->ports/4; x++)
		bg_work[x] = bg_create(wc, __wctdm_identify_module_group, x*4);

	for (x = 0; bg_work[x]; ++x)
		bg_join(bg_work[x]);

	wctdm_print_module_configuration(wc);
}

static struct pci_driver wctdm_driver;

static void wctdm_back_out_gracefully(struct wctdm *wc)
{
	int i;
	unsigned long flags;
	struct vpmadt032 *vpm;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&wc->reglock, flags);
	if (wc->not_ready) {
		wc->not_ready--;
		spin_unlock_irqrestore(&wc->reglock, flags);
		while (wctdm_wait_for_ready(wc))
			schedule();
		spin_lock_irqsave(&wc->reglock, flags);
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (wc->vpmadt032) {
		flush_workqueue(wc->vpmadt032->wq);
		clear_bit(VPM150M_ACTIVE, &wc->vpmadt032->control);
		flush_workqueue(wc->vpmadt032->wq);
		spin_lock_irqsave(&wc->reglock, flags);
		vpm = wc->vpmadt032;
		wc->vpmadt032 = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);
		vpmadt032_free(vpm);
	}

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

	spin_lock_irqsave(&wc->reglock, flags);
	for (i = 0; i < ARRAY_SIZE(wc->mods); ++i) {
		struct wctdm_module *const mod = &wc->mods[i];
		kfree(wc->chans[i]);
		wc->chans[i] = NULL;
		list_splice_init(&mod->pending_cmds, &local_list);
		list_splice_init(&mod->active_cmds, &local_list);
	}
	list_splice_init(&wc->free_isr_commands, &local_list);
	spin_unlock_irqrestore(&wc->reglock, flags);

	while (!list_empty(&local_list)) {
		struct wctdm_cmd *cmd;
		cmd = list_entry(local_list.next,
				 struct wctdm_cmd, node);
		list_del(&cmd->node);
		kfree(cmd->complete);
		kfree(cmd);
	}

	spin_lock_irqsave(&wc->frame_list_lock, flags);
	list_splice(&wc->frame_list, &local_list);
	spin_unlock_irqrestore(&wc->frame_list_lock, flags);

	while (!list_empty(&local_list)) {
		struct sframe_packet *frame;
		frame = list_entry(local_list.next,
				   struct sframe_packet, node);
		list_del(&frame->node);
		kfree(frame);
	}

	kfree(wc->board_name);
	kfree(wc->ddev->devicetype);
	kfree(wc->ddev->location);
	dahdi_free_device(wc->ddev);
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
	__le32	chksum;
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
	      le32_to_cpu(ha8_fw->chksum))) {
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
static ssize_t
enable_vpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	struct wctdm *wc = dev_get_drvdata(dev);
	unsigned int enable_vpm;
	spin_lock_irqsave(&wc->reglock, flags);
	enable_vpm = (wc->ctlreg & 0x10) != 0;
	spin_unlock_irqrestore(&wc->reglock, flags);
	return sprintf(buf, "%d\n", enable_vpm);
}

static ssize_t
enable_vpm_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	unsigned long flags;
	struct wctdm *wc = dev_get_drvdata(dev);
	unsigned int enable_vpm;
	if (count < 2)
		return -EINVAL;

	if (('0' == buf[0]) || (0 == buf[0]))
		enable_vpm = 0;
	else
		enable_vpm = 1;

	spin_lock_irqsave(&wc->reglock, flags);
	if (enable_vpm)
		wc->ctlreg |= 0x10;
	else
		wc->ctlreg &= ~0x10;
	spin_unlock_irqrestore(&wc->reglock, flags);
	return count;
}

static DEVICE_ATTR(enable_vpm, 0644,
		   enable_vpm_show, enable_vpm_store);

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

	ret = device_create_file(&wc->vb.pdev->dev,
				 &dev_attr_enable_vpm);
	if (ret) {
		dev_info(&wc->vb.pdev->dev,
			"Failed to create device attributes.\n");
	}
}

static void remove_sysfs_files(struct wctdm *wc)
{
	device_remove_file(&wc->vb.pdev->dev,
			   &dev_attr_enable_vpm);

	device_remove_file(&wc->vb.pdev->dev,
			   &dev_attr_vpm_firmware_version);

	device_remove_file(&wc->vb.pdev->dev,
			   &dev_attr_voicebus_current_latency);
}

#else

static inline void create_sysfs_files(struct wctdm *wc) { return; }
static inline void remove_sysfs_files(struct wctdm *wc) { return; }

#endif /* CONFIG_VOICEBUS_SYSFS */

static void wctdm_set_tdm410_leds(struct wctdm *wc)
{
	int i;

	if (4 != wc->desc->ports)
		return;

	wc->tdm410leds = 0; /* all on by default */
	for (i = 0; i < wc->desc->ports; ++i) {
		/* Turn off the LED for any module that isn't installed. */
		if (NONE == wc->mods[i].type)
			wc->tdm410leds |= (1 << i);
	}
}

/**
 * wctdm_allocate_irq_commands - Preallocate some commands for use in interrupt context.
 * @wc:	The board which we're allocating for.
 * @count:	The number of IRQ commands to allocate.
 *
 * We need a minimum of 4 * the current latency worth of commands for each
 * analog module. When the latency grows, new commands will be allocated, but
 * this just represents are best guess as to the number of commands we'll need
 * after probing for modules, and reduces the chance that we'll allocate
 * memory in interrupt context when the driver first loads.
 *
 */
static void wctdm_allocate_irq_commands(struct wctdm *wc, unsigned int count)
{
	unsigned long flags;
	LIST_HEAD(local_list);

	if (!count)
		return;

	while (count--) {
		struct wctdm_cmd *cmd;
		cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
		if (!cmd)
			break;
		list_add(&cmd->node, &local_list);
	}

	spin_lock_irqsave(&wc->reglock, flags);
	list_splice(&local_list, &wc->free_isr_commands);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

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
	unsigned int pos;
	int i, ret;
	struct bg *vpm_work;
	int anamods, digimods, curchan, curspan;
	
	neonmwi_offlimit_cycles = neonmwi_offlimit / MS_PER_HOOKCHECK;

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	wc->not_ready = 1;

	down(&ifacelock);
	/* \todo this is a candidate for removal... */
	for (pos = 0; pos < WC_MAX_IFACES; ++pos) {
		if (!ifaces[pos]) {
			ifaces[pos] = wc;
			break;
		}
	}
	up(&ifacelock);

	wc->desc = (struct wctdm_desc *)ent->driver_data;

	/* This is to insure that the analog span is given lowest priority */
	sema_init(&wc->syncsem, 1);
	INIT_LIST_HEAD(&wc->frame_list);
	spin_lock_init(&wc->frame_list_lock);
	init_waitqueue_head(&wc->regq);
	spin_lock_init(&wc->reglock);
	INIT_LIST_HEAD(&wc->free_isr_commands);
	wc->oldsync = -1;

	wc->board_name = kasprintf(GFP_KERNEL, "%s%d", wctdm_driver.name, pos);
	if (!wc->board_name) {
		wctdm_back_out_gracefully(wc);
		return -ENOMEM;
	}

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

	pci_set_drvdata(pdev, wc);
	wc->vb.ops = &voicebus_operations;
	wc->vb.pdev = pdev;
	wc->vb.debug = &debug;

#ifdef CONFIG_VOICEBUS_DISABLE_ASPM
	if (is_pcie(wc)) {
		pci_disable_link_state(pdev->bus->self, PCIE_LINK_STATE_L0S |
			PCIE_LINK_STATE_L1 | PCIE_LINK_STATE_CLKPM);
	};
#endif

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
		wctdm_back_out_gracefully(wc);
		return ret;
	}

	create_sysfs_files(wc);

	voicebus_lock_latency(&wc->vb);

	wc->mods_per_board = NUM_MODULES;
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

	for (i = 0; i < ARRAY_SIZE(wc->mods); i++) {
		struct wctdm_module *const mod = &wc->mods[i];
		INIT_LIST_HEAD(&mod->pending_cmds);
		INIT_LIST_HEAD(&mod->active_cmds);
		mod->dacssrc = -1;
		mod->card = i;
		set_offsets(mod, 0);
	}

	/* Start the hardware processing. */
	if (voicebus_start(&wc->vb)) {
		BUG_ON(1);
	}

	if (is_hx8(wc)) {
		ret = hx8_check_firmware(wc);
		if (ret) {
			wctdm_back_out_gracefully(wc);
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

	vpm_work = bg_create(wc, wctdm_initialize_vpm, 0);
	if (!vpm_work) {
		wctdm_back_out_gracefully(wc);
		return -ENOMEM;
	}

	/* Now track down what modules are installed */
	wctdm_identify_modules(wc);

	wctdm_set_tdm410_leds(wc);

	if (fatal_signal_pending(current)) {
		wctdm_back_out_gracefully(wc);
		bg_join(vpm_work);
		return -EINTR;
	}

	/* We need to wait for the vpm thread to finish before we setup the
	 * spans in order to ensure they are named properly. */
	bg_join(vpm_work);

/*
 * Walk the module list and create a 3-channel span for every BRI module found.
 * Empty and analog modules get a common span which is allocated outside of this loop.
 */
	anamods = digimods = 0;
	curchan = curspan = 0;
	
	for (i = 0; i < wc->mods_per_board; i++) {
		struct wctdm_module *const mod = &wc->mods[i];
		struct b400m *b4;

		if (mod->type == NONE) {
			++curspan;
			continue;
		} else if (mod->type == BRI) {
			if (!is_hx8(wc)) {
				dev_info(&wc->vb.pdev->dev, "Digital modules "
					"detected on a non-hybrid card. "
					"This is unsupported.\n");
				wctdm_back_out_gracefully(wc);
				bg_join(vpm_work);
				return -EIO;
			}
			wc->spans[curspan] = wctdm_init_span(wc, curspan,
							     curchan, 3, 1,
							     pos);
			if (!wc->spans[curspan]) {
				wctdm_back_out_gracefully(wc);
				bg_join(vpm_work);
				return -EIO;
			}
			b4 = mod->mod.bri;
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
		wctdm_init_span(wc, curspan, curchan, wc->desc->ports, 0, pos);
		wctdm_fixup_analog_span(wc, curspan);
		wc->aspan = wc->spans[curspan];
		curchan += wc->desc->ports;
		++curspan;
	}

	/* Now fix up the timeslots for the analog modules, since the digital
	 * modules are always first */
	for (i = 0; i < wc->mods_per_board; i++) {
		struct wctdm_module *const mod = &wc->mods[i];
		switch (mod->type) {
		case FXS:
			wctdm_proslic_set_ts(wc, mod, (digimods * 12) + i);
			break;
		case FXO:
			wctdm_voicedaa_set_ts(wc, mod, (digimods * 12) + i);
			break;
		case QRV:
			wctdm_qrvdri_set_ts(wc, mod, (digimods * 12) + i);
			break;
		default:
			break;
		}
	}

	/* This shouldn't ever occur, but if we don't try to trap it, the driver
	 * will be scribbling into memory it doesn't own. */
	BUG_ON(curchan > 24);

	wc->avchannels = curchan;

#ifdef USE_ASYNC_INIT
		async_synchronize_cookie(cookie);
#endif
	wc->ddev = dahdi_create_device();
	wc->ddev->manufacturer = "Digium";
	wc->ddev->location = kasprintf(GFP_KERNEL, "PCI%s Bus %02d Slot %02d",
				       (wc->desc->flags & FLAG_EXPRESS) ?
						" Express" : "",
				       pdev->bus->number,
				       PCI_SLOT(pdev->devfn) + 1);
	if (!wc->ddev->location) {
		wctdm_back_out_gracefully(wc);
		return -ENOMEM;
	}

	if (wc->vpmadt032) {
		wc->ddev->devicetype = kasprintf(GFP_KERNEL, "%s (VPMADT032)",
						wc->desc->name);
	} else if (wc->vpmoct) {
		wc->ddev->devicetype = kasprintf(GFP_KERNEL, "%s (VPMOCT032)",
						wc->desc->name);
	} else  {
		wc->ddev->devicetype = kasprintf(GFP_KERNEL, "%s",
						wc->desc->name);
	}

	if (!wc->ddev->devicetype) {
		wctdm_back_out_gracefully(wc);
		return -ENOMEM;
	}

	/* We should be ready for DAHDI to come in now. */
	for (i = 0; i < MAX_SPANS; ++i) {
		struct dahdi_span *span;

		if (!wc->spans[i])
			continue;

		span = &wc->spans[i]->span;

		list_add_tail(&span->device_node, &wc->ddev->spans);
	}

	wctdm_allocate_irq_commands(wc, anamods * latency * 4);

	if (dahdi_register_device(wc->ddev, &wc->vb.pdev->dev)) {
		dev_notice(&wc->vb.pdev->dev, "Unable to register device with DAHDI\n");
		wctdm_back_out_gracefully(wc);
		return -1;
	}

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

	if (is_initialized(wc))
		dahdi_unregister_device(wc->ddev);

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
	int i;
	unsigned long flags;
	struct wctdm *wc = pci_get_drvdata(pdev);
	struct vpmadt032 *vpmadt032;
	struct vpmoct	 *vpmoct;

	if (!wc)
		return;

	vpmadt032 = wc->vpmadt032;
	vpmoct = wc->vpmoct;

	remove_sysfs_files(wc);

	if (vpmadt032) {
		flush_workqueue(vpmadt032->wq);
		clear_bit(VPM150M_ACTIVE, &vpmadt032->control);
		flush_workqueue(vpmadt032->wq);
	} else if (vpmoct) {
		while (wctdm_wait_for_ready(wc))
			schedule();
	}

	flush_scheduled_work();

	/* shut down any BRI modules */
	for (i = 0; i < wc->mods_per_board; i += 4) {
		if (wc->mods[i].type == BRI)
			wctdm_unload_b400m(wc, i);
	}

	voicebus_stop(&wc->vb);

	if (vpmadt032) {
		spin_lock_irqsave(&wc->reglock, flags);
		wc->vpmadt032 = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);
		vpmadt032_free(vpmadt032);
	} else if (vpmoct) {
		spin_lock_irqsave(&wc->reglock, flags);
		wc->vpmoct = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);
		vpmoct_free(vpmoct);
	}

	dev_info(&wc->vb.pdev->dev, "Freed a %s\n",
			(is_hx8(wc)) ? "Hybrid card" : "Wildcard");

	/* Release span */
	wctdm_release(wc);
}

static DEFINE_PCI_DEVICE_TABLE(wctdm_pci_tbl) = {
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
	voicebus_quiesce(&wc->vb);
}
#endif

MODULE_DEVICE_TABLE(pci, wctdm_pci_tbl);

static int wctdm_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return -ENOSYS;
}

static struct pci_driver wctdm_driver = {
	.name = "wctdm24xxp",
	.probe = wctdm_init_one,
	.remove = __devexit_p(wctdm_remove_one),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
	.shutdown = wctdm_shutdown,
#endif
	.suspend = wctdm_suspend,
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

	if (-1 == fastpickup) {
		if (!strcmp(opermode, "JAPAN"))
			fastpickup = 1;
		else
			fastpickup = 0;
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
module_param(fastpickup, int, 0400);
MODULE_PARM_DESC(fastpickup,
		 "Set to 1 to shorten the calibration delay when taking " \
		 "an FXO port off hook. This can be required for Type-II " \
		 "CID. If -1 the calibration delay will depend on the " \
		 "current opermode.\n");
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
