/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004, Xorcom
 *
 * Derived from ztdummy
 *
 * Copyright (C) 2002, Hermes Softlab
 * Copyright (C) 2004, Digium, Inc.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>	/* for udelay */
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <dahdi/kernel.h>
#include "xbus-core.h"
#include "xproto.h"
#include "xpp_dahdi.h"
#include "parport_debug.h"

static const char rcsid[] = "$Id$";

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *xpp_proc_toplevel = NULL;
#define	PROC_DIR		"xpp"
#define	PROC_XPD_SUMMARY	"summary"
#endif

#define	MAX_QUEUE_LEN		10000
#define	DELAY_UNTIL_DIALTONE	3000

DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
EXPORT_SYMBOL(debug);
static DEF_PARM_BOOL(prefmaster, 0, 0644,
		     "Do we want to be dahdi preferred sync master");
// DEF_ARRAY(int, pcmtx, 4, 0, "Forced PCM values to transmit");

#include "dahdi_debug.h"

static void phonedev_cleanup(xpd_t *xpd);

#ifdef	DEBUG_SYNC_PARPORT
/*
 * Use parallel port to sample our PCM sync and diagnose quality and
 * potential problems. A logic analizer or a scope should be connected
 * to the data bits of the parallel port.
 *
 * Array parameter: Choose the two xbuses Id's to sample.
 *                  This can be changed on runtime as well. Example:
 *                    echo "3,5" > /sys/module/xpp/parameters/parport_xbuses
 */
static int parport_xbuses[2] = { 0, 1 };

unsigned int parport_xbuses_num_values;
module_param_array(parport_xbuses, int, &parport_xbuses_num_values, 0577);
MODULE_PARM_DESC(parport_xbuses, "Id's of xbuses to sample (1-2)");

/*
 * Flip a single bit in the parallel port:
 *   - The bit number is either bitnum0 or bitnum1
 *   - Bit is selected by xbus number from parport_xbuses[]
 */
void xbus_flip_bit(xbus_t *xbus, unsigned int bitnum0, unsigned int bitnum1)
{
	int num = xbus->num;

	if (num == parport_xbuses[0])
		flip_parport_bit(bitnum0);
	if (num == parport_xbuses[1])
		flip_parport_bit(bitnum1);
}
EXPORT_SYMBOL(xbus_flip_bit);
#endif

static atomic_t num_registered_spans = ATOMIC_INIT(0);

int total_registered_spans(void)
{
	return atomic_read(&num_registered_spans);
}

#ifdef	CONFIG_PROC_FS
static const struct file_operations xpd_read_proc_ops;
#endif

/*------------------------- XPD Management -------------------------*/

/*
 * Called by put_xpd() when XPD has no more references.
 */
static void xpd_destroy(struct kref *kref)
{
	xpd_t *xpd;

	xpd = kref_to_xpd(kref);
	XPD_DBG(DEVICES, xpd, "%s\n", __func__);
	xpd_device_unregister(xpd);
}

int refcount_xpd(xpd_t *xpd)
{
	struct kref *kref = &xpd->kref;

	return atomic_read(&kref->refcount);
}

xpd_t *get_xpd(const char *msg, xpd_t *xpd)
{
	XPD_DBG(DEVICES, xpd, "%s: refcount_xpd=%d\n", msg, refcount_xpd(xpd));
	kref_get(&xpd->kref);
	return xpd;
}
EXPORT_SYMBOL(get_xpd);

void put_xpd(const char *msg, xpd_t *xpd)
{
	XPD_DBG(DEVICES, xpd, "%s: refcount_xpd=%d\n", msg, refcount_xpd(xpd));
	kref_put(&xpd->kref, xpd_destroy);
}
EXPORT_SYMBOL(put_xpd);

static void xpd_proc_remove(xbus_t *xbus, xpd_t *xpd)
{
#ifdef CONFIG_PROC_FS
	if (xpd->proc_xpd_dir) {
		if (xpd->proc_xpd_summary) {
			XPD_DBG(PROC, xpd, "Removing proc '%s'\n",
				PROC_XPD_SUMMARY);
			remove_proc_entry(PROC_XPD_SUMMARY, xpd->proc_xpd_dir);
			xpd->proc_xpd_summary = NULL;
		}
		XPD_DBG(PROC, xpd, "Removing %s/%s proc directory\n",
			xbus->busname, xpd->xpdname);
		remove_proc_entry(xpd->xpdname, xbus->proc_xbus_dir);
		xpd->proc_xpd_dir = NULL;
	}
#endif
}

static int xpd_proc_create(xbus_t *xbus, xpd_t *xpd)
{
#ifdef	CONFIG_PROC_FS
	XPD_DBG(PROC, xpd, "Creating proc directory\n");
	xpd->proc_xpd_dir = proc_mkdir(xpd->xpdname, xbus->proc_xbus_dir);
	if (!xpd->proc_xpd_dir) {
		XPD_ERR(xpd, "Failed to create proc directory\n");
		goto err;
	}
	xpd->proc_xpd_summary = proc_create_data(PROC_XPD_SUMMARY, 0444,
						 xpd->proc_xpd_dir,
						 &xpd_read_proc_ops, xpd);
	if (!xpd->proc_xpd_summary) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_XPD_SUMMARY);
		goto err;
	}
	SET_PROC_DIRENTRY_OWNER(xpd->proc_xpd_summary);
#endif
	return 0;
#ifdef	CONFIG_PROC_FS
err:
	xpd_proc_remove(xbus, xpd);
	return -EFAULT;
#endif
}

void xpd_free(xpd_t *xpd)
{
	xbus_t *xbus = NULL;

	if (!xpd)
		return;
	if (xpd->xproto)
		xproto_put(xpd->xproto);	/* was taken in xpd_alloc() */
	xpd->xproto = NULL;
	xbus = xpd->xbus;
	if (!xbus)
		return;
	XPD_DBG(DEVICES, xpd, "\n");
	xpd_proc_remove(xbus, xpd);
	xbus_xpd_unbind(xbus, xpd);
	phonedev_cleanup(xpd);
	KZFREE(xpd);
	DBG(DEVICES, "refcount_xbus=%d\n", refcount_xbus(xbus));
	/*
	 * This must be last, so the xbus cannot be released before the xpd
	 */
	put_xbus(__func__, xbus);	/* was taken in xpd_alloc() */
}
EXPORT_SYMBOL(xpd_free);

/*
 * Synchronous part of XPD detection.
 * Called from new_card()
 */
int create_xpd(xbus_t *xbus, const xproto_table_t *proto_table, int unit,
	       int subunit, __u8 type, __u8 subtype, int subunits,
	       int subunit_ports, __u8 port_dir)
{
	xpd_t *xpd = NULL;
	bool to_phone;

	BUG_ON(type == XPD_TYPE_NOMODULE);
	to_phone = BIT(subunit) & port_dir;
	BUG_ON(!xbus);
	xpd = xpd_byaddr(xbus, unit, subunit);
	if (xpd) {
		XPD_NOTICE(xpd, "XPD at %d%d already exists\n", unit, subunit);
		return 0;
	}
	if (subunit_ports <= 0 || subunit_ports > CHANNELS_PERXPD) {
		XBUS_NOTICE(xbus, "Illegal number of ports %d for XPD %d%d\n",
			    subunit_ports, unit, subunit);
		return 0;
	}
	xpd =
	    proto_table->xops->card_new(xbus, unit, subunit, proto_table,
					subtype, subunits, subunit_ports,
					to_phone);
	if (!xpd) {
		XBUS_NOTICE(xbus, "card_new(%d,%d,%d,%d,%d) failed. Ignored.\n",
			    unit, subunit, proto_table->type, subtype,
			    to_phone);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(create_xpd);

#ifdef CONFIG_PROC_FS

/**
 * Prints a general procfs entry for the bus, under xpp/BUSNAME/summary
 */
static int xpd_read_proc_show(struct seq_file *sfile, void *data)
{
	int len = 0;
	xpd_t *xpd = sfile->private;
	int i;

	if (!xpd)
		return -EINVAL;

	seq_printf(sfile,
		    "%s (%s, card %s, span %d)\n" "timing_priority: %d\n"
		    "timer_count: %d span->mainttimer=%d\n", xpd->xpdname,
		    xpd->type_name, (xpd->card_present) ? "present" : "missing",
		    (SPAN_REGISTERED(xpd)) ? PHONEDEV(xpd).span.spanno : 0,
		    PHONEDEV(xpd).timing_priority, xpd->timer_count,
		    PHONEDEV(xpd).span.mainttimer);
	seq_printf(sfile, "xpd_state: %s (%d)\n",
		    xpd_statename(xpd->xpd_state), xpd->xpd_state);
	seq_printf(sfile, "open_counter=%d refcount=%d\n",
		    atomic_read(&PHONEDEV(xpd).open_counter),
		    refcount_xpd(xpd));
	seq_printf(sfile, "Address: U=%d S=%d\n", xpd->addr.unit,
		    xpd->addr.subunit);
	seq_printf(sfile, "Subunits: %d\n", xpd->subunits);
	seq_printf(sfile, "Type: %d.%d\n\n", xpd->type, xpd->subtype);
	seq_printf(sfile, "pcm_len=%d\n\n", PHONEDEV(xpd).pcm_len);
	seq_printf(sfile, "wanted_pcm_mask=0x%04X\n\n",
		    PHONEDEV(xpd).wanted_pcm_mask);
	seq_printf(sfile, "mute_dtmf=0x%04X\n\n",
		    PHONEDEV(xpd).mute_dtmf);
	seq_printf(sfile, "STATES:");
	seq_printf(sfile, "\n\t%-17s: ", "output_relays");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ",
			    IS_SET(PHONEDEV(xpd).digital_outputs, i));
	}
	seq_printf(sfile, "\n\t%-17s: ", "input_relays");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ",
			    IS_SET(PHONEDEV(xpd).digital_inputs, i));
	}
	seq_printf(sfile, "\n\t%-17s: ", "offhook");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ", IS_OFFHOOK(xpd, i));
	}
	seq_printf(sfile, "\n\t%-17s: ", "oht_pcm_pass");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ",
			    IS_SET(PHONEDEV(xpd).oht_pcm_pass, i));
	}
	seq_printf(sfile, "\n\t%-17s: ", "msg_waiting");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ", PHONEDEV(xpd).msg_waiting[i]);
	}
	seq_printf(sfile, "\n\t%-17s: ", "ringing");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ", PHONEDEV(xpd).ringing[i]);
	}
	seq_printf(sfile, "\n\t%-17s: ", "no_pcm");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d ", IS_SET(PHONEDEV(xpd).no_pcm, i));
	}
#if 1
	if (SPAN_REGISTERED(xpd)) {
		seq_printf(sfile,
			"\nPCM:\n            |"
			"         [readchunk]       |"
			"         [writechunk]      | W D");
		for_each_line(xpd, i) {
			struct dahdi_chan *chan = XPD_CHAN(xpd, i);
			__u8 rchunk[DAHDI_CHUNKSIZE];
			__u8 wchunk[DAHDI_CHUNKSIZE];
			__u8 *rp;
			__u8 *wp;
			int j;

			if (IS_SET(PHONEDEV(xpd).digital_outputs, i))
				continue;
			if (IS_SET(PHONEDEV(xpd).digital_inputs, i))
				continue;
			if (IS_SET(PHONEDEV(xpd).digital_signalling, i))
				continue;
			rp = chan->readchunk;
			wp = chan->writechunk;
			memcpy(rchunk, rp, DAHDI_CHUNKSIZE);
			memcpy(wchunk, wp, DAHDI_CHUNKSIZE);
			seq_printf(sfile, "\n  port %2d>  |  ", i);
			for (j = 0; j < DAHDI_CHUNKSIZE; j++)
				seq_printf(sfile, "%02X ", rchunk[j]);
			seq_printf(sfile, " |  ");
			for (j = 0; j < DAHDI_CHUNKSIZE; j++)
				seq_printf(sfile, "%02X ", wchunk[j]);
			seq_printf(sfile, " | %c",
				(IS_SET(PHONEDEV(xpd).wanted_pcm_mask, i))
					?  '+' : ' ');
			seq_printf(sfile, " %c",
				(IS_SET(PHONEDEV(xpd).mute_dtmf, i))
					? '-' : ' ');
		}
	}
#endif
#if 0
	if (SPAN_REGISTERED(xpd)) {
		seq_printf(sfile, "\nSignalling:\n");
		for_each_line(xpd, i) {
			struct dahdi_chan *chan = XPD_CHAN(xpd, i);
			seq_printf(sfile,
				    "\t%2d> sigcap=0x%04X sig=0x%04X\n", i,
				    chan->sigcap, chan->sig);
		}
	}
#endif
	seq_printf(sfile, "\nCOUNTERS:\n");
	for (i = 0; i < XPD_COUNTER_MAX; i++) {
		seq_printf(sfile, "\t\t%-20s = %d\n",
			    xpd_counters[i].name, xpd->counters[i]);
	}
	seq_printf(sfile, "<-- len=%d\n", len);
	return 0;
}

static int xpd_read_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, xpd_read_proc_show, PDE_DATA(inode));
}

static const struct file_operations xpd_read_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= xpd_read_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#endif

const char *xpd_statename(enum xpd_state st)
{
	switch (st) {
	case XPD_STATE_START:
		return "START";
	case XPD_STATE_INIT_REGS:
		return "INIT_REGS";
	case XPD_STATE_READY:
		return "READY";
	case XPD_STATE_NOHW:
		return "NOHW";
	}
	return NULL;
}

bool xpd_setstate(xpd_t *xpd, enum xpd_state newstate)
{
	BUG_ON(!xpd);
	XPD_DBG(DEVICES, xpd, "%s: %s (%d) -> %s (%d)\n", __func__,
		xpd_statename(xpd->xpd_state), xpd->xpd_state,
		xpd_statename(newstate), newstate);
	switch (newstate) {
	case XPD_STATE_START:
		goto badstate;
	case XPD_STATE_INIT_REGS:
		if (xpd->xpd_state != XPD_STATE_START)
			goto badstate;
		if (xpd->addr.subunit != 0) {
			XPD_NOTICE(xpd,
				"%s: Moving to %s allowed only for subunit 0\n",
				__func__, xpd_statename(newstate));
			goto badstate;
		}
		break;
	case XPD_STATE_READY:
		if (xpd->addr.subunit == 0) {
			/* Unit 0 script initialize registers of all subunits */
			if (xpd->xpd_state != XPD_STATE_INIT_REGS)
				goto badstate;
		} else {
			if (xpd->xpd_state != XPD_STATE_START)
				goto badstate;
		}
		break;
	case XPD_STATE_NOHW:
		break;
	default:
		XPD_ERR(xpd, "%s: Unknown newstate=%d\n", __func__, newstate);
	}
	xpd->xpd_state = newstate;
	return 1;
badstate:
	XPD_NOTICE(xpd, "%s: cannot transition: %s (%d) -> %s (%d)\n", __func__,
		   xpd_statename(xpd->xpd_state), xpd->xpd_state,
		   xpd_statename(newstate), newstate);
	return 0;
}

/*
 * Cleanup/initialize phonedev
 */
static void phonedev_cleanup(xpd_t *xpd)
{
	struct phonedev *phonedev = &PHONEDEV(xpd);
	unsigned int x;

	for (x = 0; x < phonedev->channels; x++) {
		if (phonedev->chans[x]) {
			KZFREE(phonedev->chans[x]);
			phonedev->chans[x] = NULL;
		}
		if (phonedev->ec[x]) {
			KZFREE(phonedev->ec[x]);
			phonedev->ec[x] = NULL;
		}
	}
	phonedev->channels = 0;
}

int phonedev_alloc_channels(xpd_t *xpd, int channels)
{
	struct phonedev *phonedev = &PHONEDEV(xpd);
	int old_channels = phonedev->channels;
	unsigned int x;

	XPD_DBG(DEVICES, xpd, "Reallocating channels: %d -> %d\n",
			old_channels, channels);
	phonedev_cleanup(xpd);
	phonedev->channels = channels;
	for (x = 0; x < phonedev->channels; x++) {
		if (!
		    (phonedev->chans[x] =
		     KZALLOC(sizeof(*(phonedev->chans[x])), GFP_KERNEL))) {
			ERR("%s: Unable to allocate channel %d\n", __func__, x);
			goto err;
		}
		phonedev->ec[x] =
		    KZALLOC(sizeof(*(phonedev->ec[x])), GFP_KERNEL);
		if (!phonedev->ec[x]) {
			ERR("%s: Unable to allocate ec state %d\n", __func__,
			    x);
			goto err;
		}
	}
	return 0;
err:
	phonedev_cleanup(xpd);
	return -ENOMEM;
}
EXPORT_SYMBOL(phonedev_alloc_channels);

__must_check static int phonedev_init(xpd_t *xpd,
				      const xproto_table_t *proto_table,
				      int channels, xpp_line_t no_pcm)
{
	struct phonedev *phonedev = &PHONEDEV(xpd);

	spin_lock_init(&phonedev->lock_recompute_pcm);
	phonedev->no_pcm = no_pcm;
	phonedev->offhook_state = 0x0;	/* ONHOOK */
	phonedev->phoneops = proto_table->phoneops;
	phonedev->digital_outputs = 0;
	phonedev->digital_inputs = 0;
	atomic_set(&phonedev->dahdi_registered, 0);
	atomic_set(&phonedev->open_counter, 0);
	if (phonedev_alloc_channels(xpd, channels) < 0)
		goto err;
	return 0;
err:
	return -ENOMEM;
}


/*
 * xpd_alloc - Allocator for new XPD's
 *
 */
__must_check xpd_t *xpd_alloc(xbus_t *xbus, int unit, int subunit,
	int subtype, int subunits, size_t privsize,
	const xproto_table_t *proto_table, int channels)
{
	xpd_t *xpd = NULL;
	size_t alloc_size = sizeof(xpd_t) + privsize;
	int type = proto_table->type;
	xpp_line_t no_pcm = 0;

	BUG_ON(!proto_table);
	XBUS_DBG(DEVICES, xbus, "type=%d channels=%d (alloc_size=%zd)\n", type,
		 channels, alloc_size);
	if (channels > CHANNELS_PERXPD) {
		XBUS_ERR(xbus, "%s: type=%d: too many channels %d\n", __func__,
			 type, channels);
		goto err;
	}

	if ((xpd = KZALLOC(alloc_size, GFP_KERNEL)) == NULL) {
		XBUS_ERR(xbus, "%s: type=%d: Unable to allocate memory\n",
			 __func__, type);
		goto err;
	}
	xpd->priv = (__u8 *)xpd + sizeof(xpd_t);
	spin_lock_init(&xpd->lock);
	xpd->card_present = 0;
	xpd->type = proto_table->type;
	xpd->xproto = proto_table;
	xpd->xops = proto_table->xops;
	xpd->xpd_state = XPD_STATE_START;
	xpd->subtype = subtype;
	xpd->subunits = subunits;
	kref_init(&xpd->kref);

	/* For USB-1 disable some channels */
	if (MAX_SEND_SIZE(xbus) < RPACKET_SIZE(GLOBAL, PCM_WRITE)) {
		no_pcm =
		    0x7F | PHONEDEV(xpd).digital_outputs | PHONEDEV(xpd).
		    digital_inputs;
		XBUS_NOTICE(xbus,
			"max xframe size = %d, disabling some PCM channels. "
			"no_pcm=0x%04X\n",
			MAX_SEND_SIZE(xbus), PHONEDEV(xpd).no_pcm);
	}
	if (phonedev_init(xpd, proto_table, channels, no_pcm) < 0)
		goto err;
	xbus_xpd_bind(xbus, xpd, unit, subunit);
	if (xpd_proc_create(xbus, xpd) < 0)
		goto err;
	/*
	 * This makes sure the xbus cannot be removed before this xpd
	 * is removed in xpd_free()
	 */
	xbus = get_xbus(__func__, xbus->num);	/* returned in xpd_free() */
	xproto_get(type);	/* will be returned in xpd_free() */
	return xpd;
err:
	if (xpd) {
		xpd_proc_remove(xbus, xpd);
		phonedev_cleanup(xpd);
		KZFREE(xpd);
	}
	return NULL;
}
EXPORT_SYMBOL(xpd_alloc);

/*
 * The xpd isn't open by anyone, we can unregister it and free it
 */
void xpd_remove(xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_INFO(xpd, "Remove\n");
	CALL_XMETHOD(card_remove, xpd);
	xpd_free(xpd);
}

void update_xpd_status(xpd_t *xpd, int alarm_flag)
{
	struct dahdi_span *span = &PHONEDEV(xpd).span;

	if (!SPAN_REGISTERED(xpd)) {
#if 0
		XPD_NOTICE(xpd,
			"%s: XPD is not registered. Skipping.\n",
			__func__);
#endif
		return;
	}
	switch (alarm_flag) {
	case DAHDI_ALARM_NONE:
		xpd->last_response = jiffies;
		break;
	default:
		// Nothing
		break;
	}
	if (span->alarms == alarm_flag)
		return;
	XPD_DBG(GENERAL, xpd, "Update XPD alarms: %s -> %02X\n",
		PHONEDEV(xpd).span.name, alarm_flag);
	span->alarms = alarm_flag;
	dahdi_alarm_notify(span);
}
EXPORT_SYMBOL(update_xpd_status);

/*
 * Used to block/pass PCM during onhook-transfers. E.g:
 *  - Playing FSK after FXS ONHOOK for MWI (non-neon style)
 *  - Playing DTFM/FSK for FXO Caller-ID detection.
 */
void oht_pcm(xpd_t *xpd, int pos, bool pass)
{
	if (pass) {
		LINE_DBG(SIGNAL, xpd, pos, "OHT PCM: pass\n");
		BIT_SET(PHONEDEV(xpd).oht_pcm_pass, pos);
	} else {
		LINE_DBG(SIGNAL, xpd, pos, "OHT PCM: block\n");
		BIT_CLR(PHONEDEV(xpd).oht_pcm_pass, pos);
	}
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
}
EXPORT_SYMBOL(oht_pcm);

/*
 * Update our hookstate -- for PCM block/pass
 */
void mark_offhook(xpd_t *xpd, int pos, bool to_offhook)
{
	if (to_offhook) {
		LINE_DBG(SIGNAL, xpd, pos, "OFFHOOK\n");
		BIT_SET(PHONEDEV(xpd).offhook_state, pos);
	} else {
		LINE_DBG(SIGNAL, xpd, pos, "ONHOOK\n");
		BIT_CLR(PHONEDEV(xpd).offhook_state, pos);
	}
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
}
EXPORT_SYMBOL(mark_offhook);

/*
 * Send a signalling notification to Asterisk
 */
void notify_rxsig(xpd_t *xpd, int pos, enum dahdi_rxsig rxsig)
{
	/*
	 * We should not spinlock before calling dahdi_hooksig() as
	 * it may call back into our xpp_hooksig() and cause
	 * a nested spinlock scenario
	 */
	LINE_DBG(SIGNAL, xpd, pos, "rxsig=%s\n", rxsig2str(rxsig));
	if (SPAN_REGISTERED(xpd))
		dahdi_hooksig(XPD_CHAN(xpd, pos), rxsig);
}
EXPORT_SYMBOL(notify_rxsig);

/*
 * Called when hardware state changed:
 *  - FXS -- the phone was picked up or hanged-up.
 *  - FXO -- we answered the phone or handed-up.
 */
void hookstate_changed(xpd_t *xpd, int pos, bool to_offhook)
{
	BUG_ON(!xpd);
	mark_offhook(xpd, pos, to_offhook);
	if (!to_offhook) {
		oht_pcm(xpd, pos, 0);
		/*
		 * To prevent latest PCM to stay in buffers
		 * indefinitely, mark this channel for a
		 * single silence transmittion.
		 *
		 * This bit will be cleared on the next tick.
		 */
		BIT_SET(PHONEDEV(xpd).silence_pcm, pos);
	}
	notify_rxsig(xpd, pos,
		     (to_offhook) ? DAHDI_RXSIG_OFFHOOK : DAHDI_RXSIG_ONHOOK);
}
EXPORT_SYMBOL(hookstate_changed);

#define	XPP_MAX_LEN	512

/*------------------------- Dahdi Interfaces -----------------------*/

/*
 * Called with spinlock held on chan. Must not call back
 * dahdi functions.
 */
static int _xpp_open(struct dahdi_chan *chan)
{
	xpd_t *xpd;
	xbus_t *xbus;
	int pos;
	int open_counter;

	if (!chan) {
		NOTICE("open called on a null chan\n");
		return -EINVAL;
	}
	xpd = chan->pvt;
	if (!xpd) {
		NOTICE("open called on a chan with no pvt (xpd)\n");
		BUG();
	}
	xbus = xpd->xbus;
	if (!xbus) {
		NOTICE("open called on a chan with no xbus\n");
		BUG();
	}
	pos = chan->chanpos - 1;
	if (!xpd->card_present) {
		LINE_NOTICE(xpd, pos, "Cannot open -- device not ready\n");
		return -ENODEV;
	}
	open_counter = atomic_inc_return(&PHONEDEV(xpd).open_counter);
	LINE_DBG(DEVICES, xpd, pos, "%s[%d]: open_counter=%d\n", current->comm,
		 current->pid, open_counter);
	if (PHONE_METHOD(card_open, xpd))
		CALL_PHONE_METHOD(card_open, xpd, pos);
	return 0;
}

int xpp_open(struct dahdi_chan *chan)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&chan->lock, flags);
	res = _xpp_open(chan);
	spin_unlock_irqrestore(&chan->lock, flags);
	return res;
}
EXPORT_SYMBOL(xpp_open);

int xpp_close(struct dahdi_chan *chan)
{
	xpd_t *xpd = chan->pvt;
	int pos = chan->chanpos - 1;
	int open_counter;

	if (PHONE_METHOD(card_close, xpd))
		CALL_PHONE_METHOD(card_close, xpd, pos);
	/* from xpp_open(): */
	open_counter = atomic_dec_return(&PHONEDEV(xpd).open_counter);
	LINE_DBG(DEVICES, xpd, pos, "%s[%d]: open_counter=%d\n", current->comm,
		 current->pid, open_counter);
	return 0;
}
EXPORT_SYMBOL(xpp_close);

void report_bad_ioctl(const char *msg, xpd_t *xpd, int pos, unsigned int cmd)
{
	char *extra_msg = "";

	if (_IOC_TYPE(cmd) == 'J')
		extra_msg = " (for old ZAPTEL)";
	XPD_NOTICE(xpd, "%s: Bad ioctl%s\n", msg, extra_msg);
	XPD_NOTICE(xpd, "ENOTTY: chan=%d cmd=0x%x\n", pos, cmd);
	XPD_NOTICE(xpd, "        IOC_TYPE=0x%02X\n", _IOC_TYPE(cmd));
	XPD_NOTICE(xpd, "        IOC_DIR=0x%02X\n", _IOC_DIR(cmd));
	XPD_NOTICE(xpd, "        IOC_NR=%d\n", _IOC_NR(cmd));
	XPD_NOTICE(xpd, "        IOC_SIZE=0x%02X\n", _IOC_SIZE(cmd));
}
EXPORT_SYMBOL(report_bad_ioctl);

int xpp_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long arg)
{
	xpd_t *xpd = chan->pvt;
	int pos = chan->chanpos - 1;

	if (!xpd) {
		ERR("%s: channel in pos %d, was already closed. Ignore.\n",
		    __func__, pos);
		return -ENODEV;
	}
	switch (cmd) {
	default:
		/* Some span-specific commands before we give up: */
		if (PHONE_METHOD(card_ioctl, xpd)) {
			return CALL_PHONE_METHOD(card_ioctl, xpd, pos, cmd,
						 arg);
		}
		report_bad_ioctl(THIS_MODULE->name, xpd, pos, cmd);
		return -ENOTTY;
	}
	return 0;
}
EXPORT_SYMBOL(xpp_ioctl);

int xpp_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	xpd_t *xpd = chan->pvt;
	xbus_t *xbus;
	int pos = chan->chanpos - 1;

	if (!xpd) {
		ERR("%s: channel in pos %d, was already closed. Ignore.\n",
		    __func__, pos);
		return -ENODEV;
	}
	if (!PHONE_METHOD(card_hooksig, xpd)) {
		LINE_ERR(xpd, pos,
			 "%s: No hooksig method for this channel. Ignore.\n",
			 __func__);
		return -ENODEV;
	}
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	DBG(SIGNAL, "Setting %s to %s (%d)\n", chan->name, txsig2str(txsig),
	    txsig);
	return CALL_PHONE_METHOD(card_hooksig, xpd, pos, txsig);
}
EXPORT_SYMBOL(xpp_hooksig);

/* Req: Set the requested chunk size.  This is the unit in which you must
   report results for conferencing, etc */
int xpp_setchunksize(struct dahdi_span *span, int chunksize);

/* Enable maintenance modes */
int xpp_maint(struct dahdi_span *span, int cmd)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	int ret = 0;
#if 0
	char loopback_data[] = "THE-QUICK-BROWN-FOX-JUMPED-OVER-THE-LAZY-DOG";
#endif

	DBG(GENERAL, "span->mainttimer=%d\n", span->mainttimer);
	switch (cmd) {
	case DAHDI_MAINT_NONE:
		INFO("XXX Turn off local and remote loops XXX\n");
		break;
	case DAHDI_MAINT_LOCALLOOP:
		INFO("XXX Turn on local loopback XXX\n");
		break;
	case DAHDI_MAINT_REMOTELOOP:
		INFO("XXX Turn on remote loopback XXX\n");
		break;
	case DAHDI_MAINT_LOOPUP:
		INFO("XXX Send loopup code XXX\n");
		break;
	case DAHDI_MAINT_LOOPDOWN:
		INFO("XXX Send loopdown code XXX\n");
		break;
	default:
		ERR("XPP: Unknown maint command: %d\n", cmd);
		ret = -EINVAL;
		break;
	}
	if (span->mainttimer || span->maintstat)
		update_xpd_status(xpd, DAHDI_ALARM_LOOPBACK);
	return ret;
}
EXPORT_SYMBOL(xpp_maint);

#ifdef	CONFIG_DAHDI_WATCHDOG
/*
 * If the watchdog detects no received data, it will call the
 * watchdog routine
 */
int xpp_watchdog(struct dahdi_span *span, int cause)
{
	static int rate_limit;

	if ((rate_limit++ % 1000) == 0)
		DBG(GENERAL, "\n");
	return 0;
}
EXPORT_SYMBOL(xpp_watchdog);
#endif

/*
 * Hardware Echo Canceller management
 */
static void echocan_free(struct dahdi_chan *chan,
			 struct dahdi_echocan_state *ec)
{
	xpd_t *xpd;
	xbus_t *xbus;
	int pos = chan->chanpos - 1;
	const struct echoops *echoops;

	xpd = chan->pvt;
	xbus = xpd->xbus;
	echoops = ECHOOPS(xbus);
	if (!echoops)
		return;
	LINE_DBG(GENERAL, xpd, pos, "mode=0x%X\n", ec->status.mode);
	CALL_EC_METHOD(ec_set, xbus, xpd, pos, 0);
	CALL_EC_METHOD(ec_update, xbus, xbus);
	put_xpd(__func__, xpd);	/* aquired in xpp_echocan_create() */
}

static const struct dahdi_echocan_features xpp_ec_features = {
};

static const struct dahdi_echocan_ops xpp_ec_ops = {
	.echocan_free = echocan_free,
};

const char *xpp_echocan_name(const struct dahdi_chan *chan)
{
	xpd_t *xpd;
	xbus_t *xbus;
	int pos;

	if (!chan) {
		NOTICE("%s(NULL)\n", __func__);
		return "XPP";
	}
	xpd = chan->pvt;
	xbus = xpd->xbus;
	pos = chan->chanpos - 1;
	LINE_DBG(GENERAL, xpd, pos, "\n");
	if (!ECHOOPS(xbus))
		return NULL;
	/*
	 * quirks and limitations
	 */
	if (xbus->quirks.has_fxo) {
		if (xbus->quirks.has_digital_span && xpd->type == XPD_TYPE_FXO) {
			LINE_NOTICE(xpd, pos,
				    "quirk: give up HWEC on FXO: "
				    "AB has digital span\n");
			return NULL;
		} else if (xbus->sync_mode != SYNC_MODE_AB
			   && xpd->type == XPD_TYPE_FXS) {
			LINE_NOTICE(xpd, pos,
				    "quirk: give up HWEC on FXS: "
				    "AB has FXO and is sync slave\n");
			return NULL;
		}
	}
	return "XPP";
}
EXPORT_SYMBOL(xpp_echocan_name);

int xpp_echocan_create(struct dahdi_chan *chan,
	struct dahdi_echocanparams *ecp,
	struct dahdi_echocanparam *p,
	struct dahdi_echocan_state **ec)
{
	xpd_t *xpd;
	xbus_t *xbus;
	int pos;
	struct phonedev *phonedev;
	const struct echoops *echoops;
	int ret;

	xpd = chan->pvt;
	xbus = xpd->xbus;
	pos = chan->chanpos - 1;
	echoops = ECHOOPS(xbus);
	if (!echoops)
		return -ENODEV;
	phonedev = &PHONEDEV(xpd);
	*ec = phonedev->ec[pos];
	(*ec)->ops = &xpp_ec_ops;
	(*ec)->features = xpp_ec_features;
	xpd = get_xpd(__func__, xpd);	/* Returned in echocan_free() */
	LINE_DBG(GENERAL, xpd, pos, "(tap=%d, param_count=%d)\n",
		 ecp->tap_length, ecp->param_count);
	ret = CALL_EC_METHOD(ec_set, xbus, xpd, pos, 1);
	CALL_EC_METHOD(ec_update, xbus, xbus);
	return ret;
}
EXPORT_SYMBOL(xpp_echocan_create);

void xpp_span_assigned(struct dahdi_span *span)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);

	XPD_INFO(xpd, "Span assigned: %d\n", span->spanno);
	if (xpd->card_present) {
		span->alarms &= ~DAHDI_ALARM_NOTOPEN;
		dahdi_alarm_notify(&phonedev->span);
	}
}
EXPORT_SYMBOL(xpp_span_assigned);

static const struct dahdi_span_ops xpp_span_ops = {
	.owner = THIS_MODULE,
	.open = xpp_open,
	.close = xpp_close,
	.ioctl = xpp_ioctl,
	.maint = xpp_maint,
	.echocan_create = xpp_echocan_create,
	.echocan_name = xpp_echocan_name,
	.assigned = xpp_span_assigned,
};

static const struct dahdi_span_ops xpp_rbs_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = xpp_hooksig,
	.open = xpp_open,
	.close = xpp_close,
	.ioctl = xpp_ioctl,
	.maint = xpp_maint,
	.echocan_create = xpp_echocan_create,
	.echocan_name = xpp_echocan_name,
	.assigned = xpp_span_assigned,
};

void xpd_set_spanname(xpd_t *xpd)
{
	struct dahdi_span *span = &PHONEDEV(xpd).span;

	snprintf(span->name, MAX_SPANNAME, "%s/%s", xpd->xbus->busname,
		 xpd->xpdname);
	/*
	 * The "Xorcom XPD" is a prefix in one of the regexes we
	 * use in our dahdi_genconf to match for PRI cards.
	 * FIXME: After moving completely to sysfs, we can remove
	 * this horseshit.
	 */
	snprintf(span->desc, MAX_SPANDESC, "Xorcom XPD [%s].%d: %s",
		 xpd->xbus->label, span->offset + 1, xpd->type_name);
}
EXPORT_SYMBOL(xpd_set_spanname);

static void xpd_init_span(xpd_t *xpd, unsigned offset, int cn)
{
	struct dahdi_span *span;

	memset(&PHONEDEV(xpd).span, 0, sizeof(struct dahdi_span));
	phonedev_alloc_channels(xpd, cn);
	span = &PHONEDEV(xpd).span;
	span->deflaw = DAHDI_LAW_MULAW;	/* card_* drivers may override */
	span->channels = cn;
	span->chans = PHONEDEV(xpd).chans;

	span->flags = DAHDI_FLAG_RBS;
	span->offset = offset;
	if (PHONEDEV(xpd).phoneops->card_hooksig)
		span->ops = &xpp_rbs_span_ops;	/* Only with RBS bits */
	else
		span->ops = &xpp_span_ops;
	xpd_set_spanname(xpd);
	list_add_tail(&span->device_node, &xpd->xbus->ddev->spans);
}

int xpd_dahdi_preregister(xpd_t *xpd, unsigned offset)
{
	xbus_t *xbus;
	int cn;
	struct phonedev *phonedev;

	BUG_ON(!xpd);

	xbus = xpd->xbus;

	if (!IS_PHONEDEV(xpd)) {
		XPD_ERR(xpd, "Not a telephony device\n");
		return -EBADF;
	}

	phonedev = &PHONEDEV(xpd);

	if (SPAN_REGISTERED(xpd)) {
		XPD_ERR(xpd, "Already registered\n");
		return -EEXIST;
	}

	cn = PHONEDEV(xpd).channels;
	xpd_init_span(xpd, offset, cn);
	XPD_DBG(DEVICES, xpd, "Preregister local span %d: %d channels.\n",
		offset + 1, cn);
	CALL_PHONE_METHOD(card_dahdi_preregistration, xpd, 1);
	return 0;
}

int xpd_dahdi_postregister(xpd_t *xpd)
{
	int cn;

	atomic_inc(&num_registered_spans);
	atomic_inc(&PHONEDEV(xpd).dahdi_registered);
	CALL_PHONE_METHOD(card_dahdi_postregistration, xpd, 1);
	/*
	 * Update dahdi about our state:
	 *   - Since asterisk didn't open the channel yet,
	 *     the report is discarded anyway.
	 *   - Our FXS driver have another notification mechanism that
	 *     is triggered (indirectly) by the open() of the channe.
	 *   - The real fix should be in Asterisk (to get the correct state
	 *     after open).
	 */
	for_each_line(xpd, cn) {
		if (IS_OFFHOOK(xpd, cn))
			notify_rxsig(xpd, cn, DAHDI_RXSIG_OFFHOOK);
	}
	return 0;
}

/*
 * Try our best to make asterisk close all channels related to
 * this Astribank:
 *   - Set span state to DAHDI_ALARM_NOTOPEN in all relevant spans.
 *   - Notify dahdi afterwards about spans
 *     (so it can see all changes at once).
 *   - Also send DAHDI_EVENT_REMOVED on all channels.
 */
void xpd_dahdi_preunregister(xpd_t *xpd)
{
	if (!xpd || !IS_PHONEDEV(xpd))
		return;
	XPD_DBG(DEVICES, xpd, "\n");
	update_xpd_status(xpd, DAHDI_ALARM_NOTOPEN);
	if (xpd->card_present)
		CALL_PHONE_METHOD(card_dahdi_preregistration, xpd, 0);
	/* Now notify dahdi */
	if (SPAN_REGISTERED(xpd)) {
		int j;

		dahdi_alarm_notify(&PHONEDEV(xpd).span);
		XPD_DBG(DEVICES, xpd,
			"Queuing DAHDI_EVENT_REMOVED on all channels "
			"to ask user to release them\n");
		for (j = 0; j < PHONEDEV(xpd).span.channels; j++) {
			dahdi_qevent_lock(XPD_CHAN(xpd, j),
					  DAHDI_EVENT_REMOVED);
		}
	}
}

void xpd_dahdi_postunregister(xpd_t *xpd)
{
	if (!xpd || !IS_PHONEDEV(xpd))
		return;
	atomic_dec(&PHONEDEV(xpd).dahdi_registered);
	atomic_dec(&num_registered_spans);
	if (xpd->card_present)
		CALL_PHONE_METHOD(card_dahdi_postregistration, xpd, 0);
}

/*------------------------- Initialization -------------------------*/

static void do_cleanup(void)
{
#ifdef CONFIG_PROC_FS
	if (xpp_proc_toplevel) {
		DBG(GENERAL, "Removing '%s' from proc\n", PROC_DIR);
		remove_proc_entry(PROC_DIR, NULL);
		xpp_proc_toplevel = NULL;
	}
#endif
}

static int __init xpp_dahdi_init(void)
{
	int ret = 0;
	void *top = NULL;

	INFO("revision %s MAX_XPDS=%d (%d*%d)\n", XPP_VERSION, MAX_XPDS,
	     MAX_UNIT, MAX_SUBUNIT);
#ifdef CONFIG_PROC_FS
	xpp_proc_toplevel = proc_mkdir(PROC_DIR, NULL);
	if (!xpp_proc_toplevel) {
		ret = -EIO;
		goto err;
	}
	top = xpp_proc_toplevel;
#endif
	ret = xbus_core_init();
	if (ret) {
		ERR("xbus_core_init failed (%d)\n", ret);
		goto err;
	}
	ret = xbus_pcm_init(top);
	if (ret) {
		ERR("xbus_pcm_init failed (%d)\n", ret);
		xbus_core_shutdown();
		goto err;
	}
	return 0;
err:
	do_cleanup();
	return ret;
}

static void __exit xpp_dahdi_cleanup(void)
{
	xbus_pcm_shutdown();
	xbus_core_shutdown();
	do_cleanup();
}

MODULE_DESCRIPTION("XPP Dahdi Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_VERSION(XPP_VERSION);

module_init(xpp_dahdi_init);
module_exit(xpp_dahdi_cleanup);
