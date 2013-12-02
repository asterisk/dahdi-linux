/*
 * A4A,A4B,A8A,A8B TDM FXS/FXO Interface Driver for DAHDI Telephony interface
 *
 * Copyright (C) 2013 Digium, Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
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

#include <dahdi/kernel.h>

#include <linux/version.h>
#include <linux/mutex.h>

#include <oct612x.h>

#include "wcxb.h"
#include "wcxb_spi.h"
#include "wcxb_flash.h"

/*!
 * \brief Default ringer debounce (in ms)
 */
#define DEFAULT_RING_DEBOUNCE	1024
#define POLARITY_DEBOUNCE	64		/* Polarity debounce (in ms) */

#define OHT_TIMER		6000	/* How long after RING to retain OHT */

#define FLAG_EXPRESS	(1 << 0)

#define NUM_MODULES		8

#define VPM_SUPPORT

#define CMD_WR(addr, val) (((addr<<8)&0xff00) | (val&0xff))

enum battery_state {
	BATTERY_UNKNOWN = 0,
	BATTERY_DEBOUNCING_PRESENT,
	BATTERY_DEBOUNCING_PRESENT_FROM_LOST_ALARM,
	BATTERY_DEBOUNCING_PRESENT_ALARM,
	BATTERY_PRESENT,
	BATTERY_DEBOUNCING_LOST,
	BATTERY_DEBOUNCING_LOST_FROM_PRESENT_ALARM,
	BATTERY_DEBOUNCING_LOST_ALARM,
	BATTERY_LOST,
};

enum ring_detector_state {
	RINGOFF = 0,
	DEBOUNCING_RINGING_POSITIVE,
	DEBOUNCING_RINGING_NEGATIVE,
	RINGING,
	DEBOUNCING_RINGOFF,
};

enum polarity_state {
	UNKNOWN_POLARITY = 0,
	POLARITY_DEBOUNCE_POSITIVE,
	POLARITY_POSITIVE,
	POLARITY_DEBOUNCE_NEGATIVE,
	POLARITY_NEGATIVE,
};

struct wcaxx_chan {
	struct dahdi_chan chan;
	struct dahdi_echocan_state ec;
	int timeslot;
	unsigned int hwpreec_enabled:1;
};

struct fxo {
	enum ring_detector_state ring_state:4;
	enum battery_state battery_state:4;
	enum polarity_state polarity_state:4;
	u8 ring_polarity_change_count:4;
	u8 hook_ring_shadow;
	s8 line_voltage_status;
	int offhook;
	int neonmwi_state;
	int neonmwi_last_voltage;
	unsigned int neonmwi_debounce;
	unsigned int neonmwi_offcounter;
	unsigned long display_fxovoltage;
	unsigned long ringdebounce_timer;
	unsigned long battdebounce_timer;
	unsigned long poldebounce_timer;
};

struct fxs {
	int idletxhookstate;	/* IDLE changing hook state */
	/* lasttxhook reflects the last value written to the proslic's reg 64
	 * (LINEFEED_CONTROL) in bits 0-2.  Bit 4 indicates if the last write is
	 * pending i.e. it is in process of being written to the register NOTE:
	 * in order for this value to actually be written to the proslic, the
	 * appropriate matching value must be written into the sethook variable
	 * so that it gets queued and handled by the voicebus ISR.
	*/
	u8 lasttxhook;
	u8 linefeed_control_shadow;
	u8 hook_state_shadow;
	u8 oht_active:1;
	u8 off_hook:1;
	int palarms;
	struct dahdi_vmwi_info vmwisetting;
	int vmwi_active_messages;
	int vmwi_linereverse;
	int reversepolarity;	/* polarity reversal */
	struct {
		u8 vals[12];
	} calregs;
	unsigned long check_alarm;
	unsigned long check_proslic;
	unsigned long oppending_timeout;
	unsigned long ohttimer;
};

#define fxs_lf(fxs, value) _fxs_lf((fxs), SLIC_LF_##value)
static inline bool _fxs_lf(const struct fxs *fxs, const unsigned value)
{
	return (fxs->lasttxhook & SLIC_LF_SETMASK) == value;
}

enum module_type {
	NONE = 0,
	FXS,
	FXO,
};

#define MODULE_POLL_TIME_MS 10

struct wcaxx_mod_poll {
	struct wcxb_spi_message m;
	struct wcxb_spi_transfer t;
	struct wcaxx_module *mod;
	struct wcaxx *wc;
	u8 buffer[6];
	u8 master_buffer[6];
};

struct wcaxx_module {
	union modtypes {
		struct fxo fxo;
		struct fxs fxs;
	} mod;
	u8 card;
	u8 subaddr;
	enum module_type type;
	int sethook; /* pending hook state command */
	int dacssrc;
	struct wcxb_spi_device *spi;
	struct wcaxx_mod_poll *mod_poll;
};

struct _device_desc {
	const char *name;
	unsigned int ports;
};

static const struct _device_desc device_a8a = { "Wildcard A8A", 8};
static const struct _device_desc device_a8b = { "Wildcard A8B", 8};
static const struct _device_desc device_a4a = { "Wildcard A4A", 4};
static const struct _device_desc device_a4b = { "Wildcard A4B", 4};

struct wcaxx {
	const struct _device_desc *desc;
	const char *board_name;

	unsigned long framecount;
	unsigned long module_poll_time;
	int mods_per_board;

	spinlock_t reglock;
	struct wcaxx_module mods[NUM_MODULES];
	struct wcxb xb;
	struct dahdi_span  span;
	struct wcaxx_chan *chans[NUM_MODULES];
	struct dahdi_echocan_state *ec[NUM_MODULES];
	int companding;
	struct dahdi_device *ddev;
	struct wcxb_spi_master *master;
#define INITIALIZED 0
	unsigned long bit_flags;
	/* 4 SPI devices that are matched to the chip selects. The 4 port
	 * modules will share a single SPI device since they use the same chip
	 * select. */
	struct wcxb_spi_device *spi_devices[4];
	struct vpm450m *vpm;
	struct list_head card_node;
	u16 num;
};

static inline bool is_pcie(const struct wcaxx *wc)
{
	return (wc->desc == &device_a8b) || (wc->desc == &device_a4b);
}

static inline bool is_four_port(const struct wcaxx *wc)
{
	return (4 == wc->desc->ports);
}

#ifdef VPM_SUPPORT
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include "oct6100api/oct6100_api.h"

#define ECHOCAN_NUM_CHANS 8

#define FLAG_DTMF	 (1 << 0)
#define FLAG_MUTE	 (1 << 1)
#define FLAG_ECHO	 (1 << 2)

#define OCT_CHIP_ID			0
#define OCT_MAX_TDM_STREAMS		4
#define OCT_TONEEVENT_BUFFER_SIZE	128
#define SOUT_STREAM			1
#define RIN_STREAM			0
#define SIN_STREAM			2

static int vpmsupport = 1;
static int wcaxx_vpm_init(struct wcaxx *wc);
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
	UINT32 aulEchoChanHndl[32];
	int chanflags[32];
	int ecmode[32];
	int numchans;
};

static int wcaxx_oct612x_write(struct oct612x_context *context,
			      u32 address, u16 value)
{
	struct wcaxx *wc = dev_get_drvdata(context->dev);
	wcxb_set_echocan_reg(&wc->xb, address, value);
	return 0;
}

static int wcaxx_oct612x_read(struct oct612x_context *context, u32 address,
			     u16 *value)
{
	struct wcaxx *wc = dev_get_drvdata(context->dev);
	*value = wcxb_get_echocan_reg(&wc->xb, address);
	return 0;
}

static int wcaxx_oct612x_write_smear(struct oct612x_context *context,
				    u32 address, u16 value, size_t count)
{
	unsigned int i;
	struct wcaxx *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		wcxb_set_echocan_reg(&wc->xb, address + (i << 1), value);
	return 0;
}

static int wcaxx_oct612x_write_burst(struct oct612x_context *context,
				    u32 address, const u16 *buffer,
				    size_t count)
{
	unsigned int i;
	struct wcaxx *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		wcxb_set_echocan_reg(&wc->xb, address + (i << 1), buffer[i]);
	return 0;
}

static int wcaxx_oct612x_read_burst(struct oct612x_context *context,
				   u32 address, u16 *buffer, size_t count)
{
	unsigned int i;
	struct wcaxx *wc = dev_get_drvdata(context->dev);
	for (i = 0; i < count; ++i)
		buffer[i] = wcxb_get_echocan_reg(&wc->xb, address + (i << 1));
	return 0;
}

static const struct oct612x_ops wcaxx_oct612x_ops = {
	.write = wcaxx_oct612x_write,
	.read = wcaxx_oct612x_read,
	.write_smear = wcaxx_oct612x_write_smear,
	.write_burst = wcaxx_oct612x_write_burst,
	.read_burst = wcaxx_oct612x_read_burst,
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

static void vpm450m_setec(struct vpm450m *vpm450m, int channel, int eclen)
{
	if (eclen) {
		vpm450m->chanflags[channel] |= FLAG_ECHO;
		vpm450m_setecmode(vpm450m, channel,
				  cOCT6100_ECHO_OP_MODE_NORMAL);
	} else {
		vpm450m->chanflags[channel] &= ~FLAG_ECHO;
		if (vpm450m->chanflags[channel] & (FLAG_DTMF | FLAG_MUTE)) {
			vpm450m_setecmode(vpm450m, channel,
					  cOCT6100_ECHO_OP_MODE_HT_RESET);
		} else {
			vpm450m_setecmode(vpm450m, channel,
					  cOCT6100_ECHO_OP_MODE_POWER_DOWN);
		}
	}
}

static UINT32 tdmmode_chan_to_slot_map(int mode, int channel)
{
	/* Four phases on the tdm bus, skip three of them per channel */
	/* Due to a bug in the octasic, we had to move the data onto phase 2 */
	return 1+(channel*4);
}

static int echocan_initialize_channel(struct vpm450m *vpm,
				      int channel, int mode)
{
	tOCT6100_CHANNEL_OPEN	ChannelOpen;
	UINT32		law_to_use = cOCT6100_PCM_U_LAW;
	UINT32		tdmslot_setting;
	UINT32		ulResult;

	if (0 > channel || ECHOCAN_NUM_CHANS <= channel)
		return -1;

	tdmslot_setting = tdmmode_chan_to_slot_map(mode, channel);

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

static struct vpm450m *init_vpm450m(struct wcaxx *wc, int isalaw,
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
		dev_info(&wc->xb.pdev->dev, "Unable to allocate vpm450m struct\n");
		return NULL;
	}

	vpm450m->context.dev = &wc->xb.pdev->dev;
	vpm450m->context.ops = &wcaxx_oct612x_ops;

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

	vpm450m->numchans = ECHOCAN_NUM_CHANS;
	dev_info(&wc->xb.pdev->dev, "Echo cancellation for %d channels\n",
		 wc->desc->ports);

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
	ChipOpen->ulMaxChannels		= vpm450m->numchans;
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

	/* In this example we will maintain the API using polling so interrupts
	 * must be disabled */
	ChipOpen->InterruptConfig.ulErrorH100Config =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulErrorMemoryConfig =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulFatalGeneralConfig =
						cOCT6100_INTERRUPT_DISABLE;
	ChipOpen->InterruptConfig.ulFatalMemoryConfig =
						cOCT6100_INTERRUPT_DISABLE;

	ChipOpen->ulSoftToneEventsBufSize = OCT_TONEEVENT_BUFFER_SIZE;

	/* Inserting default values into tOCT6100_GET_INSTANCE_SIZE structure
	 * parameters. */
	Oct6100GetInstanceSizeDef(&InstanceSize);

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
	/* Open all channels */
	for (i = 0; i < ECHOCAN_NUM_CHANS; i++) {
		ulResult = echocan_initialize_channel(vpm450m, i, isalaw);
		if (0 != ulResult) {
			dev_info(&wc->xb.pdev->dev,
				"Unable to echocan_initialize_channel: %x\n",
				ulResult);
			return NULL;
		}
	}

	if (vpmsupport)
		wcxb_enable_echocan(&wc->xb);
	else
		wcxb_disable_echocan(&wc->xb);

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

static const char *wcaxx_echocan_name(const struct dahdi_chan *chan)
{
	struct wcaxx *wc = chan->pvt;
	if (wc->vpm)
		return "VPMOCT032";
	else
		return NULL;
}

static int wcaxx_echocan_create(struct dahdi_chan *chan,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p,
			     struct dahdi_echocan_state **ec)
{
	struct wcaxx *wc = chan->pvt;
	int channel;
	const struct dahdi_echocan_ops *ops;
	const struct dahdi_echocan_features *features;

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

	*ec = wc->ec[chan->chanpos - 1];
	(*ec)->ops = ops;
	(*ec)->features = *features;

	channel = chan->chanpos-1;

	if (wc->vpm)
		vpm450m_setec(wc->vpm, channel, ecp->tap_length);
	return 0;
}

static void echocan_free(struct dahdi_chan *chan,
			 struct dahdi_echocan_state *ec)
{
	struct wcaxx *wc = chan->pvt;
	int channel;

	memset(ec, 0, sizeof(*ec));

	channel = chan->chanpos - 1;

	if (wc->vpm)
		vpm450m_setec(wc->vpm, channel, 0);
}

static int wcaxx_vpm_init(struct wcaxx *wc)
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
	int res;

	if (!vpmsupport) {
		dev_info(&wc->xb.pdev->dev, "VPM: Support Disabled\n");
		return -1;
	}

	wcxb_reset_echocan(&wc->xb);
	if (!wcxb_is_echocan_present(&wc->xb)) {
		dev_info(&wc->xb.pdev->dev, "VPM not present.\n");
		return -1;
	}

#if defined(HOTPLUG_FIRMWARE)
	res = request_firmware(&firmware, oct032_firmware, &wc->xb.pdev->dev);
	if ((0 != res) || !firmware) {
		dev_notice(&wc->xb.pdev->dev,
			   "VPM450: firmware %s not available from userspace\n",
			   oct032_firmware);
		return -1;
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

	wc->vpm = init_vpm450m(wc, companding, firmware);
	if (!wc->vpm) {
		dev_notice(&wc->xb.pdev->dev, "VPM450: Failed to initialize\n");
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		return -EIO;
	}

	if (firmware != &embedded_firmware)
		release_firmware(firmware);

	dev_info(&wc->xb.pdev->dev,
		 "VPM450: Present and operational servicing %d span\n", 1);

	return 0;
}
#endif /* VPM_SUPPORT */

static inline bool is_initialized(struct wcaxx *wc)
{
	return (test_bit(INITIALIZED, &wc->bit_flags) > 0);
}

static inline struct wcxb_spi_device *
_get_spi_device_for_8_port(struct wcaxx *wc, unsigned int port, bool altcs)
{
	switch (port) {
	case 0:
		return wc->spi_devices[0];
	case 1:
		return (altcs) ? wc->spi_devices[0] : wc->spi_devices[1];
	case 2:
		WARN_ON(!altcs);
		return wc->spi_devices[0];
	case 3:
		WARN_ON(!altcs);
		return wc->spi_devices[0];
	case 4:
		return wc->spi_devices[2];
	case 5:
		return (altcs) ? wc->spi_devices[2] : wc->spi_devices[3];
	case 6:
		WARN_ON(!altcs);
		return wc->spi_devices[2];
	case 7:
		WARN_ON(!altcs);
		return wc->spi_devices[2];
	default:
		WARN_ON(1);
		return wc->spi_devices[0];
	}
}

static inline struct wcxb_spi_device *
_get_spi_device_for_4_port(struct wcaxx *wc, unsigned int port)
{
	if (port > 3) {
		WARN_ON(1);
		return wc->spi_devices[0];
	} else {
		return wc->spi_devices[port];
	}
}

static inline struct wcxb_spi_device *
get_spi_device_for_port(struct wcaxx *wc, unsigned int port, bool altcs)
{
	if (is_four_port(wc))
		return _get_spi_device_for_4_port(wc, port);
	else
		return _get_spi_device_for_8_port(wc, port, altcs);
}

static u8 wcaxx_getreg(struct wcaxx *wc,
			struct wcaxx_module *const mod, int addr);
static void wcaxx_setreg(struct wcaxx *wc, struct wcaxx_module *const mod,
			  int addr, int val);

static DEFINE_MUTEX(card_list_lock);
static LIST_HEAD(card_list);

#include "adt_lec.h"

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

/* Following define is a logical exclusive OR to determine if the polarity of
 * an fxs line is to be reversed. The items taken into account are:
 *	overall polarity reversal for the module,
 *	polarity reversal for the port,
 *	and the state of the line reversal MWI indicator
 */
#define POLARITY_XOR(fxs) \
	((reversepolarity != 0) ^ ((fxs)->reversepolarity != 0) ^ \
	((fxs)->vmwi_linereverse != 0))

static int reversepolarity;

static alpha  indirect_regs[] = {
	{0, 255, "DTMF_ROW_0_PEAK", 0x55C2},
	{1, 255, "DTMF_ROW_1_PEAK", 0x51E6},
	{2, 255, "DTMF_ROW2_PEAK", 0x4B85},
	{3, 255, "DTMF_ROW3_PEAK", 0x4937},
	{4, 255, "DTMF_COL1_PEAK", 0x3333},
	{5, 255, "DTMF_FWD_TWIST", 0x0202},
	{6, 255, "DTMF_RVS_TWIST", 0x0202},
	{7, 255, "DTMF_ROW_RATIO_TRES", 0x0198},
	{8, 255, "DTMF_COL_RATIO_TRES", 0x0198},
	{9, 255, "DTMF_ROW_2ND_ARM", 0x0611},
	{10, 255, "DTMF_COL_2ND_ARM", 0x0202},
	{11, 255, "DTMF_PWR_MIN_TRES", 0x00E5},
	{12, 255, "DTMF_OT_LIM_TRES", 0x0A1C},
	{13, 0, "OSC1_COEF", 0x7B30},
	{14, 1, "OSC1X", 0x0063},
	{15, 2, "OSC1Y", 0x0000},
	{16, 3, "OSC2_COEF", 0x7870},
	{17, 4, "OSC2X", 0x007D},
	{18, 5, "OSC2Y", 0x0000},
	{19, 6, "RING_V_OFF", 0x0000},
	{20, 7, "RING_OSC", 0x7EF0},
	{21, 8, "RING_X", 0x0160},
	{22, 9, "RING_Y", 0x0000},
	{23, 255, "PULSE_ENVEL", 0x2000},
	{24, 255, "PULSE_X", 0x2000},
	{25, 255, "PULSE_Y", 0x0000},
	{26, 13, "RECV_DIGITAL_GAIN", 0x2000}, /* playback volume set lower */
	{27, 14, "XMIT_DIGITAL_GAIN", 0x4000},
	{28, 15, "LOOP_CLOSE_TRES", 0x1000},
	{29, 16, "RING_TRIP_TRES", 0x3600},
	{30, 17, "COMMON_MIN_TRES", 0x1000},
	{31, 18, "COMMON_MAX_TRES", 0x0200},
	{32, 19, "PWR_ALARM_Q1Q2", 0x07C0},
	{33, 20, "PWR_ALARM_Q3Q4", 0x4C00 /* 0x2600 */},
	{34, 21, "PWR_ALARM_Q5Q6", 0x1B80},
	{35, 22, "LOOP_CLOSURE_FILTER", 0x8000},
	{36, 23, "RING_TRIP_FILTER", 0x0320},
	{37, 24, "TERM_LP_POLE_Q1Q2", 0x008C},
	{38, 25, "TERM_LP_POLE_Q3Q4", 0x0100},
	{39, 26, "TERM_LP_POLE_Q5Q6", 0x0010},
	{40, 27, "CM_BIAS_RINGING", 0x0C00},
	{41, 64, "DCDC_MIN_V", 0x0C00},
	{42, 255, "DCDC_XTRA", 0x1000},
	{43, 66, "LOOP_CLOSE_TRES_LOW", 0x1000},
};

/* Undefine to enable Power alarm / Transistor debug -- note: do not
   enable for normal operation! */
/* #define PAQ_DEBUG */

#define DEBUG_CARD (1 << 0)
#define DEBUG_ECHOCAN (1 << 1)

#include "fxo_modes.h"

static inline struct dahdi_chan *
get_dahdi_chan(const struct wcaxx *wc, struct wcaxx_module *const mod)
{
	return wc->span.chans[mod->card];
}

static inline void mod_hooksig(struct wcaxx *wc,
			       struct wcaxx_module *mod,
			       enum dahdi_rxsig rxsig)
{
	dahdi_hooksig(get_dahdi_chan(wc, mod), rxsig);
}

static void wcaxx_release(struct wcaxx *wc);

static int fxovoltage;
static unsigned int battdebounce;
static unsigned int battalarm;
static unsigned int battthresh;
static int debug;
static int int_mode;
#ifdef DEBUG
static int robust;
static int digitalloopback;
#endif
static int lowpower;
static int boostringer;
static int fastringer;
static int _opermode;
static char *opermode = "FCC";
static int fxshonormode;
static int alawoverride;
static char *companding = "auto";
static int fastpickup = -1; /* -1 auto, 0 no, 1 yes */
static int fxotxgain;
static int fxorxgain;
static int fxstxgain;
static int fxsrxgain;
static int nativebridge;
static int ringdebounce = DEFAULT_RING_DEBOUNCE;
static int latency = WCXB_DEFAULT_LATENCY;
static unsigned int max_latency = WCXB_DEFAULT_MAXLATENCY;
static int forceload;

#define MS_PER_HOOKCHECK	(1)
#define NEONMWI_ON_DEBOUNCE	(100/MS_PER_HOOKCHECK)
static int neonmwi_monitor;
static int neonmwi_level = 75;		/* neon mwi trip voltage */
static int neonmwi_envelope = 10;
/* Time in milliseconds the monitor is checked before saying no message is
 * waiting */
static int neonmwi_offlimit = 16000;
static int neonmwi_offlimit_cycles;

static int wcaxx_init_proslic(struct wcaxx *wc,
			       struct wcaxx_module *const mod, int fast,
			       int manual, int sane);


struct wcaxx_setreg_memory {
	struct wcxb_spi_message m;
	struct wcxb_spi_transfer t;
	u8	buffer[3];
};

/**
 * wcxb_spi_complete_setreg - Cleanup after a SPI write.
 *
 * We don't care about the results of setreg. Just go ahead and free up the
 * messages.
 *
 */
static void wcaxx_complete_setreg(void *arg)
{
	struct wcaxx_setreg_memory *setreg = arg;
	kfree(setreg);
}

static void wcaxx_setreg(struct wcaxx *wc, struct wcaxx_module *mod,
			  int addr, int val)
{
	struct wcaxx_setreg_memory *setreg = kzalloc(sizeof(*setreg),
						      GFP_ATOMIC);
	struct wcxb_spi_message	*const m = &setreg->m;
	struct wcxb_spi_transfer *const t = &setreg->t;
	if (!setreg) {
		WARN_ON_ONCE(!setreg);
		return;
	}
	wcxb_spi_message_init(m);
	t->tx_buf = setreg->buffer;
	wcxb_spi_message_add_tail(t, m);
	if (FXO == mod->type) {
		static const int ADDRS[4] = {0x00, 0x08, 0x04, 0x0c};
		setreg->buffer[0] = 0x20 | ADDRS[mod->subaddr];
	} else {
		setreg->buffer[0] = 1 << mod->subaddr;
	}
	setreg->buffer[1] = (addr) & 0x7f;
	setreg->buffer[2] = val;
	t->len = 3;
	m->complete = &wcaxx_complete_setreg;
	m->arg = setreg;
	wcxb_spi_async(mod->spi, m);
}

/**
 * wcaxx_fsxinit - Initilize all SPI devices to 3 byte mode.
 *
 * All the modules on the card need to be initialized to 3 byte mode in order to
 * talk to the daisy-chained SLIC / DAA on the quad modules.
 *
 */
static void wcaxx_fxsinit(struct wcxb_spi_device *const spi)
{
	int res;
	u8 data_byte[2] = {0, 0x80};
	struct wcxb_spi_transfer t;
	struct wcxb_spi_message	m;
	memset(&t, 0, sizeof(t));
	wcxb_spi_message_init(&m);
	t.tx_buf = data_byte;
	t.len = sizeof(data_byte);
	wcxb_spi_message_add_tail(&t, &m);
	res = wcxb_spi_sync(spi, &m);
	WARN_ON_ONCE(0 != res);
	return;
}

static u8
wcaxx_getreg(struct wcaxx *wc, struct wcaxx_module *const mod, int addr)
{
	int res;
	u8 buffer[3];
	struct wcxb_spi_message	m;
	struct wcxb_spi_transfer t;
	memset(&t, 0, sizeof(t));
	wcxb_spi_message_init(&m);

	t.tx_buf = t.rx_buf = buffer;
	t.len = sizeof(buffer);
	wcxb_spi_message_add_tail(&t, &m);

	if (FXO == mod->type) {
		static const int ADDRS[4] = {0x00, 0x08, 0x04, 0x0c};
		buffer[0] = 0x60 | ADDRS[mod->subaddr];
		buffer[1] = addr & 0x7f;
		buffer[2] = 0;
	} else {
		buffer[0] = 1 << mod->subaddr;
		buffer[1] = (addr | 0x80) & 0xff;
		buffer[2] = 0;
	}
	res = wcxb_spi_sync(mod->spi, &m);
	WARN_ON_ONCE(0 != res);
	return buffer[2];
}

static int wcaxx_getregs(struct wcaxx *wc, struct wcaxx_module *const mod,
			 int *const addresses, const size_t count)
{
	int x;
	for (x = 0; x < count; ++x)
		addresses[x] = wcaxx_getreg(wc, mod, addresses[x]);
	return 0;
}

static int wait_access(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	unsigned char data = 0;
	int count = 0;

	#define MAX 10 /* attempts */

	/* Wait for indirect access */
	while (count++ < MAX) {
		data = wcaxx_getreg(wc, mod, I_STATUS);
		if (!data)
			return 0;
	}

	if (count > (MAX-1)) {
		dev_notice(&wc->xb.pdev->dev,
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

static int wcaxx_proslic_setreg_indirect(struct wcaxx *wc,
					  struct wcaxx_module *const mod,
					  unsigned char address,
					  unsigned short data)
{
	int res = -1;

	address = translate_3215(address);
	if (address == 255)
		return 0;

	if (!wait_access(wc, mod)) {
		wcaxx_setreg(wc, mod, IDA_LO, (u8)(data & 0xFF));
		wcaxx_setreg(wc, mod, IDA_HI, (u8)((data & 0xFF00)>>8));
		wcaxx_setreg(wc, mod, IAA, address);
		res = 0;
	};
	return res;
}

static int wcaxx_proslic_getreg_indirect(struct wcaxx *wc,
					  struct wcaxx_module *const mod,
					  unsigned char address)
{
	int res = -1;
	char *p = NULL;

	address = translate_3215(address);
	if (address == 255)
		return 0;

	if (!wait_access(wc, mod)) {
		wcaxx_setreg(wc, mod, IAA, address);
		if (!wait_access(wc, mod)) {
			int addresses[2] = {IDA_LO, IDA_HI};
			wcaxx_getregs(wc, mod, addresses,
				      ARRAY_SIZE(addresses));
			res = addresses[0] | (addresses[1] << 8);
		} else
			p = "Failed to wait inside\n";
	} else
		p = "failed to wait\n";
	if (p)
		dev_notice(&wc->xb.pdev->dev, "%s", p);
	return res;
}

static int
wcaxx_proslic_init_indirect_regs(struct wcaxx *wc, struct wcaxx_module *mod)
{
	unsigned char i;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) {
		if (wcaxx_proslic_setreg_indirect(wc, mod,
				indirect_regs[i].address,
				indirect_regs[i].initial))
			return -1;
	}

	return 0;
}

static int wcaxx_proslic_verify_indirect_regs(struct wcaxx *wc,
					       struct wcaxx_module *mod)
{
	int passed = 1;
	unsigned short i, initial;
	int j;

	for (i = 0; i < ARRAY_SIZE(indirect_regs); i++) {
		j = wcaxx_proslic_getreg_indirect(wc, mod,
						(u8)indirect_regs[i].address);
		if (j < 0) {
			dev_notice(&wc->xb.pdev->dev,
				   "Failed to read indirect register %d\n", i);
			return -1;
		}
		initial = indirect_regs[i].initial;

		if ((j != initial) && (indirect_regs[i].altaddr != 255)) {
			dev_notice(&wc->xb.pdev->dev,
				   "!!!!!!! %s  iREG %X = %X  should be %X\n",
				   indirect_regs[i].name,
				   indirect_regs[i].address, j, initial);
			 passed = 0;
		}
	}

	if (passed) {
		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
			 "Init Indirect Registers completed successfully.\n");
		}
	} else {
		dev_notice(&wc->xb.pdev->dev,
			" !!!!! Init Indirect Registers UNSUCCESSFULLY.\n");
		return -1;
	}
	return 0;
}

/**
 * wcaxx_proslic_check_oppending -
 *
 * Ensures that a write to the line feed register on the SLIC has been
 * processed. If it hasn't after the timeout value, then it will resend the
 * command and wait for another timeout period.
 *
 */
static void wcaxx_proslic_check_oppending(struct wcaxx *wc,
					   struct wcaxx_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;
	unsigned long flags;

	if (!(fxs->lasttxhook & SLIC_LF_OPPENDING))
		return;

	/* Monitor the Pending LF state change, for the next 100ms */
	spin_lock_irqsave(&wc->reglock, flags);

	if (!(fxs->lasttxhook & SLIC_LF_OPPENDING)) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}

	if ((fxs->linefeed_control_shadow & SLIC_LF_SETMASK) ==
	    (fxs->lasttxhook & SLIC_LF_SETMASK)) {
		fxs->lasttxhook &= SLIC_LF_SETMASK;
		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
				 "SLIC_LF OK: card=%d shadow=%02x "
				 "lasttxhook=%02x framecount=%ld\n", mod->card,
				 fxs->linefeed_control_shadow,
				 fxs->lasttxhook, wc->framecount);
		}
	} else if (time_after(wc->framecount, fxs->oppending_timeout)) {
		/* Check again in 100 ms */
		fxs->oppending_timeout = wc->framecount + 100;

		wcaxx_setreg(wc, mod, LINE_STATE, fxs->lasttxhook);
		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
				 "SLIC_LF RETRY: card=%d shadow=%02x "
				 "lasttxhook=%02x framecount=%ld\n", mod->card,
				 fxs->linefeed_control_shadow,
				 fxs->lasttxhook, wc->framecount);
		}
	}

	spin_unlock_irqrestore(&wc->reglock, flags);
}

/* 256ms interrupt */
static void wcaxx_proslic_recheck_sanity(struct wcaxx *wc,
					  struct wcaxx_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;
	int res;
	unsigned long flags;
	const unsigned int MAX_ALARMS = 10;

#ifdef PAQ_DEBUG
	res = mod->isrshadow[1];
	res &= ~0x3;
	if (res) {
		mod->isrshadow[1] = 0;
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->xb.pdev->dev,
				   "Power alarm (%02x) on module %d, resetting!\n",
				   res, card + 1);
			mod->sethook = CMD_WR(19, res);
			/* Update shadow register to avoid extra power alarms
			 * until next read */
			mod->isrshadow[1] = 0;
		} else {
			if (fxs->palarms == MAX_ALARMS) {
				dev_notice(&wc->xb.pdev->dev,
					   "Too many power alarms on card %d, NOT resetting!\n",
					   card + 1);
			}
		}
	}
#else
	spin_lock_irqsave(&wc->reglock, flags);

	/* reg 64 has to be zero at last isr read */
	res = !fxs->linefeed_control_shadow &&
		!(fxs->lasttxhook & SLIC_LF_OPPENDING) && /* not a transition */
		fxs->lasttxhook; /* not an intended zero */

	if (res) {
		fxs->palarms++;
		if (fxs->palarms < MAX_ALARMS) {
			dev_notice(&wc->xb.pdev->dev,
				   "Power alarm on module %d, resetting!\n",
				   mod->card + 1);
			if (fxs->lasttxhook == SLIC_LF_RINGING) {
				fxs->lasttxhook = POLARITY_XOR(fxs) ?
							SLIC_LF_ACTIVE_REV :
							SLIC_LF_ACTIVE_FWD;
			}
			fxs->lasttxhook |= SLIC_LF_OPPENDING;
			mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
			fxs->oppending_timeout = wc->framecount + 100;

			/* Update shadow register to avoid extra power alarms
			 * until next read */
			fxs->linefeed_control_shadow = fxs->lasttxhook;
		} else {
			if (fxs->palarms == MAX_ALARMS) {
				dev_notice(&wc->xb.pdev->dev,
					   "Too many power alarms on card %d, "
					   "NOT resetting!\n", mod->card + 1);
			}
		}
	}
	spin_unlock_irqrestore(&wc->reglock, flags);
#endif
}

static inline bool is_fxo_ringing(const struct fxo *const fxo)
{
	return ((fxo->hook_ring_shadow & 0x60) &&
		((fxo->battery_state == BATTERY_PRESENT) ||
		 (fxo->battery_state == BATTERY_DEBOUNCING_LOST)));
}

static inline bool is_fxo_ringing_positive(const struct fxo *const fxo)
{
	return (((fxo->hook_ring_shadow & 0x60) == 0x20) &&
		((fxo->battery_state == BATTERY_PRESENT) ||
		 (fxo->battery_state == BATTERY_DEBOUNCING_LOST)));
}

static inline bool is_fxo_ringing_negative(const struct fxo *const fxo)
{
	return (((fxo->hook_ring_shadow & 0x60) == 0x40) &&
		((fxo->battery_state == BATTERY_PRESENT) ||
		 (fxo->battery_state == BATTERY_DEBOUNCING_LOST)));
}

static inline void set_ring(struct fxo *fxo, enum ring_detector_state new)
{
	fxo->ring_state = new;
}

static void wcaxx_fxo_ring_detect(struct wcaxx *wc, struct wcaxx_module *mod)
{
	struct fxo *const fxo = &mod->mod.fxo;
	static const unsigned int POLARITY_CHANGES_NEEDED = 2;

	/* Look for ring status bits (Ring Detect Signal Negative and Ring
	 * Detect Signal Positive) to transition back and forth
	 * POLARITY_CHANGES_NEEDED times to indicate that a ring is occurring.
	 * Provide some number of samples to allow for the transitions to occur
	 * before giving up.  NOTE: neon mwi voltages will trigger one of these
	 * bits to go active but not to have transitions between the two bits
	 * (i.e. no negative to positive or positive to negative traversals) */

	switch (fxo->ring_state) {
	case DEBOUNCING_RINGING_POSITIVE:
		if (is_fxo_ringing_negative(fxo)) {
			if (++fxo->ring_polarity_change_count >
						POLARITY_CHANGES_NEEDED) {
				mod_hooksig(wc, mod, DAHDI_RXSIG_RING);
				set_ring(fxo, RINGING);
				if (debug) {
					dev_info(&wc->xb.pdev->dev,
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
			if (++fxo->ring_polarity_change_count >
						POLARITY_CHANGES_NEEDED) {
				mod_hooksig(wc, mod, DAHDI_RXSIG_RING);
				set_ring(fxo, RINGING);
				if (debug) {
					dev_info(&wc->xb.pdev->dev,
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
					dev_info(&wc->xb.pdev->dev,
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
wcaxx_check_battery_lost(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	struct fxo *const fxo = &mod->mod.fxo;

	/* possible existing states:
	   battery lost, no debounce timer
	   battery lost, debounce timer (going to battery present)
	   battery present or unknown, no debounce timer
	   battery present or unknown, debounce timer (going to battery lost)
	*/
	switch (fxo->battery_state) {
	case BATTERY_DEBOUNCING_PRESENT_ALARM:
		fxo->battery_state = BATTERY_DEBOUNCING_LOST_FROM_PRESENT_ALARM;
		fxo->battdebounce_timer = wc->framecount + battdebounce;
		break;
	case BATTERY_DEBOUNCING_PRESENT:
		fxo->battery_state = BATTERY_LOST;
		break;
	case BATTERY_DEBOUNCING_PRESENT_FROM_LOST_ALARM:
		fxo->battery_state = BATTERY_DEBOUNCING_LOST_ALARM;
		fxo->battdebounce_timer = wc->framecount +
						battalarm - battdebounce;
		break;
	case BATTERY_UNKNOWN:
		mod_hooksig(wc, mod, DAHDI_RXSIG_ONHOOK);
	case BATTERY_PRESENT:
		fxo->battery_state = BATTERY_DEBOUNCING_LOST;
		fxo->battdebounce_timer = wc->framecount + battdebounce;
		break;
	case BATTERY_DEBOUNCING_LOST_FROM_PRESENT_ALARM:
	case BATTERY_DEBOUNCING_LOST: /* Intentional drop through */
		if (time_after(wc->framecount, fxo->battdebounce_timer)) {
			if (debug) {
				dev_info(&wc->xb.pdev->dev,
					 "NO BATTERY on %d/%d!\n",
					 wc->span.spanno,
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
wcaxx_check_battery_present(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	struct fxo *const fxo = &mod->mod.fxo;

	switch (fxo->battery_state) {
	case BATTERY_DEBOUNCING_PRESENT_FROM_LOST_ALARM:
	case BATTERY_DEBOUNCING_PRESENT: /* intentional drop through */
		if (time_after(wc->framecount, fxo->battdebounce_timer)) {
			if (debug) {
				dev_info(&wc->xb.pdev->dev,
					 "BATTERY on %d/%d (%s)!\n",
					 wc->span.spanno, mod->card + 1,
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
			fxo->battdebounce_timer = wc->framecount +
						   battalarm - battdebounce;
		}
		break;
	case BATTERY_DEBOUNCING_PRESENT_ALARM:
		if (time_after(wc->framecount, fxo->battdebounce_timer)) {
			fxo->battery_state = BATTERY_PRESENT;
			dahdi_alarm_channel(get_dahdi_chan(wc, mod),
					    DAHDI_ALARM_NONE);
		}
		break;
	case BATTERY_PRESENT:
		break;
	case BATTERY_DEBOUNCING_LOST_ALARM:
		fxo->battery_state = BATTERY_DEBOUNCING_PRESENT_FROM_LOST_ALARM;
		fxo->battdebounce_timer = wc->framecount + battdebounce;
		break;
	case BATTERY_DEBOUNCING_LOST_FROM_PRESENT_ALARM:
		fxo->battery_state = BATTERY_DEBOUNCING_PRESENT_ALARM;
		fxo->battdebounce_timer = wc->framecount +
						battalarm - battdebounce;
		break;
	case BATTERY_DEBOUNCING_LOST:
		fxo->battery_state = BATTERY_PRESENT;
		break;
	case BATTERY_UNKNOWN:
		mod_hooksig(wc, mod, DAHDI_RXSIG_OFFHOOK);
	case BATTERY_LOST: /* intentional drop through */
		fxo->battery_state = BATTERY_DEBOUNCING_PRESENT;
		fxo->battdebounce_timer = wc->framecount + battdebounce;
		break;
	}
}

static void
wcaxx_fxo_stop_debouncing_polarity(struct wcaxx *wc,
				   struct wcaxx_module *const mod)
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
wcaxx_fxo_check_polarity(struct wcaxx *wc, struct wcaxx_module *const mod,
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
				dev_info(&wc->xb.pdev->dev,
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
				dev_info(&wc->xb.pdev->dev,
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

static bool is_neon_voltage_present(const struct fxo *fxo, u8 abs_voltage)
{
	return (fxo->battery_state == BATTERY_PRESENT &&
		abs_voltage > neonmwi_level &&
		(0 == fxo->neonmwi_last_voltage ||
		 ((fxo->line_voltage_status >= fxo->neonmwi_last_voltage -
						neonmwi_envelope) &&
		  (fxo->line_voltage_status <= fxo->neonmwi_last_voltage +
						neonmwi_envelope)
		 )
		)
	       );
}

static void do_neon_monitor(struct wcaxx *wc,
			    struct wcaxx_module *mod, u8 abs_voltage)
{
	struct fxo *const fxo = &mod->mod.fxo;
	struct dahdi_chan *const chan = get_dahdi_chan(wc, mod);

	/* Look for 4 consecutive voltage readings where the voltage is over the
	 * neon limit but does not vary greatly from the last reading */
	if (is_neon_voltage_present(fxo, abs_voltage)) {
		fxo->neonmwi_last_voltage = fxo->line_voltage_status;
		if (NEONMWI_ON_DEBOUNCE == fxo->neonmwi_debounce) {
			fxo->neonmwi_offcounter = neonmwi_offlimit_cycles;
			if (0 == fxo->neonmwi_state) {
				dahdi_qevent_lock(chan,
						DAHDI_EVENT_NEONMWI_ACTIVE);
				fxo->neonmwi_state = 1;
				if (debug) {
					dev_info(&wc->xb.pdev->dev,
						 "NEON MWI active for card %d\n",
						 mod->card+1);
				}
			}
			fxo->neonmwi_debounce++;
		} else if (NEONMWI_ON_DEBOUNCE > fxo->neonmwi_debounce) {
			fxo->neonmwi_debounce++;
		} else {
			fxo->neonmwi_offcounter = neonmwi_offlimit_cycles;
		}
	} else {
		fxo->neonmwi_debounce = 0;
		fxo->neonmwi_last_voltage = 0;
	}

	/* If no neon mwi pulse for given period of time, indicte no neon mwi
	 * state */
	if (fxo->neonmwi_state && 0 < fxo->neonmwi_offcounter) {
		fxo->neonmwi_offcounter--;
		if (0 == fxo->neonmwi_offcounter) {
			dahdi_qevent_lock(get_dahdi_chan(wc, mod),
					  DAHDI_EVENT_NEONMWI_INACTIVE);
			fxo->neonmwi_state = 0;
			if (debug) {
				dev_info(&wc->xb.pdev->dev,
					 "NEON MWI cleared for card %d\n",
					 mod->card+1);
			}
		}
	}
}

static void
wcaxx_voicedaa_check_hook(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	signed char b;
	u8 abs_voltage;
	struct fxo *const fxo = &mod->mod.fxo;

	/* Try to track issues that plague slot one FXO's */
	b = fxo->hook_ring_shadow & 0x9b;

	if (fxo->offhook) {
		if (b != 0x9)
			wcaxx_setreg(wc, mod, 5, 0x9);
	} else {
		if (b != 0x8)
			wcaxx_setreg(wc, mod, 5, 0x8);

		wcaxx_fxo_ring_detect(wc, mod);
	}

	abs_voltage = abs(fxo->line_voltage_status);

	if (fxovoltage && time_after(wc->framecount, fxo->display_fxovoltage)) {
		/* Every 100 ms */
		fxo->display_fxovoltage = wc->framecount + 100;
		dev_info(&wc->xb.pdev->dev,
			 "Port %d: Voltage: %d\n",
			 mod->card + 1, fxo->line_voltage_status);
	}

	if (unlikely(DAHDI_RXSIG_INITIAL ==
				get_dahdi_chan(wc, mod)->rxhooksig)) {
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
		wcaxx_fxo_stop_debouncing_polarity(wc, mod);
		wcaxx_check_battery_lost(wc, mod);
	} else {
		wcaxx_check_battery_present(wc, mod);
		wcaxx_fxo_check_polarity(wc, mod,
					 (fxo->line_voltage_status > 0));
	}

	/* Look for neon mwi pulse */
	if (neonmwi_monitor && !fxo->offhook)
		do_neon_monitor(wc, mod, abs_voltage);
#undef MS_PER_CHECK_HOOK
}

static void
wcaxx_fxs_hooksig(struct wcaxx *wc, struct wcaxx_module *const mod,
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
		dev_notice(&wc->xb.pdev->dev,
			   "Can't set tx state to %d\n", txsig);
		return;
	}

	if (x != fxs->lasttxhook) {
		fxs->lasttxhook = x | SLIC_LF_OPPENDING;
		mod->sethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
		fxs->oppending_timeout = wc->framecount + 100;
		spin_unlock_irqrestore(&wc->reglock, flags);

		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
				 "Setting FXS hook state to %d (%02x) framecount=%ld\n",
				 txsig, x, wc->framecount);
		}
	} else {
		spin_unlock_irqrestore(&wc->reglock, flags);
	}
}

static void
wcaxx_fxs_off_hook(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;

	if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			"fxs_off_hook: Card %d Going off hook\n", mod->card);
	}
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
		wcaxx_fxs_hooksig(wc, mod, DAHDI_TXSIG_OFFHOOK);
	dahdi_hooksig(get_dahdi_chan(wc, mod), DAHDI_RXSIG_OFFHOOK);

#ifdef DEBUG
	if (robust)
		wcaxx_init_proslic(wc, mod, 1, 0, 1);
#endif
}

/**
 * wcaxx_fxs_on_hook - Report on hook to DAHDI.
 * @wc:		Board hosting the module.
 * @card:	Index of the module / port to place on hook.
 *
 * If we are intentionally dropping battery to signal a forward
 * disconnect we do not want to place the line "On-Hook". In this
 * case, the core of DAHDI will place us on hook when one of the RBS
 * timers expires.
 *
 */
static void
wcaxx_fxs_on_hook(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			"fxs_on_hook: Card %d Going on hook\n", mod->card);
	}

	if ((mod->mod.fxs.lasttxhook & SLIC_LF_SETMASK) != SLIC_LF_OPEN)
		wcaxx_fxs_hooksig(wc, mod, DAHDI_TXSIG_ONHOOK);
	dahdi_hooksig(get_dahdi_chan(wc, mod), DAHDI_RXSIG_ONHOOK);
}

static void
wcaxx_isr_misc_fxs(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	struct fxs *const fxs = &mod->mod.fxs;
	unsigned long flags;

	if (time_after(wc->framecount, fxs->check_alarm)) {
		/* Accept an alarm once per 10 seconds */
		fxs->check_alarm = wc->framecount + (1000*10);
		if (fxs->palarms)
			fxs->palarms--;
	}

	if (fxs->off_hook && !(fxs->hook_state_shadow & 1)) {
		wcaxx_fxs_on_hook(wc, mod);
		fxs->off_hook = 0;
	} else if (!fxs->off_hook && (fxs->hook_state_shadow & 1)) {
		wcaxx_fxs_off_hook(wc, mod);
		fxs->off_hook = 1;
	}

	wcaxx_proslic_check_oppending(wc, mod);

	if (time_after(wc->framecount, fxs->check_proslic)) {
		fxs->check_proslic = wc->framecount + 250; /* every 250ms */
		wcaxx_proslic_recheck_sanity(wc, mod);
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
			fxs->idletxhookstate = POLARITY_XOR(fxs) ?
					SLIC_LF_ACTIVE_REV : SLIC_LF_ACTIVE_FWD;
			spin_lock_irqsave(&wc->reglock, flags);
			if (SLIC_LF_OHTRAN_FWD == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING |
							SLIC_LF_ACTIVE_FWD;
				/* Data enqueued here */
				mod->sethook = CMD_WR(LINE_STATE,
						      fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d OnHookTransfer stop\n",
						 mod->card);
				}
			} else if (SLIC_LF_OHTRAN_REV == fxs->lasttxhook) {
				/* Apply the change if appropriate */
				fxs->lasttxhook = SLIC_LF_OPPENDING |
							SLIC_LF_ACTIVE_REV;
				/* Data enqueued here */
				mod->sethook = CMD_WR(LINE_STATE,
						      fxs->lasttxhook);
				if (debug & DEBUG_CARD) {
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d OnHookTransfer stop\n",
						 mod->card);
				}
			}
			spin_unlock_irqrestore(&wc->reglock, flags);
		} else {
			fxs->oht_active = 0;
			/* Switch to active */
			fxs->idletxhookstate = POLARITY_XOR(fxs) ?
					SLIC_LF_ACTIVE_REV : SLIC_LF_ACTIVE_FWD;
			if (debug & DEBUG_CARD) {
				dev_info(&wc->xb.pdev->dev,
					 "Channel %d OnHookTransfer abort\n",
					 mod->card);
			}
		}

	}
}

static void wcaxx_handle_receive(struct wcxb *xb, void *_frame)
{
	int i, j;
	struct wcaxx *wc = container_of(xb, struct wcaxx, xb);
	u8 *const frame = _frame;

	wc->framecount++;

	if (time_after(wc->framecount, wc->module_poll_time)) {
		for (i = 0; i < wc->mods_per_board; i++) {
			struct wcaxx_module *const mod = &wc->mods[i];
			if (mod->mod_poll) {
				wcxb_spi_async(mod->spi, &mod->mod_poll->m);
				mod->mod_poll = NULL;
			}
		}
		wc->module_poll_time = wc->framecount + MODULE_POLL_TIME_MS;
	}

	/* TODO: This protection needs to be thought about. */
	if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &wc->span.flags))
		return;

	for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
		for (i = 0; i < wc->span.channels; i++) {
			wc->chans[i]->chan.readchunk[j] =
				frame[j*WCXB_DMA_CHAN_SIZE+(1+i*4)];
		}
	}
	for (i = 0; i < wc->span.channels; i++) {
		struct dahdi_chan *const c = wc->span.chans[i];
		__dahdi_ec_chunk(c, c->readchunk, c->readchunk, c->writechunk);
	}
	_dahdi_receive(&wc->span);
	return;
}

static void wcaxx_handle_transmit(struct wcxb *xb, void *_frame)
{
	int i, j;
	struct wcaxx *wc = container_of(xb, struct wcaxx, xb);
	u8 *const frame = _frame;

	wcxb_spi_handle_interrupt(wc->master);

	/* TODO: This protection needs to be thought about. */
	if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &wc->span.flags))
		return;

	_dahdi_transmit(&wc->span);
	for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
		for (i = 0; i <  wc->span.channels; i++) {
			struct dahdi_chan *c = &wc->chans[i]->chan;
			frame[j*WCXB_DMA_CHAN_SIZE+(1+i*4)] = c->writechunk[j];
		}
	}
	return;
}

static int wcaxx_voicedaa_insane(struct wcaxx *wc, struct wcaxx_module *mod)
{
	int blah;
	blah = wcaxx_getreg(wc, mod, 2);
	if (blah != 0x3)
		return -2;
	blah = wcaxx_getreg(wc, mod, 11);
	if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			 "VoiceDAA System: %02x\n", blah & 0xf);
	}
	return 0;
}

static int
wcaxx_proslic_insane(struct wcaxx *wc, struct wcaxx_module *const mod)
{
	int blah, reg1, insane_report;
	insane_report = 0;

	blah = wcaxx_getreg(wc, mod, 0);
	if (blah != 0xff && (debug & DEBUG_CARD)) {
		dev_info(&wc->xb.pdev->dev,
			 "ProSLIC on module %d, product %d, "
			 "version %d\n", mod->card, (blah & 0x30) >> 4,
			 (blah & 0xf));
	}

#if 0
	if ((blah & 0x30) >> 4) {
		dev_info(&wc->xb.pdev->dev,
			 "ProSLIC on module %d is not a 3210.\n", mod->card);
		return -1;
	}
#endif
	if (((blah & 0xf) == 0) || ((blah & 0xf) == 0xf)) {
		/* SLIC not loaded */
		return -1;
	}

	/* let's be really sure this is an FXS before we continue */
	reg1 = wcaxx_getreg(wc, mod, 1);
	if ((0x80 != (blah & 0xf0)) || (0x88 != reg1)) {
		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
				 "DEBUG: not FXS b/c reg0=%x or "
				 "reg1 != 0x88 (%x).\n", blah, reg1);
		}
		return -1;
	}

	blah = wcaxx_getreg(wc, mod, 8);
	if (blah != 0x2) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d insane (1) %d should be 2\n",
			   mod->card, blah);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d Reg 8 Reads %d Expected "
			   "is 0x2\n", mod->card, blah);
	}

	blah = wcaxx_getreg(wc, mod, 64);
	if (blah != 0x0) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d insane (2)\n",
			   mod->card);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d Reg 64 Reads %d Expected "
			   "is 0x0\n", mod->card, blah);
	}

	blah = wcaxx_getreg(wc, mod, 11);
	if (blah != 0x33) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d insane (3)\n", mod->card);
		return -1;
	} else if (insane_report) {
		dev_notice(&wc->xb.pdev->dev,
			   "ProSLIC on module %d Reg 11 Reads %d "
			   "Expected is 0x33\n", mod->card, blah);
	}

	/* Just be sure it's setup right. */
	wcaxx_setreg(wc, mod, 30, 0);

	if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			 "ProSLIC on module %d seems sane.\n", mod->card);
	}
	return 0;
}

static int
wcaxx_proslic_powerleak_test(struct wcaxx *wc,
			      struct wcaxx_module *const mod)
{
	unsigned long start;
	unsigned char vbat;

	/* Turn off linefeed */
	wcaxx_setreg(wc, mod, LINE_STATE, 0);

	/* Power down */
	wcaxx_setreg(wc, mod, 14, 0x10);

	start = jiffies;

	/* TODO: Why is this sleep necessary.  Without it, the first read
	 * comes back with a 0 value. */
	msleep(20);

	while ((vbat = wcaxx_getreg(wc, mod, 82)) > 0x6) {
		if (time_after(jiffies, start + HZ/4))
			break;
	}

	if (vbat < 0x06) {
		dev_notice(&wc->xb.pdev->dev,
			   "Excessive leakage detected on module %d: %d "
			   "volts (%02x) after %d ms\n", mod->card,
			   376 * vbat / 1000, vbat,
			   (int)((jiffies - start) * 1000 / HZ));
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			 "Post-leakage voltage: %d volts\n", 376 * vbat / 1000);
	}
	return 0;
}

static int wcaxx_powerup_proslic(struct wcaxx *wc,
				 struct wcaxx_module *mod, int fast)
{
	unsigned char vbat;
	unsigned long origjiffies;
	int lim;

	/* Set period of DC-DC converter to 1/64 khz */
	wcaxx_setreg(wc, mod, 92, 0xc0 /* was 0xff */);

	/* Wait for VBat to powerup */
	origjiffies = jiffies;

	/* Disable powerdown */
	wcaxx_setreg(wc, mod, 14, 0);

	/* If fast, don't bother checking anymore */
	if (fast)
		return 0;

	while ((vbat = wcaxx_getreg(wc, mod, 82)) < 0xc0) {
		/* Wait no more than 500ms */
		if ((jiffies - origjiffies) > HZ/2)
			break;
	}

	if (vbat < 0xc0) {
		dev_notice(&wc->xb.pdev->dev, "ProSLIC on module %d failed to powerup within %d ms (%d mV only)\n\n -- DID YOU REMEMBER TO PLUG IN THE HD POWER CABLE TO THE TDM CARD??\n",
		       mod->card, (int)(((jiffies - origjiffies) * 1000 / HZ)),
			vbat * 375);
		return -1;
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			 "ProSLIC on module %d powered up to -%d volts (%02x) "
			 "in %d ms\n", mod->card, vbat * 376 / 1000, vbat,
			 (int)(((jiffies - origjiffies) * 1000 / HZ)));
	}

	/* Proslic max allowed loop current, reg 71 LOOP_I_LIMIT */
	/* If out of range, just set it to the default value     */
	lim = (loopcurrent - 20) / 3;
	if (loopcurrent > 41) {
		lim = 0;
		if (debug & DEBUG_CARD) {
			dev_info(&wc->xb.pdev->dev,
				 "Loop current out of range! Setting to default 20mA!\n");
		}
	} else if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev,
			 "Loop current set to %dmA!\n", (lim*3)+20);
	}
	wcaxx_setreg(wc, mod, LOOP_I_LIMIT, lim);

	/* Engage DC-DC converter */
	wcaxx_setreg(wc, mod, 93, 0x19 /* was 0x19 */);
	return 0;

}

static int
wcaxx_proslic_manual_calibrate(struct wcaxx *wc,
				struct wcaxx_module *const mod)
{
	unsigned long origjiffies;
	unsigned char i;

	/* Disable all interupts in DR21-23 */
	wcaxx_setreg(wc, mod, 21, 0);
	wcaxx_setreg(wc, mod, 22, 0);
	wcaxx_setreg(wc, mod, 23, 0);

	wcaxx_setreg(wc, mod, 64, 0);

	/* (0x18) Calibrations without the ADC and DAC offset and without
	 * common mode calibration. */
	wcaxx_setreg(wc, mod, 97, 0x18);

	/* (0x47) Calibrate common mode and differential DAC mode DAC + ILIM */
	wcaxx_setreg(wc, mod, 96, 0x47);

	origjiffies = jiffies;
	while (wcaxx_getreg(wc, mod, 96) != 0) {
		if ((jiffies-origjiffies) > 80)
			return -1;
	}
	/* Initialized DR 98 and 99 to get consistant results.  98 and 99 are
	 * the results registers and the search should have same intial
	 * conditions.
	 */

	/******* The following is the manual gain mismatch calibration ********/
	/******* This is also available as a function *************************/
	msleep(20);
	wcaxx_proslic_setreg_indirect(wc, mod, 88, 0);
	wcaxx_proslic_setreg_indirect(wc, mod, 89, 0);
	wcaxx_proslic_setreg_indirect(wc, mod, 90, 0);
	wcaxx_proslic_setreg_indirect(wc, mod, 91, 0);
	wcaxx_proslic_setreg_indirect(wc, mod, 92, 0);
	wcaxx_proslic_setreg_indirect(wc, mod, 93, 0);

	/* This is necessary if the calibration occurs other than at reset */
	wcaxx_setreg(wc, mod, 98, 0x10);
	wcaxx_setreg(wc, mod, 99, 0x10);

	for (i = 0x1f; i > 0; i--) {
		wcaxx_setreg(wc, mod, 98, i);
		msleep(40);
		if ((wcaxx_getreg(wc, mod, 88)) == 0)
			break;
	}

	for (i = 0x1f; i > 0; i--) {
		wcaxx_setreg(wc, mod, 99, i);
		msleep(40);
		if ((wcaxx_getreg(wc, mod, 89)) == 0)
			break;
	}

	/******** The preceding is the manual gain mismatch calibration *******/
	/******** The following is the longitudinal Balance Cal ***************/
	wcaxx_setreg(wc, mod, 64, 1);
	msleep(100);

	wcaxx_setreg(wc, mod, 64, 0);

	/* enable interrupt for the balance Cal */
	wcaxx_setreg(wc, mod, 23, 0x4);

	/* this is a singular calibration bit for longitudinal calibration */
	wcaxx_setreg(wc, mod, 97, 0x1);
	wcaxx_setreg(wc, mod, 96, 0x40);

	wcaxx_getreg(wc, mod, 96); /* Read Reg 96 just cause */

	wcaxx_setreg(wc, mod, 21, 0xFF);
	wcaxx_setreg(wc, mod, 22, 0xFF);
	wcaxx_setreg(wc, mod, 23, 0xFF);

	/**The preceding is the longitudinal Balance Cal***/
	return 0;

}

static int
wcaxx_proslic_calibrate(struct wcaxx *wc, struct wcaxx_module *mod)
{
	unsigned long origjiffies;
	int x;

	/* Perform all calibrations */
	wcaxx_setreg(wc, mod, 97, 0x1f);

	/* Begin, no speedup */
	wcaxx_setreg(wc, mod, 96, 0x5f);

	/* Wait for it to finish */
	origjiffies = jiffies;
	while (wcaxx_getreg(wc, mod, 96)) {
		if (time_after(jiffies, (origjiffies + (2*HZ)))) {
			dev_notice(&wc->xb.pdev->dev,
				   "Timeout waiting for calibration of "
				   "module %d\n", mod->card);
			return -1;
		}
	}

	if (debug & DEBUG_CARD) {
		/* Print calibration parameters */
		dev_info(&wc->xb.pdev->dev,
			 "Calibration Vector Regs 98 - 107:\n");
		for (x = 98; x < 108; x++) {
			dev_info(&wc->xb.pdev->dev,
				 "%d: %02x\n", x, wcaxx_getreg(wc, mod, x));
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
wcaxx_set_hwgain(struct wcaxx *wc, struct wcaxx_module *mod,
		 __s32 gain, __u32 tx)
{
	if (mod->type != FXO) {
		dev_notice(&wc->xb.pdev->dev,
			   "Cannot adjust gain. Unsupported module type!\n");
		return -1;
	}

	if (tx) {
		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "setting FXO tx gain for card=%d to %d\n",
				 mod->card, gain);
		}
		if (gain >=  -150 && gain <= 0) {
			wcaxx_setreg(wc, mod, 38, 16 + (gain / -10));
			wcaxx_setreg(wc, mod, 40, 16 + (-gain % 10));
		} else if (gain <= 120 && gain > 0) {
			wcaxx_setreg(wc, mod, 38, gain/10);
			wcaxx_setreg(wc, mod, 40, (gain%10));
		} else {
			dev_notice(&wc->xb.pdev->dev,
				   "FXO tx gain is out of range (%d)\n", gain);
			return -1;
		}
	} else { /* rx */
		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "setting FXO rx gain for card=%d to %d\n",
				 mod->card, gain);
		}
		if (gain >=  -150 && gain <= 0) {
			wcaxx_setreg(wc, mod, 39, 16 + (gain / -10));
			wcaxx_setreg(wc, mod, 41, 16 + (-gain % 10));
		} else if (gain <= 120 && gain > 0) {
			wcaxx_setreg(wc, mod, 39, gain/10);
			wcaxx_setreg(wc, mod, 41, (gain%10));
		} else {
			dev_notice(&wc->xb.pdev->dev,
				   "FXO rx gain is out of range (%d)\n", gain);
			return -1;
		}
	}

	return 0;
}

static int set_lasttxhook_interruptible(struct wcaxx *wc, struct fxs *fxs,
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
			msleep(100);
		} else {
			fxs->lasttxhook = (newval & SLIC_LF_SETMASK) |
						SLIC_LF_OPPENDING;
			*psethook = CMD_WR(LINE_STATE, fxs->lasttxhook);
			spin_unlock_irqrestore(&wc->reglock, flags);
			break;
		}
	} while (1);

	return res;
}

/* Must be called from within an interruptible context */
static int set_vmwi(struct wcaxx *wc, struct wcaxx_module *const mod)
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
		dev_info(&wc->xb.pdev->dev,
			 "Setting VMWI on channel %d, messages=%d, lrev=%d\n",
			 mod->card, fxs->vmwi_active_messages,
			 fxs->vmwi_linereverse);
	}
	return 0;
}

static void
wcaxx_voicedaa_set_ts(struct wcaxx *wc, struct wcaxx_module *mod, int ts)
{
	/* 34 bits from framesysc to the first channel, 8 bits in each ts * (th
	 * e timeslot we're assigning + 1 to skip for VPMOCT issue on first
	 * timeslot + 3 in that there are 4 bytes assigned for each timeslot on
	 * framer which was copied to this card */
	/* 34 + 8 * (ts + 1 + 3) */
	wcaxx_setreg(wc, mod, 34, (ts * 8 + 42 + (ts * 3 * 8)) & 0xff);
	wcaxx_setreg(wc, mod, 35, (ts * 8 + 42 + (ts * 3 * 8)) >> 8);
	wcaxx_setreg(wc, mod, 36, (ts * 8 + 42 + (ts * 3 * 8)) & 0xff);
	wcaxx_setreg(wc, mod, 37, (ts * 8 + 42 + (ts * 3 * 8)) >> 8);

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "voicedaa: card %d new timeslot: %d\n",
			 mod->card + 1, ts);
	}
}

static int
wcaxx_init_voicedaa(struct wcaxx *wc, struct wcaxx_module *mod,
		    int fast, int manual, int sane)
{
	unsigned char reg16 = 0, reg26 = 0, reg30 = 0, reg31 = 0;
	unsigned long flags;
	unsigned long newjiffies;

	/* Send a short write to the device in order to reset the SPI state
	 * machine. It may be out of sync since the driver was probing for an
	 * FXS device on that chip select. */
	/* wcxb_spi_short_write(mod->spi); */

	spin_lock_irqsave(&wc->reglock, flags);
	mod->type = FXO;
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (!sane && wcaxx_voicedaa_insane(wc, mod))
		return -2;

	/* Software reset */
	wcaxx_setreg(wc, mod, 1, 0x80);
	msleep(100);

	/* Set On-hook speed, Ringer impedence, and ringer threshold */
	reg16 |= (fxo_modes[_opermode].ohs << 6);
	reg16 |= (fxo_modes[_opermode].rz << 1);
	reg16 |= (fxo_modes[_opermode].rt);
	wcaxx_setreg(wc, mod, 16, reg16);

	/* Enable ring detector full-wave rectifier mode */
	wcaxx_setreg(wc, mod, 18, 2);
	wcaxx_setreg(wc, mod, 24, 0);

	/* Set DC Termination:
	   Tip/Ring voltage adjust, minimum operational current, current
	   limitation */
	reg26 |= (fxo_modes[_opermode].dcv << 6);
	reg26 |= (fxo_modes[_opermode].mini << 4);
	reg26 |= (fxo_modes[_opermode].ilim << 1);
	wcaxx_setreg(wc, mod, 26, reg26);

	/* Set AC Impedence */
	reg30 = (fxo_modes[_opermode].acim);
	wcaxx_setreg(wc, mod, 30, reg30);

	/* Misc. DAA parameters */

	/* If fast pickup is set, then the off hook counter will be set to 8
	 * ms, otherwise 128 ms. */
	reg31 = (fastpickup) ? 0xe3 : 0xa3;

	reg31 |= (fxo_modes[_opermode].ohs2 << 3);
	wcaxx_setreg(wc, mod, 31, reg31);

	wcaxx_voicedaa_set_ts(wc, mod, mod->card);

	/* Enable ISO-Cap */
	wcaxx_setreg(wc, mod, 6, 0x00);

	/* Turn off the calibration delay when fastpickup is enabled. */
	if (fastpickup)
		wcaxx_setreg(wc, mod, 17, wcaxx_getreg(wc, mod, 17) | 0x20);

	/* Wait 2000ms for ISO-cap to come up */
	newjiffies = jiffies + msecs_to_jiffies(2000);

	while (time_before(jiffies, newjiffies) &&
	       !(wcaxx_getreg(wc, mod, 11) & 0xf0))
		msleep(100);

	if (!(wcaxx_getreg(wc, mod, 11) & 0xf0)) {
		dev_notice(&wc->xb.pdev->dev, "VoiceDAA did not bring up ISO link properly!\n");
		return -1;
	}

	if (debug & DEBUG_CARD) {
		dev_info(&wc->xb.pdev->dev, "ISO-Cap is now up, line side: %02x rev %02x\n",
		       wcaxx_getreg(wc, mod, 11) >> 4,
		       (wcaxx_getreg(wc, mod, 13) >> 2) & 0xf);
	}

	/* Enable on-hook line monitor */
	wcaxx_setreg(wc, mod, 5, 0x08);

	/* Take values for fxotxgain and fxorxgain and apply them to module */
	wcaxx_set_hwgain(wc, mod, fxotxgain, 1);
	wcaxx_set_hwgain(wc, mod, fxorxgain, 0);

#ifdef DEBUG
	if (digitalloopback) {
		dev_info(&wc->xb.pdev->dev,
			 "Turning on digital loopback for port %d.\n",
			 mod->card + 1);
		wcaxx_setreg(wc, mod, 10, 0x01);
	}
#endif

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "DEBUG fxotxgain:%i.%i fxorxgain:%i.%i\n",
			 (wcaxx_getreg(wc, mod, 38)/16) ?
				-(wcaxx_getreg(wc, mod, 38) - 16) :
				wcaxx_getreg(wc, mod, 38),
			 (wcaxx_getreg(wc, mod, 40)/16) ?
				-(wcaxx_getreg(wc, mod, 40) - 16) :
				wcaxx_getreg(wc, mod, 40),
			 (wcaxx_getreg(wc, mod, 39)/16) ?
				-(wcaxx_getreg(wc, mod, 39) - 16) :
				wcaxx_getreg(wc, mod, 39),
			 (wcaxx_getreg(wc, mod, 41)/16) ?
				-(wcaxx_getreg(wc, mod, 41) - 16) :
				wcaxx_getreg(wc, mod, 41));
	}

	return 0;
}

static void
wcaxx_proslic_set_ts(struct wcaxx *wc, struct wcaxx_module *mod, int ts)
{
	/* Tx Start low byte  0 */
	wcaxx_setreg(wc, mod, 2, (ts * 8 + 42 + (ts * 3 * 8)) & 0xff);
	/* Tx Start high byte 0 */
	wcaxx_setreg(wc, mod, 3, (ts * 8 + 42 + (ts * 3 * 8)) >> 8);
	/* Rx Start low byte  0 */
	wcaxx_setreg(wc, mod, 4, (ts * 8 + 42 + (ts * 3 * 8)) & 0xff);
	/* Rx Start high byte 0 */
	wcaxx_setreg(wc, mod, 5, (ts * 8 + 42 + (ts * 3 * 8)) >> 8);

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "proslic: card %d new timeslot: %d\n",
			 mod->card + 1, ts);
	}
}

static int
wcaxx_init_proslic(struct wcaxx *wc, struct wcaxx_module *const mod,
		   int fast, int manual, int sane)
{

	struct fxs *const fxs = &mod->mod.fxs;
	unsigned short tmp[5];
	unsigned long flags;
	unsigned char r19, r9;
	int x;
	int fxsmode = 0;
	int addresses[ARRAY_SIZE(fxs->calregs.vals)];

#if 0 /* TODO */
	if (wc->mods[mod->card & 0xfc].type == QRV)
		return -2;
#endif

	spin_lock_irqsave(&wc->reglock, flags);
	mod->type = FXS;
	spin_unlock_irqrestore(&wc->reglock, flags);

	/* msleep(100); */

	/* Sanity check the ProSLIC */
	if (!sane && wcaxx_proslic_insane(wc, mod))
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
		/* Make sure we turn off the DC->DC converter to prevent
		 * anything from blowing up */
		wcaxx_setreg(wc, mod, 14, 0x10);
	}

	if (wcaxx_proslic_init_indirect_regs(wc, mod)) {
		dev_info(&wc->xb.pdev->dev,
			 "Indirect Registers failed to initialize on "
			 "module %d.\n", mod->card);
		return -1;
	}

	/* Clear scratch pad area */
	wcaxx_proslic_setreg_indirect(wc, mod, 97, 0);

	/* Clear digital loopback */
	wcaxx_setreg(wc, mod, 8, 0);

	/* Revision C optimization */
	wcaxx_setreg(wc, mod, 108, 0xeb);

	/* Disable automatic VBat switching for safety to prevent
	 * Q7 from accidently turning on and burning out.
	 * If pulse dialing has trouble at high REN loads change this to 0x17 */
	wcaxx_setreg(wc, mod, 67, 0x07);

	/* Turn off Q7 */
	wcaxx_setreg(wc, mod, 66, 1);

	/* Flush ProSLIC digital filters by setting to clear, while
	   saving old values */
	for (x = 0; x < 5; x++) {
		tmp[x] = wcaxx_proslic_getreg_indirect(wc, mod, x + 35);
		wcaxx_proslic_setreg_indirect(wc, mod, x + 35, 0x8000);
	}

	/* Power up the DC-DC converter */
	if (wcaxx_powerup_proslic(wc, mod, fast)) {
		dev_notice(&wc->xb.pdev->dev,
			   "Unable to do INITIAL ProSLIC powerup on "
			   "module %d\n", mod->card);
		return -1;
	}

	if (!fast) {
		/* Check for power leaks */
		if (wcaxx_proslic_powerleak_test(wc, mod)) {
			dev_notice(&wc->xb.pdev->dev,
				   "ProSLIC module %d failed leakage test. "
				   "Check for short circuit\n", mod->card);
		}
		/* Power up again */
		if (wcaxx_powerup_proslic(wc, mod, fast)) {
			dev_notice(&wc->xb.pdev->dev,
				   "Unable to do FINAL ProSLIC powerup on "
				   "module %d\n", mod->card);
			return -1;
		}
#ifndef NO_CALIBRATION
		/* Perform calibration */
		if (manual) {
			if (wcaxx_proslic_manual_calibrate(wc, mod)) {
				dev_dbg(&wc->xb.pdev->dev,
					"Proslic failed on Manual Calibration\n");
				if (wcaxx_proslic_manual_calibrate(wc, mod)) {
					dev_notice(&wc->xb.pdev->dev,
						   "Proslic Failed on Second Attempt to Calibrate Manually. (Try -DNO_CALIBRATION in Makefile)\n");
					return -1;
				}
				dev_info(&wc->xb.pdev->dev,
					 "Proslic Passed Manual Calibration on Second Attempt\n");
			}
		} else {
			if (wcaxx_proslic_calibrate(wc, mod))  {
				dev_dbg(&wc->xb.pdev->dev,
					"ProSlic died on Auto Calibration.\n");
				if (wcaxx_proslic_calibrate(wc, mod)) {
					dev_notice(&wc->xb.pdev->dev,
						   "Proslic Failed on Second Attempt to Auto Calibrate\n");
					return -1;
				}
				dev_info(&wc->xb.pdev->dev,
					 "Proslic Passed Auto Calibration on Second Attempt\n");
			}
		}
		/* Perform DC-DC calibration */
		wcaxx_setreg(wc, mod, 93, 0x99);
		r19 = wcaxx_getreg(wc, mod, 107);
		if ((r19 < 0x2) || (r19 > 0xd)) {
			dev_notice(&wc->xb.pdev->dev,
				   "DC-DC cal has a surprising direct 107 of 0x%02x!\n",
				   r19);
			wcaxx_setreg(wc, mod, 107, 0x8);
		}

		/* Save calibration vectors */
		for (x = 0; x < ARRAY_SIZE(addresses); x++)
			addresses[x] = 96 + x;
		wcaxx_getregs(wc, mod, addresses, ARRAY_SIZE(addresses));
		for (x = 0; x < ARRAY_SIZE(fxs->calregs.vals); x++)
			fxs->calregs.vals[x] = addresses[x];
#endif

	} else {
		/* Restore calibration registers */
		for (x = 0; x < ARRAY_SIZE(fxs->calregs.vals); x++)
			wcaxx_setreg(wc, mod, 96 + x, fxs->calregs.vals[x]);
	}
	/* Calibration complete, restore original values */
	for (x = 0; x < 5; x++)
		wcaxx_proslic_setreg_indirect(wc, mod, x + 35, tmp[x]);

	if (wcaxx_proslic_verify_indirect_regs(wc, mod)) {
		dev_info(&wc->xb.pdev->dev, "Indirect Registers failed verification.\n");
		return -1;
	}

	/* U-Law 8-bit interface */
	wcaxx_proslic_set_ts(wc, mod, mod->card);

	wcaxx_setreg(wc, mod, 18, 0xff); /* clear all interrupt */
	wcaxx_setreg(wc, mod, 19, 0xff);
	wcaxx_setreg(wc, mod, 20, 0xff);
	wcaxx_setreg(wc, mod, 22, 0xff);
	wcaxx_setreg(wc, mod, 73, 0x04);

	wcaxx_setreg(wc, mod, 69, 0x4);

	if (fxshonormode) {
		static const int ACIM2TISS[16] = { 0x0, 0x1, 0x4, 0x5, 0x7,
						   0x0, 0x0, 0x6, 0x0, 0x0,
						   0x0, 0x2, 0x0, 0x3 };
		fxsmode = ACIM2TISS[fxo_modes[_opermode].acim];
		wcaxx_setreg(wc, mod, 10, 0x08 | fxsmode);
		if (fxo_modes[_opermode].ring_osc) {
			wcaxx_proslic_setreg_indirect(wc, mod, 20,
					fxo_modes[_opermode].ring_osc);
		}
		if (fxo_modes[_opermode].ring_x) {
			wcaxx_proslic_setreg_indirect(wc, mod, 21,
					fxo_modes[_opermode].ring_x);
		}
	}
	if (lowpower)
		wcaxx_setreg(wc, mod, 72, 0x10);

	if (fastringer) {
		/* Speed up Ringer */
		wcaxx_proslic_setreg_indirect(wc, mod, 20, 0x7e6d);
		wcaxx_proslic_setreg_indirect(wc, mod, 21, 0x01b9);
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wcaxx_setreg(wc, mod, 74, 0x3f);
			if (wcaxx_proslic_setreg_indirect(wc, mod, 21, 0x247))
				return -1;
			dev_info(&wc->xb.pdev->dev,
				 "Boosting fast ringer on slot %d (89V peak)\n",
				 mod->card + 1);
		} else if (lowpower) {
			if (wcaxx_proslic_setreg_indirect(wc, mod, 21, 0x14b))
				return -1;
			dev_info(&wc->xb.pdev->dev,
				 "Reducing fast ring power on slot %d "
				 "(50V peak)\n", mod->card + 1);
		} else
			dev_info(&wc->xb.pdev->dev,
				 "Speeding up ringer on slot %d (25Hz)\n",
				 mod->card + 1);
	} else {
		/* Beef up Ringing voltage to 89V */
		if (boostringer) {
			wcaxx_setreg(wc, mod, 74, 0x3f);
			if (wcaxx_proslic_setreg_indirect(wc, mod, 21, 0x1d1))
				return -1;
			dev_info(&wc->xb.pdev->dev,
				 "Boosting ringer on slot %d (89V peak)\n",
				 mod->card + 1);
		} else if (lowpower) {
			if (wcaxx_proslic_setreg_indirect(wc, mod, 21, 0x108))
				return -1;
			dev_info(&wc->xb.pdev->dev,
				 "Reducing ring power on slot %d "
				 "(50V peak)\n", mod->card + 1);
		}
	}

	if (fxstxgain || fxsrxgain) {
		r9 = wcaxx_getreg(wc, mod, 9);
		switch (fxstxgain) {
		case 35:
			r9 += 8;
			break;
		case -35:
			r9 += 4;
			break;
		case 0:
			break;
		}

		switch (fxsrxgain) {
		case 35:
			r9 += 2;
			break;
		case -35:
			r9 += 1;
			break;
		case 0:
			break;
		}
		wcaxx_setreg(wc, mod, 9, r9);
	}

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "DEBUG: fxstxgain:%s fxsrxgain:%s\n",
			 ((wcaxx_getreg(wc, mod, 9) / 8) == 1) ?
				"3.5" : ((wcaxx_getreg(wc, mod, 9) / 4) == 1) ?
							"-3.5" : "0.0",
			 ((wcaxx_getreg(wc, mod, 9) / 2) == 1) ?
				"3.5" : ((wcaxx_getreg(wc, mod, 9) % 2) ?
							"-3.5" : "0.0"));
	}

	fxs->lasttxhook = fxs->idletxhookstate;
	wcaxx_setreg(wc, mod, LINE_STATE, fxs->lasttxhook);

	/* Preset the shadow register so that we won't get a power alarm when
	 * we finish initialization, otherwise the line state register may not
	 * have been read yet. */
	fxs->linefeed_control_shadow = fxs->lasttxhook;
	return 0;
}

static void wcaxx_get_fxs_regs(struct wcaxx *wc, struct wcaxx_module *mod,
				struct wctdm_regs *regs)
{
	int  x;

	for (x = 0; x < NUM_INDIRECT_REGS; x++)
		regs->indirect[x] = wcaxx_proslic_getreg_indirect(wc, mod, x);

	for (x = 0; x < NUM_REGS; x++)
		regs->direct[x] = wcaxx_getreg(wc, mod, x);
}

static void wcaxx_get_fxo_regs(struct wcaxx *wc, struct wcaxx_module *mod,
				    struct wctdm_regs *regs)
{
	const unsigned int NUM_FXO_REGS = 60;
	int  x;

	for (x = 0; x < NUM_FXO_REGS; x++)
		regs->direct[x] = wcaxx_getreg(wc, mod, x);
}

static int
wcaxx_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct wctdm_stats stats;
	struct wctdm_regop regop;
	struct wctdm_echo_coefs echoregs;
	struct dahdi_hwgain hwgain;
	struct wcaxx *wc = chan->pvt;
	int x;
	struct wcaxx_module *const mod = &wc->mods[chan->chanpos - 1];
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

		if (fxs_lf(fxs, ACTIVE_FWD) || fxs_lf(fxs, ACTIVE_REV)) {
			int res;

			res = set_lasttxhook_interruptible(wc, fxs,
				(POLARITY_XOR(fxs) ?
				SLIC_LF_OHTRAN_REV : SLIC_LF_OHTRAN_FWD),
				&mod->sethook);

			if (debug & DEBUG_CARD) {
				if (res) {
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d TIMEOUT: "
						 "OnHookTransfer start\n",
						 chan->chanpos - 1);
				} else {
					dev_info(&wc->xb.pdev->dev,
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
			stats.tipvolt = wcaxx_getreg(wc, mod, 80) * -376;
			stats.ringvolt = wcaxx_getreg(wc, mod, 81) * -376;
			stats.batvolt = wcaxx_getreg(wc, mod, 82) * -376;
		} else if (mod->type == FXO) {
			stats.tipvolt = (s8)wcaxx_getreg(wc, mod, 29) * 1000;
			stats.ringvolt = (s8)wcaxx_getreg(wc, mod, 29) * 1000;
			stats.batvolt = (s8)wcaxx_getreg(wc, mod, 29) * 1000;
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
			wcaxx_get_fxs_regs(wc, mod, regs);
		else
			wcaxx_get_fxo_regs(wc, mod, regs);

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
			dev_info(&wc->xb.pdev->dev,
				 "Setting indirect %d to 0x%04x on %d\n",
				 regop.reg, regop.val, chan->chanpos);
			wcaxx_proslic_setreg_indirect(wc, mod, regop.reg,
						      regop.val);
		} else {
			regop.val &= 0xff;
			if (regop.reg == LINE_STATE) {
				/* Set feedback register to indicate the new
				 * state that is being set */
				fxs->lasttxhook = (regop.val & 0x0f) |
							SLIC_LF_OPPENDING;
			}
			dev_info(&wc->xb.pdev->dev,
				 "Setting direct %d to %04x on %d\n",
				 regop.reg, regop.val, chan->chanpos);
			wcaxx_setreg(wc, mod, regop.reg, regop.val);
		}
		break;
	case WCTDM_SET_ECHOTUNE:
		dev_info(&wc->xb.pdev->dev, "-- Setting echo registers:\n");
		if (copy_from_user(&echoregs, (__user void *) data,
				   sizeof(echoregs)))
			return -EFAULT;

		if (mod->type == FXO) {
			/* Set the ACIM register */
			wcaxx_setreg(wc, mod, 30, echoregs.acim);

			/* Set the digital echo canceller registers */
			wcaxx_setreg(wc, mod, 45, echoregs.coef1);
			wcaxx_setreg(wc, mod, 46, echoregs.coef2);
			wcaxx_setreg(wc, mod, 47, echoregs.coef3);
			wcaxx_setreg(wc, mod, 48, echoregs.coef4);
			wcaxx_setreg(wc, mod, 49, echoregs.coef5);
			wcaxx_setreg(wc, mod, 50, echoregs.coef6);
			wcaxx_setreg(wc, mod, 51, echoregs.coef7);
			wcaxx_setreg(wc, mod, 52, echoregs.coef8);

			dev_info(&wc->xb.pdev->dev, "-- Set echo registers successfully\n");

			break;
		} else {
			return -EINVAL;

		}
		break;
	case DAHDI_SET_HWGAIN:
		if (copy_from_user(&hwgain, (__user void *) data,
				   sizeof(hwgain)))
			return -EFAULT;

		wcaxx_set_hwgain(wc, mod, hwgain.newgain, hwgain.tx);

		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "Setting hwgain on channel %d to %d for %s direction\n",
				 chan->chanpos-1, hwgain.newgain,
				 ((hwgain.tx) ? "tx" : "rx"));
		}
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
				dev_info(&wc->xb.pdev->dev,
					 "Channel %d Unable to Set Polarity\n",
					 chan->chanpos - 1);
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
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d TIMEOUT: Set Reverse Polarity\n",
						 chan->chanpos - 1);
				} else if (debug & DEBUG_CARD) {
					dev_info(&wc->xb.pdev->dev,
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
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d TIMEOUT: Set Normal Polarity\n",
						 chan->chanpos - 1);
				} else if (debug & DEBUG_CARD) {
					dev_info(&wc->xb.pdev->dev,
						 "Channel %d Set Normal Polarity\n",
						 chan->chanpos - 1);
				}
			}
		}
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static int wcaxx_open(struct dahdi_chan *chan)
{
	struct wcaxx *const wc = chan->pvt;
	unsigned long flags;
	struct wcaxx_module *const mod = &wc->mods[chan->chanpos - 1];

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

static inline struct wcaxx *span_to_wcaxx(struct dahdi_span *span)
{
	struct wcaxx *wc = container_of(span, struct wcaxx, span);
	return wc;
}

static int wcaxx_watchdog(struct dahdi_span *span, int event)
{
	struct wcaxx *wc = span_to_wcaxx(span);
	dev_info(&wc->xb.pdev->dev, "TDM: Called watchdog\n");
	return 0;
}

static int wcaxx_close(struct dahdi_chan *chan)
{
	struct wcaxx *wc;
	int x;

	wc = chan->pvt;
	for (x = 0; x < wc->mods_per_board; x++) {
		struct wcaxx_module *const mod = &wc->mods[x];
		if (FXS == mod->type) {
			mod->mod.fxs.idletxhookstate =
			    POLARITY_XOR(&mod->mod.fxs) ? SLIC_LF_ACTIVE_REV :
							  SLIC_LF_ACTIVE_FWD;
		}
	}

	return 0;
}

static int wcaxx_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct wcaxx *wc = chan->pvt;
	struct wcaxx_module *const mod = &wc->mods[chan->chanpos - 1];

	if (mod->type == FXO) {
		switch (txsig) {
		case DAHDI_TXSIG_START:
		case DAHDI_TXSIG_OFFHOOK:
			mod->mod.fxo.offhook = 1;
			mod->sethook = CMD_WR(5, 0x9);
			/* wcaxx_setreg(wc, chan->chanpos - 1, 5, 0x9); */
			break;
		case DAHDI_TXSIG_ONHOOK:
			mod->mod.fxo.offhook = 0;
			mod->sethook = CMD_WR(5, 0x8);
			/* wcaxx_setreg(wc, chan->chanpos - 1, 5, 0x8); */
			break;
		default:
			dev_notice(&wc->xb.pdev->dev,
				   "Can't set tx state to %d\n", txsig);
			break;
		}
	} else if (mod->type == FXS) {
		wcaxx_fxs_hooksig(wc, mod, txsig);
	}
	return 0;
}

static void wcaxx_dacs_connect(struct wcaxx *wc, int srccard, int dstcard)
{
	struct wcaxx_module *const srcmod = &wc->mods[srccard];
	struct wcaxx_module *const dstmod = &wc->mods[dstcard];
	unsigned int type;

	if (wc->mods[dstcard].dacssrc > -1) {
		dev_notice(&wc->xb.pdev->dev, "wcaxx_dacs_connect: Can't have double sourcing yet!\n");
		return;
	}
	type = wc->mods[srccard].type;
	if ((type == FXS) || (type == FXO)) {
		dev_notice(&wc->xb.pdev->dev,
			   "wcaxx_dacs_connect: Unsupported modtype for "
			   "card %d\n", srccard);
		return;
	}
	type = wc->mods[dstcard].type;
	if ((type != FXS) && (type != FXO)) {
		dev_notice(&wc->xb.pdev->dev,
			   "wcaxx_dacs_connect: Unsupported modtype "
			   "for card %d\n", dstcard);
		return;
	}

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "connect %d => %d\n", srccard, dstcard);
	}

	dstmod->dacssrc = srccard;

	/* make srccard transmit to srccard+24 on the TDM bus */
	if (srcmod->type == FXS) {
		/* proslic */
		wcaxx_setreg(wc, srcmod, PCM_XMIT_START_COUNT_LSB,
			     ((srccard+24) * 8) & 0xff);
		wcaxx_setreg(wc, srcmod, PCM_XMIT_START_COUNT_MSB,
			     ((srccard+24) * 8) >> 8);
	} else if (srcmod->type == FXO) {
		/* daa TX */
		wcaxx_setreg(wc, srcmod, 34, ((srccard+24) * 8) & 0xff);
		wcaxx_setreg(wc, srcmod, 35, ((srccard+24) * 8) >> 8);
	}

	/* have dstcard receive from srccard+24 on the TDM bus */
	if (dstmod->type == FXS) {
		/* proslic */
		wcaxx_setreg(wc, dstmod, PCM_RCV_START_COUNT_LSB,
			     ((srccard+24) * 8) & 0xff);
		wcaxx_setreg(wc, dstmod, PCM_RCV_START_COUNT_MSB,
			     ((srccard+24) * 8) >> 8);
	} else if (dstmod->type == FXO) {
		/* daa RX */
		wcaxx_setreg(wc, dstmod, 36, ((srccard+24) * 8) & 0xff);
		wcaxx_setreg(wc, dstmod, 37, ((srccard+24) * 8) >> 8);
	}
}

static void wcaxx_dacs_disconnect(struct wcaxx *wc, int card)
{
	struct wcaxx_module *const mod = &wc->mods[card];
	struct wcaxx_module *dacssrc;

	if (mod->dacssrc <= -1)
		return;

	dacssrc = &wc->mods[mod->dacssrc];

	if (debug) {
		dev_info(&wc->xb.pdev->dev,
			 "wcaxx_dacs_disconnect: restoring TX for %d and RX for %d\n",
			 mod->dacssrc, card);
	}

	/* restore TX (source card) */
	if (dacssrc->type == FXS) {
		wcaxx_setreg(wc, dacssrc, PCM_XMIT_START_COUNT_LSB,
			     (mod->dacssrc * 8) & 0xff);
		wcaxx_setreg(wc, dacssrc, PCM_XMIT_START_COUNT_MSB,
			     (mod->dacssrc * 8) >> 8);
	} else if (dacssrc->type == FXO) {
		wcaxx_setreg(wc, mod, 34, (card * 8) & 0xff);
		wcaxx_setreg(wc, mod, 35, (card * 8) >> 8);
	} else {
		dev_warn(&wc->xb.pdev->dev,
			 "WARNING: wcaxx_dacs_disconnect() called "
			 "on unsupported modtype\n");
	}

	/* restore RX (this card) */
	if (FXS == mod->type) {
		wcaxx_setreg(wc, mod, PCM_RCV_START_COUNT_LSB,
			     (card * 8) & 0xff);
		wcaxx_setreg(wc, mod, PCM_RCV_START_COUNT_MSB,
			     (card * 8) >> 8);
	} else if (FXO == mod->type) {
		wcaxx_setreg(wc, mod, 36, (card * 8) & 0xff);
		wcaxx_setreg(wc, mod, 37, (card * 8) >> 8);
	} else {
		dev_warn(&wc->xb.pdev->dev,
			 "WARNING: wcaxx_dacs_disconnect() called "
			 "on unsupported modtype\n");
	}

	mod->dacssrc = -1;
}

static int wcaxx_dacs(struct dahdi_chan *dst, struct dahdi_chan *src)
{
	struct wcaxx *wc;

	if (!nativebridge)
		return 0; /* should this return -1 since unsuccessful? */

	wc = dst->pvt;

	if (src) {
		wcaxx_dacs_connect(wc, src->chanpos - 1, dst->chanpos - 1);
		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "dacs connecct: %d -> %d!\n\n",
				 src->chanpos, dst->chanpos);
		}
	} else {
		wcaxx_dacs_disconnect(wc, dst->chanpos - 1);
		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "dacs disconnect: %d!\n", dst->chanpos);
		}
	}
	return 0;
}

/**
 * wcaxx_chanconfig - Called when the channels are being configured.
 *
 * Ensure that the card is completely ready to go before we allow the channels
 * to be completely configured. This is to allow lengthy initialization
 * actions to take place in background on driver load and ensure we're synced
 * up by the time dahdi_cfg is run.
 *
 */
static int
wcaxx_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	struct wcaxx *wc = chan->pvt;
	if ((file->f_flags & O_NONBLOCK) && !is_initialized(wc))
		return -EAGAIN;
	return 0;
}

/*
 * wcaxx_assigned - Called when span is assigned.
 * @span:	The span that is now assigned.
 *
 * This function is called by the core of DAHDI after the span number and
 * channel numbers have been assigned.
 *
 */
static void wcaxx_assigned(struct dahdi_span *span)
{
	struct dahdi_span *s;
	struct dahdi_device *ddev = span->parent;
	struct wcaxx *wc = NULL;

	list_for_each_entry(s, &ddev->spans, device_node) {
		wc = container_of(s, struct wcaxx, span);
		if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &s->flags))
			return;
	}
}

static const struct dahdi_span_ops wcaxx_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = wcaxx_hooksig,
	.open = wcaxx_open,
	.close = wcaxx_close,
	.ioctl = wcaxx_ioctl,
	.watchdog = wcaxx_watchdog,
	.chanconfig = wcaxx_chanconfig,
	.dacs = wcaxx_dacs,
	.assigned = wcaxx_assigned,
#ifdef VPM_SUPPORT
	.echocan_create = wcaxx_echocan_create,
	.echocan_name = wcaxx_echocan_name,
#endif
};

static struct wcaxx_chan *
wcaxx_init_chan(struct wcaxx *wc, struct dahdi_span *s, int channo)
{
	struct wcaxx_chan *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return NULL;

	snprintf(c->chan.name, sizeof(c->chan.name), "WCTDM/%d/%d",
		 wc->num, channo);
	c->chan.chanpos = channo+1;
	c->chan.span = s;
	c->chan.pvt = wc;
	c->timeslot = channo;
	return c;
}

static void wcaxx_init_span(struct wcaxx *wc)
{
	int x;
	struct wcaxx_chan *c;
	struct dahdi_echocan_state *ec[NUM_MODULES] = {NULL, };

	/* DAHDI stuff */
	wc->span.offset = 0;

	sprintf(wc->span.name, "WCTDM/%d", wc->num);

	snprintf(wc->span.desc, sizeof(wc->span.desc) - 1,
		 "%s", wc->desc->name);

	if (wc->companding == DAHDI_LAW_DEFAULT) {
		wc->span.deflaw = DAHDI_LAW_MULAW;
	} else if (wc->companding == DAHDI_LAW_ALAW) {
		/* Force everything to alaw */
		wc->span.deflaw = DAHDI_LAW_ALAW;
	} else {
		/* Auto set to ulaw */
		wc->span.deflaw = DAHDI_LAW_MULAW;
	}

	wc->span.ops = &wcaxx_span_ops;
	wc->span.flags = DAHDI_FLAG_RBS;
	wc->span.spantype = SPANTYPE_ANALOG_MIXED;

	wc->span.chans = kmalloc(sizeof(wc->span.chans[0]) * wc->desc->ports,
			 GFP_KERNEL);
	if (!wc->span.chans)
		return;

	/* allocate channels for the span */
	for (x = 0; x < wc->desc->ports; x++) {
		c = wcaxx_init_chan(wc, &wc->span, x);
		if (!c)
			return;
		wc->chans[x] = c;
		wc->span.chans[x] = &c->chan;

		/* TODO: Should echocan state hide under VPM_ENABLED or does
		 * software ec use it? */
		ec[x] = kzalloc(sizeof(*ec[x]), GFP_KERNEL);
	}

	wc->span.channels = wc->desc->ports;
	memcpy(wc->ec, ec, sizeof(wc->ec));
	memset(ec, 0, sizeof(ec));
}

/**
 * should_set_alaw() - Should be called after all the spans are initialized.
 *
 * Returns true if the module companding should be set to alaw, otherwise
 * false.
 */
static bool should_set_alaw(const struct wcaxx *wc)
{
	if (DAHDI_LAW_ALAW == wc->companding)
		return true;
	else
		return false;
}

static void wcaxx_fixup_span(struct wcaxx *wc)
{
	struct dahdi_span *s;
	int x, y;

	y = 0;
	s = &wc->span;

	for (x = 0; x < wc->desc->ports; x++) {
		struct wcaxx_module *const mod = &wc->mods[x];
		if (debug) {
			dev_info(&wc->xb.pdev->dev,
				 "fixup_analog: x=%d, y=%d modtype=%d, "
				 "s->chans[%d]=%p\n", x, y, mod->type,
				 y, s->chans[y]);
		}
		if (mod->type == FXO) {
			int val;
			s->chans[y++]->sigcap = DAHDI_SIG_FXSKS |
				DAHDI_SIG_FXSLS | DAHDI_SIG_SF |
				DAHDI_SIG_CLEAR;
			val = should_set_alaw(wc) ? 0x20 : 0x28;
#ifdef DEBUG
			val = (digitalloopback) ? 0x30 : val;
#endif
			wcaxx_setreg(wc, mod, 33, val);
			wcaxx_voicedaa_set_ts(wc, mod, wc->chans[x]->timeslot);
		} else if (mod->type == FXS) {

			/* NOTE: Digital loopback does not work on the FXS
			 * modules in the same way since the data is still
			 * companded by the ProSLIC and doesn't appear to have
			 * perfect symetry. */

			s->chans[y++]->sigcap = DAHDI_SIG_FXOKS |
				DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS |
				DAHDI_SIG_SF | DAHDI_SIG_EM | DAHDI_SIG_CLEAR;
			wcaxx_setreg(wc, mod, 1,
				     (should_set_alaw(wc) ? 0x20 : 0x28));
			wcaxx_proslic_set_ts(wc, mod, wc->chans[x]->timeslot);
		} else {
			s->chans[y++]->sigcap = 0;
		}
	}
}

static bool wcaxx_init_fxs_port(struct wcaxx *wc, struct wcaxx_module *mod)
{
	u8 readi;
	enum {UNKNOWN = 0, SANE = 1};
	int ret = wcaxx_init_proslic(wc, mod, 0, 0, UNKNOWN);
	if (!ret) {
		if (debug & DEBUG_CARD) {
			readi = wcaxx_getreg(wc, mod, LOOP_I_LIMIT);
			dev_info(&wc->xb.pdev->dev,
				 "Proslic module %d loop current is %dmA\n",
				 mod->card, ((readi*3) + 20));
		}
		return true;
	}

	if (ret != -2) {
		/* Init with Manual Calibration */
		if (!wcaxx_init_proslic(wc, mod, 0, 1, SANE)) {
			if (debug & DEBUG_CARD) {
				readi = wcaxx_getreg(wc, mod, LOOP_I_LIMIT);
				dev_info(&wc->xb.pdev->dev,
					 "Proslic module %d loop current is %dmA\n",
					 mod->card, ((readi*3)+20));
			}
		} else {
			dev_notice(&wc->xb.pdev->dev,
				   "Port %d: FAILED FXS (%s)\n", mod->card + 1,
				   fxshonormode ? fxo_modes[_opermode].name :
						  "FCC");
		}
		return true;
	}
	return false;
}

static void wcaxx_reset_module(struct wcaxx *wc, struct wcaxx_module *mod)
{

	u32 reg_val = (1UL << (mod->spi->chip_select + 12));
	wcxb_gpio_clear(&wc->xb, reg_val);
	udelay(500);
	wcxb_gpio_set(&wc->xb, reg_val);
	msleep(250); /* TODO: What should this value be? */
}

static bool check_for_single_fxs(struct wcaxx *wc, unsigned int port)
{
	bool result;
	struct wcaxx_module *mod = &wc->mods[port];

	mod->spi = get_spi_device_for_port(wc, mod->card, false);
	mod->subaddr = 0;
	wcaxx_reset_module(wc, mod);
	wcaxx_fxsinit(mod->spi);
	result = wcaxx_init_fxs_port(wc, mod);
	if (!result)
		mod->type = NONE;

	/* It is currently unclear why this read is necessary for some of the
	 * S100M modules to properly function. */
	wcaxx_getreg(wc, mod, 0x00);
	return result;
}

static bool check_for_single_fxo(struct wcaxx *wc, unsigned int port)
{
	bool result;
	struct wcaxx_module *mod = &wc->mods[port];
	mod->spi = get_spi_device_for_port(wc, mod->card, false);
	mod->subaddr = 0;
	wcaxx_reset_module(wc, mod);
	result = (wcaxx_init_voicedaa(wc, mod, 0, 0, 0) == 0);
	if (!result)
		mod->type = NONE;
	return result;
}

static bool check_for_quad_fxs(struct wcaxx *wc, unsigned int base_port)
{
	int port;
	int offset;
	struct wcaxx_module *mod = &wc->mods[base_port + 1];

	/* Cannot have quad port modules on the 4 port base cards. */
	if (is_four_port(wc))
		return false;

	/* We can assume that the base port has already been configured as an
	 * FXS port if we're even in this function */
	mod->spi = get_spi_device_for_port(wc, mod->card, true);
	mod->subaddr = offset = 1;
	if (wcaxx_init_fxs_port(wc, mod)) {
		/* This must be a 4 port FXS module...  */
		for (port = base_port + 2; port < base_port+4; ++port) {
			mod = &wc->mods[port];
			mod->spi = get_spi_device_for_port(wc, mod->card, true);
			mod->subaddr = ++offset;
			if (!wcaxx_init_fxs_port(wc, mod)) {
				/* This means that a quad-module failed to
				 * setup ports 3 or 4? */
				dev_err(&wc->xb.pdev->dev,
					"Quad-FXS at base %d failed initialization.\n",
					base_port);
				goto error_exit;
			}
		}
		return true;
	}
error_exit:
	for (port = base_port + 1; port < base_port + 4; ++port) {
		mod = &wc->mods[port];
		mod->type = NONE;
	}
	return false;
}

static bool check_for_quad_fxo(struct wcaxx *wc, unsigned int base_port)
{
	int port;
	int offset;
	struct wcaxx_module *mod = &wc->mods[base_port + 1];

	/* Cannot have quad port modules on the 4 port base cards. */
	if (is_four_port(wc))
		return false;

	/* We can assume that the base port has already been configured as an
	 * FXO port if we're even in this function */
	mod->spi = get_spi_device_for_port(wc, mod->card, true);
	mod->subaddr = offset = 1;
	if (!wcaxx_init_voicedaa(wc, mod, 0, 0, 0)) {
		/* This must be a 4 port FXO module. */
		for (port = base_port + 2; port < base_port + 4; ++port) {
			mod = &wc->mods[port];
			mod->spi = get_spi_device_for_port(wc, mod->card, true);
			mod->subaddr = ++offset;
			if (wcaxx_init_voicedaa(wc, mod, 0, 0, 0)) {
				dev_err(&wc->xb.pdev->dev,
					"Quad-FXO at base %d failed initialization.\n",
					base_port);
				goto error_exit;
			}
		}
		return true;
	}
error_exit:
	for (port = base_port + 1; port < base_port + 4; ++port) {
		mod = &wc->mods[port];
		mod->type = NONE;
	}
	return false;
}

static void __wcaxx_identify_four_port_module_group(struct wcaxx *wc)
{
	int i;
	for (i = 0; i < wc->desc->ports; i++) {
		if (!check_for_single_fxs(wc, i))
			check_for_single_fxo(wc, i);
	}
	return;
}

static void
__wcaxx_identify_module_group(struct wcaxx *wc, unsigned long base)
{
	if (check_for_single_fxs(wc, base)) {
		if (check_for_quad_fxs(wc, base)) {
			/* S400M installed */
			return;
		} else if (check_for_single_fxs(wc, base + 1)) {
			/* Two S110M installed */
			return;
		} else if (check_for_single_fxo(wc, base + 1)) {
			/* 1 S110M 1 X100M */
			return;
		} else {
			/* 1 S110M 1 Empty */
			return;
		}
	} else if (check_for_single_fxo(wc, base)) {
		if (check_for_quad_fxo(wc, base)) {
			/* X400M installed */
			return;
		} else if (check_for_single_fxo(wc, base + 1)) {
			/* Two X100M installed */
			return;
		} else if (check_for_single_fxs(wc, base + 1)) {
			/* 1 X100M 1 S100M installed */
			return;
		} else {
			/* 1 X100M 1 Empty */
			return;
		}
	} else if (check_for_single_fxs(wc, base + 1)) {
		/* 1 Empty 1 S110M installed */
		return;
	} else if (check_for_single_fxo(wc, base + 1)) {
		/* 1 Empty 1 X100M installed */
		return;
	}
	/* No module */
	return;
}

/**
 * wcaxx_print_moule_configuration - Print the configuration to the kernel log
 * @wc:		The card we're interested in.
 *
 * This is to ensure that the module configuration from each card shows up
 * sequentially in the kernel log, as opposed to interleaved with one another.
 *
 */
static void wcaxx_print_module_configuration(const struct wcaxx *const wc)
{
	int i;
	static DEFINE_MUTEX(print);

	mutex_lock(&print);
	for (i = 0; i < wc->mods_per_board; ++i) {
		const struct wcaxx_module *const mod = &wc->mods[i];

		switch (mod->type) {
		case FXO:
			dev_info(&wc->xb.pdev->dev,
				 "Port %d: Installed -- AUTO FXO (%s mode)\n",
				 i + 1, fxo_modes[_opermode].name);
			break;
		case FXS:
			dev_info(&wc->xb.pdev->dev,
				 "Port %d: Installed -- AUTO FXS/DPO\n", i + 1);
			break;
		case NONE:
			dev_info(&wc->xb.pdev->dev,
				 "Port %d: Not installed\n", i + 1);
			break;
		}
	}
	mutex_unlock(&print);
}

static void wcaxx_identify_modules(struct wcaxx *wc)
{
	int x;
	unsigned long flags;

	/* A8A/A8B - Reset the modules. */
	wcxb_gpio_clear(&wc->xb, 0xf000);
	msleep(50); /* TODO: what should these values be? */
	wcxb_gpio_set(&wc->xb, 0xf000);
	msleep(250); /* TODO: What should these values be? */

	/* Place all units in the daisy chain mode of operation. This allows
	 * multiple devices to share a chip select (like on the X400 and S400
	 * modules) */
	for (x = 0; x < ARRAY_SIZE(wc->spi_devices); ++x)
		wcaxx_fxsinit(wc->spi_devices[x]);

	spin_lock_irqsave(&wc->reglock, flags);
	wc->mods_per_board = wc->desc->ports;
	spin_unlock_irqrestore(&wc->reglock, flags);

	BUG_ON(wc->desc->ports % 4);

	if (is_four_port(wc)) {
		__wcaxx_identify_four_port_module_group(wc);
	} else {
		for (x = 0; x < wc->desc->ports/4; x++)
			__wcaxx_identify_module_group(wc, x*4);
	}

	wcaxx_print_module_configuration(wc);
}

static struct pci_driver wcaxx_driver;

static void wcaxx_back_out_gracefully(struct wcaxx *wc)
{
	int i;
	unsigned long flags;

	clear_bit(INITIALIZED, &wc->bit_flags);
	smp_mb__after_clear_bit();

	/* Make sure we're not on the card list anymore. */
	mutex_lock(&card_list_lock);
	list_del(&wc->card_node);
	mutex_unlock(&card_list_lock);

	wcxb_release(&wc->xb);

	for (i = 0; i < wc->mods_per_board; i++) {
		struct wcaxx_module *const mod = &wc->mods[i];
		kfree(mod->mod_poll);
		mod->mod_poll = NULL;
	}

	kfree(wc->span.chans);
	wc->span.chans = NULL;

	spin_lock_irqsave(&wc->reglock, flags);
	for (i = 0; i < wc->span.channels; ++i) {
		kfree(wc->chans[i]);
		kfree(wc->ec[i]);
		wc->chans[i] = NULL;
		wc->ec[i] = NULL;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	for (i = 0; i < ARRAY_SIZE(wc->spi_devices); i++)
		wcxb_spi_device_destroy(wc->spi_devices[i]);
	wcxb_spi_master_destroy(wc->master);

	kfree(wc->board_name);
	if (wc->ddev) {
		kfree(wc->ddev->devicetype);
		kfree(wc->ddev->location);
		kfree(wc->ddev->hardware_id);
		dahdi_free_device(wc->ddev);
	}
	kfree(wc);
}

static const struct wcxb_operations wcxb_operations = {
	.handle_receive = wcaxx_handle_receive,
	.handle_transmit = wcaxx_handle_transmit,
};

struct cmd_results {
	u8 results[8];
};

static int wcaxx_check_firmware(struct wcaxx *wc)
{
	char *filename;
	u32 firmware_version;
	const bool force_firmware = false;
	const unsigned int A4A_VERSION = 0x0a0017;
	const unsigned int A4B_VERSION = 0x0b0017;
	const unsigned int A8A_VERSION = 0x1d0017;
	const unsigned int A8B_VERSION = 0x1d0017;

	if (wc->desc == &device_a8a) {
		firmware_version = A8A_VERSION;
		filename = "dahdi-fw-a8a.bin";
	} else if (wc->desc == &device_a8b) {
		firmware_version = A8B_VERSION;
		filename = "dahdi-fw-a8b.bin";
	} else if (wc->desc == &device_a4a) {
		firmware_version = A4A_VERSION;
		filename = "dahdi-fw-a4a.bin";
	} else if (wc->desc == &device_a4b) {
		firmware_version = A4B_VERSION;
		filename = "dahdi-fw-a4b.bin";
	} else {
		/* This is a bug in the driver code */
		WARN_ON(1);
		return 0;
	}

	return wcxb_check_firmware(&wc->xb, firmware_version,
				   filename, force_firmware);
}

static void wcaxx_check_sethook(struct wcaxx *wc, struct wcaxx_module *mod)
{
	if (mod->sethook) {
		wcaxx_setreg(wc, mod, ((mod->sethook >> 8) & 0xff),
				  mod->sethook & 0xff);
		mod->sethook = 0;
	}
}

static void wcaxx_poll_fxs_complete(void *arg)
{
	struct wcaxx_mod_poll *poll_fxs = arg;
	struct wcaxx *wc = poll_fxs->wc;
	struct wcaxx_module *const mod = poll_fxs->mod;

	if (!is_initialized(wc)) {
		kfree(poll_fxs);
		return;
	}

	mod->mod.fxs.hook_state_shadow		= poll_fxs->buffer[2];
	mod->mod.fxs.linefeed_control_shadow	= poll_fxs->buffer[5];
	wcaxx_isr_misc_fxs(poll_fxs->wc, poll_fxs->mod);
	memcpy(poll_fxs->buffer, poll_fxs->master_buffer,
	       sizeof(poll_fxs->buffer));
	wcaxx_check_sethook(poll_fxs->wc, poll_fxs->mod);
	mod->mod_poll = poll_fxs;
}

/**
 * wcaxx_start_poll_fxs - Starts the interrupt polling loop for FXS modules.
 *
 * To stop the polling loop, clear the initialized bit and then flush the
 * pending wcxb_spi messages.
 *
 */
static int wcaxx_start_poll_fxs(struct wcaxx *wc, struct wcaxx_module *mod)
{
	struct wcaxx_mod_poll *mod_poll = kzalloc(sizeof(*mod_poll),
						    GFP_KERNEL);
	struct wcxb_spi_message	*m = &mod_poll->m;
	struct wcxb_spi_transfer *t = &mod_poll->t;

	WARN_ON(!is_initialized(wc));
	if (!mod_poll)
		return -ENOMEM;

	memset(t, 0, sizeof(*t));
	wcxb_spi_message_init(m);

	t->tx_buf = t->rx_buf = mod_poll->buffer;
	t->len = sizeof(mod_poll->buffer);
	wcxb_spi_message_add_tail(t, m);
	mod_poll->wc = wc;
	mod_poll->mod = mod;

	mod_poll->master_buffer[0] = 1 << mod_poll->mod->subaddr;
	mod_poll->master_buffer[1] = (LOOP_STAT | 0x80) & 0xff;
	mod_poll->master_buffer[2] = 0;

	mod_poll->master_buffer[3] = mod_poll->master_buffer[0];
	mod_poll->master_buffer[4] = (LINE_STATE | 0x80) & 0xff;
	mod_poll->master_buffer[5] = 0;

	memcpy(mod_poll->buffer, mod_poll->master_buffer,
	       sizeof(mod_poll->buffer));

	m->arg = mod_poll;
	m->complete = &wcaxx_poll_fxs_complete;
	wcxb_spi_async(mod->spi, m);
	return 0;
}

static void wcaxx_poll_fxo_complete(void *arg)
{
	struct wcaxx_mod_poll *poll_fxo = arg;
	struct wcaxx *wc = poll_fxo->wc;
	struct wcaxx_module *const mod = poll_fxo->mod;

	if (!is_initialized(wc)) {
		kfree(poll_fxo);
		return;
	}

	mod->mod.fxo.hook_ring_shadow		= poll_fxo->buffer[2];
	mod->mod.fxo.line_voltage_status	= poll_fxo->buffer[5];
	wcaxx_voicedaa_check_hook(poll_fxo->wc, poll_fxo->mod);
	memcpy(poll_fxo->buffer, poll_fxo->master_buffer,
	       sizeof(poll_fxo->buffer));
	wcaxx_check_sethook(poll_fxo->wc, poll_fxo->mod);
	mod->mod_poll = poll_fxo;
}

/**
 * wcaxx_start_poll_fxo - Starts the interrupt polling loop for FXS modules.
 *
 * To stop the polling loop, clear the initialized bit and then flush the
 * pending wcxb_spi messages.
 *
 */
static int wcaxx_start_poll_fxo(struct wcaxx *wc, struct wcaxx_module *mod)
{
	static const int ADDRS[4] = {0x00, 0x08, 0x04, 0x0c};
	struct wcaxx_mod_poll *poll_fxo = kzalloc(sizeof(*poll_fxo),
						    GFP_KERNEL);
	struct wcxb_spi_message	*m = &poll_fxo->m;
	struct wcxb_spi_transfer *t = &poll_fxo->t;

	WARN_ON(!is_initialized(wc));
	if (!poll_fxo)
		return -ENOMEM;

	memset(t, 0, sizeof(*t));
	wcxb_spi_message_init(m);

	t->tx_buf = t->rx_buf = poll_fxo->buffer;
	t->len = sizeof(poll_fxo->buffer);
	wcxb_spi_message_add_tail(t, m);
	poll_fxo->wc = wc;
	poll_fxo->mod = mod;

	poll_fxo->master_buffer[0] = 0x60 | ADDRS[poll_fxo->mod->subaddr];
	poll_fxo->master_buffer[1] = 5 & 0x7f; /* Hook / Ring State */
	poll_fxo->master_buffer[2] = 0;

	poll_fxo->master_buffer[3] = poll_fxo->master_buffer[0];
	poll_fxo->master_buffer[4] = 29 & 0x7f; /* Battery */
	poll_fxo->master_buffer[5] = 0;

	memcpy(poll_fxo->buffer, poll_fxo->master_buffer,
	       sizeof(poll_fxo->buffer));

	m->arg = poll_fxo;
	m->complete = &wcaxx_poll_fxo_complete;
	wcxb_spi_async(mod->spi, m);
	return 0;
}

/**
 * wcaxx_read_serial - Returns the serial number of the board.
 * @wc: The board whos serial number we are reading.
 *
 * The buffer returned is dynamically allocated and must be kfree'd by the
 * caller. If memory could not be allocated, NULL is returned.
 *
 * Must be called in process context.
 *
 */
static char *wcaxx_read_serial(struct wcaxx *wc)
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

static void wcaxx_start_module_polling(struct wcaxx *wc)
{
	int x;
	WARN_ON(!is_initialized(wc));
	for (x = 0; x < wc->mods_per_board; x++) {
		struct wcaxx_module *const mod = &wc->mods[x];
		switch (mod->type) {
		case FXO:
			wcaxx_start_poll_fxo(wc, mod);
			break;
		case FXS:
			wcaxx_start_poll_fxs(wc, mod);
			break;
		case NONE:
			break;
		}
	}
	wc->module_poll_time = wc->framecount + MODULE_POLL_TIME_MS;
}

/**
 * t43x_assign_num - Assign wc->num a unique value and place on card_list
 *
 */
static void wcaxx_assign_num(struct wcaxx *wc)
{
	mutex_lock(&card_list_lock);
	if (list_empty(&card_list)) {
		wc->num = 0;
		list_add(&wc->card_node, &card_list);
	} else {
		struct wcaxx *cur;
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

#ifdef USE_ASYNC_INIT
struct async_data {
	struct pci_dev *pdev;
	const struct pci_device_id *ent;
};
static int __devinit
__wcaxx_init_one(struct pci_dev *pdev, const struct pci_device_id *ent,
		 async_cookie_t cookie)
#else
static int __devinit
__wcaxx_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
#endif
{
	struct wcaxx *wc;
	int i, ret;
	int curchan;

	neonmwi_offlimit_cycles = neonmwi_offlimit / MS_PER_HOOKCHECK;

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	wcaxx_assign_num(wc);

	wc->desc = (struct _device_desc *)ent->driver_data;

	spin_lock_init(&wc->reglock);

	wc->board_name = kasprintf(GFP_KERNEL, "%s%d",
				   wcaxx_driver.name, wc->num);
	if (!wc->board_name) {
		wcaxx_back_out_gracefully(wc);
		return -ENOMEM;
	}

#ifdef CONFIG_VOICEBUS_DISABLE_ASPM
	if (is_pcie(wc)) {
		pci_disable_link_state(pdev->bus->self, PCIE_LINK_STATE_L0S |
			PCIE_LINK_STATE_L1 | PCIE_LINK_STATE_CLKPM);
	};
#endif

	pci_set_drvdata(pdev, wc);
	wc->xb.ops = &wcxb_operations;
	wc->xb.pdev = pdev;
	wc->xb.debug = &debug;

	ret = wcxb_init(&wc->xb, wc->board_name, int_mode);
	if (ret) {
		wcaxx_back_out_gracefully(wc);
		return ret;
	}

	wcxb_set_minlatency(&wc->xb, latency);
	wcxb_set_maxlatency(&wc->xb, max_latency);

	ret = wcaxx_check_firmware(wc);
	if (ret) {
		wcaxx_back_out_gracefully(wc);
		return ret;
	}

	wcxb_lock_latency(&wc->xb);

	wc->mods_per_board = NUM_MODULES;

	if (alawoverride) {
		companding = "alaw";
		dev_info(&wc->xb.pdev->dev,
			"The module parameter alawoverride has been deprecated. Please use the parameter companding=alaw instead");
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

	wc->master = wcxb_spi_master_create(&pdev->dev,
					    wc->xb.membase + 0x280, true);
	for (i = 0; i < ARRAY_SIZE(wc->spi_devices); i++)
		wc->spi_devices[i] = wcxb_spi_device_create(wc->master, 3-i);

	for (i = 0; i < ARRAY_SIZE(wc->mods); i++) {
		struct wcaxx_module *const mod = &wc->mods[i];
		mod->dacssrc = -1;
		mod->card = i;
		mod->spi = NULL;
		mod->subaddr = 0;
		mod->type = NONE;
	}

	ret = wcaxx_vpm_init(wc);
	if (!ret)
		wcxb_enable_echocan(&wc->xb);

	/* Now track down what modules are installed */
	wcaxx_identify_modules(wc);

	/* Start the hardware processing. */
	if (wcxb_start(&wc->xb)) {
		WARN_ON(1);
		return -EIO;
	}

	if (fatal_signal_pending(current)) {
		wcaxx_back_out_gracefully(wc);
		return -EINTR;
	}

	curchan = 0;
	wcaxx_init_span(wc);
	wcaxx_fixup_span(wc);
	curchan += wc->desc->ports;

#ifdef USE_ASYNC_INIT
	async_synchronize_cookie(cookie);
#endif
	wc->ddev = dahdi_create_device();
	if (!wc->ddev) {
		wcaxx_back_out_gracefully(wc);
		return -ENOMEM;
	}
	wc->ddev->manufacturer = "Digium";
	wc->ddev->location = kasprintf(GFP_KERNEL, "PCI Bus %02d Slot %02d",
				       pdev->bus->number,
				       PCI_SLOT(pdev->devfn) + 1);
	if (!wc->ddev->location) {
		wcaxx_back_out_gracefully(wc);
		return -ENOMEM;
	}

	wc->ddev->devicetype = kasprintf(GFP_KERNEL, "%s", wc->desc->name);

	if (!wc->ddev->devicetype) {
		wcaxx_back_out_gracefully(wc);
		return -ENOMEM;
	}

	wc->ddev->hardware_id = wcaxx_read_serial(wc);

	list_add_tail(&wc->span.device_node, &wc->ddev->spans);

	if (dahdi_register_device(wc->ddev, &wc->xb.pdev->dev)) {
		dev_notice(&wc->xb.pdev->dev, "Unable to register device with DAHDI\n");
		wcaxx_back_out_gracefully(wc);
		return -1;
	}

	dev_info(&wc->xb.pdev->dev, "Found a %s (SN: %s)\n",
		 wc->desc->name, wc->ddev->hardware_id);

	set_bit(INITIALIZED, &wc->bit_flags);
	wcaxx_start_module_polling(wc);
	wcxb_unlock_latency(&wc->xb);
	return 0;
}

#ifdef USE_ASYNC_INIT
static __devinit void
wcaxx_init_one_async(void *data, async_cookie_t cookie)
{
	struct async_data *dat = data;
	__wcaxx_init_one(dat->pdev, dat->ent, cookie);
	kfree(dat);
}

static int __devinit
wcaxx_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct async_data *dat;

	dat = kmalloc(sizeof(*dat), GFP_KERNEL);
	/* If we can't allocate the memory for the async_data, odds are we won't
	 * be able to initialize the device either, but let's try synchronously
	 * anyway... */
	if (!dat)
		return __wcaxx_init_one(pdev, ent, 0);

	dat->pdev = pdev;
	dat->ent = ent;
	async_schedule(wcaxx_init_one_async, dat);
	return 0;
}
#else
static int __devinit
wcaxx_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return __wcaxx_init_one(pdev, ent);
}
#endif

static void wcaxx_release(struct wcaxx *wc)
{
	if (is_initialized(wc))
		dahdi_unregister_device(wc->ddev);

	wcaxx_back_out_gracefully(wc);
}

static void __devexit wcaxx_remove_one(struct pci_dev *pdev)
{
	struct wcaxx *wc = pci_get_drvdata(pdev);

	if (!wc)
		return;

	dev_info(&wc->xb.pdev->dev, "Removing a %s.\n", wc->desc->name);

	flush_scheduled_work();
	wcxb_stop(&wc->xb);

#ifdef VPM_SUPPORT
	if (wc->vpm)
		release_vpm450m(wc->vpm);
	wc->vpm = NULL;
#endif

	wcaxx_release(wc);
}

static DEFINE_PCI_DEVICE_TABLE(wcaxx_pci_tbl) = {
	{ 0xd161, 0x800d, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		(unsigned long) &device_a8b
	},
	{ 0xd161, 0x800c, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		(unsigned long) &device_a8a
	},
	{ 0xd161, 0x8010, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		(unsigned long) &device_a4b
	},
	{ 0xd161, 0x800f, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		(unsigned long) &device_a4a
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, wcaxx_pci_tbl);

static void wcaxx_shutdown(struct pci_dev *pdev)
{
	struct wcaxx *wc = pci_get_drvdata(pdev);
	wcxb_stop(&wc->xb);
}

static int wcaxx_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return -ENOSYS;
}

static struct pci_driver wcaxx_driver = {
	.name = "wcaxx",
	.probe = wcaxx_init_one,
	.remove = __devexit_p(wcaxx_remove_one),
	.shutdown = wcaxx_shutdown,
	.suspend = wcaxx_suspend,
	.id_table = wcaxx_pci_tbl,
};

static int __init wcaxx_init(void)
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
		pr_notice("Invalid/unknown operating mode '%s' specified. Please choose one of:\n",
			  opermode);
		for (x = 0; x < ARRAY_SIZE(fxo_modes); x++)
			pr_notice("  %s\n", fxo_modes[x].name);
		pr_notice("Note this option is CASE SENSITIVE!\n");
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

	/* for the voicedaa_check_hook defaults, if the user has not
	 * overridden them by specifying them as module parameters, then get
	 * the values from the selected operating mode */
	if (!battdebounce)
		battdebounce = fxo_modes[_opermode].battdebounce;
	if (!battalarm)
		battalarm = fxo_modes[_opermode].battalarm;
	if (!battthresh)
		battthresh = fxo_modes[_opermode].battthresh;

	res = dahdi_pci_module(&wcaxx_driver);
	if (res)
		return -ENODEV;

#ifdef USE_ASYNC_INIT
	async_synchronize_full();
#endif
	return 0;
}

static void __exit wcaxx_cleanup(void)
{
	pci_unregister_driver(&wcaxx_driver);
}


module_param(debug, int, 0600);
module_param(int_mode, int, 0400);
MODULE_PARM_DESC(int_mode,
	"0 = Use MSI interrupt if available. 1 = Legacy interrupt only.\n");
module_param(fastpickup, int, 0400);
MODULE_PARM_DESC(fastpickup,
	"Set to 1 to shorten the calibration delay when taking an FXO port off "
	"hook. This can be required for Type-II CID. If -1 the calibration "
	"delay will depend on the current opermode.\n");
module_param(fxovoltage, int, 0600);
module_param(loopcurrent, int, 0600);
module_param(reversepolarity, int, 0600);
#ifdef DEBUG
module_param(robust, int, 0600);
module_param(digitalloopback, int, 0400);
MODULE_PARM_DESC(digitalloopback,
	"Set to 1 to place FXO modules into loopback mode for troubleshooting.");
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
#endif

module_param(forceload, int, 0600);
MODULE_PARM_DESC(forceload,
	"Set to 1 in order to force an FPGA reload after power on.");

module_param(companding, charp, 0400);
MODULE_PARM_DESC(companding,
	"Change the companding to \"auto\" or \"alaw\" or \"ulaw\". Auto "
	"(default) will set everything to ulaw unless a BRI module is "
	"installed. It will use alaw in that case.");

MODULE_DESCRIPTION("A4A,A4B,A8A,A8B Driver for Analog Telephony Cards");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(wcaxx_init);
module_exit(wcaxx_cleanup);
