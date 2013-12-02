/*
 * Digium, Inc.  Wildcard te43x T1/E1 card Driver
 *
 * Written by Russ Meyerriecks <rmeyerriecks@digium.com>
 * Copyright (C) 2012 - 2013, Digium, Inc.
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

#define pr_fmt(fmt)             KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/crc32.h>

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <oct612x.h>

#include <stdbool.h>
#include <dahdi/kernel.h>

#include "wct4xxp/wct4xxp.h"	/* For certain definitions */
#include "wcxb.h"
#include "wcxb_spi.h"
#include "wcxb_flash.h"

static const char *TE435_FW_FILENAME = "dahdi-fw-te435.bin";
static const u32 TE435_VERSION = 0xe0017;

/* #define RPC_RCLK */
#define VPM_SUPPORT

enum linemode {
	T1 = 1,
	E1,
	J1,
};

#define STATUS_LED_GREEN	(1 << 9)
#define STATUS_LED_RED		(1 << 10)
#define STATUS_LED_YELLOW	(STATUS_LED_RED | STATUS_LED_GREEN)
#define LED_MASK		 0x7f8
#define FALC_CPU_RESET		(1 << 11)

/* Interrupt definitions */
#define FALC_INT		(1<<3)
#define SPI_INT			(1<<4)
#define DMA_STOPPED		(1<<5)

struct t43x;

struct t43x_span {
	struct t43x *owner;
	struct dahdi_span span;
	struct {
		unsigned int nmf:1;
		unsigned int sendingyellow:1;
	} flags;
	unsigned char txsigs[16];	/* Copy of tx sig registers */
	unsigned long lofalarmtimer;
	unsigned long losalarmtimer;
	unsigned long aisalarmtimer;
	unsigned long yelalarmtimer;
	unsigned long recoverytimer;
	unsigned long loopuptimer;
	unsigned long loopdntimer;

	struct dahdi_chan *chans[32];		/* Channels */
	struct dahdi_echocan_state *ec[32];	/* Echocan state for channels */

	bool debounce;
	int syncpos;
	int sync;
};

struct t43x_clksrc_work {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	struct work_struct work;
#else
	struct delayed_work work;
#endif
	spinlock_t lock;
	enum wcxb_clock_sources clksrc;
	bool is_timing_master;
};

/* One structure per card */
struct t43x {
	spinlock_t reglock;

	const struct t43x_desc *devtype;
	unsigned long blinktimer;
	bool blink;
	struct dahdi_device *ddev;
	struct t43x_span *tspans[4];
	int numspans;			/* Number of spans on the card */
	int spansstarted;		/* Number of spans started */

	/* protected by t1.reglock */
	struct timer_list timer;
	struct work_struct timer_work;
	struct workqueue_struct *wq;
	struct t43x_clksrc_work clksrc_work;
	unsigned int not_ready;		/* 0 when entire card is ready to go */
#ifdef VPM_SUPPORT
	struct vpm450m *vpm;
	char *vpm_name;
#endif
	struct mutex lock;
	bool latency_locked;
	int syncsrc;
	int num;			/* card index */
	int intr_span;			/* span to service next interrupt */

	struct wcxb xb;
	struct list_head		card_node;
};

static void t43x_handle_transmit(struct wcxb *xb, void *vfp);
static void t43x_handle_receive(struct wcxb *xb, void *vfp);
static void t43x_handle_interrupt(struct wcxb *xb, u32 pending);

static struct wcxb_operations xb_ops = {
	.handle_receive = t43x_handle_receive,
	.handle_transmit = t43x_handle_transmit,
	.handle_interrupt = t43x_handle_interrupt,
};

/* Maintenance Mode Registers */
#define LIM0		0x36
#define LIM0_LL		(1<<1)
#define LIM1		0x37
#define LIM1_RL		(1<<1)
#define LIM1_JATT	(1<<2)

/* Clear Channel Registers */
#define CCB1		0x2f
#define CCB2		0x30
#define CCB3		0x31

#define FECL_T		0x50	/* Framing Error Counter Lower Byte */
#define FECH_T		0x51	/* Framing Error Counter Higher Byte */
#define CVCL_T		0x52	/* Code Violation Counter Lower Byte */
#define CVCH_T		0x53	/* Code Violation Counter Higher Byte */
#define CEC1L_T		0x54	/* CRC Error Counter 1 Lower Byte */
#define CEC1H_T		0x55	/* CRC Error Counter 1 Higher Byte */
#define EBCL_T		0x56	/* E-Bit Error Counter Lower Byte */
#define EBCH_T		0x57	/* E-Bit Error Counter Higher Byte */
#define BECL_T		0x58	/* Bit Error Counter Lower Byte */
#define BECH_T		0x59	/* Bit Error Counter Higher Byte */
#define COEC_T		0x5A	/* COFA Event Counter */
#define PRBSSTA_T	0xDA	/* PRBS Status Register */
#define FRS1_T		0x4D	/* Framer Receive Status Reg 1 */

#define ISR3_SEC (1 << 6)	/* Internal one-second interrupt bit mask */
#define ISR3_ES (1 << 7)	/* Errored Second interrupt bit mask */

#define IMR0		0x14

/* pci memory map offsets */
#define DMA1		0x00	/* dma addresses */
#define DMA2		0x04
#define DMA3		0x08
#define CNTL		0x0C	/* fpga control register */
#define INT_MASK	0x10	/* interrupt vectors from oct and framer */
#define INT_STAT	0x14
#define FRAMER_BASE	0x00000800	/* framer's address space */

#define NOSYNC_ALARMS (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | \
		       DAHDI_ALARM_LOOPBACK)

static int debug;
static int timingcable;
static int force_firmware;
static int alarmdebounce	= 2500; /* LOF/LFA def to 2.5s AT&T TR54016*/
static int losalarmdebounce	= 2500; /* LOS def to 2.5s AT&T TR54016*/
static int aisalarmdebounce	= 2500; /* AIS(blue) def to 2.5s AT&T TR54016*/
static int yelalarmdebounce	= 500;  /* RAI(yellow) def to 0.5s AT&T guide */
static char *default_linemode	= "t1"; /* 'e1', 't1', or 'j1' */
static int latency		= WCXB_DEFAULT_LATENCY;
static int max_latency		= WCXB_DEFAULT_MAXLATENCY;

struct t43x_firm_header {
	u8	header[6];
	__le32	chksum;
	u8	pad[18];
	__le32	version;
} __packed;

static int t43x_check_for_interrupts(struct t43x *wc);
static void t43x_check_alarms(struct t43x *wc, int span_idx);
static void t43x_check_sigbits(struct t43x *wc, int span_idx);
static const struct dahdi_span_ops t43x_span_ops;
static void __t43x_set_timing_source_auto(struct t43x *wc);

#ifndef VPM_SUPPORT
static int vpmsupport;
#else
static int vpmsupport = 1;

#include "oct6100api/oct6100_api.h"

#define ECHOCAN_NUM_CHANS 128

#define FLAGBIT_DTMF	1
#define FLAGBIT_MUTE	2
#define FLAGBIT_ECHO	3
#define FLAGBIT_ALAW	4

#define OCT_CHIP_ID			0
#define OCT_MAX_TDM_STREAMS		4
#define OCT_TONEEVENT_BUFFER_SIZE	128
#define SOUT_STREAM			1
#define RIN_STREAM			0
#define SIN_STREAM			2

static void t43x_vpm_init(struct t43x *wc);
static void echocan_free(struct dahdi_chan *chan,
		struct dahdi_echocan_state *ec);
static const struct dahdi_echocan_features vpm_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};
static const struct dahdi_echocan_ops vpm_ec_ops = {
	.echocan_free = echocan_free,
};

struct vpm450m {
	tPOCT6100_INSTANCE_API pApiInstance;
	struct oct612x_context context;
	UINT32 aulEchoChanHndl[ECHOCAN_NUM_CHANS];
	int ecmode[ECHOCAN_NUM_CHANS];
	unsigned long chanflags[ECHOCAN_NUM_CHANS];
};

static int t43x_oct612x_write(struct oct612x_context *context,
			      u32 address, u16 value)
{
	struct t43x *wc = dev_get_drvdata(context->dev);
	wcxb_set_echocan_reg(&wc->xb, address, value);
	return 0;
}

static int t43x_oct612x_read(struct oct612x_context *context, u32 address,
			     u16 *value)
{
	struct t43x *wc = dev_get_drvdata(context->dev);
	*value = wcxb_get_echocan_reg(&wc->xb, address);
	return 0;
}

static int t43x_oct612x_write_smear(struct oct612x_context *context,
				    u32 address, u16 value, size_t count)
{
	unsigned int i;
	struct t43x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		wcxb_set_echocan_reg(&wc->xb, address + (i << 1), value);
	return 0;
}

static int t43x_oct612x_write_burst(struct oct612x_context *context,
				    u32 address, const u16 *buffer,
				    size_t count)
{
	unsigned int i;
	struct t43x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		wcxb_set_echocan_reg(&wc->xb, address + (i << 1), buffer[i]);
	return 0;
}

static int t43x_oct612x_read_burst(struct oct612x_context *context,
				   u32 address, u16 *buffer, size_t count)
{
	unsigned int i;
	struct t43x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		buffer[i] = wcxb_get_echocan_reg(&wc->xb, address + (i << 1));
	return 0;
}

static const struct oct612x_ops t43x_oct612x_ops = {
	.write = t43x_oct612x_write,
	.read = t43x_oct612x_read,
	.write_smear = t43x_oct612x_write_smear,
	.write_burst = t43x_oct612x_write_burst,
	.read_burst = t43x_oct612x_read_burst,
};

static void vpm450m_setecmode(struct vpm450m *vpm450m, int channel, int mode)
{
	tOCT6100_CHANNEL_MODIFY *modify;
	UINT32 ulResult;

	if (vpm450m->ecmode[channel] == mode)
		return;
	modify = kzalloc(sizeof(tOCT6100_CHANNEL_MODIFY), GFP_ATOMIC);
	if (!modify) {
		pr_notice("Unable to allocate memory for setec!\n");
		return;
	}
	Oct6100ChannelModifyDef(modify);
	modify->ulEchoOperationMode = mode;
	modify->ulChannelHndl = vpm450m->aulEchoChanHndl[channel];
	ulResult = Oct6100ChannelModify(vpm450m->pApiInstance, modify);
	if (ulResult != GENERIC_OK) {
		pr_notice("Failed to apply echo can changes on channel %d %d %08x!\n",
			  vpm450m->aulEchoChanHndl[channel], channel, ulResult);
	} else {
#ifdef OCTASIC_DEBUG
		pr_debug("Echo can on channel %d set to %d\n", channel, mode);
#endif
		vpm450m->ecmode[channel] = mode;
	}
	kfree(modify);
}

static void vpm450m_set_alaw_companding(struct vpm450m *vpm450m, int channel,
					bool alaw)
{
	tOCT6100_CHANNEL_MODIFY *modify;
	UINT32 ulResult;
	UINT32		law_to_use = (alaw) ? cOCT6100_PCM_A_LAW :
					      cOCT6100_PCM_U_LAW;

	/* If we're already in this companding mode, no need to do anything. */
	if (alaw == (test_bit(FLAGBIT_ALAW, &vpm450m->chanflags[channel]) > 0))
		return;

	modify = kzalloc(sizeof(tOCT6100_CHANNEL_MODIFY), GFP_ATOMIC);
	if (!modify) {
		pr_notice("Unable to allocate memory for setec!\n");
		return;
	}

	Oct6100ChannelModifyDef(modify);
	modify->ulChannelHndl =		      vpm450m->aulEchoChanHndl[channel];
	modify->fTdmConfigModified =		TRUE;
	modify->TdmConfig.ulSinPcmLaw =		law_to_use;
	modify->TdmConfig.ulRinPcmLaw =		law_to_use;
	modify->TdmConfig.ulSoutPcmLaw =	law_to_use;
	modify->TdmConfig.ulRoutPcmLaw =	law_to_use;
	ulResult = Oct6100ChannelModify(vpm450m->pApiInstance, modify);
	if (ulResult != GENERIC_OK) {
		pr_notice("Failed to apply echo can changes on channel %d %d %08x!\n",
			  vpm450m->aulEchoChanHndl[channel], channel, ulResult);
	} else {
		if (debug) {
			pr_info("Changed companding on channel %d to %s.\n",
				channel, (alaw) ? "alaw" : "ulaw");
		}
		if (alaw)
			set_bit(FLAGBIT_ALAW, &vpm450m->chanflags[channel]);
		else
			clear_bit(FLAGBIT_ALAW, &vpm450m->chanflags[channel]);
	}
	kfree(modify);
}

static void vpm450m_setec(struct vpm450m *vpm450m, int channel, int eclen)
{
	if (eclen) {
		set_bit(FLAGBIT_ECHO, &vpm450m->chanflags[channel]);
		vpm450m_setecmode(vpm450m, channel,
				cOCT6100_ECHO_OP_MODE_NORMAL);
	} else {
		unsigned long *flags = &vpm450m->chanflags[channel];
		clear_bit(FLAGBIT_ECHO, &vpm450m->chanflags[channel]);
		if (test_bit(FLAGBIT_DTMF, flags) ||
		    test_bit(FLAGBIT_MUTE, flags)) {
			vpm450m_setecmode(vpm450m, channel,
					cOCT6100_ECHO_OP_MODE_HT_RESET);
		} else {
			vpm450m_setecmode(vpm450m, channel,
					cOCT6100_ECHO_OP_MODE_POWER_DOWN);
		}
	}
}

static UINT32 tdmmode_chan_to_slot_map(int channel)
{
	/* Four phases on the tdm bus, skip three of them per channel */
	/* Due to a bug in the octasic, we had to move the data onto phase 2 */
	return  1 + ((channel % 32) * 4) + (channel / 32);
}

static int echocan_initialize_channel(
		struct vpm450m *vpm, int channel, int mode)
{
	tOCT6100_CHANNEL_OPEN	ChannelOpen;
	UINT32		law_to_use = (mode) ? cOCT6100_PCM_A_LAW :
					      cOCT6100_PCM_U_LAW;
	UINT32		tdmslot_setting;
	UINT32		ulResult;

	tdmslot_setting = tdmmode_chan_to_slot_map(channel);

	/* Fill Open channel structure with defaults */
	Oct6100ChannelOpenDef(&ChannelOpen);

	/* Assign the handle memory.*/
	ChannelOpen.pulChannelHndl = &vpm->aulEchoChanHndl[channel];
	ChannelOpen.ulUserChanId = channel;
	/* Enable Tone disabling for Fax and Modems */
	ChannelOpen.fEnableToneDisabler = TRUE;

	/* Passthrough TDM data by default, no echocan */
	ChannelOpen.ulEchoOperationMode = cOCT6100_ECHO_OP_MODE_POWER_DOWN;

	/* Configure the TDM settings.*/
	/* Input from the framer */
	ChannelOpen.TdmConfig.ulSinStream		= SIN_STREAM;
	ChannelOpen.TdmConfig.ulSinTimeslot		= tdmslot_setting;
	ChannelOpen.TdmConfig.ulSinPcmLaw		= law_to_use;

	/* Input from the Host (pre-framer) */
	ChannelOpen.TdmConfig.ulRinStream		= RIN_STREAM;
	ChannelOpen.TdmConfig.ulRinTimeslot		= tdmslot_setting;
	ChannelOpen.TdmConfig.ulRinPcmLaw		= law_to_use;

	/* Output to the Host */
	ChannelOpen.TdmConfig.ulSoutStream		= SOUT_STREAM;
	ChannelOpen.TdmConfig.ulSoutTimeslot		= tdmslot_setting;
	ChannelOpen.TdmConfig.ulSoutPcmLaw		= law_to_use;

	/* From asterisk after echo-cancellation - goes nowhere */
	ChannelOpen.TdmConfig.ulRoutStream		= cOCT6100_UNASSIGNED;
	ChannelOpen.TdmConfig.ulRoutTimeslot		= cOCT6100_UNASSIGNED;
	ChannelOpen.TdmConfig.ulRoutPcmLaw		= law_to_use;

	/* Set the desired VQE features.*/
	ChannelOpen.VqeConfig.fEnableNlp		= TRUE;
	ChannelOpen.VqeConfig.fRinDcOffsetRemoval	= TRUE;
	ChannelOpen.VqeConfig.fSinDcOffsetRemoval	= TRUE;
	ChannelOpen.VqeConfig.ulComfortNoiseMode =
						cOCT6100_COMFORT_NOISE_NORMAL;

	/* Open the channel.*/
	ulResult = Oct6100ChannelOpen(vpm->pApiInstance, &ChannelOpen);

	return ulResult;
}

static struct vpm450m *init_vpm450m(struct t43x *wc, int *laws, int numspans,
					const struct firmware *firmware)
{
	tOCT6100_CHIP_OPEN *ChipOpen;
	tOCT6100_GET_INSTANCE_SIZE InstanceSize;
	tOCT6100_CHANNEL_OPEN *ChannelOpen;
	UINT32 ulResult;
	struct vpm450m *vpm450m;
	int x, i;

	vpm450m = kzalloc(sizeof(struct vpm450m), GFP_KERNEL);
	if (!vpm450m) {
		dev_info(&wc->xb.pdev->dev,
			 "Unable to allocate vpm450m struct\n");
		return NULL;
	}

	vpm450m->context.dev = &wc->xb.pdev->dev;
	vpm450m->context.ops = &t43x_oct612x_ops;

	ChipOpen = kzalloc(sizeof(tOCT6100_CHIP_OPEN), GFP_KERNEL);
	if (!ChipOpen) {
		dev_info(&wc->xb.pdev->dev, "Unable to allocate ChipOpen\n");
		kfree(vpm450m);
		return NULL;
	}

	ChannelOpen = kzalloc(sizeof(tOCT6100_CHANNEL_OPEN), GFP_KERNEL);
	if (!ChannelOpen) {
		dev_info(&wc->xb.pdev->dev, "Unable to allocate ChannelOpen\n");
		kfree(vpm450m);
		kfree(ChipOpen);
		return NULL;
	}

	for (x = 0; x < ARRAY_SIZE(vpm450m->ecmode); x++)
		vpm450m->ecmode[x] = -1;

	dev_info(&wc->xb.pdev->dev, "Echo cancellation for %d channels\n",
		wc->numspans * 32);

	Oct6100ChipOpenDef(ChipOpen);
	ChipOpen->pProcessContext = &vpm450m->context;

	/* Change default parameters as needed */
	/* upclk oscillator is at 33.33 Mhz */
	ChipOpen->ulUpclkFreq = cOCT6100_UPCLK_FREQ_33_33_MHZ;

	/* mclk will be generated by internal PLL at 133 Mhz */
	ChipOpen->fEnableMemClkOut	= TRUE;
	ChipOpen->ulMemClkFreq		= cOCT6100_MCLK_FREQ_133_MHZ;

	/* User defined Chip ID.*/
	ChipOpen->ulUserChipId		= OCT_CHIP_ID;

	/* Set the maximums that the chip needs to support */
	ChipOpen->ulMaxChannels		= wc->numspans * 32;
	ChipOpen->ulMaxTdmStreams	= OCT_MAX_TDM_STREAMS;

	/* External Memory Settings */
	/* Use DDR memory.*/
	ChipOpen->ulMemoryType		= cOCT6100_MEM_TYPE_DDR;
	ChipOpen->ulNumMemoryChips	= 1;
	ChipOpen->ulMemoryChipSize	= cOCT6100_MEMORY_CHIP_SIZE_32MB;

	ChipOpen->pbyImageFile = (PUINT8) firmware->data;
	ChipOpen->ulImageSize = firmware->size;

	/* Set TDM data stream frequency */
	for (i = 0; i < ChipOpen->ulMaxTdmStreams; i++)
		ChipOpen->aulTdmStreamFreqs[i] = cOCT6100_TDM_STREAM_FREQ_8MHZ;

	/* Configure TDM sampling */
	ChipOpen->ulTdmSampling = cOCT6100_TDM_SAMPLE_AT_FALLING_EDGE;
	/* Disable to save RAM footprint space */
	ChipOpen->fEnableChannelRecording = false;

	/* In this example we will maintain the API using polling so
	   interrupts must be disabled */
	ChipOpen->InterruptConfig.ulErrorH100Config =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulErrorMemoryConfig =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulFatalGeneralConfig =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulFatalMemoryConfig =
						cOCT6100_INTERRUPT_DISABLE;

	ChipOpen->ulSoftToneEventsBufSize = OCT_TONEEVENT_BUFFER_SIZE;

	/* Inserting default values into tOCT6100_GET_INSTANCE_SIZE
	   structure parameters. */
	Oct6100GetInstanceSizeDef(&InstanceSize);

	/* Reset octasic device */
	wcxb_reset_echocan(&wc->xb);

	/* Get the size of the OCT6100 instance structure. */
	ulResult = Oct6100GetInstanceSize(ChipOpen, &InstanceSize);
	if (ulResult != cOCT6100_ERR_OK) {
		dev_info(&wc->xb.pdev->dev, "Unable to get instance size: %x\n",
				ulResult);
		return NULL;
	}

	vpm450m->pApiInstance = vmalloc(InstanceSize.ulApiInstanceSize);
	if (!vpm450m->pApiInstance) {
		dev_info(&wc->xb.pdev->dev,
			"Out of memory (can't allocate %d bytes)!\n",
			InstanceSize.ulApiInstanceSize);
		return NULL;
	}

	/* Perform the actual configuration of the chip. */
	wcxb_enable_echocan_dram(&wc->xb);
	ulResult = Oct6100ChipOpen(vpm450m->pApiInstance, ChipOpen);
	if (ulResult != cOCT6100_ERR_OK) {
		dev_info(&wc->xb.pdev->dev, "Unable to Oct6100ChipOpen: %x\n",
				ulResult);
		return NULL;
	}

	/* OCT6100 is now booted and channels can be opened */
	/* Open 31 channels/span since we're skipping the first on the VPM */
	for (x = 0; x < numspans; x++) {
		for (i = 0; i < 31; i++) {
			ulResult = echocan_initialize_channel(vpm450m,
							      (x*32)+i,
							      laws[x]);
			if (0 != ulResult) {
				dev_info(&wc->xb.pdev->dev,
					"Unable to echocan_initialize_channel: %d %x\n",
					(x*32)+i, ulResult);
				return NULL;
			} else if (laws[x]) {
				set_bit(FLAGBIT_ALAW,
					&vpm450m->chanflags[(x*32)+i]);
			}
		}
	}

	if (vpmsupport != 0)
		wcxb_enable_echocan(&wc->xb);

	kfree(ChipOpen);
	kfree(ChannelOpen);
	return vpm450m;
}

static void release_vpm450m(struct vpm450m *vpm450m)
{
	UINT32 ulResult;
	tOCT6100_CHIP_CLOSE ChipClose;

	Oct6100ChipCloseDef(&ChipClose);
	ulResult = Oct6100ChipClose(vpm450m->pApiInstance, &ChipClose);
	if (ulResult != cOCT6100_ERR_OK)
		pr_notice("Failed to close chip, code %08x!\n", ulResult);
	vfree(vpm450m->pApiInstance);
	kfree(vpm450m);
}

static const char *__t43x_echocan_name(struct t43x *wc)
{
	if (wc->vpm)
		return wc->vpm_name;
	else
		return NULL;
}

static const char *t43x_echocan_name(const struct dahdi_chan *chan)
{
	struct t43x *wc = chan->pvt;
	return __t43x_echocan_name(wc);
}

static int t43x_echocan_create(struct dahdi_chan *chan,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p,
			     struct dahdi_echocan_state **ec)
{
	struct t43x *wc = chan->pvt;
	struct t43x_span *ts = container_of(chan->span, struct t43x_span, span);
	int channel = chan->chanpos - 1;
	const struct dahdi_echocan_ops *ops;
	const struct dahdi_echocan_features *features;
	const bool alaw = (chan->span->deflaw == 2);

	if (!vpmsupport || !wc->vpm)
		return -ENODEV;

	ops = &vpm_ec_ops;
	features = &vpm_ec_features;

	if (ecp->param_count > 0) {
		dev_warn(&wc->xb.pdev->dev,
			 "%s echo canceller does not support parameters; failing request\n",
			 chan->ec_factory->get_name(chan));
		return -EINVAL;
	}

	*ec = ts->ec[channel];
	(*ec)->ops = ops;
	(*ec)->features = *features;

	channel += (32*chan->span->offset);
	vpm450m_set_alaw_companding(wc->vpm, channel, alaw);
	vpm450m_setec(wc->vpm, channel, ecp->tap_length);
	return 0;
}

static void echocan_free(struct dahdi_chan *chan,
			struct dahdi_echocan_state *ec)
{
	struct t43x *wc = chan->pvt;
	int channel = chan->chanpos - 1;
	if (!wc->vpm)
		return;
	memset(ec, 0, sizeof(*ec));
	channel += (32*chan->span->offset);
	vpm450m_setec(wc->vpm, channel, 0);
}

static void t43x_vpm_init(struct t43x *wc)
{
	int laws[8] = {0, };
	int x;

	struct firmware embedded_firmware;
	const struct firmware *firmware = &embedded_firmware;
#if !defined(HOTPLUG_FIRMWARE)
	extern void _binary_dahdi_fw_oct6114_064_bin_size;
	extern u8 _binary_dahdi_fw_oct6114_064_bin_start[];
	extern void _binary_dahdi_fw_oct6114_128_bin_size;
	extern u8 _binary_dahdi_fw_oct6114_128_bin_start[];
#else
	static const char oct064_firmware[] = "dahdi-fw-oct6114-064.bin";
	static const char oct128_firmware[] = "dahdi-fw-oct6114-128.bin";
	const char *oct_firmware;
#endif

	if (!vpmsupport) {
		dev_info(&wc->xb.pdev->dev, "VPM450: Support Disabled\n");
		return;
	}

#if defined(HOTPLUG_FIRMWARE)
	if (wc->numspans == 2) {
		wc->vpm_name = "VPMOCT064";
		oct_firmware = oct064_firmware;
	} else {
		wc->vpm_name = "VPMOCT128";
		oct_firmware = oct128_firmware;
	}
	if ((request_firmware(&firmware, oct_firmware, &wc->xb.pdev->dev) != 0)
			|| !firmware) {
		dev_notice(&wc->xb.pdev->dev,
			   "VPM450: firmware %s not available from userspace\n",
			   oct_firmware);
		return;
	}
#else
	/* Yes... this is weird. objcopy gives us a symbol containing
	   the size of the firmware, not a pointer a variable containing
	   the size. The only way we can get the value of the symbol
	   is to take its address, so we define it as a pointer and
	   then cast that value to the proper type.
	*/
	if (wc->numspans == 2) {
		embedded_firmware.data = _binary_dahdi_fw_oct6114_064_bin_start;
		embedded_firmware.size =
				(size_t)&_binary_dahdi_fw_oct6114_064_bin_size;
	} else {
		embedded_firmware.size =
				(size_t)&_binary_dahdi_fw_oct6114_128_bin_size;
		embedded_firmware.data = _binary_dahdi_fw_oct6114_128_bin_start;
	}
#endif

	/* Setup alaw vs ulaw rules */
	for (x = 0; x < wc->numspans; x++) {
		if (wc->tspans[x]->span.channels > 24)
			laws[x] = 1;
	}

	wc->vpm = init_vpm450m(wc, laws, wc->numspans, firmware);

	if (!wc->vpm) {
		dev_notice(&wc->xb.pdev->dev, "VPM450: Failed to initialize\n");
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		return;
	}

	if (firmware != &embedded_firmware)
		release_firmware(firmware);

	dev_info(&wc->xb.pdev->dev,
		"VPM450: Present and operational servicing %d span(s)\n",
		wc->numspans);

}
#endif /* VPM_SUPPORT */

static int t43x_clear_maint(struct dahdi_span *span);

static DEFINE_MUTEX(card_list_lock);
static LIST_HEAD(card_list);

struct t43x_desc {
	const char *name;
};

static const struct t43x_desc te435 = {"Wildcard TE435"}; /* pci express quad */
static const struct t43x_desc te235 = {"Wildcard TE235"}; /* pci express dual */

static int __t43x_pci_get(struct t43x *wc, unsigned int addr)
{
	unsigned int res = ioread8(wc->xb.membase + addr);
	return res;
}

static inline int __t43x_pci_set(struct t43x *wc, unsigned int addr, int val)
{
	iowrite8(val, wc->xb.membase + addr);
	__t43x_pci_get(wc, 0);
	return 0;
}

static inline int t43x_pci_get(struct t43x *wc, int addr)
{
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t43x_pci_get(wc, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline int t43x_pci_set(struct t43x *wc, int addr, int val)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t43x_pci_set(wc, addr, val);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline int
__t43x_framer_set(struct t43x *wc, int unit, int addr, int val)
{
	return __t43x_pci_set(wc, FRAMER_BASE + (unit << 8) + addr, val);
}

static inline int t43x_framer_set(struct t43x *wc, int unit, int addr, int val)
{
	return t43x_pci_set(wc, FRAMER_BASE + (unit << 8) + addr, val);
}

static inline int __t43x_framer_get(struct t43x *wc, int unit, int addr)
{
	return __t43x_pci_get(wc, FRAMER_BASE + (unit << 8) + addr);
}

static inline int t43x_framer_get(struct t43x *wc, int unit, int addr)
{
	return t43x_pci_get(wc, FRAMER_BASE + (unit << 8) + addr);
}

static void t43x_framer_reset(struct t43x *wc)
{
	/*
	 * When the framer is reset, RCLK will stop. The FPGA must be switched
	 * to it's internal clock when this happens, but it's only safe to
	 * switch the clock source on the FPGA when the DMA engine is stopped.
	 *
	 */
	wcxb_stop_dma(&wc->xb);
	wcxb_wait_for_stop(&wc->xb, 50);
	wcxb_set_clksrc(&wc->xb, WCXB_CLOCK_SELF);
	wcxb_gpio_clear(&wc->xb, FALC_CPU_RESET);
	msleep_interruptible(100);
	wcxb_gpio_set(&wc->xb, FALC_CPU_RESET);
}

static void t43x_setleds(struct t43x *wc, u32 leds)
{
	wcxb_gpio_set(&wc->xb, leds & LED_MASK);
	wcxb_gpio_clear(&wc->xb, ~leds & LED_MASK);
}

static void t43x_set_cas_mode(struct t43x *wc, int span_idx)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;
	int i, offset;
	int reg;
	unsigned long flags;
	bool span_has_cas_channel = false;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s span: %d\n", __func__,
				span_idx);

	if (dahdi_is_e1_span(&ts->span)) {
		span_has_cas_channel = !(ts->span.lineconfig&DAHDI_CONFIG_CCS);
	} else {
		unsigned char ccb[3] = {0, 0, 0};
		/* Sort out channels that use CAS signalling */
		for (i = 0; i < ts->span.channels; i++) {
			offset = i/8;
			if (offset >= ARRAY_SIZE(ccb)) {
				WARN_ON(1);
				break;
			}
			if (ts->span.chans[i]->flags & DAHDI_FLAG_CLEAR)
				ccb[offset] |= 1 << (7 - (i % 8));
			else
				ccb[offset] &= ~(1 << (7 - (i % 8)));
		}

		spin_lock_irqsave(&wc->reglock, flags);
		__t43x_framer_set(wc, fidx, CCB1, ccb[0]);
		__t43x_framer_set(wc, fidx, CCB2, ccb[1]);
		__t43x_framer_set(wc, fidx, CCB3, ccb[2]);
		spin_unlock_irqrestore(&wc->reglock, flags);

		if ((~ccb[0]) | (~ccb[1]) | (~ccb[2]))
			span_has_cas_channel = true;
	}

	/* Unmask CAS RX interrupt if any single channel is in CAS mode */
	/* This interrupt is called RSC in T1 and CASC in E1 */
	spin_lock_irqsave(&wc->reglock, flags);
	reg = __t43x_framer_get(wc, fidx, IMR0);
	if (span_has_cas_channel)
		__t43x_framer_set(wc, fidx, IMR0, reg & ~0x08);
	else
		__t43x_framer_set(wc, fidx, IMR0, reg | 0x08);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

/**
 * _t43x_free_channels - Free the memory allocated for the channels.
 *
 * Must be called with wc->reglock held.
 *
 */
static void _t43x_free_channels(struct t43x *wc)
{
	unsigned int x, y;

	for (x = 0; x < ARRAY_SIZE(wc->tspans); x++) {
		if (!wc->tspans[x])
			continue;
		for (y = 0; y < ARRAY_SIZE(wc->tspans[x]->chans); y++) {
			kfree(wc->tspans[x]->chans[y]);
			kfree(wc->tspans[x]->ec[y]);
		}
		kfree(wc->tspans[x]);
	}
}

static void free_wc(struct t43x *wc)
{
	unsigned long flags;
	LIST_HEAD(list);

	mutex_lock(&card_list_lock);
	list_del(&wc->card_node);
	mutex_unlock(&card_list_lock);

	spin_lock_irqsave(&wc->reglock, flags);
	_t43x_free_channels(wc);
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (wc->wq)
		destroy_workqueue(wc->wq);
	kfree(wc->ddev->location);
	kfree(wc->ddev->devicetype);
	kfree(wc->ddev->hardware_id);
	if (wc->ddev)
		dahdi_free_device(wc->ddev);
	kfree(wc);
}

static void t43x_serial_setup(struct t43x *wc)
{
	unsigned long flags;
	int slot, fidx;

	dev_info(&wc->xb.pdev->dev,
		 "Setting up global serial parameters for card %d\n", wc->num);

	t43x_framer_reset(wc);

	spin_lock_irqsave(&wc->reglock, flags);
	/* GPC1: Multiplex mode enabled, FSC is output, active low, RCLK from
	 * channel 0 */
	__t43x_framer_set(wc, 0, 0x85, 0xe0);
	/* GPC3: Enable Multi Purpose Switches */
	__t43x_framer_set(wc, 0, 0xd3, 0xa1);
	/* GPC4: Enable Multi Purpose Switches */
	__t43x_framer_set(wc, 0, 0xd4, 0x83);
	/* GPC5: Enable Multi Purpose Switches */
	__t43x_framer_set(wc, 0, 0xd5, 0xe5);
	/* GPC4: Enable Multi Purpose Switches */
	__t43x_framer_set(wc, 0, 0xd6, 0x87);
	/* IPC: Interrupt push/pull active low, 8KHz ref clk */
	__t43x_framer_set(wc, 0, 0x08, 0x05);

	/* Global clocks (8.192 Mhz CLK) */
	__t43x_framer_set(wc, 0, 0x92, 0x00);
	__t43x_framer_set(wc, 0, 0x93, 0x18);
	__t43x_framer_set(wc, 0, 0x94, 0xfb);
	__t43x_framer_set(wc, 0, 0x95, 0x0b);
	__t43x_framer_set(wc, 0, 0x96, 0x00);
	__t43x_framer_set(wc, 0, 0x97, 0x0b);
	__t43x_framer_set(wc, 0, 0x98, 0xdb);
	__t43x_framer_set(wc, 0, 0x99, 0xdf);

	spin_unlock_irqrestore(&wc->reglock, flags);

	for (fidx = 0; fidx < 4; fidx++) {
		/* For dual-span, put the second and third framers
		 * onto the first and second timeslots */
		if (wc->numspans == 2)
			slot = (fidx - 1) & 3;
		else
			slot = fidx;
		spin_lock_irqsave(&wc->reglock, flags);

		/* Configure interrupts */

		/* GCR: Interrupt on Activation/Deactivation of AIX, LOS */
		__t43x_framer_set(wc, fidx, 0x46, 0xc0);

		/* Configure system interface */

		/* SIC1: 8.192 Mhz clock/bus, double buffer receive /
		 * transmit, byte interleaved */
		__t43x_framer_set(wc, fidx, 0x3e, 0xc2);
		/* SIC2: No FFS, no CRB, SSC2 = 0, phase = unit */
		__t43x_framer_set(wc, fidx, 0x3f, (slot << 1));

		/* SIC3: Edges for capture - original rx rising edge */
		__t43x_framer_set(wc, fidx, 0x40, 0x04);
		/* SIC4: CES high, SYPR opposite edge */
		__t43x_framer_set(wc, fidx, 0x2a, 0x06);

#ifndef RPC_RCLK
		/* CMR1: RCLK is at 8.192 Mhz dejittered */
		__t43x_framer_set(wc, fidx, 0x44, 0x30);
#else
		/* CMR1: RCLK is at 8.192 Mhz dejittered, ref clock is this
		 * channel */
		__t43x_framer_set(wc, fidx, 0x44, 0x38 | (fidx << 6));
#endif

		/* CMR2: We provide sync and clock for tx and rx. */
		__t43x_framer_set(wc, fidx, 0x45, 0x00);

		/* XC0: Normal operation of Sa-bits */
		__t43x_framer_set(wc, fidx, 0x22, 0x07);
		__t43x_framer_set(wc, fidx, 0x23, 0xfa);	/* XC1 */

		__t43x_framer_set(wc, fidx, 0x24, 0x07);	/* RC0 */
		__t43x_framer_set(wc, fidx, 0x25, 0xfa);	/* RC1 */

		/* Configure ports */
		/* PC1: SPYR/SPYX input on RPA/XPA */
		__t43x_framer_set(wc, fidx, 0x80, 0x00);
		/* PC2: Unused stuff */
		__t43x_framer_set(wc, fidx, 0x81, 0xBB);
#ifndef RPC_RCLK
		/* PC3: Unused stuff */
		__t43x_framer_set(wc, fidx, 0x82, 0xBB);
#else
		/* PC3: RPC is RCLK, XPC is low output */
		__t43x_framer_set(wc, fidx, 0x82, 0xFB);
#endif
		/* PC4: Unused stuff */
		__t43x_framer_set(wc, fidx, 0x83, 0xBB);
		/* PC5: XMFS active low, SCLKR is input, RCLK is output */
		__t43x_framer_set(wc, fidx, 0x84, 0x01);
		__t43x_framer_set(wc, fidx, 0x3b, 0x00);	/* Clear LCR1 */

		/* Make sure unused ports are set to T1 configuration as a
		 * default. This is required in order to make clock recovery
		 * works on spans that are actually configured. */

		__t43x_framer_set(wc, fidx, 0x1c, 0xf0);
		__t43x_framer_set(wc, fidx, 0x1d, 0x9c);
		__t43x_framer_set(wc, fidx, 0x1e, 0x20);
		__t43x_framer_set(wc, fidx, 0x20, 0x0c);
		/* FMR5: Enable RBS mode */
		__t43x_framer_set(wc, fidx, 0x21, 0x40);

		__t43x_framer_set(wc, fidx, 0x36, 0x08);
		__t43x_framer_set(wc, fidx, 0x37, 0xf0);
		__t43x_framer_set(wc, fidx, 0x3a, 0x21);

		__t43x_framer_set(wc, fidx, 0x02, 0x50);
		__t43x_framer_set(wc, fidx, 0x02, 0x00);

		__t43x_framer_set(wc, fidx, 0x38, 0x0a);
		__t43x_framer_set(wc, fidx, 0x39, 0x15);
		/* Tri-state the TX output for the port */
		__t43x_framer_set(wc, fidx, 0x28, 0x40);

		spin_unlock_irqrestore(&wc->reglock, flags);
	}
}

/**
 * t43x_span_assigned - Called when the span is assigned by DAHDI.
 * @span:	Span that has been assigned.
 *
 * When this function is called, the span has a valid spanno and all the
 * channels on the span have valid channel numbers assigned.
 *
 * This function is necessary because a device may be registered, and
 * then user space may then later decide to assign span numbers and the
 * channel numbers.
 *
 */
static void t43x_span_assigned(struct dahdi_span *span)
{
	struct t43x_span *tspan = container_of(span, struct t43x_span, span);
	struct t43x *wc  = tspan->owner;
	struct dahdi_span *pos;
	unsigned int unassigned_spans = 0;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	span->alarms = DAHDI_ALARM_NONE;

	/* We use this to make sure all the spans are assigned before
	 * running the serial setup. */
	list_for_each_entry(pos, &wc->ddev->spans, device_node) {
		if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &pos->flags))
			++unassigned_spans;
	}

	if (0 == unassigned_spans) {
		/* Now all the spans are assigned so we can go ahead and start
		 * things up. */
		t43x_serial_setup(wc);
	}
}

static int syncsrc;
static int syncnum;
static int syncspan;
static DEFINE_SPINLOCK(synclock);

static void __t43x_set_rclk_src(struct t43x *wc, int span)
{
#ifndef RPC_RCLK
	int cmr1 = 0x38;	/* Clock Mode: RCLK sourced by DCO-R1
				   by default, Disable Clock-Switching */
	int fidx;

	if (2 == wc->numspans) {
		u8 reg;

		/* Since the clock always comes from the first span (which is
		 * not connected to a physical port on the dual span) we can
		 * always set the framer mode register to match whatever span
		 * we're currently sourcing the timing from. This ensures that
		 * the DCO-R is always expecting the right clock speed from the
		 * line. */

		fidx = span+1;

		reg = __t43x_framer_get(wc, fidx, 0x1d);
		__t43x_framer_set(wc, 0, 0x1d, reg);

		reg = __t43x_framer_get(wc, fidx, 0x1e);
		__t43x_framer_set(wc, 0, 0x1e, reg);

	} else {
		fidx = span;
	}

	cmr1 |= (fidx) << 6;
	__t43x_framer_set(wc, 0, 0x44, cmr1);
#else
	int fidx = (2 == wc->numspans) ? span+1 : span;
	int gpc1 = __t43x_framer_get(wc, 0, 0x85);

	gpc1 &= ~3;
	gpc1 |= fidx & 3;

	t43x_framer_set(wc, 0, 0x85, gpc1);
#endif
	dev_info(&wc->xb.pdev->dev, "RCLK source set to span %d\n", span+1);
}

/* This is called from the workqueue to wait for the TDM engine stop */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void t43x_clksrc_work_fn(void *data)
{
	struct t43x_clksrc_work  *work = data;
#else
static void t43x_clksrc_work_fn(struct work_struct *data)
{
	struct t43x_clksrc_work  *work = container_of(to_delayed_work(data),
						struct t43x_clksrc_work, work);
#endif
	struct t43x *wc = container_of(work, struct t43x, clksrc_work);

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "t43x_clksrc_work() called from queue\n");
	}

	if (wcxb_is_stopped(&wc->xb)) {

		/* Set new clock select */
		if (work->is_timing_master)
			wcxb_enable_timing_header_driver(&wc->xb);
		else
			wcxb_disable_timing_header_driver(&wc->xb);
		wcxb_set_clksrc(&wc->xb, work->clksrc);

		/* Restart DMA processing */
		wcxb_start(&wc->xb);
	} else {
		/* Stop DMA again in case DMA underrun int restarted it */
		wcxb_stop_dma(&wc->xb);
		queue_delayed_work(wc->wq, &work->work, msecs_to_jiffies(10));
	}
}

/**
 * __t43x_set_sclk_src - Change the current source of the source clock.
 * @wc:		The board to change the clock source on.
 * @mode:	The clock mode that we would like to move to.
 * @master:	If true, drive the clock on the timing header.
 *
 * The clock srouce cannot be changed while DMA is active, so this function
 * will stop the DMA, then queue a delayed work item in order to come back and
 * check that DMA was actually stopped before changing the source of the clock.
 *
 */
static void
__t43x_set_sclk_src(struct t43x *wc, enum wcxb_clock_sources mode, bool master)
{
	struct t43x_clksrc_work *const work = &wc->clksrc_work;
	unsigned long flags;
	bool changed = false;

	/* Cannot drive the clock on the header while also slaving from it. */
	WARN_ON(master && (mode == WCXB_CLOCK_SLAVE));

	spin_lock_irqsave(&work->lock, flags);
	if (!delayed_work_pending(&work->work)) {
		/* We want to check the actual settings. */
		changed = (wcxb_get_clksrc(&wc->xb) != mode) ||
		    (wcxb_is_timing_header_driver_enabled(&wc->xb) != master);
	} else {
		/* Otherwise, we'll check if delayed work is going to set it to
		 * the same value. */

		 changed = (work->clksrc != mode) ||
			   (work->is_timing_master != master);
	}
	if (changed) {
		work->clksrc = mode;
		work->is_timing_master = master;
	}
	spin_unlock_irqrestore(&work->lock, flags);

	if (!changed) {
		if (debug)
			dev_info(&wc->xb.pdev->dev, "Clock source is unchanged\n");
		return;
	}

	wcxb_stop_dma(&wc->xb);

	dev_dbg(&wc->xb.pdev->dev,
		"Queueing delayed work for clock source change\n");

	queue_delayed_work(wc->wq, &work->work, msecs_to_jiffies(10));
}

static ssize_t t43x_timing_master_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct t43x *wc = dev_get_drvdata(dev);

	if (wcxb_is_timing_header_driver_enabled(&wc->xb))
		return sprintf(buf, "%d\n", wc->syncsrc);
	else
		return sprintf(buf, "%d\n", -1);
}

static DEVICE_ATTR(timing_master, 0400, t43x_timing_master_show, NULL);

static void create_sysfs_files(struct t43x *wc)
{
	int ret;
	ret = device_create_file(&wc->xb.pdev->dev,
				 &dev_attr_timing_master);
	if (ret) {
		dev_info(&wc->xb.pdev->dev,
			"Failed to create device attributes.\n");
	}
}

static void remove_sysfs_files(struct t43x *wc)
{
	device_remove_file(&wc->xb.pdev->dev, &dev_attr_timing_master);
}

static inline void __t43x_update_timing(struct t43x *wc)
{
	int i;

	/* update sync src info */
	if (wc->syncsrc != syncsrc) {
		dev_info(&wc->xb.pdev->dev, "Swapping card %d from %d to %d\n",
			 wc->num, wc->syncsrc, syncsrc);
		wc->syncsrc = syncsrc;
		/* Update sync sources */
		for (i = 0; i < wc->numspans; i++)
			wc->tspans[i]->span.syncsrc = wc->syncsrc;
		if (syncnum == wc->num) {
			__t43x_set_rclk_src(wc, syncspan-1);
			__t43x_set_sclk_src(wc, WCXB_CLOCK_RECOVER, 1);
			if (debug) {
				dev_notice(&wc->xb.pdev->dev,
					   "Card %d, using sync span %d, master\n",
					   wc->num, syncspan);
			}
		} else {
			__t43x_set_sclk_src(wc, WCXB_CLOCK_SLAVE, 0);
			if (debug) {
				dev_notice(&wc->xb.pdev->dev,
					   "Card %d, using Timing Bus, NOT master\n",
					   wc->num);
			}
		}
	}
}

static int __t43x_findsync(struct t43x *wc)
{
	int i;
	unsigned long flags;
	int p;
	int nonzero;
	int newsyncsrc = 0;		/* DAHDI span number */
	int newsyncnum = 0;		/* wct4xxp card number */
	int newsyncspan = 0;		/* span on given wct4xxp card */
	struct t43x *cur;

	spin_lock_irqsave(&synclock, flags);
	if (!wc->num) {
		/* If we're the first card, go through all the motions, up to 8
		 * levels of sync source */
		p = 1;
		while (p < 8) {
			nonzero = 0;
			list_for_each_entry(cur, &card_list, card_node) {
				for (i = 0; i < cur->numspans; i++) {
					struct t43x_span *const ts =
							cur->tspans[i];
					struct dahdi_span *const s =
							&cur->tspans[i]->span;
					if (!ts->syncpos)
						continue;
					nonzero = 1;
					if ((ts->syncpos == p) &&
					    !(s->alarms & NOSYNC_ALARMS) &&
					    (s->flags & DAHDI_FLAG_RUNNING)) {
						/* This makes a good sync
						 * source */
						newsyncsrc = s->spanno;
						newsyncnum = cur->num;
						newsyncspan = i + 1;
						/* Jump out */
						goto found;
					}
				}
			}
			if (nonzero)
				p++;
			else
				break;
		}
found:
		if ((syncnum != newsyncnum) ||
		    (syncsrc != newsyncsrc) ||
		    (newsyncspan != syncspan)) {
			if (debug) {
				dev_notice(&wc->xb.pdev->dev,
					   "New syncnum: %d (was %d), syncsrc: %d (was %d), syncspan: %d (was %d)\n",
					   newsyncnum, syncnum, newsyncsrc,
					   syncsrc, newsyncspan, syncspan);
			}
			syncnum = newsyncnum;
			syncsrc = newsyncsrc;
			syncspan = newsyncspan;
			nonzero = 0;
			list_for_each_entry(cur, &card_list, card_node)
				__t43x_update_timing(cur);
		}
	}
	__t43x_update_timing(wc);
	spin_unlock_irqrestore(&synclock, flags);
	return 0;
}

static void __t43x_set_timing_source_auto(struct t43x *wc)
{
	int x, i;
	int firstprio, secondprio;
	firstprio = secondprio = 4;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "timing source auto\n");

	if (timingcable) {
		__t43x_findsync(wc);
	} else {
		if (debug)
			dev_info(&wc->xb.pdev->dev,
				 "Evaluating spans for timing source\n");
		for (x = 0; x < wc->numspans; x++) {
			if ((wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) &&
			   !(wc->tspans[x]->span.alarms & (DAHDI_ALARM_RED |
							   DAHDI_ALARM_BLUE))) {
				if (debug) {
					dev_info(&wc->xb.pdev->dev,
						 "span %d is green : syncpos %d\n",
						 x+1, wc->tspans[x]->syncpos);
				}
				if (wc->tspans[x]->syncpos) {
					/* Valid rsync source in recovered
					   timing mode */
					if (firstprio == 4)
						firstprio = x;
					else if (wc->tspans[x]->syncpos <
						wc->tspans[firstprio]->syncpos)
						firstprio = x;
				} else {
					/* Valid rsync source in system timing
					   mode */
					if (secondprio == 4)
						secondprio = x;
				}
			}
		}
		if (firstprio != 4) {
			wc->syncsrc = firstprio;
			__t43x_set_rclk_src(wc, firstprio);
			__t43x_set_sclk_src(wc, WCXB_CLOCK_RECOVER, 0);
			dev_info(&wc->xb.pdev->dev,
				 "Recovered timing mode, RCLK set to span %d\n",
				 firstprio+1);
		} else if (secondprio != 4) {
			wc->syncsrc = -1;
			__t43x_set_rclk_src(wc, secondprio);
			__t43x_set_sclk_src(wc, WCXB_CLOCK_SELF, 0);
			dev_info(&wc->xb.pdev->dev,
				 "System timing mode, RCLK set to span %d\n",
				secondprio+1);
		} else {
			wc->syncsrc = -1;
			dev_info(&wc->xb.pdev->dev,
				 "All spans in alarm : No valid span to source RCLK from\n");
			/* Default rclk to lock with span 1 */
			__t43x_set_rclk_src(wc, 0);
			__t43x_set_sclk_src(wc, WCXB_CLOCK_SELF, 0);
		}

		/* Propagate sync selection to dahdi_span struct
		 * this is read by dahdi_tool to display the span's
		 * master/slave sync information */
		for (i = 0; i < wc->numspans; i++)
			wc->tspans[i]->span.syncsrc = wc->syncsrc + 1;
	}
}

static void
t43x_configure_t1(struct t43x *wc, int span_idx, int lineconfig, int txlevel)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;
	unsigned int fmr4, fmr2, fmr1, fmr0, lim2;
	char *framing, *line;
	int mytxlevel, reg;
	unsigned long flags;

	if ((txlevel > 7) || (txlevel < 4))
		mytxlevel = 0;
	else
		mytxlevel = txlevel - 4;

	/* FMR1: Mode 0, T1 mode, CRC on for ESF, 8.192 Mhz system data rate,
	 * no XAIS */
	fmr1 = 0x9c;
	/* FMR2: no payload loopback, don't auto yellow alarm */
	fmr2 = 0x20;

	if (SPANTYPE_DIGITAL_J1 == ts->span.spantype) {
		fmr4 = 0x1c;
	} else {
		/* FMR4: Lose sync on 2 out of 5 framing bits, auto resync */
		fmr4 = 0x0c;
	}

	/* LIM2: 50% peak is a "1", Advanced Loss Recovery, Multi Purpose
	 * Analog Switch enabled */
	lim2 = 0x23;
	/* LIM2: Add line buildout */
	lim2 |= (mytxlevel << 6);

	spin_lock_irqsave(&wc->reglock, flags);

	__t43x_framer_set(wc, fidx, 0x1d, fmr1);
	__t43x_framer_set(wc, fidx, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		/* WCT4XX has workaround for errata fmr0 = 0xb0 */
		/* was fmr0 = 0xa0; */
		fmr0 = 0xb0;
	} else {
		line = "B8ZS";
		fmr0 = 0xf0;
	}
	if (lineconfig & DAHDI_CONFIG_D4) {
		framing = "D4";
	} else {
		framing = "ESF";
		fmr4 |= 0x2;
		fmr2 |= 0xc0;
	}

	/* Suppress RSC interrupt for cleared channels */
	__t43x_framer_set(wc, fidx, 0x09, 0x80);
	__t43x_framer_set(wc, fidx, 0x1c, fmr0);

	__t43x_framer_set(wc, fidx, 0x20, fmr4);
	/* FMR5: Enable RBS mode */
	__t43x_framer_set(wc, fidx, 0x21, 0x40);

	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No
	 * remote loop, no DRS */
	__t43x_framer_set(wc, fidx, 0x37, 0xf0);
	/* LIM0: Enable auto long haul mode, no local loop (set after LIM1) */
	__t43x_framer_set(wc, fidx, 0x36, 0x08);

	/* CMDR: Reset the receiver and transmitter line interface */
	__t43x_framer_set(wc, fidx, 0x02, 0x50);
	/* CMDR: Reset the receiver and transmitter line interface */
	__t43x_framer_set(wc, fidx, 0x02, 0x00);
	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "card %d span %d: setting Rtx to 0ohm for T1\n",
			 wc->num, span_idx);
	}
	/* PC6: set Rtx to 0ohm for T1 */
	__t43x_framer_set(wc, fidx, 0x86, 0x00);

	/* Bugfix register for errata #3 */
	__t43x_framer_set(wc, fidx, 0xbd, 0x05);

	/* LIM2: 50% peak amplitude is a "1" */
	__t43x_framer_set(wc, fidx, 0x3a, lim2);
	/* PCD: LOS after 176 consecutive "zeros" */
	__t43x_framer_set(wc, fidx, 0x38, 0x0a);
	/* PCR: 22 "ones" clear LOS */
	__t43x_framer_set(wc, fidx, 0x39, 0x15);

	reg = __t43x_framer_get(wc, fidx, 0x24);
	if (SPANTYPE_DIGITAL_J1 == ts->span.spantype) {
		/* set J1 overide */
		__t43x_framer_set(wc, fidx, 0x24, reg | 0x80);
	} else {
		/* clear J1 overide */
		__t43x_framer_set(wc, fidx, 0x24, reg & ~0x80);
	}

	/* Generate pulse mask for T1 */
	switch (mytxlevel) {
	case 3:
		__t43x_framer_set(wc, fidx, 0x26, 0x07);	/* XPM0 */
		__t43x_framer_set(wc, fidx, 0x27, 0x01);	/* XPM1 */
		__t43x_framer_set(wc, fidx, 0x28, 0x00);	/* XPM2 */
		break;
	case 2:
		__t43x_framer_set(wc, fidx, 0x26, 0x8c);	/* XPM0 */
		__t43x_framer_set(wc, fidx, 0x27, 0x11);	/* XPM1 */
		__t43x_framer_set(wc, fidx, 0x28, 0x01);	/* XPM2 */
		break;
	case 1:
		__t43x_framer_set(wc, fidx, 0x26, 0x8c);	/* XPM0 */
		__t43x_framer_set(wc, fidx, 0x27, 0x01);	/* XPM1 */
		__t43x_framer_set(wc, fidx, 0x28, 0x00);	/* XPM2 */
		break;
	case 0:
	default:
		__t43x_framer_set(wc, fidx, 0x26, 0x1a);	/* XPM0 */
		__t43x_framer_set(wc, fidx, 0x27, 0x27);	/* XPM1 */
		__t43x_framer_set(wc, fidx, 0x28, 0x01);	/* XPM2 */
		break;
	}

	__t43x_framer_set(wc, fidx, 0x14, 0xff);	/* IMR0 */
	__t43x_framer_set(wc, fidx, 0x15, 0xff);	/* IMR1 */

	/* IMR2: All the alarms */
	__t43x_framer_set(wc, fidx, 0x16, 0x00);
	/* IMR3: ES, SEC, LLBSC, rx slips */
	__t43x_framer_set(wc, fidx, 0x17, 0x34);
	/* IMR4: Slips on transmit */
	__t43x_framer_set(wc, fidx, 0x18, 0x3f);

	spin_unlock_irqrestore(&wc->reglock, flags);
	dev_info(&wc->xb.pdev->dev, "Span %d configured for %s/%s\n",
			span_idx, framing, line);
}

static void t43x_configure_e1(struct t43x *wc, int span_idx, int lineconfig)
{
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;
	unsigned int fmr2, fmr1, fmr0;
	unsigned int cas = 0;
	unsigned int imr3extra = 0;
	char *crc4 = "";
	char *framing, *line;
	unsigned long flags;

	fmr1 = 0x44; /* FMR1: E1 mode, Automatic force resync, PCM30 mode,
			8.192 Mhz backplane, no XAIS */
	fmr2 = 0x03; /* FMR2: Auto transmit remote alarm, auto loss of
			multiframe recovery, no payload loopback */

	if (lineconfig & DAHDI_CONFIG_CRC4) {
		fmr1 |= 0x08;	/* CRC4 transmit */
		fmr2 |= 0xc0;	/* CRC4 receive */
		crc4 = "/CRC4";
	}

	spin_lock_irqsave(&wc->reglock, flags);

	__t43x_framer_set(wc, fidx, 0x1d, fmr1);
	__t43x_framer_set(wc, fidx, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		/* workaround for errata #2 in ES v3 09-10-16 */
		fmr0 = 0xb0;
	} else {
		line = "HDB3";
		fmr0 = 0xf0;
	}

	if (lineconfig & DAHDI_CONFIG_CCS) {
		framing = "CCS";
		imr3extra = 0x28;
	} else {
		framing = "CAS";
		cas = 0x40;
	}

	__t43x_framer_set(wc, fidx, 0x1c, fmr0);

	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No
	 * remote loop, no DRS */
	__t43x_framer_set(wc, fidx, 0x37, 0xf0);
	/* LIM0: Enable auto long haul mode, no local loop (must be after
	 * LIM1) */
	__t43x_framer_set(wc, fidx, 0x36, 0x08);

	/* CMDR: Reset the receiver and transmitter line interface */
	__t43x_framer_set(wc, fidx, 0x02, 0x50);
	/* CMDR: Reset the receiver and transmitter line interface */
	__t43x_framer_set(wc, fidx, 0x02, 0x00);
	if (debug)
		dev_info(&wc->xb.pdev->dev,
				"setting Rtx to 7.5ohm for E1\n");
	/* PC6: turn on 7.5ohm Rtx for E1 */
	__t43x_framer_set(wc, fidx, 0x86, 0x40);

	/* Condition receive line interface for E1 after reset */
	__t43x_framer_set(wc, fidx, 0xbb, 0x17);
	__t43x_framer_set(wc, fidx, 0xbc, 0x55);
	__t43x_framer_set(wc, fidx, 0xbb, 0x97);
	__t43x_framer_set(wc, fidx, 0xbb, 0x11);
	__t43x_framer_set(wc, fidx, 0xbc, 0xaa);
	__t43x_framer_set(wc, fidx, 0xbb, 0x91);
	__t43x_framer_set(wc, fidx, 0xbb, 0x12);
	__t43x_framer_set(wc, fidx, 0xbc, 0x55);
	__t43x_framer_set(wc, fidx, 0xbb, 0x92);
	__t43x_framer_set(wc, fidx, 0xbb, 0x0c);
	__t43x_framer_set(wc, fidx, 0xbb, 0x00);
	__t43x_framer_set(wc, fidx, 0xbb, 0x8c);

	/* LIM2: 50% peak amplitude isa "1", Multi Purpose Analog Switch
	 * enabled */
	__t43x_framer_set(wc, fidx, 0x3a, 0x22);

	/* PCD: LOS after 176 consecutive "zeros" */
	__t43x_framer_set(wc, fidx, 0x38, 0x0a);
	/* PCR: 22 "ones" clear LOS */
	__t43x_framer_set(wc, fidx, 0x39, 0x15);

	/* XSW: Spare bits all to 1 */
	__t43x_framer_set(wc, fidx, 0x20, 0x9f);
	/* XSP: E-bit set when async. AXS auto, XSIF to 1 */
	__t43x_framer_set(wc, fidx, 0x21, 0x1c|cas);

	/* Generate pulse mask for E1 */
	__t43x_framer_set(wc, fidx, 0x26, 0x74);	/* XPM0 */
	__t43x_framer_set(wc, fidx, 0x27, 0x02);	/* XPM1 */
	__t43x_framer_set(wc, fidx, 0x28, 0x00);	/* XPM2 */

	__t43x_framer_set(wc, fidx, 0x14, 0xff);	/* IMR0 */
	__t43x_framer_set(wc, fidx, 0x15, 0xff);	/* IMR1 */

	__t43x_framer_set(wc, fidx, 0x16, 0x00); /* IMR2: the alarm stuff! */
	__t43x_framer_set(wc, fidx, 0x17, 0x04 | imr3extra);	/* IMR3: AIS */
	__t43x_framer_set(wc, fidx, 0x18, 0x3f); /* IMR4: slips on transmit */

	spin_unlock_irqrestore(&wc->reglock, flags);
	dev_info(&wc->xb.pdev->dev, "Span configured for %s/%s%s\n",
			framing, line, crc4);
}

static void t43x_framer_start(struct t43x *wc)
{
	int unit;
	struct t43x_span *ts;
	unsigned long flags;
	int res;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	/* Disable fpga hardware interrupts */
	wcxb_disable_interrupts(&wc->xb);

	/* Disable DMA */
	wcxb_stop_dma(&wc->xb);
	res = wcxb_wait_for_stop(&wc->xb, 50);
	if (res)
		dev_warn(&wc->xb.pdev->dev, "DMA engine did not stop.\n");

	for (unit = 0; unit < wc->numspans; unit++) {
		ts = wc->tspans[unit];
		if (dahdi_is_e1_span(&ts->span)) {
			t43x_configure_e1(wc, unit, ts->span.lineconfig);
		} else { /* is a T1 card */
			t43x_configure_t1(wc, unit, ts->span.lineconfig,
					ts->span.txlevel);
		}
		t43x_set_cas_mode(wc, unit);

		set_bit(DAHDI_FLAGBIT_RUNNING, &ts->span.flags);
	}

	for (unit = 0; unit < wc->numspans; unit++) {
		/* Get this party started */
		local_irq_save(flags);
		t43x_check_alarms(wc, unit);
		t43x_check_sigbits(wc, unit);
		local_irq_restore(flags);
	}

	dev_info(&wc->xb.pdev->dev, "Enabling DMA controller and interrupts\n");

	/* start interrupts and DMA processing */
	wcxb_start(&wc->xb);

	t43x_check_for_interrupts(wc);

	/* force re-evaluation of timing source */
	wc->syncsrc = -1;
	spin_lock_irqsave(&wc->reglock, flags);
	__t43x_set_timing_source_auto(wc);
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* Clear all counters */
	for (unit = 0; unit < wc->numspans; unit++) {
		ts = wc->tspans[unit];
		memset(&ts->span.count, 0, sizeof(ts->span.count));
	}

	/* Invoke timer function to set leds */
	mod_timer(&wc->timer, jiffies);
}

#ifndef RPC_RCLK
/**
 * t43x_check_spanconfig - Return 0 if the span configuration is valid.
 * @wc - The card to check.
 *
 * The TE435 cannot sync timing from a span in a different line mode than the
 * first span. This function should be called after the spans are configured to
 * ensure that the are not configured in this mode.
 *
 */
static int t43x_check_spanconfig(const struct t43x *wc)
{
	unsigned int i;
	bool span_1_is_e1;

	if (&te435 != wc->devtype)
		return 0;

	span_1_is_e1 = dahdi_is_e1_span(&wc->tspans[0]->span);

	for (i = 1; i < wc->numspans; ++i) {
		struct t43x_span *ts = wc->tspans[i];

		/* We only need to check spans that we could be a sync source */
		if (!ts->syncpos)
			continue;

		if ((bool)dahdi_is_e1_span(&ts->span) == span_1_is_e1)
			continue;

		dev_warn(&wc->xb.pdev->dev,
			 "Local span %d is configured as a sync source but the line mode does not match local span 1.\n",
			 ts->span.offset + 1);
		dev_warn(&wc->xb.pdev->dev,
			 "Please configure local span 1 as a sync src and ensure all other local sync sources match the line config of span 1.\n");

		return -EINVAL;
	}

	return 0;
}
#endif

static int t43x_startup(struct file *file, struct dahdi_span *span)
{
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct t43x *wc = ts->owner;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

#ifndef RPC_RCLK
	if (t43x_check_spanconfig(wc))
		return -EINVAL;
#endif

	/* Reset framer with proper parameters and start */
	dev_info(&wc->xb.pdev->dev,
			"Calling startup (flags is %lu)\n", span->flags);
	t43x_framer_start(wc);

	return 0;
}

static inline bool is_initialized(struct t43x *wc)
{
	WARN_ON(wc->not_ready < 0);
	return (wc->not_ready == 0);
}

/**
 * t43x_wait_for_ready
 *
 * Check if the board has finished any setup and is ready to start processing
 * calls.
 */
static int t43x_wait_for_ready(struct t43x *wc)
{
	while (!is_initialized(wc)) {
		if (fatal_signal_pending(current))
			return -EIO;
		msleep_interruptible(250);
	}
	return 0;
}

static int t43x_chanconfig(struct file *file,
			    struct dahdi_chan *chan, int sigtype)
{
	struct t43x *wc = chan->pvt;

	if (file->f_flags & O_NONBLOCK && !is_initialized(wc))
		return -EAGAIN;
	else
		t43x_wait_for_ready(wc);

	if (test_bit(DAHDI_FLAGBIT_RUNNING, &chan->span->flags))
		t43x_set_cas_mode(wc, chan->span->offset);
	return 0;
}

static int t43x_rbsbits(struct dahdi_chan *chan, int bits)
{
	u_char m, c;
	int n, b;
	struct t43x *wc = chan->pvt;
	struct t43x_span *ts = container_of(chan->span, struct t43x_span, span);
	int fidx = (2 == wc->numspans) ?
				chan->span->offset+1 : chan->span->offset;
	unsigned long flags;

	if (dahdi_is_e1_span(&ts->span)) { /* do it E1 way */
		if (chan->chanpos == 16)
			return 0;

		n = chan->chanpos - 1;
		if (chan->chanpos > 15)
			n--;
		b = (n % 15);
		spin_lock_irqsave(&wc->reglock, flags);
		c = ts->txsigs[b];
		m = (n / 15) << 2; /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		ts->txsigs[b] = c;
		/* output them to the chip */
		__t43x_framer_set(wc, fidx, 0x71 + b, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	} else if (ts->span.lineconfig & DAHDI_CONFIG_D4) {
		n = chan->chanpos - 1;
		b = (n / 4);
		spin_lock_irqsave(&wc->reglock, flags);
		c = ts->txsigs[b];
		m = ((3 - (n % 4)) << 1); /* nibble selector */
		c &= ~(0x3 << m); /* keep the other nibble */
		c |= ((bits >> 2) & 0x3) << m; /* put our new nibble here */
		ts->txsigs[b] = c;
		/* output them to the chip */
		__t43x_framer_set(wc, fidx, 0x70 + b, c);
		__t43x_framer_set(wc, fidx, 0x70 + b + 6, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	} else if (ts->span.lineconfig & DAHDI_CONFIG_ESF) {
		n = chan->chanpos - 1;
		b = (n / 2);
		spin_lock_irqsave(&wc->reglock, flags);
		c = ts->txsigs[b];
		m = ((n % 2) << 2); /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		ts->txsigs[b] = c;
		/* output them to the chip */
		__t43x_framer_set(wc, fidx, 0x70 + b, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	}

	return 0;
}

static inline void t43x_dahdi_rbsbits(struct dahdi_chan *c, const int rxs)
{
	if (!(c->sig & DAHDI_SIG_CLEAR) && (c->rxsig != rxs))
		dahdi_rbsbits(c, rxs);
}

static void t43x_check_sigbits(struct t43x *wc, int span_idx)
{
	int a, i, rxs;
	struct t43x_span *ts = wc->tspans[span_idx];
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;

	if (dahdi_is_e1_span(&ts->span)) {
		for (i = 0; i < 15; i++) {
			a = t43x_framer_get(wc, fidx, 0x71 + i);
			rxs = (a & 0xf);
			t43x_dahdi_rbsbits(ts->span.chans[i+16], rxs);

			rxs = (a >> 4) & 0xf;
			t43x_dahdi_rbsbits(ts->span.chans[i], rxs);
		}
	} else if (ts->span.lineconfig & DAHDI_CONFIG_D4) {
		for (i = 0; i < 24; i += 4) {
			a = t43x_framer_get(wc, fidx, 0x70 + (i>>2));
			rxs = (a & 0x3) << 2;
			t43x_dahdi_rbsbits(ts->span.chans[i+3], rxs);

			rxs = (a & 0xc);
			t43x_dahdi_rbsbits(ts->span.chans[i+2], rxs);

			rxs = (a >> 2) & 0xc;
			t43x_dahdi_rbsbits(ts->span.chans[i+1], rxs);

			rxs = (a >> 4) & 0xc;
			t43x_dahdi_rbsbits(ts->span.chans[i], rxs);
		}
	} else {
		for (i = 0; i < 24; i += 2) {
			a = t43x_framer_get(wc, fidx, 0x70 + (i>>1));
			rxs = (a & 0xf);
			t43x_dahdi_rbsbits(ts->span.chans[i+1], rxs);

			rxs = (a >> 4) & 0xf;
			t43x_dahdi_rbsbits(ts->span.chans[i], rxs);
		}
	}
}

struct maint_work_struct {
	struct work_struct work;
	struct t43x *wc;
	int cmd;
	struct dahdi_span *span;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void t43x_maint_work(void *data)
{
	struct maint_work_struct *w = data;
#else
static void t43x_maint_work(struct work_struct *work)
{
	struct maint_work_struct *w = container_of(work,
					struct maint_work_struct, work);
#endif

	struct t43x *wc = w->wc;
	struct dahdi_span *span = w->span;
	int reg = 0;
	int cmd = w->cmd;
	unsigned long flags;
	int fidx = (wc->numspans == 2) ? span->offset+1 : span->offset;

	if (dahdi_is_e1_span(span)) {
		switch (cmd) {
		case DAHDI_MAINT_NONE:
			dev_info(&wc->xb.pdev->dev,
				 "Clearing all maint modes\n");
			t43x_clear_maint(span);
			break;
		case DAHDI_MAINT_LOCALLOOP:
			dev_info(&wc->xb.pdev->dev,
				 "Turning on local loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM0);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM0, reg | LIM0_LL);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_NETWORKLINELOOP:
			dev_info(&wc->xb.pdev->dev,
					"Turning on network line loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM1);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM1, reg | LIM1_RL);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
			dev_info(&wc->xb.pdev->dev,
				"Turning on network payload loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM1);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM1,
					  reg | (LIM1_RL | LIM1_JATT));
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_FAS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 5));
			break;
		case DAHDI_MAINT_MULTI_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 4));
			break;
		case DAHDI_MAINT_CRC_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 3));
			break;
		case DAHDI_MAINT_CAS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 2));
			break;
		case DAHDI_MAINT_PRBS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 1));
			break;
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 0));
			break;
		default:
			dev_info(&wc->xb.pdev->dev,
					"Unknown E1 maint command: %d\n", cmd);
			goto cleanup;
		}
	} else {
		switch (cmd) {
		case DAHDI_MAINT_NONE:
			dev_info(&wc->xb.pdev->dev,
				 "Clearing all maint modes\n");
			t43x_clear_maint(span);
			break;
		case DAHDI_MAINT_LOCALLOOP:
			dev_info(&wc->xb.pdev->dev,
				 "Turning on local loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM0);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM0, reg | LIM0_LL);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_NETWORKLINELOOP:
			dev_info(&wc->xb.pdev->dev,
					"Turning on network line loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM1);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM1, reg | LIM1_RL);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
			dev_info(&wc->xb.pdev->dev,
				"Turning on network payload loopback\n");
			t43x_clear_maint(span);
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t43x_framer_get(wc, fidx, LIM1);
			if (reg < 0) {
				spin_unlock_irqrestore(&wc->reglock, flags);
				goto cleanup;
			}
			__t43x_framer_set(wc, fidx, LIM1,
					  reg | (LIM1_RL | LIM1_JATT));
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		case DAHDI_MAINT_LOOPUP:
			dev_info(&wc->xb.pdev->dev,
				 "Transmitting loopup code\n");
			t43x_clear_maint(span);
			t43x_framer_set(wc, fidx, 0x21, 0x50);
			break;
		case DAHDI_MAINT_LOOPDOWN:
			dev_info(&wc->xb.pdev->dev,
				 "Transmitting loopdown code\n");
			t43x_clear_maint(span);
			t43x_framer_set(wc, fidx, 0x21, 0x60);
			break;
		case DAHDI_MAINT_FAS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 5));
			break;
		case DAHDI_MAINT_MULTI_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 4));
			break;
		case DAHDI_MAINT_CRC_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 3));
			break;
		case DAHDI_MAINT_CAS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 2));
			break;
		case DAHDI_MAINT_PRBS_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 1));
			break;
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			t43x_framer_set(wc, fidx, 0x1b, (1 << 0));
			break;
		default:
			dev_info(&wc->xb.pdev->dev,
					"Unknown T1 maint command: %d\n", cmd);
			return;
		}
	}

	/* update DAHDI_ALARM_LOOPBACK status bit and check timing source */
	spin_lock_irqsave(&wc->reglock, flags);
	if (!span->maintstat)
		span->alarms &= ~DAHDI_ALARM_LOOPBACK;
	dahdi_alarm_notify(span);
	__t43x_set_timing_source_auto(wc);
	spin_unlock_irqrestore(&wc->reglock, flags);

cleanup:
	kfree(w);
	return;
}

static int t43x_reset_counters(struct dahdi_span *span)
{
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	memset(&ts->span.count, 0, sizeof(ts->span.count));
	return 0;
}

static int t43x_maint(struct dahdi_span *span, int cmd)
{
	struct maint_work_struct *work;
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct t43x *wc = ts->owner;

	if (dahdi_is_e1_span(span)) {
		switch (cmd) {
		case DAHDI_MAINT_NONE:
		case DAHDI_MAINT_LOCALLOOP:
		case DAHDI_MAINT_NETWORKLINELOOP:
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
		case DAHDI_MAINT_FAS_DEFECT:
		case DAHDI_MAINT_MULTI_DEFECT:
		case DAHDI_MAINT_CRC_DEFECT:
		case DAHDI_MAINT_CAS_DEFECT:
		case DAHDI_MAINT_PRBS_DEFECT:
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			break;
		case DAHDI_MAINT_LOOPUP:
		case DAHDI_MAINT_LOOPDOWN:
			dev_info(&wc->xb.pdev->dev,
				"Only local loop supported in E1 mode\n");
			return -ENOSYS;
		case DAHDI_RESET_COUNTERS:
			t43x_reset_counters(span);
			return 0;
		default:
			dev_info(&wc->xb.pdev->dev,
					"Unknown E1 maint command: %d\n", cmd);
			return -ENOSYS;
		}
	} else {
		switch (cmd) {
		case DAHDI_MAINT_NONE:
		case DAHDI_MAINT_LOCALLOOP:
		case DAHDI_MAINT_NETWORKLINELOOP:
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
		case DAHDI_MAINT_LOOPUP:
		case DAHDI_MAINT_LOOPDOWN:
		case DAHDI_MAINT_FAS_DEFECT:
		case DAHDI_MAINT_MULTI_DEFECT:
		case DAHDI_MAINT_CRC_DEFECT:
		case DAHDI_MAINT_CAS_DEFECT:
		case DAHDI_MAINT_PRBS_DEFECT:
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			break;
		case DAHDI_RESET_COUNTERS:
			t43x_reset_counters(span);
			return 0;
		default:
			dev_info(&wc->xb.pdev->dev,
					"Unknown T1 maint command: %d\n", cmd);
			return -ENOSYS;
		}
	}

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		dev_info(&wc->xb.pdev->dev,
				"Failed to allocate memory for workqueue\n");
		return -ENOMEM;
	}

	work->span = span;
	work->wc = wc;
	work->cmd = cmd;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&work->work, t43x_maint_work, work);
#else
	INIT_WORK(&work->work, t43x_maint_work);
#endif
	queue_work(wc->wq, &work->work);
	return 0;
}

static int t43x_clear_maint(struct dahdi_span *span)
{
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct t43x *wc = ts->owner;
	int reg = 0;
	unsigned long flags;
	int fidx = (wc->numspans == 2) ? span->offset+1 : span->offset;

	/* Turn off local loop */
	spin_lock_irqsave(&wc->reglock, flags);
	reg = __t43x_framer_get(wc, fidx, LIM0);
	if (reg < 0) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		return -EIO;
	}
	__t43x_framer_set(wc, fidx, LIM0, reg & ~LIM0_LL);

	/* Turn off remote loop & jitter attenuator */
	reg = __t43x_framer_get(wc, fidx, LIM1);
	if (reg < 0) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		return -EIO;
	}
	__t43x_framer_set(wc, fidx, LIM1, reg & ~(LIM1_RL | LIM1_JATT));

	/* Clear loopup/loopdown signals on the line */
	__t43x_framer_set(wc, fidx, 0x21, 0x40);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return 0;
}

static int t43x_ioctl(struct dahdi_chan *chan, unsigned int cmd,
			unsigned long data)
{
	struct t4_regs regs;
	unsigned int x;
	struct t43x *wc;
	int fidx;

	switch (cmd) {
	case WCT4_GET_REGS:
		wc = chan->pvt;
		fidx = (wc->numspans == 2) ? chan->span->offset+1 :
				chan->span->offset;
		for (x = 0; x < sizeof(regs.regs) / sizeof(regs.regs[0]); x++)
			regs.regs[x] = t43x_framer_get(wc, fidx, x);

		if (copy_to_user((void __user *) data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static void t43x_chan_set_sigcap(struct dahdi_span *span, int x)
{
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct dahdi_chan *chan = ts->chans[x];

	chan->sigcap = DAHDI_SIG_CLEAR;
	/* E&M variant supported depends on span type */
	if (dahdi_is_e1_span(span)) {
		/* E1 sigcap setup */
		if (span->lineconfig & DAHDI_CONFIG_CCS) {
			/* CCS setup */
			chan->sigcap |= DAHDI_SIG_MTP2 | DAHDI_SIG_SF;
			return;
		}
		/* clear out sig and sigcap for channel 16 on E1 CAS
		 * lines, otherwise, set it correctly */
		if (x == 15) {
			/* CAS signaling channel setup */
			ts->chans[15]->sigcap = 0;
			ts->chans[15]->sig = 0;
			return;
		}
		/* normal CAS setup */
		chan->sigcap |= DAHDI_SIG_EM_E1 | DAHDI_SIG_FXSLS |
			DAHDI_SIG_FXSGS | DAHDI_SIG_FXSKS | DAHDI_SIG_SF |
			DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS | DAHDI_SIG_FXOKS |
			DAHDI_SIG_CAS | DAHDI_SIG_DACS_RBS;
	} else {
		/* T1 sigcap setup */
		chan->sigcap |= DAHDI_SIG_EM | DAHDI_SIG_FXSLS |
			DAHDI_SIG_FXSGS | DAHDI_SIG_FXSKS | DAHDI_SIG_MTP2 |
			DAHDI_SIG_SF | DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS |
			DAHDI_SIG_FXOKS | DAHDI_SIG_CAS | DAHDI_SIG_DACS_RBS;
	}
}

static int
t43x_spanconfig(struct file *file, struct dahdi_span *span,
		 struct dahdi_lineconfig *lc)
{
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct t43x *wc = ts->owner;
	int i;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	if (file->f_flags & O_NONBLOCK) {
		if (!is_initialized(wc))
			return -EAGAIN;
	} else {
		t43x_wait_for_ready(wc);
	}

	if (lc->sync < 0)
		lc->sync = 0;
	if (lc->sync > wc->numspans) {
		dev_warn(&wc->xb.pdev->dev,
			 "WARNING: Cannot set priority on span %d to %d. Please set to a number between 1 and %d\n",
			 span->spanno, lc->sync, wc->numspans);
		lc->sync = 0;
	}

	/* remove this span number from the current sync sources, if there */
	for (i = 0; i < wc->numspans; i++) {
		if (wc->tspans[i]->sync == span->spanno)
			wc->tspans[i]->sync = 0;
	}

	wc->tspans[span->offset]->syncpos = lc->sync;
	/* if a sync src, put it in proper place */
	if (lc->sync)
		wc->tspans[lc->sync - 1]->sync = span->spanno;

	/* make sure that sigcaps gets updated if necessary */
	for (i = 0; i < span->channels; i++)
		t43x_chan_set_sigcap(span, i);

	/* If already running, apply changes immediately */
	if (test_bit(DAHDI_FLAGBIT_RUNNING, &span->flags))
		return t43x_startup(file, span);

	return 0;
}

/*
 * Initialize a span
 *
 */
static int
t43x_init_one_span(struct t43x *wc, struct t43x_span *ts, enum linemode type)
{
	int x;
	struct dahdi_chan *chans[32] = {NULL,};
	struct dahdi_echocan_state *ec[32] = {NULL,};
	unsigned long flags;
	int res = 0;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	for (x = 0; x < ((E1 == type) ? 31 : 24); x++) {
		chans[x] = kzalloc(sizeof(*chans[x]), GFP_KERNEL);
		ec[x] = kzalloc(sizeof(*ec[x]), GFP_KERNEL);
		if (!chans[x] || !ec[x])
			goto error_exit;
	}

	/* Stop the interrupt handler so that we may swap the channel array. */
	disable_irq(wc->xb.pdev->irq);

	spin_lock_irqsave(&wc->reglock, flags);
	for (x = 0; x < ARRAY_SIZE(ts->chans); x++) {
		kfree(ts->chans[x]);
		kfree(ts->ec[x]);
		ts->chans[x] = NULL;
		ts->ec[x] = NULL;
	}
	memcpy(ts->chans, chans, sizeof(ts->chans));
	memcpy(ts->ec, ec, sizeof(ts->ec));

	switch (type) {
	case E1:
		ts->span.channels = 31;
		ts->span.spantype = SPANTYPE_DIGITAL_E1;
		ts->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_HDB3 |
			DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
		ts->span.deflaw = DAHDI_LAW_ALAW;
		break;
	case T1:
		ts->span.channels = 24;
		ts->span.spantype = SPANTYPE_DIGITAL_T1;
		ts->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS |
			DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
		ts->span.deflaw = DAHDI_LAW_MULAW;
		break;
	case J1:
		ts->span.channels = 24;
		ts->span.spantype = SPANTYPE_DIGITAL_J1;
		ts->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS |
			DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
		ts->span.deflaw = DAHDI_LAW_MULAW;
		break;
	default:
		spin_unlock_irqrestore(&wc->reglock, flags);
		res = -EINVAL;
		goto error_exit;
	}

	spin_unlock_irqrestore(&wc->reglock, flags);

	set_bit(DAHDI_FLAGBIT_RBS, &ts->span.flags);
	for (x = 0; x < ts->span.channels; x++) {
		sprintf(ts->chans[x]->name, "%s/%d", ts->span.name, x + 1);
		t43x_chan_set_sigcap(&ts->span, x);
		ts->chans[x]->pvt = wc;
		ts->chans[x]->chanpos = x + 1;
	}

	t43x_reset_counters(&ts->span);

	/* Span is in red alarm by default ? */
	ts->span.alarms = DAHDI_ALARM_NONE;

	enable_irq(wc->xb.pdev->irq);
	return 0;

error_exit:
	enable_irq(wc->xb.pdev->irq);

	for (x = 0; x < ARRAY_SIZE(chans); ++x) {
		kfree(chans[x]);
		kfree(ec[x]);
	}
	return res;
}

/*
 * Initialize all spans (one time)
 */

static void t43x_init_spans(struct t43x *wc, enum linemode type)
{
	struct t43x_span *ts;
	int x;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		sprintf(ts->span.name,
			"WCTE%d/%d/%d", wc->numspans, wc->num, x + 1);
		snprintf(ts->span.desc, sizeof(ts->span.desc) - 1,
			 "WCTE%d3X (PCI) Card %d Span %d",
			 wc->numspans, wc->num, x + 1);

		ts->span.chans = ts->chans;
		ts->span.flags = DAHDI_FLAG_RBS;
		ts->span.ops = &t43x_span_ops;
		ts->span.offset = x;

		ts->owner = wc;

		t43x_init_one_span(wc, ts, type);

		list_add_tail(&ts->span.device_node, &wc->ddev->spans);
	}
}

/**
 * t1xx_set_linemode - Change the type of span before assignment.
 * @span:		The span to change.
 * @linemode:		Text string for the line mode.
 *
 * This function may be called after the dahdi_device is registered but
 * before the spans are assigned numbers (and are visible to the rest of
 * DAHDI).
 *
 */
static int t43x_set_linemode(struct dahdi_span *span, enum spantypes linemode)
{
	int res;
	struct t43x_span *ts = container_of(span, struct t43x_span, span);
	struct t43x *wc = ts->owner;

	if (debug)
		dev_info(&wc->xb.pdev->dev, "%s\n", __func__);

	/* We may already be set to the requested type. */
	if (span->spantype == linemode) {
		ts->span.alarms = DAHDI_ALARM_NONE;
		return 0;
	}

	res = t43x_wait_for_ready(wc);
	if (res)
		return res;

	mutex_lock(&wc->lock);

	switch (linemode) {
	case SPANTYPE_DIGITAL_T1:
		dev_info(&wc->xb.pdev->dev,
			 "Changing from %s to T1 line mode.\n",
			 dahdi_spantype2str(span->spantype));
		res = t43x_init_one_span(wc, ts, T1);
		break;
	case SPANTYPE_DIGITAL_E1:
		dev_info(&wc->xb.pdev->dev,
			 "Changing from %s to E1 line mode.\n",
			 dahdi_spantype2str(span->spantype));
		res = t43x_init_one_span(wc, ts, E1);
		break;
	case SPANTYPE_DIGITAL_J1:
		dev_info(&wc->xb.pdev->dev,
			 "Changing from %s to J1 line mode.\n",
			 dahdi_spantype2str(span->spantype));
		res = t43x_init_one_span(wc, ts, J1);
		break;
	default:
		dev_err(&wc->xb.pdev->dev,
			"Got invalid linemode '%s' from dahdi\n",
			dahdi_spantype2str(linemode));
		res = -EINVAL;
	}

	/* Since we probably reallocated the channels we need to make
	 * sure they are configured before setting INITIALIZED again. */
	if (!res)
		dahdi_init_span(span);

	mutex_unlock(&wc->lock);
	return res;
}

static int t43x_hardware_post_init(struct t43x *wc, enum linemode *type)
{
	int reg;
	int x;

	if (!strcasecmp(default_linemode, "e1")) {
		*type = E1;
	} else if (!strcasecmp(default_linemode, "t1")) {
		*type = T1;
	} else if (!strcasecmp(default_linemode, "j1")) {
		*type = J1;
	} else {
		dev_warn(&wc->xb.pdev->dev,
			 "'%s' is an unknown linemode. Defaulting to 't1'\n",
			 default_linemode);
		*type = T1;
	}

	if (debug) {
		dev_info(&wc->xb.pdev->dev, "linemode: %s\n",
			 (*type == T1) ? "T1" : ((J1 == *type) ? "J1" : "E1"));
	}

	/* what version of the FALC are we using? */
	wcxb_gpio_set(&wc->xb, FALC_CPU_RESET);
	reg = t43x_framer_get(wc, 0, 0x4a);
	if (reg < 0) {
		dev_info(&wc->xb.pdev->dev,
				"Failed to read FALC version (%x)\n", reg);
		return -EIO;
	}
	dev_info(&wc->xb.pdev->dev, "FALC version: %1x\n", reg);

	/* make sure reads and writes work */
	for (x = 0; x < 256; x++) {
		t43x_framer_set(wc, 0, 0x14, x);
		reg = t43x_framer_get(wc, 0, 0x14);
		if (reg < 0) {
			dev_info(&wc->xb.pdev->dev,
					"Failed register read (%d)\n", reg);
			return -EIO;
		}
		if (reg != x) {
			dev_info(&wc->xb.pdev->dev,
				"Register test failed. Wrote '%x' but read '%x'\n",
				x, reg);
			return -EIO;
		}
	}

	/* Enable all the GPIO outputs. */
	t43x_setleds(wc, -1);

	return 0;
}

static void t43x_check_alarms(struct t43x *wc, int span_idx)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	unsigned char c, d;
	int alarms;
	int x, j;
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;

	if (!(test_bit(DAHDI_FLAGBIT_RUNNING, &ts->span.flags)))
		return;

	spin_lock(&wc->reglock);

	c = __t43x_framer_get(wc, fidx, 0x4c);
	d = __t43x_framer_get(wc, fidx, 0x4d);

	/* start with existing span alarms */
	alarms = ts->span.alarms;

	if (dahdi_is_e1_span(&ts->span)) {
		if (c & 0x04) {
			/* No multiframe found, force RAI high after 400ms only
			 * if we haven't found a multiframe since last loss of
			 * frame */
			if (!ts->flags.nmf) {
				/* LIM0: Force RAI High */
				__t43x_framer_set(wc, fidx, 0x20, 0x9f | 0x20);
				ts->flags.nmf = 1;
				dev_info(&wc->xb.pdev->dev,
					 "NMF workaround on!\n");
			}
			/* Reset to CRC4 mode */
			__t43x_framer_set(wc, fidx, 0x1e, 0xc3);
			/* Force Resync */
			__t43x_framer_set(wc, fidx, 0x1c, 0xf2);
			/* Force Resync */
			__t43x_framer_set(wc, fidx, 0x1c, 0xf0);
		} else if (!(c & 0x02)) {
			if (ts->flags.nmf) {
				/* LIM0: Clear forced RAI */
				__t43x_framer_set(wc, fidx, 0x20, 0x9f);
				ts->flags.nmf = 0;
				dev_info(&wc->xb.pdev->dev,
						"NMF workaround off!\n");
			}
		}
	}

	if (ts->span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
		for (x = 0, j = 0; x < ts->span.channels; x++)
			if ((ts->chans[x]->flags & DAHDI_FLAG_OPEN) ||
			    dahdi_have_netdev(ts->chans[x]))
				j++;
		if (!j)
			alarms |= DAHDI_ALARM_NOTOPEN;
		else
			alarms &= ~DAHDI_ALARM_NOTOPEN;
	}

	if (c & 0x20) { /* LOF/LFA */
		if (!(alarms & DAHDI_ALARM_RED) && (0 == ts->lofalarmtimer)) {
			ts->lofalarmtimer = (jiffies +
				msecs_to_jiffies(alarmdebounce)) ?: 1;
		}
	} else {
		ts->lofalarmtimer = 0;
	}

	if (c & 0x80) { /* LOS */
		if (!(alarms & DAHDI_ALARM_RED) && (0 == ts->losalarmtimer)) {
			ts->losalarmtimer = (jiffies +
				msecs_to_jiffies(losalarmdebounce)) ?: 1;
		}
	} else {
		ts->losalarmtimer = 0;
	}

	if (!(c & (0x80|0x20)))
		alarms &= ~DAHDI_ALARM_RED;

	if (c & 0x40) { /* AIS */
		if (!(alarms & DAHDI_ALARM_BLUE) && (0 == ts->aisalarmtimer))
			ts->aisalarmtimer = (jiffies +
				      msecs_to_jiffies(aisalarmdebounce)) ?: 1;
	} else {
		ts->aisalarmtimer = 0;
		alarms &= ~DAHDI_ALARM_BLUE;
	}

	/* Keep track of recovering */
	if (alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE|DAHDI_ALARM_NOTOPEN)) {
		ts->recoverytimer = 0;
		alarms &= ~DAHDI_ALARM_RECOVER;
	} else if (ts->span.alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE)) {
		if (0 == ts->recoverytimer) {
			ts->recoverytimer = (jiffies + 5*HZ) ?: 1;
			alarms |= DAHDI_ALARM_RECOVER;
		}
	}

	if (c & 0x10) { /* receiving yellow (RAI) */
		if (!(alarms & DAHDI_ALARM_YELLOW) && !ts->yelalarmtimer) {
			ts->yelalarmtimer = (jiffies +
				msecs_to_jiffies(yelalarmdebounce)) ?: 1;
		}
	} else {
		ts->yelalarmtimer = 0;
		alarms &= ~DAHDI_ALARM_YELLOW;
	}

	if (ts->span.alarms != alarms) {
		/* re-evaluate timing source if alarm state is changed */
		if ((alarms ^ ts->span.alarms) & NOSYNC_ALARMS) {
			ts->span.alarms = alarms;
			__t43x_set_timing_source_auto(wc);
		} else {
			ts->span.alarms = alarms;
		}

		spin_unlock(&wc->reglock);

		dahdi_alarm_notify(&ts->span);
	} else {
		spin_unlock(&wc->reglock);
	}
}

static void t43x_check_loopcodes(struct t43x *wc, int span_idx)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	unsigned char frs1;
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;

	frs1 = t43x_framer_get(wc, fidx, 0x4d);

	/* Detect loopup code if we're not sending one */
	if ((ts->span.maintstat != DAHDI_MAINT_LOOPUP) && (frs1 & 0x08)) {
		/* Loop-up code detected */
		if ((ts->span.maintstat != DAHDI_MAINT_REMOTELOOP) &&
				(0 == ts->loopuptimer))
			ts->loopuptimer = (jiffies + msecs_to_jiffies(800)) | 1;
	} else {
		ts->loopuptimer = 0;
	}

	/* Same for loopdown code */
	if ((ts->span.maintstat != DAHDI_MAINT_LOOPDOWN) && (frs1 & 0x10)) {
		/* Loop-down code detected */
		if ((ts->span.maintstat == DAHDI_MAINT_REMOTELOOP) &&
				(0 == ts->loopdntimer))
			ts->loopdntimer = (jiffies + msecs_to_jiffies(800)) | 1;
	} else {
		ts->loopdntimer = 0;
	}
}

static void t43x_debounce_alarms(struct t43x *wc, int span_idx)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	int alarms;
	unsigned long flags;
	unsigned int fmr4;
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;

	spin_lock_irqsave(&wc->reglock, flags);
	alarms = ts->span.alarms;

	if (ts->lofalarmtimer && time_after(jiffies, ts->lofalarmtimer)) {
		alarms |= DAHDI_ALARM_RED;
		ts->lofalarmtimer = 0;
		dev_info(&wc->xb.pdev->dev, "LOF alarm detected\n");
	}

	if (ts->losalarmtimer && time_after(jiffies, ts->losalarmtimer)) {
		alarms |= DAHDI_ALARM_RED;
		ts->losalarmtimer = 0;
		dev_info(&wc->xb.pdev->dev, "LOS alarm detected\n");
	}

	if (ts->aisalarmtimer && time_after(jiffies, ts->aisalarmtimer)) {
		alarms |= DAHDI_ALARM_BLUE;
		ts->aisalarmtimer = 0;
		dev_info(&wc->xb.pdev->dev, "AIS alarm detected\n");
	}

	if (ts->yelalarmtimer && time_after(jiffies, ts->yelalarmtimer)) {
		alarms |= DAHDI_ALARM_YELLOW;
		ts->yelalarmtimer = 0;
		dev_info(&wc->xb.pdev->dev, "YEL alarm detected\n");
	}

	if (ts->recoverytimer && time_after(jiffies, ts->recoverytimer)) {
		alarms &= ~(DAHDI_ALARM_RECOVER);
		ts->recoverytimer = 0;
		memset(&ts->span.count, 0, sizeof(ts->span.count));
		dev_info(&wc->xb.pdev->dev, "Alarms cleared\n");
	}

	if (alarms != ts->span.alarms) {
		/* re-evaluate timing source if alarm state is changed */
		if ((alarms ^ ts->span.alarms) & NOSYNC_ALARMS) {
			ts->span.alarms = alarms;
			__t43x_set_timing_source_auto(wc);
		} else {
			ts->span.alarms = alarms;
		}

		spin_unlock_irqrestore(&wc->reglock, flags);
		dahdi_alarm_notify(&ts->span);
		spin_lock_irqsave(&wc->reglock, flags);
	}

	/* If receiving alarms (except Yellow), go into Yellow alarm state */
	if (alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE|
				DAHDI_ALARM_NOTOPEN|DAHDI_ALARM_RECOVER)) {
		if (!ts->flags.sendingyellow) {
			dev_info(&wc->xb.pdev->dev, "Setting yellow alarm\n");
			/* We manually do yellow alarm to handle RECOVER
			 * and NOTOPEN, otherwise it's auto anyway */
			fmr4 = __t43x_framer_get(wc, fidx, 0x20);
			__t43x_framer_set(wc, fidx, 0x20, fmr4 | 0x20);
			ts->flags.sendingyellow = 1;
		}
	} else {
		if (ts->flags.sendingyellow) {
			dev_info(&wc->xb.pdev->dev, "Clearing yellow alarm\n");
			/* We manually do yellow alarm to handle RECOVER  */
			fmr4 = __t43x_framer_get(wc, fidx, 0x20);
			__t43x_framer_set(wc, fidx, 0x20, fmr4 & ~0x20);
			ts->flags.sendingyellow = 0;
		}
	}

	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void t43x_debounce_loopcodes(struct t43x *wc, int span_idx)
{
	struct t43x_span *ts = wc->tspans[span_idx];
	unsigned long flags;
	int fidx = (wc->numspans == 2) ? span_idx+1 : span_idx;

	spin_lock_irqsave(&wc->reglock, flags);
	if (ts->loopuptimer && time_after(jiffies, ts->loopuptimer)) {
		/* Loop-up code debounced */
		dev_info(&wc->xb.pdev->dev, "Loopup detected, enabling remote loop\n");
		__t43x_framer_set(wc, fidx, 0x36, 0x08);	/* LIM0: Disable
							   any local loop */
		__t43x_framer_set(wc, fidx, 0x37, 0xf6);	/* LIM1: Enable
							   remote loop */
		ts->span.maintstat = DAHDI_MAINT_REMOTELOOP;
		ts->loopuptimer = 0;
		dahdi_alarm_notify(&ts->span);
		__t43x_set_timing_source_auto(wc);
	}

	if (ts->loopdntimer && time_after(jiffies, ts->loopdntimer)) {
		/* Loop-down code debounced */
		dev_info(&wc->xb.pdev->dev, "Loopdown detected, disabling remote loop\n");
		__t43x_framer_set(wc, fidx, 0x36, 0x08);	/* LIM0: Disable
							   any local loop */
		__t43x_framer_set(wc, fidx, 0x37, 0xf0);	/* LIM1: Disable
							   remote loop */
		ts->span.maintstat = DAHDI_MAINT_NONE;
		ts->loopdntimer = 0;
		dahdi_alarm_notify(&ts->span);
		__t43x_set_timing_source_auto(wc);
	}
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void handle_leds(struct t43x *wc)
{
	u32 led = 0;
	struct t43x_span *ts;
	int span_idx;

	if (time_after(jiffies, wc->blinktimer)) {
		wc->blink = !wc->blink;
		wc->blinktimer = jiffies + HZ/2;
	}

	for (span_idx = wc->numspans-1; span_idx >= 0; span_idx--) {
		led >>= 2;
		ts = wc->tspans[span_idx];
		if ((ts->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE)) ||
				ts->losalarmtimer)  {
			/* When we're in red alarm, blink the led once a
			 * second. */
			if (wc->blink)
				led |=  STATUS_LED_RED;
		} else if (ts->span.alarms & DAHDI_ALARM_YELLOW) {
			led |= STATUS_LED_YELLOW;
		} else {
			if (test_bit(DAHDI_FLAGBIT_RUNNING, &ts->span.flags))
				led |= STATUS_LED_GREEN;
		}
	}

	if (wc->numspans == 2)
		led >>= 2;

	if (led != (ioread32be(wc->xb.membase) & LED_MASK))
		t43x_setleds(wc, led);
}

static void t43x_handle_receive(struct wcxb *xb, void *vfp)
{
	int i, j, s;
	u_char *frame = (u_char *) vfp;
	struct t43x *wc = container_of(xb, struct t43x, xb);
	struct t43x_span *ts;

	for (s = 0; s < wc->numspans; s++) {
		ts = wc->tspans[s];
		if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &ts->span.flags))
			continue;

		for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
			for (i = 0; i < ts->span.channels; i++) {
				ts->chans[i]->readchunk[j] =
					frame[j*WCXB_DMA_CHAN_SIZE+(s+1+i*4)];
			}
		}

		if (0 == vpmsupport) {
			for (i = 0; i < ts->span.channels; i++) {
				struct dahdi_chan *const c = ts->span.chans[i];
				__dahdi_ec_chunk(c, c->readchunk, c->readchunk,
						 c->writechunk);
			}
		}

		_dahdi_receive(&ts->span);
	}
}

static void t43x_handle_transmit(struct wcxb *xb, void *vfp)
{
	int i, j, s;
	u_char *frame = (u_char *) vfp;
	struct t43x *wc = container_of(xb, struct t43x, xb);
	struct t43x_span *ts;

	for (s = 0; s < wc->numspans; s++) {
		ts = wc->tspans[s];
		if (!test_bit(DAHDI_FLAGBIT_REGISTERED,
			      &ts->span.flags)) {
			continue;
		}

		_dahdi_transmit(&ts->span);

		for (j = 0; j < DAHDI_CHUNKSIZE; j++)
			for (i = 0; i < ts->span.channels; i++)
				frame[j*WCXB_DMA_CHAN_SIZE+(s+1+i*4)] =
					ts->chans[i]->writechunk[j];
	}
}

#define SPAN_DEBOUNCE \
	(ts->lofalarmtimer || ts->losalarmtimer || \
	 ts->aisalarmtimer || ts->yelalarmtimer || \
	 ts->recoverytimer || \
	 ts->loopuptimer   || ts->loopdntimer)

#define SPAN_ALARMS \
	(ts->span.alarms & ~DAHDI_ALARM_NOTOPEN)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void timer_work_func(void *param)
{
	struct t43x *wc = param;
#else
static void timer_work_func(struct work_struct *work)
{
	struct t43x *wc = container_of(work, struct t43x, timer_work);
#endif
	struct t43x_span *ts;
	int x;
	bool start_timer = false;

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		if (ts->debounce) {
			/* Clear it now in case the interrupt needs to set it
			 * again */
			ts->debounce = false;
			t43x_debounce_alarms(wc, x);
			if (!dahdi_is_e1_span(&ts->span))
				t43x_debounce_loopcodes(wc, x);
			if (SPAN_DEBOUNCE || SPAN_ALARMS)
				ts->debounce = true;
			if (ts->debounce)
				start_timer = true;
		}
	}

	handle_leds(wc);
	if (start_timer)
		mod_timer(&wc->timer, jiffies + HZ/10);
}

static void handle_falc_int(struct t43x *wc, int unit)
{
	struct t43x_span *ts = wc->tspans[unit];
	unsigned char gis, isr0, isr1, isr2, isr3, isr4;
	static int intcount;
	bool start_timer;
	bool recheck_sigbits = false;
	int fidx = (wc->numspans == 2) ? unit+1 : unit;

	intcount++;
	start_timer = false;

	spin_lock(&wc->reglock);
	gis = __t43x_framer_get(wc, fidx, FRMR_GIS);
	isr0 = (gis&FRMR_GIS_ISR0) ? __t43x_framer_get(wc, fidx, FRMR_ISR0) : 0;
	isr1 = (gis&FRMR_GIS_ISR1) ? __t43x_framer_get(wc, fidx, FRMR_ISR1) : 0;
	isr2 = (gis&FRMR_GIS_ISR2) ? __t43x_framer_get(wc, fidx, FRMR_ISR2) : 0;
	isr3 = (gis&FRMR_GIS_ISR3) ? __t43x_framer_get(wc, fidx, FRMR_ISR3) : 0;
	isr4 = (gis&FRMR_GIS_ISR4) ? __t43x_framer_get(wc, fidx, FRMR_ISR4) : 0;

	if ((debug) && !(isr3 & ISR3_SEC)) {
		dev_info(&wc->xb.pdev->dev,
			 "span: %d gis: %02x, isr0: %02x, isr1: %02x, isr2: %02x, isr3: %02x, isr4: %02x, intcount=%u\n",
			 unit, gis, isr0, isr1, isr2, isr3, isr4, intcount);
	}

	/* Collect performance counters once per second */
	if (isr3 & ISR3_SEC) {
		ts->span.count.fe += __t43x_framer_get(wc, fidx, FECL_T);
		ts->span.count.crc4 += __t43x_framer_get(wc, fidx, CEC1L_T);
		ts->span.count.cv += __t43x_framer_get(wc, fidx, CVCL_T);
		ts->span.count.ebit += __t43x_framer_get(wc, fidx, EBCL_T);
		ts->span.count.be += __t43x_framer_get(wc, fidx, BECL_T);
		ts->span.count.prbs = __t43x_framer_get(wc, fidx, FRS1_T);

		if (DAHDI_RXSIG_INITIAL == ts->span.chans[0]->rxhooksig)
			recheck_sigbits = true;
	}
	spin_unlock(&wc->reglock);

	/* Collect errored second counter once per second */
	if (isr3 & ISR3_ES)
		ts->span.count.errsec += 1;

	/* RSC/CASC: Received Signaling Information Changed */
	/* This interrupt is enabled by set_cas_mode() for CAS channels */
	if (isr0 & 0x08 || recheck_sigbits)
		t43x_check_sigbits(wc, unit);

	if (dahdi_is_e1_span(&ts->span)) {
		/* E1 checks */
		if ((isr3 & 0x68) || isr2 || (isr1 & 0x7f)) {
			t43x_check_alarms(wc, unit);
			start_timer = true;
		}
	} else {
		/* T1 checks */
		if (isr2) {
			t43x_check_alarms(wc, unit);
			start_timer = true;
		}
		if (isr3 & 0x08) {	/* T1 LLBSC */
			t43x_check_loopcodes(wc, unit);
			start_timer = true;
		}
	}

	if (!ts->span.alarms) {
		if ((isr3 & 0x3) || (isr4 & 0xc0))
			ts->span.count.timingslips++;
	}

	if (start_timer) {
		ts->debounce = true;
		if (!timer_pending(&wc->timer))
			mod_timer(&wc->timer, jiffies + HZ/10);
	}
}

static void t43x_handle_interrupt(struct wcxb *xb, u32 pending)
{
	u32 status;
	struct t43x *wc = container_of(xb, struct t43x, xb);

	if (!(pending & FALC_INT))
		return;

	/* Service at most one framer per interrupt cycle */
	status = t43x_framer_get(wc, 0, FRMR_CIS);
	if (status & (1 << (wc->intr_span + ((wc->numspans == 2) ? 1 : 0))))
		handle_falc_int(wc, wc->intr_span);
	if (++wc->intr_span >= wc->numspans)
		wc->intr_span = 0;
}

static void t43x_timer(unsigned long data)
{
	struct t43x *wc = (struct t43x *)data;

	if (!is_initialized(wc))
		return;

	queue_work(wc->wq, &wc->timer_work);
	return;
}

static const struct dahdi_span_ops t43x_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = t43x_spanconfig,
	.chanconfig = t43x_chanconfig,
	.startup = t43x_startup,
	.rbsbits = t43x_rbsbits,
	.maint = t43x_maint,
	.ioctl = t43x_ioctl,
	.assigned = t43x_span_assigned,
	.set_spantype = t43x_set_linemode,
#ifdef VPM_SUPPORT
	.echocan_create = t43x_echocan_create,
	.echocan_name = t43x_echocan_name,
#endif /* VPM_SUPPORT */
};

/**
 * t43x_check_for_interrupts - Return 0 if the card is generating interrupts.
 * @wc:	The card to check.
 *
 * If the card is not generating interrupts, this function will also place all
 * the spans on the card into red alarm.
 *
 */
static int t43x_check_for_interrupts(struct t43x *wc)
{
	unsigned int starting_framecount = wc->xb.framecount;
	unsigned long stop_time = jiffies + HZ*2;
	unsigned long flags;
	int x;

	msleep(20);
	spin_lock_irqsave(&wc->reglock, flags);
	while (starting_framecount == wc->xb.framecount) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		if (time_after(jiffies, stop_time)) {
			for (x = 0; x < wc->numspans; x++)
				wc->tspans[x]->span.alarms = DAHDI_ALARM_RED;
			dev_err(&wc->xb.pdev->dev, "Interrupts not detected.\n");
			return -EIO;
		}
		msleep(100);
		spin_lock_irqsave(&wc->reglock, flags);
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}

/**
 * t43x_read_serial - Returns the serial number of the board.
 * @wc: The board whos serial number we are reading.
 *
 * The buffer returned is dynamically allocated and must be kfree'd by the
 * caller. If memory could not be allocated, NULL is returned.
 *
 * Must be called in process context.
 *
 */
static char *t43x_read_serial(struct t43x *wc)
{
	int i;
	static const int MAX_SERIAL = 20*5;
	const unsigned int SERIAL_ADDRESS = 0x1f0000;
	unsigned char *serial = kzalloc(MAX_SERIAL + 1, GFP_KERNEL);
	struct wcxb const *xb = &wc->xb;
	struct wcxb_spi_master *flash_spi_master = NULL;
	struct wcxb_spi_device *flash_spi_device = NULL;
	const unsigned int FLASH_SPI_BASE = 0x200;

	if (!serial)
		return NULL;

	flash_spi_master = wcxb_spi_master_create(&xb->pdev->dev,
						  xb->membase + FLASH_SPI_BASE,
						  false);
	if (!flash_spi_master)
		return NULL;

	flash_spi_device = wcxb_spi_device_create(flash_spi_master, 0);
	if (!flash_spi_device)
		goto error_exit;

	wcxb_flash_read(flash_spi_device, SERIAL_ADDRESS,
			serial, MAX_SERIAL);

	for (i = 0; i < MAX_SERIAL; ++i) {
		if ((serial[i] < 0x20) || (serial[i] > 0x7e)) {
			serial[i] = '\0';
			break;
		}
	}

	if (!i) {
		kfree(serial);
		serial = NULL;
	} else {
		/* Limit the size of the buffer to just what is needed to
		 * actually hold the serial number. */
		unsigned char *new_serial;
		new_serial = kasprintf(GFP_KERNEL, "%s", serial);
		kfree(serial);
		serial = new_serial;
	}

error_exit:
	wcxb_spi_device_destroy(flash_spi_device);
	wcxb_spi_master_destroy(flash_spi_master);
	return serial;
}


/**
 * t43x_assign_num - Assign wc->num a unique value and place on card_list
 *
 */
static void t43x_assign_num(struct t43x *wc)
{
	mutex_lock(&card_list_lock);
	if (list_empty(&card_list)) {
		wc->num = 0;
		list_add(&wc->card_node, &card_list);
	} else {
		struct t43x *cur;
		struct list_head *insert_pos;
		int new_num = 0;

		insert_pos = &card_list;
		list_for_each_entry(cur, &card_list, card_node) {
			if (new_num != cur->num)
				break;
			new_num++;
			insert_pos = &cur->card_node;
		}

		wc->num = new_num;
		list_add_tail(&wc->card_node, insert_pos);
	}
	mutex_unlock(&card_list_lock);
}

/*
 * Initialize the card (one time)
 *
 */
static int __devinit t43x_init_one(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct t43x *wc;
	struct t43x_span *ts;
	unsigned int x;
	int res;
	enum linemode type;

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc) {
		pci_disable_device(pdev);
		return -ENOMEM;
	}

	t43x_assign_num(wc);

	wc->blinktimer = jiffies;
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	wc->not_ready = 1;

	spin_lock_init(&wc->reglock);

	pci_set_drvdata(pdev, wc);
	wc->xb.pdev = pdev;
	wc->xb.ops = &xb_ops;
	wc->xb.debug = &debug;

	res = wcxb_init(&wc->xb, KBUILD_MODNAME, 1);
	if (res)
		goto fail_exit;

	mutex_init(&wc->lock);
	setup_timer(&wc->timer, t43x_timer, (unsigned long)wc);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&wc->timer_work, timer_work_func, wc);
#else
	INIT_WORK(&wc->timer_work, timer_work_func);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&wc->clksrc_work.work, t43x_clksrc_work_fn,
		  &wc->clksrc_work.work);
#else
	INIT_DELAYED_WORK(&wc->clksrc_work.work, t43x_clksrc_work_fn);
#endif
	spin_lock_init(&wc->clksrc_work.lock);

	wc->ddev = dahdi_create_device();
	if (!wc->ddev) {
		res = -ENOMEM;
		goto fail_exit;
	}
	wc->ddev->manufacturer = "Digium";
	wc->ddev->location = kasprintf(GFP_KERNEL, "PCI Bus %02d Slot %02d",
				      pdev->bus->number,
				      PCI_SLOT(pdev->devfn) + 1);
	if (!wc->ddev->location) {
		res = -ENOMEM;
		goto fail_exit;
	}

	/* Check for field updatable firmware */
	res = wcxb_check_firmware(&wc->xb, TE435_VERSION,
			TE435_FW_FILENAME, force_firmware);
	if (res)
		goto fail_exit;

	wc->ddev->hardware_id = t43x_read_serial(wc);

	if (wc->ddev->hardware_id == NULL) {
		dev_info(&wc->xb.pdev->dev, "Unable to read valid serial number\n");
		res = -EIO;
		goto fail_exit;
	}

	if (strncmp(wc->ddev->hardware_id, "1TE435F", 7) == 0) {
		wc->numspans = 4;
		wc->devtype = &te435;
	} else if (strncmp(wc->ddev->hardware_id, "1TE235F", 7) == 0) {
		wc->numspans = 2;
		wc->devtype = &te235;
	} else {
		dev_info(&wc->xb.pdev->dev,
			"Unable to identify board type from serial number %s\n",
			wc->ddev->hardware_id);
		res = -EIO;
		goto fail_exit;
	}

	for (x = 0; x < wc->numspans; x++) {
		ts = kzalloc(sizeof(*wc->tspans[x]), GFP_KERNEL);
		if (!ts) {
			res = -ENOMEM;
			goto fail_exit;
		}
		wc->tspans[x] = ts;
	}

	wc->wq = create_singlethread_workqueue(KBUILD_MODNAME);
	if (!wc->wq) {
		res = -ENOMEM;
		goto fail_exit;
	}

	wcxb_set_minlatency(&wc->xb, latency);
	wcxb_set_maxlatency(&wc->xb, max_latency);

	create_sysfs_files(wc);

	res = t43x_hardware_post_init(wc, &type);
	if (res)
		goto fail_exit;

	t43x_init_spans(wc, type);

#ifdef VPM_SUPPORT
	if (!wc->vpm)
		t43x_vpm_init(wc);

	if (wc->vpm) {
		wc->ddev->devicetype = kasprintf(GFP_KERNEL,
					"%s (%s)", wc->devtype->name,
					wc->vpm_name);
	} else {
		wc->ddev->devicetype = kasprintf(GFP_KERNEL,
					"%s", wc->devtype->name);
	}
#else
	wc->ddev->devicetype = kasprintf(GFP_KERNEL,
				"%s", wc->devtype->name);
#endif

	res = dahdi_register_device(wc->ddev, &wc->xb.pdev->dev);
	if (res) {
		dev_info(&wc->xb.pdev->dev, "Unable to register with DAHDI\n");
		goto fail_exit;
	}

	if (wc->ddev->hardware_id) {
		dev_info(&wc->xb.pdev->dev, "Found a %s (SN: %s)\n",
				wc->devtype->name, wc->ddev->hardware_id);
	} else {
		dev_info(&wc->xb.pdev->dev, "Found a %s\n",
				wc->devtype->name);
	}

	wc->not_ready = 0;
	return 0;

fail_exit:
	if (&wc->xb)
		wcxb_release(&wc->xb);

	if (debug)
		dev_info(&wc->xb.pdev->dev, "***At fail_exit in init_one***\n");

	for (x = 0; x < wc->numspans; x++)
		kfree(wc->tspans[x]);

	free_wc(wc);
	return res;
}

static void __devexit t43x_remove_one(struct pci_dev *pdev)
{
	struct t43x *wc = pci_get_drvdata(pdev);
	dev_info(&wc->xb.pdev->dev, "Removing a Wildcard TE43x.\n");

	if (!wc)
		return;

	wc->not_ready = 1;
	smp_mb__after_clear_bit();

	/* Stop everything */
	wcxb_stop(&wc->xb);

	/* Leave framer in reset so it no longer transmits */
	wcxb_gpio_clear(&wc->xb, FALC_CPU_RESET);

	/* Turn off status LEDs */
	t43x_setleds(wc, 0);

#ifdef VPM_SUPPORT
	if (wc->vpm)
		release_vpm450m(wc->vpm);
	wc->vpm = NULL;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	cancel_work_sync(&wc->clksrc_work.work);
#else
	cancel_delayed_work_sync(&wc->clksrc_work.work);
#endif

	del_timer_sync(&wc->timer);
	flush_workqueue(wc->wq);
	del_timer_sync(&wc->timer);

	dahdi_unregister_device(wc->ddev);

	remove_sysfs_files(wc);

	wcxb_release(&wc->xb);
	free_wc(wc);
}

static DEFINE_PCI_DEVICE_TABLE(t43x_pci_tbl) = {
	{ 0xd161, 0x800e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0 }
};

static void t43x_shutdown(struct pci_dev *pdev)
{
	struct t43x *wc = pci_get_drvdata(pdev);
	dev_info(&wc->xb.pdev->dev, "Quiescing a Wildcard TE43x.\n");
	if (!wc)
		return;

	/* Stop everything */
	wcxb_stop(&wc->xb);

}

static int t43x_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return -ENOSYS;
}

MODULE_DEVICE_TABLE(pci, t43x_pci_tbl);

static struct pci_driver t43x_driver = {
	.name = "wcte43x",
	.probe = t43x_init_one,
	.remove = __devexit_p(t43x_remove_one),
	.shutdown = t43x_shutdown,
	.suspend = t43x_suspend,
	.id_table = t43x_pci_tbl,
};

static int __init t43x_init(void)
{
	int res;

	if (strcasecmp(default_linemode, "t1") &&
	    strcasecmp(default_linemode, "j1") &&
	    strcasecmp(default_linemode, "e1")) {
		pr_err("'%s' is an unknown span type.\n", default_linemode);
		default_linemode = "t1";
		return -EINVAL;
	}

	res = dahdi_pci_module(&t43x_driver);
	if (res)
		return -ENODEV;

	return 0;
}


static void __exit t43x_cleanup(void)
{
	pci_unregister_driver(&t43x_driver);
}

module_param(debug, int, S_IRUGO | S_IWUSR);
module_param(timingcable, int, 0600);
module_param(default_linemode, charp, S_IRUGO);
MODULE_PARM_DESC(default_linemode, "\"t1\"(default), \"e1\", or \"j1\".");
module_param(alarmdebounce, int, S_IRUGO | S_IWUSR);
module_param(losalarmdebounce, int, S_IRUGO | S_IWUSR);
module_param(aisalarmdebounce, int, S_IRUGO | S_IWUSR);
module_param(yelalarmdebounce, int, S_IRUGO | S_IWUSR);
#ifdef VPM_SUPPORT
module_param(vpmsupport, int, 0600);
#endif
module_param(force_firmware, int, S_IRUGO);
module_param(latency, int, S_IRUGO);
MODULE_PARM_DESC(latency, "How many milliseconds of audio to buffer between card and host (3ms default). This number will increase during runtime, dynamically, if dahdi detects that it is too small. This is commonly refered to as a \"latency bump\"");
module_param(max_latency, int, 0600);
MODULE_PARM_DESC(max_latency, "The maximum amount of latency that the driver will permit.");

MODULE_DESCRIPTION("Wildcard Digital Card Driver");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(t43x_init);
module_exit(t43x_cleanup);
