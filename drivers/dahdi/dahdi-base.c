/*
 * DAHDI Telephony Interface Driver
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Special thanks to Steve Underwood <steve@coppice.org>
 * for substantial contributions to signal processing functions
 * in DAHDI and the Zapata library.
 *
 * Yury Bokhoncovich <byg@cf1.ru>
 * Adaptation for 2.4.20+ kernels (HDLC API was changed)
 * The work has been performed as a part of our move
 * from Cisco 3620 to IBM x305 here in F1 Group
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001 - 2011 Digium, Inc.
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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/kmod.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#if defined(HAVE_UNLOCKED_IOCTL) && defined(CONFIG_BKL)
#include <linux/smp_lock.h>
#endif

#include <linux/ppp_defs.h>

#include <asm/atomic.h>

#define DAHDI_PRINK_MACROS_USE_debug
#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)

/* Grab fasthdlc with tables */
#define FAST_HDLC_NEED_TABLES
#include <dahdi/kernel.h>
#include "ecdis.h"
#include "dahdi.h"

#ifdef CONFIG_DAHDI_PPP
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_ppp.h>
#endif

#ifdef CONFIG_DAHDI_NET
#include <linux/netdevice.h>
#endif

#include "hpec/hpec_user.h"

#include <stdbool.h>

#if defined(EMPULSE) && defined(EMFLASH)
#error "You cannot define both EMPULSE and EMFLASH"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
#ifndef CONFIG_BKL
#warning "No CONFIG_BKL is an experimental configuration."
#endif
#endif

/* Get helper arithmetic */
#include "arith.h"
#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
#include <asm/i387.h>
#endif

#define hdlc_to_chan(h) (((struct dahdi_hdlc *)(h))->chan)
#define netdev_to_chan(h) (((struct dahdi_hdlc *)(dev_to_hdlc(h)->priv))->chan)
#define chan_to_netdev(h) ((h)->hdlcnetdev->netdev)

/* macro-oni for determining a unit (channel) number */
#define	UNIT(file) MINOR(file->f_dentry->d_inode->i_rdev)

EXPORT_SYMBOL(dahdi_transcode_fops);
EXPORT_SYMBOL(dahdi_init_tone_state);
EXPORT_SYMBOL(dahdi_mf_tone);
EXPORT_SYMBOL(__dahdi_mulaw);
EXPORT_SYMBOL(__dahdi_alaw);
#ifdef CONFIG_CALC_XLAW
EXPORT_SYMBOL(__dahdi_lineartoulaw);
EXPORT_SYMBOL(__dahdi_lineartoalaw);
#else
EXPORT_SYMBOL(__dahdi_lin2mu);
EXPORT_SYMBOL(__dahdi_lin2a);
#endif
EXPORT_SYMBOL(dahdi_rbsbits);
EXPORT_SYMBOL(dahdi_qevent_nolock);
EXPORT_SYMBOL(dahdi_qevent_lock);
EXPORT_SYMBOL(dahdi_hooksig);
EXPORT_SYMBOL(dahdi_alarm_notify);
EXPORT_SYMBOL(dahdi_hdlc_abort);
EXPORT_SYMBOL(dahdi_hdlc_finish);
EXPORT_SYMBOL(dahdi_hdlc_getbuf);
EXPORT_SYMBOL(dahdi_hdlc_putbuf);
EXPORT_SYMBOL(dahdi_alarm_channel);

EXPORT_SYMBOL(dahdi_register_echocan_factory);
EXPORT_SYMBOL(dahdi_unregister_echocan_factory);

EXPORT_SYMBOL(dahdi_set_hpec_ioctl);

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *root_proc_entry;
#endif

static int deftaps = 64;

int debug;
#define DEBUG_MAIN	(1 << 0)
#define DEBUG_RBS	(1 << 5)

static int hwec_overrides_swec = 1;

/*!
 * \brief states for transmit signalling
 */
enum dahdi_txstate {
	DAHDI_TXSTATE_ONHOOK,
	DAHDI_TXSTATE_OFFHOOK,
	DAHDI_TXSTATE_START,
	DAHDI_TXSTATE_PREWINK,
	DAHDI_TXSTATE_WINK,
	DAHDI_TXSTATE_PREFLASH,
	DAHDI_TXSTATE_FLASH,
	DAHDI_TXSTATE_DEBOUNCE,
	DAHDI_TXSTATE_AFTERSTART,
	DAHDI_TXSTATE_RINGON,
	DAHDI_TXSTATE_RINGOFF,
	DAHDI_TXSTATE_KEWL,
	DAHDI_TXSTATE_AFTERKEWL,
	DAHDI_TXSTATE_PULSEBREAK,
	DAHDI_TXSTATE_PULSEMAKE,
	DAHDI_TXSTATE_PULSEAFTER,
};

typedef short sumtype[DAHDI_MAX_CHUNKSIZE];

static sumtype sums[(DAHDI_MAX_CONF + 1) * 3];

/* Translate conference aliases into actual conferences
   and vice-versa */
static short confalias[DAHDI_MAX_CONF + 1];
static short confrev[DAHDI_MAX_CONF + 1];

static sumtype *conf_sums_next;
static sumtype *conf_sums;
static sumtype *conf_sums_prev;

static struct dahdi_span *master;
struct file_operations *dahdi_transcode_fops = NULL;

#ifdef CONFIG_DAHDI_CORE_TIMER

static struct core_timer {
	struct timer_list timer;
	struct timespec start_interval;
	unsigned long interval;
	atomic_t count;
	atomic_t shutdown;
	atomic_t last_count;
} core_timer;

#endif /* CONFIG_DAHDI_CORE_TIMER */


enum dahdi_digit_mode {
	DIGIT_MODE_DTMF,
	DIGIT_MODE_MFR1,
	DIGIT_MODE_PULSE,
	DIGIT_MODE_MFR2_FWD,
	DIGIT_MODE_MFR2_REV,
};

/* At the end of silence, the tone stops */
static struct dahdi_tone dtmf_silence = {
	.tonesamples = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_DTMF_LENGTH),
};

/* At the end of silence, the tone stops */
static struct dahdi_tone mfr1_silence = {
	.tonesamples = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_MFR1_LENGTH),
};

/* At the end of silence, the tone stops */
static struct dahdi_tone mfr2_silence = {
	.tonesamples = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_MFR2_LENGTH),
};

/* A pause in the dialing */
static struct dahdi_tone tone_pause = {
	.tonesamples = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_PAUSE_LENGTH),
};

static struct dahdi_dialparams global_dialparams = {
	.dtmf_tonelen = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_DTMF_LENGTH),
	.mfv1_tonelen = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_MFR1_LENGTH),
	.mfr2_tonelen = DAHDI_MS_TO_SAMPLES(DAHDI_CONFIG_DEFAULT_MFR2_LENGTH),
};

static DEFINE_MUTEX(global_dialparamslock);

static int dahdi_chan_ioctl(struct file *file, unsigned int cmd, unsigned long data);

#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
#if (defined(CONFIG_X86) && !defined(CONFIG_X86_64)) || defined(CONFIG_I386)
struct fpu_save_buf {
	unsigned long cr0;
	unsigned long fpu_buf[128];
};

static DEFINE_PER_CPU(struct fpu_save_buf, fpu_buf);

/** dahdi_kernel_fpu_begin() - Save floating point registers
 *
 * This function is similar to kernel_fpu_begin() . However it is
 * designed to work in an interrupt context. Restoring must be done with
 * dahdi_kernel_fpu_end().
 *
 * Furthermore, the whole code between the call to
 * dahdi_kernel_fpu_begin() and dahdi_kernel_fpu_end() must reside
 * inside a spinlock. Otherwise the context might be restored to the
 * wrong process.
 *
 * Current implementation is x86/ia32-specific and will not even build on 
 * x86_64)
 * */
static inline void dahdi_kernel_fpu_begin(void)
{
	struct fpu_save_buf *buf = &__get_cpu_var(fpu_buf);
	__asm__ __volatile__ ("movl %%cr0,%0; clts" : "=r" (buf->cr0));
	__asm__ __volatile__ ("fnsave %0" : "=m" (buf->fpu_buf));
}

/** dahdi_kernel_fpu_end() - restore floating point context
 *
 * Must be used with context saved by dahdi_kernel_fpu_begin(). See its
 * documentation for further information.
 */
static inline void dahdi_kernel_fpu_end(void)
{
	struct fpu_save_buf *buf = &__get_cpu_var(fpu_buf);
	__asm__ __volatile__ ("frstor %0" : "=m" (buf->fpu_buf));
	__asm__ __volatile__ ("movl %0,%%cr0" : : "r" (buf->cr0));
}

#else /* We haven't fixed FP context saving/restoring yet */
/* Very strange things can happen when the context is not properly
 * restored. OTOH, some people do report success with this. Hence we
 * so far just issue a warning */
#warning CONFIG_DAHDI_MMX may behave randomly on this platform
#define dahdi_kernel_fpu_begin kernel_fpu_begin
#define dahdi_kernel_fpu_end   kernel_fpu_end
#endif

#endif

struct dahdi_timer {
	int ms;			/* Countdown */
	int pos;		/* Position */
	int ping;		/* Whether we've been ping'd */
	int tripped;	/* Whether we're tripped */
	struct list_head list;
	wait_queue_head_t sel;
};

static LIST_HEAD(dahdi_timers);

static DEFINE_SPINLOCK(dahdi_timer_lock);

#define DEFAULT_TONE_ZONE (-1)

struct dahdi_zone {
	int ringcadence[DAHDI_MAX_CADENCE];
	struct dahdi_tone *tones[DAHDI_TONE_MAX];
	/* Each of these is a circular list
	   of dahdi_tones to generate what we
	   want.  Use NULL if the tone is
	   unavailable */
	struct dahdi_tone dtmf[16];		/* DTMF tones for this zone, with desired length */
	struct dahdi_tone dtmf_continuous[16];	/* DTMF tones for this zone, continuous play */
	struct dahdi_tone mfr1[15];		/* MFR1 tones for this zone, with desired length */
	struct dahdi_tone mfr2_fwd[15];		/* MFR2 FWD tones for this zone, with desired length */
	struct dahdi_tone mfr2_rev[15];		/* MFR2 REV tones for this zone, with desired length */
	struct dahdi_tone mfr2_fwd_continuous[16];	/* MFR2 FWD tones for this zone, continuous play */
	struct dahdi_tone mfr2_rev_continuous[16];	/* MFR2 REV tones for this zone, continuous play */
	struct list_head node;
	struct kref refcount;
	const char *name;	/* Informational, only */
	u8 num;
};

static void tone_zone_release(struct kref *kref)
{
	struct dahdi_zone *z = container_of(kref, struct dahdi_zone, refcount);
	kfree(z->name);
	kfree(z);
}

/**
 * tone_zone_put()  - Release the reference on the tone_zone.
 *
 * On old kernels, since kref_put does not have a return value, we'll just
 * always report that we released the memory.
 *
 */
static inline int tone_zone_put(struct dahdi_zone *z)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
	kref_put(&z->refcount, tone_zone_release);
	return 1;
#else
	return kref_put(&z->refcount, tone_zone_release);
#endif
}

static inline void tone_zone_get(struct dahdi_zone *z)
{
	kref_get(&z->refcount);
}

static DEFINE_SPINLOCK(zone_lock);

/* The first zone on the list is the default zone. */
static LIST_HEAD(tone_zones);

static inline struct device *span_device(struct dahdi_span *span)
{
	return &span->parent->dev;
}

/* Protects the span_list and pseudo_chans lists from concurrent access in
 * process context.  The spin_lock is needed to synchronize with the interrupt
 * handler. */
static DEFINE_SPINLOCK(chan_lock);

struct pseudo_chan {
	struct dahdi_chan chan;
	struct list_head node;
};

static inline struct pseudo_chan *chan_to_pseudo(struct dahdi_chan *chan)
{
	return container_of(chan, struct pseudo_chan, chan);
}

enum { FIRST_PSEUDO_CHANNEL = 0x8000, };
/* This list is protected by the chan_lock. */
static LIST_HEAD(pseudo_chans);

/**
 * is_pseudo_chan() - By definition pseudo channels are not on a span.
 */
static inline bool is_pseudo_chan(const struct dahdi_chan *chan)
{
	return (NULL == chan->span);
}

static DEFINE_MUTEX(registration_mutex);
static LIST_HEAD(span_list);

static unsigned long
__for_each_channel(unsigned long (*func)(struct dahdi_chan *chan,
					 unsigned long data),
		   unsigned long data)
{
	int res;
	struct dahdi_span *s;
	struct pseudo_chan *pseudo;

	list_for_each_entry(s, &span_list, spans_node) {
		unsigned long x;
		for (x = 0; x < s->channels; x++) {
			struct dahdi_chan *const chan = s->chans[x];
			res = func(chan, data);
			if (res)
				return res;
		}
	}

	list_for_each_entry(pseudo, &pseudo_chans, node) {
		res = func(&pseudo->chan, data);
		if (res)
			return res;
	}
	return 0;
}

/**
 * _chan_from_num - Lookup a channel
 *
 * Must be called with the registration_mutex held.
 *
 */
static struct dahdi_chan *_chan_from_num(unsigned int channo)
{
	struct dahdi_span *s;
	struct pseudo_chan *pseudo;

	if (channo >= FIRST_PSEUDO_CHANNEL) {
		list_for_each_entry(pseudo, &pseudo_chans, node) {
			if (pseudo->chan.channo == channo)
				return &pseudo->chan;
		}
		return NULL;
	}

	/* When searching for the channel amongst the spans, we can use the
	 * fact that channels on a span must be numbered consecutively to skip
	 * checking each individual channel. */
	list_for_each_entry(s, &span_list, spans_node) {
		unsigned int basechan;
		struct dahdi_chan *chan;

		if (unlikely(!s->channels))
			continue;

		basechan = s->chans[0]->channo;
		if (channo >= (basechan + s->channels))
			continue;

		/* Since all the spans should be on the list in sorted order,
		 * if channo is less than base chan, the caller must be
		 * looking for a channel that has already been removed. */
		if (unlikely(channo < basechan))
			return NULL;

		chan = s->chans[channo - basechan];
		WARN_ON(chan->channo != channo);
		return chan;
	}

	return NULL;
}

static struct dahdi_chan *chan_from_num(unsigned int channo)
{
	struct dahdi_chan *chan;
	mutex_lock(&registration_mutex);
	chan = _chan_from_num(channo);
	mutex_unlock(&registration_mutex);
	return chan;
}

static inline struct dahdi_chan *chan_from_file(struct file *file)
{
	return (file->private_data) ?
			file->private_data : chan_from_num(UNIT(file));
}

/**
 * _find_span() - Find a span by span number.
 *
 * Must be called with registration_mutex held.
 *
 */
static struct dahdi_span *_find_span(int spanno)
{
	struct dahdi_span *s;
	list_for_each_entry(s, &span_list, spans_node) {
		if (s->spanno == spanno) {
			return s;
		}
	}
	return NULL;
}
/**
 * span_find_and_get() - Search for the span by number, and if found take out
 * a reference on it.
 *
 * When you are no longer using the returned pointer, you must release it with
 * a put_span call.
 *
 */
static struct dahdi_span *span_find_and_get(int spanno)
{
	struct dahdi_span *found;

	mutex_lock(&registration_mutex);
	found = _find_span(spanno);
	if (found && !get_span(found))
		found = NULL;
	mutex_unlock(&registration_mutex);
	return found;
}

static unsigned int span_count(void)
{
	unsigned int count = 0;
	struct dahdi_span *s;
	unsigned long flags;
	spin_lock_irqsave(&chan_lock, flags);
	list_for_each_entry(s, &span_list, spans_node)
		++count;
	spin_unlock_irqrestore(&chan_lock, flags);
	return count;
}

static inline bool can_provide_timing(const struct dahdi_span *const s)
{
	return !s->cannot_provide_timing;
}

static int maxconfs;

short __dahdi_mulaw[256];
short __dahdi_alaw[256];

#ifndef CONFIG_CALC_XLAW
u_char __dahdi_lin2mu[16384];

u_char __dahdi_lin2a[16384];
#endif

static u_char defgain[256];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10)
#define __RW_LOCK_UNLOCKED() RW_LOCK_UNLOCKED
#endif

#define NUM_SIGS	10

static DEFINE_SPINLOCK(ecfactory_list_lock);

static LIST_HEAD(ecfactory_list);

struct ecfactory {
	const struct dahdi_echocan_factory *ec;
	struct list_head list;
};

int dahdi_register_echocan_factory(const struct dahdi_echocan_factory *ec)
{
	struct ecfactory *cur;
	struct ecfactory *new;

	WARN_ON(!ec->owner);

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	INIT_LIST_HEAD(&new->list);

	spin_lock(&ecfactory_list_lock);

	/* make sure it isn't already registered */
	list_for_each_entry(cur, &ecfactory_list, list) {
		if (cur->ec == ec) {
			spin_unlock(&ecfactory_list_lock);
			kfree(new);
			return -EPERM;
		}
	}

	new->ec = ec;
	list_add_tail(&new->list, &ecfactory_list);

	spin_unlock(&ecfactory_list_lock);

	return 0;
}

void dahdi_unregister_echocan_factory(const struct dahdi_echocan_factory *ec)
{
	struct ecfactory *cur, *next;

	spin_lock(&ecfactory_list_lock);

	list_for_each_entry_safe(cur, next, &ecfactory_list, list) {
		if (cur->ec == ec) {
			list_del(&cur->list);
			kfree(cur);
			break;
		}
	}

	spin_unlock(&ecfactory_list_lock);
}

/* Is this span our syncronization master? */
int dahdi_is_sync_master(const struct dahdi_span *span)
{
	return span == master;
}

static inline void rotate_sums(void)
{
	/* Rotate where we sum and so forth */
	static int pos = 0;
	conf_sums_prev = sums + (DAHDI_MAX_CONF + 1) * pos;
	conf_sums = sums + (DAHDI_MAX_CONF + 1) * ((pos + 1) % 3);
	conf_sums_next = sums + (DAHDI_MAX_CONF + 1) * ((pos + 2) % 3);
	pos = (pos + 1) % 3;
	memset(conf_sums_next, 0, maxconfs * sizeof(sumtype));
}

/**
 * is_chan_dacsed() - True if chan is sourcing it's data from another channel.
 *
 */
static inline bool is_chan_dacsed(const struct dahdi_chan *const chan)
{
	return (NULL != chan->dacs_chan);
}

/**
 * can_dacs_chans() - Returns true if it may be possible to dacs two channels.
 *
 */
static bool can_dacs_chans(struct dahdi_chan *dst, struct dahdi_chan *src)
{
	if (src && dst && src->span && dst->span && src->span->ops &&
	    dst->span->ops && src->span->ops->dacs &&
	    (src->span->ops->dacs == dst->span->ops->dacs))
		return true;
	else
		return false;
}

/**
 * dahdi_chan_dacs() - Cross (or uncross) connect two channels.
 * @dst:	Channel on which to transmit the src data.
 * @src:	NULL to disable cross connect, otherwise the source of the
 *		data.
 *
 * This allows those boards that support it to cross connect one channel to
 * another in hardware.  If the cards cannot be crossed, uncross the
 * destination channel by default..
 *
 */
static int dahdi_chan_dacs(struct dahdi_chan *dst, struct dahdi_chan *src)
{
	int ret = 0;
	if (can_dacs_chans(dst, src))
		ret = dst->span->ops->dacs(dst, src);
	else if (dst->span && dst->span->ops->dacs)
		ret = dst->span->ops->dacs(dst, NULL);
	return ret;
}

static void dahdi_disable_dacs(struct dahdi_chan *chan)
{
	dahdi_chan_dacs(chan, NULL);
}

/*!
 * \return quiescent (idle) signalling states, for the various signalling types
 */
static int dahdi_q_sig(struct dahdi_chan *chan)
{
	int	x;
	static const unsigned int in_sig[NUM_SIGS][2] = {
		{ DAHDI_SIG_NONE,  0 },
		{ DAHDI_SIG_EM,    (DAHDI_ABIT << 8) },
		{ DAHDI_SIG_FXSLS, DAHDI_BBIT | (DAHDI_BBIT << 8) },
		{ DAHDI_SIG_FXSGS, DAHDI_ABIT | DAHDI_BBIT | ((DAHDI_ABIT | DAHDI_BBIT) << 8) },
		{ DAHDI_SIG_FXSKS, DAHDI_BBIT | ((DAHDI_ABIT | DAHDI_BBIT) << 8) },
		{ DAHDI_SIG_FXOLS, (DAHDI_ABIT << 8) },
		{ DAHDI_SIG_FXOGS, DAHDI_BBIT | ((DAHDI_ABIT | DAHDI_BBIT) << 8) },
		{ DAHDI_SIG_FXOKS, (DAHDI_ABIT << 8) },
		{ DAHDI_SIG_SF,    0 },
		{ DAHDI_SIG_EM_E1, DAHDI_DBIT | ((DAHDI_ABIT | DAHDI_DBIT) << 8) },
	};

	/* must have span to begin with */
	if (!chan->span)
		return -1;

	/* if RBS does not apply, return error */
	if (!(chan->span->flags & DAHDI_FLAG_RBS) || !chan->span->ops->rbsbits)
		return -1;

	if (chan->sig == DAHDI_SIG_CAS)
		return chan->idlebits;

	for (x = 0; x < NUM_SIGS; x++) {
		if (in_sig[x][0] == chan->sig)
			return in_sig[x][1];
	}

	return -1; /* not found -- error */
}

#ifdef CONFIG_PROC_FS
static const char *sigstr(int sig)
{
	switch (sig) {
	case DAHDI_SIG_FXSLS:
		return "FXSLS";
	case DAHDI_SIG_FXSKS:
		return "FXSKS";
	case DAHDI_SIG_FXSGS:
		return "FXSGS";
	case DAHDI_SIG_FXOLS:
		return "FXOLS";
	case DAHDI_SIG_FXOKS:
		return "FXOKS";
	case DAHDI_SIG_FXOGS:
		return "FXOGS";
	case DAHDI_SIG_EM:
		return "E&M";
	case DAHDI_SIG_EM_E1:
		return "E&M-E1";
	case DAHDI_SIG_CLEAR:
		return "Clear";
	case DAHDI_SIG_HDLCRAW:
		return "HDLCRAW";
	case DAHDI_SIG_HDLCFCS:
		return "HDLCFCS";
	case DAHDI_SIG_HDLCNET:
		return "HDLCNET";
	case DAHDI_SIG_HARDHDLC:
		return "Hardware-assisted HDLC";
	case DAHDI_SIG_MTP2:
		return "MTP2";
	case DAHDI_SIG_SLAVE:
		return "Slave";
	case DAHDI_SIG_CAS:
		return "CAS";
	case DAHDI_SIG_DACS:
		return "DACS";
	case DAHDI_SIG_DACS_RBS:
		return "DACS+RBS";
	case DAHDI_SIG_SF:
		return "SF (ToneOnly)";
	case DAHDI_SIG_NONE:
	default:
		return "Unconfigured";
	}
}

static int fill_alarm_string(char *buf, int count, int alarms)
{
	int len;

	if (alarms <= 0)
		return 0;

	len = snprintf(buf, count, "%s%s%s%s%s%s",
			(alarms & DAHDI_ALARM_BLUE) ? "BLUE " : "",
			(alarms & DAHDI_ALARM_YELLOW) ? "YELLOW " : "",
			(alarms & DAHDI_ALARM_RED) ? "RED " : "",
			(alarms & DAHDI_ALARM_LOOPBACK) ? "LOOP " : "",
			(alarms & DAHDI_ALARM_RECOVER) ? "RECOVERING " : "",
			(alarms & DAHDI_ALARM_NOTOPEN) ? "NOTOPEN " : "");

	return len;
}

/*
 * Sequential proc interface
 */

static void seq_fill_alarm_string(struct seq_file *sfile, int alarms)
{
	char tmp[70];

	if (fill_alarm_string(tmp, sizeof(tmp), alarms))
		seq_printf(sfile, "%s", tmp);
}

static int dahdi_seq_show(struct seq_file *sfile, void *v)
{
	long spanno = (long)sfile->private;
	int x;
	struct dahdi_span *s;

	s = span_find_and_get(spanno);
	if (!s)
		return -ENODEV;

	if (s->name)
		seq_printf(sfile, "Span %d: %s ", s->spanno, s->name);
	if (s->desc)
		seq_printf(sfile, "\"%s\"", s->desc);
	else
		seq_printf(sfile, "\"\"");

	if (dahdi_is_sync_master(s))
		seq_printf(sfile, " (MASTER)");

	if (s->lineconfig) {
		/* framing first */
		if (s->lineconfig & DAHDI_CONFIG_B8ZS)
			seq_printf(sfile, " B8ZS/");
		else if (s->lineconfig & DAHDI_CONFIG_AMI)
			seq_printf(sfile, " AMI/");
		else if (s->lineconfig & DAHDI_CONFIG_HDB3)
			seq_printf(sfile, " HDB3/");
		/* then coding */
		if (s->lineconfig & DAHDI_CONFIG_ESF)
			seq_printf(sfile, "ESF");
		else if (s->lineconfig & DAHDI_CONFIG_D4)
			seq_printf(sfile, "D4");
		else if (s->lineconfig & DAHDI_CONFIG_CCS)
			seq_printf(sfile, "CCS");
		/* E1's can enable CRC checking */
		if (s->lineconfig & DAHDI_CONFIG_CRC4)
			seq_printf(sfile, "/CRC4");
	}

	seq_printf(sfile, " ");

	/* list alarms */
	seq_fill_alarm_string(sfile, s->alarms);
	if (s->syncsrc &&
		(s->syncsrc == s->spanno))
		seq_printf(sfile, "ClockSource ");
	seq_printf(sfile, "\n");
	if (s->count.bpv)
		seq_printf(sfile, "\tBPV count: %d\n", s->count.bpv);
	if (s->count.crc4)
		seq_printf(sfile, "\tCRC4 error count: %d\n", s->count.crc4);
	if (s->count.ebit)
		seq_printf(sfile, "\tE-bit error count: %d\n", s->count.ebit);
	if (s->count.fas)
		seq_printf(sfile, "\tFAS error count: %d\n", s->count.fas);
	if (s->parent->irqmisses)
		seq_printf(sfile, "\tIRQ misses: %d\n", s->parent->irqmisses);
	if (s->timingslips)
		seq_printf(sfile, "\tTiming slips: %d\n", s->timingslips);
	seq_printf(sfile, "\n");

	for (x = 0; x < s->channels; x++) {
		struct dahdi_chan *chan = s->chans[x];

		if (chan->name)
			seq_printf(sfile, "\t%4d %s ", chan->channo,
					chan->name);

		if (chan->sig) {
			if (chan->sig == DAHDI_SIG_SLAVE)
				seq_printf(sfile, "%s ",
						sigstr(chan->master->sig));
			else {
				seq_printf(sfile, "%s ", sigstr(chan->sig));
				if (chan->nextslave &&
					(chan->master->channo == chan->channo))
					seq_printf(sfile, "Master ");
			}
		} else if (!chan->sigcap) {
			seq_printf(sfile, "Reserved ");
		}

		if (test_bit(DAHDI_FLAGBIT_OPEN, &chan->flags))
			seq_printf(sfile, "(In use) ");

#ifdef	OPTIMIZE_CHANMUTE
		if (chan->chanmute)
			seq_printf(sfile, "(no pcm) ");
#endif

		seq_fill_alarm_string(sfile, chan->chan_alarms);

		if (chan->ec_factory)
			seq_printf(sfile, "(EC: %s - %s) ",
					chan->ec_factory->get_name(chan),
					chan->ec_state ? "ACTIVE" : "INACTIVE");

		seq_printf(sfile, "\n");
	}
	put_span(s);
	return 0;
}

static int dahdi_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dahdi_seq_show, PDE(inode)->data);
}

static const struct file_operations dahdi_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= dahdi_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#endif

static int dahdi_first_empty_alias(void)
{
	/* Find the first conference which has no alias pointing to it */
	int x;
	for (x=1;x<DAHDI_MAX_CONF;x++) {
		if (!confrev[x])
			return x;
	}
	return -1;
}

static void recalc_maxconfs(void)
{
	int x;

	for (x = DAHDI_MAX_CONF - 1; x > 0; x--) {
		if (confrev[x]) {
			maxconfs = x + 1;
			return;
		}
	}

	maxconfs = 0;
}

static int dahdi_first_empty_conference(void)
{
	/* Find the first conference which has no alias */
	int x;

	for (x = DAHDI_MAX_CONF - 1; x > 0; x--) {
		if (!confalias[x])
			return x;
	}

	return -1;
}

static int dahdi_get_conf_alias(int x)
{
	int a;

	if (confalias[x])
		return confalias[x];

	/* Allocate an alias */
	a = dahdi_first_empty_alias();
	confalias[x] = a;
	confrev[a] = x;

	/* Highest conference may have changed */
	recalc_maxconfs();

	return a;
}

static unsigned long _chan_in_conf(struct dahdi_chan *chan, unsigned long x)
{
	const int confmode = chan->confmode & DAHDI_CONF_MODE_MASK;
	return (chan && (chan->confna == x) &&
	    (confmode == DAHDI_CONF_CONF ||
	     confmode == DAHDI_CONF_CONFANN ||
	     confmode == DAHDI_CONF_CONFMON ||
	     confmode == DAHDI_CONF_CONFANNMON ||
	     confmode == DAHDI_CONF_REALANDPSEUDO)) ? 1 : 0;
}

static void dahdi_check_conf(int x)
{
	unsigned long res;
	unsigned long flags;

	/* return if no valid conf number */
	if (x <= 0)
		return;

	/* Return if there is no alias */
	if (!confalias[x])
		return;

	spin_lock_irqsave(&chan_lock, flags);
	res = __for_each_channel(_chan_in_conf, x);
	spin_unlock_irqrestore(&chan_lock, flags);
	if (res)
		return;

	/* If we get here, nobody is in the conference anymore.  Clear it out
	   both forward and reverse */
	confrev[confalias[x]] = 0;
	confalias[x] = 0;

	/* Highest conference may have changed */
	recalc_maxconfs();
}

/* enqueue an event on a channel */
static void __qevent(struct dahdi_chan *chan, int event)
{
	/* if full, ignore */
	if ((chan->eventoutidx == 0) && (chan->eventinidx == (DAHDI_MAX_EVENTSIZE - 1)))
		return;

	/* if full, ignore */
	if (chan->eventinidx == (chan->eventoutidx - 1))
		return;

	/* save the event */
	chan->eventbuf[chan->eventinidx++] = event;

	/* wrap the index, if necessary */
	if (chan->eventinidx >= DAHDI_MAX_EVENTSIZE)
		chan->eventinidx = 0;

	/* wake em all up */
	wake_up_interruptible(&chan->waitq);

	return;
}

void dahdi_qevent_nolock(struct dahdi_chan *chan, int event)
{
	__qevent(chan, event);
}

void dahdi_qevent_lock(struct dahdi_chan *chan, int event)
{
	unsigned long flags;
	spin_lock_irqsave(&chan->lock, flags);
	__qevent(chan, event);
	spin_unlock_irqrestore(&chan->lock, flags);
}

static inline void calc_fcs(struct dahdi_chan *ss, int inwritebuf)
{
	int x;
	unsigned int fcs = PPP_INITFCS;
	unsigned char *data = ss->writebuf[inwritebuf];
	int len = ss->writen[inwritebuf];

	/* Not enough space to do FCS calculation */
	if (len < 2)
		return;

	for (x = 0; x < len - 2; x++)
		fcs = PPP_FCS(fcs, data[x]);

	fcs ^= 0xffff;
	/* Send out the FCS */
	data[len - 2] = (fcs & 0xff);
	data[len - 1] = (fcs >> 8) & 0xff;
}

static int dahdi_reallocbufs(struct dahdi_chan *ss, int blocksize, int numbufs)
{
	unsigned char *newtxbuf = NULL;
	unsigned char *newrxbuf = NULL;
	unsigned char *oldtxbuf = NULL;
	unsigned char *oldrxbuf = NULL;
	unsigned long flags;
	int x;

	/* Check numbufs */
	if (numbufs < 2)
		numbufs = 2;

	if (numbufs > DAHDI_MAX_NUM_BUFS)
		numbufs = DAHDI_MAX_NUM_BUFS;

	/* We need to allocate our buffers now */
	if (blocksize) {
		newtxbuf = kzalloc(blocksize * numbufs, GFP_KERNEL);
		if (NULL == newtxbuf)
			return -ENOMEM;
		newrxbuf = kzalloc(blocksize * numbufs, GFP_KERNEL);
		if (NULL == newrxbuf) {
			kfree(newtxbuf);
			return -ENOMEM;
		}
	}

	/* Now that we've allocated our new buffers, we can safely
 	   move things around... */

	spin_lock_irqsave(&ss->lock, flags);

	ss->blocksize = blocksize; /* set the blocksize */
	oldrxbuf = ss->readbuf[0]; /* Keep track of the old buffer */
	oldtxbuf = ss->writebuf[0];
	ss->readbuf[0] = NULL;

	if (newrxbuf) {
		BUG_ON(NULL == newtxbuf);
		for (x = 0; x < numbufs; x++) {
			ss->readbuf[x] = newrxbuf + x * blocksize;
			ss->writebuf[x] = newtxbuf + x * blocksize;
		}
	} else {
		for (x = 0; x < numbufs; x++) {
			ss->readbuf[x] = NULL;
			ss->writebuf[x] = NULL;
		}
	}

	/* Mark all buffers as empty */
	for (x = 0; x < numbufs; x++) {
		ss->writen[x] =
		ss->writeidx[x]=
		ss->readn[x]=
		ss->readidx[x] = 0;
	}

	/* Keep track of where our data goes (if it goes
	   anywhere at all) */
	if (newrxbuf) {
		ss->inreadbuf = 0;
		ss->inwritebuf = 0;
	} else {
		ss->inreadbuf = -1;
		ss->inwritebuf = -1;
	}

	ss->outreadbuf = -1;
	ss->outwritebuf = -1;
	ss->numbufs = numbufs;

	if ((ss->txbufpolicy == DAHDI_POLICY_WHEN_FULL) || (ss->txbufpolicy == DAHDI_POLICY_HALF_FULL))
		ss->txdisable = 1;
	else
		ss->txdisable = 0;

	if (ss->rxbufpolicy == DAHDI_POLICY_WHEN_FULL)
		ss->rxdisable = 1;
	else
		ss->rxdisable = 0;

	spin_unlock_irqrestore(&ss->lock, flags);

	kfree(oldtxbuf);
	kfree(oldrxbuf);

	return 0;
}

static int dahdi_hangup(struct dahdi_chan *chan);
static void dahdi_set_law(struct dahdi_chan *chan, int law);

/* Pull a DAHDI_CHUNKSIZE piece off the queue.  Returns
   0 on success or -1 on failure.  If failed, provides
   silence */
static int __buf_pull(struct confq *q, u_char *data, struct dahdi_chan *c)
{
	int oldoutbuf = q->outbuf;
	/* Ain't nuffin to read */
	if (q->outbuf < 0) {
		if (data)
			memset(data, DAHDI_LIN2X(0,c), DAHDI_CHUNKSIZE);
		return -1;
	}
	if (data)
		memcpy(data, q->buf[q->outbuf], DAHDI_CHUNKSIZE);
	q->outbuf = (q->outbuf + 1) % DAHDI_CB_SIZE;

	/* Won't be nuffin next time */
	if (q->outbuf == q->inbuf) {
		q->outbuf = -1;
	}

	/* If they thought there was no space then
	   there is now where we just read */
	if (q->inbuf < 0)
		q->inbuf = oldoutbuf;
	return 0;
}

/* Returns a place to put stuff, or NULL if there is
   no room */

static u_char *__buf_pushpeek(struct confq *q)
{
	if (q->inbuf < 0)
		return NULL;
	return q->buf[q->inbuf];
}

static u_char *__buf_peek(struct confq *q)
{
	if (q->outbuf < 0)
		return NULL;
	return q->buf[q->outbuf];
}

/* Push something onto the queue, or assume what
   is there is valid if data is NULL */
static int __buf_push(struct confq *q, const u_char *data)
{
	int oldinbuf = q->inbuf;
	if (q->inbuf < 0) {
		return -1;
	}
	if (data)
		/* Copy in the data */
		memcpy(q->buf[q->inbuf], data, DAHDI_CHUNKSIZE);

	/* Advance the inbuf pointer */
	q->inbuf = (q->inbuf + 1) % DAHDI_CB_SIZE;

	if (q->inbuf == q->outbuf) {
		/* No space anymore... */
		q->inbuf = -1;
	}
	/* If they don't think data is ready, let
	   them know it is now */
	if (q->outbuf < 0) {
		q->outbuf = oldinbuf;
	}
	return 0;
}

static void reset_conf(struct dahdi_chan *chan)
{
	int x;

	/* Empty out buffers and reset to initialization */

	for (x = 0; x < DAHDI_CB_SIZE; x++)
		chan->confin.buf[x] = chan->confin.buffer + DAHDI_CHUNKSIZE * x;

	chan->confin.inbuf = 0;
	chan->confin.outbuf = -1;

	for (x = 0; x < DAHDI_CB_SIZE; x++)
		chan->confout.buf[x] = chan->confout.buffer + DAHDI_CHUNKSIZE * x;

	chan->confout.inbuf = 0;
	chan->confout.outbuf = -1;
}


static const struct dahdi_echocan_factory *find_echocan(const char *name)
{
	struct ecfactory *cur;
	char *name_upper;
	char *c;
	const char *d;
	char modname_buf[128] = "dahdi_echocan_";
	unsigned int tried_once = 0;

	name_upper = kmalloc(strlen(name) + 1, GFP_KERNEL);
	if (!name_upper)
		return NULL;

	for (c = name_upper, d = name; *d; c++, d++) {
		*c = toupper(*d);
	}

	*c = '\0';

retry:
	spin_lock(&ecfactory_list_lock);

	list_for_each_entry(cur, &ecfactory_list, list) {
		if (!strcmp(name_upper, cur->ec->get_name(NULL))) {
			if (try_module_get(cur->ec->owner)) {
				spin_unlock(&ecfactory_list_lock);
				kfree(name_upper);
				return cur->ec;
			} else {
				spin_unlock(&ecfactory_list_lock);
				kfree(name_upper);
				return NULL;
			}
		}
	}

	spin_unlock(&ecfactory_list_lock);

	if (tried_once) {
		kfree(name_upper);
		return NULL;
	}

	/* couldn't find it, let's try to load it */

	for (c = &modname_buf[strlen(modname_buf)], d = name; *d; c++, d++) {
		*c = tolower(*d);
	}

	request_module("%s", modname_buf);

	tried_once = 1;

	/* and try one more time */
	goto retry;
}

static void release_echocan(const struct dahdi_echocan_factory *ec)
{
	if (ec)
		module_put(ec->owner);
}

/**
 * is_gain_allocated() - True if gain tables were dynamically allocated.
 * @chan:  The channel to check.
 */
static inline bool is_gain_allocated(const struct dahdi_chan *chan)
{
	return (chan->rxgain && (chan->rxgain != defgain));
}

static const char *hwec_def_name = "HWEC";
static const char *hwec_get_name(const struct dahdi_chan *chan)
{
	if (chan && chan->span && chan->span->ops->echocan_name)
		return chan->span->ops->echocan_name(chan);
	else
		return hwec_def_name;
}

static int hwec_echocan_create(struct dahdi_chan *chan,
	struct dahdi_echocanparams *ecp, struct dahdi_echocanparam *p,
	struct dahdi_echocan_state **ec)
{
	if (chan->span && chan->span->ops->echocan_create)
		return chan->span->ops->echocan_create(chan, ecp, p, ec);
	else
		return -ENODEV;
}

static const struct dahdi_echocan_factory hwec_factory = {
	.get_name = hwec_get_name,
	.owner = THIS_MODULE,
	.echocan_create = hwec_echocan_create,
};

/**
 * dahdi_enable_hw_preechocan - Let the board driver enable hwpreec if possible.
 * @chan:	The channel to monitor.
 *
 * Returns 0 on success, if there is a software echocanceler attached on
 * the channel, or the span does not have an enable_hw_preechocan callback.
 * Otherwise an error code.
 *
 */
static int dahdi_enable_hw_preechocan(struct dahdi_chan *chan)
{
	int res;
	unsigned long flags;

	spin_lock_irqsave(&chan->lock, flags);
	if (chan->ec_factory != &hwec_factory)
		res = -ENODEV;
	else
		res = 0;
	spin_unlock_irqrestore(&chan->lock, flags);

	if (-ENODEV == res)
		return 0;

	if (chan->span->ops->enable_hw_preechocan)
		return chan->span->ops->enable_hw_preechocan(chan);
	else
		return 0;
}

/**
 * dahdi_disable_hw_preechocan - Disable any hardware pre echocan monitoring.
 * @chan:	The channel to stop monitoring.
 *
 * Give the board driver the option to free any resources needed to monitor
 * the preechocan stream.
 *
 */
static void dahdi_disable_hw_preechocan(struct dahdi_chan *chan)
{
	if (chan->span->ops->disable_hw_preechocan)
		chan->span->ops->disable_hw_preechocan(chan);
}

/* 
 * close_channel - close the channel, resetting any channel variables
 * @chan: the dahdi_chan to close
 *
 * This function is called before either the parent span is linked into the
 * span list, or for pseudos, place on the psuedo_list.  Therefore, this
 * function nor it's callers should depend on the channel being findable
 * via those methods.
 */
static void close_channel(struct dahdi_chan *chan)
{
	unsigned long flags;
	const void *rxgain = NULL;
	struct dahdi_echocan_state *ec_state;
	const struct dahdi_echocan_factory *ec_current;
	int oldconf;
	short *readchunkpreec;
#ifdef CONFIG_DAHDI_PPP
	struct ppp_channel *ppp;
#endif

	might_sleep();

	if (chan->conf_chan &&
	    ((DAHDI_CONF_MONITOR_RX_PREECHO == chan->confmode) ||
	     (DAHDI_CONF_MONITOR_TX_PREECHO == chan->confmode) ||
	     (DAHDI_CONF_MONITORBOTH_PREECHO == chan->confmode))) {
		void *readchunkpreec;

		spin_lock_irqsave(&chan->conf_chan->lock, flags);
		readchunkpreec = chan->conf_chan->readchunkpreec;
		chan->conf_chan->readchunkpreec = NULL;
		spin_unlock_irqrestore(&chan->conf_chan->lock, flags);

		if (readchunkpreec) {
			dahdi_disable_hw_preechocan(chan->conf_chan);
			kfree(readchunkpreec);
		}
	}

	/* XXX Buffers should be send out before reallocation!!! XXX */
	if (!(chan->flags & DAHDI_FLAG_NOSTDTXRX))
		dahdi_reallocbufs(chan, 0, 0);
	spin_lock_irqsave(&chan->lock, flags);
#ifdef CONFIG_DAHDI_PPP
	ppp = chan->ppp;
	chan->ppp = NULL;
#endif
	ec_state = chan->ec_state;
	chan->ec_state = NULL;
	ec_current = chan->ec_current;
	chan->ec_current = NULL;
	readchunkpreec = chan->readchunkpreec;
	chan->readchunkpreec = NULL;
	chan->curtone = NULL;
	if (chan->curzone) {
		struct dahdi_zone *zone = chan->curzone;
		chan->curzone = NULL;
		tone_zone_put(zone);
	}
	chan->cadencepos = 0;
	chan->pdialcount = 0;
	dahdi_hangup(chan);
	chan->itimerset = chan->itimer = 0;
	chan->pulsecount = 0;
	chan->pulsetimer = 0;
	chan->ringdebtimer = 0;
	chan->txdialbuf[0] = '\0';
	chan->digitmode = DIGIT_MODE_DTMF;
	chan->dialing = 0;
	chan->afterdialingtimer = 0;
	  /* initialize IO MUX mask */
	chan->iomask = 0;
	/* save old conf number, if any */
	oldconf = chan->confna;
	  /* initialize conference variables */
	chan->_confn = 0;
	chan->confna = 0;
	chan->confmode = 0;
	if ((chan->sig & __DAHDI_SIG_DACS) != __DAHDI_SIG_DACS)
		chan->dacs_chan = NULL;

	chan->confmute = 0;
	chan->gotgs = 0;
	reset_conf(chan);
	chan->dacs_chan = NULL;

	if (is_gain_allocated(chan))
		rxgain = chan->rxgain;

	chan->rxgain = defgain;
	chan->txgain = defgain;
	chan->eventinidx = chan->eventoutidx = 0;
	chan->flags &= ~(DAHDI_FLAG_LOOPED | DAHDI_FLAG_LINEAR | DAHDI_FLAG_PPP | DAHDI_FLAG_SIGFREEZE);

	dahdi_set_law(chan, DAHDI_LAW_DEFAULT);

	memset(chan->conflast, 0, sizeof(chan->conflast));
	memset(chan->conflast1, 0, sizeof(chan->conflast1));
	memset(chan->conflast2, 0, sizeof(chan->conflast2));

	if (chan->span && oldconf)
		dahdi_disable_dacs(chan);

	spin_unlock_irqrestore(&chan->lock, flags);

	if (ec_state) {
		ec_state->ops->echocan_free(chan, ec_state);
		release_echocan(ec_current);
	}

	/* release conference resource, if any to release */
	if (oldconf)
		dahdi_check_conf(oldconf);

	if (rxgain)
		kfree(rxgain);

	if (readchunkpreec) {
		dahdi_disable_hw_preechocan(chan);
		kfree(readchunkpreec);
	}

#ifdef CONFIG_DAHDI_PPP
	if (ppp) {
		tasklet_kill(&chan->ppp_calls);
		skb_queue_purge(&chan->ppp_rq);
		ppp_unregister_channel(ppp);
		kfree(ppp);
	}
#endif

}

static int dahdi_ioctl_freezone(unsigned long data)
{
	struct dahdi_zone *z;
	struct dahdi_zone *found = NULL;
	int num;

	if (get_user(num, (int __user *) data))
		return -EFAULT;

	spin_lock(&zone_lock);
	list_for_each_entry(z, &tone_zones, node) {
		if (z->num == num) {
			found = z;
			break;
		}
	}
	if (found) {
		list_del(&found->node);
	}
	spin_unlock(&zone_lock);

	if (found) {
		if (debug) {
			module_printk(KERN_INFO,
				      "Unregistering tone zone %d (%s)\n",
				      found->num, found->name);
		}
		tone_zone_put(found);
	}
	return 0;
}

static int dahdi_register_tone_zone(struct dahdi_zone *zone)
{
	struct dahdi_zone *cur;
	int res = 0;

	kref_init(&zone->refcount);
	spin_lock(&zone_lock);
	list_for_each_entry(cur, &tone_zones, node) {
		if (cur->num == zone->num) {
			res = -EINVAL;
			break;
		}
	}
	if (!res) {
		list_add_tail(&zone->node, &tone_zones);
		if (debug) {
			module_printk(KERN_INFO,
				      "Registered tone zone %d (%s)\n",
				      zone->num, zone->name);
		}
	}
	spin_unlock(&zone_lock);

	return res;
}

static int start_tone_digit(struct dahdi_chan *chan, int tone)
{
	struct dahdi_tone *playtone = NULL;
	int base, max;

	if (!chan->curzone)
		return -ENODATA;

	switch (chan->digitmode) {
	case DIGIT_MODE_DTMF:
		/* Set dialing so that a dial operation doesn't interrupt this tone */
		chan->dialing = 1;
		base = DAHDI_TONE_DTMF_BASE;
		max = DAHDI_TONE_DTMF_MAX;
		break;
	case DIGIT_MODE_MFR2_FWD:
		base = DAHDI_TONE_MFR2_FWD_BASE;
		max = DAHDI_TONE_MFR2_FWD_MAX;
		break;
	case DIGIT_MODE_MFR2_REV:
		base = DAHDI_TONE_MFR2_REV_BASE;
		max = DAHDI_TONE_MFR2_REV_MAX;
		break;
	default:
		return -EINVAL;
	}

	if ((tone < base) || (tone > max))
		return -EINVAL;

	switch (chan->digitmode) {
	case DIGIT_MODE_DTMF:
		playtone = &chan->curzone->dtmf_continuous[tone - base];
		break;
	case DIGIT_MODE_MFR2_FWD:
		playtone = &chan->curzone->mfr2_fwd_continuous[tone - base];
		break;
	case DIGIT_MODE_MFR2_REV:
		playtone = &chan->curzone->mfr2_rev_continuous[tone - base];
		break;
	}

	if (!playtone || !playtone->tonesamples)
		return -ENOSYS;

	chan->curtone = playtone;

	return 0;
}

static int start_tone(struct dahdi_chan *chan, int tone)
{
	int res = -EINVAL;

	/* Stop the current tone, no matter what */
	chan->tonep = 0;
	chan->curtone = NULL;
	chan->pdialcount = 0;
	chan->txdialbuf[0] = '\0';
	chan->dialing = 0;

	if (tone == -1) {
		/* Just stop the current tone */
		res = 0;
	} else if (!chan->curzone) {
		static int __warnonce = 1;
		if (__warnonce) {
			__warnonce = 0;
			/* The tonezones are loaded by dahdi_cfg based on /etc/dahdi/system.conf. */
			module_printk(KERN_WARNING, "DAHDI: Cannot start tones until tone zone is loaded.\n");
		}
		/* Note that no tone zone exists at the moment */
		res = -ENODATA;
	} else if ((tone >= 0 && tone <= DAHDI_TONE_MAX)) {
		/* Have a tone zone */
		if (chan->curzone->tones[tone]) {
			chan->curtone = chan->curzone->tones[tone];
			res = 0;
		} else { /* Indicate that zone is loaded but no such tone exists */
			res = -ENOSYS;
		}
	} else if (chan->digitmode == DIGIT_MODE_DTMF ||
			chan->digitmode == DIGIT_MODE_MFR2_FWD ||
			chan->digitmode == DIGIT_MODE_MFR2_REV) {
		res = start_tone_digit(chan, tone);
	} else {
		chan->dialing = 0;
		res = -EINVAL;
	}

	if (chan->curtone)
		dahdi_init_tone_state(&chan->ts, chan->curtone);

	return res;
}

static int set_tone_zone(struct dahdi_chan *chan, int zone)
{
	int res = 0;
	struct dahdi_zone *cur;
	struct dahdi_zone *z;
	unsigned long flags;

	z = NULL;
	spin_lock(&zone_lock);
	if ((DEFAULT_TONE_ZONE == zone) && !list_empty(&tone_zones)) {
		z = list_entry(tone_zones.next, struct dahdi_zone, node);
		tone_zone_get(z);
	} else {
		list_for_each_entry(cur, &tone_zones, node) {
			if (cur->num != (u8)zone)
				continue;
			z = cur;
			tone_zone_get(z);
			break;
		}
	}
	spin_unlock(&zone_lock);

	if (unlikely(!z))
		return -ENODATA;

	spin_lock_irqsave(&chan->lock, flags);
	if (chan->curzone) {
		struct dahdi_zone *zone = chan->curzone;
		chan->curzone = NULL;
		tone_zone_put(zone);
	}
	chan->curzone = z;
	memcpy(chan->ringcadence, z->ringcadence, sizeof(chan->ringcadence));
	spin_unlock_irqrestore(&chan->lock, flags);

	return res;
}

static void dahdi_set_law(struct dahdi_chan *chan, int law)
{
	if (DAHDI_LAW_DEFAULT == law) {
		if (chan->deflaw)
			law = chan->deflaw;
		else
			if (chan->span) law = chan->span->deflaw;
			else law = DAHDI_LAW_MULAW;
	}
	if (law == DAHDI_LAW_ALAW) {
		chan->xlaw = __dahdi_alaw;
#ifdef CONFIG_CALC_XLAW
		chan->lineartoxlaw = __dahdi_lineartoalaw;
#else
		chan->lin2x = __dahdi_lin2a;
#endif
	} else {
		chan->xlaw = __dahdi_mulaw;
#ifdef CONFIG_CALC_XLAW
		chan->lineartoxlaw = __dahdi_lineartoulaw;
#else
		chan->lin2x = __dahdi_lin2mu;
#endif
	}
}

/**
 * __dahdi_init_chan - Initialize the channel data structures.
 * @chan:	The channel to initialize
 *
 */
static void __dahdi_init_chan(struct dahdi_chan *chan)
{
	might_sleep();

	spin_lock_init(&chan->lock);
	init_waitqueue_head(&chan->waitq);
	if (!chan->master)
		chan->master = chan;
	if (!chan->readchunk)
		chan->readchunk = chan->sreadchunk;
	if (!chan->writechunk)
		chan->writechunk = chan->swritechunk;
	chan->rxgain = NULL;
	chan->txgain = NULL;
	close_channel(chan);
}

/**
 * dahdi_chan_reg - Mark the channel registered.
 *
 * This must be called after close channel during registration, normally
 * covered by the call to __dahdi_init_chan, to avoid "HDLC hangage"
 */
static inline void dahdi_chan_reg(struct dahdi_chan *chan)
{
	set_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags);
}

/**
 * dahdi_lboname() - Convert line build out number to string.
 *
 */
const char *dahdi_lboname(int lbo)
{
	/* names of tx level settings */
	static const char *const dahdi_txlevelnames[] = {
		"0 db (CSU)/0-133 feet (DSX-1)",
		"133-266 feet (DSX-1)",
		"266-399 feet (DSX-1)",
		"399-533 feet (DSX-1)",
		"533-655 feet (DSX-1)",
		"-7.5db (CSU)",
		"-15db (CSU)",
		"-22.5db (CSU)"
	};

	if ((lbo < 0) || (lbo > 7))
		return "Unknown";
	return dahdi_txlevelnames[lbo];
}
EXPORT_SYMBOL(dahdi_lboname);

#if defined(CONFIG_DAHDI_NET) || defined(CONFIG_DAHDI_PPP)
static inline void print_debug_writebuf(struct dahdi_chan* ss, struct sk_buff *skb, int oldbuf)
{
#ifdef CONFIG_DAHDI_DEBUG
	int x;

	module_printk(KERN_NOTICE, "Buffered %d bytes to go out in buffer %d\n", ss->writen[oldbuf], oldbuf);
	module_printk(KERN_DEBUG "");
	for (x=0;x<ss->writen[oldbuf];x++)
		printk("%02x ", ss->writebuf[oldbuf][x]);
	printk("\n");
#endif
}
#endif

#ifdef CONFIG_DAHDI_NET
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 26)
static inline struct net_device_stats *hdlc_stats(struct net_device *dev)
{
	return &dev->stats;
}
#endif

static int dahdi_net_open(struct net_device *dev)
{
	int res = hdlc_open(dev);
	struct dahdi_chan *ms = netdev_to_chan(dev);

/*	if (!dev->hard_start_xmit) return res; is this really necessary? --byg */
	if (res) /* this is necessary to avoid kernel panic when UNSPEC link encap, proven --byg */
		return res;

	if (!ms) {
		module_printk(KERN_NOTICE, "dahdi_net_open: nothing??\n");
		return -EINVAL;
	}
	if (test_bit(DAHDI_FLAGBIT_OPEN, &ms->flags)) {
		module_printk(KERN_NOTICE, "%s is already open!\n", ms->name);
		return -EBUSY;
	}
	if (!dahdi_have_netdev(ms)) {
		module_printk(KERN_NOTICE, "%s is not a net device!\n", ms->name);
		return -EINVAL;
	}
	ms->txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	ms->rxbufpolicy = DAHDI_POLICY_IMMEDIATE;

	res = dahdi_reallocbufs(ms, DAHDI_DEFAULT_MTU_MRU, DAHDI_DEFAULT_NUM_BUFS);
	if (res)
		return res;

	fasthdlc_init(&ms->rxhdlc, (ms->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
	fasthdlc_init(&ms->txhdlc, (ms->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
	ms->infcs = PPP_INITFCS;

	netif_start_queue(chan_to_netdev(ms));

#ifdef CONFIG_DAHDI_DEBUG
	module_printk(KERN_NOTICE, "DAHDINET: Opened channel %d name %s\n", ms->channo, ms->name);
#endif
	return 0;
}

static int dahdi_register_hdlc_device(struct net_device *dev, const char *dev_name)
{
	int result;

	if (dev_name && *dev_name) {
		if ((result = dev_alloc_name(dev, dev_name)) < 0)
			return result;
	}
	result = register_netdev(dev);
	if (result != 0)
		return -EIO;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,14)
	if (netif_carrier_ok(dev))
		netif_carrier_off(dev); /* no carrier until DCD goes up */
#endif
	return 0;
}

static int dahdi_net_stop(struct net_device *dev)
{
	hdlc_device *h = dev_to_hdlc(dev);
	struct dahdi_hdlc *hdlc = h->priv;

	struct dahdi_chan *ms = hdlc_to_chan(hdlc);
	if (!ms) {
		module_printk(KERN_NOTICE, "dahdi_net_stop: nothing??\n");
		return 0;
	}
	if (!dahdi_have_netdev(ms)) {
		module_printk(KERN_NOTICE, "dahdi_net_stop: %s is not a net device!\n", ms->name);
		return 0;
	}
	/* Not much to do here.  Just deallocate the buffers */
	netif_stop_queue(chan_to_netdev(ms));
	dahdi_reallocbufs(ms, 0, 0);
	hdlc_close(dev);
	return 0;
}

/* kernel 2.4.20+ has introduced attach function, dunno what to do,
 just copy sources from dscc4 to be sure and ready for further mastering,
 NOOP right now (i.e. really a stub)  --byg */
static int dahdi_net_attach(struct net_device *dev, unsigned short encoding,
        unsigned short parity)
{
/*        struct net_device *dev = hdlc_to_dev(hdlc);
        struct dscc4_dev_priv *dpriv = dscc4_priv(dev);

        if (encoding != ENCODING_NRZ &&
            encoding != ENCODING_NRZI &&
            encoding != ENCODING_FM_MARK &&
            encoding != ENCODING_FM_SPACE &&
            encoding != ENCODING_MANCHESTER)
                return -EINVAL;

        if (parity != PARITY_NONE &&
            parity != PARITY_CRC16_PR0_CCITT &&
            parity != PARITY_CRC16_PR1_CCITT &&
            parity != PARITY_CRC32_PR0_CCITT &&
            parity != PARITY_CRC32_PR1_CCITT)
                return -EINVAL;

        dpriv->encoding = encoding;
        dpriv->parity = parity;*/
        return 0;
}

static struct dahdi_hdlc *dahdi_hdlc_alloc(void)
{
	return kzalloc(sizeof(struct dahdi_hdlc), GFP_KERNEL);
}

static int dahdi_xmit(struct sk_buff *skb, struct net_device *dev)
{
	/* FIXME: this construction seems to be not very optimal for me but I
	 * could find nothing better at the moment (Friday, 10PM :( )  --byg
	 * */
	struct dahdi_chan *ss = netdev_to_chan(dev);
	struct net_device_stats *stats = hdlc_stats(dev);

	int retval = 1;
	int x,oldbuf;
	unsigned int fcs;
	unsigned char *data;
	unsigned long flags;
	/* See if we have any buffers */
	spin_lock_irqsave(&ss->lock, flags);
	if (skb->len > ss->blocksize - 2) {
		module_printk(KERN_ERR, "dahdi_xmit(%s): skb is too large (%d > %d)\n", dev->name, skb->len, ss->blocksize -2);
		stats->tx_dropped++;
		retval = 0;
	} else if (ss->inwritebuf >= 0) {
		/* We have a place to put this packet */
		/* XXX We should keep the SKB and avoid the memcpy XXX */
		data = ss->writebuf[ss->inwritebuf];
		memcpy(data, skb->data, skb->len);
		ss->writen[ss->inwritebuf] = skb->len;
		ss->writeidx[ss->inwritebuf] = 0;
		/* Calculate the FCS */
		fcs = PPP_INITFCS;
		for (x=0;x<skb->len;x++)
			fcs = PPP_FCS(fcs, data[x]);
		/* Invert it */
		fcs ^= 0xffff;
		/* Send it out LSB first */
		data[ss->writen[ss->inwritebuf]++] = (fcs & 0xff);
		data[ss->writen[ss->inwritebuf]++] = (fcs >> 8) & 0xff;
		/* Advance to next window */
		oldbuf = ss->inwritebuf;
		ss->inwritebuf = (ss->inwritebuf + 1) % ss->numbufs;

		if (ss->inwritebuf == ss->outwritebuf) {
			/* Whoops, no more space.  */
		    ss->inwritebuf = -1;

		    netif_stop_queue(chan_to_netdev(ss));
		}
		if (ss->outwritebuf < 0) {
			/* Let the interrupt handler know there's
			   some space for us */
			ss->outwritebuf = oldbuf;
		}
		dev->trans_start = jiffies;
		stats->tx_packets++;
		stats->tx_bytes += ss->writen[oldbuf];
		print_debug_writebuf(ss, skb, oldbuf);
		retval = 0;
		/* Free the SKB */
		dev_kfree_skb_any(skb);
	}
	spin_unlock_irqrestore(&ss->lock, flags);
	return retval;
}

static int dahdi_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	return hdlc_ioctl(dev, ifr, cmd);
}

#endif

#ifdef CONFIG_DAHDI_PPP

static int dahdi_ppp_xmit(struct ppp_channel *ppp, struct sk_buff *skb)
{

	/*
	 * If we can't handle the packet right now, return 0.  If we
	 * we handle or drop it, return 1.  Always free if we return
	 * 1 and never if we return 0
         */
	struct dahdi_chan *ss = ppp->private;
	int x,oldbuf;
	unsigned int fcs;
	unsigned char *data;
	unsigned long flags;
	int retval = 0;

	/* See if we have any buffers */
	spin_lock_irqsave(&ss->lock, flags);
	if (!(test_bit(DAHDI_FLAGBIT_OPEN, &ss->flags))) {
		module_printk(KERN_ERR, "Can't transmit on closed channel\n");
		retval = 1;
	} else if (skb->len > ss->blocksize - 4) {
		module_printk(KERN_ERR, "dahdi_ppp_xmit(%s): skb is too large (%d > %d)\n", ss->name, skb->len, ss->blocksize -2);
		retval = 1;
	} else if (ss->inwritebuf >= 0) {
		/* We have a place to put this packet */
		/* XXX We should keep the SKB and avoid the memcpy XXX */
		data = ss->writebuf[ss->inwritebuf];
		/* Start with header of two bytes */
		/* Add "ALL STATIONS" and "UNNUMBERED" */
		data[0] = 0xff;
		data[1] = 0x03;
		ss->writen[ss->inwritebuf] = 2;

		/* Copy real data and increment amount written */
		memcpy(data + 2, skb->data, skb->len);

		ss->writen[ss->inwritebuf] += skb->len;

		/* Re-set index back to zero */
		ss->writeidx[ss->inwritebuf] = 0;

		/* Calculate the FCS */
		fcs = PPP_INITFCS;
		for (x=0;x<skb->len + 2;x++)
			fcs = PPP_FCS(fcs, data[x]);
		/* Invert it */
		fcs ^= 0xffff;

		/* Point past the real data now */
		data += (skb->len + 2);

		/* Send FCS out LSB first */
		data[0] = (fcs & 0xff);
		data[1] = (fcs >> 8) & 0xff;

		/* Account for FCS length */
		ss->writen[ss->inwritebuf]+=2;

		/* Advance to next window */
		oldbuf = ss->inwritebuf;
		ss->inwritebuf = (ss->inwritebuf + 1) % ss->numbufs;

		if (ss->inwritebuf == ss->outwritebuf) {
			/* Whoops, no more space.  */
			ss->inwritebuf = -1;
		}
		if (ss->outwritebuf < 0) {
			/* Let the interrupt handler know there's
			   some space for us */
			ss->outwritebuf = oldbuf;
		}
		print_debug_writebuf(ss, skb, oldbuf);
		retval = 1;
	}
	spin_unlock_irqrestore(&ss->lock, flags);
	if (retval) {
		/* Get rid of the SKB if we're returning non-zero */
		/* N.B. this is called in process or BH context so
		   dev_kfree_skb is OK. */
		dev_kfree_skb(skb);
	}
	return retval;
}

static int dahdi_ppp_ioctl(struct ppp_channel *ppp, unsigned int cmd, unsigned long flags)
{
	return -EIO;
}

static struct ppp_channel_ops ztppp_ops =
{
	.start_xmit = dahdi_ppp_xmit,
	.ioctl      = dahdi_ppp_ioctl,
};

#endif

/**
 * is_monitor_mode() - True if the confmode indicates that one channel is monitoring another.
 *
 */
static bool is_monitor_mode(int confmode)
{
	confmode &= DAHDI_CONF_MODE_MASK;
	if ((confmode == DAHDI_CONF_MONITOR) ||
	    (confmode == DAHDI_CONF_MONITORTX) ||
	    (confmode == DAHDI_CONF_MONITORBOTH) ||
	    (confmode == DAHDI_CONF_MONITOR_RX_PREECHO) ||
	    (confmode == DAHDI_CONF_MONITOR_TX_PREECHO) ||
	    (confmode == DAHDI_CONF_MONITORBOTH_PREECHO)) {
		return true;
	} else {
		return false;
	}
}

static unsigned long _chan_cleanup(struct dahdi_chan *pos, unsigned long data)
{
	unsigned long flags;
	struct dahdi_chan *const chan = (struct dahdi_chan *)data;
	/* Remove anyone pointing to us as master
	   and make them their own thing */
	if (pos->master == chan)
		pos->master = pos;

	if (((pos->confna == chan->channo) &&
	    is_monitor_mode(pos->confmode)) ||
	    (pos->dacs_chan == chan) ||
	    (pos->conf_chan == chan)) {
		/* Take them out of conference with us */
		/* release conference resource if any */
		if (pos->confna)
			dahdi_check_conf(pos->confna);

		dahdi_disable_dacs(pos);
		spin_lock_irqsave(&pos->lock, flags);
		pos->confna = 0;
		pos->_confn = 0;
		pos->confmode = 0;
		pos->conf_chan = NULL;
		pos->dacs_chan = NULL;
		spin_unlock_irqrestore(&pos->lock, flags);
	}

	return 0;
}

static const struct file_operations nodev_fops;

static void dahdi_chan_unreg(struct dahdi_chan *chan)
{
	unsigned long flags;

	might_sleep();

	/* In the case of surprise removal of hardware, make sure any open
	 * file handles to this channel are disassociated with the actual
	 * dahdi_chan. */
	if (chan->file) {
		module_printk(KERN_NOTICE,
			"%s: surprise removal: chan %d\n",
			__func__, chan->channo);
		chan->file->private_data = NULL;
		chan->file->f_op = &nodev_fops;
		/*
		 * From now on, any file_operations for this device
		 * would call the nodev_fops methods.
		 */
	}

	spin_lock_irqsave(&chan->lock, flags);
	release_echocan(chan->ec_factory);
	chan->ec_factory = NULL;
	spin_unlock_irqrestore(&chan->lock, flags);

#ifdef CONFIG_DAHDI_NET
	if (dahdi_have_netdev(chan)) {
		unregister_hdlc_device(chan->hdlcnetdev->netdev);
		free_netdev(chan->hdlcnetdev->netdev);
		kfree(chan->hdlcnetdev);
		chan->hdlcnetdev = NULL;
	}
#endif
	clear_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags);

#ifdef CONFIG_DAHDI_PPP
	if (chan->ppp) {
		module_printk(KERN_NOTICE, "HUH???  PPP still attached??\n");
	}
#endif
	spin_lock_irqsave(&chan_lock, flags);
	__for_each_channel(_chan_cleanup, (unsigned long)chan);
	spin_unlock_irqrestore(&chan_lock, flags);

	chan->channo = -1;

	/* Let processeses out of their poll_wait() */
	wake_up_interruptible(&chan->waitq);

	/* release tone_zone */
	close_channel(chan);

	if (chan->file) {
		if (test_bit(DAHDI_FLAGBIT_OPEN, &chan->flags)) {
			clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
			if (chan->span) {
				if (chan->span->ops->close) {
					int res;

					res = chan->span->ops->close(chan);
					if (res)
						module_printk(KERN_NOTICE,
							"%s: close() failed: %d\n",
							__func__, res);
				}
			}
		}
		msleep(20);
		/*
		 * FIXME: THE BIG SLEEP above, is hiding a terrible
		 * race condition:
		 *  - the module_put() ahead, would allow the low-level driver
		 *    to free the channel.
		 *  - We should make sure no-one reference this channel
		 *    from now on.
		 */
		if (chan->span)
			put_span(chan->span);
	}
}

static ssize_t dahdi_chan_read(struct file *file, char __user *usrbuf,
			       size_t count, loff_t *ppos)
{
	struct dahdi_chan *chan = file->private_data;
	int amnt;
	int res, rv;
	int oldbuf,x;
	unsigned long flags;

	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;

	if (unlikely(!chan)) {
		/*
		 * This should never happen. Surprise device removal
		 * should lead us to the nodev_* file_operations
		 */
		msleep(5);
		module_printk(KERN_ERR, "%s: NODEV\n", __func__);
		return -ENODEV;
	}

	if (unlikely(count < 1))
		return -EINVAL;

	for (;;) {
		spin_lock_irqsave(&chan->lock, flags);
		if (chan->eventinidx != chan->eventoutidx) {
			spin_unlock_irqrestore(&chan->lock, flags);
			return -ELAST /* - chan->eventbuf[chan->eventoutidx]*/;
		}
		res = chan->outreadbuf;
		if (chan->rxdisable)
			res = -1;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0)
			break;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wake up when data is available or when the board driver
		 * unregistered the channel. */
		rv = wait_event_interruptible(chan->waitq,
			(!chan->file->private_data || chan->outreadbuf > -1));
		if (rv)
			return rv;
		if (unlikely(!chan->file->private_data))
			return -ENODEV;
	}
	amnt = count;
	if (chan->flags & DAHDI_FLAG_LINEAR) {
		if (amnt > (chan->readn[res] << 1))
			amnt = chan->readn[res] << 1;
		if (amnt) {
			/* There seems to be a max stack size, so we have
			   to do this in smaller pieces */
			short lindata[128];
			int left = amnt >> 1; /* amnt is in bytes */
			int pos = 0;
			int pass;
			while (left) {
				pass = left;
				if (pass > 128)
					pass = 128;
				for (x = 0; x < pass; x++)
					lindata[x] = DAHDI_XLAW(chan->readbuf[res][x + pos], chan);
				if (copy_to_user(usrbuf + (pos << 1), lindata, pass << 1))
					return -EFAULT;
				left -= pass;
				pos += pass;
			}
		}
	} else {
		if (amnt > chan->readn[res])
			amnt = chan->readn[res];
		if (amnt) {
			if (copy_to_user(usrbuf, chan->readbuf[res], amnt))
				return -EFAULT;
		}
	}
	spin_lock_irqsave(&chan->lock, flags);
	chan->readidx[res] = 0;
	chan->readn[res] = 0;
	oldbuf = res;
	chan->outreadbuf = (res + 1) % chan->numbufs;
	if (chan->outreadbuf == chan->inreadbuf) {
		/* Out of stuff */
		chan->outreadbuf = -1;
		if (chan->rxbufpolicy == DAHDI_POLICY_WHEN_FULL)
			chan->rxdisable = 1;
	}
	if (chan->inreadbuf < 0) {
		/* Notify interrupt handler that we have some space now */
		chan->inreadbuf = oldbuf;
	}
	spin_unlock_irqrestore(&chan->lock, flags);

	return amnt;
}

static int num_filled_bufs(struct dahdi_chan *chan)
{
	int range1, range2;

	if (chan->inwritebuf < 0) {
		return chan->numbufs;
	}

	if (chan->outwritebuf < 0) {
		return 0;
	}

	if (chan->outwritebuf <= chan->inwritebuf) {
		return chan->inwritebuf - chan->outwritebuf;
	}

	/* This means (in > out) and we have wrap around */
	range1 = chan->numbufs - chan->outwritebuf;
	range2 = chan->inwritebuf;

	return range1 + range2;
}

static ssize_t dahdi_chan_write(struct file *file, const char __user *usrbuf,
				size_t count, loff_t *ppos)
{
	unsigned long flags;
	struct dahdi_chan *chan = file->private_data;
	int res, amnt, oldbuf, rv, x;

	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;

	if (unlikely(!chan)) {
		/*
		 * This should never happen. Surprise device removal
		 * should lead us to the nodev_* file_operations
		 */
		msleep(5);
		module_printk(KERN_ERR, "%s: NODEV\n", __func__);
		return -ENODEV;
	}

	if (unlikely(count < 1))
		return -EINVAL;

	for (;;) {
		spin_lock_irqsave(&chan->lock, flags);
		if ((chan->curtone || chan->pdialcount) && !is_pseudo_chan(chan)) {
			chan->curtone = NULL;
			chan->tonep = 0;
			chan->dialing = 0;
			chan->txdialbuf[0] = '\0';
			chan->pdialcount = 0;
		}
		if (chan->eventinidx != chan->eventoutidx) {
			spin_unlock_irqrestore(&chan->lock, flags);
			return -ELAST;
		}
		res = chan->inwritebuf;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0)
			break;
		if (file->f_flags & O_NONBLOCK) {
#ifdef BUFFER_DEBUG
			printk("Error: Nonblock\n");
#endif
			return -EAGAIN;
		}

		/* Wake up when room in the write queue is available or when
		 * the board driver unregistered the channel. */
		rv = wait_event_interruptible(chan->waitq,
			(!chan->file->private_data || chan->inwritebuf > -1));
		if (rv)
			return rv;
		if (unlikely(!chan->file->private_data))
			return -ENODEV;
	}

	amnt = count;
	if (chan->flags & DAHDI_FLAG_LINEAR) {
		if (amnt > (chan->blocksize << 1))
			amnt = chan->blocksize << 1;
	} else {
		if (amnt > chan->blocksize)
			amnt = chan->blocksize;
	}

#ifdef CONFIG_DAHDI_DEBUG
	module_printk(KERN_NOTICE, "dahdi_chan_write(chan: %d, res: %d, outwritebuf: %d amnt: %d\n",
		      chan->channo, res, chan->outwritebuf, amnt);
#endif

	if (amnt) {
		if (chan->flags & DAHDI_FLAG_LINEAR) {
			/* There seems to be a max stack size, so we have
			   to do this in smaller pieces */
			short lindata[128];
			int left = amnt >> 1; /* amnt is in bytes */
			int pos = 0;
			int pass;
			while (left) {
				pass = left;
				if (pass > 128)
					pass = 128;
				if (copy_from_user(lindata, usrbuf + (pos << 1), pass << 1)) {
					return -EFAULT;
				}
				left -= pass;
				for (x = 0; x < pass; x++)
					chan->writebuf[res][x + pos] = DAHDI_LIN2X(lindata[x], chan);
				pos += pass;
			}
			chan->writen[res] = amnt >> 1;
		} else {
			if (copy_from_user(chan->writebuf[res], usrbuf, amnt)) {
				return -EFAULT;
			}
			chan->writen[res] = amnt;
		}
#ifdef CONFIG_DAHDI_ECHOCAN_PROCESS_TX
		if ((chan->ec_state) &&
		    (ECHO_MODE_ACTIVE == chan->ec_state->status.mode) &&
		    (chan->ec_state->ops->echocan_process_tx)) {
			struct dahdi_echocan_state *const ec = chan->ec_state;
			for (x = 0; x < chan->writen[res]; ++x) {
				short tx;
				tx = DAHDI_XLAW(chan->writebuf[res][x], chan);
				ec->ops->echocan_process_tx(ec, &tx, 1);
				chan->writebuf[res][x] = DAHDI_LIN2X((int) tx,
								     chan);
			}
		}
#endif
		chan->writeidx[res] = 0;
		if (chan->flags & DAHDI_FLAG_FCS)
			calc_fcs(chan, res);
		oldbuf = res;
		spin_lock_irqsave(&chan->lock, flags);
		chan->inwritebuf = (res + 1) % chan->numbufs;

		if (chan->inwritebuf == chan->outwritebuf) {
			/* Don't stomp on the transmitter, just wait for them to
			   wake us up */
			chan->inwritebuf = -1;
			/* Make sure the transmitter is transmitting in case of POLICY_WHEN_FULL */
			chan->txdisable = 0;
		}

		if (chan->outwritebuf < 0) {
			/* Okay, the interrupt handler has been waiting for us.  Give them a buffer */
			chan->outwritebuf = oldbuf;
		}

		if ((chan->txbufpolicy == DAHDI_POLICY_HALF_FULL) && (chan->txdisable)) {
			if (num_filled_bufs(chan) >= (chan->numbufs >> 1)) {
#ifdef BUFFER_DEBUG
				printk("Reached buffer fill mark of %d\n", num_filled_bufs(chan));
#endif
				chan->txdisable = 0;
			}
		}

#ifdef BUFFER_DEBUG
		if ((chan->statcount <= 0) || (amnt != 128) || (num_filled_bufs(chan) != chan->lastnumbufs)) {
			printk("amnt: %d Number of filled buffers: %d\n", amnt, num_filled_bufs(chan));
			chan->statcount = 32000;
			chan->lastnumbufs = num_filled_bufs(chan);
		}
#endif

		spin_unlock_irqrestore(&chan->lock, flags);

		if (chan->flags & DAHDI_FLAG_NOSTDTXRX && chan->span->ops->hdlc_hard_xmit)
			chan->span->ops->hdlc_hard_xmit(chan);
	}
	return amnt;
}

static int dahdi_ctl_open(struct file *file)
{
	/* Nothing to do, really */
	return 0;
}

static int dahdi_chan_open(struct file *file)
{
	/* Nothing to do here for now either */
	return 0;
}

static int dahdi_ctl_release(struct file *file)
{
	/* Nothing to do */
	return 0;
}

static int dahdi_chan_release(struct file *file)
{
	/* Nothing to do for now */
	return 0;
}

static void set_txtone(struct dahdi_chan *ss, int fac, int init_v2, int init_v3)
{
	if (fac == 0) {
		ss->v2_1 = 0;
		ss->v3_1 = 0;
		return;
	}
	ss->txtone = fac;
	ss->v1_1 = 0;
	ss->v2_1 = init_v2;
	ss->v3_1 = init_v3;
	return;
}

static void dahdi_rbs_sethook(struct dahdi_chan *chan, int txsig, int txstate,
		int timeout)
{
	static const struct {
		unsigned int sig_type;
		/* Index is dahdi_txsig enum */
		unsigned int bits[DAHDI_TXSIG_TOTAL];
	} outs[NUM_SIGS] = {
		{
			/*
			 * We set the idle case of the DAHDI_SIG_NONE to this pattern to make idle E1 CAS
			 * channels happy. Should not matter with T1, since on an un-configured channel,
			 * who cares what the sig bits are as long as they are stable
			 */
			.sig_type = DAHDI_SIG_NONE,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_ACD,
		}, {
			.sig_type = DAHDI_SIG_EM,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_ABCD,
		}, {
			.sig_type = DAHDI_SIG_FXSLS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_ABCD,
		}, {
			.sig_type = DAHDI_SIG_FXSGS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABCD,
#ifndef CONFIG_CAC_GROUNDSTART
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_AC,
#endif
		}, {
			.sig_type = DAHDI_SIG_FXSKS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_ABCD,
		}, {
			.sig_type = DAHDI_SIG_FXOLS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_BD,
		}, {
			.sig_type = DAHDI_SIG_FXOGS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_BD,
		}, {
			.sig_type = DAHDI_SIG_FXOKS,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_BD,
			.bits[DAHDI_TXSIG_KEWL]    = DAHDI_BITS_ABCD,
		}, {
			.sig_type = DAHDI_SIG_SF,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_BITS_BCD,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_ABCD,
			.bits[DAHDI_TXSIG_KEWL]    = DAHDI_BITS_BCD,
		}, {
			.sig_type = DAHDI_SIG_EM_E1,
			.bits[DAHDI_TXSIG_ONHOOK]  = DAHDI_DBIT,
			.bits[DAHDI_TXSIG_OFFHOOK] = DAHDI_BITS_ABD,
			.bits[DAHDI_TXSIG_START]   = DAHDI_BITS_ABD,
			.bits[DAHDI_TXSIG_KEWL]    = DAHDI_DBIT,
		}
	};
	int x;

	/* if no span, return doing nothing */
	if (!chan->span)
		return;

	if (!(chan->span->flags & DAHDI_FLAG_RBS)) {
		module_printk(KERN_NOTICE, "dahdi_rbs: Tried to set RBS hook state on non-RBS channel %s\n", chan->name);
		return;
	}
	if ((txsig > 3) || (txsig < 0)) {
		module_printk(KERN_NOTICE, "dahdi_rbs: Tried to set RBS hook state %d (> 3) on  channel %s\n", txsig, chan->name);
		return;
	}
	if (!chan->span->ops->rbsbits && !chan->span->ops->hooksig) {
		module_printk(KERN_NOTICE, "dahdi_rbs: Tried to set RBS hook state %d on channel %s while span %s lacks rbsbits or hooksig function\n",
			txsig, chan->name, chan->span->name);
		return;
	}
	/* Don't do anything for RBS */
	if (chan->sig == DAHDI_SIG_DACS_RBS)
		return;
	chan->txstate = txstate;

	/* if tone signalling */
	if (chan->sig == DAHDI_SIG_SF) {
		chan->txhooksig = txsig;
		if (chan->txtone) { /* if set to make tone for tx */
			if ((txsig && !(chan->toneflags & DAHDI_REVERSE_TXTONE)) ||
			 ((!txsig) && (chan->toneflags & DAHDI_REVERSE_TXTONE))) {
				set_txtone(chan,chan->txtone,chan->tx_v2,chan->tx_v3);
			} else {
				set_txtone(chan,0,0,0);
			}
		}
		chan->otimer = timeout * DAHDI_CHUNKSIZE;			/* Otimer is timer in samples */
		return;
	}
	if (chan->span->ops->hooksig) {
		if (chan->txhooksig != txsig) {
			chan->txhooksig = txsig;
			chan->span->ops->hooksig(chan, txsig);
		}
		chan->otimer = timeout * DAHDI_CHUNKSIZE;			/* Otimer is timer in samples */
		return;
	} else {
		for (x = 0; x < NUM_SIGS; x++) {
			if (outs[x].sig_type == chan->sig) {
#ifdef CONFIG_DAHDI_DEBUG
				module_printk(KERN_NOTICE, "Setting bits to %d for channel %s state %d in %d signalling\n", outs[x].bits[txsig], chan->name, txsig, chan->sig);
#endif
				chan->txhooksig = txsig;
				chan->txsig = outs[x].bits[txsig];
				chan->span->ops->rbsbits(chan, chan->txsig);
				chan->otimer = timeout * DAHDI_CHUNKSIZE;	/* Otimer is timer in samples */
				return;
			}
		}
	}
	module_printk(KERN_NOTICE, "dahdi_rbs: Don't know RBS signalling type %d on channel %s\n", chan->sig, chan->name);
}

static int dahdi_cas_setbits(struct dahdi_chan *chan, int bits)
{
	/* if no span, return as error */
	if (!chan->span)
		return -1;
	if (chan->span->ops->rbsbits) {
		chan->txsig = bits;
		chan->span->ops->rbsbits(chan, bits);
	} else {
		module_printk(KERN_NOTICE, "Huh?  CAS setbits, but no RBS bits function\n");
	}

	return 0;
}

static int dahdi_hangup(struct dahdi_chan *chan)
{
	int x, res = 0;

	/* Can't hangup pseudo channels */
	if (!chan->span)
		return 0;

	/* Can't hang up a clear channel */
	if (chan->flags & (DAHDI_FLAG_CLEAR | DAHDI_FLAG_NOSTDTXRX))
		return -EINVAL;

	chan->kewlonhook = 0;

	if ((chan->sig == DAHDI_SIG_FXSLS) || (chan->sig == DAHDI_SIG_FXSKS) ||
			(chan->sig == DAHDI_SIG_FXSGS)) {
		chan->ringdebtimer = RING_DEBOUNCE_TIME;
	}

	if (chan->span->flags & DAHDI_FLAG_RBS) {
		if (chan->sig == DAHDI_SIG_CAS) {
			dahdi_cas_setbits(chan, chan->idlebits);
		} else if ((chan->sig == DAHDI_SIG_FXOKS) && (chan->txstate != DAHDI_TXSTATE_ONHOOK)
			/* if other party is already on-hook we shouldn't do any battery drop */
			&& !((chan->rxhooksig == DAHDI_RXSIG_ONHOOK) && (chan->itimer <= 0))) {
			/* Do RBS signalling on the channel's behalf */
			dahdi_rbs_sethook(chan, DAHDI_TXSIG_KEWL, DAHDI_TXSTATE_KEWL, DAHDI_KEWLTIME);
		} else
			dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_ONHOOK, 0);
	} else {
		/* Let the driver hang up the line if it wants to  */
		if (chan->span->ops->sethook) {
			if (chan->txhooksig != DAHDI_ONHOOK) {
				chan->txhooksig = DAHDI_ONHOOK;
				res = chan->span->ops->sethook(chan, DAHDI_ONHOOK);
			} else
				res = 0;
		}
	}

	/* if not registered yet, just return here */
	if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags))
		return res;

	/* Mark all buffers as empty */
	for (x = 0; x < chan->numbufs; x++) {
		chan->writen[x] =
		chan->writeidx[x]=
		chan->readn[x]=
		chan->readidx[x] = 0;
	}

	if (chan->readbuf[0]) {
		chan->inreadbuf = 0;
		chan->inwritebuf = 0;
	} else {
		chan->inreadbuf = -1;
		chan->inwritebuf = -1;
	}
	chan->outreadbuf = -1;
	chan->outwritebuf = -1;
	chan->dialing = 0;
	chan->afterdialingtimer = 0;
	chan->curtone = NULL;
	chan->pdialcount = 0;
	chan->cadencepos = 0;
	chan->txdialbuf[0] = 0;

	return res;
}

static int initialize_channel(struct dahdi_chan *chan)
{
	int res;
	unsigned long flags;
	const void *rxgain = NULL;
	struct dahdi_echocan_state *ec_state;
	const struct dahdi_echocan_factory *ec_current;

	if ((res = dahdi_reallocbufs(chan, DAHDI_DEFAULT_BLOCKSIZE, DAHDI_DEFAULT_NUM_BUFS)))
		return res;

	spin_lock_irqsave(&chan->lock, flags);

	chan->rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	chan->txbufpolicy = DAHDI_POLICY_IMMEDIATE;

	ec_state = chan->ec_state;
	chan->ec_state = NULL;
	ec_current = chan->ec_current;
	chan->ec_current = NULL;

	chan->txdisable = 0;
	chan->rxdisable = 0;

	chan->digitmode = DIGIT_MODE_DTMF;
	chan->dialing = 0;
	chan->afterdialingtimer = 0;

	chan->cadencepos = 0;
	chan->firstcadencepos = 0; /* By default loop back to first cadence position */

	/* HDLC & FCS stuff */
	fasthdlc_init(&chan->rxhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
	fasthdlc_init(&chan->txhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
	chan->infcs = PPP_INITFCS;

	/* Timings for RBS */
	chan->prewinktime = DAHDI_DEFAULT_PREWINKTIME;
	chan->preflashtime = DAHDI_DEFAULT_PREFLASHTIME;
	chan->winktime = DAHDI_DEFAULT_WINKTIME;
	chan->flashtime = DAHDI_DEFAULT_FLASHTIME;

	if (chan->sig & __DAHDI_SIG_FXO)
		chan->starttime = DAHDI_DEFAULT_RINGTIME;
	else
		chan->starttime = DAHDI_DEFAULT_STARTTIME;
	chan->rxwinktime = DAHDI_DEFAULT_RXWINKTIME;
	chan->rxflashtime = DAHDI_DEFAULT_RXFLASHTIME;
	chan->debouncetime = DAHDI_DEFAULT_DEBOUNCETIME;
	chan->pulsemaketime = DAHDI_DEFAULT_PULSEMAKETIME;
	chan->pulsebreaktime = DAHDI_DEFAULT_PULSEBREAKTIME;
	chan->pulseaftertime = DAHDI_DEFAULT_PULSEAFTERTIME;

	/* Initialize RBS timers */
	chan->itimerset = chan->itimer = chan->otimer = 0;
	chan->ringdebtimer = 0;

	/* Reset conferences */
	reset_conf(chan);

	chan->dacs_chan = NULL;

	/* I/O Mask, etc */
	chan->iomask = 0;
	/* release conference resource if any */
	if (chan->confna)
		dahdi_check_conf(chan->confna);
	if ((chan->sig & __DAHDI_SIG_DACS) != __DAHDI_SIG_DACS) {
		chan->confna = 0;
		chan->confmode = 0;
		chan->conf_chan = NULL;
		dahdi_disable_dacs(chan);
	}
	chan->_confn = 0;
	memset(chan->conflast, 0, sizeof(chan->conflast));
	memset(chan->conflast1, 0, sizeof(chan->conflast1));
	memset(chan->conflast2, 0, sizeof(chan->conflast2));
	chan->confmute = 0;
	chan->gotgs = 0;
	chan->curtone = NULL;
	chan->tonep = 0;
	chan->pdialcount = 0;
	if (is_gain_allocated(chan))
		rxgain = chan->rxgain;
	chan->rxgain = defgain;
	chan->txgain = defgain;
	chan->eventinidx = chan->eventoutidx = 0;
	dahdi_set_law(chan, DAHDI_LAW_DEFAULT);
	dahdi_hangup(chan);

	/* Make sure that the audio flag is cleared on a clear channel */
	if ((chan->sig & DAHDI_SIG_CLEAR) || (chan->sig & DAHDI_SIG_HARDHDLC))
		chan->flags &= ~DAHDI_FLAG_AUDIO;

	if ((chan->sig == DAHDI_SIG_CLEAR) || (chan->sig == DAHDI_SIG_HARDHDLC))
		chan->flags &= ~(DAHDI_FLAG_PPP | DAHDI_FLAG_FCS | DAHDI_FLAG_HDLC);

	chan->flags &= ~DAHDI_FLAG_LINEAR;
	if (chan->curzone) {
		/* Take cadence from tone zone */
		memcpy(chan->ringcadence, chan->curzone->ringcadence, sizeof(chan->ringcadence));
	} else {
		/* Do a default */
		memset(chan->ringcadence, 0, sizeof(chan->ringcadence));
		chan->ringcadence[0] = chan->starttime;
		chan->ringcadence[1] = DAHDI_RINGOFFTIME;
	}

	if (ec_state) {
		ec_state->ops->echocan_free(chan, ec_state);
		release_echocan(ec_current);
	}

	spin_unlock_irqrestore(&chan->lock, flags);

	set_tone_zone(chan, DEFAULT_TONE_ZONE);

	if (rxgain)
		kfree(rxgain);

	return 0;
}

static int dahdi_timing_open(struct file *file)
{
	struct dahdi_timer *t;
	unsigned long flags;

	if (!(t = kzalloc(sizeof(*t), GFP_KERNEL)))
		return -ENOMEM;

	init_waitqueue_head(&t->sel);
	INIT_LIST_HEAD(&t->list);
	file->private_data = t;

	spin_lock_irqsave(&dahdi_timer_lock, flags);
	list_add(&t->list, &dahdi_timers);
	spin_unlock_irqrestore(&dahdi_timer_lock, flags);

	return 0;
}

static int dahdi_timer_release(struct file *file)
{
	struct dahdi_timer *t, *cur, *next;
	unsigned long flags;

	if (!(t = file->private_data))
		return 0;

	spin_lock_irqsave(&dahdi_timer_lock, flags);

	list_for_each_entry_safe(cur, next, &dahdi_timers, list) {
		if (t == cur) {
			list_del(&cur->list);
			break;
		}
	}

	spin_unlock_irqrestore(&dahdi_timer_lock, flags);

	if (!cur) {
		module_printk(KERN_NOTICE, "Timer: Not on list??\n");
		return 0;
	}

	kfree(cur);

	return 0;
}

static const struct file_operations dahdi_chan_fops;

static int dahdi_specchan_open(struct file *file)
{
	int res = 0;
	struct dahdi_chan *const chan = chan_from_file(file);

	if (chan && chan->sig) {
		/* Make sure we're not already open, a net device, or a slave device */
		if (dahdi_have_netdev(chan))
			res = -EBUSY;
		else if (chan->master != chan)
			res = -EBUSY;
		else if ((chan->sig & __DAHDI_SIG_DACS) == __DAHDI_SIG_DACS)
			res = -EBUSY;
		else if (!test_and_set_bit(DAHDI_FLAGBIT_OPEN, &chan->flags)) {
			unsigned long flags;
			res = initialize_channel(chan);
			if (res) {
				/* Reallocbufs must have failed */
				clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
				return res;
			}
			spin_lock_irqsave(&chan->lock, flags);
			if (is_pseudo_chan(chan))
				chan->flags |= DAHDI_FLAG_AUDIO;
			if (chan->span) {
				const struct dahdi_span_ops *const ops =
								chan->span->ops;
				if (!try_module_get(ops->owner)) {
					res = -ENXIO;
				} else if (ops->open) {
					res = ops->open(chan);
					if (res)
						module_put(ops->owner);
				}
			}
			if (!res) {
				chan->file = file;
				file->private_data = chan;
				/* Since we know we're a channel now, we can
				 * update the f_op pointer and bypass a few of
				 * the checks on the minor number. */
				file->f_op = &dahdi_chan_fops;
				spin_unlock_irqrestore(&chan->lock, flags);
			} else {
				spin_unlock_irqrestore(&chan->lock, flags);
				close_channel(chan);
				clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
			}
		} else {
			res = -EBUSY;
		}
	} else
		res = -ENXIO;
	return res;
}

static int dahdi_specchan_release(struct file *file)
{
	int res=0;
	unsigned long flags;
	struct dahdi_chan *chan = chan_from_file(file);

	if (chan) {
		/* Chan lock protects contents against potentially non atomic accesses.
		 * So if the pointer setting is not atomic, we should protect */
#ifdef CONFIG_DAHDI_MIRROR
		if (chan->srcmirror) {
			struct dahdi_chan *const srcmirror = chan->srcmirror;
			spin_lock_irqsave(&srcmirror->lock, flags);
			if (chan == srcmirror->txmirror) {
				module_printk(KERN_INFO, "Chan %d tx mirror " \
					      "to %d stopped\n",
					      srcmirror->txmirror->channo,
					      srcmirror->channo);
				srcmirror->txmirror = NULL;
			}

			if (chan == srcmirror->rxmirror) {
				module_printk(KERN_INFO, "Chan %d rx mirror " \
					      "to %d stopped\n",
					      srcmirror->rxmirror->channo,
					      srcmirror->channo);
				chan->srcmirror->rxmirror = NULL;
			}
			spin_unlock_irqrestore(&chan->srcmirror->lock, flags);
		}
#endif /* CONFIG_DAHDI_MIRROR */

		spin_lock_irqsave(&chan->lock, flags);
		chan->file = NULL;
		file->private_data = NULL;
#ifdef CONFIG_DAHDI_MIRROR
		chan->srcmirror = NULL;
#endif /* CONFIG_DAHDI_MIRROR */

		spin_unlock_irqrestore(&chan->lock, flags);
		close_channel(chan);
		clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
		if (chan->span) {
			struct module *owner = chan->span->ops->owner;

			if (chan->span->ops->close)
				res = chan->span->ops->close(chan);
			module_put(owner);
		}
	} else
		res = -ENXIO;
	return res;
}

static int can_open_timer(void)
{
#ifdef CONFIG_DAHDI_CORE_TIMER
	return 1;
#else
	return (list_empty(&span_list)) ? 0 : 1;
#endif
}

static unsigned int max_pseudo_channels = 512;
static unsigned int num_pseudo_channels;

/**
 * dahdi_alloc_pseudo() - Returns a new pseudo channel.
 *
 * Call with the registration_mutex held since this function will determine a
 * channel number, and must be protected from additional registrations while
 * that is happening.
 *
 */
static struct dahdi_chan *dahdi_alloc_pseudo(struct file *file)
{
	struct pseudo_chan *pseudo;
	unsigned long flags;
	unsigned int channo;
	struct pseudo_chan *p;
	struct list_head *pos = &pseudo_chans;

	/* Don't allow /dev/dahdi/pseudo to open if there is not a timing
	 * source. */
	if (!can_open_timer())
		return NULL;

	if (unlikely(num_pseudo_channels >= max_pseudo_channels))
		return NULL;

	pseudo = kzalloc(sizeof(*pseudo), GFP_KERNEL);
	if (NULL == pseudo)
		return NULL;

	pseudo->chan.sig = DAHDI_SIG_CLEAR;
	pseudo->chan.sigcap = DAHDI_SIG_CLEAR;
	pseudo->chan.flags = DAHDI_FLAG_AUDIO;
	pseudo->chan.span = NULL; /* No span == psuedo channel */

	channo = FIRST_PSEUDO_CHANNEL;
	list_for_each_entry(p, &pseudo_chans, node) {
		if (channo != p->chan.channo)
			break;
		pos = &p->node;
		++channo;
	}

	pseudo->chan.channo = channo;
	pseudo->chan.chanpos = channo - FIRST_PSEUDO_CHANNEL + 1;
	__dahdi_init_chan(&pseudo->chan);
	dahdi_chan_reg(&pseudo->chan);

	snprintf(pseudo->chan.name, sizeof(pseudo->chan.name)-1,
		 "Pseudo/%d", pseudo->chan.chanpos);

	file->private_data = &pseudo->chan;

	/* Once we place the pseudo chan on the list...it's registered and
	 * live. */
	spin_lock_irqsave(&chan_lock, flags);
	++num_pseudo_channels;
	list_add(&pseudo->node, pos);
	spin_unlock_irqrestore(&chan_lock, flags);

	return &pseudo->chan;
}

static void dahdi_free_pseudo(struct dahdi_chan *chan)
{
	struct pseudo_chan *pseudo;
	unsigned long flags;

	if (!chan)
		return;

	mutex_lock(&registration_mutex);
	pseudo = chan_to_pseudo(chan);

	spin_lock_irqsave(&chan_lock, flags);
	list_del(&pseudo->node);
	--num_pseudo_channels;
	spin_unlock_irqrestore(&chan_lock, flags);

	dahdi_chan_unreg(chan);
	mutex_unlock(&registration_mutex);
	kfree(pseudo);
}

static int dahdi_open(struct inode *inode, struct file *file)
{
	int unit = UNIT(file);
	struct dahdi_chan *chan;
	/* Minor 0: Special "control" descriptor */
	if (unit == DAHDI_CTL)
		return dahdi_ctl_open(file);
	if (unit == DAHDI_TRANSCODE) {
		if (!dahdi_transcode_fops) {
			if (request_module("dahdi_transcode")) {
				return -ENXIO;
			}
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		__MOD_INC_USE_COUNT (dahdi_transcode_fops->owner);
#else
		if (!try_module_get(dahdi_transcode_fops->owner)) {
			return -ENXIO;
		}
#endif
		if (dahdi_transcode_fops && dahdi_transcode_fops->open) {
			return dahdi_transcode_fops->open(inode, file);
		} else {
			/* dahdi_transcode module should have exported a
			 * file_operations table. */
			 WARN_ON(1);
		}
		return -ENXIO;
	}
	if (unit == DAHDI_TIMER) {
		if (can_open_timer()) {
			return dahdi_timing_open(file);
		} else {
			return -ENXIO;
		}
	}
	if (unit == DAHDI_CHANNEL)
		return dahdi_chan_open(file);
	if (unit == DAHDI_PSEUDO) {
		mutex_lock(&registration_mutex);
		chan = dahdi_alloc_pseudo(file);
		mutex_unlock(&registration_mutex);
		if (unlikely(!chan))
			return -ENOMEM;
		return dahdi_specchan_open(file);
	}
	return dahdi_specchan_open(file);
}

/**
 * dahdi_ioctl_defaultzone() - Set defzone to the default.
 * @defzone:	The number of the default zone.
 *
 * The default zone is the zone that will be used if the channels request the
 * default zone in dahdi_ioctl_chanconfig.  The first entry on the tone_zones
 * list is the default zone.  This function searches the list for the zone,
 * and if found, moves it to the head of the list.
 */
static int dahdi_ioctl_defaultzone(unsigned long data)
{
	int defzone;
	struct dahdi_zone *cur;
	struct dahdi_zone *dz = NULL;

	if (get_user(defzone, (int __user *)data))
		return -EFAULT;

	spin_lock(&zone_lock);
	list_for_each_entry(cur, &tone_zones, node) {
		if (cur->num != defzone)
			continue;
		dz = cur;
		break;
	}
	if (dz)
		list_move(&dz->node, &tone_zones);
	spin_unlock(&zone_lock);

	return (dz) ? 0 : -EINVAL;
}

/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128

/* The tones to be loaded can (will) be a mix of regular tones,
   DTMF tones and MF tones. We need to load DTMF and MF tones
   a bit differently than regular tones because their storage
   format is much simpler (an array structure field of the zone
   structure, rather an array of pointers).
*/
static int dahdi_ioctl_loadzone(unsigned long data)
{
	struct load_zone_workarea {
		struct dahdi_tone *samples[MAX_TONES];
		short next[MAX_TONES];
		struct dahdi_tone_def_header th;
		struct dahdi_tone_def td;
	} *work;

	size_t space;
	size_t size;
	int res;
	int x;
	void *ptr;
	struct dahdi_zone *z = NULL;
	struct dahdi_tone *t = NULL;
	void __user * user_data = (void __user *)data;
	const unsigned char MAX_ZONE = -1;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;

	if (copy_from_user(&work->th, user_data, sizeof(work->th))) {
		res = -EFAULT;
		goto error_exit;
	}

	if ((work->th.zone < 0) || (work->th.zone > MAX_ZONE)) {
		res = -EINVAL;
		goto error_exit;
	}

	user_data += sizeof(work->th);

	if ((work->th.count < 0) || (work->th.count > MAX_TONES)) {
		module_printk(KERN_NOTICE, "Too many tones included\n");
		res = -EINVAL;
		goto error_exit;
	}

	space = size = sizeof(*z) + work->th.count * sizeof(*t);

	if (size > MAX_SIZE) {
		res = -E2BIG;
		goto error_exit;
	}

	z = ptr = kzalloc(size, GFP_KERNEL);
	if (!z) {
		res = -ENOMEM;
		goto error_exit;
	}

	ptr = (char *) ptr + sizeof(*z);
	space -= sizeof(*z);

	z->name = kasprintf(GFP_KERNEL, work->th.name);
	if (!z->name) {
		res = -ENOMEM;
		goto error_exit;
	}

	for (x = 0; x < DAHDI_MAX_CADENCE; x++)
		z->ringcadence[x] = work->th.ringcadence[x];

	mutex_lock(&global_dialparamslock);
	for (x = 0; x < work->th.count; x++) {
		enum {
			REGULAR_TONE,
			DTMF_TONE,
			MFR1_TONE,
			MFR2_FWD_TONE,
			MFR2_REV_TONE,
		} tone_type;

		if (space < sizeof(*t)) {
			module_printk(KERN_NOTICE, "Insufficient tone zone space\n");
			res = -EINVAL;
			goto unlock_error_exit;
		}

		res = copy_from_user(&work->td, user_data,
				     sizeof(work->td));
		if (res) {
			res = -EFAULT;
			goto unlock_error_exit;
		}

		user_data += sizeof(work->td);

		if ((work->td.tone >= 0) && (work->td.tone < DAHDI_TONE_MAX)) {
			tone_type = REGULAR_TONE;

			t = work->samples[x] = ptr;

			space -= sizeof(*t);
			ptr = (char *) ptr + sizeof(*t);

			/* Remember which sample is work->next */
			work->next[x] = work->td.next;

			/* Make sure the "next" one is sane */
			if ((work->next[x] >= work->th.count) ||
			    (work->next[x] < 0)) {
				module_printk(KERN_NOTICE,
					      "Invalid 'next' pointer: %d\n",
					      work->next[x]);
				res = -EINVAL;
				goto unlock_error_exit;
			}
		} else if ((work->td.tone >= DAHDI_TONE_DTMF_BASE) &&
			   (work->td.tone <= DAHDI_TONE_DTMF_MAX)) {
			tone_type = DTMF_TONE;
			work->td.tone -= DAHDI_TONE_DTMF_BASE;
			t = &z->dtmf[work->td.tone];
		} else if ((work->td.tone >= DAHDI_TONE_MFR1_BASE) &&
			   (work->td.tone <= DAHDI_TONE_MFR1_MAX)) {
			tone_type = MFR1_TONE;
			work->td.tone -= DAHDI_TONE_MFR1_BASE;
			t = &z->mfr1[work->td.tone];
		} else if ((work->td.tone >= DAHDI_TONE_MFR2_FWD_BASE) &&
			   (work->td.tone <= DAHDI_TONE_MFR2_FWD_MAX)) {
			tone_type = MFR2_FWD_TONE;
			work->td.tone -= DAHDI_TONE_MFR2_FWD_BASE;
			t = &z->mfr2_fwd[work->td.tone];
		} else if ((work->td.tone >= DAHDI_TONE_MFR2_REV_BASE) &&
			   (work->td.tone <= DAHDI_TONE_MFR2_REV_MAX)) {
			tone_type = MFR2_REV_TONE;
			work->td.tone -= DAHDI_TONE_MFR2_REV_BASE;
			t = &z->mfr2_rev[work->td.tone];
		} else {
			module_printk(KERN_NOTICE,
				      "Invalid tone (%d) defined\n",
				      work->td.tone);
			res = -EINVAL;
			goto unlock_error_exit;
		}

		t->fac1 = work->td.fac1;
		t->init_v2_1 = work->td.init_v2_1;
		t->init_v3_1 = work->td.init_v3_1;
		t->fac2 = work->td.fac2;
		t->init_v2_2 = work->td.init_v2_2;
		t->init_v3_2 = work->td.init_v3_2;
		t->modulate = work->td.modulate;

		switch (tone_type) {
		case REGULAR_TONE:
			t->tonesamples = work->td.samples;
			if (!z->tones[work->td.tone])
				z->tones[work->td.tone] = t;
			break;
		case DTMF_TONE:
			t->tonesamples = global_dialparams.dtmf_tonelen;
			t->next = &dtmf_silence;
			z->dtmf_continuous[work->td.tone] = *t;
			z->dtmf_continuous[work->td.tone].next =
				&z->dtmf_continuous[work->td.tone];
			break;
		case MFR1_TONE:
			switch (work->td.tone + DAHDI_TONE_MFR1_BASE) {
			case DAHDI_TONE_MFR1_KP:
			case DAHDI_TONE_MFR1_ST:
			case DAHDI_TONE_MFR1_STP:
			case DAHDI_TONE_MFR1_ST2P:
			case DAHDI_TONE_MFR1_ST3P:
				/* signaling control tones are always 100ms */
				t->tonesamples = 100 * DAHDI_CHUNKSIZE;
				break;
			default:
				t->tonesamples = global_dialparams.mfv1_tonelen;
				break;
			}
			t->next = &mfr1_silence;
			break;
		case MFR2_FWD_TONE:
			t->tonesamples = global_dialparams.mfr2_tonelen;
			t->next = &dtmf_silence;
			z->mfr2_fwd_continuous[work->td.tone] = *t;
			z->mfr2_fwd_continuous[work->td.tone].next =
					&z->mfr2_fwd_continuous[work->td.tone];
			break;
		case MFR2_REV_TONE:
			t->tonesamples = global_dialparams.mfr2_tonelen;
			t->next = &dtmf_silence;
			z->mfr2_rev_continuous[work->td.tone] = *t;
			z->mfr2_rev_continuous[work->td.tone].next =
					&z->mfr2_rev_continuous[work->td.tone];
			break;
		}
	}
	mutex_unlock(&global_dialparamslock);

	for (x = 0; x < work->th.count; x++) {
		if (work->samples[x])
			work->samples[x]->next = work->samples[work->next[x]];
	}

	z->num = work->th.zone;

	/* After we call dahdi_register_tone_zone, the only safe way to free
	 * the zone is with a tone_zone_put call. */
	res = dahdi_register_tone_zone(z);
	if (res)
		tone_zone_put(z);

	kfree(work);
	return res;

unlock_error_exit:
	mutex_unlock(&global_dialparamslock);
error_exit:
	if (z)
		kfree(z->name);
	kfree(z);
	kfree(work);
	return res;
}

void dahdi_init_tone_state(struct dahdi_tone_state *ts, struct dahdi_tone *zt)
{
	ts->v1_1 = 0;
	ts->v2_1 = zt->init_v2_1;
	ts->v3_1 = zt->init_v3_1;
	ts->v1_2 = 0;
	ts->v2_2 = zt->init_v2_2;
	ts->v3_2 = zt->init_v3_2;
	ts->modulate = zt->modulate;
}

struct dahdi_tone *dahdi_mf_tone(const struct dahdi_chan *chan, char digit, int digitmode)
{
	unsigned int tone_index;

	if (!chan->curzone) {
		static int __warnonce = 1;
		if (__warnonce) {
			__warnonce = 0;
			/* The tonezones are loaded by dahdi_cfg based on /etc/dahdi/system.conf. */
			module_printk(KERN_WARNING, "Cannot get dtmf tone until tone zone is loaded.\n");
		}
		return NULL;
	}

	switch (digitmode) {
	case DIGIT_MODE_PULSE:
		/* We should only get here with a pulse digit if we need
		 * to "dial" 'W' (wait 0.5 second) 
		 */
		if (digit == 'W')
			return &tone_pause;

		return NULL;
	case DIGIT_MODE_DTMF:
		switch (digit) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			tone_index = DAHDI_TONE_DTMF_0 + (digit - '0');
			break;
		case '*':
			tone_index = DAHDI_TONE_DTMF_s;
			break;
		case '#':
			tone_index = DAHDI_TONE_DTMF_p;
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			tone_index = DAHDI_TONE_DTMF_A + (digit - 'A');
			break;
		case 'W':
			return &tone_pause;
		default:
			return NULL;
		}
		return &chan->curzone->dtmf[tone_index - DAHDI_TONE_DTMF_BASE];
	case DIGIT_MODE_MFR1:
		switch (digit) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			tone_index = DAHDI_TONE_MFR1_0 + (digit - '0');
			break;
		case '*':
			tone_index = DAHDI_TONE_MFR1_KP;
			break;
		case '#':
			tone_index = DAHDI_TONE_MFR1_ST;
			break;
		case 'A':
			tone_index = DAHDI_TONE_MFR1_STP;
			break;
		case 'B':
			tone_index = DAHDI_TONE_MFR1_ST2P;
			break;
		case 'C':
			tone_index = DAHDI_TONE_MFR1_ST3P;
			break;
		case 'W':
			return &tone_pause;
		default:
			return NULL;
		}
		return &chan->curzone->mfr1[tone_index - DAHDI_TONE_MFR1_BASE];
	case DIGIT_MODE_MFR2_FWD:
		switch (digit) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			tone_index = DAHDI_TONE_MFR2_FWD_1 + (digit - '1');
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			tone_index = DAHDI_TONE_MFR2_FWD_10 + (digit - 'A');
			break;
		case 'W':
			return &tone_pause;
		default:
			return NULL;
		}
		return &chan->curzone->mfr2_fwd[tone_index - DAHDI_TONE_MFR2_FWD_BASE];
	case DIGIT_MODE_MFR2_REV:
		switch (digit) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			tone_index = DAHDI_TONE_MFR2_REV_1 + (digit - '1');
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			tone_index = DAHDI_TONE_MFR2_REV_10 + (digit - 'A');
			break;
		case 'W':
			return &tone_pause;
		default:
			return NULL;
		}
		return &chan->curzone->mfr2_rev[tone_index - DAHDI_TONE_MFR2_REV_BASE];
	default:
		return NULL;
	}
}

static void __do_dtmf(struct dahdi_chan *chan)
{
	char c;

	/* Called with chan->lock held */
	while ((c = chan->txdialbuf[0])) {
		memmove(chan->txdialbuf, chan->txdialbuf + 1, sizeof(chan->txdialbuf) - 1);
		switch (c) {
		case 'T':
			chan->digitmode = DIGIT_MODE_DTMF;
			chan->tonep = 0;
			break;
		case 'M':
			chan->digitmode = DIGIT_MODE_MFR1;
			chan->tonep = 0;
			break;
		case 'O':
			chan->digitmode = DIGIT_MODE_MFR2_FWD;
			chan->tonep = 0;
			break;
		case 'R':
			chan->digitmode = DIGIT_MODE_MFR2_REV;
			chan->tonep = 0;
			break;
		case 'P':
			chan->digitmode = DIGIT_MODE_PULSE;
			chan->tonep = 0;
			break;
		default:
			if ((c != 'W') && (chan->digitmode == DIGIT_MODE_PULSE)) {
				if ((c >= '0') && (c <= '9') && (chan->txhooksig == DAHDI_TXSIG_OFFHOOK)) {
					chan->pdialcount = (c == '0') ? 10 : c - '0';
					dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_PULSEBREAK,
						       chan->pulsebreaktime);
					return;
				}
			} else {
				chan->curtone = dahdi_mf_tone(chan, c, chan->digitmode);
				chan->tonep = 0;
				if (chan->curtone) {
					dahdi_init_tone_state(&chan->ts, chan->curtone);
					return;
				}
			}
		}
	}

	/* Notify userspace process if there is nothing left */
	chan->dialing = 0;
	__qevent(chan, DAHDI_EVENT_DIALCOMPLETE);
}

static int dahdi_release(struct inode *inode, struct file *file)
{
	int unit = UNIT(file);
	int res;
	struct dahdi_chan *chan;

	if (unit == DAHDI_CTL)
		return dahdi_ctl_release(file);
	if (unit == DAHDI_TIMER) {
		return dahdi_timer_release(file);
	}
	if (unit == DAHDI_TRANSCODE) {
		/* We should not be here because the dahdi_transcode.ko module
		 * should have updated the file_operations for this file
		 * handle when the file was opened. */
		WARN_ON(1);
		return -EFAULT;
	}
	if (unit == DAHDI_CHANNEL) {
		chan = file->private_data;
		if (!chan)
			return dahdi_chan_release(file);
		else
			return dahdi_specchan_release(file);
	}
	if (unit == DAHDI_PSEUDO) {
		chan = file->private_data;
		if (chan) {
			res = dahdi_specchan_release(file);
			dahdi_free_pseudo(chan);
		} else {
			module_printk(KERN_NOTICE, "Pseudo release and no private data??\n");
			res = 0;
		}
		return res;
	}
	return dahdi_specchan_release(file);
}

/**
 * dahdi_alarm_channel() - notify userspace channel is (not) in alarm
 * @chan:	the DAHDI channel
 * @alarms:	alarm bits set
 *
 * Notify userspace about a change in alarm status of this channel.
 * 
 * Note that channel drivers should only use this function directly if
 * they have a single port per channel. Whole-span alarms should be sent
 * using dahdi_alarm_notify() .
 *
 * Does nothing if alarms on the channel have not changed. If they have,
 * triggers sending either DAHDI_EVENT_NOALARM (if they were cleared) or
 * DAHDI_EVENT_ALARM (otherwise).
 *
 * Currently it is only used by drivers of FXO ports to notify (with a
 * red alarm) they have no battery current.
 */
void dahdi_alarm_channel(struct dahdi_chan *chan, int alarms)
{
	unsigned long flags;

	spin_lock_irqsave(&chan->lock, flags);
	if (chan->chan_alarms != alarms) {
		chan->chan_alarms = alarms;
		dahdi_qevent_nolock(chan, alarms ? DAHDI_EVENT_ALARM : DAHDI_EVENT_NOALARM);
	}
	spin_unlock_irqrestore(&chan->lock, flags);
}

static void __dahdi_find_master_span(void)
{
	struct dahdi_span *s;
	unsigned long flags;
	struct dahdi_span *old_master;

	spin_lock_irqsave(&chan_lock, flags);
	old_master = master;
	list_for_each_entry(s, &span_list, spans_node) {
		if (s->alarms && old_master)
			continue;
		if (dahdi_is_digital_span(s) &&
		    !test_bit(DAHDI_FLAGBIT_RUNNING, &s->flags) &&
		    old_master)
			continue;
		if (!can_provide_timing(s))
			continue;
		if (master == s)
			continue;

		master = s;
		break;
	}
	spin_unlock_irqrestore(&chan_lock, flags);

	if ((debug & DEBUG_MAIN) && (old_master != master))
		module_printk(KERN_NOTICE, "Master changed to %s\n", s->name);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void _dahdi_find_master_span(void *work)
{
	__dahdi_find_master_span();
}
static DECLARE_WORK(find_master_work, _dahdi_find_master_span, NULL);
#else
static void _dahdi_find_master_span(struct work_struct *work)
{
	__dahdi_find_master_span();
}
static DECLARE_WORK(find_master_work, _dahdi_find_master_span);
#endif

static void dahdi_find_master_span(void)
{
	schedule_work(&find_master_work);
}

void dahdi_alarm_notify(struct dahdi_span *span)
{
	int x;

	span->alarms &= ~DAHDI_ALARM_LOOPBACK;
	/* Determine maint status */
	if (span->maintstat || span->mainttimer)
		span->alarms |= DAHDI_ALARM_LOOPBACK;
	/* DON'T CHANGE THIS AGAIN. THIS WAS DONE FOR A REASON.
 	   The expression (a != b) does *NOT* do the same thing
	   as ((!a) != (!b)) */
	/* if change in general state */
	if ((!span->alarms) != (!span->lastalarms)) {
		span->lastalarms = span->alarms;
		for (x = 0; x < span->channels; x++)
			dahdi_alarm_channel(span->chans[x], span->alarms);

		/* If we're going into or out of alarm we should try to find a
		 * new master that may be a better fit. */
		dahdi_find_master_span();

		/* Report more detailed alarms */
		if (debug & DEBUG_MAIN) {
			if (span->alarms & DAHDI_ALARM_LOS) {
				module_printk(KERN_NOTICE,
					"Span %d: Loss of signal\n",
					span->spanno);
			}
			if (span->alarms & DAHDI_ALARM_LFA) {
				module_printk(KERN_NOTICE,
					"Span %d: Loss of Frame Alignment\n",
					span->spanno);
			}
			if (span->alarms & DAHDI_ALARM_LMFA) {
				module_printk(KERN_NOTICE,
					"Span %d: Loss of Multi-Frame "\
					"Alignment\n", span->spanno);
			}
		}
	}
}

static int dahdi_timer_ioctl(struct file *file, unsigned int cmd, unsigned long data, struct dahdi_timer *timer)
{
	int j;
	unsigned long flags;
	switch(cmd) {
	case DAHDI_TIMERCONFIG:
		get_user(j, (int __user *)data);
		if (j < 0)
			j = 0;
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		timer->ms = timer->pos = j;
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
		break;
	case DAHDI_TIMERACK:
		get_user(j, (int __user *)data);
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		if ((j < 1) || (j > timer->tripped))
			j = timer->tripped;
		timer->tripped -= j;
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
		break;
	case DAHDI_GETEVENT:  /* Get event on queue */
		j = DAHDI_EVENT_NONE;
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		  /* set up for no event */
		if (timer->tripped)
			j = DAHDI_EVENT_TIMER_EXPIRED;
		if (timer->ping)
			j = DAHDI_EVENT_TIMER_PING;
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
		put_user(j, (int __user *)data);
		break;
	case DAHDI_TIMERPING:
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		timer->ping = 1;
		wake_up_interruptible(&timer->sel);
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
		break;
	case DAHDI_TIMERPONG:
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		timer->ping = 0;
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static int dahdi_ioctl_getgains(struct file *file, unsigned long data)
{
	int res = 0;
	struct dahdi_gains *gain;
	int j;
	void __user * const user_data = (void __user *)data;
	struct dahdi_chan *chan;

	gain = kzalloc(sizeof(*gain), GFP_KERNEL);
	if (!gain)
		return -ENOMEM;

	if (copy_from_user(gain, user_data, sizeof(*gain))) {
		res = -EFAULT;
		goto cleanup;
	}
	chan = (!gain->chan) ? chan_from_file(file) :
			       chan_from_num(gain->chan);
	if (!chan) {
		res = -EINVAL;
		goto cleanup;
	}

	if (!(chan->flags & DAHDI_FLAG_AUDIO)) {
		res = -EINVAL;
		goto cleanup;
	}
	gain->chan = chan->channo;
	for (j = 0; j < 256; ++j)  {
		gain->txgain[j] = chan->txgain[j];
		gain->rxgain[j] = chan->rxgain[j];
	}
	if (copy_to_user(user_data, gain, sizeof(*gain))) {
		res = -EFAULT;
		goto cleanup;
	}
cleanup:

	kfree(gain);
	return res;
}

static int dahdi_ioctl_setgains(struct file *file, unsigned long data)
{
	int res = 0;
	struct dahdi_gains *gain;
	unsigned char *txgain, *rxgain;
	int j;
	unsigned long flags;
	const int GAIN_TABLE_SIZE = sizeof(defgain);
	void __user * const user_data = (void __user *)data;
	struct dahdi_chan *chan;

	gain = kzalloc(sizeof(*gain), GFP_KERNEL);
	if (!gain)
		return -ENOMEM;

	if (copy_from_user(gain, user_data, sizeof(*gain))) {
		res = -EFAULT;
		goto cleanup;
	}

	chan = (!gain->chan) ? chan_from_file(file) :
			       chan_from_num(gain->chan);
	if (!chan) {
		res = -EINVAL;
		goto cleanup;
	}
	if (!(chan->flags & DAHDI_FLAG_AUDIO)) {
		res = -EINVAL;
		goto cleanup;
	}

	rxgain = kzalloc(GAIN_TABLE_SIZE*2, GFP_KERNEL);
	if (!rxgain) {
		res = -ENOMEM;
		goto cleanup;
	}

	gain->chan = chan->channo;
	txgain = rxgain + GAIN_TABLE_SIZE;

	for (j = 0; j < GAIN_TABLE_SIZE; ++j) {
		rxgain[j] = gain->rxgain[j];
		txgain[j] = gain->txgain[j];
	}

	if (!memcmp(rxgain, defgain, GAIN_TABLE_SIZE) &&
	    !memcmp(txgain, defgain, GAIN_TABLE_SIZE)) {
		kfree(rxgain);
		spin_lock_irqsave(&chan->lock, flags);
		if (is_gain_allocated(chan))
			kfree(chan->rxgain);
		chan->rxgain = defgain;
		chan->txgain = defgain;
		spin_unlock_irqrestore(&chan->lock, flags);
	} else {
		/* This is a custom gain setting */
		spin_lock_irqsave(&chan->lock, flags);
		if (is_gain_allocated(chan))
			kfree(chan->rxgain);
		chan->rxgain = rxgain;
		chan->txgain = txgain;
		spin_unlock_irqrestore(&chan->lock, flags);
	}

	if (copy_to_user(user_data, gain, sizeof(*gain))) {
		res = -EFAULT;
		goto cleanup;
	}
cleanup:

	kfree(gain);
	return res;
}

static int dahdi_ioctl_chandiag(struct file *file, unsigned long data)
{
	unsigned long flags;
	int channo;
	/* there really is no need to initialize this structure because when it is used it has
	 * already been completely overwritten, but apparently the compiler cannot figure that
	 * out and warns about uninitialized usage... so initialize it.
	 */
	struct dahdi_echocan_state ec_state = { .ops = NULL, };
	struct dahdi_chan *chan;
	struct dahdi_chan *temp;

	/* get channel number from user */
	get_user(channo, (int __user *)data);

	chan = chan_from_num(channo);
	if (!chan)
		return -EINVAL;

	temp = kmalloc(sizeof(*chan), GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	spin_lock_irqsave(&chan->lock, flags);
	*temp = *chan;
	if (temp->ec_state)
		ec_state = *temp->ec_state;
	if (temp->curzone)
		tone_zone_get(temp->curzone);
	spin_unlock_irqrestore(&chan->lock, flags);

	module_printk(KERN_INFO, "Dump of DAHDI Channel %d (%s,%d,%d):\n\n",
		      channo, temp->name, temp->channo, temp->chanpos);
	module_printk(KERN_INFO, "flags: %x hex, writechunk: %p, readchunk: %p\n",
		      (unsigned int) temp->flags, temp->writechunk, temp->readchunk);
	module_printk(KERN_INFO, "rxgain: %p, txgain: %p, gainalloc: %d\n",
		      temp->rxgain, temp->txgain, is_gain_allocated(temp));
	module_printk(KERN_INFO, "span: %p, sig: %x hex, sigcap: %x hex\n",
		      temp->span, temp->sig, temp->sigcap);
	module_printk(KERN_INFO, "inreadbuf: %d, outreadbuf: %d, inwritebuf: %d, outwritebuf: %d\n",
		      temp->inreadbuf, temp->outreadbuf, temp->inwritebuf, temp->outwritebuf);
	module_printk(KERN_INFO, "blocksize: %d, numbufs: %d, txbufpolicy: %d, txbufpolicy: %d\n",
		      temp->blocksize, temp->numbufs, temp->txbufpolicy, temp->rxbufpolicy);
	module_printk(KERN_INFO, "txdisable: %d, rxdisable: %d, iomask: %d\n",
		      temp->txdisable, temp->rxdisable, temp->iomask);
	module_printk(KERN_INFO, "curzone: %p, tonezone: %d, curtone: %p, tonep: %d\n",
		      temp->curzone,
		      ((temp->curzone) ? temp->curzone->num : -1),
		      temp->curtone, temp->tonep);
	module_printk(KERN_INFO, "digitmode: %d, txdialbuf: %s, dialing: %d, aftdialtimer: %d, cadpos. %d\n",
		      temp->digitmode, temp->txdialbuf, temp->dialing,
		      temp->afterdialingtimer, temp->cadencepos);
	module_printk(KERN_INFO, "confna: %d, confn: %d, confmode: %d, confmute: %d\n",
		      temp->confna, temp->_confn, temp->confmode, temp->confmute);
	module_printk(KERN_INFO, "ec: %p, deflaw: %d, xlaw: %p\n",
		      temp->ec_state, temp->deflaw, temp->xlaw);
	if (temp->ec_state) {
		module_printk(KERN_INFO, "echostate: %02x, echotimer: %d, echolastupdate: %d\n",
			      ec_state.status.mode, ec_state.status.pretrain_timer, ec_state.status.last_train_tap);
	}
	module_printk(KERN_INFO, "itimer: %d, otimer: %d, ringdebtimer: %d\n\n",
		      temp->itimer, temp->otimer, temp->ringdebtimer);

	if (temp->curzone)
		tone_zone_put(temp->curzone);
	kfree(temp);
	return 0;
}

/**
 * dahdi_ioctl_getparams() - Get channel parameters.
 *
 */
static int dahdi_ioctl_getparams(struct file *file, unsigned long data)
{
	size_t size_to_copy;
	struct dahdi_params param;
	bool return_master = false;
	struct dahdi_chan *chan;
	int j;
	size_to_copy = sizeof(struct dahdi_params);
	if (copy_from_user(&param, (void __user *)data, size_to_copy))
		return -EFAULT;

	/* check to see if the caller wants to receive our master channel
	 * number */
	if (param.channo & DAHDI_GET_PARAMS_RETURN_MASTER) {
		return_master = true;
		param.channo &= ~DAHDI_GET_PARAMS_RETURN_MASTER;
	}

	/* Pick the right channo's */
	chan = chan_from_file(file);
	if (!chan)
		chan = chan_from_num(param.channo);

	if (!chan)
		return -EINVAL;

	/* point to relevant structure */
	param.sigtype = chan->sig;  /* get signalling type */
	/* return non-zero if rx not in idle state */
	if (chan->span) {
		j = dahdi_q_sig(chan);
		if (j >= 0) { /* if returned with success */
			param.rxisoffhook = ((chan->rxsig & (j >> 8)) !=
							(j & 0xff));
		} else {
			const int sig = chan->rxhooksig;
			param.rxisoffhook = ((sig != DAHDI_RXSIG_ONHOOK) &&
				(sig != DAHDI_RXSIG_INITIAL));
		}
	} else if ((chan->txstate == DAHDI_TXSTATE_KEWL) ||
		   (chan->txstate == DAHDI_TXSTATE_AFTERKEWL)) {
		param.rxisoffhook = 1;
	} else {
		param.rxisoffhook = 0;
	}

	if (chan->span &&
	    chan->span->ops->rbsbits && !(chan->sig & DAHDI_SIG_CLEAR)) {
		param.rxbits = chan->rxsig;
		param.txbits = chan->txsig;
		param.idlebits = chan->idlebits;
	} else {
		param.rxbits = -1;
		param.txbits = -1;
		param.idlebits = 0;
	}
	if (chan->span &&
	    (chan->span->ops->rbsbits || chan->span->ops->hooksig) &&
	    !(chan->sig & DAHDI_SIG_CLEAR)) {
		param.rxhooksig = chan->rxhooksig;
		param.txhooksig = chan->txhooksig;
	} else {
		param.rxhooksig = -1;
		param.txhooksig = -1;
	}
	param.prewinktime = chan->prewinktime;
	param.preflashtime = chan->preflashtime;
	param.winktime = chan->winktime;
	param.flashtime = chan->flashtime;
	param.starttime = chan->starttime;
	param.rxwinktime = chan->rxwinktime;
	param.rxflashtime = chan->rxflashtime;
	param.debouncetime = chan->debouncetime;
	param.channo = chan->channo;
	param.chan_alarms = chan->chan_alarms;

	/* if requested, put the master channel number in the top 16 bits of
	 * the result */
	if (return_master)
		param.channo |= chan->master->channo << 16;

	param.pulsemaketime = chan->pulsemaketime;
	param.pulsebreaktime = chan->pulsebreaktime;
	param.pulseaftertime = chan->pulseaftertime;
	param.spanno = (chan->span) ? chan->span->spanno : 0;
	strlcpy(param.name, chan->name, sizeof(param.name));
	param.chanpos = chan->chanpos;
	param.sigcap = chan->sigcap;
	/* Return current law */
	if (chan->xlaw == __dahdi_alaw)
		param.curlaw = DAHDI_LAW_ALAW;
	else
		param.curlaw = DAHDI_LAW_MULAW;

	if (copy_to_user((void __user *)data, &param, size_to_copy))
		return -EFAULT;

	return 0;
}

/**
 * dahdi_ioctl_setparams() - Set channel parameters.
 *
 */
static int dahdi_ioctl_setparams(struct file *file, unsigned long data)
{
	struct dahdi_params param;
	struct dahdi_chan *chan;

	if (copy_from_user(&param, (void __user *)data, sizeof(param)))
		return -EFAULT;

	param.chan_alarms = 0; /* be explicit about the above */

	/* Pick the right channo's */
	chan = chan_from_file(file);
	if (!chan)
		chan = chan_from_num(param.channo);

	if (!chan)
		return -EINVAL;

	/* point to relevant structure */
	/* NOTE: sigtype is *not* included in this */
	  /* get timing paramters */
	chan->prewinktime = param.prewinktime;
	chan->preflashtime = param.preflashtime;
	chan->winktime = param.winktime;
	chan->flashtime = param.flashtime;
	chan->starttime = param.starttime;
	/* Update ringtime if not using a tone zone */
	if (!chan->curzone)
		chan->ringcadence[0] = chan->starttime;
	chan->rxwinktime = param.rxwinktime;
	chan->rxflashtime = param.rxflashtime;
	chan->debouncetime = param.debouncetime;
	chan->pulsemaketime = param.pulsemaketime;
	chan->pulsebreaktime = param.pulsebreaktime;
	chan->pulseaftertime = param.pulseaftertime;
	return 0;
}

/**
 * dahdi_ioctl_spanstat() - Return statistics for a span.
 *
 */
static int dahdi_ioctl_spanstat(struct file *file, unsigned long data)
{
	int ret = 0;
	struct dahdi_spaninfo spaninfo;
	struct dahdi_span *s;
	int j;
	size_t size_to_copy;
	bool via_chan = false;

	size_to_copy = sizeof(struct dahdi_spaninfo);
	if (copy_from_user(&spaninfo, (void __user *)data, size_to_copy))
		return -EFAULT;
	if (!spaninfo.spanno) {
		struct dahdi_chan *const chan = chan_from_file(file);
		if (!chan)
			return -EINVAL;

		s = chan->span;
		via_chan = true;
	} else {
		s = span_find_and_get(spaninfo.spanno);
	}
	if (!s)
		return -EINVAL;

	spaninfo.spanno = s->spanno; /* put the span # in here */
	spaninfo.totalspans = span_count();
	strlcpy(spaninfo.desc, s->desc, sizeof(spaninfo.desc));
	strlcpy(spaninfo.name, s->name, sizeof(spaninfo.name));
	spaninfo.alarms = s->alarms;		/* get alarm status */
	spaninfo.rxlevel = s->rxlevel;	/* get rx level */
	spaninfo.txlevel = s->txlevel;	/* get tx level */

	spaninfo.bpvcount = s->count.bpv;
	spaninfo.crc4count = s->count.crc4;
	spaninfo.ebitcount = s->count.ebit;
	spaninfo.fascount = s->count.fas;
	spaninfo.fecount = s->count.fe;
	spaninfo.cvcount = s->count.cv;
	spaninfo.becount = s->count.be;
	spaninfo.prbs = s->count.prbs;
	spaninfo.errsec = s->count.errsec;

	spaninfo.irqmisses = s->parent->irqmisses;	/* get IRQ miss count */
	spaninfo.syncsrc = s->syncsrc;	/* get active sync source */
	spaninfo.totalchans = s->channels;
	spaninfo.numchans = 0;
	for (j = 0; j < s->channels; j++) {
		if (s->chans[j]->sig)
			spaninfo.numchans++;
	}
	spaninfo.lbo = s->lbo;
	spaninfo.lineconfig = s->lineconfig;
	spaninfo.irq = 0;
	spaninfo.linecompat = s->linecompat;
	strlcpy(spaninfo.lboname, dahdi_lboname(s->lbo),
			  sizeof(spaninfo.lboname));
	if (s->parent->manufacturer) {
		strlcpy(spaninfo.manufacturer, s->parent->manufacturer,
			sizeof(spaninfo.manufacturer));
	}
	if (s->parent->devicetype) {
		strlcpy(spaninfo.devicetype, s->parent->devicetype,
			sizeof(spaninfo.devicetype));
	}
	if (s->parent->location) {
		strlcpy(spaninfo.location, s->parent->location,
			sizeof(spaninfo.location));
	}
	if (s->spantype) {
		strlcpy(spaninfo.spantype, s->spantype,
				  sizeof(spaninfo.spantype));
	}

	if (copy_to_user((void __user *)data, &spaninfo, size_to_copy))
		ret = -EFAULT;

	if (!via_chan)
		put_span(s);

	return ret;
}

/**
 * dahdi_ioctl_spanstat_v1() - Return statistics for a span in a legacy format.
 *
 */
static int dahdi_ioctl_spanstat_v1(struct file *file, unsigned long data)
{
	int ret = 0;
	struct dahdi_spaninfo_v1 spaninfo_v1;
	struct dahdi_span *s;
	int j;
	bool via_chan = false;

	if (copy_from_user(&spaninfo_v1, (void __user *)data,
			   sizeof(spaninfo_v1))) {
		return -EFAULT;
	}

	if (!spaninfo_v1.spanno) {
		struct dahdi_chan *const chan = chan_from_file(file);
		if (!chan)
			return -EINVAL;

		s = chan->span;
		via_chan = true;
	} else {
		s = span_find_and_get(spaninfo_v1.spanno);
	}

	if (!s)
		return -EINVAL;

	spaninfo_v1.spanno = s->spanno; /* put the span # in here */
	spaninfo_v1.totalspans = 0;
	spaninfo_v1.totalspans = span_count();
	strlcpy(spaninfo_v1.desc,
			  s->desc,
			  sizeof(spaninfo_v1.desc));
	strlcpy(spaninfo_v1.name,
			  s->name,
			  sizeof(spaninfo_v1.name));
	spaninfo_v1.alarms = s->alarms;
	spaninfo_v1.bpvcount = s->count.bpv;
	spaninfo_v1.rxlevel = s->rxlevel;
	spaninfo_v1.txlevel = s->txlevel;
	spaninfo_v1.crc4count = s->count.crc4;
	spaninfo_v1.ebitcount = s->count.ebit;
	spaninfo_v1.fascount = s->count.fas;
	spaninfo_v1.irqmisses = s->parent->irqmisses;
	spaninfo_v1.syncsrc = s->syncsrc;
	spaninfo_v1.totalchans = s->channels;
	spaninfo_v1.numchans = 0;
	for (j = 0; j < s->channels; j++) {
		if (s->chans[j]->sig)
			spaninfo_v1.numchans++;
	}
	spaninfo_v1.lbo = s->lbo;
	spaninfo_v1.lineconfig = s->lineconfig;
	spaninfo_v1.irq = 0;
	spaninfo_v1.linecompat = s->linecompat;
	strlcpy(spaninfo_v1.lboname,
			  dahdi_lboname(s->lbo),
			  sizeof(spaninfo_v1.lboname));

	if (s->parent->manufacturer) {
		strlcpy(spaninfo_v1.manufacturer, s->parent->manufacturer,
			sizeof(spaninfo_v1.manufacturer));
	}

	if (s->parent->devicetype) {
		strlcpy(spaninfo_v1.devicetype, s->parent->devicetype,
			sizeof(spaninfo_v1.devicetype));
	}

	strlcpy(spaninfo_v1.location, s->parent->location,
		sizeof(spaninfo_v1.location));

	if (s->spantype) {
		strlcpy(spaninfo_v1.spantype,
				  s->spantype,
				  sizeof(spaninfo_v1.spantype));
	}

	if (copy_to_user((void __user *)data, &spaninfo_v1,
			 sizeof(spaninfo_v1))) {
		ret = -EFAULT;
	}
	if (!via_chan)
		put_span(s);
	return ret;
}

static int dahdi_common_ioctl(struct file *file, unsigned int cmd,
			      unsigned long data)
{
	switch (cmd) {
		/* get channel parameters */
	case DAHDI_GET_PARAMS_V1: /* Intentional drop through. */
	case DAHDI_GET_PARAMS:
		return dahdi_ioctl_getparams(file, data);

	case DAHDI_SET_PARAMS:
		return dahdi_ioctl_setparams(file, data);

	case DAHDI_GETGAINS_V1: /* Intentional drop through. */
	case DAHDI_GETGAINS:  /* get gain stuff */
		return dahdi_ioctl_getgains(file, data);

	case DAHDI_SETGAINS:  /* set gain stuff */
		return dahdi_ioctl_setgains(file, data);

	case DAHDI_SPANSTAT:
		return dahdi_ioctl_spanstat(file, data);

	case DAHDI_SPANSTAT_V1:
		return dahdi_ioctl_spanstat_v1(file, data);

	case DAHDI_CHANDIAG_V1: /* Intentional drop through. */
	case DAHDI_CHANDIAG:
		return dahdi_ioctl_chandiag(file, data);
	default:
		return -ENOTTY;
	}
	return 0;
}

static const struct dahdi_dynamic_ops *dahdi_dynamic_ops;
void dahdi_set_dynamic_ops(const struct dahdi_dynamic_ops *ops)
{
	mutex_lock(&registration_mutex);
	dahdi_dynamic_ops = ops;
	mutex_unlock(&registration_mutex);
}
EXPORT_SYMBOL(dahdi_set_dynamic_ops);

static int (*dahdi_hpec_ioctl)(unsigned int cmd, unsigned long data);

void dahdi_set_hpec_ioctl(int (*func)(unsigned int cmd, unsigned long data))
{
	dahdi_hpec_ioctl = func;
}

static void recalc_slaves(struct dahdi_chan *chan)
{
	int x;
	struct dahdi_chan *last = chan;

	/* Makes no sense if you don't have a span */
	if (!chan->span)
		return;

#ifdef CONFIG_DAHDI_DEBUG
	module_printk(KERN_NOTICE, "Recalculating slaves on %s\n", chan->name);
#endif

	/* Link all slaves appropriately */
	for (x=chan->chanpos;x<chan->span->channels;x++)
		if (chan->span->chans[x]->master == chan) {
#ifdef CONFIG_DAHDI_DEBUG
			module_printk(KERN_NOTICE, "Channel %s, slave to %s, last is %s, its next will be %d\n",
				      chan->span->chans[x]->name, chan->name, last->name, x);
#endif
			last->nextslave = chan->span->chans[x];
			last = last->nextslave;
		}
	/* Terminate list */
	last->nextslave = NULL;
#ifdef CONFIG_DAHDI_DEBUG
	module_printk(KERN_NOTICE, "Done Recalculating slaves on %s (last is %s)\n", chan->name, last->name);
#endif
}

#if defined(CONFIG_DAHDI_NET) && defined(HAVE_NET_DEVICE_OPS)
static const struct net_device_ops dahdi_netdev_ops = {
	.ndo_open       = dahdi_net_open,
	.ndo_stop       = dahdi_net_stop,
	.ndo_do_ioctl   = dahdi_net_ioctl,
	.ndo_start_xmit = dahdi_xmit,
};
#endif

static int dahdi_ioctl_chanconfig(struct file *file, unsigned long data)
{
	int res = 0;
	int y;
	struct dahdi_chanconfig ch;
	struct dahdi_chan *newmaster;
	struct dahdi_chan *chan;
	struct dahdi_chan *dacs_chan = NULL;
	unsigned long flags;
	int sigcap;

	if (copy_from_user(&ch, (void __user *)data, sizeof(ch)))
		return -EFAULT;

	chan = chan_from_num(ch.chan);
	if (!chan) {
		printk(KERN_NOTICE "%s: No channel for number %d\n",
				__func__, ch.chan);
		return -EINVAL;
	}

	if (ch.sigtype == DAHDI_SIG_SLAVE) {
		newmaster = chan_from_num(ch.master);
		if (!newmaster) {
			chan_notice(chan, "%s: slave channel without master.\n",
					__func__);
			return -EINVAL;
		}
		ch.sigtype = newmaster->sig;
	} else if ((ch.sigtype & __DAHDI_SIG_DACS) == __DAHDI_SIG_DACS) {
		newmaster = chan;
		dacs_chan = chan_from_num(ch.idlebits);
		if (!dacs_chan) {
			chan_notice(chan, "%s: dacs channel not found: %d.\n",
					__func__, ch.idlebits);
			return -EINVAL;
		}
	} else {
		newmaster = chan;
	}
	spin_lock_irqsave(&chan->lock, flags);
#ifdef CONFIG_DAHDI_NET
	if (dahdi_have_netdev(chan)) {
		if (chan_to_netdev(chan)->flags & IFF_UP) {
			spin_unlock_irqrestore(&chan->lock, flags);
			module_printk(KERN_WARNING, "Can't switch HDLC net mode on channel %s, since current interface is up\n", chan->name);
			return -EBUSY;
		}
		spin_unlock_irqrestore(&chan->lock, flags);
		unregister_hdlc_device(chan->hdlcnetdev->netdev);
		spin_lock_irqsave(&chan->lock, flags);
		free_netdev(chan->hdlcnetdev->netdev);
		kfree(chan->hdlcnetdev);
		chan->hdlcnetdev = NULL;
		clear_bit(DAHDI_FLAGBIT_NETDEV, &chan->flags);
	}
#else
	if (ch.sigtype == DAHDI_SIG_HDLCNET) {
		spin_unlock_irqrestore(&chan->lock, flags);
		module_printk(KERN_WARNING, "DAHDI networking not supported by this build.\n");
		return -ENOSYS;
	}
#endif
	sigcap = chan->sigcap;
	/* If they support clear channel, then they support the HDLC and such through
	   us.  */
	if (sigcap & DAHDI_SIG_CLEAR)
		sigcap |= (DAHDI_SIG_HDLCRAW | DAHDI_SIG_HDLCFCS | DAHDI_SIG_HDLCNET | DAHDI_SIG_DACS);

	if ((sigcap & ch.sigtype) != ch.sigtype) {
		if (debug) {
			chan_notice(chan, "%s: bad sigtype. sigcap: %x, sigtype: %x.\n",
					__func__, sigcap, ch.sigtype);
		}
		res = -EINVAL;
	}

	if (chan->master != chan) {
		struct dahdi_chan *oldmaster = chan->master;

		/* Clear the master channel */
		chan->master = chan;
		chan->nextslave = NULL;
		/* Unlink this channel from the master's channel list */
		recalc_slaves(oldmaster);
	}

	if (!res) {
		chan->sig = ch.sigtype;
		if (chan->sig == DAHDI_SIG_CAS)
			chan->idlebits = ch.idlebits;
		else
			chan->idlebits = 0;
		if ((ch.sigtype & DAHDI_SIG_CLEAR) == DAHDI_SIG_CLEAR) {
			/* Set clear channel flag if appropriate */
			chan->flags &= ~DAHDI_FLAG_AUDIO;
			chan->flags |= DAHDI_FLAG_CLEAR;
		} else {
			/* Set audio flag and not clear channel otherwise */
			chan->flags |= DAHDI_FLAG_AUDIO;
			chan->flags &= ~DAHDI_FLAG_CLEAR;
		}
		if ((ch.sigtype & DAHDI_SIG_HDLCRAW) == DAHDI_SIG_HDLCRAW) {
			/* Set the HDLC flag */
			chan->flags |= DAHDI_FLAG_HDLC;
		} else {
			/* Clear the HDLC flag */
			chan->flags &= ~DAHDI_FLAG_HDLC;
		}
		if ((ch.sigtype & DAHDI_SIG_HDLCFCS) == DAHDI_SIG_HDLCFCS) {
			/* Set FCS to be calculated if appropriate */
			chan->flags |= DAHDI_FLAG_FCS;
		} else {
			/* Clear FCS flag */
			chan->flags &= ~DAHDI_FLAG_FCS;
		}
		if ((ch.sigtype & __DAHDI_SIG_DACS) == __DAHDI_SIG_DACS) {
			if (unlikely(!dacs_chan)) {
				spin_unlock_irqrestore(&chan->lock, flags);
				chan_notice(chan, "%s: dacs but no dacs_chan\n",
						__func__);
				return -EINVAL;
			}
			/* Setup conference properly */
			chan->confmode = DAHDI_CONF_DIGITALMON;
			chan->confna = ch.idlebits;
			chan->dacs_chan = dacs_chan;
			res = dahdi_chan_dacs(chan, dacs_chan);
		} else {
			dahdi_disable_dacs(chan);
		}
		chan->master = newmaster;
		/* Note new slave if we are not our own master */
		if (newmaster != chan) {
			recalc_slaves(chan->master);
		}
		if ((ch.sigtype & DAHDI_SIG_HARDHDLC) == DAHDI_SIG_HARDHDLC) {
			chan->flags &= ~DAHDI_FLAG_FCS;
			chan->flags &= ~DAHDI_FLAG_HDLC;
			chan->flags |= DAHDI_FLAG_NOSTDTXRX;
		} else {
			chan->flags &= ~DAHDI_FLAG_NOSTDTXRX;
		}

		if ((ch.sigtype & DAHDI_SIG_MTP2) == DAHDI_SIG_MTP2)
			chan->flags |= DAHDI_FLAG_MTP2;
		else
			chan->flags &= ~DAHDI_FLAG_MTP2;
	}

	/* Chanconfig can block, do not call through the function pointer with
	 * the channel lock held. */
	spin_unlock_irqrestore(&chan->lock, flags);
	if (!res && chan->span->ops->chanconfig)
		res = chan->span->ops->chanconfig(file, chan, ch.sigtype);
	spin_lock_irqsave(&chan->lock, flags);


#ifdef CONFIG_DAHDI_NET
	if (!res &&
	    (newmaster == chan) &&
	    (chan->sig == DAHDI_SIG_HDLCNET)) {
		chan->hdlcnetdev = dahdi_hdlc_alloc();
		if (chan->hdlcnetdev) {
/*				struct hdlc_device *hdlc = chan->hdlcnetdev;
			struct net_device *d = hdlc_to_dev(hdlc); mmm...get it right later --byg */

			chan->hdlcnetdev->netdev = alloc_hdlcdev(chan->hdlcnetdev);
			if (chan->hdlcnetdev->netdev) {
				chan->hdlcnetdev->chan = chan;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 23)
				SET_MODULE_OWNER(chan->hdlcnetdev->netdev);
#endif
				chan->hdlcnetdev->netdev->tx_queue_len = 50;
#ifdef HAVE_NET_DEVICE_OPS
				chan->hdlcnetdev->netdev->netdev_ops = &dahdi_netdev_ops;
#else
				chan->hdlcnetdev->netdev->do_ioctl = dahdi_net_ioctl;
				chan->hdlcnetdev->netdev->open = dahdi_net_open;
				chan->hdlcnetdev->netdev->stop = dahdi_net_stop;
#endif
				dev_to_hdlc(chan->hdlcnetdev->netdev)->attach = dahdi_net_attach;
				dev_to_hdlc(chan->hdlcnetdev->netdev)->xmit = dahdi_xmit;
				spin_unlock_irqrestore(&chan->lock, flags);
				/* Briefly restore interrupts while we register the device */
				res = dahdi_register_hdlc_device(chan->hdlcnetdev->netdev, ch.netdev_name);
				spin_lock_irqsave(&chan->lock, flags);
			} else {
				module_printk(KERN_NOTICE, "Unable to allocate hdlc: *shrug*\n");
				res = -1;
			}
			if (!res)
				set_bit(DAHDI_FLAGBIT_NETDEV, &chan->flags);
		} else {
			module_printk(KERN_NOTICE, "Unable to allocate netdev: out of memory\n");
			res = -1;
		}
	}
#endif
	if ((chan->sig == DAHDI_SIG_HDLCNET) &&
	    (chan == newmaster) &&
	    !dahdi_have_netdev(chan))
		module_printk(KERN_NOTICE, "Unable to register HDLC device for channel %s\n", chan->name);
	if (!res) {
		/* Setup default law */
		chan->deflaw = ch.deflaw;
		/* Copy back any modified settings */
		spin_unlock_irqrestore(&chan->lock, flags);
		if (copy_to_user((void __user *)data, &ch, sizeof(ch)))
			return -EFAULT;
		spin_lock_irqsave(&chan->lock, flags);
		/* And hangup */
		dahdi_hangup(chan);
		y = dahdi_q_sig(chan) & 0xff;
		if (y >= 0)
			chan->rxsig = (unsigned char) y;
		chan->rxhooksig = DAHDI_RXSIG_INITIAL;
	}
#ifdef CONFIG_DAHDI_DEBUG
	module_printk(KERN_NOTICE, "Configured channel %s, flags %04lx, sig %04x\n", chan->name, chan->flags, chan->sig);
#endif
	spin_unlock_irqrestore(&chan->lock, flags);

	return res;
}

/**
 * dahdi_ioctl_set_dialparms - Set the global dial parameters.
 * @data:	Pointer to user space that contains dahdi_dialparams.
 */
static int dahdi_ioctl_set_dialparams(unsigned long data)
{
	unsigned int i;
	struct dahdi_dialparams tdp;
	struct dahdi_zone *z;
	struct dahdi_dialparams *const gdp = &global_dialparams;

	if (copy_from_user(&tdp, (void __user *)data, sizeof(tdp)))
		return -EFAULT;

	mutex_lock(&global_dialparamslock);

	if ((tdp.dtmf_tonelen >= 10) && (tdp.dtmf_tonelen <= 4000))
		gdp->dtmf_tonelen = tdp.dtmf_tonelen;

	if ((tdp.mfv1_tonelen >= 10) && (tdp.mfv1_tonelen <= 4000))
		gdp->mfv1_tonelen = tdp.mfv1_tonelen;

	if ((tdp.mfr2_tonelen >= 10) && (tdp.mfr2_tonelen <= 4000))
		gdp->mfr2_tonelen = tdp.mfr2_tonelen;

	/* update the lengths in all currently loaded zones */
	spin_lock(&zone_lock);
	list_for_each_entry(z, &tone_zones, node) {

		for (i = 0; i < ARRAY_SIZE(z->dtmf); i++) {
			z->dtmf[i].tonesamples =
				gdp->dtmf_tonelen * DAHDI_CHUNKSIZE;
		}

		/* for MFR1, we only adjust the length of the digits */
		for (i = DAHDI_TONE_MFR1_0; i <= DAHDI_TONE_MFR1_9; i++) {
			z->mfr1[i - DAHDI_TONE_MFR1_BASE].tonesamples =
				gdp->mfv1_tonelen * DAHDI_CHUNKSIZE;
		}

		for (i = 0; i < ARRAY_SIZE(z->mfr2_fwd); i++) {
			z->mfr2_fwd[i].tonesamples =
				gdp->mfr2_tonelen * DAHDI_CHUNKSIZE;
		}

		for (i = 0; i < ARRAY_SIZE(z->mfr2_rev); i++) {
			z->mfr2_rev[i].tonesamples =
				gdp->mfr2_tonelen * DAHDI_CHUNKSIZE;
		}
	}
	spin_unlock(&zone_lock);

	dtmf_silence.tonesamples = gdp->dtmf_tonelen * DAHDI_CHUNKSIZE;
	mfr1_silence.tonesamples = gdp->mfv1_tonelen * DAHDI_CHUNKSIZE;
	mfr2_silence.tonesamples = gdp->mfr2_tonelen * DAHDI_CHUNKSIZE;

	mutex_unlock(&global_dialparamslock);

	return 0;
}

static int dahdi_ioctl_get_dialparams(unsigned long data)
{
	struct dahdi_dialparams tdp;

	mutex_lock(&global_dialparamslock);
	tdp = global_dialparams;
	mutex_unlock(&global_dialparamslock);
	if (copy_to_user((void __user *)data, &tdp, sizeof(tdp)))
		return -EFAULT;
	return 0;
}

static int dahdi_ioctl_indirect(struct file *file, unsigned long data)
{
	int res;
	struct dahdi_indirect_data ind;
	void *old;
	static bool warned;
	struct dahdi_chan *chan;

	if (copy_from_user(&ind, (void __user *)data, sizeof(ind)))
		return -EFAULT;

	chan = chan_from_num(ind.chan);
	if (!chan)
		return -EINVAL;

	if (!warned) {
		warned = true;
		module_printk(KERN_WARNING, "Using deprecated " \
			      "DAHDI_INDIRECT.  Please update " \
			      "dahdi-tools.\n");
	}

	/* Since dahdi_chan_ioctl expects to be called on file handles
	 * associated with channels, we'll temporarily set the
	 * private_data pointer on the ctl file handle just for this
	 * call. */
	old = file->private_data;
	file->private_data = chan;
	res = dahdi_chan_ioctl(file, ind.op, (unsigned long) ind.data);
	file->private_data = old;
	return res;
}

static int dahdi_ioctl_spanconfig(struct file *file, unsigned long data)
{
	int res = 0;
	struct dahdi_lineconfig lc;
	struct dahdi_span *s;

	if (copy_from_user(&lc, (void __user *)data, sizeof(lc)))
		return -EFAULT;
	s = span_find_and_get(lc.span);
	if (!s)
		return -ENXIO;

	if ((lc.lineconfig & 0x1ff0 & s->linecompat) !=
	    (lc.lineconfig & 0x1ff0)) {
		put_span(s);
		return -EINVAL;
	}
	if (s->ops->spanconfig) {
		s->lineconfig = lc.lineconfig;
		s->lbo = lc.lbo;
		s->txlevel = lc.lbo;
		s->rxlevel = 0;

		res = s->ops->spanconfig(file, s, &lc);
	}
	put_span(s);
	return res;
}

static int dahdi_ioctl_startup(struct file *file, unsigned long data)
{
	/* I/O CTL's for control interface */
	int j;
	int res = 0;
	int x, y;
	unsigned long flags;
	struct dahdi_span *s;

	if (get_user(j, (int __user *)data))
		return -EFAULT;
	s = span_find_and_get(j);
	if (!s)
		return -ENXIO;

	if (s->flags & DAHDI_FLAG_RUNNING) {
		put_span(s);
		return 0;
	}

	if (s->ops->startup)
		res = s->ops->startup(file, s);

	if (!res) {
		/* Mark as running and hangup any channels */
		s->flags |= DAHDI_FLAG_RUNNING;
		for (x = 0; x < s->channels; x++) {
			y = dahdi_q_sig(s->chans[x]) & 0xff;
			if (y >= 0)
				s->chans[x]->rxsig = (unsigned char)y;
			spin_lock_irqsave(&s->chans[x]->lock, flags);
			dahdi_hangup(s->chans[x]);
			spin_unlock_irqrestore(&s->chans[x]->lock, flags);
			/*
			 * Set the rxhooksig back to
			 * DAHDI_RXSIG_INITIAL so that new events are
			 * queued on the channel with the actual
			 * received hook state.
			 *
			 */
			s->chans[x]->rxhooksig = DAHDI_RXSIG_INITIAL;
		}

		/* Now that this span is running, it might be selected as the
		 * master span */
		__dahdi_find_master_span();
	}
	put_span(s);
	return res;
}

static int dahdi_ioctl_shutdown(unsigned long data)
{
	/* I/O CTL's for control interface */
	int j;
	int x;
	struct dahdi_span *s;

	if (get_user(j, (int __user *)data))
		return -EFAULT;
	s = span_find_and_get(j);
	if (!s)
		return -ENXIO;

	/* Unconfigure channels */
	for (x = 0; x < s->channels; x++)
		s->chans[x]->sig = 0;

	if (s->ops->shutdown) {
		int res = s->ops->shutdown(s);
		if (res) {
			put_span(s);
			return res;
		}
	}

	s->flags &= ~DAHDI_FLAG_RUNNING;
	put_span(s);
	return 0;
}

/**
 * dahdi_is_hwec_available - Is hardware echocan available on a channel?
 * @chan: The channel to check
 *
 * Returns true if there is a hardware echocan available for the attached
 * channel, or false otherwise.
 *
 */
static bool dahdi_is_hwec_available(const struct dahdi_chan *chan)
{
	if (!chan->span || !chan->span->ops->echocan_name ||
	    !hwec_factory.get_name(chan))
		return false;

	return true;
}

static int dahdi_ioctl_attach_echocan(unsigned long data)
{
	unsigned long flags;
	struct dahdi_chan *chan;
	struct dahdi_attach_echocan ae;
	const struct dahdi_echocan_factory *new = NULL, *old;

	if (copy_from_user(&ae, (void __user *)data, sizeof(ae)))
		return -EFAULT;

	chan = chan_from_num(ae.chan);
	if (!chan)
		return -EINVAL;

	ae.echocan[sizeof(ae.echocan) - 1] = '\0';
	if (dahdi_is_hwec_available(chan)) {
		if (hwec_overrides_swec) {
			chan_dbg(GENERAL, chan,
				"Using echocan '%s' instead of requested " \
				"'%s'.\n", hwec_def_name, ae.echocan);
			/* If there is a hardware echocan available we'll
			 * always use it instead of any configured software
			 * echocan. This matches the behavior in dahdi 2.4.1.2
			 * and earlier releases. */
			strlcpy(ae.echocan, hwec_def_name, sizeof(ae.echocan));

		} else if (strcasecmp(ae.echocan, hwec_def_name) != 0) {
			chan_dbg(GENERAL, chan,
				"Using '%s' on channel even though '%s' is " \
				"available.\n", ae.echocan, hwec_def_name);
		}
	}

	if (ae.echocan[0]) {
		new = find_echocan(ae.echocan);
		if (!new)
			return -EINVAL;

		if (!new->get_name(chan)) {
			release_echocan(new);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&chan->lock, flags);
	old = chan->ec_factory;
	chan->ec_factory = new;
	spin_unlock_irqrestore(&chan->lock, flags);

	if (old)
		release_echocan(old);

	return 0;
}

static int dahdi_ioctl_sfconfig(unsigned long data)
{
	int res = 0;
	unsigned long flags;
	struct dahdi_chan *chan;
	struct dahdi_sfconfig sf;

	if (copy_from_user(&sf, (void __user *)data, sizeof(sf)))
		return -EFAULT;
	chan = chan_from_num(sf.chan);
	if (!chan)
		return -EINVAL;

	if (chan->sig != DAHDI_SIG_SF)
		return -EINVAL;

	spin_lock_irqsave(&chan->lock, flags);
	chan->rxp1 = sf.rxp1;
	chan->rxp2 = sf.rxp2;
	chan->rxp3 = sf.rxp3;
	chan->txtone = sf.txtone;
	chan->tx_v2 = sf.tx_v2;
	chan->tx_v3 = sf.tx_v3;
	chan->toneflags = sf.toneflag;
	if (sf.txtone) { /* if set to make tone for tx */
		if ((chan->txhooksig &&
		     !(sf.toneflag & DAHDI_REVERSE_TXTONE)) ||
		     ((!chan->txhooksig) &&
		       (sf.toneflag & DAHDI_REVERSE_TXTONE))) {
			set_txtone(chan, sf.txtone, sf.tx_v2, sf.tx_v3);
		} else {
			set_txtone(chan, 0, 0, 0);
		}
	}
	spin_unlock_irqrestore(&chan->lock, flags);
	return res;
}

static int dahdi_ioctl_get_version(unsigned long data)
{
	struct dahdi_versioninfo vi;
	struct ecfactory *cur;
	size_t space = sizeof(vi.echo_canceller) - 1;

	memset(&vi, 0, sizeof(vi));
	strlcpy(vi.version, dahdi_version, sizeof(vi.version));
	spin_lock(&ecfactory_list_lock);
	list_for_each_entry(cur, &ecfactory_list, list) {
		strncat(vi.echo_canceller + strlen(vi.echo_canceller),
			cur->ec->get_name(NULL), space);
		space -= strlen(cur->ec->get_name(NULL));
		if (space < 1)
			break;
		if (cur->list.next && (cur->list.next != &ecfactory_list)) {
			strncat(vi.echo_canceller + strlen(vi.echo_canceller),
				", ", space);
			space -= 2;
			if (space < 1)
				break;
		}
	}
	spin_unlock(&ecfactory_list_lock);
	if (copy_to_user((void __user *)data, &vi, sizeof(vi)))
		return -EFAULT;

	return 0;
}

static int dahdi_ioctl_maint(unsigned long data)
{
	int i;
	unsigned long flags;
	int rv;
	struct dahdi_span *s;
	struct dahdi_maintinfo maint;

	  /* get struct from user */
	if (copy_from_user(&maint, (void __user *)data, sizeof(maint)))
		return -EFAULT;
	s = span_find_and_get(maint.spanno);
	if (!s)
		return -EINVAL;
	if (!s->ops->maint) {
		put_span(s);
		return -ENOSYS;
	}
	spin_lock_irqsave(&s->lock, flags);
	  /* save current maint state */
	i = s->maintstat;
	  /* set maint mode */
	s->maintstat = maint.command;
	switch (maint.command) {
	case DAHDI_MAINT_NONE:
	case DAHDI_MAINT_LOCALLOOP:
	case DAHDI_MAINT_NETWORKLINELOOP:
	case DAHDI_MAINT_NETWORKPAYLOADLOOP:
		/* if same, ignore it */
		if (i == maint.command)
			break;
		rv = s->ops->maint(s, maint.command);
		spin_unlock_irqrestore(&s->lock, flags);
		if (rv) {
			put_span(s);
			return rv;
		}
		spin_lock_irqsave(&s->lock, flags);
		break;
	case DAHDI_MAINT_LOOPUP:
	case DAHDI_MAINT_LOOPDOWN:
		s->mainttimer = DAHDI_LOOPCODE_TIME * DAHDI_CHUNKSIZE;
		rv = s->ops->maint(s, maint.command);
		spin_unlock_irqrestore(&s->lock, flags);
		if (rv) {
			put_span(s);
			return rv;
		}
		spin_lock_irqsave(&s->lock, flags);
		break;
	case DAHDI_MAINT_FAS_DEFECT:
	case DAHDI_MAINT_MULTI_DEFECT:
	case DAHDI_MAINT_CRC_DEFECT:
	case DAHDI_MAINT_CAS_DEFECT:
	case DAHDI_MAINT_PRBS_DEFECT:
	case DAHDI_MAINT_BIPOLAR_DEFECT:
	case DAHDI_MAINT_PRBS:
	case DAHDI_RESET_COUNTERS:
	case DAHDI_MAINT_ALARM_SIM:
		/* Prevent notifying an alarm state for generic
		   maintenance functions, unless the driver is
		   already in a maint state */
		if (!i)
			s->maintstat = 0;

		rv = s->ops->maint(s, maint.command);
		spin_unlock_irqrestore(&s->lock, flags);
		if (rv) {
			put_span(s);
			return rv;
		}
		spin_lock_irqsave(&s->lock, flags);
		break;
	default:
		spin_unlock_irqrestore(&s->lock, flags);
		module_printk(KERN_NOTICE,
			      "Unknown maintenance event: %d\n",
			      maint.command);
		put_span(s);
		return -ENOSYS;
	}
	dahdi_alarm_notify(s);  /* process alarm-related events */
	spin_unlock_irqrestore(&s->lock, flags);
	put_span(s);

	return 0;
}

static int dahdi_ioctl_dynamic(unsigned int cmd, unsigned long data)
{
	bool tried_load = false;
	int res;

retry_check:
	mutex_lock(&registration_mutex);
	if (!dahdi_dynamic_ops) {
		mutex_unlock(&registration_mutex);
		if (tried_load)
			return -ENOSYS;

		request_module("dahdi_dynamic");
		tried_load = true;
		goto retry_check;
	}
	if (!try_module_get(dahdi_dynamic_ops->owner)) {
		mutex_unlock(&registration_mutex);
		return -ENOSYS;
	}
	mutex_unlock(&registration_mutex);

	res = dahdi_dynamic_ops->ioctl(cmd, data);
	module_put(dahdi_dynamic_ops->owner);
	return res;
}

static int
dahdi_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	switch (cmd) {
	case DAHDI_INDIRECT:
		return dahdi_ioctl_indirect(file, data);
	case DAHDI_SPANCONFIG:
		return dahdi_ioctl_spanconfig(file, data);
	case DAHDI_STARTUP:
		return dahdi_ioctl_startup(file, data);
	case DAHDI_SHUTDOWN:
		return dahdi_ioctl_shutdown(data);
	case DAHDI_ATTACH_ECHOCAN:
		return dahdi_ioctl_attach_echocan(data);
	case DAHDI_CHANCONFIG:
		return dahdi_ioctl_chanconfig(file, data);
	case DAHDI_SFCONFIG:
		return dahdi_ioctl_sfconfig(data);
	case DAHDI_DEFAULTZONE:
		return dahdi_ioctl_defaultzone(data);
	case DAHDI_LOADZONE:
		return dahdi_ioctl_loadzone(data);
	case DAHDI_FREEZONE:
		return dahdi_ioctl_freezone(data);
	case DAHDI_SET_DIALPARAMS:
		return dahdi_ioctl_set_dialparams(data);
	case DAHDI_GET_DIALPARAMS:
		return dahdi_ioctl_get_dialparams(data);
	case DAHDI_GETVERSION:
		return dahdi_ioctl_get_version(data);
	case DAHDI_MAINT:
		return dahdi_ioctl_maint(data);
	case DAHDI_DYNAMIC_CREATE:
	case DAHDI_DYNAMIC_DESTROY:
		return dahdi_ioctl_dynamic(cmd, data);
	case DAHDI_EC_LICENSE_CHALLENGE:
	case DAHDI_EC_LICENSE_RESPONSE:
		if (dahdi_hpec_ioctl) {
			return dahdi_hpec_ioctl(cmd, data);
		} else {
			request_module("dahdi_echocan_hpec");
			if (dahdi_hpec_ioctl)
				return dahdi_hpec_ioctl(cmd, data);
		}
		return -ENOSYS;
	}

	return dahdi_common_ioctl(file, cmd, data);
}

static int ioctl_dahdi_dial(struct dahdi_chan *chan, unsigned long data)
{
	struct dahdi_dialoperation *tdo;
	unsigned long flags;
	char *s;
	int rv;
	void __user * const user_data = (void __user *)data;

	tdo = kmalloc(sizeof(*tdo), GFP_KERNEL);

	if (!tdo)
		return -ENOMEM;

	if (copy_from_user(tdo, user_data, sizeof(*tdo)))
		return -EFAULT;
	rv = 0;
	/* Force proper NULL termination and uppercase entry */
	tdo->dialstr[DAHDI_MAX_DTMF_BUF - 1] = '\0';
	for (s = tdo->dialstr; *s; s++)
		*s = toupper(*s);
	spin_lock_irqsave(&chan->lock, flags);
	if (!chan->curzone) {
		spin_unlock_irqrestore(&chan->lock, flags);
		/* The tone zones are loaded by dahdi_cfg from /etc/dahdi/system.conf */
		module_printk(KERN_WARNING, "Cannot dial until a tone zone is loaded.\n");
		return -ENODATA;
	}
	switch (tdo->op) {
	case DAHDI_DIAL_OP_CANCEL:
		chan->curtone = NULL;
		chan->dialing = 0;
		chan->txdialbuf[0] = '\0';
		chan->tonep = 0;
		chan->pdialcount = 0;
		break;
	case DAHDI_DIAL_OP_REPLACE:
		strcpy(chan->txdialbuf, tdo->dialstr);
		chan->dialing = 1;
		__do_dtmf(chan);
		break;
	case DAHDI_DIAL_OP_APPEND:
		if (strlen(tdo->dialstr) + strlen(chan->txdialbuf) >= (DAHDI_MAX_DTMF_BUF - 1)) {
			rv = -EBUSY;
			break;
		}
		strlcpy(chan->txdialbuf + strlen(chan->txdialbuf), tdo->dialstr,
			DAHDI_MAX_DTMF_BUF - strlen(chan->txdialbuf));
		if (!chan->dialing) {
			chan->dialing = 1;
			__do_dtmf(chan);
		}
		break;
	default:
		rv = -EINVAL;
	}
	spin_unlock_irqrestore(&chan->lock, flags);
	return rv;
}

static int dahdi_ioctl_setconf(struct file *file, unsigned long data)
{
	struct dahdi_confinfo conf;
	struct dahdi_chan *chan;
	struct dahdi_chan *conf_chan = NULL;
	unsigned long flags;
	unsigned int confmode;
	int oldconf;
	enum {NONE, ENABLE_HWPREEC, DISABLE_HWPREEC} preec = NONE;

	if (copy_from_user(&conf, (void __user *)data, sizeof(conf)))
		return -EFAULT;

	confmode = conf.confmode & DAHDI_CONF_MODE_MASK;

	chan = (conf.chan) ? chan_from_num(conf.chan) :
			     chan_from_file(file);
	if (!chan)
		return -EINVAL;

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;

	if ((DAHDI_CONF_DIGITALMON == confmode) ||
	    is_monitor_mode(conf.confmode)) {
		conf_chan = chan_from_num(conf.confno);
		if (!conf_chan)
			return -EINVAL;
	} else {
		/* make sure conf number makes sense, too */
		if ((conf.confno < -1) || (conf.confno > DAHDI_MAX_CONF))
			return -EINVAL;
	}

	/* if taking off of any conf, must have 0 mode */
	if ((!conf.confno) && conf.confmode)
		return -EINVAL;
	/* likewise if 0 mode must have no conf */
	if ((!conf.confmode) && conf.confno)
		return -EINVAL;
	dahdi_check_conf(conf.confno);
	conf.chan = chan->channo;  /* return with real channel # */
	spin_lock_irqsave(&chan_lock, flags);
	spin_lock(&chan->lock);
	if (conf.confno == -1)
		conf.confno = dahdi_first_empty_conference();
	if ((conf.confno < 1) && (conf.confmode)) {
		/* No more empty conferences */
		spin_unlock(&chan->lock);
		spin_unlock_irqrestore(&chan_lock, flags);
		return -EBUSY;
	}
	  /* if changing confs, clear last added info */
	if (conf.confno != chan->confna) {
		memset(chan->conflast, 0, DAHDI_MAX_CHUNKSIZE);
		memset(chan->conflast1, 0, DAHDI_MAX_CHUNKSIZE);
		memset(chan->conflast2, 0, DAHDI_MAX_CHUNKSIZE);
	}
	oldconf = chan->confna;  /* save old conference number */
	chan->confna = conf.confno;   /* set conference number */
	chan->conf_chan = conf_chan;
	chan->confmode = conf.confmode;  /* set conference mode */
	chan->_confn = 0;		     /* Clear confn */
	if (chan->span && chan->span->ops->dacs) {
		if ((confmode == DAHDI_CONF_DIGITALMON) &&
		    (chan->txgain == defgain) &&
		    (chan->rxgain == defgain) &&
		    (conf_chan->txgain == defgain) &&
		    (conf_chan->rxgain == defgain)) {
			dahdi_chan_dacs(chan, conf_chan);
		} else {
			dahdi_disable_dacs(chan);
		}
	}
	/* if we are going onto a conf */
	if (conf.confno &&
	    (confmode == DAHDI_CONF_CONF ||
	     confmode == DAHDI_CONF_CONFANN ||
	     confmode == DAHDI_CONF_CONFMON ||
	     confmode == DAHDI_CONF_CONFANNMON ||
	     confmode == DAHDI_CONF_REALANDPSEUDO)) {
		/* Get alias */
		chan->_confn = dahdi_get_conf_alias(conf.confno);
	}

	spin_unlock(&chan->lock);

	if (conf_chan) {
		if ((confmode == DAHDI_CONF_MONITOR_RX_PREECHO) ||
		    (confmode == DAHDI_CONF_MONITOR_TX_PREECHO) ||
		    (confmode == DAHDI_CONF_MONITORBOTH_PREECHO)) {
			if (!conf_chan->readchunkpreec) {
				void *temp = kmalloc(sizeof(short) *
						DAHDI_CHUNKSIZE, GFP_ATOMIC);
				if (temp) {
					preec = ENABLE_HWPREEC;

					spin_lock(&conf_chan->lock);
					conf_chan->readchunkpreec = temp;
					spin_unlock(&conf_chan->lock);
				}
			}
		} else {
			preec = DISABLE_HWPREEC;

			spin_lock(&conf_chan->lock);
			kfree(conf_chan->readchunkpreec);
			conf_chan->readchunkpreec = NULL;
			spin_unlock(&conf_chan->lock);
		}
	}

	spin_unlock_irqrestore(&chan_lock, flags);

	if (ENABLE_HWPREEC == preec) {
		int res = dahdi_enable_hw_preechocan(conf_chan);
		if (res) {
			spin_lock_irqsave(&chan_lock, flags);
			spin_lock(&conf_chan->lock);
			kfree(conf_chan->readchunkpreec);
			conf_chan->readchunkpreec = NULL;
			spin_unlock(&conf_chan->lock);
			spin_unlock_irqrestore(&chan_lock, flags);
		}
		return res;
	} else if (DISABLE_HWPREEC == preec) {
		dahdi_disable_hw_preechocan(conf_chan);
	}

	dahdi_check_conf(oldconf);
	if (copy_to_user((void __user *)data, &conf, sizeof(conf)))
		return -EFAULT;
	return 0;
}

/**
 * dahdi_ioctl_confdiag() - Output debug info about conferences to console.
 *
 * This is a pure debugging aide since the only result is to the console.
 *
 * TODO: Does anyone use this anymore?  Should it be hidden behind a debug
 * compile time option?
 */
static int dahdi_ioctl_confdiag(struct file *file, unsigned long data)
{
	struct dahdi_chan *chan;
	unsigned long flags;
	int i;
	int j;
	int c;

	chan = chan_from_file(file);
	if (!chan)
		return -EINVAL;

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;

	get_user(j, (int __user *)data);  /* get conf # */

	/* loop thru the interesting ones */
	for (i = ((j) ? j : 1); i <= ((j) ? j : DAHDI_MAX_CONF); i++) {
		struct dahdi_span *s;
		struct pseudo_chan *pseudo;
		c = 0;
		spin_lock_irqsave(&chan_lock, flags);
		list_for_each_entry(s, &span_list, spans_node) {
			int k;
			for (k = 0; k < s->channels; k++) {
				chan = s->chans[k];
				if (chan->confna != i)
					continue;
				if (!c)
					module_printk(KERN_NOTICE, "Conf #%d:\n", i);
				c = 1;
				module_printk(KERN_NOTICE, "chan %d, mode %x\n",
					      chan->channo, chan->confmode);
			}
		}
		list_for_each_entry(pseudo, &pseudo_chans, node) {
			/* skip if not in this conf */
			if (pseudo->chan.confna != i)
				continue;
			if (!c)
				module_printk(KERN_NOTICE, "Conf #%d:\n", i);
			c = 1;
			module_printk(KERN_NOTICE, "chan %d, mode %x\n",
				      pseudo->chan.channo, pseudo->chan.confmode);
		}
		spin_unlock_irqrestore(&chan_lock, flags);
		if (c)
			module_printk(KERN_NOTICE, "\n");
	}
	return 0;
}

static int dahdi_ioctl_getconf(struct file *file, unsigned long data)
{
	struct dahdi_confinfo conf;
	struct dahdi_chan *chan;

	if (copy_from_user(&conf, (void __user *)data, sizeof(conf)))
		return -EFAULT;

	chan = (!conf.chan) ? chan_from_file(file) :
			      chan_from_num(conf.chan);
	if (!chan)
		return -EINVAL;

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;
	conf.chan = chan->channo;  /* get channel number */
	conf.confno = chan->confna;  /* get conference number */
	conf.confmode = chan->confmode; /* get conference mode */
	if (copy_to_user((void __user *)data, &conf, sizeof(conf)))
		return -EFAULT;

	return 0;
}

/**
 * dahdi_ioctl_iomux() - Wait for *something* to happen.
 *
 * This is now basically like the wait_event_interruptible function, but with
 * a much more involved wait condition.
 */
static int dahdi_ioctl_iomux(struct file *file, unsigned long data)
{
	struct dahdi_chan *const chan = chan_from_file(file);
	unsigned long flags;
	unsigned int iomask;
	int ret = 0;
	DEFINE_WAIT(wait);

	if (get_user(iomask, (int __user *)data))
		return -EFAULT;

	if (unlikely(!iomask || !chan))
		return -EINVAL;

	while (1) {
		unsigned int wait_result;

		wait_result = 0;
		prepare_to_wait(&chan->waitq, &wait, TASK_INTERRUPTIBLE);
		if (unlikely(!chan->file->private_data)) {
			/*
			 * This should never happen. Surprise device removal
			 * should lead us to the nodev_* file_operations
			 */
			msleep(5);
			module_printk(KERN_ERR, "%s: NODEV\n", __func__);
			ret = -ENODEV;
			break;
		}

		spin_lock_irqsave(&chan->lock, flags);
		chan->iomask = iomask;
		if (iomask & DAHDI_IOMUX_READ) {
			if ((chan->outreadbuf > -1)  && !chan->rxdisable)
				wait_result |= DAHDI_IOMUX_READ;
		}
		if (iomask & DAHDI_IOMUX_WRITE) {
			if (chan->inwritebuf > -1)
				wait_result |= DAHDI_IOMUX_WRITE;
		}
		if (iomask & DAHDI_IOMUX_WRITEEMPTY) {
			/* if everything empty -- be sure the transmitter is
			 * enabled */
			chan->txdisable = 0;
			if (chan->outwritebuf < 0)
				wait_result |= DAHDI_IOMUX_WRITEEMPTY;
		}
		if (iomask & DAHDI_IOMUX_SIGEVENT) {
			if (chan->eventinidx != chan->eventoutidx)
				wait_result |= DAHDI_IOMUX_SIGEVENT;
		}
		spin_unlock_irqrestore(&chan->lock, flags);

		if (wait_result || (iomask & DAHDI_IOMUX_NOWAIT)) {
			put_user(wait_result, (int __user *)data);
			break;
		}

		if (signal_pending(current)) {
			finish_wait(&chan->waitq, &wait);
			return -ERESTARTSYS;
		}

		schedule();
	}

	finish_wait(&chan->waitq, &wait);
	spin_lock_irqsave(&chan->lock, flags);
	chan->iomask = 0;
	spin_unlock_irqrestore(&chan->lock, flags);
	return ret;
}

#ifdef CONFIG_DAHDI_MIRROR
static int dahdi_ioctl_rxmirror(struct file *file, unsigned long data)
{
	int res;
	int i;
	unsigned long flags;
	struct dahdi_chan *const chan = chan_from_file(file);
	struct dahdi_chan *srcmirror;

	if (!chan || chan->srcmirror)
		return -ENODEV;

	res = get_user(i, (int __user *)data);
	if (res)
		return res;

	srcmirror = chan_from_num(i);
	if (!srcmirror)
		return -EINVAL;

	module_printk(KERN_INFO, "Chan %d rx mirrored to %d\n",
		      srcmirror->channo, chan->channo);

	spin_lock_irqsave(&srcmirror->lock, flags);
	if (srcmirror->rxmirror == NULL)
		srcmirror->rxmirror = chan;

	spin_unlock_irqrestore(&srcmirror->lock, flags);
	if (srcmirror->rxmirror != chan) {
		module_printk(KERN_INFO, "Chan %d cannot be rxmirrored, " \
			      "already in use\n", srcmirror->channo);
		return -EFAULT;
	}

	spin_lock_irqsave(&chan->lock, flags);
	chan->srcmirror = srcmirror;
	chan->flags = srcmirror->flags;
	chan->sig =  srcmirror->sig;
	clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
	spin_unlock_irqrestore(&chan->lock, flags);

	return 0;
}

static int dahdi_ioctl_txmirror(struct file *file, unsigned long data)
{
	int res;
	int i;
	unsigned long flags;
	struct dahdi_chan *const chan = chan_from_file(file);
	struct dahdi_chan *srcmirror;

	if (!chan || chan->srcmirror)
		return -ENODEV;

	res = get_user(i, (int __user *)data);
	if (res)
		return res;

	srcmirror = chan_from_num(i);
	if (!srcmirror)
		return -EINVAL;

	module_printk(KERN_INFO, "Chan %d tx mirrored to %d\n",
		      srcmirror->channo, chan->channo);

	spin_lock_irqsave(&srcmirror->lock, flags);
	srcmirror->txmirror = chan;
	if (srcmirror->txmirror == NULL)
		srcmirror->txmirror = chan;
	spin_unlock_irqrestore(&srcmirror->lock, flags);

	if (srcmirror->txmirror != chan) {
		module_printk(KERN_INFO, "Chan %d cannot be txmirrored, " \
			      "already in use\n", i);
		return -EFAULT;
	}

	spin_lock_irqsave(&chan->lock, flags);
	chan->srcmirror = srcmirror;
	chan->flags = srcmirror->flags;
	chan->sig =  srcmirror->sig;
	clear_bit(DAHDI_FLAGBIT_OPEN, &chan->flags);
	spin_unlock_irqrestore(&chan->lock, flags);

	return 0;
}
#endif /* CONFIG_DAHDI_MIRROR */

static int
dahdi_chanandpseudo_ioctl(struct file *file, unsigned int cmd,
			  unsigned long data)
{
	struct dahdi_chan *chan = chan_from_file(file);
	union {
		struct dahdi_bufferinfo bi;
		struct dahdi_ring_cadence cad;
	} stack;
	unsigned long flags;
	int i, j, rv;
	void __user * const user_data = (void __user *)data;

	if (!chan)
		return -EINVAL;
	switch(cmd) {
#ifdef CONFIG_DAHDI_MIRROR
	case DAHDI_RXMIRROR:
		return dahdi_ioctl_rxmirror(file, data);

	case DAHDI_TXMIRROR:
		return dahdi_ioctl_txmirror(file, data);
#endif /* CONFIG_DAHDI_MIRROR */

	case DAHDI_DIALING:
		spin_lock_irqsave(&chan->lock, flags);
		j = chan->dialing;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (copy_to_user(user_data, &j, sizeof(int)))
			return -EFAULT;
		return 0;
	case DAHDI_DIAL:
		return ioctl_dahdi_dial(chan, data);
	case DAHDI_GET_BUFINFO:
		memset(&stack.bi, 0, sizeof(stack.bi));
		stack.bi.rxbufpolicy = chan->rxbufpolicy;
		stack.bi.txbufpolicy = chan->txbufpolicy;
		stack.bi.numbufs = chan->numbufs;
		stack.bi.bufsize = chan->blocksize;
		/* XXX FIXME! XXX */
		stack.bi.readbufs = -1;
		stack.bi.writebufs = -1;
		if (copy_to_user(user_data, &stack.bi, sizeof(stack.bi)))
			return -EFAULT;
		break;
	case DAHDI_SET_BUFINFO:
		if (copy_from_user(&stack.bi, user_data, sizeof(stack.bi)))
			return -EFAULT;
		if (stack.bi.bufsize > DAHDI_MAX_BLOCKSIZE)
			return -EINVAL;
		if (stack.bi.bufsize < 16)
			return -EINVAL;
		if (stack.bi.bufsize * stack.bi.numbufs > DAHDI_MAX_BUF_SPACE)
			return -EINVAL;
		/* It does not make sense to allow user mode to change the
		 * receive buffering policy.  DAHDI always provides received
		 * buffers to upper layers immediately.  Transmission is
		 * different since we might want to allow the kernel to build
		 * up a buffer in order to prevent underruns from the
		 * interrupt context. */
		chan->txbufpolicy = stack.bi.txbufpolicy & 0x3;
		if ((rv = dahdi_reallocbufs(chan,  stack.bi.bufsize, stack.bi.numbufs)))
			return (rv);
		break;
	case DAHDI_GET_BLOCKSIZE:  /* get blocksize */
		/* return block size */
		put_user(chan->blocksize, (int __user *)data);
		break;
	case DAHDI_SET_BLOCKSIZE:  /* set blocksize */
		get_user(j, (int __user *)data);
		/* cannot be larger than max amount */
		if (j > DAHDI_MAX_BLOCKSIZE) return(-EINVAL);
		/* cannot be less then 16 */
		if (j < 16) return(-EINVAL);
		/* allocate a single kernel buffer which we then
		sub divide into four pieces */
		if ((rv = dahdi_reallocbufs(chan, j, chan->numbufs)))
			return (rv);
		break;
	case DAHDI_FLUSH:  /* flush input buffer, output buffer, and/or event queue */
		get_user(i, (int __user *)data);  /* get param */
		spin_lock_irqsave(&chan->lock, flags);
		if (i & DAHDI_FLUSH_READ)  /* if for read (input) */
		   {
			  /* initialize read buffers and pointers */
			chan->inreadbuf = 0;
			chan->outreadbuf = -1;
			for (j=0;j<chan->numbufs;j++) {
				/* Do we need this? */
				chan->readn[j] = 0;
				chan->readidx[j] = 0;
			}
			wake_up_interruptible(&chan->waitq);  /* wake_up_interruptible waiting on read */
		   }
		if (i & DAHDI_FLUSH_WRITE) /* if for write (output) */
		   {
			  /* initialize write buffers and pointers */
			chan->outwritebuf = -1;
			chan->inwritebuf = 0;
			for (j=0;j<chan->numbufs;j++) {
				/* Do we need this? */
				chan->writen[j] = 0;
				chan->writeidx[j] = 0;
			}
			wake_up_interruptible(&chan->waitq); /* wake_up_interruptible waiting on write */
		   }
		if (i & DAHDI_FLUSH_EVENT) /* if for events */
		   {
			   /* initialize the event pointers */
			chan->eventinidx = chan->eventoutidx = 0;
		   }
		spin_unlock_irqrestore(&chan->lock, flags);
		break;
	case DAHDI_SYNC:  /* wait for no tx */
		for(;;)  /* loop forever */
		   {
			spin_lock_irqsave(&chan->lock, flags);
			  /* Know if there is a write pending */
			i = (chan->outwritebuf > -1);
			spin_unlock_irqrestore(&chan->lock, flags);
			if (!i)
				break; /* skip if none */
			rv = wait_event_interruptible(chan->waitq,
						      (!chan->file->private_data || chan->outwritebuf > -1));
			if (rv)
				return rv;
			if (unlikely(!chan->file->private_data))
				return -ENODEV;
		   }
		break;
	case DAHDI_IOMUX: /* wait for something to happen */
		return dahdi_ioctl_iomux(file, data);

	case DAHDI_GETEVENT:  /* Get event on queue */
		  /* set up for no event */
		j = DAHDI_EVENT_NONE;
		spin_lock_irqsave(&chan->lock, flags);
		  /* if some event in queue */
		if (chan->eventinidx != chan->eventoutidx)
		   {
			j = chan->eventbuf[chan->eventoutidx++];
			  /* get the data, bump index */
			  /* if index overflow, set to beginning */
			if (chan->eventoutidx >= DAHDI_MAX_EVENTSIZE)
				chan->eventoutidx = 0;
		   }
		spin_unlock_irqrestore(&chan->lock, flags);
		put_user(j, (int __user *)data);
		break;
	case DAHDI_CONFMUTE:  /* set confmute flag */
		get_user(j, (int __user *)data);  /* get conf # */
		if (!(chan->flags & DAHDI_FLAG_AUDIO)) return (-EINVAL);
		spin_lock_irqsave(&chan_lock, flags);
		chan->confmute = j;
		spin_unlock_irqrestore(&chan_lock, flags);
		break;
	case DAHDI_GETCONFMUTE:  /* get confmute flag */
		if (!(chan->flags & DAHDI_FLAG_AUDIO)) return (-EINVAL);
		j = chan->confmute;
		put_user(j, (int __user *)data);  /* get conf # */
		rv = 0;
		break;
	case DAHDI_SETTONEZONE:
		get_user(j, (int __user *) data);
		rv = set_tone_zone(chan, j);
		return rv;
	case DAHDI_GETTONEZONE:
		spin_lock_irqsave(&chan->lock, flags);
		j = (chan->curzone) ? chan->curzone->num : 0;
		spin_unlock_irqrestore(&chan->lock, flags);
		put_user(j, (int __user *) data);
		break;
	case DAHDI_SENDTONE:
		get_user(j, (int __user *)data);
		spin_lock_irqsave(&chan->lock, flags);
		rv = start_tone(chan, j);
		spin_unlock_irqrestore(&chan->lock, flags);
		return rv;
	case DAHDI_GETCONF_V1: /* intentional drop through */
	case DAHDI_GETCONF:  /* get conf stuff */
		return dahdi_ioctl_getconf(file, data);

	case DAHDI_SETCONF_V1: /* Intentional fall through. */
	case DAHDI_SETCONF:  /* set conf stuff */
		return dahdi_ioctl_setconf(file, data);

	case DAHDI_CONFDIAG_V1: /* Intentional fall-through */
	case DAHDI_CONFDIAG:  /* output diagnostic info to console */
		return dahdi_ioctl_confdiag(file, data);

	case DAHDI_CHANNO:  /* get channel number of stream */
		/* return channel number */
		put_user(chan->channo, (int __user *)data);
		break;
	case DAHDI_SETLAW:
		get_user(j, (int __user *)data);
		if ((j < 0) || (j > DAHDI_LAW_ALAW))
			return -EINVAL;
		dahdi_set_law(chan, j);
		break;
	case DAHDI_SETLINEAR:
		get_user(j, (int __user *)data);
		/* Makes no sense on non-audio channels */
		if (!(chan->flags & DAHDI_FLAG_AUDIO))
			return -EINVAL;

		if (j)
			chan->flags |= DAHDI_FLAG_LINEAR;
		else
			chan->flags &= ~DAHDI_FLAG_LINEAR;
		break;
	case DAHDI_SETCADENCE:
		if (data) {
			/* Use specific ring cadence */
			if (copy_from_user(&stack.cad, user_data,
					   sizeof(stack.cad))) {
				return -EFAULT;
			}
			memcpy(chan->ringcadence, &stack.cad, sizeof(chan->ringcadence));
			chan->firstcadencepos = 0;
			/* Looking for negative ringing time indicating where to loop back into ringcadence */
			for (i=0; i<DAHDI_MAX_CADENCE; i+=2 ) {
				if (chan->ringcadence[i]<0) {
					chan->ringcadence[i] *= -1;
					chan->firstcadencepos = i;
					break;
				}
			}
		} else {
			/* Reset to default */
			chan->firstcadencepos = 0;
			if (chan->curzone) {
				memcpy(chan->ringcadence, chan->curzone->ringcadence, sizeof(chan->ringcadence));
				/* Looking for negative ringing time indicating where to loop back into ringcadence */
				for (i=0; i<DAHDI_MAX_CADENCE; i+=2 ) {
					if (chan->ringcadence[i]<0) {
						chan->ringcadence[i] *= -1;
						chan->firstcadencepos = i;
						break;
					}
				}
			} else {
				memset(chan->ringcadence, 0, sizeof(chan->ringcadence));
				chan->ringcadence[0] = chan->starttime;
				chan->ringcadence[1] = DAHDI_RINGOFFTIME;
			}
		}
		break;
	default:
		/* Check for common ioctl's and private ones */
		rv = dahdi_common_ioctl(file, cmd, data);
		/* if no span, just return with value */
		if (!chan->span) return rv;
		if ((rv == -ENOTTY) && chan->span->ops->ioctl)
			rv = chan->span->ops->ioctl(chan, cmd, data);
		return rv;

	}
	return 0;
}

#ifdef CONFIG_DAHDI_PPP
/*
 * This is called at softirq (BH) level when there are calls
 * we need to make to the ppp_generic layer.  We do it this
 * way because the ppp_generic layer functions may not be called
 * at interrupt level.
 */
static void do_ppp_calls(unsigned long data)
{
	struct dahdi_chan *chan = (struct dahdi_chan *) data;
	struct sk_buff *skb;

	if (!chan->ppp)
		return;
	if (chan->do_ppp_wakeup) {
		chan->do_ppp_wakeup = 0;
		ppp_output_wakeup(chan->ppp);
	}
	while ((skb = skb_dequeue(&chan->ppp_rq)) != NULL)
		ppp_input(chan->ppp, skb);
	if (chan->do_ppp_error) {
		chan->do_ppp_error = 0;
		ppp_input_error(chan->ppp, 0);
	}
}
#endif

static int
ioctl_echocancel(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
		 const void __user *data)
{
	struct dahdi_echocan_state *ec = NULL, *ec_state;
	const struct dahdi_echocan_factory *ec_current;
	struct dahdi_echocanparam *params;
	int ret = 0;
	unsigned long flags;

	if (ecp->param_count > DAHDI_MAX_ECHOCANPARAMS)
		return -E2BIG;

	if (ecp->tap_length == 0) {
		/* disable mode, don't need to inspect params */
		spin_lock_irqsave(&chan->lock, flags);
		ec_state = chan->ec_state;
		chan->ec_state = NULL;
		ec_current = chan->ec_current;
		chan->ec_current = NULL;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (ec_state) {
			ec_state->ops->echocan_free(chan, ec_state);
			release_echocan(ec_current);
		}

		return 0;
	}

	params = kmalloc(sizeof(params[0]) * DAHDI_MAX_ECHOCANPARAMS, GFP_KERNEL);

	if (!params)
		return -ENOMEM;

	/* enable mode, need the params */

	if (copy_from_user(params, data,
			   sizeof(params[0]) * ecp->param_count)) {
		ret = -EFAULT;
		goto exit_with_free;
	}

	/* free any echocan that may be on the channel already */
	spin_lock_irqsave(&chan->lock, flags);
	ec_state = chan->ec_state;
	chan->ec_state = NULL;
	ec_current = chan->ec_current;
	chan->ec_current = NULL;
	spin_unlock_irqrestore(&chan->lock, flags);
	if (ec_state) {
		ec_state->ops->echocan_free(chan, ec_state);
		release_echocan(ec_current);
	}

	switch (ecp->tap_length) {
	case 32:
	case 64:
	case 128:
	case 256:
	case 512:
	case 1024:
		break;
	default:
		ecp->tap_length = deftaps;
	}

	ec_current = NULL;

	if (chan->ec_factory) {
		/* try to get another reference to the module providing
		   this channel's echo canceler */
		if (!try_module_get(chan->ec_factory->owner)) {
			module_printk(KERN_ERR, "Cannot get a reference to the"
				      " '%s' echo canceler\n",
				      chan->ec_factory->get_name(chan));
			goto exit_with_free;
		}

		/* got the reference, copy the pointer and use it for making
		   an echo canceler instance if possible */
		ec_current = chan->ec_factory;

		ret = ec_current->echocan_create(chan, ecp, params, &ec);
		if (ret) {
			release_echocan(ec_current);

			goto exit_with_free;
		}
		if (!ec) {
			module_printk(KERN_ERR, "%s failed to allocate an " \
				      "dahdi_echocan_state instance.\n",
				      ec_current->get_name(chan));
			ret = -EFAULT;
			goto exit_with_free;
		}
	}

	if (ec) {
		spin_lock_irqsave(&chan->lock, flags);
		chan->ec_current = ec_current;
		chan->ec_state = ec;
		ec->status.mode = ECHO_MODE_ACTIVE;
		if (!ec->features.CED_tx_detect) {
			echo_can_disable_detector_init(&chan->ec_state->txecdis);
		}
		if (!ec->features.CED_rx_detect) {
			echo_can_disable_detector_init(&chan->ec_state->rxecdis);
		}
		spin_unlock_irqrestore(&chan->lock, flags);
	}

exit_with_free:
	kfree(params);

	return ret;
}

static void set_echocan_fax_mode(struct dahdi_chan *chan, unsigned int channo, const char *reason, unsigned int enable)
{
	if (enable) {
		if (!chan->ec_state)
			module_printk(KERN_NOTICE, "Ignoring FAX mode request because of %s for channel %d with no echo canceller\n", reason, channo);
		else if (chan->ec_state->status.mode == ECHO_MODE_FAX)
			module_printk(KERN_NOTICE, "Ignoring FAX mode request because of %s for echo canceller already in FAX mode on channel %d\n", reason, channo);
		else if (chan->ec_state->status.mode != ECHO_MODE_ACTIVE)
			module_printk(KERN_NOTICE, "Ignoring FAX mode request because of %s for echo canceller not in active mode on channel %d\n", reason, channo);
		else if (chan->ec_state->features.NLP_automatic) {
			/* for echocans that automatically do the right thing, just
			 * mark it as being in FAX mode without making any
			 * changes, as none are necessary.
			*/
			chan->ec_state->status.mode = ECHO_MODE_FAX;
		} else if (chan->ec_state->features.NLP_toggle) {
			module_printk(KERN_NOTICE, "Disabled echo canceller NLP because of %s on channel %d\n", reason, channo);
			dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_DISABLED);
			chan->ec_state->ops->echocan_NLP_toggle(chan->ec_state, 0);
			chan->ec_state->status.mode = ECHO_MODE_FAX;
		} else {
			module_printk(KERN_NOTICE, "Idled echo canceller because of %s on channel %d\n", reason, channo);
			chan->ec_state->status.mode = ECHO_MODE_IDLE;
		}
	} else {
		if (!chan->ec_state)
			module_printk(KERN_NOTICE, "Ignoring voice mode request because of %s for channel %d with no echo canceller\n", reason, channo);
		else if (chan->ec_state->status.mode == ECHO_MODE_ACTIVE)
			module_printk(KERN_NOTICE, "Ignoring voice mode request because of %s for echo canceller already in voice mode on channel %d\n", reason, channo);
		else if ((chan->ec_state->status.mode != ECHO_MODE_FAX) &&
			 (chan->ec_state->status.mode != ECHO_MODE_IDLE))
			module_printk(KERN_NOTICE, "Ignoring voice mode request because of %s for echo canceller not in FAX or idle mode on channel %d\n", reason, channo);
		else if (chan->ec_state->features.NLP_automatic) {
			/* for echocans that automatically do the right thing, just
			 * mark it as being in active mode without making any
			 * changes, as none are necessary.
			*/
			chan->ec_state->status.mode = ECHO_MODE_ACTIVE;
		} else if (chan->ec_state->features.NLP_toggle) {
			module_printk(KERN_NOTICE, "Enabled echo canceller NLP because of %s on channel %d\n", reason, channo);
			dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_ENABLED);
			chan->ec_state->ops->echocan_NLP_toggle(chan->ec_state, 1);
			chan->ec_state->status.mode = ECHO_MODE_ACTIVE;
		} else {
			module_printk(KERN_NOTICE, "Activated echo canceller because of %s on channel %d\n", reason, channo);
			chan->ec_state->status.mode = ECHO_MODE_ACTIVE;
		}
	}
}

static inline bool
is_txstate(struct dahdi_chan *const chan, const int txstate)
{
	bool ret;
	unsigned long flags;

	spin_lock_irqsave(&chan->lock, flags);
	ret = (txstate == chan->txstate);
	spin_unlock_irqrestore(&chan->lock, flags);
	return ret;
}

static int dahdi_chan_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	struct dahdi_chan *const chan = chan_from_file(file);
	unsigned long flags;
	int j;
	int ret;
	int oldconf;
	const void *rxgain = NULL;

	if (!chan)
		return -ENOSYS;

	WARN_ON(!chan->master);

	switch(cmd) {
	case DAHDI_SETSIGFREEZE:
		get_user(j, (int __user *)data);
		spin_lock_irqsave(&chan->lock, flags);
		if (j) {
			chan->flags |= DAHDI_FLAG_SIGFREEZE;
		} else {
			chan->flags &= ~DAHDI_FLAG_SIGFREEZE;
		}
		spin_unlock_irqrestore(&chan->lock, flags);
		break;
	case DAHDI_GETSIGFREEZE:
		spin_lock_irqsave(&chan->lock, flags);
		if (chan->flags & DAHDI_FLAG_SIGFREEZE)
			j = 1;
		else
			j = 0;
		spin_unlock_irqrestore(&chan->lock, flags);
		put_user(j, (int __user *)data);
		break;
	case DAHDI_AUDIOMODE:
		/* Only literal clear channels can be put in  */
		if (chan->sig != DAHDI_SIG_CLEAR) return (-EINVAL);
		get_user(j, (int __user *)data);
		if (j) {
			spin_lock_irqsave(&chan->lock, flags);
			chan->flags |= DAHDI_FLAG_AUDIO;
			chan->flags &= ~(DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS);
			spin_unlock_irqrestore(&chan->lock, flags);
		} else {
			/* Coming out of audio mode, also clear all
			   conferencing and gain related info as well
			   as echo canceller */
			struct dahdi_echocan_state *ec_state;
			const struct dahdi_echocan_factory *ec_current;

			spin_lock_irqsave(&chan->lock, flags);
			chan->flags &= ~DAHDI_FLAG_AUDIO;
			/* save old conf number, if any */
			oldconf = chan->confna;
			  /* initialize conference variables */
			chan->_confn = 0;
			chan->confna = 0;
			chan->conf_chan = NULL;
			dahdi_disable_dacs(chan);
			chan->confmode = 0;
			chan->confmute = 0;
			memset(chan->conflast, 0, sizeof(chan->conflast));
			memset(chan->conflast1, 0, sizeof(chan->conflast1));
			memset(chan->conflast2, 0, sizeof(chan->conflast2));
			ec_state = chan->ec_state;
			chan->ec_state = NULL;
			ec_current = chan->ec_current;
			chan->ec_current = NULL;
			/* release conference resource, if any to release */
			reset_conf(chan);
			if (is_gain_allocated(chan))
				rxgain = chan->rxgain;
			else
				rxgain = NULL;

			chan->rxgain = defgain;
			chan->txgain = defgain;
			spin_unlock_irqrestore(&chan->lock, flags);

			if (ec_state) {
				ec_state->ops->echocan_free(chan, ec_state);
				release_echocan(ec_current);
			}

			if (rxgain)
				kfree(rxgain);
			if (oldconf) dahdi_check_conf(oldconf);
		}
#ifdef	DAHDI_AUDIO_NOTIFY
		if (chan->span->ops->audio_notify)
			chan->span->ops->audio_notify(chan, j);
#endif
		break;
	case DAHDI_HDLCPPP:
#ifdef CONFIG_DAHDI_PPP
		if (chan->sig != DAHDI_SIG_CLEAR) return (-EINVAL);
		get_user(j, (int __user *)data);
		if (j) {
			if (!chan->ppp) {
				chan->ppp = kzalloc(sizeof(struct ppp_channel), GFP_KERNEL);
				if (chan->ppp) {
					struct dahdi_echocan_state *tec;
					const struct dahdi_echocan_factory *ec_current;

					chan->ppp->private = chan;
					chan->ppp->ops = &ztppp_ops;
					chan->ppp->mtu = DAHDI_DEFAULT_MTU_MRU;
					chan->ppp->hdrlen = 0;
					skb_queue_head_init(&chan->ppp_rq);
					chan->do_ppp_wakeup = 0;
					tasklet_init(&chan->ppp_calls, do_ppp_calls,
						     (unsigned long)chan);
					if ((ret = dahdi_reallocbufs(chan, DAHDI_DEFAULT_MTU_MRU, DAHDI_DEFAULT_NUM_BUFS))) {
						kfree(chan->ppp);
						chan->ppp = NULL;
						return ret;
					}

					if ((ret = ppp_register_channel(chan->ppp))) {
						kfree(chan->ppp);
						chan->ppp = NULL;
						return ret;
					}
					tec = chan->ec_state;
					chan->ec_state = NULL;
					ec_current = chan->ec_current;
					chan->ec_current = NULL;
					/* Make sure there's no gain */
					if (is_gain_allocated(chan))
						kfree(chan->rxgain);
					chan->rxgain = defgain;
					chan->txgain = defgain;
					chan->flags &= ~DAHDI_FLAG_AUDIO;
					chan->flags |= (DAHDI_FLAG_PPP | DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS);

					if (tec) {
						tec->ops->echocan_free(chan, tec);
						release_echocan(ec_current);
					}
				} else
					return -ENOMEM;
			}
		} else {
			chan->flags &= ~(DAHDI_FLAG_PPP | DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS);
			if (chan->ppp) {
				struct ppp_channel *ppp = chan->ppp;
				chan->ppp = NULL;
				tasklet_kill(&chan->ppp_calls);
				skb_queue_purge(&chan->ppp_rq);
				ppp_unregister_channel(ppp);
				kfree(ppp);
			}
		}
#else
		module_printk(KERN_NOTICE, "PPP support not compiled in\n");
		return -ENOSYS;
#endif
		break;
	case DAHDI_HDLCRAWMODE:
		if (chan->sig != DAHDI_SIG_CLEAR)	return (-EINVAL);
		get_user(j, (int __user *)data);
		chan->flags &= ~(DAHDI_FLAG_AUDIO | DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS);
		if (j) {
			chan->flags |= DAHDI_FLAG_HDLC;
			fasthdlc_init(&chan->rxhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
			fasthdlc_init(&chan->txhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
		}
		break;
	case DAHDI_HDLCFCSMODE:
		if (chan->sig != DAHDI_SIG_CLEAR)	return (-EINVAL);
		get_user(j, (int __user *)data);
		chan->flags &= ~(DAHDI_FLAG_AUDIO | DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS);
		if (j) {
			chan->flags |= DAHDI_FLAG_HDLC | DAHDI_FLAG_FCS;
			fasthdlc_init(&chan->rxhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
			fasthdlc_init(&chan->txhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
		}
		break;
	case DAHDI_HDLC_RATE:
		get_user(j, (int __user *)data);
		if (j == 56) {
			chan->flags |= DAHDI_FLAG_HDLC56;
		} else {
			chan->flags &= ~DAHDI_FLAG_HDLC56;
		}

		fasthdlc_init(&chan->rxhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
		fasthdlc_init(&chan->txhdlc, (chan->flags & DAHDI_FLAG_HDLC56) ? FASTHDLC_MODE_56 : FASTHDLC_MODE_64);
		break;
	case DAHDI_ECHOCANCEL_PARAMS:
	{
		struct dahdi_echocanparams ecp;

		if (!(chan->flags & DAHDI_FLAG_AUDIO))
			return -EINVAL;
		ret = copy_from_user(&ecp,
				     (struct dahdi_echocanparams __user *)data,
				     sizeof(ecp));
		if (ret)
			return -EFAULT;
		data += sizeof(ecp);
		ret = ioctl_echocancel(chan, &ecp, (void __user *)data);
		if (ret)
			return ret;
		break;
	}
	case DAHDI_ECHOCANCEL:
	{
		struct dahdi_echocanparams ecp;

		if (!(chan->flags & DAHDI_FLAG_AUDIO))
			return -EINVAL;
		get_user(j, (int __user *) data);
		ecp.tap_length = j;
		ecp.param_count = 0;
		if ((ret = ioctl_echocancel(chan, &ecp, NULL)))
			return ret;
		break;
	}
	case DAHDI_ECHOTRAIN:
		/* get pre-training time from user */
		get_user(j, (int __user *)data);
		if ((j < 0) || (j >= DAHDI_MAX_PRETRAINING))
			return -EINVAL;
		j <<= 3;
		spin_lock_irqsave(&chan->lock, flags);
		if (chan->ec_state) {
			/* Start pretraining stage */
			if (chan->ec_state->ops->echocan_traintap) {
				chan->ec_state->status.mode = ECHO_MODE_PRETRAINING;
				chan->ec_state->status.pretrain_timer = j;
			}
			spin_unlock_irqrestore(&chan->lock, flags);
		} else {
			spin_unlock_irqrestore(&chan->lock, flags);
			return -EINVAL;
		}
		break;
	case DAHDI_ECHOCANCEL_FAX_MODE:
		if (!chan->ec_state) {
			return -EINVAL;
		} else {
			get_user(j, (int __user *) data);
			spin_lock_irqsave(&chan->lock, flags);
			set_echocan_fax_mode(chan, chan->channo, "ioctl", j ? 1 : 0);
			spin_unlock_irqrestore(&chan->lock, flags);
		}
		break;
	case DAHDI_SETTXBITS:
		if (chan->sig != DAHDI_SIG_CAS)
			return -EINVAL;
		get_user(j, (int __user *)data);
		dahdi_cas_setbits(chan, j);
		break;
	case DAHDI_GETRXBITS:
		put_user(chan->rxsig, (int __user *)data);
		break;
	case DAHDI_LOOPBACK:
		get_user(j, (int __user *)data);
		spin_lock_irqsave(&chan->lock, flags);
		if (j)
			chan->flags |= DAHDI_FLAG_LOOPED;
		else
			chan->flags &= ~DAHDI_FLAG_LOOPED;
		spin_unlock_irqrestore(&chan->lock, flags);
		break;
	case DAHDI_HOOK:
		get_user(j, (int __user *)data);
		if (chan->flags & DAHDI_FLAG_CLEAR)
			return -EINVAL;
		if (chan->sig == DAHDI_SIG_CAS)
			return -EINVAL;
		/* if no span, just do nothing */
		if (!chan->span) return(0);
		spin_lock_irqsave(&chan->lock, flags);
		/* if dialing, stop it */
		chan->curtone = NULL;
		chan->dialing = 0;
		chan->txdialbuf[0] = '\0';
		chan->tonep = 0;
		chan->pdialcount = 0;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (chan->span->flags & DAHDI_FLAG_RBS) {
			switch (j) {
			case DAHDI_ONHOOK:
				spin_lock_irqsave(&chan->lock, flags);
				dahdi_hangup(chan);
				spin_unlock_irqrestore(&chan->lock, flags);
				break;
			case DAHDI_OFFHOOK:
				spin_lock_irqsave(&chan->lock, flags);
				if ((chan->txstate == DAHDI_TXSTATE_KEWL) ||
				  (chan->txstate == DAHDI_TXSTATE_AFTERKEWL)) {
					spin_unlock_irqrestore(&chan->lock, flags);
					return -EBUSY;
				}
				dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_DEBOUNCE, chan->debouncetime);
				spin_unlock_irqrestore(&chan->lock, flags);
				break;
			case DAHDI_RING:
			case DAHDI_START:
				spin_lock_irqsave(&chan->lock, flags);
				if (!chan->curzone) {
					spin_unlock_irqrestore(&chan->lock, flags);
					module_printk(KERN_WARNING, "Cannot start tone until a tone zone is loaded.\n");
					return -ENODATA;
				}
				if (chan->txstate != DAHDI_TXSTATE_ONHOOK) {
					spin_unlock_irqrestore(&chan->lock, flags);
					return -EBUSY;
				}
				if (chan->sig & __DAHDI_SIG_FXO) {
					ret = 0;
					chan->cadencepos = 0;
					ret = chan->ringcadence[0];
					dahdi_rbs_sethook(chan, DAHDI_TXSIG_START, DAHDI_TXSTATE_RINGON, ret);
				} else
					dahdi_rbs_sethook(chan, DAHDI_TXSIG_START, DAHDI_TXSTATE_START, chan->starttime);
				spin_unlock_irqrestore(&chan->lock, flags);
				if (file->f_flags & O_NONBLOCK)
					return -EINPROGRESS;
				break;
			case DAHDI_WINK:
				spin_lock_irqsave(&chan->lock, flags);
				if (chan->txstate != DAHDI_TXSTATE_ONHOOK) {
					spin_unlock_irqrestore(&chan->lock, flags);
					return -EBUSY;
				}
				dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_PREWINK, chan->prewinktime);
				spin_unlock_irqrestore(&chan->lock, flags);
				if (file->f_flags & O_NONBLOCK)
					return -EINPROGRESS;
				wait_event_interruptible(chan->waitq,
					!chan->file->private_data || is_txstate(chan, DAHDI_TXSIG_ONHOOK));
				if (unlikely(!chan->file->private_data))
					return -ENODEV;
				if (signal_pending(current))
					return -ERESTARTSYS;
				break;
			case DAHDI_FLASH:
				spin_lock_irqsave(&chan->lock, flags);
				if (chan->txstate != DAHDI_TXSTATE_OFFHOOK) {
					spin_unlock_irqrestore(&chan->lock, flags);
					return -EBUSY;
				}
				dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_PREFLASH, chan->preflashtime);
				spin_unlock_irqrestore(&chan->lock, flags);
				if (file->f_flags & O_NONBLOCK)
					return -EINPROGRESS;
				wait_event_interruptible(chan->waitq,
					!chan->file->private_data || is_txstate(chan, DAHDI_TXSIG_OFFHOOK));
				if (unlikely(!chan->file->private_data))
					return -ENODEV;
				if (signal_pending(current))
					return -ERESTARTSYS;
				break;
			case DAHDI_RINGOFF:
				spin_lock_irqsave(&chan->lock, flags);
				dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_ONHOOK, 0);
				spin_unlock_irqrestore(&chan->lock, flags);
				break;
			default:
				return -EINVAL;
			}
		} else if (chan->span->ops->sethook) {
			if (chan->txhooksig != j) {
				chan->txhooksig = j;
				chan->span->ops->sethook(chan, j);
			}
		} else
			return -ENOSYS;
		break;
#ifdef CONFIG_DAHDI_PPP
	case PPPIOCGCHAN:
		if (chan->flags & DAHDI_FLAG_PPP) {
			return put_user(ppp_channel_index(chan->ppp),
					(int __user *)data) ? -EFAULT : 0;
		} else {
			return -EINVAL;
		}
		break;
	case PPPIOCGUNIT:
		if (chan->flags & DAHDI_FLAG_PPP) {
			return put_user(ppp_unit_number(chan->ppp),
					(int __user *)data) ? -EFAULT : 0;
		} else {
			return -EINVAL;
		}
		break;
#endif
	case DAHDI_BUFFER_EVENTS:
		if (get_user(j, (int __user *)data))
			return -EFAULT;
		if (j)
			set_bit(DAHDI_FLAGBIT_BUFEVENTS, &chan->flags);
		else
			clear_bit(DAHDI_FLAGBIT_BUFEVENTS, &chan->flags);

		break;
	default:
		return dahdi_chanandpseudo_ioctl(file, cmd, data);
	}
	return 0;
}

static int dahdi_prechan_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	int channo;
	int res;

	if (file->private_data) {
		module_printk(KERN_NOTICE, "Huh?  Prechan already has private data??\n");
	}
	switch(cmd) {
	case DAHDI_SPECIFY:
		get_user(channo, (int __user *)data);
		file->private_data = chan_from_num(channo);
		if (!file->private_data)
			return -EINVAL;
		res = dahdi_specchan_open(file);
		if (res)
			file->private_data = NULL;
		return res;
	default:
		return -ENOSYS;
	}
	return 0;
}

static long
dahdi_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	int unit = UNIT(file);
	struct dahdi_timer *timer;
	int ret;

#if defined(HAVE_UNLOCKED_IOCTL) && defined(CONFIG_BKL)
	lock_kernel();
#endif

	if (unit == DAHDI_CTL) {
		ret = dahdi_ctl_ioctl(file, cmd, data);
		goto unlock_exit;
	}

	if (unit == DAHDI_TRANSCODE) {
		/* dahdi_transcode should have updated the file_operations on
		 * this file object on open, so we shouldn't be here. */
		WARN_ON(1);
		ret = -EFAULT;
		goto unlock_exit;
	}

	if (unit == DAHDI_TIMER) {
		timer = file->private_data;
		if (timer)
			ret = dahdi_timer_ioctl(file, cmd, data, timer);
		else
			ret = -EINVAL;
		goto unlock_exit;
	}
	if (unit == DAHDI_CHANNEL) {
		if (file->private_data)
			ret = dahdi_chan_ioctl(file, cmd, data);
		else
			ret = dahdi_prechan_ioctl(file, cmd, data);
		goto unlock_exit;
	}
	if (unit == DAHDI_PSEUDO) {
		if (!file->private_data) {
			module_printk(KERN_NOTICE, "No pseudo channel structure to read?\n");
			ret = -EINVAL;
			goto unlock_exit;
		}
		ret = dahdi_chanandpseudo_ioctl(file, cmd, data);
		goto unlock_exit;
	}

	if (!file->private_data) {
		ret = -ENXIO;
		goto unlock_exit;
	}

	ret = dahdi_chan_ioctl(file, cmd, data);

unlock_exit:
#if defined(HAVE_UNLOCKED_IOCTL) && defined(CONFIG_BKL)
	unlock_kernel();
#endif
	return ret;
}

#ifndef HAVE_UNLOCKED_IOCTL
static int dahdi_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long data)
{
	return dahdi_unlocked_ioctl(file, cmd, data);
}
#endif

#ifdef HAVE_COMPAT_IOCTL
static long dahdi_ioctl_compat(struct file *file, unsigned int cmd,
		unsigned long data)
{
	if (cmd == DAHDI_SFCONFIG)
		return -ENOTTY; /* Not supported yet */

	return dahdi_unlocked_ioctl(file, cmd, data);
}
#endif

/**
 * _get_next_channo - Return the next taken channel number from the span list.
 * @span:	The span with which to start the search.
 *
 * Returns -1 if there aren't any channels on span or any of the following
 * spans, otherwise, returns the channel number of the first channel.
 *
 * Must be callled with registration_mutex held.
 *
 */
static unsigned int _get_next_channo(const struct dahdi_span *span)
{
	const struct list_head *pos = &span->spans_node;
	while (pos != &span_list) {
		span = list_entry(pos, struct dahdi_span, spans_node);
		if (span->channels)
			return span->chans[0]->channo;
		pos = pos->next;
	}
	return -1;
}

static void
set_spanno_and_basechan(struct dahdi_span *span, u32 spanno, u32 basechan)
{
	int i;
	dahdi_dev_dbg(ASSIGN, span_device(span),
		"set: spanno=%d, basechan=%d (span->channels=%d)\n",
		spanno, basechan, span->channels);
	span->spanno = spanno;
	for (i = 0; i < span->channels; ++i)
		span->chans[i]->channo = basechan + i;
}

/**
 * _assign_spanno_and_basechan - Assign next available span and channel numbers.
 *
 * This function will set span->spanno and channo for all the member channels.
 * It will assign the first available location.
 *
 * Must be called with registration_mutex held.
 *
 */
static int _assign_spanno_and_basechan(struct dahdi_span *span)
{
	struct dahdi_span *pos;
	unsigned int next_channo;
	unsigned int spanno = 1;
	unsigned int basechan = 1;

	dahdi_dev_dbg(ASSIGN, span_device(span),
		"assign: channels=%d\n", span->channels);
	list_for_each_entry(pos, &span_list, spans_node) {

		if (pos->spanno <= spanno) {
			spanno = pos->spanno + 1;
			basechan = pos->chans[0]->channo + pos->channels;
			continue;
		}

		next_channo = _get_next_channo(pos);
		if ((basechan + span->channels) >= next_channo)
			break;

		/* We can't fit here, let's look at the next location. */
		spanno = pos->spanno + 1;
		if (pos->channels)
			basechan = pos->chans[0]->channo + pos->channels;
	}

	dahdi_dev_dbg(ASSIGN, span_device(span),
		"good: spanno=%d, basechan=%d (span->channels=%d)\n",
		spanno, basechan, span->channels);
	set_spanno_and_basechan(span, spanno, basechan);
	return 0;
}

static inline struct dahdi_span *span_from_node(struct list_head *node)
{
	return container_of(node, struct dahdi_span, spans_node);
}

/*
 * Call with registration_mutex held.  Make sure all the spans are on the list
 * ordered by span.
 *
 */
static void _dahdi_add_span_to_span_list(struct dahdi_span *span)
{
	unsigned long flags;
	struct dahdi_span *pos;

	if (list_empty(&span_list)) {
		list_add_tail(&span->spans_node, &span_list);
		return;
	}

	list_for_each_entry(pos, &span_list, spans_node) {
		WARN_ON(0 == pos->spanno);
		if (pos->spanno > span->spanno)
			break;
	}

	spin_lock_irqsave(&chan_lock, flags);
	list_add(&span->spans_node, pos->spans_node.prev);
	spin_unlock_irqrestore(&chan_lock, flags);
}

/**
 * _check_spanno_and_basechan - Check if we can fit the new span in the requested location.
 *
 * Must be called with registration_mutex held.
 *
 */
static int
_check_spanno_and_basechan(struct dahdi_span *span, u32 spanno, u32 basechan)
{
	struct dahdi_span *pos;
	unsigned int next_channo;

	dahdi_dev_dbg(ASSIGN, span_device(span),
		"check: spanno=%d, basechan=%d (span->channels=%d)\n",
		spanno, basechan, span->channels);
	list_for_each_entry(pos, &span_list, spans_node) {

		next_channo = _get_next_channo(pos);
		dahdi_dev_dbg(ASSIGN, span_device(span),
			"pos: spanno=%d channels=%d (next_channo=%d)\n",
			pos->spanno, pos->channels, next_channo);

		if (pos->spanno <= spanno) {
			if (basechan < next_channo + pos->channels) {
				/* Requested basechan breaks channel sorting */
				dev_notice(span_device(span),
					"[%d] basechan (%d) is too low for wanted span %d\n",
					local_spanno(span), basechan, spanno);
				return -EINVAL;
			}
			continue;
		}

		if (next_channo == -1)
			break;

		if ((basechan + span->channels) <= next_channo)
			break;

		/* Cannot fit the span into the requested location. Abort. */
		dev_notice(span_device(span),
			"cannot fit span %d (basechan=%d) into requested location\n",
			spanno, basechan);
		return -EINVAL;
	}

	dahdi_dev_dbg(ASSIGN, span_device(span),
		"good: spanno=%d, basechan=%d (span->channels=%d)\n",
		spanno, basechan, span->channels);
	set_spanno_and_basechan(span, spanno, basechan);
	return 0;
}


struct dahdi_device *dahdi_create_device(void)
{
	struct dahdi_device *ddev;
	ddev = kzalloc(sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return NULL;
	INIT_LIST_HEAD(&ddev->spans);
	dahdi_sysfs_init_device(ddev);
	return ddev;
}
EXPORT_SYMBOL(dahdi_create_device);

void dahdi_free_device(struct dahdi_device *ddev)
{
	put_device(&ddev->dev);
}
EXPORT_SYMBOL(dahdi_free_device);

/**
 * __dahdi_init_span - Setup all the data structures for the span.
 * @span:	The span of interest.
 *
 */
static void __dahdi_init_span(struct dahdi_span *span)
{
	int x;

	INIT_LIST_HEAD(&span->spans_node);
	spin_lock_init(&span->lock);
	clear_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags);

	if (!span->deflaw) {
		module_printk(KERN_NOTICE, "Span %s didn't specify default "
			      "law. Assuming mulaw, please fix driver!\n",
			      span->name);
		span->deflaw = DAHDI_LAW_MULAW;
	}

	for (x = 0; x < span->channels; ++x) {
		span->chans[x]->span = span;
		__dahdi_init_chan(span->chans[x]);
	}
}

/**
 * dahdi_init_span - (Re)Initializes a dahdi span.
 * @span:		The span to initialize.
 *
 * Reinitializing a device span might be necessary if a span has been changed
 * (channels added / removed) between when the dahdi_device it is on was first
 * registered and when the spans are actually assigned.
 *
 */
void dahdi_init_span(struct dahdi_span *span)
{
	mutex_lock(&registration_mutex);
	__dahdi_init_span(span);
	mutex_unlock(&registration_mutex);
}
EXPORT_SYMBOL(dahdi_init_span);

/**
 * _dahdi_assign_span() - Assign a new DAHDI span
 * @span:	the DAHDI span
 * @spanno:	The span number we would like assigned. If 0, the first
 *		available spanno/basechan will be used.
 * @basechan:	The base channel number we would like. Ignored if spanno is 0.
 * @prefmaster:	will the new span be preferred as a master?
 *
 * Assigns a span for usage with DAHDI. All the channel numbers in it will
 * have their numbering started at basechan.
 *
 * If prefmaster is set to anything > 0, span will attempt to become the
 * master DAHDI span at registration time. If 0: it will only become
 * master if no other span is currently the master (i.e.: it is the
 * first one).
 *
 * Must be called with registration_mutex held, and the span must have already
 * been initialized ith the __dahdi_init_span call.
 *
 */
static int _dahdi_assign_span(struct dahdi_span *span, unsigned int spanno,
			      unsigned int basechan, int prefmaster)
{
	int res = 0;
	unsigned int x;

	if (!span || !span->ops || !span->ops->owner)
		return -EFAULT;

	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags)) {
		dev_notice(span_device(span),
			 "local span %d is already assigned span %d "
			 "with base channel %d\n", local_spanno(span), span->spanno,
			 span->chans[0]->channo);
		return -EINVAL;
	}

	if (span->ops->enable_hw_preechocan ||
	    span->ops->disable_hw_preechocan) {
		if ((NULL == span->ops->enable_hw_preechocan) ||
		    (NULL == span->ops->disable_hw_preechocan)) {
			dev_notice(span_device(span),
				"span with inconsistent enable/disable hw_preechocan");
			return -EFAULT;
		}
	}

	if (!span->deflaw) {
		module_printk(KERN_NOTICE, "Span %s didn't specify default law.  "
				"Assuming mulaw, please fix driver!\n", span->name);
		span->deflaw = DAHDI_LAW_MULAW;
	}

	/* Look through the span list to find the first available span number.
	 * The spans are kept on this list in sorted order. We'll also save
	 * off the next available channel number to use. */

	if (0 == spanno)
		res = _assign_spanno_and_basechan(span);
	else
		res = _check_spanno_and_basechan(span, spanno, basechan);

	if (res)
		return res;

	for (x = 0; x < span->channels; x++)
		dahdi_chan_reg(span->chans[x]);

#ifdef CONFIG_PROC_FS
	{
		char tempfile[17];
		snprintf(tempfile, sizeof(tempfile), "%d", span->spanno);
		span->proc_entry = create_proc_entry(tempfile, 0444,
					root_proc_entry);
		if (!span->proc_entry) {
			res = -EFAULT;
			span_err(span, "Error creating procfs entry\n");
			goto cleanup;
		}
		span->proc_entry->data = (void *)(long)span->spanno;
		span->proc_entry->proc_fops = &dahdi_proc_ops;
	}
#endif

	res = span_sysfs_create(span);
	if (res)
		goto cleanup;

	if (debug & DEBUG_MAIN) {
		module_printk(KERN_NOTICE, "Registered Span %d ('%s') with "
				"%d channels\n", span->spanno, span->name, span->channels);
	}

	_dahdi_add_span_to_span_list(span);

	set_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags);
	if (span->ops->assigned)
		span->ops->assigned(span);

	__dahdi_find_master_span();

	return 0;

cleanup:
#ifdef CONFIG_PROC_FS
	if (span->proc_entry) {
		char tempfile[17];

		snprintf(tempfile, sizeof(tempfile), "dahdi/%d", span->spanno);
		remove_proc_entry(tempfile, NULL);
		span->proc_entry = NULL;
	}
#endif
	for (x = 0; x < span->channels; x++) {
		struct dahdi_chan *chan = span->chans[x];
		if (test_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags))
			dahdi_chan_unreg(chan);
	}
	return res;
}

int dahdi_assign_span(struct dahdi_span *span, unsigned int spanno,
		      unsigned int basechan, int prefmaster)
{
	int ret;
	mutex_lock(&registration_mutex);
	ret = _dahdi_assign_span(span, spanno, basechan, prefmaster);
	mutex_unlock(&registration_mutex);
	return ret;
}

int dahdi_assign_device_spans(struct dahdi_device *ddev)
{
	struct dahdi_span *span;
	mutex_lock(&registration_mutex);
	list_for_each_entry(span, &ddev->spans, device_node)
		_dahdi_assign_span(span, 0, 0, 1);
	mutex_unlock(&registration_mutex);
	return 0;
}

static int auto_assign_spans = 1;
static const char *UNKNOWN = "";

/**
 * _dahdi_register_device - Registers a DAHDI device and assign its spans.
 * @ddev:	the DAHDI device
 *
 * If auto_assign_spans is 0, add the device to the device list and wait for
 * userspace to finish registration. Otherwise, go ahead and register the
 * spans in order as was done historically.
 *
 * Must hold registration_mutex when this function is called.
 *
 */
static int _dahdi_register_device(struct dahdi_device *ddev,
				  struct device *parent)
{
	struct dahdi_span *s;
	int ret;

	ddev->manufacturer	= (ddev->manufacturer) ?: UNKNOWN;
	ddev->location		= (ddev->location) ?: UNKNOWN;
	ddev->devicetype	= (ddev->devicetype) ?: UNKNOWN;

	list_for_each_entry(s, &ddev->spans, device_node) {
		s->parent = ddev;
		s->spanno = 0;
		__dahdi_init_span(s);
	}

	ret = dahdi_sysfs_add_device(ddev, parent);
	if (ret)
		return ret;

	if (!auto_assign_spans)
		return 0;

	list_for_each_entry(s, &ddev->spans, device_node)
		ret = _dahdi_assign_span(s, 0, 0, 1);

	if (ret)
		dahdi_sysfs_unregister_device(ddev);

	return ret;
}

/**
 * dahdi_register_device() - unregister a new DAHDI device
 * @ddev:	the DAHDI device
 *
 * Registers a device for usage with DAHDI.
 *
 */
int dahdi_register_device(struct dahdi_device *ddev, struct device *parent)
{
	int ret;

	if (!ddev)
		return -EINVAL;

	mutex_lock(&registration_mutex);
	ret = _dahdi_register_device(ddev, parent);
	mutex_unlock(&registration_mutex);

	return ret;
}
EXPORT_SYMBOL(dahdi_register_device);

static void disable_span(struct dahdi_span *span)
{
	int		x;
	unsigned long	flags;

	spin_lock_irqsave(&span->lock, flags);
	span->alarms = DAHDI_ALARM_NOTOPEN;
	for (x = 0; x < span->channels; x++) {
		/*
		 * This event may not make it to user space before the channel
		 * is gone, but let's try.
		 */
		dahdi_qevent_lock(span->chans[x], DAHDI_EVENT_REMOVED);
	}
	dahdi_alarm_notify(span);
	spin_unlock_irqrestore(&span->lock, flags);
	module_printk(KERN_INFO, "%s: span %d\n", __func__, span->spanno);
}

/**
 * _dahdi_unassign_span() - unassign a DAHDI span
 * @span:	the DAHDI span
 *
 * Unassigns a span that has been previously assigned with
 * dahdi_assign_span().
 *
 * Must be called with the registration_mutex held.
 *
 */
static int _dahdi_unassign_span(struct dahdi_span *span)
{
	int x;
	struct dahdi_span *new_master, *s;
	unsigned long flags;

	if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags)) {
		dev_info(span_device(span),
			"local span %d is already unassigned\n",
			local_spanno(span));
		return -EINVAL;
	}
	spin_lock_irqsave(&chan_lock, flags);
	list_del_init(&span->spans_node);
	spin_unlock_irqrestore(&chan_lock, flags);
	span->spanno = 0;
	clear_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags);

	/* Shutdown the span if it's running */
	if ((span->flags & DAHDI_FLAG_RUNNING) && span->ops->shutdown)
		span->ops->shutdown(span);

	if (debug & DEBUG_MAIN)
		module_printk(KERN_NOTICE, "Unassigning Span '%s' with %d channels\n", span->name, span->channels);
#ifdef CONFIG_PROC_FS
	if (span->proc_entry)
		remove_proc_entry(span->proc_entry->name, root_proc_entry);
#endif /* CONFIG_PROC_FS */

	span_sysfs_remove(span);

	for (x=0;x<span->channels;x++)
		dahdi_chan_unreg(span->chans[x]);

	new_master = master; /* FIXME: locking */
	if (master == span)
		new_master = NULL;

	spin_lock_irqsave(&chan_lock, flags);
	list_for_each_entry(s, &span_list, spans_node) {
		if ((s == new_master) || !can_provide_timing(s))
			continue;
		new_master = s;
		break;
	}
	spin_unlock_irqrestore(&chan_lock, flags);
	if (master != new_master) {
		if (debug & DEBUG_MAIN) {
			module_printk(KERN_NOTICE, "%s: Span ('%s') is new master\n", __FUNCTION__,
				      (new_master)? new_master->name: "no master");
		}
	}
	master = new_master;
	return 0;
}

static int open_channel_count(const struct dahdi_span *span)
{
	int i;
	int open_channels = 0;
	struct dahdi_chan *chan;

	for (i = 0; i < span->channels; ++i) {
		chan = span->chans[i];
		if (test_bit(DAHDI_FLAGBIT_OPEN, &chan->flags))
			++open_channels;
	}
	return open_channels;
}

int dahdi_unassign_span(struct dahdi_span *span)
{
	int ret;

	module_printk(KERN_NOTICE, "%s: %s\n", __func__, span->name);
	disable_span(span);
	if (open_channel_count(span) > 0)
		msleep(1000);	/* Give user space a chance to read this */
	mutex_lock(&registration_mutex);
	ret = _dahdi_unassign_span(span);
	mutex_unlock(&registration_mutex);
	return ret;
}

/**
 * dahdi_unregister_device() - unregister a DAHDI device
 * @span:	the DAHDI span
 *
 * Unregisters a device that has been previously registered with
 * dahdi_register_device().
 *
 */
void dahdi_unregister_device(struct dahdi_device *ddev)
{
	struct dahdi_span *s;
	struct dahdi_span *next;
	unsigned int spans_with_open_channels = 0;

	WARN_ON(!ddev);
	might_sleep();
	if (unlikely(!ddev))
		return;

	list_for_each_entry_safe(s, next, &ddev->spans, device_node) {
		disable_span(s);
		if (open_channel_count(s) > 0)
			++spans_with_open_channels;
	}

	if (spans_with_open_channels > 0)
		msleep(1000); /* give user space a chance to read this */

	mutex_lock(&registration_mutex);
	list_for_each_entry_safe(s, next, &ddev->spans, device_node) {
		_dahdi_unassign_span(s);
		list_del_init(&s->device_node);
	}
	mutex_unlock(&registration_mutex);

	dahdi_sysfs_unregister_device(ddev);

	if (UNKNOWN == ddev->location)
		ddev->location = NULL;
	if (UNKNOWN == ddev->manufacturer)
		ddev->manufacturer = NULL;
	if (UNKNOWN == ddev->devicetype)
		ddev->devicetype = NULL;

}
EXPORT_SYMBOL(dahdi_unregister_device);

/*
** This routine converts from linear to ulaw
**
** Craig Reese: IDA/Supercomputing Research Center
** Joe Campbell: Department of Defense
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) "A New Digital Technique for Implementation of Any
**     Continuous PCM Companding Law," Villeret, Michel,
**     et al. 1973 IEEE Int. Conf. on Communications, Vol 1,
**     1973, pg. 11.12-11.17
** 3) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: Signed 16 bit linear sample
** Output: 8 bit ulaw sample
*/

#define ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define CLIP 32635

#ifdef CONFIG_CALC_XLAW
unsigned char
#else
static unsigned char  __init
#endif
__dahdi_lineartoulaw(short sample)
{
  static int exp_lut[256] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
                             4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
  int sign, exponent, mantissa;
  unsigned char ulawbyte;

  /* Get the sample into sign-magnitude. */
  sign = (sample >> 8) & 0x80;          /* set aside the sign */
  if (sign != 0) sample = -sample;              /* get magnitude */
  if (sample > CLIP) sample = CLIP;             /* clip the magnitude */

  /* Convert from 16 bit linear to ulaw. */
  sample = sample + BIAS;
  exponent = exp_lut[(sample >> 7) & 0xFF];
  mantissa = (sample >> (exponent + 3)) & 0x0F;
  ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef ZEROTRAP
  if (ulawbyte == 0) ulawbyte = 0x02;   /* optional CCITT trap */
#endif
  if (ulawbyte == 0xff) ulawbyte = 0x7f;   /* never return 0xff */
  return(ulawbyte);
}

#define AMI_MASK 0x55

#ifdef CONFIG_CALC_XLAW
unsigned char
#else
static inline unsigned char __init
#endif
__dahdi_lineartoalaw (short linear)
{
    int mask;
    int seg;
    int pcm_val;
    static int seg_end[8] =
    {
         0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF
    };

    pcm_val = linear;
    if (pcm_val >= 0)
    {
        /* Sign (7th) bit = 1 */
        mask = AMI_MASK | 0x80;
    }
    else
    {
        /* Sign bit = 0 */
        mask = AMI_MASK;
        pcm_val = -pcm_val;
    }

    /* Convert the scaled magnitude to segment number. */
    for (seg = 0;  seg < 8;  seg++)
    {
        if (pcm_val <= seg_end[seg])
	    break;
    }
    /* Combine the sign, segment, and quantization bits. */
    return  ((seg << 4) | ((pcm_val >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}
/*- End of function --------------------------------------------------------*/

static inline short int __init alaw2linear (uint8_t alaw)
{
    int i;
    int seg;

    alaw ^= AMI_MASK;
    i = ((alaw & 0x0F) << 4);
    seg = (((int) alaw & 0x70) >> 4);
    if (seg)
        i = (i + 0x100) << (seg - 1);
    return (short int) ((alaw & 0x80)  ?  i  :  -i);
}
/*- End of function --------------------------------------------------------*/
static void  __init dahdi_conv_init(void)
{
	int i;

	/*
	 *  Set up mu-law conversion table
	 */
	for(i = 0;i < 256;i++)
	   {
		short mu,e,f,y;
		static short etab[]={0,132,396,924,1980,4092,8316,16764};

		mu = 255-i;
		e = (mu & 0x70)/16;
		f = mu & 0x0f;
		y = f * (1 << (e + 3));
		y += etab[e];
		if (mu & 0x80) y = -y;
	        __dahdi_mulaw[i] = y;
		__dahdi_alaw[i] = alaw2linear(i);
		/* Default (0.0 db) gain table */
		defgain[i] = i;
	   }
#ifndef CONFIG_CALC_XLAW
	  /* set up the reverse (mu-law) conversion table */
	for(i = -32768; i < 32768; i += 4)
	   {
		__dahdi_lin2mu[((unsigned short)(short)i) >> 2] = __dahdi_lineartoulaw(i);
		__dahdi_lin2a[((unsigned short)(short)i) >> 2] = __dahdi_lineartoalaw(i);
	   }
#endif
}

static inline void __dahdi_process_getaudio_chunk(struct dahdi_chan *ss, unsigned char *txb)
{
	/* We transmit data from our master channel */
	/* Called with ss->lock held */
	struct dahdi_chan *ms = ss->master;
	/* Linear representation */
	short getlin[DAHDI_CHUNKSIZE], k[DAHDI_CHUNKSIZE];
	int x;

	/* Okay, now we've got something to transmit */
	for (x=0;x<DAHDI_CHUNKSIZE;x++)
		getlin[x] = DAHDI_XLAW(txb[x], ms);

#ifndef CONFIG_DAHDI_NO_ECHOCAN_DISABLE
	if (ms->ec_state && (ms->ec_state->status.mode == ECHO_MODE_ACTIVE) && !ms->ec_state->features.CED_tx_detect) {
		for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
			if (echo_can_disable_detector_update(&ms->ec_state->txecdis, getlin[x])) {
				set_echocan_fax_mode(ms, ss->channo, "CED tx detected", 1);
				dahdi_qevent_nolock(ms, DAHDI_EVENT_TX_CED_DETECTED);
				break;
			}
		}
	}
#endif

	if ((!ms->confmute && !ms->dialing) || (is_pseudo_chan(ms))) {
		struct dahdi_chan *const conf_chan = ms->conf_chan;
		/* Handle conferencing on non-clear channel and non-HDLC channels */
		switch(ms->confmode & DAHDI_CONF_MODE_MASK) {
		case DAHDI_CONF_NORMAL:
			/* Do nuffin */
			break;
		case DAHDI_CONF_MONITOR:	/* Monitor a channel's rx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;
			/* Add monitored channel */
			if (is_pseudo_chan(conf_chan))
				ACSS(getlin, conf_chan->getlin);
			else
				ACSS(getlin, conf_chan->putlin);

			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITORTX: /* Monitor a channel's tx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;
			/* Add monitored channel */
			if (is_pseudo_chan(conf_chan))
				ACSS(getlin, conf_chan->putlin);
			else
				ACSS(getlin, conf_chan->getlin);

			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITORBOTH: /* monitor a channel's rx and tx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;
			ACSS(getlin, conf_chan->putlin);
			ACSS(getlin, conf_chan->getlin);
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITOR_RX_PREECHO:	/* Monitor a channel's rx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(getlin, is_pseudo_chan(conf_chan) ?
			     conf_chan->readchunkpreec : conf_chan->putlin);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);

			break;
		case DAHDI_CONF_MONITOR_TX_PREECHO: /* Monitor a channel's tx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(getlin, is_pseudo_chan(conf_chan) ?
			     conf_chan->putlin : conf_chan->readchunkpreec);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);

			break;
		case DAHDI_CONF_MONITORBOTH_PREECHO: /* monitor a channel's rx and tx mode */
			  /* if a pseudo-channel, ignore */
			if (is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			ACSS(getlin, conf_chan->putlin);
			ACSS(getlin, conf_chan->readchunkpreec);

			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);

			break;
		case DAHDI_CONF_REALANDPSEUDO:
			/* This strange mode takes the transmit buffer and
				puts it on the conference, minus its last sample,
				then outputs from the conference minus the
				real channel's last sample. */
			  /* if to talk on conf */
			if (ms->confmode & DAHDI_CONF_PSEUDO_TALKER) {
				/* Store temp value */
				memcpy(k, getlin, DAHDI_CHUNKSIZE * sizeof(short));
				/* Add conf value */
				ACSS(k, conf_sums_next[ms->_confn]);
				/* save last one */
				memcpy(ms->conflast2, ms->conflast1, DAHDI_CHUNKSIZE * sizeof(short));
				memcpy(ms->conflast1, k, DAHDI_CHUNKSIZE * sizeof(short));
				/*  get amount actually added */
				SCSS(ms->conflast1, conf_sums_next[ms->_confn]);
				/* Really add in new value */
				ACSS(conf_sums_next[ms->_confn], ms->conflast1);
			} else {
				memset(ms->conflast1, 0, DAHDI_CHUNKSIZE * sizeof(short));
				memset(ms->conflast2, 0, DAHDI_CHUNKSIZE * sizeof(short));
			}
			memset(getlin, 0, DAHDI_CHUNKSIZE * sizeof(short));
			txb[0] = DAHDI_LIN2X(0, ms);
			memset(txb + 1, txb[0], DAHDI_CHUNKSIZE - 1);
			/* fall through to normal conf mode */
		case DAHDI_CONF_CONF:	/* Normal conference mode */
			if (is_pseudo_chan(ms)) /* if pseudo-channel */
			   {
				  /* if to talk on conf */
				if (ms->confmode & DAHDI_CONF_TALKER) {
					/* Store temp value */
					memcpy(k, getlin, DAHDI_CHUNKSIZE * sizeof(short));
					/* Add conf value */
					ACSS(k, conf_sums[ms->_confn]);
					/*  get amount actually added */
					memcpy(ms->conflast, k, DAHDI_CHUNKSIZE * sizeof(short));
					SCSS(ms->conflast, conf_sums[ms->_confn]);
					/* Really add in new value */
					ACSS(conf_sums[ms->_confn], ms->conflast);
					memcpy(ms->getlin, getlin, DAHDI_CHUNKSIZE * sizeof(short));
				} else {
					memset(ms->conflast, 0, DAHDI_CHUNKSIZE * sizeof(short));
					memcpy(getlin, ms->getlin, DAHDI_CHUNKSIZE * sizeof(short));
				}
				txb[0] = DAHDI_LIN2X(0, ms);
				memset(txb + 1, txb[0], DAHDI_CHUNKSIZE - 1);
				break;
		 	   }
			/* fall through */
		case DAHDI_CONF_CONFMON:	/* Conference monitor mode */
			if (ms->confmode & DAHDI_CONF_LISTENER) {
				/* Subtract out last sample written to conf */
				SCSS(getlin, ms->conflast);
				/* Add in conference */
				ACSS(getlin, conf_sums[ms->_confn]);
			}
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_CONFANN:
		case DAHDI_CONF_CONFANNMON:
			/* First, add tx buffer to conf */
			ACSS(conf_sums_next[ms->_confn], getlin);
			/* Start with silence */
			memset(getlin, 0, DAHDI_CHUNKSIZE * sizeof(short));
			/* If a listener on the conf... */
			if (ms->confmode & DAHDI_CONF_LISTENER) {
				/* Subtract last value written */
				SCSS(getlin, ms->conflast);
				/* Add in conf */
				ACSS(getlin, conf_sums[ms->_confn]);
			}
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_DIGITALMON:
			/* Real digital monitoring, but still echo cancel if
			 * desired */
			if (!conf_chan)
				break;
			if (is_pseudo_chan(conf_chan)) {
				if (ms->ec_state) {
					for (x = 0; x < DAHDI_CHUNKSIZE; x++)
						txb[x] = DAHDI_LIN2X(conf_chan->getlin[x], ms);
				} else {
					memcpy(txb, conf_chan->getraw, DAHDI_CHUNKSIZE);
				}
			} else {
				if (ms->ec_state) {
					for (x = 0; x < DAHDI_CHUNKSIZE; x++)
						txb[x] = DAHDI_LIN2X(conf_chan->putlin[x], ms);
				} else {
					memcpy(txb, conf_chan->putraw,
					       DAHDI_CHUNKSIZE);
				}
			}
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				getlin[x] = DAHDI_XLAW(txb[x], ms);
			break;
		}
	}
	if (ms->confmute || (ms->ec_state && (ms->ec_state->status.mode) & __ECHO_MODE_MUTE)) {
		txb[0] = DAHDI_LIN2X(0, ms);
		memset(txb + 1, txb[0], DAHDI_CHUNKSIZE - 1);
		if (ms->ec_state && (ms->ec_state->status.mode == ECHO_MODE_STARTTRAINING)) {
			/* Transmit impulse now */
			txb[0] = DAHDI_LIN2X(16384, ms);
			ms->ec_state->status.mode = ECHO_MODE_AWAITINGECHO;
		}
	}
	/* save value from last chunk */
	memcpy(ms->getlin_lastchunk, ms->getlin, DAHDI_CHUNKSIZE * sizeof(short));
	/* save value from current */
	memcpy(ms->getlin, getlin, DAHDI_CHUNKSIZE * sizeof(short));
	/* save value from current */
	memcpy(ms->getraw, txb, DAHDI_CHUNKSIZE);
	/* if to make tx tone */
	if (ms->v1_1 || ms->v2_1 || ms->v3_1)
	{
		for (x=0;x<DAHDI_CHUNKSIZE;x++)
		{
			getlin[x] += dahdi_txtone_nextsample(ms);
			txb[x] = DAHDI_LIN2X(getlin[x], ms);
		}
	}
	/* This is what to send (after having applied gain) */
	for (x=0;x<DAHDI_CHUNKSIZE;x++)
		txb[x] = ms->txgain[txb[x]];
}

static void __putbuf_chunk(struct dahdi_chan *ss, unsigned char *rxb,
			   int bytes);

static inline void __dahdi_getbuf_chunk(struct dahdi_chan *ss, unsigned char *txb)
{


#ifdef CONFIG_DAHDI_MIRROR
	unsigned char *orig_txb = txb;
#endif /* CONFIG_DAHDI_MIRROR */

	/* Called with ss->lock held */
	/* We transmit data from our master channel */
	struct dahdi_chan *ms = ss->master;
	/* Buffer we're using */
	unsigned char *buf;
	/* Old buffer number */
	int oldbuf;
	/* Linear representation */
	int getlin;
	/* How many bytes we need to process */
	int bytes = DAHDI_CHUNKSIZE, left;
	bool needtxunderrun = false;
	int x;

	/* Let's pick something to transmit.  First source to
	   try is our write-out buffer.  Always check it first because
	   its our 'fast path' for whatever that's worth. */
	while(bytes) {
		if ((ms->outwritebuf > -1) && !ms->txdisable) {
			buf= ms->writebuf[ms->outwritebuf];
			left = ms->writen[ms->outwritebuf] - ms->writeidx[ms->outwritebuf];
			if (left > bytes)
				left = bytes;
			if (ms->flags & DAHDI_FLAG_HDLC) {
				/* If this is an HDLC channel we only send a byte of
				   HDLC. */
				for(x=0;x<left;x++) {
					if (fasthdlc_tx_need_data(&ms->txhdlc))
						/* Load a byte of data only if needed */
						fasthdlc_tx_load_nocheck(&ms->txhdlc, buf[ms->writeidx[ms->outwritebuf]++]);
					*(txb++) = fasthdlc_tx_run_nocheck(&ms->txhdlc);
				}
				bytes -= left;
			} else {
				memcpy(txb, buf + ms->writeidx[ms->outwritebuf], left);
				ms->writeidx[ms->outwritebuf]+=left;
				txb += left;
				bytes -= left;
			}
			/* Check buffer status */
			if (ms->writeidx[ms->outwritebuf] >= ms->writen[ms->outwritebuf]) {
				/* We've reached the end of our buffer.  Go to the next. */
				oldbuf = ms->outwritebuf;
				/* Clear out write index and such */
				ms->writeidx[oldbuf] = 0;
				ms->outwritebuf = (ms->outwritebuf + 1) % ms->numbufs;

				if (!(ms->flags & DAHDI_FLAG_MTP2)) {
					ms->writen[oldbuf] = 0;
					if (ms->outwritebuf == ms->inwritebuf) {
						/* Whoopsies, we're run out of buffers.  Mark ours
						as -1 and wait for the filler to notify us that
						there is something to write */
						ms->outwritebuf = -1;
						if (ms->iomask & (DAHDI_IOMUX_WRITE | DAHDI_IOMUX_WRITEEMPTY))
							wake_up_interruptible(&ms->waitq);
						/* If we're only supposed to start when full, disable the transmitter */
						if ((ms->txbufpolicy == DAHDI_POLICY_WHEN_FULL) ||
							(ms->txbufpolicy == DAHDI_POLICY_HALF_FULL))
							ms->txdisable = 1;
					}
				} else {
					if (ms->outwritebuf == ms->inwritebuf) {
						ms->outwritebuf = oldbuf;
						if (ms->iomask & (DAHDI_IOMUX_WRITE | DAHDI_IOMUX_WRITEEMPTY))
							wake_up_interruptible(&ms->waitq);
						/* If we're only supposed to start when full, disable the transmitter */
						if ((ms->txbufpolicy == DAHDI_POLICY_WHEN_FULL) ||
							(ms->txbufpolicy == DAHDI_POLICY_HALF_FULL))
							ms->txdisable = 1;
					}
				}
				if (ms->inwritebuf < 0) {
					/* The filler doesn't have a place to put data.  Now
					that we're done with this buffer, notify them. */
					ms->inwritebuf = oldbuf;
				}
/* In the very orignal driver, it was quite well known to me (Jim) that there
was a possibility that a channel sleeping on a write block needed to
be potentially woken up EVERY time a buffer was emptied, not just on the first
one, because if only done on the first one there is a slight timing potential
of missing the wakeup (between where it senses the (lack of) active condition
(with interrupts disabled) and where it does the sleep (interrupts enabled)
in the read or iomux call, etc). That is why the write and iomux calls start
with an infinite loop that gets broken out of upon an active condition,
otherwise keeps sleeping and looking. The part in this code got "optimized"
out in the later versions, and is put back now. */
				if (!(ms->flags & DAHDI_FLAG_PPP) ||
				    !dahdi_have_netdev(ms)) {
					wake_up_interruptible(&ms->waitq);
				}
				/* Transmit a flag if this is an HDLC channel */
				if (ms->flags & DAHDI_FLAG_HDLC)
					fasthdlc_tx_frame_nocheck(&ms->txhdlc);
#ifdef CONFIG_DAHDI_NET
				if (dahdi_have_netdev(ms))
					netif_wake_queue(chan_to_netdev(ms));
#endif
#ifdef CONFIG_DAHDI_PPP
				if (ms->flags & DAHDI_FLAG_PPP) {
					ms->do_ppp_wakeup = 1;
					tasklet_schedule(&ms->ppp_calls);
				}
#endif
			}
		} else if (ms->curtone && !is_pseudo_chan(ms)) {
			left = ms->curtone->tonesamples - ms->tonep;
			if (left > bytes)
				left = bytes;
			for (x=0;x<left;x++) {
				/* Pick our default value from the next sample of the current tone */
				getlin = dahdi_tone_nextsample(&ms->ts, ms->curtone);
				*(txb++) = DAHDI_LIN2X(getlin, ms);
			}
			ms->tonep+=left;
			bytes -= left;
			if (ms->tonep >= ms->curtone->tonesamples) {
				struct dahdi_tone *last;
				/* Go to the next sample of the tone */
				ms->tonep = 0;
				last = ms->curtone;
				ms->curtone = ms->curtone->next;
				if (!ms->curtone) {
					/* No more tones...  Is this dtmf or mf?  If so, go to the next digit */
					if (ms->dialing)
						__do_dtmf(ms);
				} else {
					if (last != ms->curtone)
						dahdi_init_tone_state(&ms->ts, ms->curtone);
				}
			}
		} else if (ms->flags & DAHDI_FLAG_LOOPED) {
			for (x = 0; x < bytes; x++)
				txb[x] = ms->readchunk[x];
			bytes = 0;
		} else if (ms->flags & DAHDI_FLAG_HDLC) {
			for (x=0;x<bytes;x++) {
				/* Okay, if we're HDLC, then transmit a flag by default */
				if (fasthdlc_tx_need_data(&ms->txhdlc))
					fasthdlc_tx_frame_nocheck(&ms->txhdlc);
				*(txb++) = fasthdlc_tx_run_nocheck(&ms->txhdlc);
			}
			bytes = 0;
		} else if (ms->flags & DAHDI_FLAG_CLEAR) {
			/* Clear channels that are idle in audio mode need
			   to send silence; in non-audio mode, always send 0xff
			   so stupid switches won't consider the channel active
			*/
			if (ms->flags & DAHDI_FLAG_AUDIO) {
				memset(txb, DAHDI_LIN2X(0, ms), bytes);
			} else {
				memset(txb, 0xFF, bytes);
			}
			needtxunderrun += bytes;
			bytes = 0;
		} else {
			memset(txb, DAHDI_LIN2X(0, ms), bytes);	/* Lastly we use silence on telephony channels */
			needtxunderrun += bytes;
			bytes = 0;
		}
	}

	if (needtxunderrun) {
		if (!test_bit(DAHDI_FLAGBIT_TXUNDERRUN, &ms->flags)) {
			if (test_bit(DAHDI_FLAGBIT_BUFEVENTS, &ms->flags))
				__qevent(ms, DAHDI_EVENT_WRITE_UNDERRUN);
			set_bit(DAHDI_FLAGBIT_TXUNDERRUN, &ms->flags);
		}
	} else {
		clear_bit(DAHDI_FLAGBIT_TXUNDERRUN, &ms->flags);
	}

#ifdef CONFIG_DAHDI_MIRROR
	if (ss->txmirror) {
		spin_lock(&ss->txmirror->lock);
		__putbuf_chunk(ss->txmirror, orig_txb, DAHDI_CHUNKSIZE);
		spin_unlock(&ss->txmirror->lock);
	}
#endif /* CONFIG_DAHDI_MIRROR */
}

static inline void rbs_itimer_expire(struct dahdi_chan *chan)
{
	/* the only way this could have gotten here, is if a channel
	    went onf hook longer then the wink or flash detect timeout */
	/* Called with chan->lock held */
	switch(chan->sig)
	{
	    case DAHDI_SIG_FXOLS:  /* if FXO, its definitely on hook */
	    case DAHDI_SIG_FXOGS:
	    case DAHDI_SIG_FXOKS:
		__qevent(chan,DAHDI_EVENT_ONHOOK);
		chan->gotgs = 0;
		break;
#if defined(EMFLASH) || defined(EMPULSE)
	    case DAHDI_SIG_EM:
	    case DAHDI_SIG_EM_E1:
		if (chan->rxhooksig == DAHDI_RXSIG_ONHOOK) {
			__qevent(chan,DAHDI_EVENT_ONHOOK);
			break;
		}
		__qevent(chan,DAHDI_EVENT_RINGOFFHOOK);
		break;
#endif
#ifdef	FXSFLASH
	    case DAHDI_SIG_FXSKS:
		if (chan->rxhooksig == DAHDI_RXSIG_ONHOOK) {
			__qevent(chan, DAHDI_EVENT_ONHOOK);
			break;
		}
#endif
		/* fall thru intentionally */
	    default:  /* otherwise, its definitely off hook */
		__qevent(chan,DAHDI_EVENT_RINGOFFHOOK);
		break;
	}
}

static inline void __rbs_otimer_expire(struct dahdi_chan *chan)
{
	int len = 0;
	/* Called with chan->lock held */

	chan->otimer = 0;
	/* Move to the next timer state */
	switch(chan->txstate) {
	case DAHDI_TXSTATE_RINGOFF:
		/* Turn on the ringer now that the silent time has passed */
		++chan->cadencepos;
		if (chan->cadencepos >= DAHDI_MAX_CADENCE)
			chan->cadencepos = chan->firstcadencepos;
		len = chan->ringcadence[chan->cadencepos];

		if (!len) {
			chan->cadencepos = chan->firstcadencepos;
			len = chan->ringcadence[chan->cadencepos];
		}

		dahdi_rbs_sethook(chan, DAHDI_TXSIG_START, DAHDI_TXSTATE_RINGON, len);
		__qevent(chan, DAHDI_EVENT_RINGERON);
		break;

	case DAHDI_TXSTATE_RINGON:
		/* Turn off the ringer now that the loud time has passed */
		++chan->cadencepos;
		if (chan->cadencepos >= DAHDI_MAX_CADENCE)
			chan->cadencepos = 0;
		len = chan->ringcadence[chan->cadencepos];

		if (!len) {
			chan->cadencepos = 0;
			len = chan->curzone->ringcadence[chan->cadencepos];
		}

		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_RINGOFF, len);
		__qevent(chan, DAHDI_EVENT_RINGEROFF);
		break;

	case DAHDI_TXSTATE_START:
		/* If we were starting, go off hook now ready to debounce */
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_AFTERSTART, DAHDI_AFTERSTART_TIME);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_PREWINK:
		/* Actually wink */
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_WINK, chan->winktime);
		break;

	case DAHDI_TXSTATE_WINK:
		/* Wink complete, go on hook and stabalize */
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_ONHOOK, 0);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_PREFLASH:
		/* Actually flash */
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_FLASH, chan->flashtime);
		break;

	case DAHDI_TXSTATE_FLASH:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_DEBOUNCE:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		/* See if we've gone back on hook */
		if ((chan->rxhooksig == DAHDI_RXSIG_ONHOOK) && (chan->rxflashtime > 2))
			chan->itimerset = chan->itimer = chan->rxflashtime * DAHDI_CHUNKSIZE;
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_AFTERSTART:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_KEWL:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_AFTERKEWL, DAHDI_AFTERKEWLTIME);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_AFTERKEWL:
		if (chan->kewlonhook)  {
			__qevent(chan,DAHDI_EVENT_ONHOOK);
		}
		chan->txstate = DAHDI_TXSTATE_ONHOOK;
		chan->gotgs = 0;
		break;

	case DAHDI_TXSTATE_PULSEBREAK:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_PULSEMAKE,
			chan->pulsemaketime);
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_PULSEMAKE:
		if (chan->pdialcount)
			chan->pdialcount--;
		if (chan->pdialcount)
		{
			dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK,
				DAHDI_TXSTATE_PULSEBREAK, chan->pulsebreaktime);
			break;
		}
		chan->txstate = DAHDI_TXSTATE_PULSEAFTER;
		chan->otimer = chan->pulseaftertime * DAHDI_CHUNKSIZE;
		wake_up_interruptible(&chan->waitq);
		break;

	case DAHDI_TXSTATE_PULSEAFTER:
		chan->txstate = DAHDI_TXSTATE_OFFHOOK;
		__do_dtmf(chan);
		wake_up_interruptible(&chan->waitq);
		break;

	default:
		break;
	}
}

static void __dahdi_hooksig_pvt(struct dahdi_chan *chan, enum dahdi_rxsig rxsig)
{

	/* State machines for receive hookstate transitions
		called with chan->lock held */

	if ((chan->rxhooksig) == rxsig) return;

	if ((chan->flags & DAHDI_FLAG_SIGFREEZE)) return;

	chan->rxhooksig = rxsig;
#ifdef	RINGBEGIN
	if ((chan->sig & __DAHDI_SIG_FXS) && (rxsig == DAHDI_RXSIG_RING) &&
	    (!chan->ringdebtimer))
		__qevent(chan,DAHDI_EVENT_RINGBEGIN);
#endif
	switch(chan->sig) {
	    case DAHDI_SIG_EM:  /* E and M */
	    case DAHDI_SIG_EM_E1:
		switch(rxsig) {
		    case DAHDI_RXSIG_OFFHOOK: /* went off hook */
			/* The interface is going off hook */
#ifdef	EMFLASH
			if (chan->itimer)
			{
				__qevent(chan,DAHDI_EVENT_WINKFLASH);
				chan->itimerset = chan->itimer = 0;
				break;
			}
#endif
#ifdef EMPULSE
			if (chan->itimer) /* if timer still running */
			{
			    int plen = chan->itimerset - chan->itimer;
			    if (plen <= DAHDI_MAXPULSETIME)
			    {
					if (plen >= DAHDI_MINPULSETIME)
					{
						chan->pulsecount++;

						chan->pulsetimer = DAHDI_PULSETIMEOUT;
                                                chan->itimerset = chan->itimer = 0;
						if (chan->pulsecount == 1)
							__qevent(chan,DAHDI_EVENT_PULSE_START);
					}
			    }
			    break;
			}
#endif
			/* set wink timer */
			chan->itimerset = chan->itimer = chan->rxwinktime * DAHDI_CHUNKSIZE;
			break;
		    case DAHDI_RXSIG_ONHOOK: /* went on hook */
			/* This interface is now going on hook.
			   Check for WINK, etc */
			if (chan->itimer)
				__qevent(chan,DAHDI_EVENT_WINKFLASH);
#if defined(EMFLASH) || defined(EMPULSE)
			else {
#ifdef EMFLASH
				chan->itimerset = chan->itimer = chan->rxflashtime * DAHDI_CHUNKSIZE;

#else /* EMFLASH */
				chan->itimerset = chan->itimer = chan->rxwinktime * DAHDI_CHUNKSIZE;

#endif /* EMFLASH */
				chan->gotgs = 0;
				break;
			}
#else /* EMFLASH || EMPULSE */
			else {
				__qevent(chan,DAHDI_EVENT_ONHOOK);
				chan->gotgs = 0;
			}
#endif
			chan->itimerset = chan->itimer = 0;
			break;
		    default:
			break;
		}
		break;
	   case DAHDI_SIG_FXSKS:  /* FXS Kewlstart */
		  /* ignore a bit error if loop not closed and stable */
		if (chan->txstate != DAHDI_TXSTATE_OFFHOOK) break;
#ifdef	FXSFLASH
		if (rxsig == DAHDI_RXSIG_ONHOOK) {
			chan->itimer = DAHDI_FXSFLASHMAXTIME * DAHDI_CHUNKSIZE;
			break;
		} else 	if (rxsig == DAHDI_RXSIG_OFFHOOK) {
			if (chan->itimer) {
				/* did the offhook occur in the window? if not, ignore both events */
				if (chan->itimer <= ((DAHDI_FXSFLASHMAXTIME - DAHDI_FXSFLASHMINTIME) * DAHDI_CHUNKSIZE))
					__qevent(chan, DAHDI_EVENT_WINKFLASH);
			}
			chan->itimer = 0;
			break;
		}
#endif
		/* fall through intentionally */
	   case DAHDI_SIG_FXSGS:  /* FXS Groundstart */
		if (rxsig == DAHDI_RXSIG_ONHOOK) {
			chan->ringdebtimer = RING_DEBOUNCE_TIME;
			chan->ringtrailer = 0;
			if (chan->txstate != DAHDI_TXSTATE_DEBOUNCE) {
				chan->gotgs = 0;
				__qevent(chan,DAHDI_EVENT_ONHOOK);
			}
		}
		break;
	   case DAHDI_SIG_FXOGS: /* FXO Groundstart */
		if (rxsig == DAHDI_RXSIG_START) {
			  /* if havent got gs, report it */
			if (!chan->gotgs) {
				__qevent(chan,DAHDI_EVENT_RINGOFFHOOK);
				chan->gotgs = 1;
			}
		}
		/* fall through intentionally */
	   case DAHDI_SIG_FXOLS: /* FXO Loopstart */
	   case DAHDI_SIG_FXOKS: /* FXO Kewlstart */
		switch(rxsig) {
		    case DAHDI_RXSIG_OFFHOOK: /* went off hook */
			  /* if asserti ng ring, stop it */
			if (chan->txstate == DAHDI_TXSTATE_START) {
				dahdi_rbs_sethook(chan,DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_AFTERSTART, DAHDI_AFTERSTART_TIME);
			}
			chan->kewlonhook = 0;
#ifdef CONFIG_DAHDI_DEBUG
			module_printk(KERN_NOTICE, "Off hook on channel %d, itimer = %d, gotgs = %d\n", chan->channo, chan->itimer, chan->gotgs);
#endif
			if (chan->itimer) /* if timer still running */
			{
			    int plen = chan->itimerset - chan->itimer;
			    if (plen <= DAHDI_MAXPULSETIME)
			    {
					if (plen >= DAHDI_MINPULSETIME)
					{
						chan->pulsecount++;
						chan->pulsetimer = DAHDI_PULSETIMEOUT;
						chan->itimer = chan->itimerset;
						if (chan->pulsecount == 1)
							__qevent(chan,DAHDI_EVENT_PULSE_START);
					}
			    } else
					__qevent(chan,DAHDI_EVENT_WINKFLASH);
			} else {
				  /* if havent got GS detect */
				if (!chan->gotgs) {
					__qevent(chan,DAHDI_EVENT_RINGOFFHOOK);
					chan->gotgs = 1;
					chan->itimerset = chan->itimer = 0;
				}
			}
			chan->itimerset = chan->itimer = 0;
			break;
		    case DAHDI_RXSIG_ONHOOK: /* went on hook */
			  /* if not during offhook debounce time */
			if ((chan->txstate != DAHDI_TXSTATE_DEBOUNCE) &&
			    (chan->txstate != DAHDI_TXSTATE_KEWL) &&
			    (chan->txstate != DAHDI_TXSTATE_AFTERKEWL)) {
				chan->itimerset = chan->itimer = chan->rxflashtime * DAHDI_CHUNKSIZE;
			}
			if (chan->txstate == DAHDI_TXSTATE_KEWL)
				chan->kewlonhook = 1;
			break;
		    default:
			break;
		}
	    default:
		break;
	}
}

/**
 * dahdi_hooksig() - send a signal on a channel to userspace
 * @chan:	the DAHDI channel
 * @rxsig:	signal (number) to send
 *
 * Called from a channel driver to send a DAHDI signal to userspace.
 * The signal will be queued for delivery to userspace.
 *
 * If the signal is the same as previous one sent, it won't be re-sent.
 */
void dahdi_hooksig(struct dahdi_chan *chan, enum dahdi_rxsig rxsig)
{
	  /* skip if no change */
	unsigned long flags;
	spin_lock_irqsave(&chan->lock, flags);
	__dahdi_hooksig_pvt(chan,rxsig);
	spin_unlock_irqrestore(&chan->lock, flags);
}


/**
 * dahdi_rbsbits() - set Rx RBS bits on the channel
 * @chan:	the DAHDI channel
 * @cursig:	the bits to set
 *
 * Set the channel's rxsig (received: from device to userspace) and act
 * accordingly.
 */
void dahdi_rbsbits(struct dahdi_chan *chan, int cursig)
{
	unsigned long flags;
	if (cursig == chan->rxsig)
		return;

	if ((chan->flags & DAHDI_FLAG_SIGFREEZE)) return;

	spin_lock_irqsave(&chan->lock, flags);
	switch(chan->sig) {
	    case DAHDI_SIG_FXOGS: /* FXO Groundstart */
		/* B-bit only matters for FXO GS */
		if (!(cursig & DAHDI_BBIT)) {
			__dahdi_hooksig_pvt(chan, DAHDI_RXSIG_START);
			break;
		}
		/* Fall through */
	    case DAHDI_SIG_EM:  /* E and M */
	    case DAHDI_SIG_EM_E1:
	    case DAHDI_SIG_FXOLS: /* FXO Loopstart */
	    case DAHDI_SIG_FXOKS: /* FXO Kewlstart */
		if (cursig & DAHDI_ABIT)  /* off hook */
			__dahdi_hooksig_pvt(chan,DAHDI_RXSIG_OFFHOOK);
		else /* on hook */
			__dahdi_hooksig_pvt(chan,DAHDI_RXSIG_ONHOOK);
		break;

	   case DAHDI_SIG_FXSKS:  /* FXS Kewlstart */
	   case DAHDI_SIG_FXSGS:  /* FXS Groundstart */
		/* Fall through */
	   case DAHDI_SIG_FXSLS:
		if (!(cursig & DAHDI_BBIT)) {
			/* Check for ringing first */
			__dahdi_hooksig_pvt(chan, DAHDI_RXSIG_RING);
			break;
		}
		if ((chan->sig != DAHDI_SIG_FXSLS) && (cursig & DAHDI_ABIT)) {
			    /* if went on hook */
			__dahdi_hooksig_pvt(chan, DAHDI_RXSIG_ONHOOK);
		} else {
			__dahdi_hooksig_pvt(chan, DAHDI_RXSIG_OFFHOOK);
		}
		break;
	   case DAHDI_SIG_CAS:
		/* send event that something changed */
		__qevent(chan, DAHDI_EVENT_BITSCHANGED);
		break;

	   default:
		break;
	}
	/* Keep track of signalling for next time */
	chan->rxsig = cursig;
	spin_unlock_irqrestore(&chan->lock, flags);

	if ((debug & DEBUG_RBS) && printk_ratelimit()) {
		chan_notice(chan, "Detected sigbits change to %04x\n", cursig);
	}
}

static void process_echocan_events(struct dahdi_chan *chan)
{
	union dahdi_echocan_events events = chan->ec_state->events;

	if (events.bit.CED_tx_detected) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_TX_CED_DETECTED);
		if (chan->ec_state) {
			if (chan->ec_state->status.mode == ECHO_MODE_ACTIVE)
				set_echocan_fax_mode(chan, chan->channo, "CED tx detected", 1);
			else
				module_printk(KERN_NOTICE, "Detected CED tone (tx) on channel %d\n", chan->channo);
		}
	}

	if (events.bit.CED_rx_detected) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_RX_CED_DETECTED);
		if (chan->ec_state) {
			if (chan->ec_state->status.mode == ECHO_MODE_ACTIVE)
				set_echocan_fax_mode(chan, chan->channo, "CED rx detected", 1);
			else
				module_printk(KERN_NOTICE, "Detected CED tone (rx) on channel %d\n", chan->channo);
		}
	}

	if (events.bit.CNG_tx_detected)
		dahdi_qevent_nolock(chan, DAHDI_EVENT_TX_CNG_DETECTED);

	if (events.bit.CNG_rx_detected)
		dahdi_qevent_nolock(chan, DAHDI_EVENT_RX_CNG_DETECTED);

	if (events.bit.NLP_auto_disabled) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_DISABLED);
		chan->ec_state->status.mode = ECHO_MODE_FAX;
	}

	if (events.bit.NLP_auto_enabled) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_ENABLED);
		chan->ec_state->status.mode = ECHO_MODE_ACTIVE;
	}
}

/**
 * __dahdi_ec_chunk() - process echo for a single channel
 * @ss:		DAHDI channel
 * @rxchunk:	buffer to store audio with cancelled audio
 * @preecchunk: chunk of audio on which to cancel echo
 * @txchunk:	reference chunk from the other direction
 *
 * The echo canceller function fixes received (from device to userspace)
 * audio. In order to fix it it uses the transmitted audio as a
 * reference. This call updates the echo canceller for a single chunk (8
 * bytes).
 *
 * Call with local interrupts disabled.
 */
void __dahdi_ec_chunk(struct dahdi_chan *ss, u8 *rxchunk,
		      const u8 *preecchunk, const u8 *txchunk)
{
	short rxlin;
	int x;

	spin_lock(&ss->lock);

	if (ss->readchunkpreec) {
		/* Save a copy of the audio before the echo can has its way with it */
		for (x = 0; x < DAHDI_CHUNKSIZE; x++)
			/* We only ever really need to deal with signed linear - let's just convert it now */
			ss->readchunkpreec[x] = DAHDI_XLAW(preecchunk[x], ss);
	}

	/* Perform echo cancellation on a chunk if necessary */
	if (ss->ec_state) {
#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
		dahdi_kernel_fpu_begin();
#endif
		if (ss->ec_state->status.mode & __ECHO_MODE_MUTE) {
			/* Special stuff for training the echo can */
			for (x=0;x<DAHDI_CHUNKSIZE;x++) {
				rxlin = DAHDI_XLAW(preecchunk[x], ss);
				if (ss->ec_state->status.mode == ECHO_MODE_PRETRAINING) {
					if (--ss->ec_state->status.pretrain_timer <= 0) {
						ss->ec_state->status.pretrain_timer = 0;
						ss->ec_state->status.mode = ECHO_MODE_STARTTRAINING;
					}
				}
				if (ss->ec_state->status.mode == ECHO_MODE_AWAITINGECHO) {
					ss->ec_state->status.last_train_tap = 0;
					ss->ec_state->status.mode = ECHO_MODE_TRAINING;
				}
				if ((ss->ec_state->status.mode == ECHO_MODE_TRAINING) &&
				    (ss->ec_state->ops->echocan_traintap)) {
					if (ss->ec_state->ops->echocan_traintap(ss->ec_state, ss->ec_state->status.last_train_tap++, rxlin)) {
						ss->ec_state->status.mode = ECHO_MODE_ACTIVE;
					}
				}
				rxlin = 0;
				rxchunk[x] = DAHDI_LIN2X((int)rxlin, ss);
			}
		} else if (ss->ec_state->status.mode != ECHO_MODE_IDLE) {
			ss->ec_state->events.all = 0;

			if (ss->ec_state->ops->echocan_process) {
				short rxlins[DAHDI_CHUNKSIZE], txlins[DAHDI_CHUNKSIZE];

				for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
					rxlins[x] = DAHDI_XLAW(preecchunk[x],
							       ss);
					txlins[x] = DAHDI_XLAW(txchunk[x], ss);
				}
				ss->ec_state->ops->echocan_process(ss->ec_state, rxlins, txlins, DAHDI_CHUNKSIZE);

				for (x = 0; x < DAHDI_CHUNKSIZE; x++)
					rxchunk[x] = DAHDI_LIN2X((int) rxlins[x], ss);
			} else if (ss->ec_state->ops->echocan_events)
				ss->ec_state->ops->echocan_events(ss->ec_state);

			if (ss->ec_state->events.all)
				process_echocan_events(ss);

		}
#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
		dahdi_kernel_fpu_end();
#endif
	}

	spin_unlock(&ss->lock);
}
EXPORT_SYMBOL(__dahdi_ec_chunk);

/**
 * dahdi_ec_span() - process echo for all channels in a span.
 * @span:	DAHDI span
 *
 * Similar to calling dahdi_ec_chunk() for each of the channels in the
 * span. Uses dahdi_chunk.write_chunk for the rxchunk (the chunk to fix)
 * and dahdi_chan.readchunk as the txchunk (the reference chunk).
 */
void _dahdi_ec_span(struct dahdi_span *span)
{
	int x;
	for (x = 0; x < span->channels; x++) {
		struct dahdi_chan *const chan = span->chans[x];
		if (!chan->ec_current)
			continue;
		_dahdi_ec_chunk(chan, chan->readchunk, chan->writechunk);
	}
}
EXPORT_SYMBOL(_dahdi_ec_span);

/* return 0 if nothing detected, 1 if lack of tone, 2 if presence of tone */
/* modifies buffer pointed to by 'amp' with notched-out values */
static inline int sf_detect(struct sf_detect_state *s,
                 short *amp,
                 int samples,long p1, long p2, long p3)
{
int     i,rv = 0;
long x,y;

#define	SF_DETECT_SAMPLES (DAHDI_CHUNKSIZE * 5)
#define	SF_DETECT_MIN_ENERGY 500
#define	NB 14  /* number of bits to shift left */

        /* determine energy level before filtering */
        for(i = 0; i < samples; i++)
        {
                if (amp[i] < 0) s->e1 -= amp[i];
                else s->e1 += amp[i];
        }
	/* do 2nd order IIR notch filter at given freq. and calculate
	    energy */
        for(i = 0; i < samples; i++)
        {
                x = amp[i] << NB;
                y = s->x2 + (p1 * (s->x1 >> NB)) + x;
                y += (p2 * (s->y2 >> NB)) +
			(p3 * (s->y1 >> NB));
                s->x2 = s->x1;
                s->x1 = x;
                s->y2 = s->y1;
                s->y1 = y;
                amp[i] = y >> NB;
                if (amp[i] < 0) s->e2 -= amp[i];
                else s->e2 += amp[i];
        }
	s->samps += i;
	/* if time to do determination */
	if ((s->samps) >= SF_DETECT_SAMPLES)
	{
		rv = 1; /* default to no tone */
		/* if enough energy, it is determined to be a tone */
		if (((s->e1 - s->e2) / s->samps) > SF_DETECT_MIN_ENERGY) rv = 2;
		/* reset energy processing variables */
		s->samps = 0;
		s->e1 = s->e2 = 0;
	}
	return(rv);
}

static inline void __dahdi_process_putaudio_chunk(struct dahdi_chan *ss, unsigned char *rxb)
{
	/* We transmit data from our master channel */
	/* Called with ss->lock held */
	struct dahdi_chan *ms = ss->master;
	/* Linear version of received data */
	short putlin[DAHDI_CHUNKSIZE],k[DAHDI_CHUNKSIZE];
	int x,r;

	if (ms->dialing) ms->afterdialingtimer = 50;
	else if (ms->afterdialingtimer) ms->afterdialingtimer--;
	if (ms->afterdialingtimer && !is_pseudo_chan(ms)) {
		/* Be careful since memset is likely a macro */
		rxb[0] = DAHDI_LIN2X(0, ms);
		memset(&rxb[1], rxb[0], DAHDI_CHUNKSIZE - 1);  /* receive as silence if dialing */
	}
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		rxb[x] = ms->rxgain[rxb[x]];
		putlin[x] = DAHDI_XLAW(rxb[x], ms);
	}

#ifndef CONFIG_DAHDI_NO_ECHOCAN_DISABLE
	if (ms->ec_state && (ms->ec_state->status.mode == ECHO_MODE_ACTIVE) && !ms->ec_state->features.CED_rx_detect) {
		for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
			if (echo_can_disable_detector_update(&ms->ec_state->rxecdis, putlin[x])) {
				set_echocan_fax_mode(ms, ss->channo, "CED rx detected", 1);
				dahdi_qevent_nolock(ms, DAHDI_EVENT_RX_CED_DETECTED);
				break;
			}
		}
	}
#endif

	/* if doing rx tone decoding */
	if (ms->rxp1 && ms->rxp2 && ms->rxp3)
	{
		r = sf_detect(&ms->rd,putlin,DAHDI_CHUNKSIZE,ms->rxp1,
			ms->rxp2,ms->rxp3);
		/* Convert back */
		for(x=0;x<DAHDI_CHUNKSIZE;x++)
			rxb[x] = DAHDI_LIN2X(putlin[x], ms);
		if (r) /* if something happened */
		{
			if (r != ms->rd.lastdetect)
			{
				if (((r == 2) && !(ms->toneflags & DAHDI_REVERSE_RXTONE)) ||
				    ((r == 1) && (ms->toneflags & DAHDI_REVERSE_RXTONE)))
				{
					__qevent(ms,DAHDI_EVENT_RINGOFFHOOK);
				}
				else
				{
					__qevent(ms,DAHDI_EVENT_ONHOOK);
				}
				ms->rd.lastdetect = r;
			}
		}
	}

	if (!is_pseudo_chan(ms)) {
		memcpy(ms->putlin, putlin, DAHDI_CHUNKSIZE * sizeof(short));
		memcpy(ms->putraw, rxb, DAHDI_CHUNKSIZE);
	}

	/* Take the rxc, twiddle it for conferencing if appropriate and put it
	   back */
	if ((!ms->confmute && !ms->afterdialingtimer) || is_pseudo_chan(ms)) {
		struct dahdi_chan *const conf_chan = ms->conf_chan;
		switch(ms->confmode & DAHDI_CONF_MODE_MASK) {
		case DAHDI_CONF_NORMAL:		/* Normal mode */
			/* Do nothing.  rx goes output */
			break;
		case DAHDI_CONF_MONITOR:		/* Monitor a channel's rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;
			/* Add monitored channel */
			if (is_pseudo_chan(conf_chan))
				ACSS(putlin, conf_chan->getlin);
			else
				ACSS(putlin, conf_chan->putlin);
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_MONITORTX:	/* Monitor a channel's tx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;
			/* Add monitored channel */
			if (is_pseudo_chan(conf_chan))
				ACSS(putlin, conf_chan->putlin);
			else
				ACSS(putlin, conf_chan->getlin);
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_MONITORBOTH:	/* Monitor a channel's tx and rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;
			/* Note: Technically, saturation should be done at
			   the end of the whole addition, but for performance
			   reasons, we don't do that.  Besides, it only matters
			   when you're so loud you're clipping anyway */
			ACSS(putlin, conf_chan->getlin);
			ACSS(putlin, conf_chan->putlin);
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_MONITOR_RX_PREECHO:		/* Monitor a channel's rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(putlin, is_pseudo_chan(conf_chan) ?
			     conf_chan->getlin : conf_chan->readchunkpreec);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);

			break;
		case DAHDI_CONF_MONITOR_TX_PREECHO:	/* Monitor a channel's tx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(putlin, is_pseudo_chan(conf_chan) ?
			     conf_chan->readchunkpreec : conf_chan->getlin);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);

			break;
		case DAHDI_CONF_MONITORBOTH_PREECHO:	/* Monitor a channel's tx and rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Note: Technically, saturation should be done at
			   the end of the whole addition, but for performance
			   reasons, we don't do that.  Besides, it only matters
			   when you're so loud you're clipping anyway */
			ACSS(putlin, conf_chan->getlin);
			ACSS(putlin, conf_chan->readchunkpreec);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);

			break;
		case DAHDI_CONF_REALANDPSEUDO:
			  /* do normal conf mode processing */
			if (ms->confmode & DAHDI_CONF_TALKER) {
				/* Store temp value */
				memcpy(k, putlin, DAHDI_CHUNKSIZE * sizeof(short));
				/* Add conf value */
				ACSS(k, conf_sums_next[ms->_confn]);
				/*  get amount actually added */
				memcpy(ms->conflast, k, DAHDI_CHUNKSIZE * sizeof(short));
				SCSS(ms->conflast, conf_sums_next[ms->_confn]);
				/* Really add in new value */
				ACSS(conf_sums_next[ms->_confn], ms->conflast);
			} else memset(ms->conflast, 0, DAHDI_CHUNKSIZE * sizeof(short));
			  /* do the pseudo-channel part processing */
			memset(putlin, 0, DAHDI_CHUNKSIZE * sizeof(short));
			if (ms->confmode & DAHDI_CONF_PSEUDO_LISTENER) {
				/* Subtract out previous last sample written to conf */
				SCSS(putlin, ms->conflast2);
				/* Add in conference */
				ACSS(putlin, conf_sums[ms->_confn]);
			}
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_CONF:	/* Normal conference mode */
			if (is_pseudo_chan(ms)) /* if a pseudo-channel */
			   {
				if (ms->confmode & DAHDI_CONF_LISTENER) {
					/* Subtract out last sample written to conf */
					SCSS(putlin, ms->conflast);
					/* Add in conference */
					ACSS(putlin, conf_sums[ms->_confn]);
				}
				/* Convert back */
				for(x=0;x<DAHDI_CHUNKSIZE;x++)
					rxb[x] = DAHDI_LIN2X(putlin[x], ms);
				memcpy(ss->putlin, putlin, DAHDI_CHUNKSIZE * sizeof(short));
				break;
			   }
			/* fall through */
		case DAHDI_CONF_CONFANN:  /* Conference with announce */
			if (ms->confmode & DAHDI_CONF_TALKER) {
				/* Store temp value */
				memcpy(k, putlin, DAHDI_CHUNKSIZE * sizeof(short));
				/* Add conf value */
				ACSS(k, conf_sums_next[ms->_confn]);
				/*  get amount actually added */
				memcpy(ms->conflast, k, DAHDI_CHUNKSIZE * sizeof(short));
				SCSS(ms->conflast, conf_sums_next[ms->_confn]);
				/* Really add in new value */
				ACSS(conf_sums_next[ms->_confn], ms->conflast);
			} else
				memset(ms->conflast, 0, DAHDI_CHUNKSIZE * sizeof(short));
			  /* rxc unmodified */
			break;
		case DAHDI_CONF_CONFMON:
		case DAHDI_CONF_CONFANNMON:
			if (ms->confmode & DAHDI_CONF_TALKER) {
				/* Store temp value */
				memcpy(k, putlin, DAHDI_CHUNKSIZE * sizeof(short));
				/* Subtract last value */
				SCSS(conf_sums[ms->_confn], ms->conflast);
				/* Add conf value */
				ACSS(k, conf_sums[ms->_confn]);
				/*  get amount actually added */
				memcpy(ms->conflast, k, DAHDI_CHUNKSIZE * sizeof(short));
				SCSS(ms->conflast, conf_sums[ms->_confn]);
				/* Really add in new value */
				ACSS(conf_sums[ms->_confn], ms->conflast);
			} else
				memset(ms->conflast, 0, DAHDI_CHUNKSIZE * sizeof(short));
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X((int)conf_sums_prev[ms->_confn][x], ms);
			break;
		case DAHDI_CONF_DIGITALMON:
			  /* if not a pseudo-channel, ignore */
			if (!is_pseudo_chan(ms))
				break;
			/* Add monitored channel */
			if (is_pseudo_chan(conf_chan))
				memcpy(rxb, conf_chan->getraw, DAHDI_CHUNKSIZE);
			else
				memcpy(rxb, conf_chan->putraw, DAHDI_CHUNKSIZE);
			break;
		}
	}
}

/* HDLC (or other) receiver buffer functions for read side */
static void __putbuf_chunk(struct dahdi_chan *ss, unsigned char *rxb, int bytes)
{
	/* We transmit data from our master channel */
	/* Called with ss->lock held */
	struct dahdi_chan *ms = ss->master;
	/* Our receive buffer */
	unsigned char *buf;
#if defined(CONFIG_DAHDI_NET)  || defined(CONFIG_DAHDI_PPP)
	/* SKB for receiving network stuff */
	struct sk_buff *skb=NULL;
#endif
	int oldbuf;
	int eof=0;
	int abort=0;
	int res;
	int left, x;

	while(bytes) {
#if defined(CONFIG_DAHDI_NET)  || defined(CONFIG_DAHDI_PPP)
		skb = NULL;
#endif
		abort = 0;
		eof = 0;
		/* Next, figure out if we've got a buffer to receive into */
		if (ms->inreadbuf > -1) {
			/* Read into the current buffer */
			buf = ms->readbuf[ms->inreadbuf];
			left = ms->blocksize - ms->readidx[ms->inreadbuf];
			if (left > bytes)
				left = bytes;
			if (ms->flags & DAHDI_FLAG_HDLC) {
				for (x=0;x<left;x++) {
					/* Handle HDLC deframing */
					fasthdlc_rx_load_nocheck(&ms->rxhdlc, *(rxb++));
					bytes--;
					res = fasthdlc_rx_run(&ms->rxhdlc);
					/* If there is nothing there, continue */
					if (res & RETURN_EMPTY_FLAG)
						continue;
					else if (res & RETURN_COMPLETE_FLAG) {
						/* Only count this if it's a non-empty frame */
						if (ms->readidx[ms->inreadbuf]) {
							if ((ms->flags & DAHDI_FLAG_FCS) && (ms->infcs != PPP_GOODFCS)) {
								abort = DAHDI_EVENT_BADFCS;
							} else
								eof=1;
							break;
						}
						continue;
					} else if (res & RETURN_DISCARD_FLAG) {
						/* This could be someone idling with
						  "idle" instead of "flag" */
						if (!ms->readidx[ms->inreadbuf])
							continue;
						abort = DAHDI_EVENT_ABORT;
						break;
					} else {
						unsigned char rxc;
						rxc = res;
						ms->infcs = PPP_FCS(ms->infcs, rxc);
						buf[ms->readidx[ms->inreadbuf]++] = rxc;
						/* Pay attention to the possibility of an overrun */
						if (ms->readidx[ms->inreadbuf] >= ms->blocksize) {
							if (!ss->span->alarms)
								module_printk(KERN_WARNING, "HDLC Receiver overrun on channel %s (master=%s)\n", ss->name, ss->master->name);
							abort=DAHDI_EVENT_OVERRUN;
							/* Force the HDLC state back to frame-search mode */
							ms->rxhdlc.state = 0;
							ms->rxhdlc.bits = 0;
							ms->readidx[ms->inreadbuf]=0;
							break;
						}
					}
				}
			} else {
				/* Not HDLC */
				memcpy(buf + ms->readidx[ms->inreadbuf], rxb, left);
				rxb += left;
				ms->readidx[ms->inreadbuf] += left;
				bytes -= left;
				/* End of frame is decided by block size of 'N' */
				eof = (ms->readidx[ms->inreadbuf] >= ms->blocksize);
				if (eof && (ss->flags & DAHDI_FLAG_NOSTDTXRX)) {
					eof = 0;
					abort = DAHDI_EVENT_OVERRUN;
				}
			}
			if (eof)  {
				/* Finished with this buffer, try another. */
				oldbuf = ms->inreadbuf;
				ms->infcs = PPP_INITFCS;
				ms->readn[ms->inreadbuf] = ms->readidx[ms->inreadbuf];
#ifdef CONFIG_DAHDI_DEBUG
				module_printk(KERN_NOTICE, "EOF, len is %d\n", ms->readn[ms->inreadbuf]);
#endif
#if defined(CONFIG_DAHDI_NET) || defined(CONFIG_DAHDI_PPP)
				if ((ms->flags & DAHDI_FLAG_PPP) ||
				    dahdi_have_netdev(ms)) {
#ifdef CONFIG_DAHDI_NET
#endif /* CONFIG_DAHDI_NET */
					/* Our network receiver logic is MUCH
					  different.  We actually only use a single
					  buffer */
					if (ms->readn[ms->inreadbuf] > 1) {
						/* Drop the FCS */
						ms->readn[ms->inreadbuf] -= 2;
						/* Allocate an SKB */
#ifdef CONFIG_DAHDI_PPP
						if (!ms->do_ppp_error)
#endif
							skb = dev_alloc_skb(ms->readn[ms->inreadbuf] + 2);
						if (skb) {
							unsigned char cisco_addr = *(ms->readbuf[ms->inreadbuf]);
							if (cisco_addr != 0x0f && cisco_addr != 0x8f)
								skb_reserve(skb, 2);
							/* XXX Get rid of this memcpy XXX */
							memcpy(skb->data, ms->readbuf[ms->inreadbuf], ms->readn[ms->inreadbuf]);
							skb_put(skb, ms->readn[ms->inreadbuf]);
#ifdef CONFIG_DAHDI_NET
							if (dahdi_have_netdev(ms)) {
								struct net_device_stats *stats = hdlc_stats(ms->hdlcnetdev->netdev);
								stats->rx_packets++;
								stats->rx_bytes += ms->readn[ms->inreadbuf];
							}
#endif

						} else {
#ifdef CONFIG_DAHDI_NET
							if (dahdi_have_netdev(ms)) {
								struct net_device_stats *stats = hdlc_stats(ms->hdlcnetdev->netdev);
								stats->rx_dropped++;
							}
#endif
#ifdef CONFIG_DAHDI_PPP
							if (ms->flags & DAHDI_FLAG_PPP) {
								abort = DAHDI_EVENT_OVERRUN;
							}
#endif
#if 1
#ifdef CONFIG_DAHDI_PPP
							if (!ms->do_ppp_error)
#endif
								module_printk(KERN_NOTICE, "Memory squeeze, dropped one\n");
#endif
						}
					}
					/* We don't cycle through buffers, just
					reuse the same one */
					ms->readn[ms->inreadbuf] = 0;
					ms->readidx[ms->inreadbuf] = 0;
				} else
#endif
				{
					/* This logic might confuse and astound.  Basically we need to find
					 * the previous buffer index.  It should be safe because, regardless
					 * of whether or not it has been copied to user space, nothing should
					 * have messed around with it since then */

					int comparemessage;
					/* Shut compiler up */
					int myres = 0;

					if (ms->flags & DAHDI_FLAG_MTP2) {
						comparemessage = (ms->inreadbuf - 1) & (ms->numbufs - 1);

						myres = memcmp(ms->readbuf[comparemessage], ms->readbuf[ms->inreadbuf], ms->readn[ms->inreadbuf]);
					}

					if ((ms->flags & DAHDI_FLAG_MTP2) && !myres) {
						/* Our messages are the same, so discard -
						 * 	Don't advance buffers, reset indexes and buffer sizes. */
						ms->readn[ms->inreadbuf] = 0;
						ms->readidx[ms->inreadbuf] = 0;
					} else {
						ms->inreadbuf = (ms->inreadbuf + 1) % ms->numbufs;
						if (ms->inreadbuf == ms->outreadbuf) {
							/* Whoops, we're full, and have no where else
							   to store into at the moment.  We'll drop it
							   until there's a buffer available */
#ifdef BUFFER_DEBUG
							module_printk(KERN_NOTICE, "Out of storage space\n");
#endif
							ms->inreadbuf = -1;
							/* Enable the receiver in case they've got POLICY_WHEN_FULL */
							ms->rxdisable = 0;
						}
						if (ms->outreadbuf < 0) { /* start out buffer if not already */
							ms->outreadbuf = oldbuf;
							/* if there are processes waiting in poll() on this channel,
							   wake them up */
							if (!ms->rxdisable) {
								wake_up_interruptible(&ms->waitq);
							}
						}
/* In the very orignal driver, it was quite well known to me (Jim) that there
was a possibility that a channel sleeping on a receive block needed to
be potentially woken up EVERY time a buffer was filled, not just on the first
one, because if only done on the first one there is a slight timing potential
of missing the wakeup (between where it senses the (lack of) active condition
(with interrupts disabled) and where it does the sleep (interrupts enabled)
in the read or iomux call, etc). That is why the read and iomux calls start
with an infinite loop that gets broken out of upon an active condition,
otherwise keeps sleeping and looking. The part in this code got "optimized"
out in the later versions, and is put back now. Note that this is *NOT*
needed for poll() waiters, because the poll_wait() function that is used there
is atomic enough for this purpose; it will not go to sleep before ensuring
that the waitqueue is empty. */
						if (!ms->rxdisable) { /* if receiver enabled */
							/* Notify a blocked reader that there is data available
							to be read, unless we're waiting for it to be full */
#ifdef CONFIG_DAHDI_DEBUG
							module_printk(KERN_NOTICE, "Notifying reader data in block %d\n", oldbuf);
#endif
							wake_up_interruptible(&ms->waitq);
						}
					}
				}
			}
			if (abort) {
				/* Start over reading frame */
				ms->readidx[ms->inreadbuf] = 0;
				ms->infcs = PPP_INITFCS;

#ifdef CONFIG_DAHDI_NET
				if (dahdi_have_netdev(ms)) {
					struct net_device_stats *stats = hdlc_stats(ms->hdlcnetdev->netdev);
					stats->rx_errors++;
					if (abort == DAHDI_EVENT_OVERRUN)
						stats->rx_over_errors++;
					if (abort == DAHDI_EVENT_BADFCS)
						stats->rx_crc_errors++;
					if (abort == DAHDI_EVENT_ABORT)
						stats->rx_frame_errors++;
				} else
#endif
#ifdef CONFIG_DAHDI_PPP
				if (ms->flags & DAHDI_FLAG_PPP) {
					ms->do_ppp_error = 1;
					tasklet_schedule(&ms->ppp_calls);
				} else
#endif
					if (test_bit(DAHDI_FLAGBIT_OPEN, &ms->flags) && !ss->span->alarms) {
						/* Notify the receiver... */
						__qevent(ss->master, abort);
					}
			}
		} else /* No place to receive -- drop on the floor */
			break;
#ifdef CONFIG_DAHDI_NET
		if (skb && dahdi_have_netdev(ms))
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
			skb->mac.raw = skb->data;
#else
			skb_reset_mac_header(skb);
#endif
			skb->dev = chan_to_netdev(ms);
#ifdef DAHDI_HDLC_TYPE_TRANS
			skb->protocol = hdlc_type_trans(skb,
							chan_to_netdev(ms));
#else
			skb->protocol = htons (ETH_P_HDLC);
#endif
			netif_rx(skb);
		}
#endif
#ifdef CONFIG_DAHDI_PPP
		if (skb && (ms->flags & DAHDI_FLAG_PPP)) {
			unsigned char *tmp;
			tmp = skb->data;
			skb_pull(skb, 2);
			/* Make sure that it's addressed to ALL STATIONS and UNNUMBERED */
			if (!tmp || (tmp[0] != 0xff) || (tmp[1] != 0x03)) {
				/* Invalid SKB -- drop */
				if (tmp)
					module_printk(KERN_NOTICE, "Received invalid SKB (%02x, %02x)\n", tmp[0], tmp[1]);
				dev_kfree_skb_irq(skb);
			} else {
				skb_queue_tail(&ms->ppp_rq, skb);
				tasklet_schedule(&ms->ppp_calls);
			}
		}
#endif
	}

	if (bytes) {
		if (!test_bit(DAHDI_FLAGBIT_RXOVERRUN, &ms->flags)) {
			if (test_bit(DAHDI_FLAGBIT_BUFEVENTS, &ms->flags))
				__qevent(ms, DAHDI_EVENT_READ_OVERRUN);
			set_bit(DAHDI_FLAGBIT_RXOVERRUN, &ms->flags);
		}
	} else {
		clear_bit(DAHDI_FLAGBIT_RXOVERRUN, &ms->flags);
	}
}

static inline void __dahdi_putbuf_chunk(struct dahdi_chan *ss, unsigned char *rxb)
{
	__putbuf_chunk(ss, rxb, DAHDI_CHUNKSIZE);

#ifdef CONFIG_DAHDI_MIRROR
	if (ss->rxmirror) {
		spin_lock(&ss->rxmirror->lock);
		__putbuf_chunk(ss->rxmirror, rxb, DAHDI_CHUNKSIZE);
		spin_unlock(&ss->rxmirror->lock);
	}
#endif /* CONFIG_DAHDI_MIRROR */
}

static void __dahdi_hdlc_abort(struct dahdi_chan *ss, int event)
{
	if (ss->inreadbuf >= 0)
		ss->readidx[ss->inreadbuf] = 0;
	if (test_bit(DAHDI_FLAGBIT_OPEN, &ss->flags) && !ss->span->alarms)
		__qevent(ss->master, event);
}

void dahdi_hdlc_abort(struct dahdi_chan *ss, int event)
{
	unsigned long flags;
	spin_lock_irqsave(&ss->lock, flags);
	__dahdi_hdlc_abort(ss, event);
	spin_unlock_irqrestore(&ss->lock, flags);
}

void dahdi_hdlc_putbuf(struct dahdi_chan *ss, unsigned char *rxb, int bytes)
{
	unsigned long flags;
	int left;

	spin_lock_irqsave(&ss->lock, flags);
	if (ss->inreadbuf < 0) {
#ifdef CONFIG_DAHDI_DEBUG
		module_printk(KERN_NOTICE, "No place to receive HDLC frame\n");
#endif
		spin_unlock_irqrestore(&ss->lock, flags);
		return;
	}
	/* Read into the current buffer */
	left = ss->blocksize - ss->readidx[ss->inreadbuf];
	if (left > bytes)
		left = bytes;
	if (left > 0) {
		memcpy(ss->readbuf[ss->inreadbuf] + ss->readidx[ss->inreadbuf], rxb, left);
		rxb += left;
		ss->readidx[ss->inreadbuf] += left;
		bytes -= left;
	}
	/* Something isn't fit into buffer */
	if (bytes) {
#ifdef CONFIG_DAHDI_DEBUG
		module_printk(KERN_NOTICE, "HDLC frame isn't fit into buffer space\n");
#endif
		__dahdi_hdlc_abort(ss, DAHDI_EVENT_OVERRUN);
	}
	spin_unlock_irqrestore(&ss->lock, flags);
}

void dahdi_hdlc_finish(struct dahdi_chan *ss)
{
	int oldreadbuf;
	unsigned long flags;

	spin_lock_irqsave(&ss->lock, flags);

	if ((oldreadbuf = ss->inreadbuf) < 0) {
#ifdef CONFIG_DAHDI_DEBUG
		module_printk(KERN_NOTICE, "No buffers to finish\n");
#endif
		spin_unlock_irqrestore(&ss->lock, flags);
		return;
	}

	if (!ss->readidx[ss->inreadbuf]) {
#ifdef CONFIG_DAHDI_DEBUG
		module_printk(KERN_NOTICE, "Empty HDLC frame received\n");
#endif
		spin_unlock_irqrestore(&ss->lock, flags);
		return;
	}

	ss->readn[ss->inreadbuf] = ss->readidx[ss->inreadbuf];
	ss->inreadbuf = (ss->inreadbuf + 1) % ss->numbufs;
	if (ss->inreadbuf == ss->outreadbuf) {
		ss->inreadbuf = -1;
#ifdef CONFIG_DAHDI_DEBUG
		module_printk(KERN_NOTICE, "Notifying reader data in block %d\n", oldreadbuf);
#endif
		ss->rxdisable = 0;
	}
	if (ss->outreadbuf < 0) {
		ss->outreadbuf = oldreadbuf;
	}

	if (!ss->rxdisable)
		wake_up_interruptible(&ss->waitq);
	spin_unlock_irqrestore(&ss->lock, flags);
}

/* Returns 1 if EOF, 0 if data is still in frame, -1 if EOF and no buffers left */
int dahdi_hdlc_getbuf(struct dahdi_chan *ss, unsigned char *bufptr, unsigned int *size)
{
	unsigned char *buf;
	unsigned long flags;
	int left = 0;
	int res;
	int oldbuf;

	spin_lock_irqsave(&ss->lock, flags);
	if (ss->outwritebuf > -1) {
		buf = ss->writebuf[ss->outwritebuf];
		left = ss->writen[ss->outwritebuf] - ss->writeidx[ss->outwritebuf];
		/* Strip off the empty HDLC CRC end */
		left -= 2;
		if (left <= *size) {
			*size = left;
			res = 1;
		} else
			res = 0;

		memcpy(bufptr, &buf[ss->writeidx[ss->outwritebuf]], *size);
		ss->writeidx[ss->outwritebuf] += *size;

		if (res) {
			/* Rotate buffers */
			oldbuf = ss->outwritebuf;
			ss->writeidx[oldbuf] = 0;
			ss->writen[oldbuf] = 0;
			ss->outwritebuf = (ss->outwritebuf + 1) % ss->numbufs;
			if (ss->outwritebuf == ss->inwritebuf) {
				ss->outwritebuf = -1;
				if (ss->iomask & (DAHDI_IOMUX_WRITE | DAHDI_IOMUX_WRITEEMPTY))
					wake_up_interruptible(&ss->waitq);
				/* If we're only supposed to start when full, disable the transmitter */
				if ((ss->txbufpolicy == DAHDI_POLICY_WHEN_FULL) || (ss->txbufpolicy == DAHDI_POLICY_HALF_FULL))
					ss->txdisable = 1;
				res = -1;
			}

			if (ss->inwritebuf < 0)
				ss->inwritebuf = oldbuf;

			if (!(ss->flags & DAHDI_FLAG_PPP) ||
			    !dahdi_have_netdev(ss)) {
				wake_up_interruptible(&ss->waitq);
			}
		}
	} else {
		res = -1;
		*size = 0;
	}
	spin_unlock_irqrestore(&ss->lock, flags);

	return res;
}


static void process_timers(void)
{
	struct dahdi_timer *cur;

	if (list_empty(&dahdi_timers))
		return;

	spin_lock(&dahdi_timer_lock);
	list_for_each_entry(cur, &dahdi_timers, list) {
		if (cur->ms) {
			cur->pos -= DAHDI_CHUNKSIZE;
			if (cur->pos <= 0) {
				cur->tripped++;
				cur->pos = cur->ms;
				wake_up_interruptible(&cur->sel);
			}
		}
	}
	spin_unlock(&dahdi_timer_lock);
}

static unsigned int dahdi_timer_poll(struct file *file, struct poll_table_struct *wait_table)
{
	struct dahdi_timer *timer = file->private_data;
	unsigned long flags;
	int ret = 0;
	if (timer) {
		poll_wait(file, &timer->sel, wait_table);
		spin_lock_irqsave(&dahdi_timer_lock, flags);
		if (timer->tripped || timer->ping)
			ret |= POLLPRI;
		spin_unlock_irqrestore(&dahdi_timer_lock, flags);
	} else {
		/*
		 * This should never happen. Surprise device removal
		 * should lead us to the nodev_* file_operations
		 */
		msleep(5);
		module_printk(KERN_ERR, "%s: NODEV\n", __func__);
		return POLLERR | POLLHUP | POLLRDHUP | POLLNVAL | POLLPRI;
	}
	return ret;
}

/* device poll routine */
static unsigned int
dahdi_chan_poll(struct file *file, struct poll_table_struct *wait_table)
{
	struct dahdi_chan *const c = file->private_data;
	int ret = 0;
	unsigned long flags;

	if (unlikely(!c)) {
		/*
		 * This should never happen. Surprise device removal
		 * should lead us to the nodev_* file_operations
		 */
		msleep(5);
		module_printk(KERN_ERR, "%s: NODEV\n", __func__);
		return POLLERR | POLLHUP | POLLRDHUP | POLLNVAL | POLLPRI;
	}

	poll_wait(file, &c->waitq, wait_table);

	spin_lock_irqsave(&c->lock, flags);
	ret |= (c->inwritebuf > -1) ? POLLOUT|POLLWRNORM : 0;
	ret |= ((c->outreadbuf > -1) && !c->rxdisable) ?  POLLIN|POLLRDNORM : 0;
	ret |= (c->eventoutidx != c->eventinidx) ? POLLPRI : 0;
	spin_unlock_irqrestore(&c->lock, flags);

	return ret;
}

static unsigned int dahdi_poll(struct file *file, struct poll_table_struct *wait_table)
{
	const int unit = UNIT(file);

	if (likely(unit == DAHDI_TIMER))
		return dahdi_timer_poll(file, wait_table);

	/* transcoders and channels should have updated their file_operations
	 * before poll is ever called. */
	return -EINVAL;
}

static void __dahdi_transmit_chunk(struct dahdi_chan *chan, unsigned char *buf)
{
	unsigned char silly[DAHDI_CHUNKSIZE];
	/* Called with chan->lock locked */
#ifdef	OPTIMIZE_CHANMUTE
	if(likely(chan->chanmute))
		return;
#endif
	if (!buf)
		buf = silly;
	__dahdi_getbuf_chunk(chan, buf);

	if ((chan->flags & DAHDI_FLAG_AUDIO) || (chan->confmode)) {
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_begin();
#endif
		__dahdi_process_getaudio_chunk(chan, buf);
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_end();
#endif
	}
}

static inline void __dahdi_real_transmit(struct dahdi_chan *chan)
{
	/* Called with chan->lock held */
#ifdef	OPTIMIZE_CHANMUTE
	if(likely(chan->chanmute))
		return;
#endif
	if (chan->confmode) {
		/* Pull queued data off the conference */
		__buf_pull(&chan->confout, chan->writechunk, chan);
	} else {
		__dahdi_transmit_chunk(chan, chan->writechunk);
	}
}

static void __dahdi_getempty(struct dahdi_chan *ms, unsigned char *buf)
{
	int bytes = DAHDI_CHUNKSIZE;
	int left;
	unsigned char *txb = buf;
	int x;
	short getlin;
	/* Called with ms->lock held */

	while(bytes) {
		/* Receive silence, or tone */
		if (ms->curtone) {
			left = ms->curtone->tonesamples - ms->tonep;
			if (left > bytes)
				left = bytes;
			for (x=0;x<left;x++) {
				/* Pick our default value from the next sample of the current tone */
				getlin = dahdi_tone_nextsample(&ms->ts, ms->curtone);
				*(txb++) = DAHDI_LIN2X(getlin, ms);
			}
			ms->tonep+=left;
			bytes -= left;
			if (ms->tonep >= ms->curtone->tonesamples) {
				struct dahdi_tone *last;
				/* Go to the next sample of the tone */
				ms->tonep = 0;
				last = ms->curtone;
				ms->curtone = ms->curtone->next;
				if (!ms->curtone) {
					/* No more tones...  Is this dtmf or mf?  If so, go to the next digit */
					if (ms->dialing)
						__do_dtmf(ms);
				} else {
					if (last != ms->curtone)
						dahdi_init_tone_state(&ms->ts, ms->curtone);
				}
			}
		} else {
			/* Use silence */
			memset(txb, DAHDI_LIN2X(0, ms), bytes);
			bytes = 0;
		}
	}

}

static void __dahdi_receive_chunk(struct dahdi_chan *chan, unsigned char *buf)
{
	/* Receive chunk of audio -- called with chan->lock held */
	unsigned char waste[DAHDI_CHUNKSIZE];

#ifdef	OPTIMIZE_CHANMUTE
	if(likely(chan->chanmute))
		return;
#endif
	if (!buf) {
		memset(waste, DAHDI_LIN2X(0, chan), sizeof(waste));
		buf = waste;
	}
	if ((chan->flags & DAHDI_FLAG_AUDIO) || (chan->confmode)) {
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_begin();
#endif
		__dahdi_process_putaudio_chunk(chan, buf);
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_end();
#endif
	}
	__dahdi_putbuf_chunk(chan, buf);
}

static inline void __dahdi_real_receive(struct dahdi_chan *chan)
{
	/* Called with chan->lock held */
#ifdef	OPTIMIZE_CHANMUTE
	if(likely(chan->chanmute))
		return;
#endif
	if (chan->confmode) {
		/* Load into queue if we have space */
		__buf_push(&chan->confin, chan->readchunk);
	} else {
		__dahdi_receive_chunk(chan, chan->readchunk);
	}
}

/**
 * __transmit_to_slaves() - Distribute the tx data to all the slave channels.
 *
 */
static void __transmit_to_slaves(struct dahdi_chan *const chan)
{
	u_char data[DAHDI_CHUNKSIZE];
	int i;
	int pos = DAHDI_CHUNKSIZE;
	struct dahdi_chan *slave;
	for (i = 0; i < DAHDI_CHUNKSIZE; i++) {
		for (slave = chan; (NULL != slave); slave = slave->nextslave) {
			if (pos == DAHDI_CHUNKSIZE) {
				__dahdi_transmit_chunk(chan, data);
				pos = 0;
			}
			slave->writechunk[i] = data[pos++];
		}
	}
}

int _dahdi_transmit(struct dahdi_span *span)
{
	unsigned int x;

	for (x=0;x<span->channels;x++) {
		struct dahdi_chan *const chan = span->chans[x];
		spin_lock(&chan->lock);
		if (unlikely(chan->flags & DAHDI_FLAG_NOSTDTXRX)) {
			spin_unlock(&chan->lock);
			continue;
		}
		if (chan == chan->master) {
			if (is_chan_dacsed(chan)) {
				struct dahdi_chan *const src = chan->dacs_chan;
				memcpy(chan->writechunk, src->readchunk,
				       DAHDI_CHUNKSIZE);
				if (chan->sig == DAHDI_SIG_DACS_RBS) {
					/* Just set bits for our destination */
					if (chan->txsig != src->rxsig) {
						chan->txsig = src->rxsig;
						span->ops->rbsbits(chan, src->rxsig);
					}
				}
				/* there is no further processing to do for
				 * DACS channels, so jump to the next channel
				 * in the span */
				spin_unlock(&chan->lock);
				continue;
			} else if (chan->nextslave) {
				__transmit_to_slaves(chan);
			} else {
				/* Process a normal channel */
				__dahdi_real_transmit(chan);
			}
			if (chan->otimer) {
				chan->otimer -= DAHDI_CHUNKSIZE;
				if (chan->otimer <= 0)
					__rbs_otimer_expire(chan);
			}
		}
		spin_unlock(&chan->lock);
	}

	if (span->mainttimer) {
		span->mainttimer -= DAHDI_CHUNKSIZE;
		if (span->mainttimer <= 0) {
			span->mainttimer = 0;
			span->maintstat = 0;
		}
	}
	return 0;
}
EXPORT_SYMBOL(_dahdi_transmit);

static inline void __pseudo_rx_audio(struct dahdi_chan *chan)
{
	unsigned char tmp[DAHDI_CHUNKSIZE];
	spin_lock(&chan->lock);
	__dahdi_getempty(chan, tmp);
	__dahdi_receive_chunk(chan, tmp);
	spin_unlock(&chan->lock);
}

#ifdef CONFIG_DAHDI_MIRROR
static inline void pseudo_rx_audio(struct dahdi_chan *chan)
{
	if (!chan->srcmirror)
		__pseudo_rx_audio(chan);
}
#else
static inline void pseudo_rx_audio(struct dahdi_chan *chan)
{
	__pseudo_rx_audio(chan);
}
#endif /* CONFIG_DAHDI_MIRROR */

#ifdef DAHDI_SYNC_TICK
static inline void dahdi_sync_tick(struct dahdi_span *const s)
{
	if (s->ops->sync_tick)
		s->ops->sync_tick(s, dahdi_is_sync_master(s));
}
#else
#define dahdi_sync_tick(x) do { ; } while (0)
#endif

/**
 * _process_masterspan - Handle conferencing and timers.
 *
 * There are three sets of conference sum accumulators. One for the current
 * sample chunk (conf_sums), one for the next sample chunk (conf_sums_next), and
 * one for the previous sample chunk (conf_sums_prev). The following routine
 * (rotate_sums) "rotates" the pointers to these accululator arrays as part
 * of the events of sample chink processing as follows:
 *
 * 1. All (real span) receive chunks are processed (with putbuf). The last one
 * to be processed is the master span. The data received is loaded into the
 * accumulators for the next chunk (conf_sums_next), to be in alignment with
 * current data after rotate_sums() is called (which immediately follows).
 * Keep in mind that putbuf is *also* a transmit routine for the pseudo parts
 * of channels that are in the REALANDPSEUDO conference mode. These channels
 * are processed from data in the current sample chunk (conf_sums), being
 * that this is a "transmit" function (for the pseudo part).
 *
 * 2. rotate_sums() is called.
 *
 * 3. All pseudo channel receive chunks are processed. This data is loaded into
 * the current sample chunk accumulators (conf_sums).
 *
 * 4. All conference links are processed (being that all receive data for this
 * chunk has already been processed by now).
 *
 * 5. All pseudo channel transmit chunks are processed. This data is loaded from
 * the current sample chunk accumulators (conf_sums).
 *
 * 6. All (real span) transmit chunks are processed (with getbuf).  This data is
 * loaded from the current sample chunk accumulators (conf_sums). Keep in mind
 * that getbuf is *also* a receive routine for the pseudo part of channels that
 * are in the REALANDPSEUDO conference mode. These samples are loaded into
 * the next sample chunk accumulators (conf_sums_next) to be processed as part
 * of the next sample chunk's data (next time around the world).
 *
 */
static void _process_masterspan(void)
{
	int x;
	struct pseudo_chan *pseudo;
	struct dahdi_span *s;
	u_char *data;

#ifdef CONFIG_DAHDI_CORE_TIMER
	/* We increment the calls since start here, so that if we switch over
	 * to the core timer, we know how many times we need to call
	 * process_masterspan in order to catch up since this function needs
	 * to be called (1000 / (DAHDI_CHUNKSIZE / 8)) times per second. */
	atomic_inc(&core_timer.count);
#endif
	/* Hold the chan_lock for the duration of major
	   activities which touch all sorts of channels */
	spin_lock(&chan_lock);

	/* Process any timers */
	process_timers();

	list_for_each_entry(s, &span_list, spans_node) {
		for (x = 0; x < s->channels; ++x) {
			struct dahdi_chan *const chan = s->chans[x];
			if (!chan->confmode)
				continue;
			spin_lock(&chan->lock);
			data = __buf_peek(&chan->confin);
			__dahdi_receive_chunk(chan, data);
			if (data)
				__buf_pull(&chan->confin, NULL, chan);
			spin_unlock(&chan->lock);
		}
	}

	/* This is the master channel, so make things switch over */
	rotate_sums();

	/* do all the pseudo and/or conferenced channel receives (getbuf's) */
	list_for_each_entry(pseudo, &pseudo_chans, node) {
		spin_lock(&pseudo->chan.lock);
		__dahdi_transmit_chunk(&pseudo->chan, NULL);
		spin_unlock(&pseudo->chan.lock);
	}

	/* do all the pseudo/conferenced channel transmits (putbuf's) */
	list_for_each_entry(pseudo, &pseudo_chans, node) {
		pseudo_rx_audio(&pseudo->chan);
	}

	list_for_each_entry(s, &span_list, spans_node) {
		for (x = 0; x < s->channels; x++) {
			struct dahdi_chan *const chan = s->chans[x];
			if (!chan->confmode)
				continue;
			spin_lock(&chan->lock);
			data = __buf_pushpeek(&chan->confout);
			__dahdi_transmit_chunk(chan, data);
			if (data)
				__buf_push(&chan->confout, NULL);
			spin_unlock(&chan->lock);
		}

		dahdi_sync_tick(s);
	}
	spin_unlock(&chan_lock);
}

#ifndef CONFIG_DAHDI_CORE_TIMER

static void coretimer_init(void)
{
	return;
}

static void coretimer_cleanup(void)
{
	return;
}

#else

static unsigned long core_diff_ms(struct timespec *t0, struct timespec *t1)
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

static inline unsigned long msecs_processed(const struct core_timer *const ct)
{
	return atomic_read(&ct->count) * DAHDI_MSECS_PER_CHUNK;
}

static void coretimer_func(unsigned long param)
{
	unsigned long flags;
	unsigned long ms_since_start;
	struct timespec now;
	const unsigned long MAX_INTERVAL = 100000L;
	const unsigned long ONESEC_INTERVAL = HZ;
	const long MS_LIMIT = 3000;
	long difference;

	now = current_kernel_time();

	if (atomic_read(&core_timer.count) ==
	    atomic_read(&core_timer.last_count)) {

		/* This is the code path if a board driver is not calling
		 * dahdi_receive, and therefore the core of dahdi needs to
		 * perform the master span processing itself. */

		if (!atomic_read(&core_timer.shutdown)) {
			mod_timer(&core_timer.timer, jiffies +
				  core_timer.interval);
		}

		ms_since_start = core_diff_ms(&core_timer.start_interval, &now);

		/*
		 * If the system time has changed, it is possible for us to be
		 * far behind.  If we are more than MS_LIMIT milliseconds
		 * behind (or ahead in time), just reset our time base and
		 * continue so that we do not hang the system here.
		 *
		 */
		difference = ms_since_start - msecs_processed(&core_timer);
		if (unlikely((difference >  MS_LIMIT) || (difference < 0))) {
			if (printk_ratelimit()) {
				module_printk(KERN_INFO,
					      "Detected time shift.\n");
			}
			atomic_set(&core_timer.count, 0);
			atomic_set(&core_timer.last_count, 0);
			core_timer.start_interval = now;
			return;
		}

		local_irq_save(flags);
		while (ms_since_start > msecs_processed(&core_timer))
			_process_masterspan();
		local_irq_restore(flags);

		if (ms_since_start > MAX_INTERVAL) {
			atomic_set(&core_timer.count, 0);
			atomic_set(&core_timer.last_count, 0);
			core_timer.start_interval = now;
		} else {
			atomic_set(&core_timer.last_count,
				   atomic_read(&core_timer.count));
		}

	} else {

		/* It looks like a board driver is calling dahdi_receive. We
		 * will just check again in a second. */
		atomic_set(&core_timer.count, 0);
		atomic_set(&core_timer.last_count, 0);
		core_timer.start_interval = now;
		if (!atomic_read(&core_timer.shutdown))
			mod_timer(&core_timer.timer, jiffies + ONESEC_INTERVAL);
	}
}

static void coretimer_init(void)
{
	init_timer(&core_timer.timer);
	core_timer.timer.function = coretimer_func;
	core_timer.start_interval = current_kernel_time();
	atomic_set(&core_timer.count, 0);
	atomic_set(&core_timer.shutdown, 0);
	core_timer.interval = max(msecs_to_jiffies(DAHDI_MSECS_PER_CHUNK), 1UL);
	if (core_timer.interval < (HZ/250))
		core_timer.interval = (HZ/250);
	core_timer.timer.expires = jiffies + core_timer.interval;
	add_timer(&core_timer.timer);
}

static void coretimer_cleanup(void)
{
	atomic_set(&core_timer.shutdown, 1);
	del_timer_sync(&core_timer.timer);
}

#endif /* CONFIG_DAHDI_CORE_TIMER */

/**
 * __receive_from_slaves() - Collect the rx data from all the slave channels.
 *
 */
static void __receive_from_slaves(struct dahdi_chan *const chan)
{
	u_char data[DAHDI_CHUNKSIZE];
	int i;
	int pos = 0;
	struct dahdi_chan *slave;

	for (i = 0; i < DAHDI_CHUNKSIZE; ++i) {
		for (slave = chan; (NULL != slave); slave = slave->nextslave) {
			data[pos++] = slave->readchunk[i];
			if (pos == DAHDI_CHUNKSIZE) {
				__dahdi_receive_chunk(chan, data);
				pos = 0;
			}
		}
	}
}

static inline bool should_skip_receive(const struct dahdi_chan *const chan)
{
	return (unlikely(chan->flags & DAHDI_FLAG_NOSTDTXRX) ||
		(chan->master != chan) ||
		is_chan_dacsed(chan));
}

int _dahdi_receive(struct dahdi_span *span)
{
	unsigned int x;

#ifdef CONFIG_DAHDI_WATCHDOG
	span->watchcounter--;
#endif
	for (x = 0; x < span->channels; x++) {
		struct dahdi_chan *const chan = span->chans[x];
		spin_lock(&chan->lock);
		if (should_skip_receive(chan)) {
			spin_unlock(&chan->lock);
			continue;
		}

		if (chan->nextslave) {
			__receive_from_slaves(chan);
		} else {
			/* Process a normal channel */
			__dahdi_real_receive(chan);
		}
		if (chan->itimer) {
			chan->itimer -= DAHDI_CHUNKSIZE;
			if (chan->itimer <= 0)
				rbs_itimer_expire(chan);
		}
		if (chan->ringdebtimer)
			chan->ringdebtimer--;
		if (chan->sig & __DAHDI_SIG_FXS) {
			if (chan->rxhooksig == DAHDI_RXSIG_RING)
				chan->ringtrailer = DAHDI_RINGTRAILER;
			else if (chan->ringtrailer) {
				chan->ringtrailer -= DAHDI_CHUNKSIZE;
				/* See if RING trailer is expired */
				if (!chan->ringtrailer && !chan->ringdebtimer)
					__qevent(chan, DAHDI_EVENT_RINGOFFHOOK);
			}
		}
		if (chan->pulsetimer) {
			chan->pulsetimer--;
			if (chan->pulsetimer <= 0) {
				if (chan->pulsecount) {
					if (chan->pulsecount > 12) {

						module_printk(KERN_NOTICE, "Got pulse digit %d on %s???\n",
					    chan->pulsecount,
						chan->name);
					} else if (chan->pulsecount > 11) {
						__qevent(chan, DAHDI_EVENT_PULSEDIGIT | '#');
					} else if (chan->pulsecount > 10) {
						__qevent(chan, DAHDI_EVENT_PULSEDIGIT | '*');
					} else if (chan->pulsecount > 9) {
						__qevent(chan, DAHDI_EVENT_PULSEDIGIT | '0');
					} else {
						__qevent(chan, DAHDI_EVENT_PULSEDIGIT | ('0' +
							chan->pulsecount));
					}
					chan->pulsecount = 0;
				}
			}
		}
#ifdef BUFFER_DEBUG
		chan->statcount -= DAHDI_CHUNKSIZE;
#endif
		spin_unlock(&chan->lock);
	}

	if (dahdi_is_sync_master(span))
		_process_masterspan();

	return 0;
}
EXPORT_SYMBOL(_dahdi_receive);

MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_DESCRIPTION("DAHDI Telephony Interface");
MODULE_LICENSE("GPL v2");
/* DAHDI now provides timing. If anybody wants dahdi_dummy it's probably
 * for that. So make dahdi provide it for now. This alias may be removed
 * in the future, and users are encouraged not to rely on it. */
MODULE_ALIAS("dahdi_dummy");

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Sets debugging verbosity as a bitfield, to see"\
		" general debugging set this to 1. To see RBS debugging set"\
		" this to 32");
module_param(deftaps, int, 0644);

module_param(max_pseudo_channels, int, 0644);
MODULE_PARM_DESC(max_pseudo_channels, "Maximum number of pseudo channels.");

module_param(hwec_overrides_swec, int, 0644);
MODULE_PARM_DESC(hwec_overrides_swec, "When true, a hardware echo canceller is used instead of configured SWEC.");

module_param(auto_assign_spans, int, 0644);
MODULE_PARM_DESC(auto_assign_spans,
		 "If 1 spans will automatically have their children span and "
		 "channel numbers assigned by the driver. If 0, user space "
		 "will need to assign them via /sys/bus/dahdi_devices.");

static const struct file_operations dahdi_fops = {
	.owner   = THIS_MODULE,
	.open    = dahdi_open,
	.release = dahdi_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl  = dahdi_unlocked_ioctl,
#ifdef HAVE_COMPAT_IOCTL
	.compat_ioctl = dahdi_ioctl_compat,
#endif
#else
	.ioctl   = dahdi_ioctl,
#endif
	.poll    = dahdi_poll,
};

/*
 * DAHDI stability should not depend on the calling process behaviour.
 * In case of suprise device removal, we should be able to return
 * sane results (-ENODEV) even after the underlying device was released.
 *
 * This should be OK even if the calling process (hint, hint Asterisk)
 * ignores the system calls return value.
 *
 * We simply use dummy file_operations to implement this.
 */

/*
 * Common behaviour called from all other nodev_*() file_operations
 */
static int nodev_common(const char msg[])
{
	if (printk_ratelimit()) {
		module_printk(KERN_NOTICE,
			"nodev: %s: process %d still calling\n",
			msg, current->tgid);
	}
	msleep(5);
	return -ENODEV;
}

static ssize_t nodev_chan_read(struct file *file, char __user *usrbuf,
			       size_t count, loff_t *ppos)
{
	return nodev_common("read");
}

static ssize_t nodev_chan_write(struct file *file, const char __user *usrbuf,
				size_t count, loff_t *ppos)
{
	return nodev_common("write");
}

static unsigned int
nodev_chan_poll(struct file *file, struct poll_table_struct *wait_table)
{
	return nodev_common("poll");
}

static long
nodev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	switch (cmd) {
	case DAHDI_GETEVENT:  /* Get event on queue */
		/*
		 * Hint the bugger that the channel is gone for good
		 */
		put_user(DAHDI_EVENT_REMOVED, (int __user *)data);
		break;
	}
	return nodev_common("ioctl");
}

#ifndef HAVE_UNLOCKED_IOCTL
static int nodev_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long data)
{
	return nodev_unlocked_ioctl(file, cmd, data);
}
#endif

#ifdef HAVE_COMPAT_IOCTL
static long nodev_ioctl_compat(struct file *file, unsigned int cmd,
		unsigned long data)
{
	if (cmd == DAHDI_SFCONFIG)
		return -ENOTTY; /* Not supported yet */

	return nodev_unlocked_ioctl(file, cmd, data);
}
#endif

static const struct file_operations nodev_fops = {
	.owner   = THIS_MODULE,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl  = nodev_unlocked_ioctl,
#ifdef HAVE_COMPAT_IOCTL
	.compat_ioctl = nodev_ioctl_compat,
#endif
#else
	.ioctl   = nodev_ioctl,
#endif
	.read    = nodev_chan_read,
	.write   = nodev_chan_write,
	.poll    = nodev_chan_poll,
};

static const struct file_operations dahdi_chan_fops = {
	.owner   = THIS_MODULE,
	.open    = dahdi_open,
	.release = dahdi_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl  = dahdi_unlocked_ioctl,
#ifdef HAVE_COMPAT_IOCTL
	.compat_ioctl = dahdi_ioctl_compat,
#endif
#else
	.ioctl   = dahdi_ioctl,
#endif
	.read    = dahdi_chan_read,
	.write   = dahdi_chan_write,
	.poll    = dahdi_chan_poll,
};

#ifdef CONFIG_DAHDI_WATCHDOG
static struct timer_list watchdogtimer;

static void watchdog_check(unsigned long ignored)
{
	unsigned long flags;
	static int wdcheck=0;
	struct dahdi_span *s;

	spin_lock_irqsave(&chan_lock, flags);
	list_for_each_entry(s, &span_list, spans_node) {
		if (s->flags & DAHDI_FLAG_RUNNING) {
			if (s->watchcounter == DAHDI_WATCHDOG_INIT) {
				/* Whoops, dead card */
				if ((s->watchstate == DAHDI_WATCHSTATE_OK) ||
					(s->watchstate == DAHDI_WATCHSTATE_UNKNOWN)) {
					s->watchstate = DAHDI_WATCHSTATE_RECOVERING;
					if (s->ops->watchdog) {
						module_printk(KERN_NOTICE, "Kicking span %s\n", s->name);
						s->ops->watchdog(s, DAHDI_WATCHDOG_NOINTS);
					} else {
						module_printk(KERN_NOTICE, "Span %s is dead with no revival\n", s->name);
						s->watchstate = DAHDI_WATCHSTATE_FAILED;
					}
				}
			} else {
				if ((s->watchstate != DAHDI_WATCHSTATE_OK) &&
					(s->watchstate != DAHDI_WATCHSTATE_UNKNOWN))
						module_printk(KERN_NOTICE, "Span %s is alive!\n", s->name);
				s->watchstate = DAHDI_WATCHSTATE_OK;
			}
			s->watchcounter = DAHDI_WATCHDOG_INIT;
		}
	}
	spin_unlock_irqrestore(&chan_lock, flags);
	if (!wdcheck) {
		module_printk(KERN_NOTICE, "watchdog on duty!\n");
		wdcheck=1;
	}
	mod_timer(&watchdogtimer, jiffies + 2);
}

static int __init watchdog_init(void)
{
	init_timer(&watchdogtimer);
	watchdogtimer.expires = 0;
	watchdogtimer.data =0;
	watchdogtimer.function = watchdog_check;
	/* Run every couple of jiffy or so */
	mod_timer(&watchdogtimer, jiffies + 2);
	return 0;
}

static void __exit watchdog_cleanup(void)
{
	del_timer(&watchdogtimer);
}

#endif

static int __init dahdi_init(void)
{
	int res = 0;

#ifdef CONFIG_PROC_FS
	root_proc_entry = proc_mkdir("dahdi", NULL);
	if (!root_proc_entry) {
		dahdi_err("dahdi init: Failed creating /proc/dahdi\n");
		return -EEXIST;
	}
#endif
	res = dahdi_sysfs_init(&dahdi_fops);
	if (res)
		goto failed_driver_init;

	dahdi_conv_init();
	fasthdlc_precalc();
	rotate_sums();
#ifdef CONFIG_DAHDI_WATCHDOG
	watchdog_init();
#endif
	coretimer_init();

	res = dahdi_register_echocan_factory(&hwec_factory);
	if (res) {
		WARN_ON(1);
		goto failed_register_ec_factory;
	}

	return 0;

failed_register_ec_factory:
	coretimer_cleanup();
	dahdi_sysfs_exit();
failed_driver_init:
	if (root_proc_entry) {
		remove_proc_entry("dahdi", NULL);
		root_proc_entry = NULL;
	}
	return res;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
#ifdef CONFIG_PCI
void dahdi_pci_disable_link_state(struct pci_dev *pdev, int state)
{
	u16 reg16;
	int pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	state &= (PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
		  PCIE_LINK_STATE_CLKPM);
	if (!pos)
		return;
	pci_read_config_word(pdev, pos + PCI_EXP_LNKCTL, &reg16);
	reg16 &= ~(state);
	pci_write_config_word(pdev, pos + PCI_EXP_LNKCTL, reg16);
}
EXPORT_SYMBOL(dahdi_pci_disable_link_state);
#endif /* CONFIG_PCI */
#endif /* 2.6.25 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
static inline void flush_find_master_work(void)
{
	flush_scheduled_work();
}
#else
static inline void flush_find_master_work(void)
{
	cancel_work_sync(&find_master_work);
}
#endif

static void __exit dahdi_cleanup(void)
{
	struct dahdi_zone *z;

	dahdi_unregister_echocan_factory(&hwec_factory);
	coretimer_cleanup();
	dahdi_sysfs_exit();

#ifdef CONFIG_PROC_FS
	if (root_proc_entry) {
		remove_proc_entry("dahdi", NULL);
		root_proc_entry = NULL;
	}
#endif

	module_printk(KERN_INFO, "Telephony Interface Unloaded\n");

	spin_lock(&zone_lock);
	while (!list_empty(&tone_zones)) {
		z = list_entry(tone_zones.next, struct dahdi_zone, node);
		list_del(&z->node);
		if (!tone_zone_put(z)) {
			module_printk(KERN_WARNING,
				      "Potential memory leak detected in %s\n",
				      __func__);
		}
	}
	spin_unlock(&zone_lock);

#ifdef CONFIG_DAHDI_WATCHDOG
	watchdog_cleanup();
#endif
	flush_find_master_work();
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
char *dahdi_kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;
	char *temp;
	unsigned int len;

	temp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!temp)
		return NULL;

	va_start(ap, fmt);
	len = vsnprintf(temp, PAGE_SIZE, fmt, ap);
	va_end(ap);

	p = kzalloc(len + 1, gfp);
	if (!p) {
		kfree(temp);
		return NULL;
	}

	memcpy(p, temp, len + 1);
	kfree(temp);
	return p;
}
EXPORT_SYMBOL(dahdi_kasprintf);
#endif

module_init(dahdi_init);
module_exit(dahdi_cleanup);
