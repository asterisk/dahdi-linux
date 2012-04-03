/*
 * Dynamic Span Interface for DAHDI
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2001-2010, Digium, Inc.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

#ifndef DAHDI_SYNC_TICK
#error "Dynamic support depends on DAHDI_SYNC_TICK being enabled."
#endif

/*
 * Tasklets provide better system interactive response at the cost of the
 * possibility of losing a frame of data at very infrequent intervals.  If
 * you are more concerned with the performance of your machine, enable the
 * tasklets.  If you are strict about absolutely no drops, then do not enable
 * tasklets.
 */

#undef ENABLE_TASKLETS

/*
 *  Dynamic spans implemented using TDM over X with standard message
 *  types.  Message format is as follows:
 *
 *         Byte #:          Meaning
 *         0                Number of samples per channel
 *         1                Current flags on span
 *				Bit    0: Yellow Alarm
 *	                        Bit    1: Sig bits present
 *				Bits 2-7: reserved for future use
 *         2-3		    16-bit counter value for detecting drops, network byte order.
 *         4-5		    Number of channels in the message, network byte order
 *         6...		    16-bit words, containing sig bits for each
 *                          four channels, least significant 4 bits being
 *                          the least significant channel, network byte order.
 *         the rest	    data for each channel, all samples per channel
                            before moving to the next.
 */

#define DAHDI_DYNAMIC_FLAG_YELLOW_ALARM		(1 << 0)
#define DAHDI_DYNAMIC_FLAG_SIGBITS_PRESENT	(1 << 1)
#define DAHDI_DYNAMIC_FLAG_LOOPBACK		(1 << 2)

#define ERR_NSAMP			(1 << 16)
#define ERR_NCHAN			(1 << 17)
#define ERR_LEN				(1 << 18)

static int dahdi_dynamic_init(void);
static void dahdi_dynamic_cleanup(void);

#ifdef ENABLE_TASKLETS
static int taskletrun;
static int taskletsched;
static int taskletpending;
static int taskletexec;
static int txerrors;
static struct tasklet_struct dahdi_dynamic_tlet;

static void dahdi_dynamic_tasklet(unsigned long data);
#endif

static DEFINE_MUTEX(dspan_mutex);
static DEFINE_SPINLOCK(dspan_lock);
static DEFINE_SPINLOCK(driver_lock);

static LIST_HEAD(dspan_list);
static LIST_HEAD(driver_list);

static int debug = 0;

static int hasmaster = 0;

static void checkmaster(void)
{
	int newhasmaster=0;
	int best = 9999999;
	struct dahdi_dynamic *d, *master = NULL;

	rcu_read_lock();

	list_for_each_entry_rcu(d, &dspan_list, list) {
		if (d->timing) {
			d->master = 0;
			if (!(d->span.alarms & DAHDI_ALARM_RED) &&
			    (d->timing < best)) {
				/* If not in alarm and they're
				   a better timing source, use them */
				master = d;
				best = d->timing;
				newhasmaster = 1;
			}
		}
	}

	hasmaster = newhasmaster;
	/* Mark the new master if there is one */
	if (master)
		master->master = 1;

	rcu_read_unlock();

	if (master)
		printk(KERN_INFO "TDMoX: New master: %s\n", master->span.name);
	else
		printk(KERN_INFO "TDMoX: No master.\n");
}

static void dahdi_dynamic_sendmessage(struct dahdi_dynamic *d)
{
	unsigned char *buf = d->msgbuf;
	unsigned short bits;
	int msglen = 0;
	int x;
	int offset;

	/* Byte 0: Number of samples per channel */
	*buf = DAHDI_CHUNKSIZE;
	buf++; msglen++;

	/* Byte 1: Flags */
	*buf = 0;
	if (d->span.alarms & DAHDI_ALARM_RED)
		*buf |= DAHDI_DYNAMIC_FLAG_YELLOW_ALARM;
	*buf |= DAHDI_DYNAMIC_FLAG_SIGBITS_PRESENT;
	buf++; msglen++;

	/* Bytes 2-3: Transmit counter */
	*((unsigned short *)buf) = htons((unsigned short)d->txcnt);
	d->txcnt++;
	buf++; msglen++;
	buf++; msglen++;

	/* Bytes 4-5: Number of channels */
	*((unsigned short *)buf) = htons((unsigned short)d->span.channels);
	buf++; msglen++;
	buf++; msglen++;
	bits = 0;
	offset = 0;
	for (x = 0; x < d->span.channels; x++) {
		offset = x % 4;
		bits |= (d->chans[x]->txsig & 0xf) << (offset << 2);
		if (offset == 3) {
			/* Write the bits when we have four channels */
			*((unsigned short *)buf) = htons(bits);
			buf++; msglen++;
			buf++; msglen++;
			bits = 0;
		}
	}

	if (offset != 3) {
		/* Finish it off if it's not done already */
		*((unsigned short *)buf) = htons(bits);
		buf++; msglen++;
		buf++; msglen++;
	}
	
	for (x = 0; x < d->span.channels; x++) {
		memcpy(buf, d->chans[x]->writechunk, DAHDI_CHUNKSIZE);
		buf += DAHDI_CHUNKSIZE;
		msglen += DAHDI_CHUNKSIZE;
	}
	
	d->driver->transmit(d, d->msgbuf, msglen);
	
}

static void __dahdi_dynamic_run(void)
{
	struct dahdi_dynamic *d;
	struct dahdi_dynamic_driver *drv;

	rcu_read_lock();
	list_for_each_entry_rcu(d, &dspan_list, list) {
		dahdi_transmit(&d->span);
		/* Handle all transmissions now */
		dahdi_dynamic_sendmessage(d);
	}

	list_for_each_entry_rcu(drv, &driver_list, list) {
		/* Flush any traffic still pending in the driver */
		if (drv->flush) {
			drv->flush();
		}
	}
	rcu_read_unlock();
}

#ifdef ENABLE_TASKLETS
static void dahdi_dynamic_run(void)
{
	if (likely(!taskletpending)) {
		taskletpending = 1;
		taskletsched++;
		tasklet_hi_schedule(&dahdi_dynamic_tlet);
	} else {
		txerrors++;
	}
}
#else
#define dahdi_dynamic_run __dahdi_dynamic_run
#endif

static inline struct dahdi_dynamic *dynamic_from_span(struct dahdi_span *span)
{
	return container_of(span, struct dahdi_dynamic, span);
}

void dahdi_dynamic_receive(struct dahdi_span *span, unsigned char *msg, int msglen)
{
	struct dahdi_dynamic *dtd = dynamic_from_span(span);
	int newerr=0;
	int sflags;
	int xlen;
	int x, bits, sig;
	int nchans, master;
	int newalarm;
	unsigned short rxpos, rxcnt;

	rcu_read_lock();

	if (unlikely(msglen < 6)) {
		rcu_read_unlock();
		newerr = ERR_LEN;
		if (newerr != dtd->err)
			printk(KERN_NOTICE "Span %s: Insufficient samples for header (only %d)\n", span->name, msglen);
		dtd->err = newerr;
		return;
	}
	
	/* First, check the chunksize */
	if (unlikely(*msg != DAHDI_CHUNKSIZE)) {
		rcu_read_unlock();
		newerr = ERR_NSAMP | msg[0];
		if (newerr != dtd->err)
			printk(KERN_NOTICE "Span %s: Expected %d samples, but receiving %d\n", span->name, DAHDI_CHUNKSIZE, msg[0]);
		dtd->err = newerr;
		return;
	}
	msg++;
	sflags = *msg;
	msg++;

	rxpos = ntohs(*((unsigned short *)msg));
	msg++;
	msg++;

	nchans = ntohs(*((unsigned short *)msg));
	if (unlikely(nchans != span->channels)) {
		rcu_read_unlock();
		newerr = ERR_NCHAN | nchans;
		if (newerr != dtd->err)
			printk(KERN_NOTICE "Span %s: Expected %d channels, but receiving %d\n", span->name, span->channels, nchans);
		dtd->err = newerr;
		return;
	}
	msg++;
	msg++;

	/* Okay now we've accepted the header, lets check our message
	   length... */

	/* Start with header */
	xlen = 6;
	/* Add samples of audio */
	xlen += nchans * DAHDI_CHUNKSIZE;
	/* If RBS info is there, add that */
	if (sflags & DAHDI_DYNAMIC_FLAG_SIGBITS_PRESENT) {
		/* Account for sigbits -- one short per 4 channels*/
		xlen += ((nchans + 3) / 4) * 2;
	}

	if (unlikely(xlen != msglen)) {
		rcu_read_unlock();
		newerr = ERR_LEN | xlen;
		if (newerr != dtd->err)
			printk(KERN_NOTICE "Span %s: Expected message size %d, but was %d instead\n", span->name, xlen, msglen);
		dtd->err = newerr;
		return;
	}

	bits = 0;

	/* Record sigbits if present */
	if (sflags & DAHDI_DYNAMIC_FLAG_SIGBITS_PRESENT) {
		for (x=0;x<nchans;x++) {
			if (!(x%4)) {
				/* Get new bits */
				bits = ntohs(*((unsigned short *)msg));
				msg++;
				msg++;
			}
			
			/* Pick the right bits */
			sig = (bits >> ((x % 4) << 2)) & 0xff;
			
			/* Update signalling if appropriate */
			if (sig != span->chans[x]->rxsig)
				dahdi_rbsbits(span->chans[x], sig);
				
		}
	}
	
	/* Record data for channels */
	for (x=0;x<nchans;x++) {
		memcpy(span->chans[x]->readchunk, msg, DAHDI_CHUNKSIZE);
		msg += DAHDI_CHUNKSIZE;
	}

	master = dtd->master;
	
	rxcnt = dtd->rxcnt;
	dtd->rxcnt = rxpos+1;

	/* Keep track of last received packet */
	dtd->rxjif = jiffies;

	rcu_read_unlock();

	/* Check for Yellow alarm */
	newalarm = span->alarms & ~(DAHDI_ALARM_YELLOW | DAHDI_ALARM_RED);
	if (sflags & DAHDI_DYNAMIC_FLAG_YELLOW_ALARM)
		newalarm |= DAHDI_ALARM_YELLOW;

	if (newalarm != span->alarms) {
		span->alarms = newalarm;
		dahdi_alarm_notify(span);
		checkmaster();
	}

	/* note if we had a missing packet */
	if (unlikely(rxpos != rxcnt))
		printk(KERN_NOTICE "Span %s: Expected seq no %d, but received %d instead\n", span->name, rxcnt, rxpos);

	dahdi_ec_span(span);
	dahdi_receive(span);

	/* If this is our master span, then run everything */
	if (master)
		dahdi_dynamic_run();
}
EXPORT_SYMBOL(dahdi_dynamic_receive);

/**
 * dahdi_dynamic_release() - Free the memory associated with the dahdi_dynamic.
 * @kref:	Pointer to kref embedded in dahdi_dynamic structure.
 *
 */
static void dahdi_dynamic_release(struct kref *kref)
{
	struct dahdi_dynamic *d = container_of(kref, struct dahdi_dynamic,
					       kref);
	unsigned int x;

	WARN_ON(test_bit(DAHDI_FLAGBIT_REGISTERED, &d->span.flags));

	kfree(d->msgbuf);

	for (x = 0; x < d->span.channels; x++)
		kfree(d->chans[x]);

	dahdi_free_device(d->ddev);
	kfree(d);
}

static inline int dynamic_put(struct dahdi_dynamic *d)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
	kref_put(&d->kref, dahdi_dynamic_release);
	return 1;
#else
	return kref_put(&d->kref, dahdi_dynamic_release);
#endif
}

static inline void dynamic_get(struct dahdi_dynamic *d)
{
	kref_get(&d->kref);
}

static struct dahdi_dynamic *find_dynamic(struct dahdi_dynamic_span *dds)
{
	struct dahdi_dynamic *d = NULL, *found = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(d, &dspan_list, list) {
		if (!strcmp(d->dname, dds->driver) &&
				!strcmp(d->addr, dds->addr)) {
			dynamic_get(d);
			found = d;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

static struct dahdi_dynamic_driver *find_driver(const char *name)
{
	struct dahdi_dynamic_driver *dtd, *found = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(dtd, &driver_list, list) {
		/* here's our driver */
		if (!strcmp(name, dtd->name)) {
			found = dtd;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

static int _destroy_dynamic(struct dahdi_dynamic_span *dds)
{
	unsigned long flags;
	struct dahdi_dynamic *d;

	d = find_dynamic(dds);
	if (unlikely(!d))
		return -EINVAL;

	/* We shouldn't have more than the two references at this point.  If
	 * we do, there are probably channels that are still opened. */
	if (atomic_read(&d->kref.refcount) > 2) {
		dynamic_put(d);
		return -EBUSY;
	}

	if (d->pvt) {
		if (d->driver && d->driver->destroy) {
			if (!try_module_get(d->driver->owner)) {
				/* The driver for this device is in the
				 * process of unloading. Leave this dynamic on
				 * the list so it's cleaned up when the driver
				 * unregisters. */
				 dynamic_put(d);
				 return -ENXIO;
			}
			d->driver->destroy(d);
			module_put(d->driver->owner);
		} else {
			WARN_ON(1);
		}
		d->pvt = NULL;
	}

	dahdi_unregister_device(d->ddev);

	spin_lock_irqsave(&dspan_lock, flags);
	list_del_rcu(&d->list);
	spin_unlock_irqrestore(&dspan_lock, flags);
	synchronize_rcu();

	/* One since we've removed the item from the list... */
	dynamic_put(d);
	/* ...and one for find_dynamic. */
	dynamic_put(d);
	return 0;
}

static int destroy_dynamic(struct dahdi_dynamic_span *dds)
{
	int ret;
	mutex_lock(&dspan_mutex);
	ret = _destroy_dynamic(dds);
	mutex_unlock(&dspan_mutex);
	return ret;
}

static int dahdi_dynamic_rbsbits(struct dahdi_chan *chan, int bits)
{
	/* Don't have to do anything */
	return 0;
}

static int dahdi_dynamic_open(struct dahdi_chan *chan)
{
	struct dahdi_dynamic *d = dynamic_from_span(chan->span);
	if (!try_module_get(d->driver->owner))
		return -ENODEV;
	dynamic_get(d);
	return 0;
}

static int dahdi_dynamic_chanconfig(struct file *file,
				    struct dahdi_chan *chan, int sigtype)
{
	return 0;
}

static int dahdi_dynamic_close(struct dahdi_chan *chan)
{
	struct dahdi_dynamic *d = dynamic_from_span(chan->span);
	struct module *owner = d->driver->owner;
	dynamic_put(d);
	module_put(owner);
	return 0;
}

static void dahdi_dynamic_sync_tick(struct dahdi_span *span, int is_master)
{
	struct dahdi_dynamic *head;
	struct dahdi_dynamic *d = dynamic_from_span(span);

	if (hasmaster)
		return;

	rcu_read_lock();
	head = list_entry(dspan_list.next, struct dahdi_dynamic, list);
	rcu_read_unlock();

	if (d == head)
		dahdi_dynamic_run();
	return;
}

static const struct dahdi_span_ops dynamic_ops = {
	.owner = THIS_MODULE,
	.rbsbits = dahdi_dynamic_rbsbits,
	.open = dahdi_dynamic_open,
	.close = dahdi_dynamic_close,
	.chanconfig = dahdi_dynamic_chanconfig,
	.sync_tick = dahdi_dynamic_sync_tick,
};

static int _create_dynamic(struct dahdi_dynamic_span *dds)
{
	int res = 0;
	struct dahdi_dynamic *d;
	struct dahdi_dynamic_driver *dtd;
	unsigned long flags;
	int x;
	int bufsize;

	if (dds->numchans < 1) {
		printk(KERN_NOTICE "Can't be less than 1 channel (%d)!\n",
			dds->numchans);
		return -EINVAL;
	}
	if (dds->numchans >= ARRAY_SIZE(d->chans)) {
		printk(KERN_NOTICE "Can't create dynamic span with greater "
		       "than %d channels.  See dahdi_dynamic.c and increase "
		       "DAHDI_DYNAMIC_MAX_CHANS\n", dds->numchans);
		return -EINVAL;
	}

	d = find_dynamic(dds);
	if (d) {
		dynamic_put(d);
		return -EEXIST;
	}

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	kref_init(&d->kref);
	d->ddev = dahdi_create_device();

	for (x = 0; x < dds->numchans; x++) {
		d->chans[x] = kzalloc(sizeof(*d->chans[x]), GFP_KERNEL);
		if (!d->chans[x]) {
			dynamic_put(d);
			return -ENOMEM;
		}
		d->span.channels++;
	}

	/* Allocate message buffer with sample space and header space */
	bufsize = dds->numchans * DAHDI_CHUNKSIZE + dds->numchans / 4 + 48;

	d->msgbuf = kzalloc(bufsize, GFP_KERNEL);

	if (!d->msgbuf) {
		dynamic_put(d);
		return -ENOMEM;
	}
	
	/* Setup parameters properly assuming we're going to be okay. */
	strlcpy(d->dname, dds->driver, sizeof(d->dname));
	strlcpy(d->addr, dds->addr, sizeof(d->addr));
	d->timing = dds->timing;
	snprintf(d->span.name, sizeof(d->span.name), "DYN/%s/%s",
		 dds->driver, dds->addr);
	snprintf(d->span.desc, sizeof(d->span.desc),
		 "Dynamic '%s' span at '%s'", dds->driver, dds->addr);
	d->span.deflaw = DAHDI_LAW_MULAW;
	d->span.flags |= DAHDI_FLAG_RBS;
	d->span.chans = d->chans;
	d->span.ops = &dynamic_ops;
	for (x = 0; x < d->span.channels; x++) {
		sprintf(d->chans[x]->name, "DYN/%s/%s/%d",
			dds->driver, dds->addr, x+1);
		d->chans[x]->sigcap = DAHDI_SIG_EM | DAHDI_SIG_CLEAR |
				      DAHDI_SIG_FXSLS | DAHDI_SIG_FXSKS |
				      DAHDI_SIG_FXSGS | DAHDI_SIG_FXOLS |
				      DAHDI_SIG_FXOKS | DAHDI_SIG_FXOGS |
				      DAHDI_SIG_SF | DAHDI_SIG_DACS_RBS |
				      DAHDI_SIG_CAS;
		d->chans[x]->chanpos = x + 1;
		d->chans[x]->pvt = d;
	}
	
	dtd = find_driver(dds->driver);
	if (!dtd) {
		request_module("dahdi_dynamic_%s", dds->driver);
		dtd = find_driver(dds->driver);
	}


	if (!dtd) {
		printk(KERN_NOTICE "No such driver '%s' for dynamic span\n",
			dds->driver);
		dynamic_put(d);
		return -EINVAL;
	}

	if (!try_module_get(dtd->owner)) {
		dynamic_put(d);
		return -ENODEV;
	}

	/* Remember the driver.  We also give our reference to the driver to
	 * the dahdi_dyanmic here.  Do not access dtd directly now. */
	d->driver = dtd;

	/* Create the stuff */
	res = dtd->create(d, d->addr);
	if (res) {
		printk(KERN_NOTICE "Driver '%s' (%s) rejected address '%s'\n",
			dtd->name, dtd->desc, d->addr);
		dynamic_put(d);
		module_put(dtd->owner);
		return res;
	}

	d->ddev->devicetype = d->span.name;
	d->ddev->hardware_id = d->span.name;
	dev_set_name(&d->ddev->dev, "dynamic:%s:%d", dds->driver, dtd->id++);
	list_add_tail(&d->span.device_node, &d->ddev->spans);
	/* Whee!  We're created.  Now register the span */
	if (dahdi_register_device(d->ddev, d->dev)) {
		printk(KERN_NOTICE "Unable to register span '%s'\n",
			d->span.name);
		dynamic_put(d);
		module_put(dtd->owner);
		return -EINVAL;
	}

	x = d->span.spanno;

	/* Transfer our reference to the dspan_list.  Do not touch d after
	 * this point. It also must remain on the list while registered. */
	spin_lock_irqsave(&dspan_lock, flags);
	list_add_rcu(&d->list, &dspan_list);
	spin_unlock_irqrestore(&dspan_lock, flags);

	checkmaster();

	module_put(dtd->owner);
	return x;
}

static int create_dynamic(struct dahdi_dynamic_span *dds)
{
	int ret;
	mutex_lock(&dspan_mutex);
	ret = _create_dynamic(dds);
	mutex_unlock(&dspan_mutex);
	return ret;
}

#ifdef ENABLE_TASKLETS
static void dahdi_dynamic_tasklet(unsigned long data)
{
	taskletrun++;
	if (taskletpending) {
		taskletexec++;
		__dahdi_dynamic_run();
	}
	taskletpending = 0;
}
#endif

static int dahdi_dynamic_ioctl(unsigned int cmd, unsigned long data)
{
	struct dahdi_dynamic_span dds;
	int res;
	switch(cmd) {
	case DAHDI_DYNAMIC_CREATE:
		if (copy_from_user(&dds, (__user const void *)data,
				   sizeof(dds)))
			return -EFAULT;
		if (debug)
			printk(KERN_DEBUG "Dynamic Create\n");
		res = create_dynamic(&dds);
		if (res < 0)
			return res;
		dds.spanno = res;
		/* Let them know the new span number */
		if (copy_to_user((__user void *) data, &dds, sizeof(dds)))
			return -EFAULT;
		return 0;
	case DAHDI_DYNAMIC_DESTROY:
		if (copy_from_user(&dds, (__user const void *)data,
				   sizeof(dds)))
			return -EFAULT;
		if (debug)
			printk(KERN_DEBUG "Dynamic Destroy\n");
		return destroy_dynamic(&dds);
	}

	return -ENOTTY;
}

int dahdi_dynamic_register_driver(struct dahdi_dynamic_driver *dri)
{
	unsigned long flags;
	int res = 0;

	if (!dri->owner)
		return -EINVAL;

	if (find_driver(dri->name)) {
		res = -1;
	} else {
		spin_lock_irqsave(&driver_lock, flags);
		list_add_rcu(&dri->list, &driver_list);
		spin_unlock_irqrestore(&driver_lock, flags);
	}
	return res;
}
EXPORT_SYMBOL(dahdi_dynamic_register_driver);

void dahdi_dynamic_unregister_driver(struct dahdi_dynamic_driver *dri)
{
	struct dahdi_dynamic *d, *n;
	unsigned long flags;

	mutex_lock(&dspan_mutex);

	list_for_each_entry_safe(d, n, &dspan_list, list) {
		if (d->driver == dri) {
			if (d->pvt) {
				if (d->driver && d->driver->destroy)
					d->driver->destroy(d);
				else
					WARN_ON(1);
			}
			dahdi_unregister_device(d->ddev);
			spin_lock_irqsave(&dspan_lock, flags);
			list_del_rcu(&d->list);
			spin_unlock_irqrestore(&dspan_lock, flags);
			synchronize_rcu();
			d->driver = NULL;
			dynamic_put(d);
		}
	}

	spin_lock_irqsave(&driver_lock, flags);
	list_del_rcu(&dri->list);
	spin_unlock_irqrestore(&driver_lock, flags);
	synchronize_rcu();

	mutex_unlock(&dspan_mutex);
}
EXPORT_SYMBOL(dahdi_dynamic_unregister_driver);

static struct timer_list alarmcheck;

static void check_for_red_alarm(unsigned long ignored)
{
	int newalarm;
	int alarmchanged = 0;
	struct dahdi_dynamic *d;

	rcu_read_lock();
	list_for_each_entry_rcu(d, &dspan_list, list) {
		newalarm = d->span.alarms & ~DAHDI_ALARM_RED;
		/* If nothing received for a second, consider that RED ALARM */
		if ((jiffies - d->rxjif) > 1 * HZ) {
			newalarm |= DAHDI_ALARM_RED;
			if (d->span.alarms != newalarm) {
				d->span.alarms = newalarm;
				dahdi_alarm_notify(&d->span);
				alarmchanged++;
			}
		}
	}
	rcu_read_unlock();

	if (alarmchanged)
		checkmaster();

	/* Do the next one */
	mod_timer(&alarmcheck, jiffies + 1 * HZ);
}

static const struct dahdi_dynamic_ops dahdi_dynamic_ops = {
	.owner = THIS_MODULE,
	.ioctl = dahdi_dynamic_ioctl,
};

static int dahdi_dynamic_init(void)
{
	/* Start process to check for RED ALARM */
	init_timer(&alarmcheck);
	alarmcheck.expires = 0;
	alarmcheck.data = 0;
	alarmcheck.function = check_for_red_alarm;
	/* Check once per second */
	mod_timer(&alarmcheck, jiffies + 1 * HZ);
#ifdef ENABLE_TASKLETS
	tasklet_init(&dahdi_dynamic_tlet, dahdi_dynamic_tasklet, 0);
#endif
	dahdi_set_dynamic_ops(&dahdi_dynamic_ops);

	printk(KERN_INFO "DAHDI Dynamic Span support LOADED\n");
	return 0;
}

static void dahdi_dynamic_cleanup(void)
{
	dahdi_set_dynamic_ops(NULL);

#ifdef ENABLE_TASKLETS
	if (taskletpending) {
		tasklet_disable(&dahdi_dynamic_tlet);
		tasklet_kill(&dahdi_dynamic_tlet);
	}
#endif
	del_timer_sync(&alarmcheck);
	/* Must call again in case it was running before and rescheduled
	 * itself. */
	del_timer(&alarmcheck);
	printk(KERN_INFO "DAHDI Dynamic Span support unloaded\n");
}

module_param(debug, int, 0600);

MODULE_DESCRIPTION("DAHDI Dynamic Span Support");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(dahdi_dynamic_init);
module_exit(dahdi_dynamic_cleanup);
