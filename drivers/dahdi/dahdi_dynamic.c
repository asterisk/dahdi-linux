/*
 * Dynamic Span Interface for DAHDI
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2001-2008, Digium, Inc.
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

/*
 * Tasklets provide better system interactive response at the cost of the
 * possibility of losing a frame of data at very infrequent intervals.  If
 * you are more concerned with the performance of your machine, enable the
 * tasklets.  If you are strict about absolutely no drops, then do not enable
 * tasklets.
 */

#define ENABLE_TASKLETS

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

/* Arbitrary limit to the max # of channels in a span */
#define DAHDI_DYNAMIC_MAX_CHANS	256

#define ZTD_FLAG_YELLOW_ALARM		(1 << 0)
#define ZTD_FLAG_SIGBITS_PRESENT	(1 << 1)
#define ZTD_FLAG_LOOPBACK		(1 << 2)

#define ERR_NSAMP			(1 << 16)
#define ERR_NCHAN			(1 << 17)
#define ERR_LEN				(1 << 18)

EXPORT_SYMBOL(dahdi_dynamic_register);
EXPORT_SYMBOL(dahdi_dynamic_unregister);
EXPORT_SYMBOL(dahdi_dynamic_receive);

static int ztdynamic_init(void);
static void ztdynamic_cleanup(void);

#ifdef ENABLE_TASKLETS
static int taskletrun;
static int taskletsched;
static int taskletpending;
static int taskletexec;
static int txerrors;
static struct tasklet_struct ztd_tlet;

static void ztd_tasklet(unsigned long data);
#endif

struct dahdi_dynamic {
	char addr[40];
	char dname[20];
	int err;
	int usecount;
	int dead;
	long rxjif;
	unsigned short txcnt;
	unsigned short rxcnt;
	struct dahdi_span span;
	struct dahdi_chan *chans[DAHDI_DYNAMIC_MAX_CHANS];
	struct dahdi_dynamic_driver *driver;
	void *pvt;
	int timing;
	int master;
	unsigned char *msgbuf;

	struct list_head list;
};

#ifdef DEFINE_SPINLOCK
static DEFINE_SPINLOCK(dspan_lock);
static DEFINE_SPINLOCK(driver_lock);
#else
static spinlock_t dspan_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t driver_lock = SPIN_LOCK_UNLOCKED;
#endif

static LIST_HEAD(dspan_list);
static LIST_HEAD(driver_list);

static int debug = 0;

static int hasmaster = 0;

static void checkmaster(void)
{
	int newhasmaster=0;
	int best = 9999999;
	struct dahdi_dynamic *z, *master=NULL;

	rcu_read_lock();

	list_for_each_entry_rcu(z, &dspan_list, list) {
		if (z->timing) {
			z->master = 0;
			if (!(z->span.alarms & DAHDI_ALARM_RED) &&
			    (z->timing < best) && !z->dead) {
				/* If not in alarm and they're
				   a better timing source, use them */
				master = z;
				best = z->timing;
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

static void ztd_sendmessage(struct dahdi_dynamic *z)
{
	unsigned char *buf = z->msgbuf;
	unsigned short bits;
	int msglen = 0;
	int x;
	int offset;

	/* Byte 0: Number of samples per channel */
	*buf = DAHDI_CHUNKSIZE;
	buf++; msglen++;

	/* Byte 1: Flags */
	*buf = 0;
	if (z->span.alarms & DAHDI_ALARM_RED)
		*buf |= ZTD_FLAG_YELLOW_ALARM;
	*buf |= ZTD_FLAG_SIGBITS_PRESENT;
	buf++; msglen++;

	/* Bytes 2-3: Transmit counter */
	*((unsigned short *)buf) = htons((unsigned short)z->txcnt);
	z->txcnt++;
	buf++; msglen++;
	buf++; msglen++;

	/* Bytes 4-5: Number of channels */
	*((unsigned short *)buf) = htons((unsigned short)z->span.channels);
	buf++; msglen++;
	buf++; msglen++;
	bits = 0;
	offset = 0;
	for (x=0;x<z->span.channels;x++) {
		offset = x % 4;
		bits |= (z->chans[x]->txsig & 0xf) << (offset << 2);
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
	
	for (x=0;x<z->span.channels;x++) {
		memcpy(buf, z->chans[x]->writechunk, DAHDI_CHUNKSIZE);
		buf += DAHDI_CHUNKSIZE;
		msglen += DAHDI_CHUNKSIZE;
	}
	
	z->driver->transmit(z->pvt, z->msgbuf, msglen);
	
}

static void __ztdynamic_run(void)
{
	struct dahdi_dynamic *z;
	struct dahdi_dynamic_driver *drv;
	int y;

	rcu_read_lock();
	list_for_each_entry_rcu(z, &dspan_list, list) {
		if (!z->dead) {
			for (y=0;y<z->span.channels;y++) {
				/* Echo cancel double buffered data */
				dahdi_ec_chunk(z->span.chans[y], z->span.chans[y]->readchunk, z->span.chans[y]->writechunk);
			}
			dahdi_receive(&z->span);
			dahdi_transmit(&z->span);
			/* Handle all transmissions now */
			ztd_sendmessage(z);
		}
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
static void ztdynamic_run(void)
{
	if (likely(!taskletpending)) {
		taskletpending = 1;
		taskletsched++;
		tasklet_hi_schedule(&ztd_tlet);
	} else {
		txerrors++;
	}
}
#else
#define ztdynamic_run __ztdynamic_run
#endif

static inline struct dahdi_dynamic *dynamic_from_span(struct dahdi_span *span)
{
	return container_of(span, struct dahdi_dynamic, span);
}

void dahdi_dynamic_receive(struct dahdi_span *span, unsigned char *msg, int msglen)
{
	struct dahdi_dynamic *ztd = dynamic_from_span(span);
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
		if (newerr != ztd->err) {
			printk(KERN_NOTICE "Span %s: Insufficient samples for header (only %d)\n", span->name, msglen);
		}
		ztd->err = newerr;
		return;
	}
	
	/* First, check the chunksize */
	if (unlikely(*msg != DAHDI_CHUNKSIZE)) {
		rcu_read_unlock();
		newerr = ERR_NSAMP | msg[0];
		if (newerr != 	ztd->err) {
			printk(KERN_NOTICE "Span %s: Expected %d samples, but receiving %d\n", span->name, DAHDI_CHUNKSIZE, msg[0]);
		}
		ztd->err = newerr;
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
		if (newerr != ztd->err) {
			printk(KERN_NOTICE "Span %s: Expected %d channels, but receiving %d\n", span->name, span->channels, nchans);
		}
		ztd->err = newerr;
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
	if (sflags & ZTD_FLAG_SIGBITS_PRESENT) {
		/* Account for sigbits -- one short per 4 channels*/
		xlen += ((nchans + 3) / 4) * 2;
	}

	if (unlikely(xlen != msglen)) {
		rcu_read_unlock();
		newerr = ERR_LEN | xlen;
		if (newerr != ztd->err) {
			printk(KERN_NOTICE "Span %s: Expected message size %d, but was %d instead\n", span->name, xlen, msglen);
		}
		ztd->err = newerr;
		return;
	}

	bits = 0;

	/* Record sigbits if present */
	if (sflags & ZTD_FLAG_SIGBITS_PRESENT) {
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

	master = ztd->master;
	
	rxcnt = ztd->rxcnt;
	ztd->rxcnt = rxpos+1;

	/* Keep track of last received packet */
	ztd->rxjif = jiffies;

	rcu_read_unlock();

	/* Check for Yellow alarm */
	newalarm = span->alarms & ~(DAHDI_ALARM_YELLOW | DAHDI_ALARM_RED);
	if (sflags & ZTD_FLAG_YELLOW_ALARM)
		newalarm |= DAHDI_ALARM_YELLOW;

	if (newalarm != span->alarms) {
		span->alarms = newalarm;
		dahdi_alarm_notify(span);
		checkmaster();
	}

	/* note if we had a missing packet */
	if (unlikely(rxpos != rxcnt))
		printk(KERN_NOTICE "Span %s: Expected seq no %d, but received %d instead\n", span->name, rxcnt, rxpos);

	/* If this is our master span, then run everything */
	if (master)
		ztdynamic_run();
}

static void dynamic_destroy(struct dahdi_dynamic *z)
{
	unsigned int x;

	/* Unregister span if appropriate */
	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &z->span.flags))
		dahdi_unregister(&z->span);

	/* Destroy the pvt stuff if there */
	if (z->pvt)
		z->driver->destroy(z->pvt);

	/* Free message buffer if appropriate */
	if (z->msgbuf)
		kfree(z->msgbuf);

	/* Free channels */
	for (x = 0; x < z->span.channels; x++) {
		kfree(z->chans[x]);
	}

	/* Free z */
	kfree(z);

	checkmaster();
}

static struct dahdi_dynamic *find_dynamic(struct dahdi_dynamic_span *zds)
{
	struct dahdi_dynamic *z = NULL, *found = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(z, &dspan_list, list) {
		if (!strcmp(z->dname, zds->driver) &&
				!strcmp(z->addr, zds->addr)) {
			found = z;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

static struct dahdi_dynamic_driver *find_driver(char *name)
{
	struct dahdi_dynamic_driver *ztd, *found = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(ztd, &driver_list, list) {
		/* here's our driver */
		if (!strcmp(name, ztd->name)) {
			found = ztd;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

static int destroy_dynamic(struct dahdi_dynamic_span *zds)
{
	unsigned long flags;
	struct dahdi_dynamic *z;

	z = find_dynamic(zds);
	if (unlikely(!z)) {
		return -EINVAL;
	}

	if (z->usecount) {
		printk(KERN_NOTICE "Attempt to destroy dynamic span while it is in use\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&dspan_lock, flags);
	list_del_rcu(&z->list);
	spin_unlock_irqrestore(&dspan_lock, flags);
	synchronize_rcu();

	/* Destroy it */
	dynamic_destroy(z);

	return 0;
}

static int ztd_rbsbits(struct dahdi_chan *chan, int bits)
{
	/* Don't have to do anything */
	return 0;
}

static int ztd_open(struct dahdi_chan *chan)
{
	struct dahdi_dynamic *z = dynamic_from_span(chan->span);
	if (likely(z)) {
		if (unlikely(z->dead))
			return -ENODEV;
		z->usecount++;
	}
	return 0;
}

static int ztd_chanconfig(struct dahdi_chan *chan, int sigtype)
{
	return 0;
}

static int ztd_close(struct dahdi_chan *chan)
{
	struct dahdi_dynamic *z = dynamic_from_span(chan->span);
	if (z) {
		z->usecount--;
		if (z->dead && !z->usecount)
			dynamic_destroy(z);
	}
	return 0;
}

static const struct dahdi_span_ops dynamic_ops = {
	.owner = THIS_MODULE,
	.rbsbits = ztd_rbsbits,
	.open = ztd_open,
	.close = ztd_close,
	.chanconfig = ztd_chanconfig,
};

static int create_dynamic(struct dahdi_dynamic_span *zds)
{
	struct dahdi_dynamic *z;
	struct dahdi_dynamic_driver *ztd;
	unsigned long flags;
	int x;
	int bufsize;

	if (zds->numchans < 1) {
		printk(KERN_NOTICE "Can't be less than 1 channel (%d)!\n", zds->numchans);
		return -EINVAL;
	}
	if (zds->numchans >= DAHDI_DYNAMIC_MAX_CHANS) {
		printk(KERN_NOTICE "Can't create dynamic span with greater than %d channels.  See ztdynamic.c and increase DAHDI_DYNAMIC_MAX_CHANS\n", zds->numchans);
		return -EINVAL;
	}

	z = find_dynamic(zds);
	if (z)
		return -EEXIST;

	/* Allocate memory */
	z = (struct dahdi_dynamic *) kmalloc(sizeof(struct dahdi_dynamic), GFP_KERNEL);
	if (!z) {
		return -ENOMEM;
	}

	/* Zero it out */
	memset(z, 0, sizeof(*z));

	for (x = 0; x < zds->numchans; x++) {
		if (!(z->chans[x] = kmalloc(sizeof(*z->chans[x]), GFP_KERNEL))) {
			dynamic_destroy(z);
			return -ENOMEM;
		}

		memset(z->chans[x], 0, sizeof(*z->chans[x]));
	}

	/* Allocate message buffer with sample space and header space */
	bufsize = zds->numchans * DAHDI_CHUNKSIZE + zds->numchans / 4 + 48;

	z->msgbuf = kmalloc(bufsize, GFP_KERNEL);

	if (!z->msgbuf) {
		dynamic_destroy(z);
		return -ENOMEM;
	}
	
	/* Zero out -- probably not needed but why not */
	memset(z->msgbuf, 0, bufsize);

	/* Setup parameters properly assuming we're going to be okay. */
	dahdi_copy_string(z->dname, zds->driver, sizeof(z->dname));
	dahdi_copy_string(z->addr, zds->addr, sizeof(z->addr));
	z->timing = zds->timing;
	sprintf(z->span.name, "DYN/%s/%s", zds->driver, zds->addr);
	sprintf(z->span.desc, "Dynamic '%s' span at '%s'", zds->driver, zds->addr);
	z->span.channels = zds->numchans;
	z->span.deflaw = DAHDI_LAW_MULAW;
	z->span.flags |= DAHDI_FLAG_RBS;
	z->span.chans = z->chans;
	z->span.ops = &dynamic_ops;
	for (x=0; x < z->span.channels; x++) {
		sprintf(z->chans[x]->name, "DYN/%s/%s/%d", zds->driver, zds->addr, x+1);
		z->chans[x]->sigcap = DAHDI_SIG_EM | DAHDI_SIG_CLEAR | DAHDI_SIG_FXSLS |
				      DAHDI_SIG_FXSKS | DAHDI_SIG_FXSGS | DAHDI_SIG_FXOLS |
				      DAHDI_SIG_FXOKS | DAHDI_SIG_FXOGS | DAHDI_SIG_SF | 
				      DAHDI_SIG_DACS_RBS | DAHDI_SIG_CAS;
		z->chans[x]->chanpos = x + 1;
		z->chans[x]->pvt = z;
	}
	
	ztd = find_driver(zds->driver);
	if (!ztd) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,70)
		char fn[80];
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,70)
		request_module("dahdi_dynamic_%s", zds->driver);
#else
		sprintf(fn, "dahdi_dynamic_%s", zds->driver);
		request_module(fn);
#endif
		ztd = find_driver(zds->driver);
	}


	/* Another race -- should let the module get unloaded while we
	   have it here */
	if (!ztd) {
		printk(KERN_NOTICE "No such driver '%s' for dynamic span\n", zds->driver);
		dynamic_destroy(z);
		return -EINVAL;
	}

	/* Create the stuff */
	z->pvt = ztd->create(&z->span, z->addr);
	if (!z->pvt) {
		printk(KERN_NOTICE "Driver '%s' (%s) rejected address '%s'\n", ztd->name, ztd->desc, z->addr);
		/* Creation failed */
		return -EINVAL;
	}

	/* Remember the driver */
	z->driver = ztd;

	/* Whee!  We're created.  Now register the span */
	if (dahdi_register(&z->span, 0)) {
		printk(KERN_NOTICE "Unable to register span '%s'\n", z->span.name);
		dynamic_destroy(z);
		return -EINVAL;
	}

	spin_lock_irqsave(&dspan_lock, flags);
	list_add_rcu(&z->list, &dspan_list);
	spin_unlock_irqrestore(&dspan_lock, flags);

	checkmaster();

	/* All done */
	return z->span.spanno;

}

#ifdef ENABLE_TASKLETS
static void ztd_tasklet(unsigned long data)
{
	taskletrun++;
	if (taskletpending) {
		taskletexec++;
		__ztdynamic_run();
	}
	taskletpending = 0;
}
#endif

static int ztdynamic_ioctl(unsigned int cmd, unsigned long data)
{
	struct dahdi_dynamic_span zds;
	int res;
	switch(cmd) {
	case 0:
		/* This is called just before rotation.  If none of our
		   spans are pulling timing, then now is the time to process
		   them */
		if (!hasmaster)
			ztdynamic_run();
		return 0;
	case DAHDI_DYNAMIC_CREATE:
		if (copy_from_user(&zds, (__user const void *) data, sizeof(zds)))
			return -EFAULT;
		if (debug)
			printk(KERN_DEBUG "Dynamic Create\n");
		res = create_dynamic(&zds);
		if (res < 0)
			return res;
		zds.spanno = res;
		/* Let them know the new span number */
		if (copy_to_user((__user void *) data, &zds, sizeof(zds)))
			return -EFAULT;
		return 0;
	case DAHDI_DYNAMIC_DESTROY:
		if (copy_from_user(&zds, (__user const void *) data, sizeof(zds)))
			return -EFAULT;
		if (debug)
			printk(KERN_DEBUG "Dynamic Destroy\n");
		return destroy_dynamic(&zds);
	}

	return -ENOTTY;
}

int dahdi_dynamic_register(struct dahdi_dynamic_driver *dri)
{
	unsigned long flags;
	int res = 0;

	if (find_driver(dri->name)) {
		res = -1;
	} else {
		spin_lock_irqsave(&driver_lock, flags);
		list_add_rcu(&dri->list, &driver_list);
		spin_unlock_irqrestore(&driver_lock, flags);
	}
	return res;
}

void dahdi_dynamic_unregister(struct dahdi_dynamic_driver *dri)
{
	struct dahdi_dynamic *z;
	unsigned long flags;

	spin_lock_irqsave(&driver_lock, flags);
	list_del_rcu(&dri->list);
	spin_unlock_irqrestore(&driver_lock, flags);
	synchronize_rcu();

	list_for_each_entry(z, &dspan_list, list) {
		if (z->driver == dri) {
			spin_lock_irqsave(&dspan_lock, flags);
			list_del_rcu(&z->list);
			spin_unlock_irqrestore(&dspan_lock, flags);
			synchronize_rcu();

			if (!z->usecount)
				dynamic_destroy(z);
			else
				z->dead = 1;
		}
	}
}

static struct timer_list alarmcheck;

static void check_for_red_alarm(unsigned long ignored)
{
	int newalarm;
	int alarmchanged = 0;
	struct dahdi_dynamic *z;

	rcu_read_lock();
	list_for_each_entry_rcu(z, &dspan_list, list) {
		newalarm = z->span.alarms & ~DAHDI_ALARM_RED;
		/* If nothing received for a second, consider that RED ALARM */
		if ((jiffies - z->rxjif) > 1 * HZ) {
			newalarm |= DAHDI_ALARM_RED;
			if (z->span.alarms != newalarm) {
				z->span.alarms = newalarm;
				dahdi_alarm_notify(&z->span);
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

static int ztdynamic_init(void)
{
	dahdi_set_dynamic_ioctl(ztdynamic_ioctl);

	/* Start process to check for RED ALARM */
	init_timer(&alarmcheck);
	alarmcheck.expires = 0;
	alarmcheck.data = 0;
	alarmcheck.function = check_for_red_alarm;
	/* Check once per second */
	mod_timer(&alarmcheck, jiffies + 1 * HZ);
#ifdef ENABLE_TASKLETS
	tasklet_init(&ztd_tlet, ztd_tasklet, 0);
#endif
	printk(KERN_INFO "DAHDI Dynamic Span support LOADED\n");
	return 0;
}

static void ztdynamic_cleanup(void)
{
#ifdef ENABLE_TASKLETS
	if (taskletpending) {
		tasklet_disable(&ztd_tlet);
		tasklet_kill(&ztd_tlet);
	}
#endif
	dahdi_set_dynamic_ioctl(NULL);
	del_timer(&alarmcheck);
	printk(KERN_INFO "DAHDI Dynamic Span support unloaded\n");
}

module_param(debug, int, 0600);

MODULE_DESCRIPTION("DAHDI Dynamic Span Support");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(ztdynamic_init);
module_exit(ztdynamic_cleanup);
