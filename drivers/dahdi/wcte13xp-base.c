/*
 * Digium, Inc.  Wildcard te13xp T1/E1 card Driver
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

#include <stdbool.h>
#include <dahdi/kernel.h>

#include "wct4xxp/wct4xxp.h"	/* For certain definitions */
#include "wcxb.h"
#include "wcxb_spi.h"
#include "wcxb_flash.h"

static const char *TE133_FW_FILENAME = "dahdi-fw-te133.bin";
static const char *TE134_FW_FILENAME = "dahdi-fw-te134.bin";
static const u32 TE13X_FW_VERSION = 0x6f0017;

#define VPM_SUPPORT
#define WC_MAX_IFACES 8

enum linemode {
	T1 = 1,
	E1,
	J1,
};

/* FPGA Status definitions */
#define OCT_CPU_RESET		(1 << 0)
#define OCT_CPU_DRAM_CKE	(1 << 1)
#define STATUS_LED_GREEN	(1 << 9)
#define STATUS_LED_RED		(1 << 10)
#define FALC_CPU_RESET		(1 << 11)

/* Descriptor ring definitions */
#define DRING_SIZE (1 << 7) /* Must be in multiples of 2 */
#define DMA_CHAN_SIZE 128

/* Interrupt definitions */
#define INTERRUPT_CONTROL 0x300
#define IER (INTERRUPT_CONTROL + 0x8)
#define FALC_INT (1<<3)

struct t13x {
	struct pci_dev *dev;
	spinlock_t reglock;

	u8 latency;

	const struct t13x_desc *devtype;
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
	unsigned long blinktimer;
	unsigned long loopuptimer;
	unsigned long loopdntimer;
	const char *name;
#define INITIALIZED    1
#define SHUTDOWN       2
#define READY	       3
	unsigned long bit_flags;
	u32 ledstate;
	struct dahdi_device *ddev;
	struct dahdi_span span;			/* Span */
	struct dahdi_chan *chans[32];		/* Channels */
	struct dahdi_echocan_state *ec[32];	/* Echocan state for channels */

	/* protected by t1.reglock */
	struct timer_list timer;
	struct work_struct timer_work;
	struct workqueue_struct *wq;
#ifdef VPM_SUPPORT
	struct vpm450m *vpm;
#endif
	struct mutex lock;
	struct wcxb xb;
};

static void te13x_handle_transmit(struct wcxb *xb, void *vfp);
static void te13x_handle_receive(struct wcxb *xb, void *vfp);
static void te13x_handle_interrupt(struct wcxb *xb, u32 pending);

static struct wcxb_operations xb_ops = {
	.handle_receive = te13x_handle_receive,
	.handle_transmit = te13x_handle_transmit,
	.handle_interrupt = te13x_handle_interrupt,
};

/* Maintenance Mode Registers */
#define LIM0		0x36
#define LIM0_LL		(1<<1)
#define LIM1		0x37
#define LIM1_RL	(1<<1)
#define LIM1_JATT	(1<<2)
#define FMR3_T		0x21	/* T1 Framer 3 register */
#define FMR3_E		0x31	/* E1 Framer 3 register */
#define LOOPUP		(1<<4)	/* Transmit loopup code */
#define LOOPDOWN	(1<<5)	/* Transmit loopdown code */

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
#define IERR_T 0x1B		/* Single Bit Defect Insertion Register */
#define IBV	(1 << 0) /* Bipolar violation */
#define IPE	(1 << 1) /* PRBS defect */
#define ICASE	(1 << 2) /* CAS defect */
#define ICRCE	(1 << 3) /* CRC defect */
#define IMFE	(1 << 4) /* Multiframe defect */
#define IFASE	(1 << 5) /* FAS defect */

#define IMR0		0x14
#define CCR1		0x09

/* pci memory map offsets */
#define FRAMER_BASE	0x00000800	/* framer's address space */

static int debug;
static int alarmdebounce = 2500; /* LOF/LFA def to 2.5s AT&T TR54016*/
static int losalarmdebounce = 2500; /* LOS def to 2.5s AT&T TR54016*/
static int aisalarmdebounce = 2500; /* AIS(blue) def to 2.5s AT&T TR54016*/
static int yelalarmdebounce = 500; /* RAI(yellow) def to 0.5s AT&T devguide */
static char *default_linemode = "t1"; /* 'e1', 't1', or 'j1' */
static int force_firmware;
static int latency = WCXB_DEFAULT_LATENCY;

struct t13x_firm_header {
	u8	header[6];
	__le32	chksum;
	u8	pad[18];
	__le32	version;
} __packed;


static void t13x_check_alarms(struct t13x *wc);
static void t13x_check_sigbits(struct t13x *wc);

#ifdef VPM_SUPPORT
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/firmware.h>

#include <oct612x.h>

#define ECHOCAN_NUM_CHANS 32

#define FLAGBIT_DTMF	 1
#define FLAGBIT_MUTE	 2
#define FLAGBIT_ECHO	 3
#define FLAGBIT_ALAW	 4

#define OCT_CHIP_ID			0
#define OCT_MAX_TDM_STREAMS		4
#define OCT_TONEEVENT_BUFFER_SIZE	128
#define SOUT_STREAM			1
#define RIN_STREAM			0

#define SIN_STREAM			2
#define OCT_OFFSET		(wc->xb.membase + 0x10000)
#define OCT_CONTROL_REG		(OCT_OFFSET + 0)
#define OCT_DATA_REG		(OCT_OFFSET + 0x4)
#define OCT_ADDRESS_HIGH_REG	(OCT_OFFSET + 0x8)
#define OCT_ADDRESS_LOW_REG	(OCT_OFFSET + 0xa)
#define OCT_DIRECT_WRITE_MASK	0x3001
#define OCT_INDIRECT_READ_MASK	0x0101
#define OCT_INDIRECT_WRITE_MASK	0x3101


static int vpmsupport = 1;
static const char *vpm_name = "VPMOCT032";
static void t13x_vpm_init(struct t13x *wc);
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

static void oct_reset(struct t13x *wc)
{
	wcxb_gpio_clear(&wc->xb, OCT_CPU_RESET);
	msleep_interruptible(1);
	wcxb_gpio_set(&wc->xb, OCT_CPU_RESET);

	dev_info(&wc->xb.pdev->dev, "Reset octasic\n");
}

static void oct_enable_dram(struct t13x *wc)
{
	wcxb_gpio_set(&wc->xb, OCT_CPU_DRAM_CKE);
}

static unsigned int oct_get_reg_indirect(void *data, uint32_t address)
{
	struct t13x *wc = data;
	uint16_t highaddress = ((address >> 20) & 0xfff);
	uint16_t lowaddress = ((address >> 4) & 0xfffff);
	unsigned long stop = jiffies + HZ/10;
	unsigned long flags;
	uint16_t ret;

	spin_lock_irqsave(&wc->reglock, flags);
	iowrite16be(highaddress, OCT_ADDRESS_HIGH_REG);
	iowrite16be(lowaddress, OCT_ADDRESS_LOW_REG);

	iowrite16be(OCT_INDIRECT_READ_MASK | ((address & 0xe) << 8),
				OCT_CONTROL_REG);
	do {
		ret = ioread16be(OCT_CONTROL_REG);
	} while ((ret & (1<<8)) && time_before(jiffies, stop));

	WARN_ON_ONCE(time_after_eq(jiffies, stop));

	ret = ioread16be(OCT_DATA_REG);
	spin_unlock_irqrestore(&wc->reglock, flags);

	return ret;
}

static void oct_set_reg_indirect(void *data, uint32_t address, uint16_t val)
{
	struct t13x *wc = data;
	unsigned long flags;
	uint16_t ret;
	uint16_t highaddress = ((address >> 20) & 0xfff);
	uint16_t lowaddress = ((address >> 4) & 0xffff);
	unsigned long stop = jiffies + HZ/10;

	spin_lock_irqsave(&wc->reglock, flags);
	iowrite16be(highaddress, OCT_ADDRESS_HIGH_REG);
	iowrite16be(lowaddress, OCT_ADDRESS_LOW_REG);

	iowrite16be(val, OCT_DATA_REG);
	iowrite16be(OCT_INDIRECT_WRITE_MASK | ((address & 0xe) << 8),
			OCT_CONTROL_REG);

	/* No write should take longer than 100ms */
	do {
		ret = ioread16be(OCT_CONTROL_REG);
	} while ((ret & (1<<8)) && time_before(jiffies, stop));
	spin_unlock_irqrestore(&wc->reglock, flags);

	WARN_ON_ONCE(time_after_eq(jiffies, stop));
}

static int t13x_oct612x_write(struct oct612x_context *context,
			      u32 address, u16 value)
{
	oct_set_reg_indirect(dev_get_drvdata(context->dev), address, value);
	return 0;
}

static int t13x_oct612x_read(struct oct612x_context *context, u32 address,
			     u16 *value)
{
	*value = oct_get_reg_indirect(dev_get_drvdata(context->dev), address);
	return 0;
}

static int t13x_oct612x_write_smear(struct oct612x_context *context,
				    u32 address, u16 value, size_t count)
{
	unsigned int i;
	struct t13x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		oct_set_reg_indirect(wc, address + (i << 1), value);
	return 0;
}

static int t13x_oct612x_write_burst(struct oct612x_context *context,
				    u32 address, const u16 *buffer,
				    size_t count)
{
	unsigned int i;
	struct t13x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		oct_set_reg_indirect(wc, address + (i << 1), buffer[i]);
	return 0;
}

static int t13x_oct612x_read_burst(struct oct612x_context *context,
				   u32 address, u16 *buffer, size_t count)
{
	unsigned int i;
	struct t13x *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		buffer[i] = oct_get_reg_indirect(wc, address + (i << 1));
	return 0;
}

static const struct oct612x_ops t13x_oct612x_ops = {
	.write = t13x_oct612x_write,
	.read = t13x_oct612x_read,
	.write_smear = t13x_oct612x_write_smear,
	.write_burst = t13x_oct612x_write_burst,
	.read_burst = t13x_oct612x_read_burst,
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
		pr_info("Changed companding on channel %d to %s.\n", channel,
			(alaw) ? "alaw" : "ulaw");
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
	return 1+(channel*4);
}

static int echocan_initialize_channel(
		struct vpm450m *vpm, int channel, int mode)
{
	tOCT6100_CHANNEL_OPEN	ChannelOpen;
	UINT32		law_to_use = (mode) ? cOCT6100_PCM_A_LAW :
					      cOCT6100_PCM_U_LAW;
	UINT32		tdmslot_setting;
	UINT32		ulResult;

	if (0 > channel || ECHOCAN_NUM_CHANS <= channel)
		return -1;

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

static struct vpm450m *init_vpm450m(struct t13x *wc, int isalaw,
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
		dev_info(&wc->dev->dev, "Unable to allocate vpm450m struct\n");
		return NULL;
	}

	vpm450m->context.dev = &wc->dev->dev;
	vpm450m->context.ops = &t13x_oct612x_ops;

	ChipOpen = kzalloc(sizeof(tOCT6100_CHIP_OPEN), GFP_KERNEL);
	if (!ChipOpen) {
		dev_info(&wc->dev->dev, "Unable to allocate ChipOpen\n");
		kfree(vpm450m);
		return NULL;
	}

	ChannelOpen = kzalloc(sizeof(tOCT6100_CHANNEL_OPEN), GFP_KERNEL);
	if (!ChannelOpen) {
		dev_info(&wc->dev->dev, "Unable to allocate ChannelOpen\n");
		kfree(vpm450m);
		kfree(ChipOpen);
		return NULL;
	}

	for (x = 0; x < ARRAY_SIZE(vpm450m->ecmode); x++)
		vpm450m->ecmode[x] = -1;

	dev_info(&wc->dev->dev, "Echo cancellation for %d channels\n",
		 ECHOCAN_NUM_CHANS);

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
	ChipOpen->ulMaxChannels		= ECHOCAN_NUM_CHANS;
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
	ChipOpen->fEnableChannelRecording = FALSE;

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
	oct_reset(wc);

	/* Get the size of the OCT6100 instance structure. */
	ulResult = Oct6100GetInstanceSize(ChipOpen, &InstanceSize);
	if (ulResult != cOCT6100_ERR_OK) {
		dev_info(&wc->dev->dev, "Unable to get instance size: %x\n",
				ulResult);
		return NULL;
	}

	vpm450m->pApiInstance = vmalloc(InstanceSize.ulApiInstanceSize);
	if (!vpm450m->pApiInstance) {
		dev_info(&wc->dev->dev,
			"Out of memory (can't allocate %d bytes)!\n",
			InstanceSize.ulApiInstanceSize);
		return NULL;
	}

	/* Perform the actual configuration of the chip. */
	oct_enable_dram(wc);
	ulResult = Oct6100ChipOpen(vpm450m->pApiInstance, ChipOpen);
	if (ulResult != cOCT6100_ERR_OK) {
		dev_info(&wc->dev->dev, "Unable to Oct6100ChipOpen: %x\n",
				ulResult);
		return NULL;
	}

	/* OCT6100 is now booted and channels can be opened */
	/* Open all channels */
	for (i = 0; i < ECHOCAN_NUM_CHANS; i++) {
		ulResult = echocan_initialize_channel(vpm450m, i, isalaw);
		if (0 != ulResult) {
			dev_info(&wc->dev->dev,
				"Unable to echocan_initialize_channel: %x\n",
				ulResult);
			return NULL;
		} else if (isalaw) {
			set_bit(FLAGBIT_ALAW, &vpm450m->chanflags[i]);
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

static const char *__t13x_echocan_name(struct t13x *wc)
{
	if (wc->vpm)
		return vpm_name;
	else
		return NULL;
}

static const char *t13x_echocan_name(const struct dahdi_chan *chan)
{
	struct t13x *wc = chan->pvt;
	return __t13x_echocan_name(wc);
}

static int t13x_echocan_create(struct dahdi_chan *chan,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p,
			     struct dahdi_echocan_state **ec)
{
	struct t13x *wc = chan->pvt;
	const int channel = chan->chanpos - 1;
	const struct dahdi_echocan_ops *ops;
	const struct dahdi_echocan_features *features;
	const bool alaw = (chan->span->deflaw == 2);

	if (!vpmsupport || !wc->vpm)
		return -ENODEV;

	ops = &vpm_ec_ops;
	features = &vpm_ec_features;

	if (ecp->param_count > 0) {
		dev_warn(&wc->dev->dev, "%s echo canceller does not support parameters; failing request\n",
			 chan->ec_factory->get_name(chan));
		return -EINVAL;
	}

	*ec = wc->ec[channel];
	(*ec)->ops = ops;
	(*ec)->features = *features;

	vpm450m_set_alaw_companding(wc->vpm, channel, alaw);
	vpm450m_setec(wc->vpm, channel, ecp->tap_length);
	return 0;
}

static void echocan_free(struct dahdi_chan *chan,
			struct dahdi_echocan_state *ec)
{
	struct t13x *wc = chan->pvt;
	const int channel = chan->chanpos - 1;
	if (!wc->vpm)
		return;
	memset(ec, 0, sizeof(*ec));
	vpm450m_setec(wc->vpm, channel, 0);
}

static void t13x_vpm_init(struct t13x *wc)
{
	int companding = 0;
	struct firmware embedded_firmware;
	const struct firmware *firmware = &embedded_firmware;
#if !defined(HOTPLUG_FIRMWARE)
	extern void _binary_dahdi_fw_oct6114_032_bin_size;
	extern u8 _binary_dahdi_fw_oct6114_032_bin_start[];
#else
	static const char oct032_firmware[] = "dahdi-fw-oct6114-032.bin";
#endif

	if (!vpmsupport) {
		dev_info(&wc->dev->dev, "VPM450: Support Disabled\n");
		return;
	}

#if defined(HOTPLUG_FIRMWARE)
	if ((request_firmware(&firmware, oct032_firmware, &wc->dev->dev) != 0)
			|| !firmware) {
		dev_notice(&wc->dev->dev, "VPM450: firmware %s not available from userspace\n",
				oct032_firmware);
		return;
	}
#else
	embedded_firmware.data = _binary_dahdi_fw_oct6114_032_bin_start;
	/* Yes... this is weird. objcopy gives us a symbol containing
	   the size of the firmware, not a pointer a variable containing
	   the size. The only way we can get the value of the symbol
	   is to take its address, so we define it as a pointer and
	   then cast that value to the proper type.
      */
	embedded_firmware.size = (size_t)&_binary_dahdi_fw_oct6114_032_bin_size;
#endif

	companding = dahdi_is_e1_span(&wc->span);

	wc->vpm = init_vpm450m(wc, companding, firmware);
	if (!wc->vpm) {
		dev_notice(&wc->dev->dev, "VPM450: Failed to initialize\n");
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		return;
	}

	if (firmware != &embedded_firmware)
		release_firmware(firmware);

	dev_info(&wc->dev->dev,
		"VPM450: Present and operational servicing %d span\n", 1);

}
#endif /* VPM_SUPPORT */

static int t13x_clear_maint(struct dahdi_span *span);

static struct t13x *ifaces[WC_MAX_IFACES];

struct t13x_desc {
	const char *name;
};

static const struct t13x_desc te133 = {"Wildcard TE133"}; /* pci express */
static const struct t13x_desc te134 = {"Wildcard TE134"}; /* legacy pci */

static inline bool is_pcie(const struct t13x *t1)
{
	return t1->devtype == &te133;
}

static int __t13x_pci_get(struct t13x *wc, unsigned int addr)
{
	unsigned int res = ioread8(wc->xb.membase + addr);
	return res;
}

static inline int __t13x_pci_set(struct t13x *wc, unsigned int addr, int val)
{
	iowrite8(val, wc->xb.membase + addr);
	__t13x_pci_get(wc, 0);
	return 0;
}

static inline int t13x_pci_get(struct t13x *wc, int addr)
{
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t13x_pci_get(wc, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline int t13x_pci_set(struct t13x *wc, int addr, int val)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t13x_pci_set(wc, addr, val);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline int __t13x_framer_set(struct t13x *wc, int addr, int val)
{
	return __t13x_pci_set(wc, FRAMER_BASE + addr, val);
}

static inline int t13x_framer_set(struct t13x *wc, int addr, int val)
{
	return t13x_pci_set(wc, FRAMER_BASE + addr, val);
}

static inline int __t13x_framer_get(struct t13x *wc, int addr)
{
	return __t13x_pci_get(wc, FRAMER_BASE + addr);
}

static inline int t13x_framer_get(struct t13x *wc, int addr)
{
	return t13x_pci_get(wc, FRAMER_BASE + addr);
}

static void t13x_framer_reset(struct t13x *wc)
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

static void t13x_setleds(struct t13x *wc, u32 leds)
{
	static const u32 LED_MASK = 0x600;
	wcxb_gpio_set(&wc->xb, leds & LED_MASK);
	wcxb_gpio_clear(&wc->xb, ~leds & LED_MASK);
}

static void __t13x_set_clear(struct t13x *wc)
{
	int i, offset;
	int reg;
	unsigned long flags;
	bool span_has_cas_channel = false;

	if (dahdi_is_e1_span(&wc->span)) {
		span_has_cas_channel = !(wc->span.lineconfig&DAHDI_CONFIG_CCS);
	} else {
		unsigned char ccb[3] = {0, 0, 0};
		/* Sort out channels that use CAS signalling */
		for (i = 0; i < wc->span.channels; i++) {
			offset = i/8;
			if (offset >= ARRAY_SIZE(ccb)) {
				WARN_ON(1);
				break;
			}
			if (wc->span.chans[i]->flags & DAHDI_FLAG_CLEAR)
				ccb[offset] |= 1 << (7 - (i % 8));
			else
				ccb[offset] &= ~(1 << (7 - (i % 8)));
		}

		spin_lock_irqsave(&wc->reglock, flags);
		__t13x_framer_set(wc, CCB1, ccb[0]);
		__t13x_framer_set(wc, CCB2, ccb[1]);
		__t13x_framer_set(wc, CCB3, ccb[2]);
		spin_unlock_irqrestore(&wc->reglock, flags);

		if ((~ccb[0]) | (~ccb[1]) | (~ccb[2]))
			span_has_cas_channel = true;
	}

	/* Unmask CAS RX interrupt if any single channel is in CAS mode */
	/* This interrupt is called RSC in T1 and CASC in E1 */
	spin_lock_irqsave(&wc->reglock, flags);
	reg = __t13x_framer_get(wc, IMR0);
	if (span_has_cas_channel)
		__t13x_framer_set(wc, IMR0, reg & ~0x08);
	else
		__t13x_framer_set(wc, IMR0, reg | 0x08);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

/**
 * _t13x_free_channels - Free the memory allocated for the channels.
 *
 * Must be called with wc->reglock held.
 *
 */
static void _t13x_free_channels(struct t13x *wc)
{
	int x;
	for (x = 0; x < ARRAY_SIZE(wc->chans); x++) {
		kfree(wc->chans[x]);
		kfree(wc->ec[x]);
		wc->chans[x] = NULL;
		wc->ec[x] = NULL;
	}
}

static void free_wc(struct t13x *wc)
{
	unsigned long flags;
	LIST_HEAD(list);

	spin_lock_irqsave(&wc->reglock, flags);
	_t13x_free_channels(wc);
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (wc->wq)
		destroy_workqueue(wc->wq);

	kfree(wc->ddev->location);
	kfree(wc->ddev->devicetype);
	kfree(wc->ddev->hardware_id);
	if (wc->ddev)
		dahdi_free_device(wc->ddev);
	kfree(wc->name);
	kfree(wc);
}

static void t13x_serial_setup(struct t13x *wc)
{
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	__t13x_framer_set(wc, 0x85, 0xe0);	/* GPC1: Multiplex mode
						   enabled, FSC is output,
						   active low, RCLK from
						   channel 0 */
	__t13x_framer_set(wc, 0x08, 0x05);	/* IPC: Interrupt push/pull
						   active low */

	/* Global clocks (8.192 Mhz CLK) */
	__t13x_framer_set(wc, 0x92, 0x00);
	__t13x_framer_set(wc, 0x93, 0x18);
	__t13x_framer_set(wc, 0x94, 0xfb);
	__t13x_framer_set(wc, 0x95, 0x0b);
	__t13x_framer_set(wc, 0x96, 0x00);
	__t13x_framer_set(wc, 0x97, 0x0b);
	__t13x_framer_set(wc, 0x98, 0xdb);
	__t13x_framer_set(wc, 0x99, 0xdf);

	/* Configure interrupts */
	__t13x_framer_set(wc, 0x46, 0xc0);	/* GCR: Interrupt on
						   Activation/Deactivation of
						   AIX, LOS */

	/* Configure system interface */
	__t13x_framer_set(wc, 0x3e, 0xc2);	/* SIC1: 8.192 Mhz clock/bus,
						   double buffer receive /
						   transmit, byte interleaved
						 */
	__t13x_framer_set(wc, 0x3f, 0x02);	/* SIC2: No FFS, no center
						   receive eliastic buffer,
						   phase 1 */
	__t13x_framer_set(wc, 0x40, 0x04);	/* SIC3: Edges for capture */
	__t13x_framer_set(wc, 0x44, 0x30);	/* CMR1: RCLK is at 8.192 Mhz
						   dejittered */
	__t13x_framer_set(wc, 0x45, 0x00);	/* CMR2: We provide sync and
						   clock for tx and rx. */
	__t13x_framer_set(wc, 0x22, 0x00);	/* XC0: Normal operation of
						   Sa-bits */
	__t13x_framer_set(wc, 0x23, 0x02);	/* XC1: 0 offset */
	__t13x_framer_set(wc, 0x24, 0x00);	/* RC0: Just shy of 255 */
	__t13x_framer_set(wc, 0x25, 0x03);	/* RC1: The rest of RC0 */

	/* Configure ports */
	__t13x_framer_set(wc, 0x80, 0x00);	/* PC1: SPYR/SPYX input on
						   RPA/XPA */
	__t13x_framer_set(wc, 0x81, 0x22);	/* PC2: RMFB/XSIG output/input
						   on RPB/XPB */
	__t13x_framer_set(wc, 0x82, 0x65);	/* PC3: Unused stuff */
	__t13x_framer_set(wc, 0x83, 0x35);	/* PC4: Unused stuff */
	__t13x_framer_set(wc, 0x84, 0x31);	/* PC5: XMFS active low, SCLKR
						   is input, RCLK is output */
	__t13x_framer_set(wc, 0x86, 0x03);	/* PC6: CLK1 is Tx Clock
						   output, CLK2 is 8.192 Mhz
						   from DCO-R */
	__t13x_framer_set(wc, 0x3b, 0x00);	/* Clear LCR1 */
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void t13x_configure_t1(struct t13x *wc, int lineconfig, int txlevel)
{
	unsigned int fmr4, fmr2, fmr1, fmr0, lim2;
	char *framing, *line;
	int mytxlevel;
	unsigned long flags;

	if ((txlevel > 7) || (txlevel < 4))
		mytxlevel = 0;
	else
		mytxlevel = txlevel - 4;
	fmr1 = 0x9c; /* FMR1: Mode 0, T1 mode, CRC on for ESF, 2.048 Mhz system
			data rate, no XAIS */
	fmr2 = 0x20; /* FMR2: no payload loopback, don't auto yellow alarm */


	if (SPANTYPE_DIGITAL_J1 == wc->span.spantype)
		fmr4 = 0x1c;
	else
		fmr4 = 0x0c; /* FMR4: Lose sync on 2 out of 5 framing bits,
				auto resync */

	lim2 = 0x21; /* LIM2: 50% peak is a "1", Advanced Loss recovery */
	lim2 |= (mytxlevel << 6);	/* LIM2: Add line buildout */

	spin_lock_irqsave(&wc->reglock, flags);

	__t13x_framer_set(wc, 0x1d, fmr1);
	__t13x_framer_set(wc, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		fmr0 = 0xa0;
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
	__t13x_framer_set(wc, 0x1c, fmr0);

	__t13x_framer_set(wc, 0x20, fmr4);
	__t13x_framer_set(wc, FMR3_T, 0x40);	/* FMR5: Enable RBS mode */

	__t13x_framer_set(wc, 0x37, 0xf8);	/* LIM1: Clear data in case of
						   LOS, Set receiver threshold
						   (0.5V), No remote loop, no
						   DRS */
	__t13x_framer_set(wc, 0x36, 0x08);	/* LIM0: Enable auto long haul
						   mode, no local loop (must be
						   after LIM1) */

	__t13x_framer_set(wc, 0x02, 0x50);	/* CMDR: Reset the receiver and
						   transmitter line interface
						 */
	__t13x_framer_set(wc, 0x02, 0x00);	/* CMDR: Reset the receiver and
						   transmitter line interface
						 */

	__t13x_framer_set(wc, 0x3a, lim2);	/* LIM2: 50% peak amplitude is
						   a "1" */
	__t13x_framer_set(wc, 0x38, 0x0a);	/* PCD: LOS after 176
						   consecutive "zeros" */
	__t13x_framer_set(wc, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */

	if (SPANTYPE_DIGITAL_J1 == wc->span.spantype)
		__t13x_framer_set(wc, 0x24, 0x80); /* J1 overide */

	/* Generate pulse mask for T1 */
	switch (mytxlevel) {
	case 3:
		__t13x_framer_set(wc, 0x26, 0x07);	/* XPM0 */
		__t13x_framer_set(wc, 0x27, 0x01);	/* XPM1 */
		__t13x_framer_set(wc, 0x28, 0x00);	/* XPM2 */
		break;
	case 2:
		__t13x_framer_set(wc, 0x26, 0x8c);	/* XPM0 */
		__t13x_framer_set(wc, 0x27, 0x11);	/* XPM1 */
		__t13x_framer_set(wc, 0x28, 0x01);	/* XPM2 */
		break;
	case 1:
		__t13x_framer_set(wc, 0x26, 0x8c);	/* XPM0 */
		__t13x_framer_set(wc, 0x27, 0x01);	/* XPM1 */
		__t13x_framer_set(wc, 0x28, 0x00);	/* XPM2 */
		break;
	case 0:
	default:
		__t13x_framer_set(wc, 0x26, 0x1a);	/* XPM0 */
		__t13x_framer_set(wc, 0x27, 0x1f);	/* XPM1 */
		__t13x_framer_set(wc, 0x28, 0x01);	/* XPM2 */
		break;
	}

	__t13x_framer_set(wc, 0x14, 0xff);	/* IMR0 */
	__t13x_framer_set(wc, 0x15, 0xff);	/* IMR1 */

	__t13x_framer_set(wc, 0x16, 0x00);	/* IMR2: All the alarms */
	__t13x_framer_set(wc, 0x17, 0x34);	/* IMR3:
						   ES, SEC, LLBSC, rx slips */
	__t13x_framer_set(wc, 0x18, 0x3f);	/* IMR4: Slips on transmit */

	spin_unlock_irqrestore(&wc->reglock, flags);
	dev_info(&wc->dev->dev, "Span configured for %s/%s\n", framing, line);
}

static void t13x_configure_e1(struct t13x *wc, int lineconfig)
{
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

	__t13x_framer_set(wc, 0x1d, fmr1);
	__t13x_framer_set(wc, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		fmr0 = 0xa0;
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
	__t13x_framer_set(wc, 0x1c, fmr0);

	__t13x_framer_set(wc, 0x37, 0xf0);	/* LIM1: Clear data in case of
						   LOS, Set receiver threshold
						   (0.5V), No remote loop, no
						   DRS */
	__t13x_framer_set(wc, 0x36, 0x08);	/* LIM0: Enable auto long haul
						   mode, no local loop (must be
						   after LIM1) */

	__t13x_framer_set(wc, 0x02, 0x50);	/* CMDR: Reset the receiver and
						   transmitter line interface
						 */
	__t13x_framer_set(wc, 0x02, 0x00);	/* CMDR: Reset the receiver and
						   transmitter line interface
						 */

	/* Condition receive line interface for E1 after reset */
	__t13x_framer_set(wc, 0xbb, 0x17);
	__t13x_framer_set(wc, 0xbc, 0x55);
	__t13x_framer_set(wc, 0xbb, 0x97);
	__t13x_framer_set(wc, 0xbb, 0x11);
	__t13x_framer_set(wc, 0xbc, 0xaa);
	__t13x_framer_set(wc, 0xbb, 0x91);
	__t13x_framer_set(wc, 0xbb, 0x12);
	__t13x_framer_set(wc, 0xbc, 0x55);
	__t13x_framer_set(wc, 0xbb, 0x92);
	__t13x_framer_set(wc, 0xbb, 0x0c);
	__t13x_framer_set(wc, 0xbb, 0x00);
	__t13x_framer_set(wc, 0xbb, 0x8c);

	__t13x_framer_set(wc, 0x3a, 0x20);	/* LIM2: 50% peak amplitude is
						   a "1" */
	__t13x_framer_set(wc, 0x38, 0x0a);	/* PCD: LOS after 176
						   consecutive "zeros" */
	__t13x_framer_set(wc, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */

	__t13x_framer_set(wc, 0x20, 0x9f);	/* XSW: Spare bits all to 1 */
	__t13x_framer_set(wc, 0x21, 0x1c|cas);	/* XSP: E-bit set when async.
						   AXS auto, XSIF to 1 */

	/* Generate pulse mask for E1 */
	__t13x_framer_set(wc, 0x26, 0x74);	/* XPM0 */
	__t13x_framer_set(wc, 0x27, 0x02);	/* XPM1 */
	__t13x_framer_set(wc, 0x28, 0x00);	/* XPM2 */

	__t13x_framer_set(wc, 0x14, 0xff);		/* IMR0 */
	__t13x_framer_set(wc, 0x15, 0xff);		/* IMR1 */

	__t13x_framer_set(wc, 0x16, 0x00);		/* IMR2: all the
							   alarm stuff! */
	__t13x_framer_set(wc, 0x17, 0x04 | imr3extra);	/* IMR3: AIS */
	__t13x_framer_set(wc, 0x18, 0x3f);		/* IMR4: slips on
							   transmit */

	spin_unlock_irqrestore(&wc->reglock, flags);
	dev_info(&wc->dev->dev,
			"Span configured for %s/%s%s\n", framing, line, crc4);
}

static void t13x_framer_start(struct t13x *wc)
{
	if (dahdi_is_e1_span(&wc->span)) {
		t13x_configure_e1(wc, wc->span.lineconfig);
	} else { /* is a T1 card */
		t13x_configure_t1(wc, wc->span.lineconfig, wc->span.txlevel);
	}
	__t13x_set_clear(wc);

	/* Give RCLK a short bit of time to settle */
	udelay(1);
	set_bit(DAHDI_FLAGBIT_RUNNING, &wc->span.flags);
}

static void set_span_devicetype(struct t13x *wc)
{
	const char *olddevicetype;
	olddevicetype = wc->ddev->devicetype;

#ifndef VPM_SUPPORT
	wc->ddev->devicetype = kasprintf(GFP_KERNEL,
				 "%s (VPMOCT032)", wc->devtype->name);
#else
	wc->ddev->devicetype = kasprintf(GFP_KERNEL, "%s", wc->devtype->name);
#endif /* VPM_SUPPORT */

	/* On the off chance that we were able to allocate it previously. */
	if (!wc->ddev->devicetype)
		wc->ddev->devicetype = olddevicetype;
	else
		kfree(olddevicetype);
}

/**
 * te13xp_check_for_interrupts - Return 0 if the card is generating interrupts.
 * @wc:	The card to check.
 *
 * If the card is not generating interrupts, this function will also place all
 * the spans on the card into red alarm.
 *
 */
static int te13xp_check_for_interrupts(struct t13x *wc)
{
	unsigned int starting_framecount = wc->xb.framecount;
	unsigned long stop_time = jiffies + HZ*2;
	unsigned long flags;

	msleep(20);
	spin_lock_irqsave(&wc->reglock, flags);
	while (starting_framecount == wc->xb.framecount) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		if (time_after(jiffies, stop_time)) {
			wc->span.alarms = DAHDI_ALARM_RED;
			dev_err(&wc->dev->dev, "Interrupts not detected.\n");
			return -EIO;
		}
		msleep(100);
		spin_lock_irqsave(&wc->reglock, flags);
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}

static int t13x_startup(struct file *file, struct dahdi_span *span)
{
	struct t13x *wc = container_of(span, struct t13x, span);
	unsigned long flags;
	int ret;

	set_span_devicetype(wc);

	/* Stop the DMA since the clock source may have changed. */
	wcxb_stop_dma(&wc->xb);
	ret = wcxb_wait_for_stop(&wc->xb, 50);
	if (ret) {
		dev_err(&wc->dev->dev, "Timeout waiting for DMA to stop.\n");
		return ret;
	}

	/* Reset framer with proper parameters and start */
	t13x_framer_start(wc);

	/* Do we want to SYNC on receive or not. This must always happen after
	 * the framer is fully reset. */
	wcxb_set_clksrc(&wc->xb,
			(span->syncsrc) ? WCXB_CLOCK_RECOVER : WCXB_CLOCK_SELF);

	wcxb_start(&wc->xb);
	ret = te13xp_check_for_interrupts(wc);
	if (ret)
		return ret;

	dev_info(&wc->dev->dev,
			"Calling startup (flags is %lu)\n", span->flags);

	/* Get this party started */
	local_irq_save(flags);
	t13x_check_alarms(wc);
	t13x_check_sigbits(wc);
	local_irq_restore(flags);

	return 0;
}

static int t13x_chanconfig(struct file *file,
			    struct dahdi_chan *chan, int sigtype)
{
	struct t13x *wc = chan->pvt;

	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;

	if (test_bit(DAHDI_FLAGBIT_RUNNING, &chan->span->flags) &&
	    dahdi_is_t1_span(&wc->span)) {
		__t13x_set_clear(wc);
	}
	return 0;
}

static int t13x_rbsbits(struct dahdi_chan *chan, int bits)
{
	u_char m, c;
	int n, b;
	struct t13x *wc = chan->pvt;
	unsigned long flags;

	if (dahdi_is_e1_span(&wc->span)) { /* do it E1 way */
		if (chan->chanpos == 16)
			return 0;

		n = chan->chanpos - 1;
		if (chan->chanpos > 15)
			n--;
		b = (n % 15);
		spin_lock_irqsave(&wc->reglock, flags);
		c = wc->txsigs[b];
		m = (n / 15) << 2; /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		wc->txsigs[b] = c;
		/* output them to the chip */
		__t13x_framer_set(wc, 0x71 + b, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	} else if (wc->span.lineconfig & DAHDI_CONFIG_D4) {
		n = chan->chanpos - 1;
		b = (n / 4);
		spin_lock_irqsave(&wc->reglock, flags);
		c = wc->txsigs[b];
		m = ((3 - (n % 4)) << 1); /* nibble selector */
		c &= ~(0x3 << m); /* keep the other nibble */
		c |= ((bits >> 2) & 0x3) << m; /* put our new nibble here */
		wc->txsigs[b] = c;
		/* output them to the chip */
		__t13x_framer_set(wc, 0x70 + b, c);
		__t13x_framer_set(wc, 0x70 + b + 6, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	} else if (wc->span.lineconfig & DAHDI_CONFIG_ESF) {
		n = chan->chanpos - 1;
		b = (n / 2);
		spin_lock_irqsave(&wc->reglock, flags);
		c = wc->txsigs[b];
		m = ((n % 2) << 2); /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		wc->txsigs[b] = c;
		/* output them to the chip */
		__t13x_framer_set(wc, 0x70 + b, c);
		spin_unlock_irqrestore(&wc->reglock, flags);
	}

	return 0;
}

static void t13x_check_sigbits(struct t13x *wc)
{
	int a, i, rxs;

	if (!(test_bit(DAHDI_FLAGBIT_RUNNING, &wc->span.flags)))
		return;
	if (dahdi_is_e1_span(&wc->span)) {
		for (i = 0; i < 15; i++) {
			a = t13x_framer_get(wc, 0x71 + i);
			if (a > -1) {
				/* Get high channel in low bits */
				rxs = (a & 0xf);
				if (!(wc->span.chans[i+16]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i+16]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i+16], rxs);

				rxs = (a >> 4) & 0xf;
				if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i], rxs);
			}
		}
	} else if (wc->span.lineconfig & DAHDI_CONFIG_D4) {
		for (i = 0; i < 24; i += 4) {
			a = t13x_framer_get(wc, 0x70 + (i>>2));
			if (a > -1) {
				/* Get high channel in low bits */
				rxs = (a & 0x3) << 2;
				if (!(wc->span.chans[i+3]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i+3]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i+3], rxs);

				rxs = (a & 0xc);
				if (!(wc->span.chans[i+2]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i+2]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i+2], rxs);

				rxs = (a >> 2) & 0xc;
				if (!(wc->span.chans[i+1]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i+1]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i+1], rxs);

				rxs = (a >> 4) & 0xc;
				if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i], rxs);
			}
		}
	} else {
		for (i = 0; i < 24; i += 2) {
			a = t13x_framer_get(wc, 0x70 + (i>>1));
			if (a > -1) {
				/* Get high channel in low bits */
				rxs = (a & 0xf);
				if (!(wc->span.chans[i+1]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i+1]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i+1], rxs);

				rxs = (a >> 4) & 0xf;
				if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR))
					if (wc->span.chans[i]->rxsig != rxs)
						dahdi_rbsbits(wc->span.chans[i], rxs);
			}
		}
	}
}

static void t13x_reset_counters(struct dahdi_span *span)
{
	memset(&span->count, 0, sizeof(span->count));
}

static int t13x_maint(struct dahdi_span *span, int cmd)
{
	struct t13x *wc = container_of(span, struct t13x, span);
	int reg = 0;
	unsigned long flags;

	switch (cmd) {
	case DAHDI_MAINT_NONE:
		dev_info(&wc->dev->dev, "Clearing all maint modes\n");
		t13x_clear_maint(span);
		break;
	case DAHDI_MAINT_LOCALLOOP:
		dev_info(&wc->dev->dev, "Turning on local loopback\n");
		t13x_clear_maint(span);
		spin_lock_irqsave(&wc->reglock, flags);
		reg = __t13x_framer_get(wc, LIM0);
		__t13x_framer_set(wc, LIM0, reg | LIM0_LL);
		spin_unlock_irqrestore(&wc->reglock, flags);
		break;
	case DAHDI_MAINT_NETWORKLINELOOP:
		dev_info(&wc->dev->dev,
				"Turning on network line loopback\n");
		t13x_clear_maint(span);
		spin_lock_irqsave(&wc->reglock, flags);
		reg = __t13x_framer_get(wc, LIM1);
		__t13x_framer_set(wc, LIM1, reg | LIM1_RL);
		spin_unlock_irqrestore(&wc->reglock, flags);
		break;
	case DAHDI_MAINT_NETWORKPAYLOADLOOP:
		dev_info(&wc->dev->dev,
			"Turning on network payload loopback\n");
		t13x_clear_maint(span);
		spin_lock_irqsave(&wc->reglock, flags);
		reg = __t13x_framer_get(wc, LIM1);
		__t13x_framer_set(wc, LIM1, reg | (LIM1_RL | LIM1_JATT));
		spin_unlock_irqrestore(&wc->reglock, flags);
		break;
	case DAHDI_MAINT_LOOPUP:
		dev_info(&wc->dev->dev, "Transmitting loopup code\n");
		t13x_clear_maint(span);
		if (dahdi_is_e1_span(&wc->span)) {
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t13x_framer_get(wc, FMR3_E);
			__t13x_framer_set(wc, FMR3_E, reg | LOOPUP);
			spin_unlock_irqrestore(&wc->reglock, flags);
		} else {
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t13x_framer_get(wc, FMR3_T);
			__t13x_framer_set(wc, FMR3_T, reg | LOOPUP);
			spin_unlock_irqrestore(&wc->reglock, flags);
		}
		break;
	case DAHDI_MAINT_LOOPDOWN:
		dev_info(&wc->dev->dev, "Transmitting loopdown code\n");
		t13x_clear_maint(span);
		if (dahdi_is_e1_span(&wc->span)) {
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t13x_framer_get(wc, FMR3_E);
			__t13x_framer_set(wc, FMR3_E, reg | LOOPDOWN);
			spin_unlock_irqrestore(&wc->reglock, flags);
		} else {
			spin_lock_irqsave(&wc->reglock, flags);
			reg = __t13x_framer_get(wc, FMR3_T);
			__t13x_framer_set(wc, FMR3_T, reg | LOOPDOWN);
			spin_unlock_irqrestore(&wc->reglock, flags);
		}
		break;
	case DAHDI_MAINT_FAS_DEFECT:
		t13x_framer_set(wc, IERR_T, IFASE);
		break;
	case DAHDI_MAINT_MULTI_DEFECT:
		t13x_framer_set(wc, IERR_T, IMFE);
		break;
	case DAHDI_MAINT_CRC_DEFECT:
		t13x_framer_set(wc, IERR_T, ICRCE);
		break;
	case DAHDI_MAINT_CAS_DEFECT:
		t13x_framer_set(wc, IERR_T, ICASE);
		break;
	case DAHDI_MAINT_PRBS_DEFECT:
		t13x_framer_set(wc, IERR_T, IPE);
		break;
	case DAHDI_MAINT_BIPOLAR_DEFECT:
		t13x_framer_set(wc, IERR_T, IBV);
		break;
	case DAHDI_RESET_COUNTERS:
		t13x_reset_counters(span);
		break;
	default:
		dev_info(&wc->dev->dev,
				"Unknown maint command: %d\n", cmd);
		return -ENOSYS;
	}

	/* update DAHDI_ALARM_LOOPBACK status bit and check timing source */
	spin_lock_irqsave(&wc->reglock, flags);
	if (!span->maintstat)
		span->alarms &= ~DAHDI_ALARM_LOOPBACK;
	dahdi_alarm_notify(span);
	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}

static int t13x_clear_maint(struct dahdi_span *span)
{
	struct t13x *wc = container_of(span, struct t13x, span);
	int reg = 0;
	unsigned long flags;

	/* Turn off local loop */
	spin_lock_irqsave(&wc->reglock, flags);
	reg = __t13x_framer_get(wc, LIM0);
	__t13x_framer_set(wc, LIM0, reg & ~LIM0_LL);

	/* Turn off remote loop & jitter attenuator */
	reg = __t13x_framer_get(wc, LIM1);
	__t13x_framer_set(wc, LIM1, reg & ~(LIM1_RL | LIM1_JATT));

	/* Clear loopup/loopdown signals on the line */
	if (dahdi_is_e1_span(&wc->span)) {
		reg = __t13x_framer_get(wc, FMR3_E);
		__t13x_framer_set(wc, FMR3_E, reg & ~(LOOPDOWN | LOOPUP));
	} else {
		reg = __t13x_framer_get(wc, FMR3_T);
		__t13x_framer_set(wc, FMR3_T, reg & ~(LOOPDOWN | LOOPUP));
	}

	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}


static int t13x_ioctl(struct dahdi_chan *chan, unsigned int cmd,
			unsigned long data)
{
	struct t4_regs regs;
	unsigned int x;
	struct t13x *wc;

	switch (cmd) {
	case WCT4_GET_REGS:
		wc = chan->pvt;
		for (x = 0; x < sizeof(regs.regs) / sizeof(regs.regs[0]); x++)
			regs.regs[x] = t13x_framer_get(wc, x);

		if (copy_to_user((void __user *) data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static void t13x_chan_set_sigcap(struct dahdi_span *span, int x)
{
	struct t13x *wc = container_of(span, struct t13x, span);
	struct dahdi_chan *chan = wc->chans[x];
	chan->sigcap = DAHDI_SIG_CLEAR;
	/* E&M variant supported depends on span type */
	if (dahdi_is_e1_span(&wc->span)) {
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
			wc->chans[15]->sigcap = 0;
			wc->chans[15]->sig = 0;
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
t13x_spanconfig(struct file *file, struct dahdi_span *span,
		 struct dahdi_lineconfig *lc)
{
	struct t13x *wc = container_of(span, struct t13x, span);
	int i;

	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;

	/* Set recover timing mode */
	span->syncsrc = lc->sync;

	/* make sure that sigcaps gets updated if necessary */
	for (i = 0; i < wc->span.channels; i++)
		t13x_chan_set_sigcap(span, i);

	/* If already running, apply changes immediately */
	if (test_bit(DAHDI_FLAGBIT_RUNNING, &span->flags))
		return t13x_startup(file, span);

	return 0;
}

/**
 * t13x_software_init - Initialize the board for the given type.
 * @wc:		The board to initialize.
 * @type:	The type of board we are, T1 / E1
 *
 * This function is called at startup and when the type of the span is changed
 * via the dahdi_device before the span is assigned a number.
 *
 */
static int t13x_software_init(struct t13x *wc, enum linemode type)
{
	int x;
	struct dahdi_chan *chans[32] = {NULL,};
	struct dahdi_echocan_state *ec[32] = {NULL,};
	unsigned long flags;
	int res = 0;

	/* We may already be setup properly. */
	if (wc->span.channels == ((E1 == type) ? 31 : 24))
		return 0;

	for (x = 0; x < ((E1 == type) ? 31 : 24); x++) {
		chans[x] = kzalloc(sizeof(*chans[x]), GFP_KERNEL);
		ec[x] = kzalloc(sizeof(*ec[x]), GFP_KERNEL);
		if (!chans[x] || !ec[x])
			goto error_exit;
	}

	set_span_devicetype(wc);

	/* Because the interrupt handler is running, we need to atomically
	 * swap the channel arrays. */
	spin_lock_irqsave(&wc->reglock, flags);
	_t13x_free_channels(wc);
	memcpy(wc->chans, chans, sizeof(wc->chans));
	memcpy(wc->ec, ec, sizeof(wc->ec));
	memset(chans, 0, sizeof(chans));
	memset(ec, 0, sizeof(ec));

	switch (type) {
	case E1:
		wc->span.channels = 31;
		wc->span.spantype = SPANTYPE_DIGITAL_E1;
		wc->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_HDB3 |
			DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
		wc->span.deflaw = DAHDI_LAW_ALAW;
		break;
	case T1:
		wc->span.channels = 24;
		wc->span.spantype = SPANTYPE_DIGITAL_T1;
		wc->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS |
			DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
		wc->span.deflaw = DAHDI_LAW_MULAW;
		break;
	case J1:
		wc->span.channels = 24;
		wc->span.spantype = SPANTYPE_DIGITAL_J1;
		wc->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS |
			DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
		wc->span.deflaw = DAHDI_LAW_MULAW;
		break;
	default:
		spin_unlock_irqrestore(&wc->reglock, flags);
		res = -EINVAL;
		goto error_exit;
	}

	spin_unlock_irqrestore(&wc->reglock, flags);

	dev_info(&wc->dev->dev, "Setting up global serial parameters for %s\n",
		dahdi_spantype2str(wc->span.spantype));

	t13x_serial_setup(wc);
	set_bit(DAHDI_FLAGBIT_RBS, &wc->span.flags);
	for (x = 0; x < wc->span.channels; x++) {
		sprintf(wc->chans[x]->name, "%s/%d", wc->span.name, x + 1);
		t13x_chan_set_sigcap(&wc->span, x);
		wc->chans[x]->pvt = wc;
		wc->chans[x]->chanpos = x + 1;
	}

	set_span_devicetype(wc);

	return 0;

error_exit:
	for (x = 0; x < ARRAY_SIZE(chans); ++x) {
		kfree(chans[x]);
		kfree(ec[x]);
	}
	return res;
}

/**
 * t13x_set_linemode - Change the type of span before assignment.
 * @span:		The span to change.
 * @linemode:		Text string for the line mode.
 *
 * This function may be called after the dahdi_device is registered but
 * before the spans are assigned numbers (and are visible to the rest of
 * DAHDI).
 *
 */
static int t13x_set_linemode(struct dahdi_span *span, enum spantypes linemode)
{
	int res;
	struct t13x *wc = container_of(span, struct t13x, span);

	/* We may already be set to the requested type. */
	if (span->spantype == linemode) {
		span->alarms = DAHDI_ALARM_NONE;
		return 0;
	}

	mutex_lock(&wc->lock);

	/* Stop the processing of the channels since we're going to change
	 * them. */
	clear_bit(INITIALIZED, &wc->bit_flags);
	disable_irq(wc->xb.pdev->irq);

	synchronize_irq(wc->dev->irq);
	smp_mb__after_clear_bit();
	del_timer_sync(&wc->timer);
	flush_workqueue(wc->wq);

	t13x_framer_reset(wc);
	t13x_serial_setup(wc);

	switch (linemode) {
	case SPANTYPE_DIGITAL_T1:
		dev_info(&wc->dev->dev,
			 "Changing from %s to T1 line mode.\n",
			 dahdi_spantype2str(wc->span.spantype));
		res = t13x_software_init(wc, T1);
		break;
	case SPANTYPE_DIGITAL_E1:
		dev_info(&wc->dev->dev,
			 "Changing from %s to E1 line mode.\n",
			 dahdi_spantype2str(wc->span.spantype));
		res = t13x_software_init(wc, E1);
		break;
	case SPANTYPE_DIGITAL_J1:
		dev_info(&wc->dev->dev,
			 "Changing from %s to E1 line mode.\n",
			 dahdi_spantype2str(wc->span.spantype));
		res = t13x_software_init(wc, J1);
	default:
		dev_err(&wc->dev->dev,
			"Got invalid linemode '%s' from dahdi\n",
			dahdi_spantype2str(linemode));
		res = -EINVAL;
	}

	/* Since we probably reallocated the channels we need to make
	 * sure they are configured before setting INITIALIZED again. */
	if (!res) {
		dahdi_init_span(span);
		set_bit(INITIALIZED, &wc->bit_flags);
		mod_timer(&wc->timer, jiffies + HZ/5);
	}
	wcxb_lock_latency(&wc->xb);
	enable_irq(wc->xb.pdev->irq);
	mutex_unlock(&wc->lock);
	msleep(10);
	wcxb_unlock_latency(&wc->xb);
	return res;
}

static int t13x_hardware_post_init(struct t13x *wc, enum linemode *type)
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
		dev_warn(&wc->dev->dev,
			 "'%s' is an unknown linemode. Defaulting to 't1'\n",
			 default_linemode);
		*type = T1;
	}

	if (debug) {
		dev_info(&wc->dev->dev, "linemode: %s\n",
			 (*type == T1) ? "T1" : ((J1 == *type) ? "J1" : "E1"));
	}

	/* what version of the FALC are we using? */
	wcxb_gpio_set(&wc->xb, FALC_CPU_RESET);
	reg = t13x_framer_get(wc, 0x4a);
	if (reg < 0) {
		dev_info(&wc->dev->dev,
				"Failed to read FALC version (%x)\n", reg);
		return -EIO;
	}
	dev_info(&wc->dev->dev, "FALC version: %1x\n", reg);

	/* make sure reads and writes work */
	for (x = 0; x < 256; x++) {
		t13x_framer_set(wc, 0x14, x);
		reg = t13x_framer_get(wc, 0x14);
		if (reg < 0) {
			dev_info(&wc->dev->dev,
					"Failed register read (%d)\n", reg);
			return -EIO;
		}
		if (reg != x) {
			dev_info(&wc->dev->dev,
				"Register test failed. Wrote '%x' but read '%x'\n",
				x, reg);
			return -EIO;
		}
	}

	/* Enable all the GPIO outputs. */
	t13x_setleds(wc, wc->ledstate);

	return 0;
}

static void t13x_check_alarms(struct t13x *wc)
{
	unsigned char c, d;
	int alarms;
	int x, j;

	if (!(test_bit(DAHDI_FLAGBIT_RUNNING, &wc->span.flags)))
		return;

	spin_lock(&wc->reglock);

	c = __t13x_framer_get(wc, 0x4c);
	d = __t13x_framer_get(wc, 0x4d);

	/* start with existing span alarms */
	alarms = wc->span.alarms;

	if (dahdi_is_e1_span(&wc->span)) {
		if (c & 0x04) {
			/* No multiframe found, force RAI high after 400ms only
			 * if we haven't found a multiframe since last loss of
			 * frame */
			if (!wc->flags.nmf) {
				/* LIM0: Force RAI High */
				__t13x_framer_set(wc, 0x20, 0x9f | 0x20);
				wc->flags.nmf = 1;
				dev_info(&wc->dev->dev, "NMF workaround on!\n");
			}
			__t13x_framer_set(wc, 0x1e, 0xc3);	/* Reset to CRC4 mode */
			__t13x_framer_set(wc, 0x1c, 0xf2);	/* Force Resync */
			__t13x_framer_set(wc, 0x1c, 0xf0);	/* Force Resync */
		} else if (!(c & 0x02)) {
			if (wc->flags.nmf) {
				/* LIM0: Clear forced RAI */
				__t13x_framer_set(wc, 0x20, 0x9f);
				wc->flags.nmf = 0;
				dev_info(&wc->dev->dev,
						"NMF workaround off!\n");
			}
		}
	}

	if (wc->span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
		for (x = 0, j = 0; x < wc->span.channels; x++)
			if ((wc->span.chans[x]->flags & DAHDI_FLAG_OPEN) ||
			    dahdi_have_netdev(wc->span.chans[x]))
				j++;
		if (!j)
			alarms |= DAHDI_ALARM_NOTOPEN;
		else
			alarms &= ~DAHDI_ALARM_NOTOPEN;
	}

	if (c & 0x20) { /* LOF/LFA */
		if (!(alarms & DAHDI_ALARM_RED) && (0 == wc->lofalarmtimer))
			wc->lofalarmtimer = (jiffies + msecs_to_jiffies(alarmdebounce)) ?: 1;
	} else {
		wc->lofalarmtimer = 0;
	}

	if (c & 0x80) { /* LOS */
		if (!(alarms & DAHDI_ALARM_RED) && (0 == wc->losalarmtimer))
			wc->losalarmtimer = (jiffies + msecs_to_jiffies(losalarmdebounce)) ?: 1;
	} else {
		wc->losalarmtimer = 0;
	}

	if (!(c & (0x80|0x20)))
		alarms &= ~DAHDI_ALARM_RED;

	if (c & 0x40) { /* AIS */
		if (!(alarms & DAHDI_ALARM_BLUE) && (0 == wc->aisalarmtimer))
			wc->aisalarmtimer = (jiffies + msecs_to_jiffies(aisalarmdebounce)) ?: 1;
	} else {
		wc->aisalarmtimer = 0;
		alarms &= ~DAHDI_ALARM_BLUE;
	}

	/* Keep track of recovering */
	if (alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE|DAHDI_ALARM_NOTOPEN)) {
		wc->recoverytimer = 0;
		alarms &= ~DAHDI_ALARM_RECOVER;
	} else if (wc->span.alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE)) {
		if (0 == wc->recoverytimer) {
			wc->recoverytimer = (jiffies + 5*HZ) ?: 1;
			alarms |= DAHDI_ALARM_RECOVER;
		}
	}

	if (c & 0x10) { /* receiving yellow (RAI) */
		if (!(alarms & DAHDI_ALARM_YELLOW) && (0 == wc->yelalarmtimer))
			wc->yelalarmtimer = (jiffies + msecs_to_jiffies(yelalarmdebounce)) ?: 1;
	} else {
		wc->yelalarmtimer = 0;
		alarms &= ~DAHDI_ALARM_YELLOW;
	}

	if (wc->span.alarms != alarms) {
		wc->span.alarms = alarms;
		spin_unlock(&wc->reglock);
		dahdi_alarm_notify(&wc->span);
	} else {
		spin_unlock(&wc->reglock);
	}
}

static void t13x_check_loopcodes(struct t13x *wc)
{
	unsigned char frs1;

	frs1 = t13x_framer_get(wc, 0x4d);

	/* Detect loopup code if we're not sending one */
	if ((wc->span.maintstat != DAHDI_MAINT_LOOPUP) && (frs1 & 0x08)) {
		/* Loop-up code detected */
		if ((wc->span.maintstat != DAHDI_MAINT_REMOTELOOP) &&
				(0 == wc->loopuptimer))
			wc->loopuptimer = (jiffies + msecs_to_jiffies(800)) ?: 1;
	} else {
		wc->loopuptimer = 0;
	}

	/* Same for loopdown code */
	if ((wc->span.maintstat != DAHDI_MAINT_LOOPDOWN) && (frs1 & 0x10)) {
		/* Loop-down code detected */
		if ((wc->span.maintstat == DAHDI_MAINT_REMOTELOOP) &&
				(0 == wc->loopdntimer))
			wc->loopdntimer = (jiffies + msecs_to_jiffies(800)) ?: 1;
	} else {
		wc->loopdntimer = 0;
	}
}

static void t13x_debounce_alarms(struct t13x *wc)
{
	int alarms;
	unsigned long flags;
	unsigned int fmr4;

	spin_lock_irqsave(&wc->reglock, flags);
	alarms = wc->span.alarms;

	if (wc->lofalarmtimer && time_after(jiffies, wc->lofalarmtimer)) {
		alarms |= DAHDI_ALARM_RED;
		wc->lofalarmtimer = 0;
		dev_info(&wc->dev->dev, "LOF alarm detected\n");
	}

	if (wc->losalarmtimer && time_after(jiffies, wc->losalarmtimer)) {
		alarms |= DAHDI_ALARM_RED;
		wc->losalarmtimer = 0;
		dev_info(&wc->dev->dev, "LOS alarm detected\n");
	}

	if (wc->aisalarmtimer && time_after(jiffies, wc->aisalarmtimer)) {
		alarms |= DAHDI_ALARM_BLUE;
		wc->aisalarmtimer = 0;
		dev_info(&wc->dev->dev, "AIS alarm detected\n");
	}

	if (wc->yelalarmtimer && time_after(jiffies, wc->yelalarmtimer)) {
		alarms |= DAHDI_ALARM_YELLOW;
		wc->yelalarmtimer = 0;
		dev_info(&wc->dev->dev, "YEL alarm detected\n");
	}

	if (wc->recoverytimer && time_after(jiffies, wc->recoverytimer)) {
		alarms &= ~(DAHDI_ALARM_RECOVER);
		wc->recoverytimer = 0;
		dev_info(&wc->dev->dev, "Alarms cleared\n");
	}

	if (alarms != wc->span.alarms) {
		wc->span.alarms = alarms;
		spin_unlock_irqrestore(&wc->reglock, flags);
		dahdi_alarm_notify(&wc->span);
		spin_lock_irqsave(&wc->reglock, flags);
	}

	/* If receiving alarms (except Yellow), go into Yellow alarm state */
	if (alarms & (DAHDI_ALARM_RED|DAHDI_ALARM_BLUE|
				DAHDI_ALARM_NOTOPEN|DAHDI_ALARM_RECOVER)) {
		if (!wc->flags.sendingyellow) {
			dev_info(&wc->dev->dev, "Setting yellow alarm\n");
			/* We manually do yellow alarm to handle RECOVER
			 * and NOTOPEN, otherwise it's auto anyway */
			fmr4 = __t13x_framer_get(wc, 0x20);
			__t13x_framer_set(wc, 0x20, fmr4 | 0x20);
			wc->flags.sendingyellow = 1;
		}
	} else {
		if (wc->flags.sendingyellow) {
			dev_info(&wc->dev->dev, "Clearing yellow alarm\n");
			/* We manually do yellow alarm to handle RECOVER  */
			fmr4 = __t13x_framer_get(wc, 0x20);
			__t13x_framer_set(wc, 0x20, fmr4 & ~0x20);
			wc->flags.sendingyellow = 0;
		}
	}

	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void t13x_debounce_loopcodes(struct t13x *wc)
{
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	if (wc->loopuptimer && time_after(jiffies, wc->loopuptimer)) {
		/* Loop-up code debounced */
		dev_info(&wc->dev->dev, "Loopup detected, enabling remote loop\n");
		__t13x_framer_set(wc, 0x36, 0x08);	/* LIM0: Disable
							   any local loop */
		__t13x_framer_set(wc, 0x37, 0xf6);	/* LIM1: Enable
							   remote loop */
		wc->span.maintstat = DAHDI_MAINT_REMOTELOOP;
		wc->loopuptimer = 0;
		spin_unlock_irqrestore(&wc->reglock, flags);
		dahdi_alarm_notify(&wc->span);
		spin_lock_irqsave(&wc->reglock, flags);
	}

	if (wc->loopdntimer && time_after(jiffies, wc->loopdntimer)) {
		/* Loop-down code debounced */
		dev_info(&wc->dev->dev, "Loopdown detected, disabling remote loop\n");
		__t13x_framer_set(wc, 0x36, 0x08);	/* LIM0: Disable
							   any local loop */
		__t13x_framer_set(wc, 0x37, 0xf0);	/* LIM1: Disable
							   remote loop */
		wc->span.maintstat = DAHDI_MAINT_NONE;
		wc->loopdntimer = 0;
		spin_unlock_irqrestore(&wc->reglock, flags);
		dahdi_alarm_notify(&wc->span);
		spin_lock_irqsave(&wc->reglock, flags);
	}
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void handle_leds(struct t13x *wc)
{
	u32 led;
	unsigned long flags;
#define SET_LED_RED(a) ((a | STATUS_LED_RED) & ~STATUS_LED_GREEN)
#define SET_LED_GREEN(a) ((a | STATUS_LED_GREEN) & ~STATUS_LED_RED)
#define UNSET_LED_REDGREEN(a) (a & ~(STATUS_LED_RED | STATUS_LED_GREEN))
#define SET_LED_YELLOW(a) (a | STATUS_LED_RED | STATUS_LED_GREEN)

	led = wc->ledstate;

	if ((wc->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE)) ||
			wc->losalarmtimer)  {
		/* When we're in red alarm, blink the led once a second. */
		if (time_after(jiffies, wc->blinktimer)) {
			led = (led & STATUS_LED_RED) ? UNSET_LED_REDGREEN(led) :
						  SET_LED_RED(led);
		}
	} else if (wc->span.alarms & DAHDI_ALARM_YELLOW) {
		led = SET_LED_YELLOW(led);
	} else {
		if (test_bit(DAHDI_FLAGBIT_RUNNING, &wc->span.flags))
			led = SET_LED_GREEN(led);
		else
			led = UNSET_LED_REDGREEN(led);
	}

	if (led != wc->ledstate) {
		wc->blinktimer = jiffies + HZ/2;
		/* TODO: set ledstate in t13x_setleds() */
		spin_lock_irqsave(&wc->reglock, flags);
		wc->ledstate = led;
		spin_unlock_irqrestore(&wc->reglock, flags);
		t13x_setleds(wc, led);
	}
}

static void te13x_handle_receive(struct wcxb *xb, void *vfp)
{
	int i, j;
	u_char *frame = (u_char *) vfp;
	struct t13x *wc = container_of(xb, struct t13x, xb);

	for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
		for (i = 0; i < wc->span.channels; i++) {
			wc->chans[i]->readchunk[j] =
					frame[j*DMA_CHAN_SIZE+(1+i*4)];
		}
	}

	if (0 == vpmsupport) {
		for (i = 0; i < wc->span.channels; i++) {
			struct dahdi_chan *const c = wc->span.chans[i];
			__dahdi_ec_chunk(c, c->readchunk, c->readchunk,
					 c->writechunk);
		}
	}

	_dahdi_receive(&wc->span);
}

static void te13x_handle_transmit(struct wcxb *xb, void *vfp)
{
	int i, j;
	u_char *frame = (u_char *) vfp;
	struct t13x *wc = container_of(xb, struct t13x, xb);

	_dahdi_transmit(&wc->span);

	for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
		for (i = 0; i < wc->span.channels; i++) {
			frame[j*DMA_CHAN_SIZE+(1+i*4)] =
				wc->chans[i]->writechunk[j];
		}
	}
}

#define SPAN_DEBOUNCE \
	(wc->lofalarmtimer || wc->losalarmtimer || \
	 wc->aisalarmtimer || wc->yelalarmtimer || \
	 wc->recoverytimer || \
	 wc->loopuptimer   || wc->loopdntimer)

#define SPAN_ALARMS \
	(wc->span.alarms & ~DAHDI_ALARM_NOTOPEN)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void timer_work_func(void *param)
{
	struct t13x *wc = param;
#else
static void timer_work_func(struct work_struct *work)
{
	struct t13x *wc = container_of(work, struct t13x, timer_work);
#endif
	static int work_count;

	if (debug)
		dev_notice(&wc->dev->dev, "Timer work: %d!\n", ++work_count);

	t13x_debounce_alarms(wc);
	if (!dahdi_is_e1_span(&wc->span))
		t13x_debounce_loopcodes(wc);
	handle_leds(wc);
	if (test_bit(INITIALIZED, &wc->bit_flags)) {
		if (SPAN_DEBOUNCE || SPAN_ALARMS)
			mod_timer(&wc->timer, jiffies + HZ/10);
	}
}

static void handle_falc_int(struct t13x *wc)
{
	unsigned char gis, isr0, isr1, isr2, isr3, isr4;
	unsigned long flags;
	static int intcount;
	bool start_timer;
	bool recheck_sigbits = false;

	intcount++;
	start_timer = FALSE;

	spin_lock_irqsave(&wc->reglock, flags);
	gis = __t13x_framer_get(wc, FRMR_GIS);
	isr0 = (gis & FRMR_GIS_ISR0) ? __t13x_framer_get(wc, FRMR_ISR0) : 0;
	isr1 = (gis & FRMR_GIS_ISR1) ? __t13x_framer_get(wc, FRMR_ISR1) : 0;
	isr2 = (gis & FRMR_GIS_ISR2) ? __t13x_framer_get(wc, FRMR_ISR2) : 0;
	isr3 = (gis & FRMR_GIS_ISR3) ? __t13x_framer_get(wc, FRMR_ISR3) : 0;
	isr4 = (gis & FRMR_GIS_ISR4) ? __t13x_framer_get(wc, FRMR_ISR4) : 0;

	if ((debug) && !(isr3 & ISR3_SEC)) {
		dev_info(&wc->dev->dev, "gis: %02x, isr0: %02x, isr1: %02x, "\
			"isr2: %02x, isr3: %02x, isr4: %02x, intcount=%u\n",
			gis, isr0, isr1, isr2, isr3, isr4, intcount);
	}

	/* Collect performance counters once per second */
	if (isr3 & ISR3_SEC) {
		wc->span.count.fe += __t13x_framer_get(wc, FECL_T);
		wc->span.count.crc4 += __t13x_framer_get(wc, CEC1L_T);
		wc->span.count.cv += __t13x_framer_get(wc, CVCL_T);
		wc->span.count.ebit += __t13x_framer_get(wc, EBCL_T);
		wc->span.count.be += __t13x_framer_get(wc, BECL_T);
		wc->span.count.prbs = __t13x_framer_get(wc, FRS1_T);

		if (DAHDI_RXSIG_INITIAL == wc->span.chans[0]->rxhooksig)
			recheck_sigbits = true;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* Collect errored second counter once per second */
	if (isr3 & ISR3_ES)
		wc->span.count.errsec += 1;

	if ((isr0 & 0x08) || recheck_sigbits)
		t13x_check_sigbits(wc);

	if (dahdi_is_e1_span(&wc->span)) {
		/* E1 checks */
		if ((isr3 & 0x68) || isr2 || (isr1 & 0x7f)) {
			t13x_check_alarms(wc);
			start_timer = TRUE;
		}
	} else {
		/* T1 checks */
		if (isr2) {
			t13x_check_alarms(wc);
			start_timer = TRUE;
		}
		if (isr3 & 0x08) {	/* T1 LLBSC */
			t13x_check_loopcodes(wc);
			start_timer = TRUE;
		}
	}

	if (!wc->span.alarms) {
		if ((isr3 & 0x3) || (isr4 & 0xc0))
			wc->span.count.timingslips++;
	} else
		wc->span.count.timingslips = 0;

	if (start_timer && !timer_pending(&wc->timer) &&
	    test_bit(INITIALIZED, &wc->bit_flags)) {
		mod_timer(&wc->timer, jiffies + HZ/10);
	}
}

static void te13x_handle_interrupt(struct wcxb *xb, u32 pending)
{
	struct t13x *wc = container_of(xb, struct t13x, xb);

	if (pending & FALC_INT) {
		handle_falc_int(wc);
	}
}

static void te13xp_timer(unsigned long data)
{
	struct t13x *wc = (struct t13x *)data;

	if (unlikely(!test_bit(INITIALIZED, &wc->bit_flags)))
		return;

	queue_work(wc->wq, &wc->timer_work);
	return;
}

static inline void create_sysfs_files(struct t13x *wc) { return; }
static inline void remove_sysfs_files(struct t13x *wc) { return; }

static const struct dahdi_span_ops t13x_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = t13x_spanconfig,
	.chanconfig = t13x_chanconfig,
	.startup = t13x_startup,
	.rbsbits = t13x_rbsbits,
	.maint = t13x_maint,
	.ioctl = t13x_ioctl,
	.set_spantype = t13x_set_linemode,
#ifdef VPM_SUPPORT
	.echocan_create = t13x_echocan_create,
	.echocan_name = t13x_echocan_name,
#endif /* VPM_SUPPORT */
};

#define SPI_BASE 0x200
#define SPISRR	(SPI_BASE + 0x40)
#define SPICR	(SPI_BASE + 0x60)
#define SPISR	(SPI_BASE + 0x64)
#define SPIDTR	(SPI_BASE + 0x68)
#define SPIDRR	(SPI_BASE + 0x6c)
#define SPISSR	(SPI_BASE + 0x70)

/**
 * t13x_read_serial - Returns the serial number of the board.
 * @wc: The board whos serial number we are reading.
 *
 * The buffer returned is dynamically allocated and must be kfree'd by the
 * caller. If memory could not be allocated, NULL is returned.
 *
 * Must be called in process context.
 *
 * TODO: Move this up into wcxb.c
 */
static char *t13x_read_serial(struct t13x *wc)
{
	int i;
	static const int MAX_SERIAL = 20*5;
	const unsigned int SERIAL_ADDRESS = 0x1f0000;
	unsigned char *serial = kzalloc(MAX_SERIAL + 1, GFP_KERNEL);
	struct wcxb const *xb = &wc->xb;
	struct wcxb_spi_master *flash_spi_master = NULL;
	struct wcxb_spi_device *flash_spi_device = NULL;

	if (!serial)
		return NULL;

	flash_spi_master = wcxb_spi_master_create(&xb->pdev->dev,
						  xb->membase + SPI_BASE,
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

static int __devinit te13xp_init_one(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct t13x *wc;
	const struct t13x_desc *d = (struct t13x_desc *) ent->driver_data;
	unsigned int x;
	int res;
	unsigned int index = -1;
	enum linemode type;

	for (x = 0; x < ARRAY_SIZE(ifaces); x++) {
		if (!ifaces[x]) {
			index = x;
			break;
		}
	}

	if (-1 == index) {
		pr_info("Too many interfaces\n");
		pci_disable_device(pdev);
		return -EIO;
	}

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc) {
		return -ENOMEM;
	}

	wc->dev = pdev;
	wc->devtype = d;
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	/* Set the performance counters to 0 */
	t13x_reset_counters(&wc->span);

	ifaces[index] = wc;

	sprintf(wc->span.name, "WCT13x/%d", index);
	snprintf(wc->span.desc, sizeof(wc->span.desc) - 1, "%s Card %d",
		 d->name, index);
	wc->ledstate = -1;
	spin_lock_init(&wc->reglock);
	mutex_init(&wc->lock);
	setup_timer(&wc->timer, te13xp_timer, (unsigned long)wc);

#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&wc->timer_work, timer_work_func, wc);
#	else
	INIT_WORK(&wc->timer_work, timer_work_func);
#	endif

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

	wc->name = kasprintf(GFP_KERNEL, "wcte13xp%d", index);
	if (!wc->name) {
		res = -ENOMEM;
		goto fail_exit;
	}

	pci_set_drvdata(pdev, wc);

	/* Setup wcxb library */
	wc->xb.pdev = pdev;
	wc->xb.ops = &xb_ops;
	wc->xb.debug = &debug;
	res = wcxb_init(&wc->xb, wc->name, 1);
	if (res)
		goto fail_exit;

	/* Check for field updatable firmware */
	if (is_pcie(wc)) {
		res = wcxb_check_firmware(&wc->xb, TE13X_FW_VERSION,
				TE133_FW_FILENAME, force_firmware);
		if (res)
			goto fail_exit;
	} else {
		res = wcxb_check_firmware(&wc->xb, TE13X_FW_VERSION,
				TE134_FW_FILENAME, force_firmware);
		if (res)
			goto fail_exit;
	}

	wc->ddev->hardware_id = t13x_read_serial(wc);

	wc->wq = create_singlethread_workqueue(wc->name);
	if (!wc->wq) {
		res = -ENOMEM;
		goto fail_exit;
	}

	/* Initial buffer latency size,
	   adjustable on load by modparam "latency" */
	if (latency > 0 && latency < DRING_SIZE) {
		wcxb_set_minlatency(&wc->xb, latency);
		if (WCXB_DEFAULT_LATENCY != latency)
			dev_info(&wc->dev->dev,
				"latency manually overridden to %d\n",
				latency);
	} else {
		dev_info(&wc->dev->dev,
			"latency module parameter must be between 1 and %d\n",
			DRING_SIZE);
		res = -EPERM;
		goto fail_exit;
	}

	create_sysfs_files(wc);

	res = t13x_hardware_post_init(wc, &type);
	if (res)
		goto fail_exit;

	wc->span.chans = wc->chans;

	res = t13x_software_init(wc, type);
	if (res)
		goto fail_exit;

#ifdef VPM_SUPPORT
	if (!wc->vpm)
		t13x_vpm_init(wc);
#endif

	wc->span.ops = &t13x_span_ops;
	list_add_tail(&wc->span.device_node, &wc->ddev->spans);

	/* Span is in red alarm by default */
	wc->span.alarms = DAHDI_ALARM_NONE;

	res = dahdi_register_device(wc->ddev, &wc->dev->dev);
	if (res) {
		dev_info(&wc->dev->dev, "Unable to register with DAHDI\n");
		goto fail_exit;
	}

	set_bit(INITIALIZED, &wc->bit_flags);
	mod_timer(&wc->timer, jiffies + HZ/5);

	if (wc->ddev->hardware_id) {
		dev_info(&wc->dev->dev, "Found a %s (SN: %s)\n",
				wc->devtype->name, wc->ddev->hardware_id);
	} else {
		dev_info(&wc->dev->dev, "Found a %s\n",
				wc->devtype->name);
	}

	return 0;

fail_exit:
	if (&wc->xb)
		wcxb_release(&wc->xb);

	free_wc(wc);
	return res;
}

static void __devexit te13xp_remove_one(struct pci_dev *pdev)
{
	struct t13x *wc = pci_get_drvdata(pdev);
	dev_info(&wc->dev->dev, "Removing a Wildcard TE13xP.\n");
	if (!wc)
		return;

	clear_bit(INITIALIZED, &wc->bit_flags);
	smp_mb__after_clear_bit();

	/* Quiesce DMA engine interrupts */
	wcxb_stop(&wc->xb);

	/* Leave framer in reset so it no longer transmits */
	wcxb_gpio_clear(&wc->xb, FALC_CPU_RESET);

	del_timer_sync(&wc->timer);
	flush_workqueue(wc->wq);
	del_timer_sync(&wc->timer);

	/* Turn off status LED */
	t13x_setleds(wc, 0);

#ifdef VPM_SUPPORT
	if (wc->vpm)
		release_vpm450m(wc->vpm);
	wc->vpm = NULL;
#endif

	dahdi_unregister_device(wc->ddev);

	remove_sysfs_files(wc);

	wcxb_release(&wc->xb);
	free_wc(wc);
}

static DEFINE_PCI_DEVICE_TABLE(te13xp_pci_tbl) = {
	{ 0xd161, 0x800a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &te133},
	{ 0xd161, 0x800b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &te134},
	{ 0 }
};

static void te13xp_shutdown(struct pci_dev *pdev)
{
	struct t13x *wc = pci_get_drvdata(pdev);
	dev_info(&wc->dev->dev, "Quiescing a Wildcard TE13xP.\n");
	if (!wc)
		return;

	/* Quiesce and mask DMA engine interrupts */
	wcxb_stop(&wc->xb);
}

static int te13xp_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return -ENOSYS;
}

MODULE_DEVICE_TABLE(pci, te13xp_pci_tbl);

static struct pci_driver te13xp_driver = {
	.name = "wcte13xp",
	.probe = te13xp_init_one,
	.remove = __devexit_p(te13xp_remove_one),
	.shutdown = te13xp_shutdown,
	.suspend = te13xp_suspend,
	.id_table = te13xp_pci_tbl,
};

static int __init te13xp_init(void)
{
	int res;

	if (strcasecmp(default_linemode, "t1") &&
	    strcasecmp(default_linemode, "j1") &&
	    strcasecmp(default_linemode, "e1")) {
		pr_err("'%s' is an unknown span type.\n", default_linemode);
		default_linemode = "t1";
		return -EINVAL;
	}

	res = dahdi_pci_module(&te13xp_driver);
	if (res)
		return -ENODEV;

	return 0;
}


static void __exit te13xp_cleanup(void)
{
	pci_unregister_driver(&te13xp_driver);
}

module_param(debug, int, S_IRUGO | S_IWUSR);
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

MODULE_DESCRIPTION("Wildcard Digital Card Driver");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(te13xp_init);
module_exit(te13xp_cleanup);
