/*
 * Dummy DAHDI Driver for DAHDI Telephony interface
 *
 * Required: kernel > 2.6.0
 *
 * Written by Robert Pleh <robert.pleh@hermes.si>
 * 2.6 version by Tony Hoyle
 * Unified by Mark Spencer <markster@digium.com>
 *
 * Converted to use HighResTimers on i386 by Jeffery Palmer <jeff@triggerinc.com>
 *
 * Copyright (C) 2002, Hermes Softlab
 * Copyright (C) 2004-2009, Digium, Inc.
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

/*
 * To use the high resolution timers, in your kernel CONFIG_HIGH_RES_TIMERS 
 * needs to be enabled (Processor type and features -> High Resolution 
 * Timer Support), and optionally HPET (Processor type and features -> 
 * HPET Timer Support) provides a better clock source.
 */

#include <linux/version.h>

#if defined(CONFIG_HIGH_RES_TIMERS) && \
	LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define USE_HIGHRESTIMER
#endif

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#if defined(USE_HIGHRESTIMER)
#include <linux/hrtimer.h>
#else
#include <linux/timer.h>
#endif

#include <dahdi/kernel.h>

#ifndef HAVE_HRTIMER_ACCESSORS
#if defined(USE_HIGHRESTIMER) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))
/* Compatibility with new hrtimer interface */
static inline ktime_t hrtimer_get_expires(const struct hrtimer *timer)
{
	return timer->expires;
}

static inline void hrtimer_set_expires(struct hrtimer *timer, ktime_t time)
{
	timer->expires = time;
}
#endif
#endif

struct dahdi_dummy {
	struct dahdi_device *ddev;
	struct dahdi_span span;
	struct dahdi_chan _chan;
	struct dahdi_chan *chan;
#if !defined(USE_HIGHRESTIMER)
	unsigned long calls_since_start;
	struct timespec start_interval;
#endif
};

static struct dahdi_dummy *ztd;

static int debug = 0;

#ifdef USE_HIGHRESTIMER
#define CLOCK_SRC "HRtimer"
static struct hrtimer zaptimer;
#define DAHDI_RATE 1000                     /* DAHDI ticks per second */
#define DAHDI_TIME (1000000 / DAHDI_RATE)  /* DAHDI tick time in us */
#define DAHDI_TIME_NS (DAHDI_TIME * 1000)  /* DAHDI tick time in ns */
#else
#define CLOCK_SRC "Linux26"
static struct timer_list timer;
static atomic_t shutdown;
#define JIFFIES_INTERVAL max(HZ/250, 1) 	/* 4ms is fine for dahdi_dummy */
#endif

/* Different bits of the debug variable: */
#define DEBUG_GENERAL (1 << 0)
#define DEBUG_TICKS   (1 << 1)

#if defined(USE_HIGHRESTIMER)
static enum hrtimer_restart dahdi_dummy_hr_int(struct hrtimer *htmr)
{
	unsigned long overrun;
	
	/* Trigger DAHDI */
	dahdi_receive(&ztd->span);
	dahdi_transmit(&ztd->span);

	/* Overrun should always return 1, since we are in the timer that 
	 * expired.
	 * We should worry if overrun is 2 or more; then we really missed 
	 * a tick */
	overrun = hrtimer_forward(&zaptimer, hrtimer_get_expires(htmr), 
			ktime_set(0, DAHDI_TIME_NS));
	if(overrun > 1) {
		if(printk_ratelimit())
			printk(KERN_NOTICE "dahdi_dummy: HRTimer missed %lu ticks\n", 
					overrun - 1);
	}

	if(debug && DEBUG_TICKS) {
		static int count = 0;
		/* Printk every 5 seconds, good test to see if timer is 
		 * running properly */
		if (count++ % 5000 == 0)
			printk(KERN_DEBUG "dahdi_dummy: 5000 ticks from hrtimer\n");
	}

	/* Always restart the timer */
	return HRTIMER_RESTART;
}
#else
static unsigned long timespec_diff_ms(struct timespec *t0, struct timespec *t1)
{
	long nanosec, sec;
	unsigned long ms;
	sec = (t1->tv_sec - t0->tv_sec);
	nanosec = (t1->tv_nsec - t0->tv_nsec);
	while (nanosec >= NSEC_PER_SEC) {
		nanosec -= NSEC_PER_SEC;
		++sec;
	}
	while (nanosec < 0) {
		nanosec += NSEC_PER_SEC;
		--sec;
	}
	ms = (sec * 1000) + (nanosec / 1000000L);
	return ms;
}

static void dahdi_dummy_timer(unsigned long param)
{
	unsigned long ms_since_start;
	struct timespec now;
	const unsigned long MAX_INTERVAL = 100000L;
	const unsigned long MS_LIMIT = 3000;

	if (!atomic_read(&shutdown))
		mod_timer(&timer, jiffies + JIFFIES_INTERVAL);

	now = current_kernel_time();
	ms_since_start = timespec_diff_ms(&ztd->start_interval, &now);

	/*
	 * If the system time has changed, it is possible for us to be far
	 * behind.  If we are more than MS_LIMIT milliseconds behind, just
	 * reset our time base and continue so that we do not hang the system
	 * here.
	 *
	 */
	if (unlikely((ms_since_start - ztd->calls_since_start) > MS_LIMIT)) {
		if (printk_ratelimit()) {
			printk(KERN_INFO
			       "dahdi_dummy: Detected time shift.\n");
		}
		ztd->calls_since_start = 0;
		ztd->start_interval = now;
		return;
	}

	while (ms_since_start > ztd->calls_since_start) {
		ztd->calls_since_start++;
		dahdi_receive(&ztd->span);
		dahdi_transmit(&ztd->span);
	}

	if (ms_since_start > MAX_INTERVAL) {
		ztd->calls_since_start = 0;
		ztd->start_interval = now;
	}
}
#endif

static const struct dahdi_span_ops dummy_ops = {
	.owner = THIS_MODULE,
};

static int dahdi_dummy_initialize(struct dahdi_dummy *ztd)
{
	int res = 0;
	/* DAHDI stuff */
	ztd->ddev = dahdi_create_device();
	if (!ztd->ddev)
		return -ENOMEM;
	dev_set_name(&ztd->ddev->dev, "dahdi_dummy");
	ztd->chan = &ztd->_chan;
	sprintf(ztd->span.name, "DAHDI_DUMMY/1");
	snprintf(ztd->span.desc, sizeof(ztd->span.desc) - 1, "%s (source: " CLOCK_SRC ") %d", ztd->span.name, 1);
	sprintf(ztd->chan->name, "DAHDI_DUMMY/%d/%d", 1, 0);
	ztd->ddev->devicetype = "DAHDI Dummy Timing";
	ztd->chan->chanpos = 1;
	ztd->span.chans = &ztd->chan;
	ztd->span.channels = 0;		/* no channels on our span */
	ztd->span.deflaw = DAHDI_LAW_MULAW;
	ztd->chan->pvt = ztd;
	ztd->span.ops = &dummy_ops;
	list_add_tail(&ztd->span.device_node, &ztd->ddev->spans);
	res = dahdi_register_device(ztd->ddev, NULL);
	return res;
}

int init_module(void)
{
	int res;
	ztd = kzalloc(sizeof(*ztd), GFP_KERNEL);
	if (ztd == NULL) {
		printk(KERN_ERR "dahdi_dummy: Unable to allocate memory\n");
		return -ENOMEM;
	}

	res = dahdi_dummy_initialize(ztd);
	if (res) {
		printk(KERN_ERR
		       "dahdi_dummy: Unable to intialize DAHDI driver (%d)\n",
		       res);
		kfree(ztd);
		return res;
	}

#if defined(USE_HIGHRESTIMER)
	printk(KERN_DEBUG "dahdi_dummy: Trying to load High Resolution Timer\n");
	hrtimer_init(&zaptimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	printk(KERN_DEBUG "dahdi_dummy: Initialized High Resolution Timer\n");

	/* Set timer callback function */
	zaptimer.function = dahdi_dummy_hr_int;

	printk(KERN_DEBUG "dahdi_dummy: Starting High Resolution Timer\n");
	hrtimer_start(&zaptimer, ktime_set(0, DAHDI_TIME_NS), HRTIMER_MODE_REL);
	printk(KERN_INFO "dahdi_dummy: High Resolution Timer started, good to go\n");
#else
	init_timer(&timer);
	timer.function = dahdi_dummy_timer;
	ztd->start_interval = current_kernel_time();
	timer.expires = jiffies + JIFFIES_INTERVAL;
	atomic_set(&shutdown, 0);
	add_timer(&timer);
#endif

	if (debug)
		printk(KERN_DEBUG "dahdi_dummy: init() finished\n");
	return 0;
}


void cleanup_module(void)
{
#if defined(USE_HIGHRESTIMER)
	/* Stop high resolution timer */
	hrtimer_cancel(&zaptimer);
#else
	atomic_set(&shutdown, 1);
	del_timer_sync(&timer);
#endif
	dahdi_unregister_device(ztd->ddev);
	dahdi_free_device(ztd->ddev);
	kfree(ztd);
	if (debug)
		printk(KERN_DEBUG "dahdi_dummy: cleanup() finished\n");
}

module_param(debug, int, 0600);

MODULE_DESCRIPTION("Timing-Only Driver");
MODULE_AUTHOR("Robert Pleh <robert.pleh@hermes.si>");
MODULE_LICENSE("GPL v2");
