/*
 * Copyright (c) 2005, Adaptive Digital Technologies, Inc.
 * Copyright (c) 2005-2010, Digium Incorporated
 *
 * File Name: GpakCust.c
 *
 * Description:
 *   This file contains host system dependent functions to support generic
 *   G.PAK API functions. The file is integrated into the host processor
 *   connected to C55x G.PAK DSPs via a Host Port Interface.
 *
 *   Note: This file is supplied by Adaptive Digital Technologies and
 *   modified by Digium in order to support the VPMADT032 modules.
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

#include <linux/version.h>
#include <linux/types.h>
#include <linux/pci.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#include <dahdi/kernel.h>
#include <dahdi/user.h>

#include "GpakCust.h"
#include "GpakApi.h"

#include "adt_lec.h"
#include "voicebus.h"
#include "vpmadtreg.h"

static DEFINE_SPINLOCK(ifacelock);
static struct vpmadt032 *ifaces[MAX_DSP_CORES];

#define vpm_info(vpm, format, arg...)         \
	dev_info(&vpm->vb->pdev->dev , format , ## arg)

static inline struct vpmadt032 *find_iface(const unsigned short dspid)
{
	struct vpmadt032 *ret;

	spin_lock(&ifacelock);
	if (ifaces[dspid]) {
		ret = ifaces[dspid];
	} else {
		ret = NULL;
	}
	spin_unlock(&ifacelock);
	return ret;
}

static struct vpmadt032_cmd *vpmadt032_get_free_cmd(struct vpmadt032 *vpm)
{
	struct vpmadt032_cmd *cmd;
	might_sleep();
	cmd = kmalloc(sizeof(struct vpmadt032_cmd), GFP_KERNEL);
	if (unlikely(!cmd))
		return NULL;
	memset(cmd, 0, sizeof(*cmd));
	init_completion(&cmd->complete);
	return cmd;
}

/* Wait for any outstanding commands to the VPMADT032 to complete */
static inline int vpmadt032_io_wait(struct vpmadt032 *vpm)
{
	unsigned long flags;
	int empty;
	while (1) {
		spin_lock_irqsave(&vpm->list_lock, flags);
		empty = list_empty(&vpm->pending_cmds) && list_empty(&vpm->active_cmds);
		spin_unlock_irqrestore(&vpm->list_lock, flags);
		if (empty) {
			break;
		} else {
			msleep(1);
		}
	}
	return 0;
}

/* Issue a read command to a register on the VPMADT032. We'll get the results
 * later. */
static struct vpmadt032_cmd *vpmadt032_getreg_full_async(struct vpmadt032 *vpm, int pagechange,
	unsigned short addr)
{
	unsigned long flags;
	struct vpmadt032_cmd *cmd;
	cmd = vpmadt032_get_free_cmd(vpm);
	if (!cmd)
		return NULL;
	cmd->desc = (pagechange) ? __VPM150M_RWPAGE | __VPM150M_RD : __VPM150M_RD;
	cmd->address = addr;
	cmd->data = 0;
	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_cmds);
	spin_unlock_irqrestore(&vpm->list_lock, flags);
	return cmd;
}

/* Get the results from a previous call to vpmadt032_getreg_full_async. */
static int vpmadt032_getreg_full_return(struct vpmadt032 *vpm, int pagechange,
	u16 addr, u16 *outbuf, struct vpmadt032_cmd *cmd)
{
	unsigned long flags;
	unsigned long ret;
	BUG_ON(!cmd);

	/* We'll wait for 2s */
	ret = wait_for_completion_timeout(&cmd->complete, HZ*2);
	if (unlikely(!ret)) {
		spin_lock_irqsave(&vpm->list_lock, flags);
		list_del(&cmd->node);
		spin_unlock_irqrestore(&vpm->list_lock, flags);
		kfree(cmd);
		return -EIO;
	}

	if (cmd->desc & __VPM150M_FIN) {
		*outbuf = cmd->data;
		cmd->desc = 0;
		ret = 0;
	}

	list_del(&cmd->node);
	kfree(cmd);
	return 0;
}

/* Read one of the registers on the VPMADT032 */
static int vpmadt032_getreg_full(struct vpmadt032 *vpm, int pagechange, u16 addr, u16 *outbuf)
{
	struct vpmadt032_cmd *cmd;
	cmd = vpmadt032_getreg_full_async(vpm, pagechange, addr);
	if (unlikely(!cmd)) {
		return -ENOMEM;
	}
	return vpmadt032_getreg_full_return(vpm, pagechange, addr, outbuf, cmd);
}

static int vpmadt032_setreg_full(struct vpmadt032 *vpm, int pagechange, unsigned int addr,
                        u16 data)
{
	unsigned long flags;
	struct vpmadt032_cmd *cmd;
	cmd = vpmadt032_get_free_cmd(vpm);
	if (!cmd)
		return -ENOMEM;
	cmd->desc = cpu_to_le16((pagechange) ? (__VPM150M_WR|__VPM150M_RWPAGE) : __VPM150M_WR);
	cmd->address = cpu_to_le16(addr);
	cmd->data = cpu_to_le16(data);
	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_cmds);
	spin_unlock_irqrestore(&vpm->list_lock, flags);
	return 0;
}


static int vpmadt032_setpage(struct vpmadt032 *vpm, u16 addr)
{
	addr &= 0xf;
	/* We do not need to set the page if we're already on the page we're
	 * interested in. */
	if (vpm->curpage == addr)
		return 0;
	else
		vpm->curpage = addr;

	return vpmadt032_setreg_full(vpm, 1, 0, addr);
}

static unsigned char vpmadt032_getpage(struct vpmadt032 *vpm)
{
	unsigned short res;
	const int pagechange = 1;
	vpmadt032_getreg_full(vpm, pagechange, 0, &res);
	return res;
}

static int vpmadt032_getreg(struct vpmadt032 *vpm, unsigned int addr, u16 *data)
{
	unsigned short res;
	vpmadt032_setpage(vpm, addr >> 16);
	res = vpmadt032_getreg_full(vpm, 0, addr & 0xffff, data);
 	return res;
}

static int vpmadt032_setreg(struct vpmadt032 *vpm, unsigned int addr, u16 data)
{
	int res;
	vpmadt032_setpage(vpm, addr >> 16);
	res = vpmadt032_setreg_full(vpm, 0, addr & 0xffff, data);
	return res;
}

struct change_order {
	struct list_head node;
	unsigned int channel;
	struct adt_lec_params params;
};

static struct change_order *alloc_change_order(void)
{
	return kzalloc(sizeof(struct change_order), GFP_ATOMIC);
}

static void free_change_order(struct change_order *order)
{
	kfree(order);
}

static int vpmadt032_control(struct vpmadt032 *vpm, unsigned short int channel,
			     GpakAlgCtrl_t control_code,
			     GPAK_AlgControlStat_t *pstatus)
{
	gpakAlgControlStat_t stat;
	int retry = 4;
	while (retry--) {
		stat = gpakAlgControl(vpm->dspid, channel,
				      control_code, pstatus);

		if (AcDspCommFailure == stat)
			msleep(5);
		else
			break;
	}

	return stat;
}

static int vpmadt032_enable_ec(struct vpmadt032 *vpm, int channel,
			       enum adt_companding companding)
{
	int res;
	GPAK_AlgControlStat_t pstatus;
	GpakAlgCtrl_t control;

	control = (ADT_COMP_ALAW == companding) ? EnableALawSwCompanding :
						  EnableMuLawSwCompanding;
 
	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN) {
		const char *law;
		law = (control == EnableMuLawSwCompanding) ? "MuLaw" : "ALaw";
		vpm_info(vpm, "Enabling ecan on channel: %d (%s)\n",
			 channel, law);
	}
	res = vpmadt032_control(vpm, channel, control, &pstatus);
	if (res) {
		vpm_info(vpm, "Unable to set SW Companding on "
			 "channel %d (reason %d)\n", channel, res);
	}
	res = vpmadt032_control(vpm, channel, EnableEcanA, &pstatus);
	return res;
}

static int vpmadt032_disable_ec(struct vpmadt032 *vpm, int channel)
{
	int res;
	GPAK_AlgControlStat_t pstatus;

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		vpm_info(vpm, "Disabling ecan on channel: %d\n", channel);

	res = vpmadt032_control(vpm, channel, BypassSwCompanding, &pstatus);
	if (res) {
		vpm_info(vpm, "Unable to disable sw companding on "
			 "echo cancellation channel %d (reason %d)\n",
			 channel, res);
	}
	res = vpmadt032_control(vpm, channel, BypassEcanA, &pstatus);
	return res;
}

static struct change_order *get_next_order(struct vpmadt032 *vpm)
{
	struct change_order *order;

	spin_lock(&vpm->change_list_lock);
	if (!list_empty(&vpm->change_list)) {
		order = list_entry(vpm->change_list.next,
				   struct change_order, node);
		list_del(&order->node);
	} else {
		order = NULL;
	}
	spin_unlock(&vpm->change_list_lock);

	return order;
}

static int nlp_settings_changed(const struct adt_lec_params *a,
				const struct adt_lec_params *b)
{
	return ((a->nlp_type != b->nlp_type) ||
		(a->nlp_threshold != b->nlp_threshold) ||
		(a->nlp_max_suppress != b->nlp_max_suppress));
}

static int echocan_on(const struct adt_lec_params *new,
		      const struct adt_lec_params *old)
{
	return ((new->tap_length != old->tap_length) &&
	       (new->tap_length > 0));
}

static int echocan_off(const struct adt_lec_params *new,
		       const struct adt_lec_params *old)
{
	return ((new->tap_length != old->tap_length) &&
	       (0 == new->tap_length));
}

static void update_channel_config(struct vpmadt032 *vpm, unsigned int channel,
				  struct adt_lec_params *desired)
{
	int res;
	GPAK_AlgControlStat_t pstatus;
	GPAK_ChannelConfigStat_t cstatus;
	GPAK_TearDownChanStat_t tstatus;
	GpakChannelConfig_t chanconfig;

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN) {
		vpm_info(vpm, "Reconfiguring chan %d for nlp %d, "
			 "nlp_thresh %d, and max_supp %d\n", channel + 1,
			 desired->nlp_type, desired->nlp_threshold,
			 desired->nlp_max_suppress);
	}

	vpm->setchanconfig_from_state(vpm, channel, &chanconfig);

	res = gpakTearDownChannel(vpm->dspid, channel, &tstatus);
	if (res)
		return;

	res = gpakConfigureChannel(vpm->dspid, channel, tdmToTdm,
				   &chanconfig, &cstatus);
	if (res)
		return;

	if (!desired->tap_length) {
		res = vpmadt032_control(vpm, channel,
				     BypassSwCompanding, &pstatus);
		if (res) {
			vpm_info(vpm, "Unable to disable sw companding on "
				 "echo cancellation channel %d (reason %d)\n",
				 channel, res);
		}
		vpmadt032_control(vpm, channel, BypassEcanA, &pstatus);
	}

	return;
}

/**
 * vpmadt032_bh - Changes the echocan parameters on the vpmadt032 module.
 *
 * This function is typically scheduled to run in the workqueue by the
 * vpmadt032_echocan_with_params function.  This is because communicating with
 * the hardware can take some time while messages are sent to the VPMADT032
 * module and the driver waits for the responses.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void vpmadt032_bh(void *data)
{
	struct vpmadt032 *vpm = data;
#else
static void vpmadt032_bh(struct work_struct *data)
{
	struct vpmadt032 *vpm = container_of(data, struct vpmadt032, work);
#endif
	struct change_order *order;

	while ((order = get_next_order(vpm))) {

		struct adt_lec_params *old;
		struct adt_lec_params *new;
		unsigned int channel;

		channel = order->channel;
		BUG_ON(channel >= ARRAY_SIZE(vpm->curecstate));
		old = &vpm->curecstate[channel];
		new = &order->params;

		if (nlp_settings_changed(new, old))
			update_channel_config(vpm, channel, new);
		else if (echocan_on(new, old))
			vpmadt032_enable_ec(vpm, channel, new->companding);
		else if (echocan_off(new, old))
			vpmadt032_disable_ec(vpm, channel);

		*old = order->params;
		free_change_order(order);
	}
}

#include "adt_lec.c"
static void vpmadt032_check_and_schedule_update(struct vpmadt032 *vpm,
						struct change_order *order)
{
	struct change_order *cur;
	struct change_order *n;

	INIT_LIST_HEAD(&order->node);
	spin_lock(&vpm->change_list_lock);
	list_for_each_entry_safe(cur, n, &vpm->change_list, node) {
		if (cur->channel == order->channel) {
			list_replace(&cur->node, &order->node);
			free_change_order(cur);
			break;
		}
	}
	if (list_empty(&order->node))
		list_add_tail(&order->node, &vpm->change_list);
	spin_unlock(&vpm->change_list_lock);

	queue_work(vpm->wq, &vpm->work);
}
int vpmadt032_echocan_create(struct vpmadt032 *vpm, int channo,
			     enum adt_companding companding,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p)
{
	unsigned int ret;
	struct change_order *order = alloc_change_order();
	if (!order)
		return -ENOMEM;

	memcpy(&order->params, &vpm->curecstate[channo], sizeof(order->params));
	ret = adt_lec_parse_params(&order->params, ecp, p);
	if (ret) {
		free_change_order(order);
		return ret;
	}

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN) {
		vpm_info(vpm, "Channel is %d length %d\n",
			 channo, ecp->tap_length);
	}

	/* The driver cannot control the number of taps on the VPMADT032
	 * module. Instead, it uses tap_length to enable or disable the echo
	 * cancellation. */
	order->params.tap_length = (ecp->tap_length) ? 1 : 0;
	order->params.companding = companding;
	order->channel = channo;

	vpmadt032_check_and_schedule_update(vpm, order);
	return 0;
}
EXPORT_SYMBOL(vpmadt032_echocan_create);

void vpmadt032_echocan_free(struct vpmadt032 *vpm, int channo,
	struct dahdi_echocan_state *ec)
{
	struct change_order *order;
	order = alloc_change_order();
	WARN_ON(!order);
	if (!order)
		return;

	adt_lec_init_defaults(&order->params, 0);
	order->params.nlp_type = vpm->options.vpmnlptype;
	order->params.nlp_threshold = vpm->options.vpmnlpthresh;
	order->params.nlp_max_suppress = vpm->options.vpmnlpmaxsupp;
	order->channel = channo;

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		vpm_info(vpm, "Channel is %d length 0\n", channo);

	vpmadt032_check_and_schedule_update(vpm, order);
}
EXPORT_SYMBOL(vpmadt032_echocan_free);

struct vpmadt032 *
vpmadt032_alloc(struct vpmadt032_options *options)
{
	struct vpmadt032 *vpm;
	int i;

	might_sleep();

	vpm = kzalloc(sizeof(*vpm), GFP_KERNEL);
	if (!vpm)
		return NULL;

	/* Init our vpmadt032 struct */
	memcpy(&vpm->options, options, sizeof(*options));
	spin_lock_init(&vpm->list_lock);
	spin_lock_init(&vpm->change_list_lock);
	INIT_LIST_HEAD(&vpm->change_list);
	INIT_LIST_HEAD(&vpm->pending_cmds);
	INIT_LIST_HEAD(&vpm->active_cmds);
	sema_init(&vpm->sem, 1);
	vpm->curpage = 0x80;
	vpm->dspid = -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&vpm->work, vpmadt032_bh, vpm);
#else
	INIT_WORK(&vpm->work, vpmadt032_bh);
#endif

	/* Do not use the global workqueue for processing these events.  Some of
	 * the operations can take 100s of ms, most of that time spent sleeping.
	 * On single CPU systems, this unduly serializes operations accross
	 * multiple vpmadt032 instances. */
	vpm->wq = create_singlethread_workqueue("vpmadt032");
	if (!vpm->wq) {
		kfree(vpm);
		return NULL;
	}

	/* Place this structure in the ifaces array so that the DspId from the
	 * Gpak Library can be used to locate it. */
	spin_lock(&ifacelock);
	for (i=0; i<MAX_DSP_CORES; ++i) {
		if (NULL == ifaces[i]) {
			ifaces[i] = vpm;
			vpm->dspid = i;
			break;
		}
	}
	spin_unlock(&ifacelock);

	if (-1 == vpm->dspid) {
		kfree(vpm);
		dev_notice(&vpm->vb->pdev->dev, "Unable to initialize another vpmadt032 modules\n");
		vpm = NULL;
	}

	return vpm;
}
EXPORT_SYMBOL(vpmadt032_alloc);

int vpmadt032_reset(struct vpmadt032 *vpm)
{
	int res;
	gpakPingDspStat_t pingstatus;
	u16 version;
	unsigned long stoptime;
	struct device *const dev = &vpm->vb->pdev->dev;

	might_sleep();

	set_bit(VPM150M_HPIRESET, &vpm->control);
	msleep(2000);
	/* It should never take longer than 5 seconds. */
	stoptime = jiffies + 3*HZ;
	while (test_bit(VPM150M_HPIRESET, &vpm->control) &&
	       time_before(jiffies, stoptime))
		msleep(1);

	if (time_after(jiffies, stoptime)) {
		dev_dbg(dev, "Detected failure to clear HPIRESET.\n");
		return -EIO;
	}

	/* Set us up to page 0 */
	vpmadt032_setpage(vpm, 0);

	res = vpmadtreg_loadfirmware(vpm->vb);
	if (res) {
		dev_err(dev, "Failed to load VPMADT032 firmware.\n");
		return res;
	}
	vpm->curpage = -1;

	stoptime = jiffies + 3*HZ;
	set_bit(VPM150M_SWRESET, &vpm->control);
	while (test_bit(VPM150M_SWRESET, &vpm->control) &&
	       time_before(jiffies, stoptime))
		msleep(1);

	if (time_after(jiffies, stoptime)) {
		dev_dbg(dev, "Detected failure to clear SWRESET.\n");
		return -EIO;
	}


	/* Set us up to page 0 */
	pingstatus = gpakPingDsp(vpm->dspid, &version);
	if (!pingstatus) {
		vpm_info(vpm, "VPM present and operational "
			"(Firmware version %x)\n", version);
		vpm->version = version;
		res = 0;
	} else {
		res = -EIO;
	}
	return res;
}
EXPORT_SYMBOL(vpmadt032_reset);

/**
 * vpmadt032_test - Check if there is a VPMADT032 present on voicebus device.
 * @vpm:	Allocated with vpmadt032_alloc previously.
 * @vb:		Voicebus structure to test on.
 *
 * Returns 0 if there is a device, otherwise -ENODEV.
 *
 */
int vpmadt032_test(struct vpmadt032 *vpm, struct voicebus *vb)
{
	u8 reg;
	int i, x;
	struct device *dev = &vb->pdev->dev;

	vpm->vb = vb;

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		dev_info(dev, "VPMADT032 Testing page access: ");

	for (i = 0; i < 0xf; i++) {
		for (x = 0; x < 3; x++) {
			vpmadt032_setpage(vpm, i);
			reg = vpmadt032_getpage(vpm);
			if (reg != i) {
				if (vpm->options.debug &
				    DEBUG_VPMADT032_ECHOCAN) {
					dev_err(dev, "Failed: Sent %x != %x " \
						"VPMADT032 Failed HI page " \
						"test\n", i, reg);
				}
				return -ENODEV;
			}
		}
	}

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		dev_info(dev, "Passed\n");

	return 0;
}
EXPORT_SYMBOL(vpmadt032_test);

/**
 * vpmadt032_init - Initialize and load VPMADT032 firmware.
 * @vpm:	Allocated with vpmadt032_alloc previously.
 *
 * Returns 0 on success.  This must be called after vpmadt032_test already
 * checked if there appears to be a VPMADT032 installed on the board.
 *
 */
int vpmadt032_init(struct vpmadt032 *vpm)
{
	int i;
	u16 reg;
	int res = -EFAULT;
	unsigned long stoptime;
	struct device *dev;
	gpakPingDspStat_t pingstatus;

	BUG_ON(!vpm->setchanconfig_from_state);
	BUG_ON(!vpm->wq);
	BUG_ON(!vpm->vb);

	might_sleep();

	dev = &vpm->vb->pdev->dev;

	stoptime = jiffies + 3*HZ;
	set_bit(VPM150M_HPIRESET, &vpm->control);
	while (test_bit(VPM150M_HPIRESET, &vpm->control) &&
	       time_before(jiffies, stoptime))
		msleep(1);

	if (time_after(jiffies, stoptime)) {
		dev_dbg(dev, "Detected failure to clear HPIRESET.\n");
		res = -EIO;
		goto failed_exit;
	}
	msleep(250);

	/* Set us up to page 0 */
	vpmadt032_setpage(vpm, 0);
	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		dev_info(&vpm->vb->pdev->dev, "VPMADT032 now doing address test: ");

	for (i = 0; i < 16; i++) {
		int x;
		for (x = 0; x < 2; x++) {
			vpmadt032_setreg(vpm, 0x1000, i);
			vpmadt032_getreg(vpm, 0x1000, &reg);
			if (reg != i) {
				printk(KERN_CONT "VPMADT032 Failed address test\n");
				res = -EIO;
			}
		}
	}

	if (vpm->options.debug & DEBUG_VPMADT032_ECHOCAN)
		printk(KERN_CONT "Passed\n");

	stoptime = jiffies + 3*HZ;
	set_bit(VPM150M_HPIRESET, &vpm->control);
	while (test_bit(VPM150M_HPIRESET, &vpm->control) &&
	       time_before(jiffies, stoptime))
		msleep(1);

	if (time_after(jiffies, stoptime)) {
		dev_dbg(dev, "Detected failure to clear SWRESET.\n");
		res = -EIO;
		goto failed_exit;
	}

	res = vpmadtreg_loadfirmware(vpm->vb);
	if (res) {
		dev_info(&vpm->vb->pdev->dev, "Failed to load the firmware.\n");
		return res;
	}
	vpm->curpage = -1;

	dev_info(&vpm->vb->pdev->dev, "Booting VPMADT032\n");

	stoptime = jiffies + 3*HZ;
	set_bit(VPM150M_SWRESET, &vpm->control);
	while (test_bit(VPM150M_SWRESET, &vpm->control) &&
	       time_before(jiffies, stoptime))
		msleep(1);

	if (time_after(jiffies, stoptime)) {
		dev_dbg(dev, "Detected failure to clear SWRESET.\n");
		res = -EIO;
		goto failed_exit;
	}

	pingstatus = gpakPingDsp(vpm->dspid, &vpm->version);

	if (!pingstatus) {
		dev_info(&vpm->vb->pdev->dev, "VPM present and operational "
			"(Firmware version %x)\n", vpm->version);
	} else {
		dev_notice(&vpm->vb->pdev->dev, "VPMADT032 Failed! Unable to ping the DSP (%d)!\n", pingstatus);
		res = -EIO;
		goto failed_exit;
	}

	return 0;

failed_exit:
	return res;
}
EXPORT_SYMBOL(vpmadt032_init);


void vpmadt032_get_default_parameters(struct GpakEcanParms *p)
{
	memset(p, 0, sizeof(*p));

	p->EcanTapLength = 1024;
	p->EcanNlpType = DEFAULT_NLPTYPE;
	p->EcanAdaptEnable = 1;

#ifdef CONFIG_DAHDI_NO_ECHOCAN_DISABLE
	p->EcanG165DetEnable = 0;
#else
	p->EcanG165DetEnable = 1;
#endif

	p->EcanDblTalkThresh = 6;
	p->EcanMaxDoubleTalkThres = 40;
	p->EcanNlpThreshold = DEFAULT_NLPTHRESH;
	p->EcanNlpConv = 18;
	p->EcanNlpUnConv = 12;
	p->EcanNlpMaxSuppress = DEFAULT_NLPMAXSUPP;
	p->EcanCngThreshold = 43;
	p->EcanAdaptLimit = 25;
	p->EcanCrossCorrLimit = 15;
	p->EcanNumFirSegments = 3;
	p->EcanFirSegmentLen = 48;
	p->EcanReconvergenceCheckEnable = 1;
	p->EcanTandemOperationEnable = 0;
	p->EcanMixedFourWireMode = 0;
	p->EcanSaturationLevel = 3;
	p->EcanNLPSaturationThreshold = 6;
}
EXPORT_SYMBOL(vpmadt032_get_default_parameters);

void vpmadt032_free(struct vpmadt032 *vpm)
{
	unsigned long flags;
	struct vpmadt032_cmd *cmd;
	struct change_order *order;
	LIST_HEAD(local_list);

	if (!vpm)
		return;

	BUG_ON(!vpm->wq);

	destroy_workqueue(vpm->wq);

	/* Move all the commands onto the local list protected by the locks */
	spin_lock_irqsave(&vpm->list_lock, flags);
	list_splice(&vpm->pending_cmds, &local_list);
	list_splice(&vpm->active_cmds, &local_list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);

	while (!list_empty(&local_list)) {
		cmd = list_entry(local_list.next, struct vpmadt032_cmd, node);
		list_del(&cmd->node);
		kfree(cmd);
	}

	spin_lock(&vpm->change_list_lock);
	list_splice(&vpm->change_list, &local_list);
	spin_unlock(&vpm->change_list_lock);

	while (!list_empty(&local_list)) {
		order = list_entry(local_list.next, struct change_order, node);
		list_del(&order->node);
		kfree(order);
	}

	BUG_ON(ifaces[vpm->dspid] != vpm);
	spin_lock(&ifacelock);
	ifaces[vpm->dspid] = NULL;
	spin_unlock(&ifacelock);
	kfree(vpm);
}
EXPORT_SYMBOL(vpmadt032_free);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadDspMemory - Read DSP memory.
 *
 * FUNCTION
 *  This function reads a contiguous block of words from DSP memory starting at
 *  the specified address.
 *
 * RETURNS
 *  nothing
 *
 */
void gpakReadDspMemory(
    unsigned short int DspId,   /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    DSP_ADDRESS DspAddress,     /* DSP's memory address of first word */
    unsigned int NumWords,      /* number of contiguous words to read */
    DSP_WORD *pWordValues       /* pointer to array of word values variable */
    )
{
	struct vpmadt032 *vpm = find_iface(DspId);
	int i;
	int ret;

	vpmadt032_io_wait(vpm);
	if ( NumWords < VPM150M_MAX_COMMANDS ) {
		struct vpmadt032_cmd *cmds[VPM150M_MAX_COMMANDS];
		memset(cmds, 0, sizeof(cmds));
		vpmadt032_setpage(vpm, DspAddress >> 16);
		DspAddress &= 0xffff;
		for (i=0; i < NumWords; ++i) {
			if (!(cmds[i] = vpmadt032_getreg_full_async(vpm,0,DspAddress+i))) {
				return;
			}
		}
		for (i=NumWords-1; i >=0; --i) {
			ret = vpmadt032_getreg_full_return(vpm,0,DspAddress+i,&pWordValues[i],
				cmds[i]);
			if (0 != ret) {
				return;
			}
		}
	}
	else {
		for (i=0; i<NumWords; ++i) {
			vpmadt032_getreg(vpm, DspAddress + i, &pWordValues[i]);
		}
	}
	return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakWriteDspMemory - Write DSP memory.
 *
 * FUNCTION
 *  This function writes a contiguous block of words to DSP memory starting at
 *  the specified address.
 *
 * RETURNS
 *  nothing
 *
 */
void gpakWriteDspMemory(
    unsigned short int DspId,   /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    DSP_ADDRESS DspAddress,     /* DSP's memory address of first word */
    unsigned int NumWords,      /* number of contiguous words to write */
    DSP_WORD *pWordValues       /* pointer to array of word values to write */
    )
{

	struct vpmadt032 *vpm = find_iface(DspId);
	int i;

	//dev_info(&vpm->vb->pdev->dev, "Writing %d words to memory\n", NumWords);
	if (vpm) {
		for (i = 0; i < NumWords; ++i) {
			vpmadt032_setreg(vpm, DspAddress + i, pWordValues[i]);
		}
#if 0
		for (i = 0; i < NumWords; i++) {
			if (wctdm_vpmadt032_getreg(wc, DspAddress + i) != pWordValues[i]) {
				dev_notice(&vpm->vb->pdev->dev, "Error in write.  Address %x is not %x\n", DspAddress + i, pWordValues[i]);
			}
		}
#endif
	}
	return;

}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakHostDelay - Delay for a fixed time interval.
 *
 * FUNCTION
 *  This function delays for a fixed time interval before returning. The time
 *  interval is the Host Port Interface sampling period when polling a DSP for
 *  replies to command messages.
 *
 * RETURNS
 *  nothing
 *
 */
void gpakHostDelay(void)
{
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakLockAccess - Lock access to the specified DSP.
 *
 * FUNCTION
 *  This function aquires exclusive access to the specified DSP.
 *
 * RETURNS
 *  nothing
 *
 */
void gpakLockAccess(unsigned short DspId)
{
	struct vpmadt032 *vpm;

	vpm = find_iface(DspId);

	if (!vpm)
		return;

	down(&vpm->sem);
	return;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakUnlockAccess - Unlock access to the specified DSP.
 *
 * FUNCTION
 *  This function releases exclusive access to the specified DSP.
 *
 * RETURNS
 *  nothing
 *
 */
void gpakUnlockAccess(unsigned short DspId)
{
	struct vpmadt032 *vpm;

	vpm = find_iface(DspId);

	if (vpm) {
		up(&vpm->sem);
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadFile - Read a block of bytes from a G.PAK Download file.
 *
 * FUNCTION
 *  This function reads a contiguous block of bytes from a G.PAK Download file
 *  starting at the current file position.
 *
 * RETURNS
 *  The number of bytes read from the file.
 *   -1 indicates an error occurred.
 *    0 indicates all bytes have been read (end of file)
 *
 */
int gpakReadFile(
    GPAK_FILE_ID FileId,        /* G.PAK Download File Identifier */
    unsigned char *pBuffer,	/* pointer to buffer for storing bytes */
    unsigned int NumBytes       /* number of bytes to read */
    )
{
	/* The firmware is loaded into the part by a closed-source firmware
	 * loader, and therefore this function should never be called. */
	WARN_ON(1);
	return -1;
}
