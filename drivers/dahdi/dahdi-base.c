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
 * Copyright (C) 2001 - 2010 Digium, Inc.
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
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/kmod.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/delay.h>

#ifdef HAVE_UNLOCKED_IOCTL
#include <linux/smp_lock.h>
#endif

#include <linux/ppp_defs.h>

#include <asm/atomic.h>

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)

#include <dahdi/version.h>
/* Grab fasthdlc with tables */
#define FAST_HDLC_NEED_TABLES
#include <dahdi/kernel.h>
#include "ecdis.h"

#ifndef CONFIG_OLD_HDLC_API
#define NEW_HDLC_INTERFACE
#endif

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

/* Get helper arithmetic */
#include "arith.h"
#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
#include <asm/i387.h>
#endif

#define hdlc_to_ztchan(h) (((struct dahdi_hdlc *)(h))->chan)
#define dev_to_ztchan(h) (((struct dahdi_hdlc *)(dev_to_hdlc(h)->priv))->chan)
#define ztchan_to_dev(h) ((h)->hdlcnetdev->netdev)

/* macro-oni for determining a unit (channel) number */
#define	UNIT(file) MINOR(file->f_dentry->d_inode->i_rdev)

/* names of tx level settings */
static char *dahdi_txlevelnames[] = {
"0 db (CSU)/0-133 feet (DSX-1)",
"133-266 feet (DSX-1)",
"266-399 feet (DSX-1)",
"399-533 feet (DSX-1)",
"533-655 feet (DSX-1)",
"-7.5db (CSU)",
"-15db (CSU)",
"-22.5db (CSU)"
} ;

EXPORT_SYMBOL(dahdi_transcode_fops);
EXPORT_SYMBOL(dahdi_init_tone_state);
EXPORT_SYMBOL(dahdi_mf_tone);
EXPORT_SYMBOL(dahdi_register);
EXPORT_SYMBOL(dahdi_unregister);
EXPORT_SYMBOL(__dahdi_mulaw);
EXPORT_SYMBOL(__dahdi_alaw);
#ifdef CONFIG_CALC_XLAW
EXPORT_SYMBOL(__dahdi_lineartoulaw);
EXPORT_SYMBOL(__dahdi_lineartoalaw);
#else
EXPORT_SYMBOL(__dahdi_lin2mu);
EXPORT_SYMBOL(__dahdi_lin2a);
#endif
EXPORT_SYMBOL(dahdi_lboname);
EXPORT_SYMBOL(dahdi_transmit);
EXPORT_SYMBOL(dahdi_receive);
EXPORT_SYMBOL(dahdi_rbsbits);
EXPORT_SYMBOL(dahdi_qevent_nolock);
EXPORT_SYMBOL(dahdi_qevent_lock);
EXPORT_SYMBOL(dahdi_hooksig);
EXPORT_SYMBOL(dahdi_alarm_notify);
EXPORT_SYMBOL(dahdi_set_dynamic_ioctl);
EXPORT_SYMBOL(dahdi_ec_chunk);
EXPORT_SYMBOL(dahdi_ec_span);
EXPORT_SYMBOL(dahdi_hdlc_abort);
EXPORT_SYMBOL(dahdi_hdlc_finish);
EXPORT_SYMBOL(dahdi_hdlc_getbuf);
EXPORT_SYMBOL(dahdi_hdlc_putbuf);
EXPORT_SYMBOL(dahdi_alarm_channel);
EXPORT_SYMBOL(dahdi_register_chardev);
EXPORT_SYMBOL(dahdi_unregister_chardev);

EXPORT_SYMBOL(dahdi_register_echocan_factory);
EXPORT_SYMBOL(dahdi_unregister_echocan_factory);

EXPORT_SYMBOL(dahdi_set_hpec_ioctl);

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *root_proc_entry;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, NULL, "%s", name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_device_create(class, NULL, devt, device, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_device_create(class, devt, device, name)
#else
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_simple_device_add(class, devt, device, name)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_DESTROY(class, devt) \
	device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define CLASS_DEV_DESTROY(class, devt) \
	class_device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(devt)
#else
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(class, devt)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
static struct class *dahdi_class = NULL;
#else
static struct class_simple *dahdi_class = NULL;
#define class_create class_simple_create
#define class_destroy class_simple_destroy
#endif

static int deftaps = 64;

static int debug;

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
static struct file_operations dahdi_fops;
struct file_operations *dahdi_transcode_fops = NULL;

static struct {
	int	src;	/* source conf number */
	int	dst;	/* dst conf number */
} conf_links[DAHDI_MAX_CONF + 1];

#ifdef CONFIG_DAHDI_CORE_TIMER

static struct core_timer {
	struct timer_list timer;
	struct timespec start_interval;
	atomic_t count;
	atomic_t shutdown;
	atomic_t last_count;
} core_timer;

#endif /* CONFIG_DAHDI_CORE_TIMER */



/* There are three sets of conference sum accumulators. One for the current
sample chunk (conf_sums), one for the next sample chunk (conf_sums_next), and
one for the previous sample chunk (conf_sums_prev). The following routine
(rotate_sums) "rotates" the pointers to these accululator arrays as part
of the events of sample chink processing as follows:

The following sequence is designed to be looked at from the reference point
of the receive routine of the master span.

1. All (real span) receive chunks are processed (with putbuf). The last one
to be processed is the master span. The data received is loaded into the
accumulators for the next chunk (conf_sums_next), to be in alignment with
current data after rotate_sums() is called (which immediately follows).
Keep in mind that putbuf is *also* a transmit routine for the pseudo parts
of channels that are in the REALANDPSEUDO conference mode. These channels
are processed from data in the current sample chunk (conf_sums), being
that this is a "transmit" function (for the pseudo part).

2. rotate_sums() is called.

3. All pseudo channel receive chunks are processed. This data is loaded into
the current sample chunk accumulators (conf_sums).

4. All conference links are processed (being that all receive data for this
chunk has already been processed by now).

5. All pseudo channel transmit chunks are processed. This data is loaded from
the current sample chunk accumulators (conf_sums).

6. All (real span) transmit chunks are processed (with getbuf).  This data is
loaded from the current sample chunk accumulators (conf_sums). Keep in mind
that getbuf is *also* a receive routine for the pseudo part of channels that
are in the REALANDPSEUDO conference mode. These samples are loaded into
the next sample chunk accumulators (conf_sums_next) to be processed as part
of the next sample chunk's data (next time around the world).

*/

enum dahdi_digit_mode {
	DIGIT_MODE_DTMF,
	DIGIT_MODE_MFR1,
	DIGIT_MODE_PULSE,
	DIGIT_MODE_MFR2_FWD,
	DIGIT_MODE_MFR2_REV,
};

#include "digits.h"

static struct dahdi_dialparams global_dialparams = {
	.dtmf_tonelen = DEFAULT_DTMF_LENGTH,
	.mfv1_tonelen = DEFAULT_MFR1_LENGTH,
	.mfr2_tonelen = DEFAULT_MFR2_LENGTH,
};

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

static LIST_HEAD(zaptimers);

#ifdef DEFINE_SPINLOCK
static DEFINE_SPINLOCK(zaptimerlock);
static DEFINE_SPINLOCK(bigzaplock);
#else
static spinlock_t zaptimerlock = SPIN_LOCK_UNLOCKED;
static spinlock_t bigzaplock = SPIN_LOCK_UNLOCKED;
#endif

struct dahdi_zone {
	atomic_t refcount;
	char name[40];	/* Informational, only */
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
};

#ifdef DEFINE_RWLOCK
static DEFINE_RWLOCK(zone_lock);
static DEFINE_RWLOCK(chan_lock);
#else
static rwlock_t zone_lock = RW_LOCK_UNLOCKED;
static rwlock_t chan_lock = RW_LOCK_UNLOCKED;
#endif

static bool valid_channo(const int channo)
{
	return ((channo >= DAHDI_MAX_CHANNELS) || (channo < 1)) ?
			false : true;
}

#define VALID_CHANNEL(j) do { \
	if (!valid_channo(j)) \
		return -EINVAL; \
	if (!chans[j]) \
		return -ENXIO; \
} while (0)

/* Protected by chan_lock. */
static struct dahdi_span *spans[DAHDI_MAX_SPANS];
static struct dahdi_chan *chans[DAHDI_MAX_CHANNELS];

static int maxspans = 0;

static struct dahdi_chan *chan_from_file(struct file *file)
{
	return (file->private_data) ? file->private_data :
			((valid_channo(UNIT(file))) ? chans[UNIT(file)] : NULL);
}

static struct dahdi_span *find_span(int spanno)
{
	return (spanno > 0 && spanno < DAHDI_MAX_SPANS) ? spans[spanno] : NULL;
}

static inline unsigned int span_count(void)
{
	return maxspans;
}

static int maxchans = 0;
static int maxconfs = 0;
static int maxlinks = 0;

static int default_zone = -1;

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

static struct dahdi_zone *tone_zones[DAHDI_TONE_ZONE_MAX];

#define NUM_SIGS	10

#ifdef DEFINE_RWLOCK
static DEFINE_RWLOCK(ecfactory_list_lock);
#else
static rwlock_t ecfactory_list_lock = __RW_LOCK_UNLOCKED();
#endif

static LIST_HEAD(ecfactory_list);

struct ecfactory {
	const struct dahdi_echocan_factory *ec;
	struct list_head list;
};

int dahdi_register_echocan_factory(const struct dahdi_echocan_factory *ec)
{
	struct ecfactory *cur;

	WARN_ON(!ec->owner);

	write_lock(&ecfactory_list_lock);

	/* make sure it isn't already registered */
	list_for_each_entry(cur, &ecfactory_list, list) {
		if (cur->ec == ec) {
			write_unlock(&ecfactory_list_lock);
			return -EPERM;
		}
	}

	if (!(cur = kzalloc(sizeof(*cur), GFP_KERNEL))) {
		write_unlock(&ecfactory_list_lock);
		return -ENOMEM;
	}

	cur->ec = ec;
	INIT_LIST_HEAD(&cur->list);

	list_add_tail(&cur->list, &ecfactory_list);

	write_unlock(&ecfactory_list_lock);

	return 0;
}

void dahdi_unregister_echocan_factory(const struct dahdi_echocan_factory *ec)
{
	struct ecfactory *cur, *next;

	write_lock(&ecfactory_list_lock);

	list_for_each_entry_safe(cur, next, &ecfactory_list, list) {
		if (cur->ec == ec) {
			list_del(&cur->list);
			break;
		}
	}

	write_unlock(&ecfactory_list_lock);
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

static int dahdi_chan_echocan_create(struct dahdi_chan *chan,
				     struct dahdi_echocanparams *ecp,
				     struct dahdi_echocanparam *p,
				     struct dahdi_echocan_state **ec)
{
	if (chan->span && chan->span->ops->echocan_create)
		return chan->span->ops->echocan_create(chan, ecp, p, ec);
	else
		return -ENODEV;
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

	if (len > 0)
		buf[--len] = '\0';	/* strip last space */

	return len;
}

static int dahdi_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int x, len = 0, real_count;
	long spanno;
	struct dahdi_span *s;


	/* In Linux 2.6, page is always PROC_BLOCK_SIZE=(PAGE_SIZE-1024) bytes.
	 * 0<count<=PROC_BLOCK_SIZE . count=1 will produce an error in
	 * vsnprintf ('head -c 1 /proc/dahdi/1', 'dd bs=1').
	 * An ugly hack. Good way: seq_printf (seq_file.c). */
        real_count = count;
	count = PAGE_SIZE-1024;
	spanno = (long)data;
	if (!spanno)
		return 0;

	s = find_span(spanno);
	if (!s)
		return -ENODEV;

	if (s->name)
		len += snprintf(page + len, count - len, "Span %d: %s ",
				s->spanno, s->name);
	if (s->desc)
		len += snprintf(page + len, count - len, "\"%s\"",
				s->desc);
	else
		len += snprintf(page + len, count - len, "\"\"");

	if (s == master)
		len += snprintf(page + len, count - len, " (MASTER)");

	if (s->lineconfig) {
		/* framing first */
		if (s->lineconfig & DAHDI_CONFIG_B8ZS)
			len += snprintf(page + len, count - len, " B8ZS/");
		else if (s->lineconfig & DAHDI_CONFIG_AMI)
			len += snprintf(page + len, count - len, " AMI/");
		else if (s->lineconfig & DAHDI_CONFIG_HDB3)
			len += snprintf(page + len, count - len, " HDB3/");
		/* then coding */
		if (s->lineconfig & DAHDI_CONFIG_ESF)
			len += snprintf(page + len, count - len, "ESF");
		else if (s->lineconfig & DAHDI_CONFIG_D4)
			len += snprintf(page + len, count - len, "D4");
		else if (s->lineconfig & DAHDI_CONFIG_CCS)
			len += snprintf(page + len, count - len, "CCS");
		/* E1's can enable CRC checking */
		if (s->lineconfig & DAHDI_CONFIG_CRC4)
			len += snprintf(page + len, count - len, "/CRC4");
	}

	len += snprintf(page + len, count - len, " ");

	/* list alarms */
	len += fill_alarm_string(page + len, count - len, s->alarms);
	if (s->syncsrc &&
		(s->syncsrc == s->spanno))
		len += snprintf(page + len, count - len, "ClockSource ");
	len += snprintf(page + len, count - len, "\n");
	if (s->count.bpv)
		len += snprintf(page + len, count - len, "\tBPV count: %d\n",
				s->count.bpv);
	if (s->count.crc4)
		len += snprintf(page + len, count - len,
				"\tCRC4 error count: %d\n",
				s->count.crc4);
	if (s->count.ebit)
		len += snprintf(page + len, count - len,
				"\tE-bit error count: %d\n",
				s->count.ebit);
	if (s->count.fas)
		len += snprintf(page + len, count - len,
				"\tFAS error count: %d\n",
				s->count.fas);
	if (s->irqmisses)
		len += snprintf(page + len, count - len,
				"\tIRQ misses: %d\n",
				s->irqmisses);
	if (s->timingslips)
		len += snprintf(page + len, count - len,
				"\tTiming slips: %d\n",
				s->timingslips);
	len += snprintf(page + len, count - len, "\n");

	for (x = 0; x < s->channels; x++) {
		struct dahdi_chan *chan = s->chans[x];

		if (chan->name)
			len += snprintf(page + len, count - len,
					"\t%4d %s ", chan->channo, chan->name);

		if (chan->sig) {
			if (chan->sig == DAHDI_SIG_SLAVE)
				len += snprintf(page+len, count-len, "%s ",
						sigstr(chan->master->sig));
			else {
				len += snprintf(page+len, count-len, "%s ",
						sigstr(chan->sig));
				if (chan->nextslave &&
					(chan->master->channo == chan->channo))
					len += snprintf(page+len, count-len,
							"Master ");
			}
		} else if (!chan->sigcap) {
			len += snprintf(page+len, count-len, "Reserved ");
		}

		if (test_bit(DAHDI_FLAGBIT_OPEN, &chan->flags))
			len += snprintf(page + len, count - len, "(In use) ");

#ifdef	OPTIMIZE_CHANMUTE
		if (chan->chanmute)
			len += snprintf(page+len, count-len, "(no pcm) ");
#endif

		len += fill_alarm_string(page+len, count-len,
				chan->chan_alarms);

		if (chan->ec_factory)
			len += snprintf(page+len, count-len, "(SWEC: %s) ",
					chan->ec_factory->name);

		if (chan->ec_state)
			len += snprintf(page+len, count-len, "(EC: %s) ",
					chan->ec_state->ops->name);

		len += snprintf(page+len, count-len, "\n");

		/* If everything printed so far is before beginning 
		 * of request */
		if (len <= off) {
			off -= len;
			len = 0;
		}

		/* stop if we've already generated enough */
		if (len > off + count)
			break;
		/* stop if we're NEAR danger limit. let it be -128 bytes. */
		if (len > count-128)
			break;
	}
	count = real_count;
	/* If everything printed so far is before beginning of request */
	if (len <= off) {
		off = 0;
		len = 0;
	}
	*start = page + off;
	len -= off;		/* un-count any remaining offset */
	*eof = 1;
	if (len > count)
		len = count;	/* don't return bytes not asked for */
	return len;
}
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

static void recalc_maxlinks(void)
{
	int x;

	for (x = DAHDI_MAX_CONF - 1; x > 0; x--) {
		if (conf_links[x].src || conf_links[x].dst) {
			maxlinks = x + 1;
			return;
		}
	}

	maxlinks = 0;
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

static void dahdi_check_conf(int x)
{
	int y;

	/* return if no valid conf number */
	if (x <= 0)
		return;

	/* Return if there is no alias */
	if (!confalias[x])
		return;

	for (y = 0; y < maxchans; y++) {
		struct dahdi_chan *const chan = chans[y];
		const int confmode = chan->confmode & DAHDI_CONF_MODE_MASK;
		if (chan && (chan->confna == x) &&
		    (confmode == DAHDI_CONF_CONF ||
		     confmode == DAHDI_CONF_CONFANN ||
		     confmode == DAHDI_CONF_CONFMON ||
		     confmode == DAHDI_CONF_CONFANNMON ||
		     confmode == DAHDI_CONF_REALANDPSEUDO)) {
			return;
		}
	}

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
	if (chan->iomask & DAHDI_IOMUX_SIGEVENT)
		wake_up_interruptible(&chan->eventbufq);

	wake_up_interruptible(&chan->readbufq);
	wake_up_interruptible(&chan->writebufq);
	wake_up_interruptible(&chan->sel);

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

/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
static int schluffen(wait_queue_head_t *q)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(q, &wait);
	current->state = TASK_INTERRUPTIBLE;

	if (!signal_pending(current))
		schedule();

	current->state = TASK_RUNNING;
	remove_wait_queue(q, &wait);

	if (signal_pending(current))
		return -ERESTARTSYS;

	return 0;
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
static int __buf_push(struct confq *q, u_char *data)
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
	read_lock(&ecfactory_list_lock);

	list_for_each_entry(cur, &ecfactory_list, list) {
		if (!strcmp(name_upper, cur->ec->name)) {
			if (try_module_get(cur->ec->owner)) {
				read_unlock(&ecfactory_list_lock);
				kfree(name_upper);
				return cur->ec;
			} else {
				read_unlock(&ecfactory_list_lock);
				kfree(name_upper);
				return NULL;
			}
		}
	}

	read_unlock(&ecfactory_list_lock);

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

/* 
 * close_channel - close the channel, resetting any channel variables
 * @chan: the dahdi_chan to close
 *
 * This function might be called before the channel is placed on the global
 * array of channels, (chans), and therefore, neither this function nor it's
 * children should depend on the dahdi_chan.channo member which is not set yet.
 */
static void close_channel(struct dahdi_chan *chan)
{
	unsigned long flags;
	void *rxgain = NULL;
	struct dahdi_echocan_state *ec_state;
	const struct dahdi_echocan_factory *ec_current;
	int oldconf;
	short *readchunkpreec;
#ifdef CONFIG_DAHDI_PPP
	struct ppp_channel *ppp;
#endif

	might_sleep();

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
	if (chan->curzone)
		atomic_dec(&chan->curzone->refcount);
	chan->curzone = NULL;
	chan->cadencepos = 0;
	chan->pdialcount = 0;
	dahdi_hangup(chan);
	chan->itimerset = chan->itimer = 0;
	chan->pulsecount = 0;
	chan->pulsetimer = 0;
	chan->ringdebtimer = 0;
	init_waitqueue_head(&chan->sel);
	init_waitqueue_head(&chan->readbufq);
	init_waitqueue_head(&chan->writebufq);
	init_waitqueue_head(&chan->eventbufq);
	init_waitqueue_head(&chan->txstateq);
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
	if ((chan->sig & __DAHDI_SIG_DACS) != __DAHDI_SIG_DACS) {
		chan->confna = 0;
		chan->confmode = 0;
	}
	chan->confmute = 0;
	/* release conference resource, if any to release */
	if (oldconf) dahdi_check_conf(oldconf);
	chan->gotgs = 0;
	reset_conf(chan);

	if (chan->gainalloc && chan->rxgain)
		rxgain = chan->rxgain;

	chan->rxgain = defgain;
	chan->txgain = defgain;
	chan->gainalloc = 0;
	chan->eventinidx = chan->eventoutidx = 0;
	chan->flags &= ~(DAHDI_FLAG_LOOPED | DAHDI_FLAG_LINEAR | DAHDI_FLAG_PPP | DAHDI_FLAG_SIGFREEZE);

	dahdi_set_law(chan, DAHDI_LAW_DEFAULT);

	memset(chan->conflast, 0, sizeof(chan->conflast));
	memset(chan->conflast1, 0, sizeof(chan->conflast1));
	memset(chan->conflast2, 0, sizeof(chan->conflast2));

	if (chan->span && oldconf)
		dahdi_disable_dacs(chan);

	if (ec_state) {
		ec_state->ops->echocan_free(chan, ec_state);
		release_echocan(ec_current);
	}

	spin_unlock_irqrestore(&chan->lock, flags);

	if (rxgain)
		kfree(rxgain);
	if (readchunkpreec)
		kfree(readchunkpreec);

#ifdef CONFIG_DAHDI_PPP
	if (ppp) {
		tasklet_kill(&chan->ppp_calls);
		skb_queue_purge(&chan->ppp_rq);
		ppp_unregister_channel(ppp);
		kfree(ppp);
	}
#endif

}

static int free_tone_zone(int num)
{
	struct dahdi_zone *z = NULL;
	int res = 0;

	if ((num >= DAHDI_TONE_ZONE_MAX) || (num < 0))
		return -EINVAL;

	write_lock(&zone_lock);
	if (tone_zones[num]) {
		if (!atomic_read(&tone_zones[num]->refcount)) {
			z = tone_zones[num];
			tone_zones[num] = NULL;
		} else {
			res = -EBUSY;
		}
	}
	write_unlock(&zone_lock);

	if (z)
		kfree(z);

	return res;
}

static int dahdi_register_tone_zone(int num, struct dahdi_zone *zone)
{
	int res = 0;

	if ((num >= DAHDI_TONE_ZONE_MAX) || (num < 0))
		return -EINVAL;

	write_lock(&zone_lock);
	if (tone_zones[num]) {
		res = -EINVAL;
	} else {
		res = 0;
		tone_zones[num] = zone;
	}
	write_unlock(&zone_lock);

	if (!res)
		module_printk(KERN_INFO, "Registered tone zone %d (%s)\n", num, zone->name);

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
	struct dahdi_zone *z;

	/* Do not call with the channel locked. */

	if (zone == -1)
		zone = default_zone;

	if ((zone >= DAHDI_TONE_ZONE_MAX) || (zone < 0))
		return -EINVAL;

	read_lock(&zone_lock);

	if ((z = tone_zones[zone])) {
		unsigned long flags;

		spin_lock_irqsave(&chan->lock, flags);

		if (chan->curzone)
			atomic_dec(&chan->curzone->refcount);

		atomic_inc(&z->refcount);
		chan->curzone = z;
		chan->tonezone = zone;
		memcpy(chan->ringcadence, z->ringcadence, sizeof(chan->ringcadence));

		spin_unlock_irqrestore(&chan->lock, flags);
	} else {
		res = -ENODATA;
	}

	read_unlock(&zone_lock);

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

static int dahdi_chan_reg(struct dahdi_chan *chan)
{
	int x;
	unsigned long flags;

	might_sleep();

	spin_lock_init(&chan->lock);
	if (!chan->master)
		chan->master = chan;
	if (!chan->readchunk)
		chan->readchunk = chan->sreadchunk;
	if (!chan->writechunk)
		chan->writechunk = chan->swritechunk;
	dahdi_set_law(chan, DAHDI_LAW_DEFAULT);
	close_channel(chan);

	write_lock_irqsave(&chan_lock, flags);
	for (x = 1; x < DAHDI_MAX_CHANNELS; x++) {
		if (chans[x])
			continue;

		chans[x] = chan;
		if (maxchans < x + 1)
			maxchans = x + 1;
		chan->channo = x;
		/* set this AFTER running close_channel() so that
		   HDLC channels wont cause hangage */
		set_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags);
		break;
	}
	write_unlock_irqrestore(&chan_lock, flags);
	if (DAHDI_MAX_CHANNELS == x) {
		module_printk(KERN_ERR, "No more channels available\n");
		return -ENOMEM;
	}

	return 0;
}

char *dahdi_lboname(int x)
{
	if ((x < 0) || (x > 7))
		return "Unknown";
	return dahdi_txlevelnames[x];
}

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
static inline struct net_device_stats *hdlc_stats(struct net_device *dev)
{
	return &dev->stats;
}
#endif

#ifdef NEW_HDLC_INTERFACE
static int dahdi_net_open(struct net_device *dev)
{
	int res = hdlc_open(dev);
	struct dahdi_chan *ms = dev_to_ztchan(dev);

/*	if (!dev->hard_start_xmit) return res; is this really necessary? --byg */
	if (res) /* this is necessary to avoid kernel panic when UNSPEC link encap, proven --byg */
		return res;
#else
static int dahdi_net_open(hdlc_device *hdlc)
{
	struct dahdi_chan *ms = hdlc_to_ztchan(hdlc);
	int res;
#endif
	if (!ms) {
		module_printk(KERN_NOTICE, "dahdi_net_open: nothing??\n");
		return -EINVAL;
	}
	if (test_bit(DAHDI_FLAGBIT_OPEN, &ms->flags)) {
		module_printk(KERN_NOTICE, "%s is already open!\n", ms->name);
		return -EBUSY;
	}
	if (!(ms->flags & DAHDI_FLAG_NETDEV)) {
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

	netif_start_queue(ztchan_to_dev(ms));

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

#ifdef NEW_HDLC_INTERFACE
static int dahdi_net_stop(struct net_device *dev)
{
    hdlc_device *h = dev_to_hdlc(dev);
    struct dahdi_hdlc *hdlc = h->priv;

#else
static void dahdi_net_close(hdlc_device *hdlc)
{
#endif
	struct dahdi_chan *ms = hdlc_to_ztchan(hdlc);
	if (!ms) {
#ifdef NEW_HDLC_INTERFACE
		module_printk(KERN_NOTICE, "dahdi_net_stop: nothing??\n");
		return 0;
#else
		module_printk(KERN_NOTICE, "dahdi_net_close: nothing??\n");
		return;
#endif
	}
	if (!(ms->flags & DAHDI_FLAG_NETDEV)) {
#ifdef NEW_HDLC_INTERFACE
		module_printk(KERN_NOTICE, "dahdi_net_stop: %s is not a net device!\n", ms->name);
		return 0;
#else
		module_printk(KERN_NOTICE, "dahdi_net_close: %s is not a net device!\n", ms->name);
		return;
#endif
	}
	/* Not much to do here.  Just deallocate the buffers */
        netif_stop_queue(ztchan_to_dev(ms));
	dahdi_reallocbufs(ms, 0, 0);
	hdlc_close(dev);
#ifdef NEW_HDLC_INTERFACE
	return 0;
#else
	return;
#endif
}

#ifdef NEW_HDLC_INTERFACE
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
#endif

static struct dahdi_hdlc *dahdi_hdlc_alloc(void)
{
	return kzalloc(sizeof(struct dahdi_hdlc), GFP_KERNEL);
}

#ifdef NEW_HDLC_INTERFACE
static int dahdi_xmit(struct sk_buff *skb, struct net_device *dev)
{
	/* FIXME: this construction seems to be not very optimal for me but I could find nothing better at the moment (Friday, 10PM :( )  --byg */
/*	struct dahdi_chan *ss = hdlc_to_ztchan(list_entry(dev, struct dahdi_hdlc, netdev.netdev));*/
	struct dahdi_chan *ss = dev_to_ztchan(dev);
	struct net_device_stats *stats = hdlc_stats(dev);

#else
static int dahdi_xmit(hdlc_device *hdlc, struct sk_buff *skb)
{
	struct dahdi_chan *ss = hdlc_to_ztchan(hdlc);
	struct net_device *dev = &ss->hdlcnetdev->netdev.netdev;
	struct net_device_stats *stats = &ss->hdlcnetdev->netdev.stats;
#endif
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

		    netif_stop_queue(ztchan_to_dev(ss));
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

#ifdef NEW_HDLC_INTERFACE
static int dahdi_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	return hdlc_ioctl(dev, ifr, cmd);
}
#else
static int dahdi_net_ioctl(hdlc_device *hdlc, struct ifreq *ifr, int cmd)
{
	return -EIO;
}
#endif

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


static void dahdi_chan_unreg(struct dahdi_chan *chan)
{
	int x;
	unsigned long flags;

	might_sleep();

	/* In the case of surprise removal of hardware, make sure any open
	 * file handles to this channel are disassociated with the actual
	 * dahdi_chan. */
	if (chan->file) {
		chan->file->private_data = NULL;
		if (chan->span)
			module_put(chan->span->ops->owner);
	}

	release_echocan(chan->ec_factory);

#ifdef CONFIG_DAHDI_NET
	if (chan->flags & DAHDI_FLAG_NETDEV) {
		unregister_hdlc_device(chan->hdlcnetdev->netdev);
		free_netdev(chan->hdlcnetdev->netdev);
		kfree(chan->hdlcnetdev);
		chan->hdlcnetdev = NULL;
	}
#endif
	write_lock_irqsave(&chan_lock, flags);
	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags)) {
		chans[chan->channo] = NULL;
		clear_bit(DAHDI_FLAGBIT_REGISTERED, &chan->flags);
	}
#ifdef CONFIG_DAHDI_PPP
	if (chan->ppp) {
		module_printk(KERN_NOTICE, "HUH???  PPP still attached??\n");
	}
#endif
	maxchans = 0;
	for (x = 1; x < DAHDI_MAX_CHANNELS; x++) {
		struct dahdi_chan *const pos = chans[x];
		if (!pos)
			continue;
		maxchans = x + 1;
		/* Remove anyone pointing to us as master
		   and make them their own thing */
		if (pos->master == chan)
			pos->master = pos;

		if ((pos->confna == chan->channo) &&
		    is_monitor_mode(pos->confmode) &&
		    ((pos->confmode & DAHDI_CONF_MODE_MASK) ==
		      DAHDI_CONF_DIGITALMON)) {
			/* Take them out of conference with us */
			/* release conference resource if any */
			if (pos->confna) {
				dahdi_check_conf(pos->confna);
				if (pos->span)
					dahdi_disable_dacs(pos);
			}
			pos->confna = 0;
			pos->_confn = 0;
			pos->confmode = 0;
		}
	}
	chan->channo = -1;
	write_unlock_irqrestore(&chan_lock, flags);
}

static ssize_t dahdi_chan_read(struct file *file, char __user *usrbuf,
			       size_t count)
{
	struct dahdi_chan *chan = file->private_data;
	int amnt;
	int res, rv;
	int oldbuf,x;
	unsigned long flags;

	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;

	if (unlikely(!chan)) {
		/* We would typically be here because of surprise hardware
		 * removal or driver unbinding while a user space application
		 * has a channel open.  Most telephony applications are run at
		 * elevated priorities so this sleep can prevent the high
		 * priority threads from consuming the CPU if they're not
		 * expecting surprise device removal.
		 */
		msleep(5);
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
		rv = schluffen(&chan->readbufq);
		if (rv)
			return rv;
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
				size_t count)
{
	unsigned long flags;
	struct dahdi_chan *chan = file->private_data;
	int res, amnt, oldbuf, rv, x;

	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;

	if (unlikely(!chan)) {
		/* We would typically be here because of surprise hardware
		 * removal or driver unbinding while a user space application
		 * has a channel open.  Most telephony applications are run at
		 * elevated priorities so this sleep can prevent the high
		 * priority threads from consuming the CPU if they're not
		 * expecting surprise device removal.
		 */
		msleep(5);
		return -ENODEV;
	}

	if (unlikely(count < 1))
		return -EINVAL;

	for (;;) {
		spin_lock_irqsave(&chan->lock, flags);
		if ((chan->curtone || chan->pdialcount) && !(chan->flags & DAHDI_FLAG_PSEUDO)) {
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
		/* Wait for something to be available */
		rv = schluffen(&chan->writebufq);
		if (rv) {
			return rv;
		}
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
	void *rxgain=NULL;
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

	init_waitqueue_head(&chan->sel);
	init_waitqueue_head(&chan->readbufq);
	init_waitqueue_head(&chan->writebufq);
	init_waitqueue_head(&chan->eventbufq);
	init_waitqueue_head(&chan->txstateq);

	/* Reset conferences */
	reset_conf(chan);

	/* I/O Mask, etc */
	chan->iomask = 0;
	/* release conference resource if any */
	if (chan->confna)
		dahdi_check_conf(chan->confna);
	if ((chan->sig & __DAHDI_SIG_DACS) != __DAHDI_SIG_DACS) {
		chan->confna = 0;
		chan->confmode = 0;
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
	if (chan->gainalloc && chan->rxgain)
		rxgain = chan->rxgain;
	chan->rxgain = defgain;
	chan->txgain = defgain;
	chan->gainalloc = 0;
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

	set_tone_zone(chan, -1);

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

	spin_lock_irqsave(&zaptimerlock, flags);
	list_add(&t->list, &zaptimers);
	spin_unlock_irqrestore(&zaptimerlock, flags);

	return 0;
}

static int dahdi_timer_release(struct file *file)
{
	struct dahdi_timer *t, *cur, *next;
	unsigned long flags;

	if (!(t = file->private_data))
		return 0;

	spin_lock_irqsave(&zaptimerlock, flags);

	list_for_each_entry_safe(cur, next, &zaptimers, list) {
		if (t == cur) {
			list_del(&cur->list);
			break;
		}
	}

	spin_unlock_irqrestore(&zaptimerlock, flags);

	if (!cur) {
		module_printk(KERN_NOTICE, "Timer: Not on list??\n");
		return 0;
	}

	kfree(cur);

	return 0;
}

static int dahdi_specchan_open(struct file *file)
{
	int res = 0;
	struct dahdi_chan *const chan = chan_from_file(file);

	if (chan && chan->sig) {
		/* Make sure we're not already open, a net device, or a slave device */
		if (chan->flags & DAHDI_FLAG_NETDEV)
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
			if (chan->flags & DAHDI_FLAG_PSEUDO)
				chan->flags |= DAHDI_FLAG_AUDIO;
			if (chan->span) {
				if (!try_module_get(chan->span->ops->owner))
					res = -ENXIO;
				else if (chan->span->ops->open)
					res = chan->span->ops->open(chan);
			}
			if (!res) {
				chan->file = file;
				file->private_data = chan;
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
		spin_lock_irqsave(&chan->lock, flags);
		chan->file = NULL;
		file->private_data = NULL;
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
	return maxspans > 0;
#endif
}

static struct dahdi_chan *dahdi_alloc_pseudo(void)
{
	struct dahdi_chan *pseudo;

	/* Don't allow /dev/dahdi/pseudo to open if there is not a timing
	 * source. */
	if (!can_open_timer())
		return NULL;

	if (!(pseudo = kzalloc(sizeof(*pseudo), GFP_KERNEL)))
		return NULL;

	pseudo->sig = DAHDI_SIG_CLEAR;
	pseudo->sigcap = DAHDI_SIG_CLEAR;
	pseudo->flags = DAHDI_FLAG_PSEUDO | DAHDI_FLAG_AUDIO;

	if (dahdi_chan_reg(pseudo)) {
		kfree(pseudo);
		pseudo = NULL;
	} else {
		snprintf(pseudo->name, sizeof(pseudo->name)-1,"Pseudo/%d", pseudo->channo); 
	}

	return pseudo;
}

static void dahdi_free_pseudo(struct dahdi_chan *pseudo)
{
	if (pseudo) {
		dahdi_chan_unreg(pseudo);
		kfree(pseudo);
	}
}

static int dahdi_open(struct inode *inode, struct file *file)
{
	int unit = UNIT(file);
	struct dahdi_chan *chan;
	/* Minor 0: Special "control" descriptor */
	if (!unit)
		return dahdi_ctl_open(file);
	if (unit == 250) {
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
	if (unit == 253) {
		if (can_open_timer()) {
			return dahdi_timing_open(file);
		} else {
			return -ENXIO;
		}
	}
	if (unit == 254)
		return dahdi_chan_open(file);
	if (unit == 255) {
		chan = dahdi_alloc_pseudo();
		if (chan) {
			file->private_data = chan;
			return dahdi_specchan_open(file);
		} else {
			return -ENXIO;
		}
	}
	return dahdi_specchan_open(file);
}

#if 0
static int dahdi_open(struct file *file)
{
	int res;
	unsigned long flags;
	spin_lock_irqsave(&bigzaplock, flags);
	res = __dahdi_open(file);
	spin_unlock_irqrestore(&bigzaplock, flags);
	return res;
}
#endif

static ssize_t dahdi_read(struct file *file, char __user *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);
	struct dahdi_chan *chan;

	/* Can't read from control */
	if (!unit) {
		return -EINVAL;
	}

	if (unit == 253)
		return -EINVAL;

	if (unit == 254) {
		chan = file->private_data;
		if (!chan)
			return -EINVAL;
		return dahdi_chan_read(file, usrbuf, count);
	}

	if (unit == 255) {
		chan = file->private_data;
		if (!chan) {
			module_printk(KERN_NOTICE, "No pseudo channel structure to read?\n");
			return -EINVAL;
		}
		return dahdi_chan_read(file, usrbuf, count);
	}
	if (count < 0)
		return -EINVAL;

	return dahdi_chan_read(file, usrbuf, count);
}

static ssize_t dahdi_write(struct file *file, const char __user *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);
	struct dahdi_chan *chan;
	/* Can't read from control */
	if (!unit)
		return -EINVAL;
	if (count < 0)
		return -EINVAL;
	if (unit == 253)
		return -EINVAL;
	if (unit == 254) {
		chan = file->private_data;
		if (!chan)
			return -EINVAL;
		return dahdi_chan_write(file, usrbuf, count);
	}
	if (unit == 255) {
		chan = file->private_data;
		if (!chan) {
			module_printk(KERN_NOTICE, "No pseudo channel structure to read?\n");
			return -EINVAL;
		}
		return dahdi_chan_write(file, usrbuf, count);
	}
	return dahdi_chan_write(file, usrbuf, count);

}

static int dahdi_set_default_zone(int defzone)
{
	if ((defzone < 0) || (defzone >= DAHDI_TONE_ZONE_MAX))
		return -EINVAL;
	write_lock(&zone_lock);
	if (!tone_zones[defzone]) {
		write_unlock(&zone_lock);
		return -EINVAL;
	}
	if ((default_zone != -1) && tone_zones[default_zone])
		atomic_dec(&tone_zones[default_zone]->refcount);
	atomic_inc(&tone_zones[defzone]->refcount);
	default_zone = defzone;
	write_unlock(&zone_lock);
	return 0;
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
static int ioctl_load_zone(unsigned long data)
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
	void *slab, *ptr;
	struct dahdi_zone *z;
	struct dahdi_tone *t;
	void __user * user_data = (void __user *)data;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;

	if (copy_from_user(&work->th, user_data, sizeof(work->th))) {
		kfree(work);
		return -EFAULT;
	}

	user_data += sizeof(work->th);

	if ((work->th.count < 0) || (work->th.count > MAX_TONES)) {
		module_printk(KERN_NOTICE, "Too many tones included\n");
		kfree(work);
		return -EINVAL;
	}

	space = size = sizeof(*z) + work->th.count * sizeof(*t);

	if (size > MAX_SIZE) {
		kfree(work);
		return -E2BIG;
	}

	z = ptr = slab = kzalloc(size, GFP_KERNEL);
	if (!z) {
		kfree(work);
		return -ENOMEM;
	}

	ptr += sizeof(*z);
	space -= sizeof(*z);

	dahdi_copy_string(z->name, work->th.name, sizeof(z->name));

	for (x = 0; x < DAHDI_MAX_CADENCE; x++)
		z->ringcadence[x] = work->th.ringcadence[x];

	atomic_set(&z->refcount, 0);

	for (x = 0; x < work->th.count; x++) {
		enum {
			REGULAR_TONE,
			DTMF_TONE,
			MFR1_TONE,
			MFR2_FWD_TONE,
			MFR2_REV_TONE,
		} tone_type;

		if (space < sizeof(*t)) {
			kfree(slab);
			kfree(work);
			module_printk(KERN_NOTICE, "Insufficient tone zone space\n");
			return -EINVAL;
		}

		res = copy_from_user(&work->td, user_data,
				     sizeof(work->td));
		if (res) {
			kfree(slab);
			kfree(work);
			return -EFAULT;
		}

		user_data += sizeof(work->td);

		if ((work->td.tone >= 0) && (work->td.tone < DAHDI_TONE_MAX)) {
			tone_type = REGULAR_TONE;

			t = work->samples[x] = ptr;

			space -= sizeof(*t);
			ptr += sizeof(*t);

			/* Remember which sample is work->next */
			work->next[x] = work->td.next;

			/* Make sure the "next" one is sane */
			if ((work->next[x] >= work->th.count) ||
			    (work->next[x] < 0)) {
				module_printk(KERN_NOTICE,
					      "Invalid 'next' pointer: %d\n",
					      work->next[x]);
				kfree(slab);
				kfree(work);
				return -EINVAL;
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
			kfree(slab);
			kfree(work);
			return -EINVAL;
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

	for (x = 0; x < work->th.count; x++) {
		if (work->samples[x])
			work->samples[x]->next = work->samples[work->next[x]];
	}

	res = dahdi_register_tone_zone(work->th.zone, z);
	if (res) {
		kfree(slab);
	} else {
		if ( -1 == default_zone ) {
			dahdi_set_default_zone(work->th.zone);
		}
	}

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
		/* You should not get here */
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

	if (!unit)
		return dahdi_ctl_release(file);
	if (unit == 253) {
		return dahdi_timer_release(file);
	}
	if (unit == 250) {
		/* We should not be here because the dahdi_transcode.ko module
		 * should have updated the file_operations for this file
		 * handle when the file was opened. */
		WARN_ON(1);
		return -EFAULT;
	}
	if (unit == 254) {
		chan = file->private_data;
		if (!chan)
			return dahdi_chan_release(file);
		else
			return dahdi_specchan_release(file);
	}
	if (unit == 255) {
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

#if 0
static int dahdi_release(struct inode *inode, struct file *file)
{
	/* Lock the big zap lock when handling a release */
	unsigned long flags;
	int res;
	spin_lock_irqsave(&bigzaplock, flags);
	res = __dahdi_release(file);
	spin_unlock_irqrestore(&bigzaplock, flags);
	return res;
}
#endif


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

		/* Switch to other master if current master in alarm */
		for (x=1; x<maxspans; x++) {
			struct dahdi_span *const s = spans[x];
			if (s && !s->alarms && (s->flags & DAHDI_FLAG_RUNNING)) {
				if (debug && (master != s)) {
					module_printk(KERN_NOTICE,
						"Master changed to %s\n",
						s->name);
				}
				master = s;
				break;
			}
		}
		/* Report more detailed alarms */
		if (debug) {
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
		spin_lock_irqsave(&zaptimerlock, flags);
		timer->ms = timer->pos = j;
		spin_unlock_irqrestore(&zaptimerlock, flags);
		break;
	case DAHDI_TIMERACK:
		get_user(j, (int __user *)data);
		spin_lock_irqsave(&zaptimerlock, flags);
		if ((j < 1) || (j > timer->tripped))
			j = timer->tripped;
		timer->tripped -= j;
		spin_unlock_irqrestore(&zaptimerlock, flags);
		break;
	case DAHDI_GETEVENT:  /* Get event on queue */
		j = DAHDI_EVENT_NONE;
		spin_lock_irqsave(&zaptimerlock, flags);
		  /* set up for no event */
		if (timer->tripped)
			j = DAHDI_EVENT_TIMER_EXPIRED;
		if (timer->ping)
			j = DAHDI_EVENT_TIMER_PING;
		spin_unlock_irqrestore(&zaptimerlock, flags);
		put_user(j, (int __user *)data);
		break;
	case DAHDI_TIMERPING:
		spin_lock_irqsave(&zaptimerlock, flags);
		timer->ping = 1;
		wake_up_interruptible(&timer->sel);
		spin_unlock_irqrestore(&zaptimerlock, flags);
		break;
	case DAHDI_TIMERPONG:
		spin_lock_irqsave(&zaptimerlock, flags);
		timer->ping = 0;
		spin_unlock_irqrestore(&zaptimerlock, flags);
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
	if (!gain->chan) {
		chan = chan_from_file(file);
	} else {
		if (valid_channo(gain->chan))
			chan = chans[gain->chan];
		else
			chan = NULL;
	}
	if (chan) {
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

	if (!gain->chan) {
		chan = chan_from_file(file);
	} else {
		if (valid_channo(gain->chan))
			chan = chans[gain->chan];
		else
			chan = NULL;
	}

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
		if (chan->gainalloc)
			kfree(chan->rxgain);
		chan->gainalloc = 0;
		chan->rxgain = defgain;
		chan->txgain = defgain;
		spin_unlock_irqrestore(&chan->lock, flags);
	} else {
		/* This is a custom gain setting */
		spin_lock_irqsave(&chan->lock, flags);
		if (chan->gainalloc)
			kfree(chan->rxgain);
		chan->gainalloc = 1;
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

	VALID_CHANNEL(channo);
	chan = chans[channo];
	if (!chan)
		return -EINVAL;

	temp = kmalloc(sizeof(*chan), GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	/* lock channel */
	spin_lock_irqsave(&chan->lock, flags);
	/* make static copy of channel */
	*temp = *chan;
	if (temp->ec_state)
		ec_state = *temp->ec_state;

	/* release it. */
	spin_unlock_irqrestore(&chan->lock, flags);

	module_printk(KERN_INFO, "Dump of DAHDI Channel %d (%s,%d,%d):\n\n",
		      channo, temp->name, temp->channo, temp->chanpos);
	module_printk(KERN_INFO, "flags: %x hex, writechunk: %p, readchunk: %p\n",
		      (unsigned int) temp->flags, temp->writechunk, temp->readchunk);
	module_printk(KERN_INFO, "rxgain: %p, txgain: %p, gainalloc: %d\n",
		      temp->rxgain, temp->txgain, temp->gainalloc);
	module_printk(KERN_INFO, "span: %p, sig: %x hex, sigcap: %x hex\n",
		      temp->span, temp->sig, temp->sigcap);
	module_printk(KERN_INFO, "inreadbuf: %d, outreadbuf: %d, inwritebuf: %d, outwritebuf: %d\n",
		      temp->inreadbuf, temp->outreadbuf, temp->inwritebuf, temp->outwritebuf);
	module_printk(KERN_INFO, "blocksize: %d, numbufs: %d, txbufpolicy: %d, txbufpolicy: %d\n",
		      temp->blocksize, temp->numbufs, temp->txbufpolicy, temp->rxbufpolicy);
	module_printk(KERN_INFO, "txdisable: %d, rxdisable: %d, iomask: %d\n",
		      temp->txdisable, temp->rxdisable, temp->iomask);
	module_printk(KERN_INFO, "curzone: %p, tonezone: %d, curtone: %p, tonep: %d\n",
		      temp->curzone, temp->tonezone, temp->curtone, temp->tonep);
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
	chan = file->private_data;
	if (!param.channo || chan) {
		param.channo = chan->channo;
	} else {
		/* Check validity of channel */
		VALID_CHANNEL(param.channo);
		chan = chans[param.channo];
	}
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
	dahdi_copy_string(param.name, chan->name, sizeof(param.name));
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
	chan = file->private_data;
	if (!param.channo || chan) {
		param.channo = chan->channo;
	} else {
		/* Check validity of channel */
		VALID_CHANNEL(param.channo);
		chan = chans[param.channo];
	}

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

static int dahdi_common_ioctl(struct file *file, unsigned int cmd, unsigned long data, int unit)
{
	union {
		struct dahdi_spaninfo_v1 spaninfo_v1;
		struct dahdi_spaninfo spaninfo;
	} stack;

	struct dahdi_span *s;
	int i,j;
	size_t size_to_copy;
	void __user * const user_data = (void __user *)data;

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
		size_to_copy = sizeof(struct dahdi_spaninfo);
		if (copy_from_user(&stack.spaninfo, user_data, size_to_copy))
			return -EFAULT;
		i = stack.spaninfo.spanno; /* get specified span number */
		if (i == 0) {
			/* if to figure it out for this chan */
			if (!chans[unit])
				return -EINVAL;
			i = chans[unit]->span->spanno;
		}
		s = find_span(i);
		if (!s)
			return -EINVAL;
		stack.spaninfo.spanno = i; /* put the span # in here */
		stack.spaninfo.totalspans = span_count();
		dahdi_copy_string(stack.spaninfo.desc, s->desc, sizeof(stack.spaninfo.desc));
		dahdi_copy_string(stack.spaninfo.name, s->name, sizeof(stack.spaninfo.name));
		stack.spaninfo.alarms = s->alarms;		/* get alarm status */
		stack.spaninfo.rxlevel = s->rxlevel;	/* get rx level */
		stack.spaninfo.txlevel = s->txlevel;	/* get tx level */

		stack.spaninfo.bpvcount = s->count.bpv;
		stack.spaninfo.crc4count = s->count.crc4;
		stack.spaninfo.ebitcount = s->count.ebit;
		stack.spaninfo.fascount = s->count.fas;
		stack.spaninfo.fecount = s->count.fe;
		stack.spaninfo.cvcount = s->count.cv;
		stack.spaninfo.becount = s->count.be;
		stack.spaninfo.prbs = s->count.prbs;
		stack.spaninfo.errsec = s->count.errsec;

		stack.spaninfo.irqmisses = s->irqmisses;	/* get IRQ miss count */
		stack.spaninfo.syncsrc = s->syncsrc;	/* get active sync source */
		stack.spaninfo.totalchans = s->channels;
		stack.spaninfo.numchans = 0;
		for (j = 0; j < s->channels; j++) {
			if (s->chans[j]->sig)
				stack.spaninfo.numchans++;
		}
		stack.spaninfo.lbo = s->lbo;
		stack.spaninfo.lineconfig = s->lineconfig;
		stack.spaninfo.irq = s->irq;
		stack.spaninfo.linecompat = s->linecompat;
		dahdi_copy_string(stack.spaninfo.lboname, dahdi_lboname(s->lbo), sizeof(stack.spaninfo.lboname));
		if (s->manufacturer)
			dahdi_copy_string(stack.spaninfo.manufacturer, s->manufacturer,
				sizeof(stack.spaninfo.manufacturer));
		if (s->devicetype)
			dahdi_copy_string(stack.spaninfo.devicetype, s->devicetype, sizeof(stack.spaninfo.devicetype));
		dahdi_copy_string(stack.spaninfo.location, s->location, sizeof(stack.spaninfo.location));
		if (s->spantype)
			dahdi_copy_string(stack.spaninfo.spantype, s->spantype, sizeof(stack.spaninfo.spantype));

		if (copy_to_user(user_data, &stack.spaninfo, size_to_copy))
			return -EFAULT;
		break;
	case DAHDI_SPANSTAT_V1:
		size_to_copy = sizeof(struct dahdi_spaninfo_v1);
		if (copy_from_user(&stack.spaninfo_v1,
				   (__user struct dahdi_spaninfo_v1 *) data,
				   size_to_copy))
			return -EFAULT;
		i = stack.spaninfo_v1.spanno; /* get specified span number */
		if (i == 0) {
			/* if to figure it out for this chan */
			if (!chans[unit])
				return -EINVAL;
			i = chans[unit]->span->spanno;
		}
		s = find_span(i);
		if (!s)
			return -EINVAL;
		stack.spaninfo_v1.spanno = i; /* put the span # in here */
		stack.spaninfo_v1.totalspans = 0;
		stack.spaninfo_v1.totalspans = span_count();
		dahdi_copy_string(stack.spaninfo_v1.desc,
				  s->desc,
				  sizeof(stack.spaninfo_v1.desc));
		dahdi_copy_string(stack.spaninfo_v1.name,
				  s->name,
				  sizeof(stack.spaninfo_v1.name));
		stack.spaninfo_v1.alarms = s->alarms;
		stack.spaninfo_v1.bpvcount = s->count.bpv;
		stack.spaninfo_v1.rxlevel = s->rxlevel;
		stack.spaninfo_v1.txlevel = s->txlevel;
		stack.spaninfo_v1.crc4count = s->count.crc4;
		stack.spaninfo_v1.ebitcount = s->count.ebit;
		stack.spaninfo_v1.fascount = s->count.fas;
		stack.spaninfo_v1.irqmisses = s->irqmisses;
		stack.spaninfo_v1.syncsrc = s->syncsrc;
		stack.spaninfo_v1.totalchans = s->channels;
		stack.spaninfo_v1.numchans = 0;
		for (j = 0; j < s->channels; j++) {
			if (s->chans[j]->sig)
				stack.spaninfo_v1.numchans++;
		}
		stack.spaninfo_v1.lbo = s->lbo;
		stack.spaninfo_v1.lineconfig = s->lineconfig;
		stack.spaninfo_v1.irq = s->irq;
		stack.spaninfo_v1.linecompat = s->linecompat;
		dahdi_copy_string(stack.spaninfo_v1.lboname,
				  dahdi_lboname(s->lbo),
				  sizeof(stack.spaninfo_v1.lboname));
		if (s->manufacturer)
			dahdi_copy_string(stack.spaninfo_v1.manufacturer,
				s->manufacturer,
				sizeof(stack.spaninfo_v1.manufacturer));
		if (s->devicetype)
			dahdi_copy_string(stack.spaninfo_v1.devicetype,
					  s->devicetype,
					  sizeof(stack.spaninfo_v1.devicetype));
		dahdi_copy_string(stack.spaninfo_v1.location,
				  s->location,
				  sizeof(stack.spaninfo_v1.location));
		if (s->spantype)
			dahdi_copy_string(stack.spaninfo_v1.spantype,
					  s->spantype,
					  sizeof(stack.spaninfo_v1.spantype));

		if (copy_to_user((__user struct dahdi_spaninfo_v1 *) data,
				 &stack.spaninfo_v1, size_to_copy))
			return -EFAULT;
		break;
	case DAHDI_CHANDIAG_V1: /* Intentional drop through. */
	case DAHDI_CHANDIAG:
		return dahdi_ioctl_chandiag(file, data);
	default:
		return -ENOTTY;
	}
	return 0;
}

static int (*dahdi_dynamic_ioctl)(unsigned int cmd, unsigned long data);

void dahdi_set_dynamic_ioctl(int (*func)(unsigned int cmd, unsigned long data))
{
	dahdi_dynamic_ioctl = func;
}

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
			last->nextslave = x;
			last = chan->span->chans[x];
		}
	/* Terminate list */
	last->nextslave = 0;
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
	unsigned long flags;
	int sigcap;

	if (copy_from_user(&ch, (void __user *)data, sizeof(ch)))
		return -EFAULT;
	VALID_CHANNEL(ch.chan);
	if (ch.sigtype == DAHDI_SIG_SLAVE) {
		/* We have to use the master's sigtype */
		if ((ch.master < 1) || (ch.master >= DAHDI_MAX_CHANNELS))
			return -EINVAL;
		if (!chans[ch.master])
			return -EINVAL;
		ch.sigtype = chans[ch.master]->sig;
		newmaster = chans[ch.master];
	} else if ((ch.sigtype & __DAHDI_SIG_DACS) == __DAHDI_SIG_DACS) {
		newmaster = chans[ch.chan];
		if ((ch.idlebits < 1) || (ch.idlebits >= DAHDI_MAX_CHANNELS))
			return -EINVAL;
		if (!chans[ch.idlebits])
			return -EINVAL;
	} else {
		newmaster = chans[ch.chan];
	}
	chan = chans[ch.chan];
	spin_lock_irqsave(&chan->lock, flags);
#ifdef CONFIG_DAHDI_NET
	if (chan->flags & DAHDI_FLAG_NETDEV) {
		if (ztchan_to_dev(chan)->flags & IFF_UP) {
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
		chan->flags &= ~DAHDI_FLAG_NETDEV;
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

	if ((sigcap & ch.sigtype) != ch.sigtype)
		res = -EINVAL;

	if (chan->master != chan) {
		struct dahdi_chan *oldmaster = chan->master;

		/* Clear the master channel */
		chan->master = chan;
		chan->nextslave = 0;
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
			/* Setup conference properly */
			chan->confmode = DAHDI_CONF_DIGITALMON;
			chan->confna = ch.idlebits;
			res = dahdi_chan_dacs(chan,
					      chans[ch.idlebits]);
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

	if (!res && chan->span->ops->chanconfig)
		res = chan->span->ops->chanconfig(chan, ch.sigtype);

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
				chan->hdlcnetdev->netdev->irq = chan->span->irq;
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
				chan->flags |= DAHDI_FLAG_NETDEV;
		} else {
			module_printk(KERN_NOTICE, "Unable to allocate netdev: out of memory\n");
			res = -1;
		}
	}
#endif
	if ((chan->sig == DAHDI_SIG_HDLCNET) &&
	    (chan == newmaster) &&
	    !(chan->flags & DAHDI_FLAG_NETDEV))
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

static int dahdi_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	/* I/O CTL's for control interface */
	int i,j;
	int res = 0;
	int x,y;
	unsigned long flags;
	void __user * const user_data = (void __user *)data;
	int rv;
	struct dahdi_span *s;
	struct dahdi_chan *chan;

	switch (cmd) {
	case DAHDI_INDIRECT:
	{
		struct dahdi_indirect_data ind;
		void *old;
		static bool warned;

		if (copy_from_user(&ind, (void __user *)data, sizeof(ind)))
			return -EFAULT;

		VALID_CHANNEL(ind.chan);
		chan = chans[ind.chan];
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
	case DAHDI_SPANCONFIG:
	{
		struct dahdi_lineconfig lc;
		struct dahdi_span *s;

		if (copy_from_user(&lc, user_data, sizeof(lc)))
			return -EFAULT;
		s = find_span(lc.span);
		if (!s)
			return -ENXIO;
		if ((lc.lineconfig & 0x1ff0 & s->linecompat) !=
		    (lc.lineconfig & 0x1ff0))
			return -EINVAL;
		if (s->ops->spanconfig) {
			s->lineconfig = lc.lineconfig;
			s->lbo = lc.lbo;
			s->txlevel = lc.lbo;
			s->rxlevel = 0;

			return s->ops->spanconfig(s, &lc);
		}
		return 0;
	}
	case DAHDI_STARTUP:
		if (get_user(j, (int __user *)data))
			return -EFAULT;
		s = find_span(j);
		if (!s)
			return -ENXIO;

		if (s->flags & DAHDI_FLAG_RUNNING)
			return 0;

		if (s->ops->startup)
			res = s->ops->startup(s);

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
		}
		return 0;
	case DAHDI_SHUTDOWN:
		if (get_user(j, (int __user *)data))
			return -EFAULT;
		s = find_span(j);
		if (!s)
			return -ENXIO;

		/* Unconfigure channels */
		for (x = 0; x < s->channels; x++)
			s->chans[x]->sig = 0;

		if (s->ops->shutdown)
			res =  s->ops->shutdown(s);
		s->flags &= ~DAHDI_FLAG_RUNNING;
		return 0;
	case DAHDI_ATTACH_ECHOCAN:
	{
		struct dahdi_attach_echocan ae;
		const struct dahdi_echocan_factory *new = NULL, *old;

		if (copy_from_user(&ae, user_data, sizeof(ae)))
			return -EFAULT;

		VALID_CHANNEL(ae.chan);

		ae.echocan[sizeof(ae.echocan) - 1] = 0;
		if (ae.echocan[0]) {
			if (!(new = find_echocan(ae.echocan))) {
				return -EINVAL;
			}
		}

		spin_lock_irqsave(&chans[ae.chan]->lock, flags);
		old = chans[ae.chan]->ec_factory;
		chans[ae.chan]->ec_factory = new;
		spin_unlock_irqrestore(&chans[ae.chan]->lock, flags);

		if (old) {
			release_echocan(old);
		}

		break;
	}
	case DAHDI_CHANCONFIG:
		return dahdi_ioctl_chanconfig(file, data);

	case DAHDI_SFCONFIG:
	{
		struct dahdi_sfconfig sf;

		if (copy_from_user(&sf, user_data, sizeof(sf)))
			return -EFAULT;
		VALID_CHANNEL(sf.chan);
		if (chans[sf.chan]->sig != DAHDI_SIG_SF) return -EINVAL;
		spin_lock_irqsave(&chans[sf.chan]->lock, flags);
		chans[sf.chan]->rxp1 = sf.rxp1;
		chans[sf.chan]->rxp2 = sf.rxp2;
		chans[sf.chan]->rxp3 = sf.rxp3;
		chans[sf.chan]->txtone = sf.txtone;
		chans[sf.chan]->tx_v2 = sf.tx_v2;
		chans[sf.chan]->tx_v3 = sf.tx_v3;
		chans[sf.chan]->toneflags = sf.toneflag;
		if (sf.txtone) /* if set to make tone for tx */
		{
			if ((chans[sf.chan]->txhooksig && !(sf.toneflag & DAHDI_REVERSE_TXTONE)) ||
			 ((!chans[sf.chan]->txhooksig) && (sf.toneflag & DAHDI_REVERSE_TXTONE)))
			{
				set_txtone(chans[sf.chan],sf.txtone,sf.tx_v2,sf.tx_v3);
			}
			else
			{
				set_txtone(chans[sf.chan],0,0,0);
			}
		}
		spin_unlock_irqrestore(&chans[sf.chan]->lock, flags);
		return res;
	}
	case DAHDI_DEFAULTZONE:
		if (get_user(j, (int __user *)data))
			return -EFAULT;
		return dahdi_set_default_zone(j);
	case DAHDI_LOADZONE:
		return ioctl_load_zone(data);
	case DAHDI_FREEZONE:
		get_user(j, (int __user *) data);
		return free_tone_zone(j);
	case DAHDI_SET_DIALPARAMS:
	{
		struct dahdi_dialparams tdp;

		if (copy_from_user(&tdp, user_data, sizeof(tdp)))
			return -EFAULT;

		if ((tdp.dtmf_tonelen >= 10) && (tdp.dtmf_tonelen <= 4000)) {
			global_dialparams.dtmf_tonelen = tdp.dtmf_tonelen;
		}
		if ((tdp.mfv1_tonelen >= 10) && (tdp.mfv1_tonelen <= 4000)) {
			global_dialparams.mfv1_tonelen = tdp.mfv1_tonelen;
		}
		if ((tdp.mfr2_tonelen >= 10) && (tdp.mfr2_tonelen <= 4000)) {
			global_dialparams.mfr2_tonelen = tdp.mfr2_tonelen;
		}

		/* update the lengths in all currently loaded zones */
		write_lock(&zone_lock);
		for (j = 0; j < ARRAY_SIZE(tone_zones); j++) {
			struct dahdi_zone *z = tone_zones[j];

			if (!z)
				continue;

			for (i = 0; i < ARRAY_SIZE(z->dtmf); i++) {
				z->dtmf[i].tonesamples = global_dialparams.dtmf_tonelen * DAHDI_CHUNKSIZE;
			}

			/* for MFR1, we only adjust the length of the digits */
			for (i = DAHDI_TONE_MFR1_0; i <= DAHDI_TONE_MFR1_9; i++) {
				z->mfr1[i - DAHDI_TONE_MFR1_BASE].tonesamples = global_dialparams.mfv1_tonelen * DAHDI_CHUNKSIZE;
			}

			for (i = 0; i < ARRAY_SIZE(z->mfr2_fwd); i++) {
				z->mfr2_fwd[i].tonesamples = global_dialparams.mfr2_tonelen * DAHDI_CHUNKSIZE;
			}

			for (i = 0; i < ARRAY_SIZE(z->mfr2_rev); i++) {
				z->mfr2_rev[i].tonesamples = global_dialparams.mfr2_tonelen * DAHDI_CHUNKSIZE;
			}
		}
		write_unlock(&zone_lock);

		dtmf_silence.tonesamples = global_dialparams.dtmf_tonelen * DAHDI_CHUNKSIZE;
		mfr1_silence.tonesamples = global_dialparams.mfv1_tonelen * DAHDI_CHUNKSIZE;
		mfr2_silence.tonesamples = global_dialparams.mfr2_tonelen * DAHDI_CHUNKSIZE;

		break;
	}
	case DAHDI_GET_DIALPARAMS:
	{
		struct dahdi_dialparams tdp;

		tdp = global_dialparams;
		if (copy_to_user(user_data, &tdp, sizeof(tdp)))
			return -EFAULT;
		break;
	}
	case DAHDI_GETVERSION:
	{
		struct dahdi_versioninfo vi;
		struct ecfactory *cur;
		size_t space = sizeof(vi.echo_canceller) - 1;

		memset(&vi, 0, sizeof(vi));
		dahdi_copy_string(vi.version, DAHDI_VERSION, sizeof(vi.version));
		read_lock(&ecfactory_list_lock);
		list_for_each_entry(cur, &ecfactory_list, list) {
			strncat(vi.echo_canceller + strlen(vi.echo_canceller), cur->ec->name, space);
			space -= strlen(cur->ec->name);
			if (space < 1) {
				break;
			}
			if (cur->list.next && (cur->list.next != &ecfactory_list)) {
				strncat(vi.echo_canceller + strlen(vi.echo_canceller), ", ", space);
				space -= 2;
				if (space < 1) {
					break;
				}
			}
		}
		read_unlock(&ecfactory_list_lock);
		if (copy_to_user(user_data, &vi, sizeof(vi)))
			return -EFAULT;
		break;
	}
	case DAHDI_MAINT:  /* do maintenance stuff */
	{
		struct dahdi_maintinfo maint;
		  /* get struct from user */
		if (copy_from_user(&maint, user_data, sizeof(maint)))
			return -EFAULT;
		s = find_span(maint.spanno);
		if (!s)
			return -EINVAL;
		if (!s->ops->maint)
			return -ENOSYS;
		spin_lock_irqsave(&s->lock, flags);
		  /* save current maint state */
		i = s->maintstat;
		  /* set maint mode */
		s->maintstat = maint.command;
		switch(maint.command) {
		case DAHDI_MAINT_NONE:
		case DAHDI_MAINT_LOCALLOOP:
		case DAHDI_MAINT_NETWORKLINELOOP:
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
			/* if same, ignore it */
			if (i == maint.command)
				break;
			rv = s->ops->maint(s, maint.command);
			spin_unlock_irqrestore(&s->lock, flags);
			if (rv)
				return rv;
			spin_lock_irqsave(&s->lock, flags);
			break;
		case DAHDI_MAINT_LOOPUP:
		case DAHDI_MAINT_LOOPDOWN:
			s->mainttimer = DAHDI_LOOPCODE_TIME * DAHDI_CHUNKSIZE;
			rv = s->ops->maint(s, maint.command);
			spin_unlock_irqrestore(&s->lock, flags);
			if (rv)
				return rv;
			rv = schluffen(&s->maintq);
			if (rv)
				return rv;
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
			if(!i)
				s->maintstat = 0;

			rv = s->ops->maint(s, maint.command);
			spin_unlock_irqrestore(&s->lock, flags);
			if (rv)
				return rv;
			spin_lock_irqsave(&s->lock, flags);
			break;
		default:
			spin_unlock_irqrestore(&s->lock, flags);
			module_printk(KERN_NOTICE,
				      "Unknown maintenance event: %d\n",
				      maint.command);
			return -ENOSYS;
		}
		dahdi_alarm_notify(s);  /* process alarm-related events */
		spin_unlock_irqrestore(&s->lock, flags);
		break;
	}
	case DAHDI_DYNAMIC_CREATE:
	case DAHDI_DYNAMIC_DESTROY:
		if (dahdi_dynamic_ioctl) {
			return dahdi_dynamic_ioctl(cmd, data);
		} else {
			request_module("dahdi_dynamic");
			if (dahdi_dynamic_ioctl)
				return dahdi_dynamic_ioctl(cmd, data);
		}
		return -ENOSYS;
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
	default:
		return dahdi_common_ioctl(file, cmd, data, 0);
	}
	return 0;
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
		dahdi_copy_string(chan->txdialbuf + strlen(chan->txdialbuf), tdo->dialstr, DAHDI_MAX_DTMF_BUF - strlen(chan->txdialbuf));
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
	unsigned long flags;
	unsigned int confmode;
	int j;

	if (copy_from_user(&conf, (void __user *)data, sizeof(conf)))
		return -EFAULT;

	confmode = conf.confmode & DAHDI_CONF_MODE_MASK;

	if (conf.chan) {
		VALID_CHANNEL(conf.chan);
		chan = chans[conf.chan];
	} else {
		if (file->private_data) {
			chan = file->private_data;
		} else {
			VALID_CHANNEL(UNIT(file));
			chan = chans[UNIT(file)];
		}
	}

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;

	if (is_monitor_mode(conf.confmode)) {
		/* Monitor mode -- it's a channel */
		if ((conf.confno < 0) || (conf.confno >= DAHDI_MAX_CHANNELS) ||
		    !chans[conf.confno])
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
	conf.chan = chan->channo;  /* return with real channel # */
	spin_lock_irqsave(&bigzaplock, flags);
	spin_lock(&chan->lock);
	if (conf.confno == -1)
		conf.confno = dahdi_first_empty_conference();
	if ((conf.confno < 1) && (conf.confmode)) {
		/* No more empty conferences */
		spin_unlock(&chan->lock);
		spin_unlock_irqrestore(&bigzaplock, flags);
		return -EBUSY;
	}
	  /* if changing confs, clear last added info */
	if (conf.confno != chan->confna) {
		memset(chan->conflast, 0, DAHDI_MAX_CHUNKSIZE);
		memset(chan->conflast1, 0, DAHDI_MAX_CHUNKSIZE);
		memset(chan->conflast2, 0, DAHDI_MAX_CHUNKSIZE);
	}
	j = chan->confna;  /* save old conference number */
	chan->confna = conf.confno;   /* set conference number */
	chan->confmode = conf.confmode;  /* set conference mode */
	chan->_confn = 0;		     /* Clear confn */
	dahdi_check_conf(j);
	dahdi_check_conf(conf.confno);
	if (chan->span && chan->span->ops->dacs) {
		if ((confmode == DAHDI_CONF_DIGITALMON) &&
		    (chan->txgain == defgain) &&
		    (chan->rxgain == defgain) &&
		    (chans[conf.confno]->txgain == defgain) &&
		    (chans[conf.confno]->rxgain == defgain)) {
			dahdi_chan_dacs(chan, chans[conf.confno]);
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

	if (chans[conf.confno]) {
		if ((confmode == DAHDI_CONF_MONITOR_RX_PREECHO) ||
		    (confmode == DAHDI_CONF_MONITOR_TX_PREECHO) ||
		    (confmode == DAHDI_CONF_MONITORBOTH_PREECHO)) {
			void *temp = kmalloc(sizeof(*chan->readchunkpreec) *
					     DAHDI_CHUNKSIZE, GFP_ATOMIC);
			chans[conf.confno]->readchunkpreec = temp;
		} else {
			kfree(chans[conf.confno]->readchunkpreec);
			chans[conf.confno]->readchunkpreec = NULL;
		}
	}

	spin_unlock(&chan->lock);
	spin_unlock_irqrestore(&bigzaplock, flags);
	if (copy_to_user((void __user *)data, &conf, sizeof(conf)))
		return -EFAULT;
	return 0;
}

static int dahdi_ioctl_conflink(struct file *file, unsigned long data)
{
	struct dahdi_chan *chan;
	struct dahdi_confinfo conf;
	unsigned long flags;
	int res = 0;
	int i;

	if (file->private_data) {
		chan = file->private_data;
	} else {
		VALID_CHANNEL(UNIT(file));
		chan = chans[UNIT(file)];
	}

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;
	if (copy_from_user(&conf, (void __user *)data, sizeof(conf)))
		return -EFAULT;
	/* check sanity of arguments */
	if ((conf.chan < 0) || (conf.chan > DAHDI_MAX_CONF))
		return -EINVAL;
	if ((conf.confno < 0) || (conf.confno > DAHDI_MAX_CONF))
		return -EINVAL;
	/* cant listen to self!! */
	if (conf.chan && (conf.chan == conf.confno))
		return -EINVAL;

	spin_lock_irqsave(&bigzaplock, flags);
	spin_lock(&chan->lock);

	/* if to clear all links */
	if ((!conf.chan) && (!conf.confno)) {
		/* clear all the links */
		memset(conf_links, 0, sizeof(conf_links));
		recalc_maxlinks();
		spin_unlock(&chan->lock);
		spin_unlock_irqrestore(&bigzaplock, flags);
		return 0;
	}
	/* look for already existant specified combination */
	for (i = 1; i <= DAHDI_MAX_CONF; i++) {
		/* if found, exit */
		if ((conf_links[i].src == conf.chan) &&
		    (conf_links[i].dst == conf.confno))
			break;
	}
	if (i <= DAHDI_MAX_CONF) { /* if found */
		if (!conf.confmode) { /* if to remove link */
			conf_links[i].src = 0;
			conf_links[i].dst = 0;
		} else { /* if to add and already there, error */
			res = -EEXIST;
		}
	} else { /* if not found */
		if (conf.confmode) { /* if to add link */
			/* look for empty location */
			for (i = 1; i <= DAHDI_MAX_CONF; i++) {
				/* if empty, exit loop */
				if ((!conf_links[i].src) &&
				    (!conf_links[i].dst))
					break;
			}
			/* if empty spot found */
			if (i <= DAHDI_MAX_CONF) {
				conf_links[i].src = conf.chan;
				conf_links[i].dst = conf.confno;
			} else { /* if no empties -- error */
				res = -ENOSPC;
			 }
		} else { /* if to remove, and not found -- error */
			res = -ENOENT;
		}
	}
	recalc_maxlinks();
	spin_unlock(&chan->lock);
	spin_unlock_irqrestore(&bigzaplock, flags);
	return res;
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
	int rv;
	int i;
	int j;
	int c;
	int k;

	if (file->private_data) {
		chan = file->private_data;
	} else {
		VALID_CHANNEL(UNIT(file));
		chan = chans[UNIT(file)];
	}

	if (!(chan->flags & DAHDI_FLAG_AUDIO))
		return -EINVAL;

	get_user(j, (int __user *)data);  /* get conf # */

	/* loop thru the interesting ones */
	for (i = ((j) ? j : 1); i <= ((j) ? j : DAHDI_MAX_CONF); i++) {
		c = 0;
		for (k = 1; k < DAHDI_MAX_CHANNELS; k++) {
			struct dahdi_chan *const pos = chans[k];
			/* skip if no pointer */
			if (!pos)
				continue;
			/* skip if not in this conf */
			if (pos->confna != i)
				continue;
			if (!c) {
				module_printk(KERN_NOTICE,
					      "Conf #%d:\n", i);
			}
			c = 1;
			module_printk(KERN_NOTICE, "chan %d, mode %x\n",
				      k, pos->confmode);
		}
		rv = 0;
		for (k = 1; k <= DAHDI_MAX_CONF; k++) {
			if (conf_links[k].dst == i) {
				if (!c) {
					c = 1;
					module_printk(KERN_NOTICE,
						      "Conf #%d:\n", i);
				}
				if (!rv) {
					rv = 1;
					module_printk(KERN_NOTICE,
						      "Snooping on:\n");
				}
				module_printk(KERN_NOTICE, "conf %d\n",
					      conf_links[k].src);
			}
		}
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

	if (!conf.chan) {
		/* If 0, use the current channel. */
		if (file->private_data) {
			chan = file->private_data;
		} else {
			VALID_CHANNEL(UNIT(file));
			chan = chans[UNIT(file)];
		}
	} else {
		VALID_CHANNEL(conf.chan);
		chan = chans[conf.chan];
	}
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
	int ret;
	void __user * const user_data = (void __user *)data;

	if (!chan)
		return -EINVAL;
	switch(cmd) {
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
			wake_up_interruptible(&chan->readbufq);  /* wake_up_interruptible waiting on read */
			wake_up_interruptible(&chan->sel); /* wake_up_interruptible waiting on select */
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
			wake_up_interruptible(&chan->writebufq); /* wake_up_interruptible waiting on write */
			wake_up_interruptible(&chan->sel);  /* wake_up_interruptible waiting on select */
			   /* if IO MUX wait on write empty, well, this
				certainly *did* empty the write */
			if (chan->iomask & DAHDI_IOMUX_WRITEEMPTY)
				wake_up_interruptible(&chan->eventbufq); /* wake_up_interruptible waiting on IOMUX */
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
			if (!i) break; /* skip if none */
			rv = schluffen(&chan->writebufq);
			if (rv) return(rv);
		   }
		break;
	case DAHDI_IOMUX: /* wait for something to happen */
		get_user(chan->iomask, (int __user *)data);  /* save mask */
		if (!chan->iomask) return(-EINVAL);  /* cant wait for nothing */
		for(;;)  /* loop forever */
		   {
			  /* has to have SOME mask */
			ret = 0;  /* start with empty return value */
			spin_lock_irqsave(&chan->lock, flags);
			  /* if looking for read */
			if (chan->iomask & DAHDI_IOMUX_READ)
			   {
				/* if read available */
				if ((chan->outreadbuf > -1)  && !chan->rxdisable)
					ret |= DAHDI_IOMUX_READ;
			   }
			  /* if looking for write avail */
			if (chan->iomask & DAHDI_IOMUX_WRITE)
			   {
				if (chan->inwritebuf > -1)
					ret |= DAHDI_IOMUX_WRITE;
			   }
			  /* if looking for write empty */
			if (chan->iomask & DAHDI_IOMUX_WRITEEMPTY)
			   {
				  /* if everything empty -- be sure the transmitter is enabled */
				chan->txdisable = 0;
				if (chan->outwritebuf < 0)
					ret |= DAHDI_IOMUX_WRITEEMPTY;
			   }
			  /* if looking for signalling event */
			if (chan->iomask & DAHDI_IOMUX_SIGEVENT)
			   {
				  /* if event */
				if (chan->eventinidx != chan->eventoutidx)
					ret |= DAHDI_IOMUX_SIGEVENT;
			   }
			spin_unlock_irqrestore(&chan->lock, flags);
			  /* if something to return, or not to wait */
			if (ret || (chan->iomask & DAHDI_IOMUX_NOWAIT))
			   {
				  /* set return value */
				put_user(ret, (int __user *)data);
				break; /* get out of loop */
			   }
			rv = schluffen(&chan->eventbufq);
			if (rv) return(rv);
		   }
		  /* clear IO MUX mask */
		chan->iomask = 0;
		break;
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
		spin_lock_irqsave(&bigzaplock, flags);
		chan->confmute = j;
		spin_unlock_irqrestore(&bigzaplock, flags);
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
		if (chan->curzone)
			j = chan->tonezone;
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

	case DAHDI_CONFLINK:  /* do conf link stuff */
		return dahdi_ioctl_conflink(file, data);

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
		rv = dahdi_common_ioctl(file, cmd, data, chan->channo);
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
	int ret;
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

	/* attempt to use the span's echo canceler; fall back to built-in
	   if it fails (but not if an error occurs) */
	ret = dahdi_chan_echocan_create(chan, ecp, params, &ec);

	if ((ret == -ENODEV) && chan->ec_factory) {
		/* try to get another reference to the module providing
		   this channel's echo canceler */
		if (!try_module_get(chan->ec_factory->owner)) {
			module_printk(KERN_ERR, "Cannot get a reference to the '%s' echo canceler\n", chan->ec_factory->name);
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
				      ec_current->name);
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

static int dahdi_chan_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	struct dahdi_chan *const chan = chan_from_file(file);
	unsigned long flags;
	int j, rv;
	int ret;
	int oldconf;
	void *rxgain=NULL;

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
			if (chan->gainalloc && chan->rxgain)
				rxgain = chan->rxgain;
			else
				rxgain = NULL;

			chan->rxgain = defgain;
			chan->txgain = defgain;
			chan->gainalloc = 0;
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
					if (chan->gainalloc)
						kfree(chan->rxgain);
					chan->rxgain = defgain;
					chan->txgain = defgain;
					chan->gainalloc = 0;
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
#if 0
				rv = schluffen(&chan->txstateq);
				if (rv) return rv;
#endif
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
				rv = schluffen(&chan->txstateq);
				if (rv) return rv;
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
				rv = schluffen(&chan->txstateq);
				if (rv) return rv;
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
		if (channo < 1)
			return -EINVAL;
		if (channo > DAHDI_MAX_CHANNELS)
			return -EINVAL;
		file->private_data = chans[channo];
		res = dahdi_specchan_open(file);
		if (res)
			file->private_data = NULL;
		return res;
	default:
		return -ENOSYS;
	}
	return 0;
}

#ifdef HAVE_UNLOCKED_IOCTL
static long dahdi_ioctl(struct file *file, unsigned int cmd, unsigned long data)
#else
static int dahdi_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long data)
#endif
{
	int unit = UNIT(file);
	struct dahdi_timer *timer;
	int ret;

#ifdef HAVE_UNLOCKED_IOCTL
	lock_kernel();
#endif

	if (!unit) {
		ret = dahdi_ctl_ioctl(file, cmd, data);
		goto unlock_exit;
	}

	if (unit == 250) {
		/* dahdi_transcode should have updated the file_operations on
		 * this file object on open, so we shouldn't be here. */
		WARN_ON(1);
		ret = -EFAULT;
		goto unlock_exit;
	}

	if (unit == 253) {
		timer = file->private_data;
		if (timer)
			ret = dahdi_timer_ioctl(file, cmd, data, timer);
		else
			ret = -EINVAL;
		goto unlock_exit;
	}
	if (unit == 254) {
		if (file->private_data)
			ret = dahdi_chan_ioctl(file, cmd, data);
		else
			ret = dahdi_prechan_ioctl(file, cmd, data);
		goto unlock_exit;
	}
	if (unit == 255) {
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
#ifdef HAVE_UNLOCKED_IOCTL
	unlock_kernel();
#endif
	return ret;
}

#ifdef HAVE_COMPAT_IOCTL
static long dahdi_ioctl_compat(struct file *file, unsigned int cmd,
		unsigned long data)
{
	if (cmd == DAHDI_SFCONFIG)
		return -ENOTTY; /* Not supported yet */

	return dahdi_ioctl(file, cmd, data);
}
#endif

/**
 * dahdi_register() - unregister a new DAHDI span
 * @span:	the DAHDI span
 * @prefmaster:	will the new span be preferred as a master?
 *
 * Registers a span for usage with DAHDI. All the channel numbers in it
 * will get the lowest available channel numbers.
 *
 * If prefmaster is set to anything > 0, span will attempt to become the
 * master DAHDI span at registration time. If 0: it will only become
 * master if no other span is currently the master (i.e.: it is the
 * first one).
 */
int dahdi_register(struct dahdi_span *span, int prefmaster)
{
	int x;
	int res = 0;

	if (!span)
		return -EINVAL;

	if (!span->ops)
		return -EINVAL;

	if (!span->ops->owner)
		return -EINVAL;


	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags)) {
		module_printk(KERN_ERR, "Span %s already appears to be registered\n", span->name);
		return -EBUSY;
	}

	for (x = 1; x < maxspans; x++) {
		if (spans[x] == span) {
			module_printk(KERN_ERR, "Span %s already in list\n", span->name);
			return -EBUSY;
		}
	}

	for (x = 1; x < DAHDI_MAX_SPANS; x++) {
		if (!spans[x])
			break;
	}

	if (x < DAHDI_MAX_SPANS) {
		spans[x] = span;
		if (maxspans < x + 1)
			maxspans = x + 1;
	} else {
		module_printk(KERN_ERR, "Too many DAHDI spans registered\n");
		return -EBUSY;
	}

	set_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags);
	span->spanno = x;

	spin_lock_init(&span->lock);

	if (!span->deflaw) {
		module_printk(KERN_NOTICE, "Span %s didn't specify default law.  "
				"Assuming mulaw, please fix driver!\n", span->name);
		span->deflaw = DAHDI_LAW_MULAW;
	}

	for (x = 0; x < span->channels; x++) {
		span->chans[x]->span = span;
		res = dahdi_chan_reg(span->chans[x]);
		if (res) {
			for (x--; x >= 0; x--)
				dahdi_chan_unreg(span->chans[x]);
			goto unreg_channels;
		}
	}

#ifdef CONFIG_PROC_FS
	{
		char tempfile[17];
		snprintf(tempfile, sizeof(tempfile), "dahdi/%d", span->spanno);
		span->proc_entry = create_proc_read_entry(tempfile, 0444,
					NULL, dahdi_proc_read,
					(int *) (long) span->spanno);
	}
#endif

	for (x = 0; x < span->channels; x++) {
		if (span->chans[x]->channo < 250) {
			char chan_name[32];
			snprintf(chan_name, sizeof(chan_name), "dahdi!%d", 
					span->chans[x]->channo);
			CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, 
					span->chans[x]->channo), NULL, chan_name);
		}
	}

	if (debug) {
		module_printk(KERN_NOTICE, "Registered Span %d ('%s') with "
				"%d channels\n", span->spanno, span->name, span->channels);
	}

	if (!master || prefmaster) {
		master = span;
		if (debug) {
			module_printk(KERN_NOTICE, "Span ('%s') is new master\n", 
					span->name);
		}
	}

	return 0;

unreg_channels:
	spans[span->spanno] = NULL;
	return res;
}


/**
 * dahdi_unregister() - unregister a DAHDI span
 * @span:	the DAHDI span
 *
 * Unregisters a span that has been previously registered with
 * dahdi_register().
 */
int dahdi_unregister(struct dahdi_span *span)
{
	int x;
	int new_maxspans;
	static struct dahdi_span *new_master;

#ifdef CONFIG_PROC_FS
	char tempfile[17];
#endif /* CONFIG_PROC_FS */

	if (!test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags)) {
		module_printk(KERN_ERR, "Span %s does not appear to be registered\n", span->name);
		return -1;
	}
	/* Shutdown the span if it's running */
	if (span->flags & DAHDI_FLAG_RUNNING)
		if (span->ops->shutdown)
			span->ops->shutdown(span);

	if (spans[span->spanno] != span) {
		module_printk(KERN_ERR, "Span %s has spanno %d which is something else\n", span->name, span->spanno);
		return -1;
	}
	if (debug)
		module_printk(KERN_NOTICE, "Unregistering Span '%s' with %d channels\n", span->name, span->channels);
#ifdef CONFIG_PROC_FS
	snprintf(tempfile, sizeof(tempfile)-1, "dahdi/%d", span->spanno);
        remove_proc_entry(tempfile, NULL);
#endif /* CONFIG_PROC_FS */

	for (x = 0; x < span->channels; x++) {
		if (span->chans[x]->channo < 250)
			CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, span->chans[x]->channo));
	}

	spans[span->spanno] = NULL;
	span->spanno = 0;
	clear_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags);
	for (x=0;x<span->channels;x++)
		dahdi_chan_unreg(span->chans[x]);
	new_maxspans = 0;
	new_master = master; /* FIXME: locking */
	if (master == span)
		new_master = NULL;
	for (x=1;x<DAHDI_MAX_SPANS;x++) {
		if (spans[x]) {
			new_maxspans = x+1;
			if (!new_master)
				new_master = spans[x];
		}
	}
	maxspans = new_maxspans;
	if (master != new_master)
		if (debug)
			module_printk(KERN_NOTICE, "%s: Span ('%s') is new master\n", __FUNCTION__,
				      (new_master)? new_master->name: "no master");
	master = new_master;

	return 0;
}

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

	if ((!ms->confmute && !ms->dialing) || (ms->flags & DAHDI_FLAG_PSEUDO)) {
		/* Handle conferencing on non-clear channel and non-HDLC channels */
		struct dahdi_chan *const conf_chan = chans[ms->confna];
		switch(ms->confmode & DAHDI_CONF_MODE_MASK) {
		case DAHDI_CONF_NORMAL:
			/* Do nuffin */
			break;
		case DAHDI_CONF_MONITOR:	/* Monitor a channel's rx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO) break;
			/* Add monitored channel */
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO)
				ACSS(getlin, conf_chan->getlin);
			else
				ACSS(getlin, conf_chan->putlin);

			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITORTX: /* Monitor a channel's tx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO) break;
			/* Add monitored channel */
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO)
				ACSS(getlin, conf_chan->putlin);
			else
				ACSS(getlin, conf_chan->getlin);

			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITORBOTH: /* monitor a channel's rx and tx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO) break;
			ACSS(getlin, conf_chan->putlin);
			ACSS(getlin, conf_chan->getlin);
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);
			break;
		case DAHDI_CONF_MONITOR_RX_PREECHO:	/* Monitor a channel's rx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO)
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(getlin, conf_chan->flags & DAHDI_FLAG_PSEUDO ?
			     conf_chan->readchunkpreec : conf_chan->putlin);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);

			break;
		case DAHDI_CONF_MONITOR_TX_PREECHO: /* Monitor a channel's tx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO)
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(getlin, conf_chan->flags & DAHDI_FLAG_PSEUDO ?
			     conf_chan->putlin : conf_chan->readchunkpreec);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				txb[x] = DAHDI_LIN2X(getlin[x], ms);

			break;
		case DAHDI_CONF_MONITORBOTH_PREECHO: /* monitor a channel's rx and tx mode */
			  /* if a pseudo-channel, ignore */
			if (ms->flags & DAHDI_FLAG_PSEUDO)
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
			if (ms->flags & DAHDI_FLAG_PSEUDO) /* if pseudo-channel */
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
			/* Real digital monitoring, but still echo cancel if desired */
			if (!conf_chan)
				break;
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO) {
				if (ms->ec_state) {
					for (x=0;x<DAHDI_CHUNKSIZE;x++)
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
			for (x=0;x<DAHDI_CHUNKSIZE;x++)
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

static inline void __dahdi_getbuf_chunk(struct dahdi_chan *ss, unsigned char *txb)
{
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
							wake_up_interruptible(&ms->eventbufq);
						/* If we're only supposed to start when full, disable the transmitter */
						if ((ms->txbufpolicy == DAHDI_POLICY_WHEN_FULL) ||
							(ms->txbufpolicy == DAHDI_POLICY_HALF_FULL))
							ms->txdisable = 1;
					}
				} else {
					if (ms->outwritebuf == ms->inwritebuf) {
						ms->outwritebuf = oldbuf;
						if (ms->iomask & (DAHDI_IOMUX_WRITE | DAHDI_IOMUX_WRITEEMPTY))
							wake_up_interruptible(&ms->eventbufq);
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
				if (!(ms->flags & (DAHDI_FLAG_NETDEV | DAHDI_FLAG_PPP))) {
					wake_up_interruptible(&ms->writebufq);
					wake_up_interruptible(&ms->sel);
					if (ms->iomask & DAHDI_IOMUX_WRITE)
						wake_up_interruptible(&ms->eventbufq);
				}
				/* Transmit a flag if this is an HDLC channel */
				if (ms->flags & DAHDI_FLAG_HDLC)
					fasthdlc_tx_frame_nocheck(&ms->txhdlc);
#ifdef CONFIG_DAHDI_NET
				if (ms->flags & DAHDI_FLAG_NETDEV)
					netif_wake_queue(ztchan_to_dev(ms));
#endif
#ifdef CONFIG_DAHDI_PPP
				if (ms->flags & DAHDI_FLAG_PPP) {
					ms->do_ppp_wakeup = 1;
					tasklet_schedule(&ms->ppp_calls);
				}
#endif
			}
		} else if (ms->curtone && !(ms->flags & DAHDI_FLAG_PSEUDO)) {
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
			bytes = 0;
		} else {
			memset(txb, DAHDI_LIN2X(0, ms), bytes);	/* Lastly we use silence on telephony channels */
			bytes = 0;
		}
	}
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
		wake_up_interruptible(&chan->txstateq);
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
		wake_up_interruptible(&chan->txstateq);
		break;

	case DAHDI_TXSTATE_PREFLASH:
		/* Actually flash */
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_FLASH, chan->flashtime);
		break;

	case DAHDI_TXSTATE_FLASH:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->txstateq);
		break;

	case DAHDI_TXSTATE_DEBOUNCE:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		/* See if we've gone back on hook */
		if ((chan->rxhooksig == DAHDI_RXSIG_ONHOOK) && (chan->rxflashtime > 2))
			chan->itimerset = chan->itimer = chan->rxflashtime * DAHDI_CHUNKSIZE;
		wake_up_interruptible(&chan->txstateq);
		break;

	case DAHDI_TXSTATE_AFTERSTART:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_OFFHOOK, DAHDI_TXSTATE_OFFHOOK, 0);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->txstateq);
		break;

	case DAHDI_TXSTATE_KEWL:
		dahdi_rbs_sethook(chan, DAHDI_TXSIG_ONHOOK, DAHDI_TXSTATE_AFTERKEWL, DAHDI_AFTERKEWLTIME);
		if (chan->file && (chan->file->f_flags & O_NONBLOCK))
			__qevent(chan, DAHDI_EVENT_HOOKCOMPLETE);
		wake_up_interruptible(&chan->txstateq);
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
		wake_up_interruptible(&chan->txstateq);
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
		wake_up_interruptible(&chan->txstateq);
		break;

	case DAHDI_TXSTATE_PULSEAFTER:
		chan->txstate = DAHDI_TXSTATE_OFFHOOK;
		__do_dtmf(chan);
		wake_up_interruptible(&chan->txstateq);
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
}

static void process_echocan_events(struct dahdi_chan *chan)
{
	union dahdi_echocan_events events = chan->ec_state->events;

	if (events.CED_tx_detected) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_TX_CED_DETECTED);
		if (chan->ec_state) {
			if (chan->ec_state->status.mode == ECHO_MODE_ACTIVE)
				set_echocan_fax_mode(chan, chan->channo, "CED tx detected", 1);
			else
				module_printk(KERN_NOTICE, "Detected CED tone (tx) on channel %d\n", chan->channo);
		}
	}

	if (events.CED_rx_detected) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_RX_CED_DETECTED);
		if (chan->ec_state) {
			if (chan->ec_state->status.mode == ECHO_MODE_ACTIVE)
				set_echocan_fax_mode(chan, chan->channo, "CED rx detected", 1);
			else
				module_printk(KERN_NOTICE, "Detected CED tone (rx) on channel %d\n", chan->channo);
		}
	}

	if (events.CNG_tx_detected)
		dahdi_qevent_nolock(chan, DAHDI_EVENT_TX_CNG_DETECTED);

	if (events.CNG_rx_detected)
		dahdi_qevent_nolock(chan, DAHDI_EVENT_RX_CNG_DETECTED);

	if (events.NLP_auto_disabled) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_DISABLED);
		chan->ec_state->status.mode = ECHO_MODE_FAX;
	}

	if (events.NLP_auto_enabled) {
		dahdi_qevent_nolock(chan, DAHDI_EVENT_EC_NLP_ENABLED);
		chan->ec_state->status.mode = ECHO_MODE_ACTIVE;
	}
}

static inline void __dahdi_ec_chunk(struct dahdi_chan *ss, unsigned char *rxchunk, const unsigned char *txchunk)
{
	short rxlin, txlin;
	int x;
	unsigned long flags;

	spin_lock_irqsave(&ss->lock, flags);

	if (ss->readchunkpreec) {
		/* Save a copy of the audio before the echo can has its way with it */
		for (x = 0; x < DAHDI_CHUNKSIZE; x++)
			/* We only ever really need to deal with signed linear - let's just convert it now */
			ss->readchunkpreec[x] = DAHDI_XLAW(rxchunk[x], ss);
	}

	/* Perform echo cancellation on a chunk if necessary */
	if (ss->ec_state) {
#if defined(CONFIG_DAHDI_MMX) || defined(ECHO_CAN_FP)
		dahdi_kernel_fpu_begin();
#endif
		if (ss->ec_state->status.mode & __ECHO_MODE_MUTE) {
			/* Special stuff for training the echo can */
			for (x=0;x<DAHDI_CHUNKSIZE;x++) {
				rxlin = DAHDI_XLAW(rxchunk[x], ss);
				txlin = DAHDI_XLAW(txchunk[x], ss);
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
#if 0
						module_printk(KERN_NOTICE, "Finished training (%d taps trained)!\n", ss->ec_state->status.last_train_tap);
#endif
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
					rxlins[x] = DAHDI_XLAW(rxchunk[x], ss);
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
	spin_unlock_irqrestore(&ss->lock, flags);
}

/**
 * dahdi_ec_chunk() - process echo for a single channel
 * @ss:		DAHDI channel
 * @rxchunk:	chunk of audio on which to cancel echo
 * @txchunk:	reference chunk from the other direction
 *
 * The echo canceller function fixes received (from device to userspace)
 * audio. In order to fix it it uses the transmitted audio as a
 * reference. This call updates the echo canceller for a single chunk (8
 * bytes).
 */
void dahdi_ec_chunk(struct dahdi_chan *ss, unsigned char *rxchunk, const unsigned char *txchunk)
{
	__dahdi_ec_chunk(ss, rxchunk, txchunk);
}

/**
 * dahdi_ec_span() - process echo for all channels in a span.
 * @span:	DAHDI span
 *
 * Similar to calling dahdi_ec_chunk() for each of the channels in the
 * span. Uses dahdi_chunk.write_chunk for the rxchunk (the chunk to fix)
 * and dahdi_chan.readchunk as the txchunk (the reference chunk).
 */
void dahdi_ec_span(struct dahdi_span *span)
{
	int x;
	for (x = 0; x < span->channels; x++) {
		if (span->chans[x]->ec_current)
			__dahdi_ec_chunk(span->chans[x], span->chans[x]->readchunk, span->chans[x]->writechunk);
	}
}

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
	if (ms->afterdialingtimer && (!(ms->flags & DAHDI_FLAG_PSEUDO))) {
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

	if (!(ms->flags &  DAHDI_FLAG_PSEUDO)) {
		memcpy(ms->putlin, putlin, DAHDI_CHUNKSIZE * sizeof(short));
		memcpy(ms->putraw, rxb, DAHDI_CHUNKSIZE);
	}

	/* Take the rxc, twiddle it for conferencing if appropriate and put it
	   back */
	if ((!ms->confmute && !ms->afterdialingtimer) ||
	    (ms->flags & DAHDI_FLAG_PSEUDO)) {
		struct dahdi_chan *const conf_chan = chans[ms->confna];
		switch(ms->confmode & DAHDI_CONF_MODE_MASK) {
		case DAHDI_CONF_NORMAL:		/* Normal mode */
			/* Do nothing.  rx goes output */
			break;
		case DAHDI_CONF_MONITOR:		/* Monitor a channel's rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!(ms->flags & DAHDI_FLAG_PSEUDO)) break;
			/* Add monitored channel */
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO)
				ACSS(putlin, conf_chan->getlin);
			else
				ACSS(putlin, conf_chan->putlin);
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_MONITORTX:	/* Monitor a channel's tx mode */
			  /* if not a pseudo-channel, ignore */
			if (!(ms->flags & DAHDI_FLAG_PSEUDO)) break;
			/* Add monitored channel */
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO)
				ACSS(putlin, conf_chan->putlin);
			else
				ACSS(putlin, conf_chan->getlin);
			/* Convert back */
			for(x=0;x<DAHDI_CHUNKSIZE;x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);
			break;
		case DAHDI_CONF_MONITORBOTH:	/* Monitor a channel's tx and rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!(ms->flags & DAHDI_FLAG_PSEUDO)) break;
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
			if (!(ms->flags & DAHDI_FLAG_PSEUDO))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(putlin, conf_chan->flags & DAHDI_FLAG_PSEUDO ?
			     conf_chan->getlin : conf_chan->readchunkpreec);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);

			break;
		case DAHDI_CONF_MONITOR_TX_PREECHO:	/* Monitor a channel's tx mode */
			  /* if not a pseudo-channel, ignore */
			if (!(ms->flags & DAHDI_FLAG_PSEUDO))
				break;

			if (!conf_chan->readchunkpreec)
				break;

			/* Add monitored channel */
			ACSS(putlin, conf_chan->flags & DAHDI_FLAG_PSEUDO ?
			     conf_chan->readchunkpreec : conf_chan->getlin);
			for (x = 0; x < DAHDI_CHUNKSIZE; x++)
				rxb[x] = DAHDI_LIN2X(putlin[x], ms);

			break;
		case DAHDI_CONF_MONITORBOTH_PREECHO:	/* Monitor a channel's tx and rx mode */
			  /* if not a pseudo-channel, ignore */
			if (!(ms->flags & DAHDI_FLAG_PSEUDO))
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
			if (ms->flags & DAHDI_FLAG_PSEUDO) /* if a pseudo-channel */
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
			if (!(ms->flags & DAHDI_FLAG_PSEUDO)) break;
			/* Add monitored channel */
			if (conf_chan->flags & DAHDI_FLAG_PSEUDO)
				memcpy(rxb, conf_chan->getraw, DAHDI_CHUNKSIZE);
			else
				memcpy(rxb, conf_chan->putraw, DAHDI_CHUNKSIZE);
			break;
		}
	}
}

/* HDLC (or other) receiver buffer functions for read side */
static inline void __putbuf_chunk(struct dahdi_chan *ss, unsigned char *rxb, int bytes)
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
				if (ms->flags & (DAHDI_FLAG_NETDEV | DAHDI_FLAG_PPP)) {
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
							if (ms->flags & DAHDI_FLAG_NETDEV) {
								struct net_device_stats *stats = hdlc_stats(ms->hdlcnetdev->netdev);
								stats->rx_packets++;
								stats->rx_bytes += ms->readn[ms->inreadbuf];
							}
#endif

						} else {
#ifdef CONFIG_DAHDI_NET
							if (ms->flags & DAHDI_FLAG_NETDEV) {
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
								wake_up_interruptible(&ms->sel);
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
							wake_up_interruptible(&ms->readbufq);
							if (ms->iomask & DAHDI_IOMUX_READ)
								wake_up_interruptible(&ms->eventbufq);
						}
					}
				}
			}
			if (abort) {
				/* Start over reading frame */
				ms->readidx[ms->inreadbuf] = 0;
				ms->infcs = PPP_INITFCS;

#ifdef CONFIG_DAHDI_NET
				if (ms->flags & DAHDI_FLAG_NETDEV) {
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
#if 0
				module_printk(KERN_NOTICE, "torintr_receive: Aborted %d bytes of frame on %d\n", amt, ss->master);
#endif

			}
		} else /* No place to receive -- drop on the floor */
			break;
#ifdef CONFIG_DAHDI_NET
		if (skb && (ms->flags & DAHDI_FLAG_NETDEV))
#ifdef NEW_HDLC_INTERFACE
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
			skb->mac.raw = skb->data;
#else
			skb_reset_mac_header(skb);
#endif
			skb->dev = ztchan_to_dev(ms);
#ifdef DAHDI_HDLC_TYPE_TRANS
			skb->protocol = hdlc_type_trans(skb, ztchan_to_dev(ms));
#else
			skb->protocol = htons (ETH_P_HDLC);
#endif
			netif_rx(skb);
		}
#else
			hdlc_netif_rx(&ms->hdlcnetdev->netdev, skb);
#endif
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
}

static inline void __dahdi_putbuf_chunk(struct dahdi_chan *ss, unsigned char *rxb)
{
	__putbuf_chunk(ss, rxb, DAHDI_CHUNKSIZE);
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
	int res;
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
	res = left;
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

	if (!ss->rxdisable) {
		wake_up_interruptible(&ss->readbufq);
		wake_up_interruptible(&ss->sel);
		if (ss->iomask & DAHDI_IOMUX_READ)
			wake_up_interruptible(&ss->eventbufq);
	}
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
					wake_up_interruptible(&ss->eventbufq);
				/* If we're only supposed to start when full, disable the transmitter */
				if ((ss->txbufpolicy == DAHDI_POLICY_WHEN_FULL) || (ss->txbufpolicy == DAHDI_POLICY_HALF_FULL))
					ss->txdisable = 1;
				res = -1;
			}

			if (ss->inwritebuf < 0)
				ss->inwritebuf = oldbuf;

			if (!(ss->flags & (DAHDI_FLAG_NETDEV | DAHDI_FLAG_PPP))) {
				wake_up_interruptible(&ss->writebufq);
				wake_up_interruptible(&ss->sel);
				if ((ss->iomask & DAHDI_IOMUX_WRITE) && (res >= 0))
					wake_up_interruptible(&ss->eventbufq);
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
	unsigned long flags;
	struct dahdi_timer *cur;

	spin_lock_irqsave(&zaptimerlock, flags);

	list_for_each_entry(cur, &zaptimers, list) {
		if (cur->ms) {
			cur->pos -= DAHDI_CHUNKSIZE;
			if (cur->pos <= 0) {
				cur->tripped++;
				cur->pos = cur->ms;
				wake_up_interruptible(&cur->sel);
			}
		}
	}

	spin_unlock_irqrestore(&zaptimerlock, flags);
}

static unsigned int dahdi_timer_poll(struct file *file, struct poll_table_struct *wait_table)
{
	struct dahdi_timer *timer = file->private_data;
	unsigned long flags;
	int ret = 0;
	if (timer) {
		poll_wait(file, &timer->sel, wait_table);
		spin_lock_irqsave(&zaptimerlock, flags);
		if (timer->tripped || timer->ping)
			ret |= POLLPRI;
		spin_unlock_irqrestore(&zaptimerlock, flags);
	} else
		ret = -EINVAL;
	return ret;
}

/* device poll routine */
static unsigned int
dahdi_chan_poll(struct file *file, struct poll_table_struct *wait_table)
{

	struct dahdi_chan *const chan = file->private_data;
	int	ret;
	unsigned long flags;

	  /* do the poll wait */
	if (chan) {
		poll_wait(file, &chan->sel, wait_table);
		ret = 0; /* start with nothing to return */
		spin_lock_irqsave(&chan->lock, flags);
		   /* if at least 1 write buffer avail */
		if (chan->inwritebuf > -1) {
			ret |= POLLOUT | POLLWRNORM;
		}
		if ((chan->outreadbuf > -1) && !chan->rxdisable) {
			ret |= POLLIN | POLLRDNORM;
		}
		if (chan->eventoutidx != chan->eventinidx)
		   {
			/* Indicate an exception */
			ret |= POLLPRI;
		   }
		spin_unlock_irqrestore(&chan->lock, flags);
	} else
		ret = -EINVAL;
	return(ret);  /* return what we found */
}

static unsigned int dahdi_poll(struct file *file, struct poll_table_struct *wait_table)
{
	int unit = UNIT(file);

	if (!unit)
		return -EINVAL;

	if (unit == 250)
		return dahdi_transcode_fops->poll(file, wait_table);

	if (unit == 253)
		return dahdi_timer_poll(file, wait_table);

	if (unit == 254) {
		if (!file->private_data)
			return -EINVAL;
		return dahdi_chan_poll(file, wait_table);
	}
	if (unit == 255) {
		if (!file->private_data) {
			module_printk(KERN_NOTICE, "No pseudo channel structure to read?\n");
			return -EINVAL;
		}
		return dahdi_chan_poll(file, wait_table);
	}
	return dahdi_chan_poll(file, wait_table);
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

int dahdi_transmit(struct dahdi_span *span)
{
	int x,y,z;
	unsigned long flags;

	for (x=0;x<span->channels;x++) {
		struct dahdi_chan *const chan = span->chans[x];
		spin_lock_irqsave(&chan->lock, flags);
		if (chan->flags & DAHDI_FLAG_NOSTDTXRX) {
			spin_unlock_irqrestore(&chan->lock, flags);
			continue;
		}
		if (chan == chan->master) {
			if (chan->otimer) {
				chan->otimer -= DAHDI_CHUNKSIZE;
				if (chan->otimer <= 0)
					__rbs_otimer_expire(chan);
			}
			if (chan->flags & DAHDI_FLAG_AUDIO) {
				__dahdi_real_transmit(chan);
			} else {
				if (chan->nextslave) {
					u_char data[DAHDI_CHUNKSIZE];
					int pos=DAHDI_CHUNKSIZE;
					/* Process master/slaves one way */
					for (y=0;y<DAHDI_CHUNKSIZE;y++) {
						/* Process slaves for this byte too */
						z = x;
						do {
							if (pos==DAHDI_CHUNKSIZE) {
								/* Get next chunk */
								__dahdi_transmit_chunk(chan, data);
								pos = 0;
							}
							span->chans[z]->writechunk[y] = data[pos++];
							z = span->chans[z]->nextslave;
						} while(z);
					}
				} else {
					/* Process independents elsewise */
					__dahdi_real_transmit(chan);
				}
			}
			if (chan->sig == DAHDI_SIG_DACS_RBS) {
				struct dahdi_chan *const conf =
							chans[chan->confna];
				if (conf && (chan->txsig != conf->rxsig)) {
				    	/* Just set bits for our destination */
					chan->txsig = conf->rxsig;
					span->ops->rbsbits(chan, conf->rxsig);
				}
			}

		}
		spin_unlock_irqrestore(&chan->lock, flags);
	}
	if (span->mainttimer) {
		span->mainttimer -= DAHDI_CHUNKSIZE;
		if (span->mainttimer <= 0) {
			span->mainttimer = 0;
			if (span->ops->maint)
				span->ops->maint(span, DAHDI_MAINT_LOOPSTOP);
			span->maintstat = 0;
			wake_up_interruptible(&span->maintq);
		}
	}
	return 0;
}

static void process_masterspan(void)
{
	unsigned long flags;
	int x, y, z;
	struct dahdi_chan *chan;

#ifdef CONFIG_DAHDI_CORE_TIMER
	/* We increment the calls since start here, so that if we switch over
	 * to the core timer, we know how many times we need to call
	 * process_masterspan in order to catch up since this function needs
	 * to be called 1000 times per second. */
	atomic_inc(&core_timer.count);
#endif
	/* Hold the big zap lock for the duration of major
	   activities which touch all sorts of channels */
	spin_lock_irqsave(&bigzaplock, flags);
	read_lock(&chan_lock);
	/* Process any timers */
	process_timers();
	/* If we have dynamic stuff, call the ioctl with 0,0 parameters to
	   make it run */
	if (dahdi_dynamic_ioctl)
		dahdi_dynamic_ioctl(0, 0);

	for (x = 1; x < maxchans; x++) {
		chan = chans[x];
		if (chan && chan->confmode &&
		    !(chan->flags & DAHDI_FLAG_PSEUDO)) {
			u_char *data;
			spin_lock(&chan->lock);
			data = __buf_peek(&chan->confin);
			__dahdi_receive_chunk(chan, data);
			if (data) {
				__buf_pull(&chan->confin, NULL, chans[x]);
			}
			spin_unlock(&chan->lock);
		}
	}
	/* This is the master channel, so make things switch over */
	rotate_sums();
	/* do all the pseudo and/or conferenced channel receives (getbuf's) */
	for (x = 1; x < maxchans; x++) {
		chan = chans[x];
		if (chan && (chan->flags & DAHDI_FLAG_PSEUDO)) {
			spin_lock(&chan->lock);
			__dahdi_transmit_chunk(chan, NULL);
			spin_unlock(&chan->lock);
		}
	}
	if (maxlinks) {
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_begin();
#endif
		/* process all the conf links */
		for (x = 1; x <= maxlinks; x++) {
			/* if we have a destination conf */
			z = confalias[conf_links[x].dst];
			if (z) {
				y = confalias[conf_links[x].src];
				if (y)
					ACSS(conf_sums[z], conf_sums[y]);
			}
		}
#ifdef CONFIG_DAHDI_MMX
		dahdi_kernel_fpu_end();
#endif
	}
	/* do all the pseudo/conferenced channel transmits (putbuf's) */
	for (x = 1; x < maxchans; x++) {
		chan = chans[x];
		if (chan && (chan->flags & DAHDI_FLAG_PSEUDO)) {
			unsigned char tmp[DAHDI_CHUNKSIZE];
			spin_lock(&chan->lock);
			__dahdi_getempty(chan, tmp);
			__dahdi_receive_chunk(chan, tmp);
			spin_unlock(&chan->lock);
		}
	}
	for (x = 1; x < maxchans; x++) {
		chan = chans[x];
		if (chan && chan->confmode &&
		    !(chan->flags & DAHDI_FLAG_PSEUDO)) {
			u_char *data;
			spin_lock(&chan->lock);
			data = __buf_pushpeek(&chan->confout);
			__dahdi_transmit_chunk(chan, data);
			if (data)
				__buf_push(&chan->confout, NULL);
			spin_unlock(&chan->lock);
		}
	}
#ifdef	DAHDI_SYNC_TICK
	for (x = 0; x < maxspans; x++) {
		struct dahdi_span *const s = spans[x];
		if (s && s->ops->sync_tick)
			s->ops->sync_tick(s, s == master);
	}
#endif
	read_unlock(&chan_lock);
	spin_unlock_irqrestore(&bigzaplock, flags);
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

static void coretimer_func(unsigned long param)
{
	unsigned long ms_since_start;
	struct timespec now;
	const unsigned long MAX_INTERVAL = 100000L;
	const unsigned long FOURMS_INTERVAL = max(HZ/250, 1);
	const unsigned long ONESEC_INTERVAL = HZ;
	const unsigned long MS_LIMIT = 3000;

	now = current_kernel_time();

	if (atomic_read(&core_timer.count) ==
	    atomic_read(&core_timer.last_count)) {

		/* This is the code path if a board driver is not calling
		 * dahdi_receive, and therefore the core of dahdi needs to
		 * perform the master span processing itself. */

		if (!atomic_read(&core_timer.shutdown))
			mod_timer(&core_timer.timer, jiffies + FOURMS_INTERVAL);

		ms_since_start = core_diff_ms(&core_timer.start_interval, &now);

		/*
		 * If the system time has changed, it is possible for us to be
		 * far behind.  If we are more than MS_LIMIT milliseconds
		 * behind, just reset our time base and continue so that we do
		 * not hang the system here.
		 *
		 */
		if (unlikely((ms_since_start - atomic_read(&core_timer.count)) > MS_LIMIT)) {
			if (printk_ratelimit())
				module_printk(KERN_INFO, "Detected time shift.\n");
			atomic_set(&core_timer.count, 0);
			atomic_set(&core_timer.last_count, 0);
			core_timer.start_interval = now;
			return;
		}

		while (ms_since_start > atomic_read(&core_timer.count))
			process_masterspan();

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
	core_timer.timer.expires = jiffies + HZ;
	atomic_set(&core_timer.count, 0);
	atomic_set(&core_timer.shutdown, 0);
	add_timer(&core_timer.timer);
}

static void coretimer_cleanup(void)
{
	atomic_set(&core_timer.shutdown, 1);
	del_timer_sync(&core_timer.timer);
}

#endif /* CONFIG_DAHDI_CORE_TIMER */


int dahdi_receive(struct dahdi_span *span)
{
	int x,y,z;
	unsigned long flags;

#ifdef CONFIG_DAHDI_WATCHDOG
	span->watchcounter--;
#endif
	for (x=0;x<span->channels;x++) {
		struct dahdi_chan *const chan = span->chans[x];
		if (chan->master == chan) {
			spin_lock_irqsave(&chan->lock, flags);
			if (chan->nextslave) {
				/* Must process each slave at the same time */
				u_char data[DAHDI_CHUNKSIZE];
				int pos = 0;
				for (y=0;y<DAHDI_CHUNKSIZE;y++) {
					/* Put all its slaves, too */
					z = x;
					do {
						data[pos++] = span->chans[z]->readchunk[y];
						if (pos == DAHDI_CHUNKSIZE) {
							if (!(chan->flags & DAHDI_FLAG_NOSTDTXRX))
								__dahdi_receive_chunk(chan, data);
							pos = 0;
						}
						z=span->chans[z]->nextslave;
					} while(z);
				}
			} else {
				/* Process a normal channel */
				if (!(chan->flags & DAHDI_FLAG_NOSTDTXRX))
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
			if (chan->pulsetimer)
			{
				chan->pulsetimer--;
				if (chan->pulsetimer <= 0)
				{
					if (chan->pulsecount)
					{
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
			spin_unlock_irqrestore(&chan->lock, flags);
		}
	}

	if (span == master)
		process_masterspan();

	return 0;
}

MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_DESCRIPTION("DAHDI Telephony Interface");
MODULE_LICENSE("GPL v2");
/* DAHDI now provides timing. If anybody wants dahdi_dummy it's probably
 * for that. So make dahdi provide it for now. This alias may be removed
 * in the future, and users are encouraged not to rely on it. */
MODULE_ALIAS("dahdi_dummy");
MODULE_VERSION(DAHDI_VERSION);

module_param(debug, int, 0644);
module_param(deftaps, int, 0644);

static struct file_operations dahdi_fops = {
	.owner   = THIS_MODULE,
	.open    = dahdi_open,
	.release = dahdi_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl  = dahdi_ioctl,
#ifdef HAVE_COMPAT_IOCTL
	.compat_ioctl = dahdi_ioctl_compat,
#endif
#else
	.ioctl   = dahdi_ioctl,
#endif
	.read    = dahdi_read,
	.write   = dahdi_write,
	.poll    = dahdi_poll,
};

#ifdef CONFIG_DAHDI_WATCHDOG
static struct timer_list watchdogtimer;

static void watchdog_check(unsigned long ignored)
{
	int x;
	unsigned long flags;
	static int wdcheck=0;

	local_irq_save(flags);
	for (x=0;x<maxspans;x++) {
		s = spans[x];
		if (s && (s->flags & DAHDI_FLAG_RUNNING)) {
			if (s->watchcounter == DAHDI_WATCHDOG_INIT) {
				/* Whoops, dead card */
				if ((s->watchstate == DAHDI_WATCHSTATE_OK) ||
					(s->watchstate == DAHDI_WATCHSTATE_UNKNOWN)) {
					s->watchstate = DAHDI_WATCHSTATE_RECOVERING;
					if (s->watchdog) {
						module_printk(KERN_NOTICE, "Kicking span %s\n", s->name);
						s->watchdog(spans[x], DAHDI_WATCHDOG_NOINTS);
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
	local_irq_restore(flags);
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

int dahdi_register_chardev(struct dahdi_chardev *dev)
{
	static const char *DAHDI_STRING = "dahdi!";
	char *udevname;

	udevname = kzalloc(strlen(dev->name) + sizeof(DAHDI_STRING) + 1,
			   GFP_KERNEL);
	if (!udevname)
		return -ENOMEM;

	strcpy(udevname, DAHDI_STRING);
	strcat(udevname, dev->name);
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, dev->minor), NULL, udevname);
	kfree(udevname);
	return 0;
}

int dahdi_unregister_chardev(struct dahdi_chardev *dev)
{
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, dev->minor));

	return 0;
}

static int __init dahdi_init(void)
{
	int res = 0;

#ifdef CONFIG_PROC_FS
	root_proc_entry = proc_mkdir("dahdi", NULL);
#endif

	if ((res = register_chrdev(DAHDI_MAJOR, "dahdi", &dahdi_fops))) {
		module_printk(KERN_ERR, "Unable to register DAHDI character device handler on %d\n", DAHDI_MAJOR);
		return res;
	}

	dahdi_class = class_create(THIS_MODULE, "dahdi");
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, 253), NULL, "dahdi!timer");
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, 254), NULL, "dahdi!channel");
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, 255), NULL, "dahdi!pseudo");
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, 0), NULL, "dahdi!ctl");

	module_printk(KERN_INFO, "Telephony Interface Registered on major %d\n", DAHDI_MAJOR);
	module_printk(KERN_INFO, "Version: %s\n", DAHDI_VERSION);
	dahdi_conv_init();
	fasthdlc_precalc();
	rotate_sums();
#ifdef CONFIG_DAHDI_WATCHDOG
	watchdog_init();
#endif
	coretimer_init();
	return res;
}

static void __exit dahdi_cleanup(void)
{
	int x;

	coretimer_cleanup();

	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, 253)); /* timer */
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, 254)); /* channel */
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, 255)); /* pseudo */
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, 0)); /* ctl */
	class_destroy(dahdi_class);

	unregister_chrdev(DAHDI_MAJOR, "dahdi");

#ifdef CONFIG_PROC_FS
	remove_proc_entry("dahdi", NULL);
#endif

	module_printk(KERN_INFO, "Telephony Interface Unloaded\n");
	for (x = 0; x < DAHDI_TONE_ZONE_MAX; x++) {
		if (tone_zones[x])
			kfree(tone_zones[x]);
	}

#ifdef CONFIG_DAHDI_WATCHDOG
	watchdog_cleanup();
#endif
}

module_init(dahdi_init);
module_exit(dahdi_cleanup);
