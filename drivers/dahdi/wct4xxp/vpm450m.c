/*
 * Copyright (C) 2005-2012 Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * All Rights Reserved
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

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/version.h>

#include <dahdi/kernel.h>

/* Linux kernel 5.16 and greater has removed user-space headers from the kernel include path */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
#include <asm/types.h>
#elif defined RHEL_RELEASE_VERSION
#if defined(RHEL_RELEASE_CODE) && LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && \
              RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9,1)
#include <asm/types.h>
#endif
#else
#include <stdbool.h>
#endif

#include "vpm450m.h"
#include <oct612x.h>

static int wct4xxp_oct612x_write(struct oct612x_context *context,
				 u32 address, u16 value)
{
	struct t4 *wc = dev_get_drvdata(context->dev);
	oct_set_reg(wc, address, value);
	return 0;
}

static int wct4xxp_oct612x_read(struct oct612x_context *context, u32 address,
				u16 *value)
{
	struct t4 *wc = dev_get_drvdata(context->dev);
	*value = (u16)oct_get_reg(wc, address);
	return 0;
}

static int wct4xxp_oct612x_write_smear(struct oct612x_context *context,
				       u32 address, u16 value, size_t count)
{
	struct t4 *wc = dev_get_drvdata(context->dev);
	int i;
	for (i = 0; i < count; ++i)
		oct_set_reg(wc, address + (i << 1), value);
	return 0;
}

static int wct4xxp_oct612x_write_burst(struct oct612x_context *context,
				       u32 address, const u16 *buffer,
				       size_t count)
{
	struct t4 *wc = dev_get_drvdata(context->dev);
	int i;
	for (i = 0; i < count; ++i)
		oct_set_reg(wc, address + (i << 1), buffer[i]);
	return 0;
}

static int wct4xxp_oct612x_read_burst(struct oct612x_context *context,
				      u32 address, u16 *buffer, size_t count)
{
	struct t4 *wc = dev_get_drvdata(context->dev);
	int i;
	for (i = 0; i < count; ++i)
		buffer[i] = oct_get_reg(wc, address + (i << 1));
	return 0;
}

static const struct oct612x_ops wct4xxp_oct612x_ops = {
	.write = wct4xxp_oct612x_write,
	.read = wct4xxp_oct612x_read,
	.write_smear = wct4xxp_oct612x_write_smear,
	.write_burst = wct4xxp_oct612x_write_burst,
	.read_burst = wct4xxp_oct612x_read_burst,
};

#define SOUT_G168_1100GB_ON 0x40000004
#define SOUT_DTMF_1 0x40000011
#define SOUT_DTMF_2 0x40000012
#define SOUT_DTMF_3 0x40000013
#define SOUT_DTMF_A 0x4000001A
#define SOUT_DTMF_4 0x40000014
#define SOUT_DTMF_5 0x40000015
#define SOUT_DTMF_6 0x40000016
#define SOUT_DTMF_B 0x4000001B
#define SOUT_DTMF_7 0x40000017
#define SOUT_DTMF_8 0x40000018
#define SOUT_DTMF_9 0x40000019
#define SOUT_DTMF_C 0x4000001C
#define SOUT_DTMF_STAR 0x4000001E
#define SOUT_DTMF_0 0x40000010
#define SOUT_DTMF_POUND 0x4000001F
#define SOUT_DTMF_D 0x4000001D

#define ROUT_G168_2100GB_ON 0x10000000
#define ROUT_G168_2100GB_WSPR 0x10000002
#define ROUT_SOUT_G168_2100HB_END 0x50000003
#define ROUT_G168_1100GB_ON 0x10000004

#define ROUT_DTMF_1 0x10000011
#define ROUT_DTMF_2 0x10000012
#define ROUT_DTMF_3 0x10000013
#define ROUT_DTMF_A 0x1000001A
#define ROUT_DTMF_4 0x10000014
#define ROUT_DTMF_5 0x10000015
#define ROUT_DTMF_6 0x10000016
#define ROUT_DTMF_B 0x1000001B
#define ROUT_DTMF_7 0x10000017
#define ROUT_DTMF_8 0x10000018
#define ROUT_DTMF_9 0x10000019
#define ROUT_DTMF_C 0x1000001C
#define ROUT_DTMF_STAR 0x1000001E
#define ROUT_DTMF_0 0x10000010
#define ROUT_DTMF_POUND 0x1000001F
#define ROUT_DTMF_D 0x1000001D

#if 0 
#define cOCT6100_ECHO_OP_MODE_DIGITAL cOCT6100_ECHO_OP_MODE_HT_FREEZE
#else
#define cOCT6100_ECHO_OP_MODE_DIGITAL cOCT6100_ECHO_OP_MODE_POWER_DOWN
#endif

struct vpm450m {
	tPOCT6100_INSTANCE_API pApiInstance;
	struct oct612x_context context;
	UINT32 aulEchoChanHndl[256];
	int chanflags[256];
	int ecmode[256];
	int numchans;
};

#define FLAG_DTMF	 (1 << 0)
#define FLAG_MUTE	 (1 << 1)
#define FLAG_ECHO	 (1 << 2)
#define FLAG_ALAW	 (1 << 3)

static unsigned int tones[] = {
	SOUT_DTMF_1,
	SOUT_DTMF_2,
	SOUT_DTMF_3,
	SOUT_DTMF_A,
	SOUT_DTMF_4,
	SOUT_DTMF_5,
	SOUT_DTMF_6,
	SOUT_DTMF_B,
	SOUT_DTMF_7,
	SOUT_DTMF_8,
	SOUT_DTMF_9,
	SOUT_DTMF_C,
	SOUT_DTMF_STAR,
	SOUT_DTMF_0,
	SOUT_DTMF_POUND,
	SOUT_DTMF_D,
	SOUT_G168_1100GB_ON,

	ROUT_DTMF_1,
	ROUT_DTMF_2,
	ROUT_DTMF_3,
	ROUT_DTMF_A,
	ROUT_DTMF_4,
	ROUT_DTMF_5,
	ROUT_DTMF_6,
	ROUT_DTMF_B,
	ROUT_DTMF_7,
	ROUT_DTMF_8,
	ROUT_DTMF_9,
	ROUT_DTMF_C,
	ROUT_DTMF_STAR,
	ROUT_DTMF_0,
	ROUT_DTMF_POUND,
	ROUT_DTMF_D,
	ROUT_G168_1100GB_ON,
};

void vpm450m_set_alaw_companding(struct vpm450m *vpm450m, int channel,
					bool alaw)
{
	tOCT6100_CHANNEL_MODIFY *modify;
	UINT32 ulResult;
	UINT32		law_to_use = (alaw) ? cOCT6100_PCM_A_LAW :
					      cOCT6100_PCM_U_LAW;

	if (channel >= ARRAY_SIZE(vpm450m->chanflags)) {
		pr_err("Channel out of bounds in %s\n", __func__);
		return;
	}
	/* If we're already in this companding mode, no need to do anything. */
	if (alaw == ((vpm450m->chanflags[channel] & FLAG_ALAW) > 0))
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
			vpm450m->chanflags[channel] |= FLAG_ALAW;
		else
			vpm450m->chanflags[channel] &= ~(FLAG_ALAW);
	}
	kfree(modify);
}

static void vpm450m_setecmode(struct vpm450m *vpm450m, int channel, int mode)
{
	tOCT6100_CHANNEL_MODIFY *modify;
	UINT32 ulResult;

	if (vpm450m->ecmode[channel] == mode)
		return;
	modify = kzalloc(sizeof(*modify), GFP_ATOMIC);
	if (!modify) {
		printk(KERN_NOTICE "wct4xxp: Unable to allocate memory for setec!\n");
		return;
	}
	Oct6100ChannelModifyDef(modify);
	modify->ulEchoOperationMode = mode;
	modify->ulChannelHndl = vpm450m->aulEchoChanHndl[channel];
	ulResult = Oct6100ChannelModify(vpm450m->pApiInstance, modify);
	if (ulResult != GENERIC_OK) {
		printk(KERN_NOTICE "Failed to apply echo can changes on channel %d %08x!\n", channel, ulResult);
	} else {
#ifdef OCTASIC_DEBUG
		printk(KERN_DEBUG "Echo can on channel %d set to %d\n", channel, mode);
#endif
		vpm450m->ecmode[channel] = mode;
	}
	kfree(modify);
}

void vpm450m_setdtmf(struct vpm450m *vpm450m, int channel, int detect, int mute)
{
	tOCT6100_CHANNEL_MODIFY *modify;
	UINT32 ulResult;

	if (channel >= ARRAY_SIZE(vpm450m->chanflags)) {
		pr_err("Channel out of bounds in %s\n", __func__);
		return;
	}

	modify = kzalloc(sizeof(*modify), GFP_KERNEL);
	if (!modify) {
		printk(KERN_NOTICE "wct4xxp: Unable to allocate memory for setdtmf!\n");
		return;
	}
	Oct6100ChannelModifyDef(modify);
	modify->ulChannelHndl = vpm450m->aulEchoChanHndl[channel];
	if (mute) {
		vpm450m->chanflags[channel] |= FLAG_MUTE;
		modify->VqeConfig.fDtmfToneRemoval = TRUE;
	} else {
		vpm450m->chanflags[channel] &= ~FLAG_MUTE;
		modify->VqeConfig.fDtmfToneRemoval = FALSE;
	}
	if (detect)
		vpm450m->chanflags[channel] |= FLAG_DTMF;
	else
		vpm450m->chanflags[channel] &= ~FLAG_DTMF;
	if (vpm450m->chanflags[channel] & (FLAG_DTMF|FLAG_MUTE)) {
		if (!(vpm450m->chanflags[channel] & FLAG_ECHO)) {
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_HT_RESET);
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_HT_FREEZE);
		}
	} else {
		if (!(vpm450m->chanflags[channel] & FLAG_ECHO))
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_DIGITAL);
	}

	ulResult = Oct6100ChannelModify(vpm450m->pApiInstance, modify);
	if (ulResult != GENERIC_OK) {
		printk(KERN_NOTICE "Failed to apply dtmf mute changes on channel %d!\n", channel);
	}
/*	printk(KERN_DEBUG "VPM450m: Setting DTMF on channel %d: %s / %s\n", channel, (detect ? "DETECT" : "NO DETECT"), (mute ? "MUTE" : "NO MUTE")); */
	kfree(modify);
}

void vpm450m_setec(struct vpm450m *vpm450m, int channel, int eclen)
{
	if (channel >= ARRAY_SIZE(vpm450m->chanflags)) {
		pr_err("Channel out of bounds in %s\n", __func__);
		return;
	}

	if (eclen) {
		vpm450m->chanflags[channel] |= FLAG_ECHO;
		vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_HT_RESET);
		vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_NORMAL);
	} else {
		vpm450m->chanflags[channel] &= ~FLAG_ECHO;
		if (vpm450m->chanflags[channel] & (FLAG_DTMF | FLAG_MUTE)) {
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_HT_RESET);
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_HT_FREEZE);
		} else
			vpm450m_setecmode(vpm450m, channel, cOCT6100_ECHO_OP_MODE_DIGITAL);
	}
/*	printk(KERN_DEBUG "VPM450m: Setting EC on channel %d to %d\n", channel, eclen); */
}

int vpm450m_checkirq(struct vpm450m *vpm450m)
{
	tOCT6100_INTERRUPT_FLAGS InterruptFlags;
	
	Oct6100InterruptServiceRoutineDef(&InterruptFlags);
	Oct6100InterruptServiceRoutine(vpm450m->pApiInstance, &InterruptFlags);

	return InterruptFlags.fToneEventsPending ? 1 : 0;
}

int vpm450m_getdtmf(struct vpm450m *vpm450m, int *channel, int *tone, int *start)
{
	tOCT6100_TONE_EVENT tonefound;
	tOCT6100_EVENT_GET_TONE tonesearch;
	
	Oct6100EventGetToneDef(&tonesearch);
	tonesearch.pToneEvent = &tonefound;
	tonesearch.ulMaxToneEvent = 1;
	Oct6100EventGetTone(vpm450m->pApiInstance, &tonesearch);
	if (tonesearch.ulNumValidToneEvent) {
		if (channel)
			*channel = tonefound.ulUserChanId;
		if (tone) {
			switch(tonefound.ulToneDetected) {
			case SOUT_DTMF_1:
				*tone = '1';
				break;
			case SOUT_DTMF_2:
				*tone = '2';
				break;
			case SOUT_DTMF_3:
				*tone = '3';
				break;
			case SOUT_DTMF_A:
				*tone = 'A';
				break;
			case SOUT_DTMF_4:
				*tone = '4';
				break;
			case SOUT_DTMF_5:
				*tone = '5';
				break;
			case SOUT_DTMF_6:
				*tone = '6';
				break;
			case SOUT_DTMF_B:
				*tone = 'B';
				break;
			case SOUT_DTMF_7:
				*tone = '7';
				break;
			case SOUT_DTMF_8:
				*tone = '8';
				break;
			case SOUT_DTMF_9:
				*tone = '9';
				break;
			case SOUT_DTMF_C:
				*tone = 'C';
				break;
			case SOUT_DTMF_STAR:
				*tone = '*';
				break;
			case SOUT_DTMF_0:
				*tone = '0';
				break;
			case SOUT_DTMF_POUND:
				*tone = '#';
				break;
			case SOUT_DTMF_D:
				*tone = 'D';
				break;
			case SOUT_G168_1100GB_ON:
				*tone = 'f';
				break;
			default:
#ifdef OCTASIC_DEBUG
				printk(KERN_DEBUG "Unknown tone value %08x\n", tonefound.ulToneDetected);
#endif
				*tone = 'u';
				break;
			}
		}
		if (start)
			*start = (tonefound.ulEventType == cOCT6100_TONE_PRESENT);
		return 1;
	}
	return 0;
}

unsigned int get_vpm450m_capacity(struct device *device)
{
	struct oct612x_context context;
	UINT32 ulResult;

	tOCT6100_API_GET_CAPACITY_PINS CapacityPins;

	context.dev = device;
	context.ops = &wct4xxp_oct612x_ops;

	Oct6100ApiGetCapacityPinsDef(&CapacityPins);
	CapacityPins.pProcessContext = &context;
	CapacityPins.ulMemoryType = cOCT6100_MEM_TYPE_DDR;
	CapacityPins.fEnableMemClkOut = TRUE;
	CapacityPins.ulMemClkFreq = cOCT6100_MCLK_FREQ_133_MHZ;

	ulResult = Oct6100ApiGetCapacityPins(&CapacityPins);
	if (ulResult != cOCT6100_ERR_OK) {
		printk(KERN_DEBUG "Failed to get chip capacity, code %08x!\n", ulResult);
		return 0;
	}

	return CapacityPins.ulCapacityValue;
}

struct vpm450m *init_vpm450m(struct device *device, int *isalaw,
			     int numspans, const struct firmware *firmware)
{
	tOCT6100_CHIP_OPEN *ChipOpen;
	tOCT6100_GET_INSTANCE_SIZE InstanceSize;
	tOCT6100_CHANNEL_OPEN *ChannelOpen;
	UINT32 ulResult;
	const unsigned int mask = (8 == numspans) ? 0x7 : 0x3;
	unsigned int sout_stream, rout_stream;
	struct vpm450m *vpm450m;
	int x,y,law;
	
	vpm450m = kzalloc(sizeof(*vpm450m), GFP_KERNEL);
	if (!vpm450m)
		return NULL;

	vpm450m->context.dev = device;
	vpm450m->context.ops = &wct4xxp_oct612x_ops;

	ChipOpen = kzalloc(sizeof(*ChipOpen), GFP_KERNEL);
	if (!ChipOpen) {
		kfree(vpm450m);
		kfree(vpm450m);
		return NULL;
	}

	ChannelOpen = kzalloc(sizeof(*ChannelOpen), GFP_KERNEL);
	if (!ChannelOpen) {
		kfree(vpm450m);
		kfree(ChipOpen);
		return NULL;
	}

	for (x = 0; x < ARRAY_SIZE(vpm450m->ecmode); x++)
		vpm450m->ecmode[x] = -1;

	vpm450m->numchans = numspans * 32;
	printk(KERN_INFO "VPM450: echo cancellation for %d channels\n", vpm450m->numchans);
		
	Oct6100ChipOpenDef(ChipOpen);

	/* Setup Chip Open Parameters */
	ChipOpen->ulUpclkFreq = cOCT6100_UPCLK_FREQ_33_33_MHZ;
	Oct6100GetInstanceSizeDef(&InstanceSize);

	ChipOpen->pProcessContext = &vpm450m->context;

	ChipOpen->pbyImageFile = firmware->data;
	ChipOpen->ulImageSize = firmware->size;
	ChipOpen->fEnableMemClkOut = TRUE;
	ChipOpen->ulMemClkFreq = cOCT6100_MCLK_FREQ_133_MHZ;
	ChipOpen->ulMaxChannels = vpm450m->numchans;
	ChipOpen->ulMemoryType = cOCT6100_MEM_TYPE_DDR;
	ChipOpen->ulMemoryChipSize = cOCT6100_MEMORY_CHIP_SIZE_32MB;
	ChipOpen->ulNumMemoryChips = 1;
	ChipOpen->aulTdmStreamFreqs[0] = cOCT6100_TDM_STREAM_FREQ_8MHZ;
	ChipOpen->ulMaxFlexibleConfParticipants = 0;
	ChipOpen->ulMaxConfBridges = 0;
	ChipOpen->ulMaxRemoteDebugSessions = 0;
	ChipOpen->fEnableChannelRecording = FALSE;
	ChipOpen->ulSoftToneEventsBufSize = 64;

	if (vpm450m->numchans <= 128) {
		ChipOpen->ulMaxTdmStreams = 4;
		ChipOpen->ulTdmSampling = cOCT6100_TDM_SAMPLE_AT_FALLING_EDGE;
	} else {
		ChipOpen->ulMaxTdmStreams = 32;
		ChipOpen->fEnableFastH100Mode = TRUE;
		ChipOpen->ulTdmSampling = cOCT6100_TDM_SAMPLE_AT_RISING_EDGE;
	}

#if 0
	ChipOpen->fEnableAcousticEcho = TRUE;
#endif		

	ulResult = Oct6100GetInstanceSize(ChipOpen, &InstanceSize);
	if (ulResult != cOCT6100_ERR_OK) {
		printk(KERN_NOTICE "Failed to get instance size, code %08x!\n", ulResult);
		kfree(vpm450m);
		kfree(ChipOpen);
		kfree(ChannelOpen);
		return NULL;
	}
	
	vpm450m->pApiInstance = vmalloc(InstanceSize.ulApiInstanceSize);
	if (!vpm450m->pApiInstance) {
		printk(KERN_NOTICE "Out of memory (can't allocate %d bytes)!\n", InstanceSize.ulApiInstanceSize);
		kfree(vpm450m);
		kfree(ChipOpen);
		kfree(ChannelOpen);
		return NULL;
	}

	ulResult = Oct6100ChipOpen(vpm450m->pApiInstance, ChipOpen);
	if (ulResult != cOCT6100_ERR_OK) {
		printk(KERN_NOTICE "Failed to open chip, code %08x!\n", ulResult);
		vfree(vpm450m->pApiInstance);
		kfree(vpm450m);
		kfree(ChipOpen);
		kfree(ChannelOpen);
		return NULL;
	}

	sout_stream = (8 == numspans) ? 29 : 2;
	rout_stream = (8 == numspans) ? 24 : 3;

	for (x = 0; x < ((8 == numspans) ? 256 : 128); x++) {
		/* execute this loop always on 4 span cards but
		*  on 2 span cards only execute for the channels related to our spans */
		if (( numspans > 2) || ((x & 0x03) <2)) {
			/* span timeslots are interleaved 12341234... 
		 	*  therefore, the lower 2 bits tell us which span this 
			*  timeslot/channel
		 	*/
			if (isalaw[x & mask]) {
				law = cOCT6100_PCM_A_LAW;
				vpm450m->chanflags[x] |= FLAG_ALAW;
			} else {
				law = cOCT6100_PCM_U_LAW;
				vpm450m->chanflags[x] &= ~(FLAG_ALAW);
			}
			Oct6100ChannelOpenDef(ChannelOpen);
			ChannelOpen->pulChannelHndl = &vpm450m->aulEchoChanHndl[x];
			ChannelOpen->ulUserChanId = x;
			ChannelOpen->TdmConfig.ulRinPcmLaw = law;
			ChannelOpen->TdmConfig.ulRinStream = 0;
			ChannelOpen->TdmConfig.ulRinTimeslot = x;
			ChannelOpen->TdmConfig.ulSinPcmLaw = law;
			ChannelOpen->TdmConfig.ulSinStream = 1;
			ChannelOpen->TdmConfig.ulSinTimeslot = x;
			ChannelOpen->TdmConfig.ulSoutPcmLaw = law;
			ChannelOpen->TdmConfig.ulSoutStream = sout_stream;
			ChannelOpen->TdmConfig.ulSoutTimeslot = x;
#if 1
			ChannelOpen->TdmConfig.ulRoutPcmLaw = law;
			ChannelOpen->TdmConfig.ulRoutStream = rout_stream;
			ChannelOpen->TdmConfig.ulRoutTimeslot = x;
#endif
			ChannelOpen->VqeConfig.fEnableNlp = TRUE;
			ChannelOpen->VqeConfig.fRinDcOffsetRemoval = TRUE;
			ChannelOpen->VqeConfig.fSinDcOffsetRemoval = TRUE;
			
			ChannelOpen->fEnableToneDisabler = TRUE;
			ChannelOpen->ulEchoOperationMode = cOCT6100_ECHO_OP_MODE_DIGITAL;
			
			ulResult = Oct6100ChannelOpen(vpm450m->pApiInstance, ChannelOpen);
			if (ulResult != GENERIC_OK) {
				printk(KERN_NOTICE "Failed to open channel %d %x!\n", x, ulResult);
				continue;
			}
			for (y = 0; y < ARRAY_SIZE(tones); y++) {
				tOCT6100_TONE_DETECTION_ENABLE enable;
				Oct6100ToneDetectionEnableDef(&enable);
				enable.ulChannelHndl = vpm450m->aulEchoChanHndl[x];
				enable.ulToneNumber = tones[y];
				if (Oct6100ToneDetectionEnable(vpm450m->pApiInstance, &enable) != GENERIC_OK) 
					printk(KERN_NOTICE "Failed to enable tone detection on channel %d for tone %d!\n", x, y);
			}
		}
	}

	kfree(ChipOpen);
	kfree(ChannelOpen);
	return vpm450m;
}

void release_vpm450m(struct vpm450m *vpm450m)
{
	UINT32 ulResult;
	tOCT6100_CHIP_CLOSE ChipClose;

	Oct6100ChipCloseDef(&ChipClose);
	ulResult = Oct6100ChipClose(vpm450m->pApiInstance, &ChipClose);
	if (ulResult != cOCT6100_ERR_OK) {
		printk(KERN_NOTICE "Failed to close chip, code %08x!\n", ulResult);
	}
	vfree(vpm450m->pApiInstance);
	kfree(vpm450m);
}
