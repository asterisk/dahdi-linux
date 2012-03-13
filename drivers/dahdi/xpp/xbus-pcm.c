/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2007, Xorcom
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
#include <linux/module.h>
#include "xbus-pcm.h"
#include "xbus-core.h"
#include "xpp_dahdi.h"
#include "dahdi_debug.h"
#include "parport_debug.h"

static const char rcsid[] = "$Id$";

extern int debug;
#ifdef	OPTIMIZE_CHANMUTE
static DEF_PARM_BOOL(optimize_chanmute, 1, 0644,
		     "Optimize by muting inactive channels");
#endif

static DEF_PARM(int, disable_pcm, 0, 0644, "Disable all PCM transmissions");
#ifdef	DEBUG_PCMTX
DEF_PARM(int, pcmtx, -1, 0644,
	 "Forced PCM value to transmit (negative to disable)");
EXPORT_SYMBOL(pcmtx);
DEF_PARM(int, pcmtx_chan, 0, 0644, "channel to force PCM value");
EXPORT_SYMBOL(pcmtx_chan);
#endif
static DEF_PARM_BOOL(disable_pll_sync, 0, 0644,
		     "Disable automatic adjustment of AB clocks");

static xbus_t *syncer;		/* current syncer */
static atomic_t xpp_tick_counter = ATOMIC_INIT(0);
static struct xpp_ticker dahdi_ticker;
/*
 * The ref_ticker points to the current referece tick source.
 * I.e: one of our AB or dahdi_ticker
 */
static struct xpp_ticker *ref_ticker;
static DEFINE_SPINLOCK(ref_ticker_lock);
static DEFINE_SPINLOCK(elect_syncer_lock);
/* from /sys/bus/astribanks/drivers/xppdrv/sync */
static bool force_dahdi_sync;
static xbus_t *global_ticker;
static struct xpp_ticker global_ticks_series;

#define	PROC_SYNC		"sync"
/* Sampling cycle in usec */
#define SYNC_CYCLE		500
/* Samples from end of SYNC_CYCLE */
#define SYNC_CYCLE_SAMPLE	100
/* Number of SYNC_CYCLE's to converge speed */
#define SYNC_CONVERGE		10
/* Offset from ref_ticker to other AB's */
#define SYNC_CENTER		500
/* If within +/-SYNC_DELTA, try to stay there */
#define SYNC_DELTA		40
#define	BIG_TICK_INTERVAL	1000
/* maximal firmware drift unit (hardware limit 63) */
#define	SYNC_ADJ_MAX		20

/*
 * The USB bulk endpoints have a large jitter in the timing of frames
 * from the AB to the ehci-hcd. This is because we cannot predict
 * in which USB micro-frame our data passes. Each micro-frame is
 * A 125 usec.
 */
#define	SYNC_ADJ_QUICK	1000
#define	SYNC_ADJ_SLOW	10000

#ifdef	DAHDI_SYNC_TICK
static unsigned int dahdi_tick_count;
#endif

/*------------------------- SYNC Handling --------------------------*/

static void send_drift(xbus_t *xbus, int drift);

static void ticker_set_cycle(struct xpp_ticker *ticker, int cycle)
{
	unsigned long flags;

	spin_lock_irqsave(&ticker->lock, flags);
	if (cycle < SYNC_ADJ_QUICK)
		cycle = SYNC_ADJ_QUICK;
	if (cycle > SYNC_ADJ_SLOW)
		cycle = SYNC_ADJ_SLOW;
	ticker->cycle = cycle;
	spin_unlock_irqrestore(&ticker->lock, flags);
}

static void xpp_ticker_init(struct xpp_ticker *ticker)
{
	memset(ticker, 0, sizeof(*ticker));
	spin_lock_init(&ticker->lock);
	do_gettimeofday(&ticker->last_sample.tv);
	ticker->first_sample = ticker->last_sample;
	ticker_set_cycle(ticker, SYNC_ADJ_QUICK);
}

static int xpp_ticker_step(struct xpp_ticker *ticker, const struct timeval *t)
{
	unsigned long flags;
	long usec;
	bool cycled = 0;

	spin_lock_irqsave(&ticker->lock, flags);
	ticker->last_sample.tv = *t;
	/* rate adjust */
	if ((ticker->count % ticker->cycle) == ticker->cycle - 1) {
		usec =
		    (long)usec_diff(&ticker->last_sample.tv,
				    &ticker->first_sample.tv);
		ticker->first_sample = ticker->last_sample;
		ticker->tick_period = usec / ticker->cycle;
		cycled = 1;
	}
	ticker->count++;
	spin_unlock_irqrestore(&ticker->lock, flags);
	return cycled;
}

/*
 * No locking. It is called only from:
 *   - update_sync_master() in a globall spinlock protected code.
 *   - initalization.
 */
static inline void xbus_drift_clear(xbus_t *xbus)
{
	struct xpp_drift *di = &xbus->drift;

	do_gettimeofday(&di->last_lost_tick.tv);
	ticker_set_cycle(&xbus->ticker, SYNC_ADJ_QUICK);
	di->max_speed = -SYNC_ADJ_MAX;
	di->min_speed = SYNC_ADJ_MAX;
}

void xpp_drift_init(xbus_t *xbus)
{
	memset(&xbus->drift, 0, sizeof(xbus->drift));
	spin_lock_init(&xbus->drift.lock);
	xpp_ticker_init(&xbus->ticker);
	xbus_drift_clear(xbus);
}

#ifdef	SAMPLE_TICKS
static void sample_tick(xbus_t *xbus, int sample)
{
	if (!xbus->sample_running)
		return;
	if (xbus->sample_pos < SAMPLE_SIZE)
		xbus->sample_ticks[xbus->sample_pos++] = sample;
	else {
		xbus->sample_running = 0;
		xbus->sample_pos = 0;
	}
}
#else
#define	sample_tick(x, y)
#endif

/*
 * The following function adjust the clock of an astribank according
 * to our reference clock (another astribank or another DAHDI device).
 *
 * It is VERY hard to stabilise these corrections:
 *    - The measurments are affected by 125usec USB micro-frames.
 *    - However, if we are off by more than +/-500usec we are risking
 *      loosing frames.
 *
 * Every change must be tested rigorously with many device combinations
 * including abrupt changes in sync -- to verify the stability of the
 * algorithm.
 */
static void xpp_drift_step(xbus_t *xbus, const struct timeval *tv)
{
	struct xpp_drift *di = &xbus->drift;
	struct xpp_ticker *ticker = &xbus->ticker;
	unsigned long flags;

	spin_lock_irqsave(&di->lock, flags);
	xpp_ticker_step(&xbus->ticker, tv);
	/*
	 * Do we need to be synchronized and is there an established reference
	 * ticker (another Astribank or another DAHDI device) already?
	 */
	if (ref_ticker && ref_ticker != &xbus->ticker && syncer
	    && xbus->sync_mode == SYNC_MODE_PLL) {
		int new_delta_tick = ticker->count - ref_ticker->count;
		int lost_ticks = new_delta_tick - di->delta_tick;
		long usec_delta;

		di->delta_tick = new_delta_tick;
		if (lost_ticks) {
			static int rate_limit;

			/*
			 * We just lost some ticks. Just report it and don't
			 * try to adjust anything until we are stable again.
			 */
			di->lost_ticks++;
			di->lost_tick_count += abs(lost_ticks);
			if ((rate_limit++ % 1003) == 0) {
				/* FIXME: This should be a NOTICE.
				 * However we have several false ones at
				 * startup.
				 */
				XBUS_DBG(SYNC, xbus, "Lost %d tick%s\n",
					 lost_ticks,
					 (abs(lost_ticks) > 1) ? "s" : "");
			}
			if (abs(lost_ticks) > 100) {
				xbus_drift_clear(xbus);
				ticker->count = ref_ticker->count;
			}
		} else {
			/* Sample a delta */
			usec_delta =
			    (long)usec_diff(&ticker->last_sample.tv,
					    &ref_ticker->last_sample.tv);
			sample_tick(xbus, usec_delta);
			if ((ticker->count % SYNC_CYCLE) >
			    (SYNC_CYCLE - SYNC_CYCLE_SAMPLE))
				di->delta_sum += usec_delta;

			if ((ticker->count % SYNC_CYCLE) == 0) {
				/*
				 * Full sampling cycle passed. Let's calculate
				 */
				int offset =
				    di->delta_sum / SYNC_CYCLE_SAMPLE -
				    SYNC_CENTER;
				int offset_prev = di->offset_prev;
				int speed = xbus->sync_adjustment;
				int fix = 0;
				int best_speed =
				    (di->max_speed + di->min_speed) >> 1;

				if (offset > 0 && offset < SYNC_DELTA) {
					speed = best_speed - 1;
				} else if (offset < 0 && offset > -SYNC_DELTA) {
					speed = best_speed + 1;
				} else {
					if (offset > 0) {
						if (offset > offset_prev)
							fix--;
					} else {
						if (offset < offset_prev)
							fix++;
					}
					speed += fix;
				}
				if (speed < -SYNC_ADJ_MAX)
					speed = -SYNC_ADJ_MAX;
				if (speed > SYNC_ADJ_MAX)
					speed = SYNC_ADJ_MAX;
				if (speed < di->min_speed)
					di->min_speed = speed;
				if (speed > di->max_speed)
					di->max_speed = speed;
				if (offset > di->offset_max)
					di->offset_max = offset;
				if (offset < di->offset_min)
					di->offset_min = offset;

				XBUS_DBG(SYNC, xbus,
					"offset: %d, min_speed=%d, "
					"max_speed=%d, usec_delta(last)=%ld\n",
					offset_prev, di->min_speed,
					di->max_speed, usec_delta);
				XBUS_DBG(SYNC, xbus,
					"ADJ: speed=%d (best_speed=%d) fix=%d\n",
					speed, best_speed, fix);
				xbus->sync_adjustment_offset = speed;
				if (xbus != syncer
				    && xbus->sync_adjustment != speed)
					send_drift(xbus, speed);
				di->sync_inaccuracy =
				    abs(offset) + abs(di->offset_range) / 2;
				if (ticker->count >= SYNC_CYCLE * SYNC_CONVERGE) {
					di->offset_range =
					    di->offset_max - di->offset_min;
					di->offset_min = INT_MAX;
					di->offset_max = -INT_MAX;
					if (di->max_speed > best_speed)
						di->max_speed--;
					if (di->min_speed < best_speed)
						di->min_speed++;
				}
				di->offset_prev = offset;
				di->delta_sum = 0;
			}
		}
	}
	spin_unlock_irqrestore(&di->lock, flags);
}

const char *sync_mode_name(enum sync_mode mode)
{
	static const char *sync_mode_names[] = {
		[SYNC_MODE_AB] = "AB",
		[SYNC_MODE_NONE] = "NONE",
		[SYNC_MODE_PLL] = "PLL",
		[SYNC_MODE_QUERY] = "QUERY",
	};
	if (mode >= ARRAY_SIZE(sync_mode_names))
		return NULL;
	return sync_mode_names[mode];
}
EXPORT_SYMBOL(sync_mode_name);

/*
 * Caller must aquire/release the 'ref_ticker_lock' spinlock
 */
static void xpp_set_syncer(xbus_t *xbus, bool on)
{
	if (!xbus) {		/* Special case, no more syncers */
		DBG(SYNC, "No more syncers\n");
		syncer = NULL;
		if (ref_ticker != &dahdi_ticker)
			ref_ticker = NULL;
		return;
	}
	if (syncer != xbus && on) {
		XBUS_DBG(SYNC, xbus, "New syncer\n");
		syncer = xbus;
	} else if (syncer == xbus && !on) {
		XBUS_DBG(SYNC, xbus, "Lost syncer\n");
		syncer = NULL;
		if (ref_ticker != &dahdi_ticker)
			ref_ticker = NULL;
	} else
		XBUS_DBG(SYNC, xbus, "ignore %s (current syncer: %s)\n",
			 (on) ? "ON" : "OFF",
			 (syncer) ? syncer->busname : "NO-SYNC");
}

static void xbus_command_timer(unsigned long param)
{
	xbus_t *xbus = (xbus_t *)param;
	struct timeval now;

	BUG_ON(!xbus);
	do_gettimeofday(&now);
	xbus_command_queue_tick(xbus);
	if (!xbus->self_ticking) /* Must be 1KHz rate */
		mod_timer(&xbus->command_timer, jiffies + 1);
}

void xbus_set_command_timer(xbus_t *xbus, bool on)
{
	XBUS_DBG(SYNC, xbus, "%s\n", (on) ? "ON" : "OFF");
	if (on) {
		if (!timer_pending(&xbus->command_timer)) {
			XBUS_DBG(SYNC, xbus, "add_timer\n");
			xbus->command_timer.function = xbus_command_timer;
			xbus->command_timer.data = (unsigned long)xbus;
			xbus->command_timer.expires = jiffies + 1;
			add_timer(&xbus->command_timer);
		}
	} else if (timer_pending(&xbus->command_timer)) {
		XBUS_DBG(SYNC, xbus, "del_timer\n");
		del_timer(&xbus->command_timer);
	}
	xbus->self_ticking = !on;
}

/*
 * Called when the Astribank replies to a sync change request
 */
void got_new_syncer(xbus_t *xbus, enum sync_mode mode, int drift)
{
	unsigned long flags;
	unsigned long flags2;

	spin_lock_irqsave(&xbus->lock, flags);
	spin_lock_irqsave(&ref_ticker_lock, flags2);
	xbus->sync_adjustment = (signed char)drift;
	if (xbus->sync_mode == mode) {
		XBUS_DBG(SYNC, xbus, "Already in mode '%s'. Ignored\n",
			 sync_mode_name(mode));
		goto out;
	}
	XBUS_DBG(SYNC, xbus, "Mode %s (%d), drift=%d (pcm_rx_counter=%d)\n",
		 sync_mode_name(mode), mode, drift,
		 atomic_read(&xbus->pcm_rx_counter));
	switch (mode) {
	case SYNC_MODE_AB:
		xbus->sync_mode = mode;
		xbus_set_command_timer(xbus, 0);
		xpp_set_syncer(xbus, 1);
		global_ticker = xbus;
		break;
	case SYNC_MODE_PLL:
		xbus->sync_mode = mode;
		xbus_set_command_timer(xbus, 0);
		xpp_set_syncer(xbus, 0);
		global_ticker = xbus;
		break;
	case SYNC_MODE_NONE:	/* lost sync source */
		xbus->sync_mode = mode;
		xbus_set_command_timer(xbus, 1);
		xpp_set_syncer(xbus, 0);
		break;
	case SYNC_MODE_QUERY:	/* ignore           */
		break;
	default:
		XBUS_ERR(xbus, "%s: unknown mode=0x%X\n", __func__, mode);
	}
out:
	spin_unlock_irqrestore(&ref_ticker_lock, flags2);
	spin_unlock_irqrestore(&xbus->lock, flags);
}
EXPORT_SYMBOL(got_new_syncer);

void xbus_request_sync(xbus_t *xbus, enum sync_mode mode)
{
	unsigned long flags;

	BUG_ON(!xbus);
	XBUS_DBG(SYNC, xbus, "sent request (mode=%d)\n", mode);
	CALL_PROTO(GLOBAL, SYNC_SOURCE, xbus, NULL, mode, 0);
	if (mode == SYNC_MODE_NONE) {
		/*
		 * We must deselect the syncer *now* and not wait for the
		 * reply from the AB. Otherwise, a disconnect of the syncing
		 * AB would result in a corrupted 'syncer'.
		 */
		spin_lock_irqsave(&ref_ticker_lock, flags);
		xpp_set_syncer(xbus, 0);
		spin_unlock_irqrestore(&ref_ticker_lock, flags);
		/*
		 * We must activate timer now since the commands go to
		 * the command queue, and we should maintain something to
		 * "tick" meanwhile until the AB get these commands and
		 * start being "self_ticking".
		 */
		xbus_set_command_timer(xbus, 1);
	}
}
EXPORT_SYMBOL(xbus_request_sync);

static void reset_sync_counters(void)
{
	int i;

	//DBG(SYNC, "%d\n", atomic_read(&xpp_tick_counter));
	for (i = 0; i < MAX_BUSES; i++) {
		xbus_t *xbus = get_xbus(__func__, i);

		if (xbus == NULL)
			continue;
		/*
		 * Don't send to non self_ticking Astribanks:
		 *  - Maybe they didn't finish initialization
		 *  - Or maybe they didn't answer us in the first place
		 (e.g: wrong firmware version, etc).
		 */
		if (xbus->self_ticking) {
			if (!XBUS_FLAGS(xbus, CONNECTED)) {
				XBUS_DBG(GENERAL, xbus,
					 "Dropped packet. Is shutting down.\n");
			} else {
				/* Reset sync LEDs once in a while */
				CALL_PROTO(GLOBAL, RESET_SYNC_COUNTERS, xbus,
					   NULL);
			}
		}
		put_xbus(__func__, xbus);
	}
}

static void send_drift(xbus_t *xbus, int drift)
{
	struct timeval now;
	const char *msg;

	BUG_ON(abs(drift) > SYNC_ADJ_MAX);
	do_gettimeofday(&now);
	if (drift > xbus->sync_adjustment)
		msg = "up";
	else
		msg = "down";
	XBUS_DBG(SYNC, xbus,
		 "%sDRIFT adjust %s (%d) (last update %ld seconds ago)\n",
		 (disable_pll_sync) ? "Fake " : "", msg, drift,
		 now.tv_sec - xbus->pll_updated_at);
	if (!disable_pll_sync)
		CALL_PROTO(GLOBAL, SYNC_SOURCE, xbus, NULL, SYNC_MODE_PLL,
			   drift);
	xbus->pll_updated_at = now.tv_sec;
}

static void global_tick(void)
{
	struct timeval now;

	do_gettimeofday(&now);
	atomic_inc(&xpp_tick_counter);
	if ((atomic_read(&xpp_tick_counter) % BIG_TICK_INTERVAL) == 0)
		reset_sync_counters();
	xpp_ticker_step(&global_ticks_series, &now);
}

#ifdef	DAHDI_SYNC_TICK
void dahdi_sync_tick(struct dahdi_span *span, int is_master)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	static int redundant_ticks;	/* for extra spans */
	struct timeval now;

	if (!force_dahdi_sync)
		goto noop;
	do_gettimeofday(&now);
	BUG_ON(!xpd);
	/*
	 * Detect if any of our spans is dahdi sync master
	 */
	if (is_master) {
		static int rate_limit;

		if ((rate_limit++ % 10003) == 0)
			XPD_NOTICE(xpd,
				"Is a DAHDI sync master: "
				"ignore sync from DAHDI\n");
		goto noop;
	}
	/* Now we know for sure someone else is dahdi sync master */
	if (syncer) {
		static int rate_limit;

		if ((rate_limit++ % 5003) == 0)
			XBUS_DBG(SYNC, syncer,
				 "is a SYNCer: ignore sync from DAHDI\n");
		goto noop;
	}
	/* ignore duplicate calls from all our registered spans */
	if ((redundant_ticks++ % total_registered_spans()) != 0) {
#if 0
		static int rate_limit;

		if ((rate_limit++ % 1003) < 16)
			XPD_NOTICE(xpd, "boop (%d)\n", dahdi_tick_count);
#endif
		goto noop;
	}
	xpp_ticker_step(&dahdi_ticker, &now);
	dahdi_tick_count++;
	//flip_parport_bit(1);
noop:
	return;
}
EXPORT_SYMBOL(dahdi_sync_tick);
#endif

/*
 * called from elect_syncer()
 * if new_syncer is NULL, than we move all to SYNC_MODE_PLL
 * for DAHDI sync.
 */
static void update_sync_master(xbus_t *new_syncer, bool force_dahdi)
{
	const char *msg;
	int i;
	unsigned long flags;

	WARN_ON(new_syncer && force_dahdi);	/* Ambigous */
	force_dahdi_sync = force_dahdi;
	msg = (force_dahdi_sync) ? "DAHDI" : "NO-SYNC";
	DBG(SYNC, "%s => %s\n", (syncer) ? syncer->busname : msg,
	    (new_syncer) ? new_syncer->busname : msg);
	/*
	 * This global locking protects:
	 *   - The ref_ticker so it won't be used while we change it.
	 *   - The xbus_drift_clear() from corrupting driftinfo data.
	 * It's important to set ref_ticker now:
	 *   - We cannot make the new xbus a syncer yet (until we get
	 *     a reply from AB). Maybe it's still not self_ticking, so
	 *     we must keep the timer for the command_queue to function.
	 *   - However, we must not send drift commands to it, because
	 *     they'll revert it to PLL instead of AB.
	 */
	spin_lock_irqsave(&ref_ticker_lock, flags);
	if (syncer)
		xbus_drift_clear(syncer);	/* Clean old data */
	if (new_syncer) {
		XBUS_DBG(SYNC, new_syncer, "pcm_rx_counter=%d\n",
			 atomic_read(&new_syncer->pcm_rx_counter));
		force_dahdi_sync = 0;
		ref_ticker = &new_syncer->ticker;
		xbus_drift_clear(new_syncer);	/* Clean new data */
		xpp_set_syncer(new_syncer, 1);
		xbus_request_sync(new_syncer, SYNC_MODE_AB);
	} else if (force_dahdi_sync) {
		ref_ticker = &dahdi_ticker;
	} else {
		ref_ticker = NULL;
	}
	spin_unlock_irqrestore(&ref_ticker_lock, flags);
	DBG(SYNC, "stop unwanted syncers\n");
	/* Shut all down except the wanted sync master */
	for (i = 0; i < MAX_BUSES; i++) {
		xbus_t *xbus = get_xbus(__func__, i);
		if (xbus == NULL)
			continue;
		if (XBUS_FLAGS(xbus, CONNECTED) && xbus != new_syncer) {
			if (xbus->self_ticking)
				xbus_request_sync(xbus,
						  xbus->sync_mode_default);
			else
				XBUS_DBG(SYNC, xbus,
					 "Not self_ticking yet. Ignore\n");
		}
		put_xbus(__func__, xbus);
	}
}

void elect_syncer(const char *msg)
{
	int i;
	int j;
	uint timing_priority = INT_MAX;
	xpd_t *best_xpd = NULL;
	xbus_t *the_xbus = NULL;
	unsigned long flags;

	spin_lock_irqsave(&elect_syncer_lock, flags);
	DBG(SYNC, "%s: %s syncer=%s\n", __func__, msg,
	    (syncer) ? syncer->busname : "NULL");
	for (i = 0; i < MAX_BUSES; i++) {
		xbus_t *xbus = get_xbus(__func__, i);

		if (xbus == NULL)
			continue;
		if (XBUS_IS(xbus, READY)) {
			if (!the_xbus)
				the_xbus = xbus;	/* First candidate */
			for (j = 0; j < MAX_XPDS; j++) {
				xpd_t *xpd = xpd_of(xbus, j);
				int prio;

				if (!xpd || !xpd->card_present
				    || !IS_PHONEDEV(xpd))
					continue;
				prio =
				    CALL_PHONE_METHOD(card_timing_priority,
						      xpd);
				if (prio < 0) {
					DBG(SYNC, "%s/%s: skip sync\n",
					    xbus->busname, xpd->xpdname);
					continue;
				}
				if (prio > 0 && prio < timing_priority) {
					timing_priority = prio;
					best_xpd = xpd;
				}
			}
		}
		put_xbus(__func__, xbus);
	}
	if (best_xpd) {
		the_xbus = best_xpd->xbus;
		XPD_DBG(SYNC, best_xpd, "%s: elected with priority %d\n", msg,
			timing_priority);
	} else if (the_xbus) {
		XBUS_DBG(SYNC, the_xbus, "%s: elected\n", msg);
	} else {
		unsigned long flags;

		DBG(SYNC, "%s: No more syncers\n", msg);
		spin_lock_irqsave(&ref_ticker_lock, flags);
		xpp_set_syncer(NULL, 0);
		spin_unlock_irqrestore(&ref_ticker_lock, flags);
		the_xbus = NULL;
	}
	if (the_xbus != syncer)
		update_sync_master(the_xbus, force_dahdi_sync);
	spin_unlock_irqrestore(&elect_syncer_lock, flags);
}
EXPORT_SYMBOL(elect_syncer);

/*
 * This function should be called with the xpd already locked
 */
void update_wanted_pcm_mask(xpd_t *xpd, xpp_line_t new_mask, uint new_pcm_len)
{
	PHONEDEV(xpd).pcm_len = new_pcm_len;
	PHONEDEV(xpd).wanted_pcm_mask = new_mask;
	XPD_DBG(SIGNAL, xpd, "pcm_len=%d wanted_pcm_mask=0x%X\n",
		PHONEDEV(xpd).pcm_len, PHONEDEV(xpd).wanted_pcm_mask);
}
EXPORT_SYMBOL(update_wanted_pcm_mask);

/*
 * This function is used by FXS/FXO. The pcm_mask argument signifies
 * channels which should be *added* to the automatic calculation.
 * Normally, this argument is 0.
 */
void generic_card_pcm_recompute(xpd_t *xpd, xpp_line_t pcm_mask)
{
	int i;
	int line_count = 0;
	unsigned long flags;
	uint pcm_len;

	spin_lock_irqsave(&PHONEDEV(xpd).lock_recompute_pcm, flags);
	//XPD_DBG(SIGNAL, xpd, "pcm_mask=0x%X\n", pcm_mask);
	/* Add/remove all the trivial cases */
	pcm_mask |= PHONEDEV(xpd).offhook_state;
	pcm_mask |= PHONEDEV(xpd).oht_pcm_pass;
	pcm_mask &= ~(PHONEDEV(xpd).digital_inputs);
	pcm_mask &= ~(PHONEDEV(xpd).digital_outputs);
	for_each_line(xpd, i)
	    if (IS_SET(pcm_mask, i))
		line_count++;
	/*
	 * FIXME: Workaround a bug in sync code of the Astribank.
	 *        Send dummy PCM for sync.
	 */
	if (xpd->addr.unit == 0 && pcm_mask == 0) {
		pcm_mask = BIT(0);
		line_count = 1;
	}
	pcm_len = (line_count)
	    ? RPACKET_HEADERSIZE + sizeof(xpp_line_t) +
	    line_count * DAHDI_CHUNKSIZE : 0L;
	update_wanted_pcm_mask(xpd, pcm_mask, pcm_len);
	spin_unlock_irqrestore(&PHONEDEV(xpd).lock_recompute_pcm, flags);
}
EXPORT_SYMBOL(generic_card_pcm_recompute);

void fill_beep(u_char *buf, int num, int duration)
{
	bool alternate = (duration) ? (jiffies / (duration * 1000)) & 0x1 : 0;
	int which;
	u_char *snd;

	/*
	 * debug tones
	 */
	static u_char beep[] = {
		0x7F, 0xBE, 0xD8, 0xBE, 0x80, 0x41, 0x24, 0x41,	/* Dima */
		0x67, 0x90, 0x89, 0x90, 0xFF, 0x10, 0x09, 0x10,	/* Izzy */
	};
	static u_char beep_alt[] = {
		0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,	/* silence */
	};
	if (alternate) {
		which = num % ARRAY_SIZE(beep_alt);
		snd = &beep_alt[which];
	} else {
		which = num % ARRAY_SIZE(beep);
		snd = &beep[which];
	}
	memcpy(buf, snd, DAHDI_CHUNKSIZE);
}

static void do_ec(xpd_t *xpd)
{
	int i;

	for (i = 0; i < PHONEDEV(xpd).span.channels; i++) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);

		/* Don't echo cancel BRI D-chans */
		if (unlikely(IS_SET(PHONEDEV(xpd).digital_signalling, i)))
			continue;
		/* No ec for unwanted PCM */
		if (!IS_SET(PHONEDEV(xpd).wanted_pcm_mask, i))
			continue;
		dahdi_ec_chunk(chan, chan->readchunk,
			       PHONEDEV(xpd).ec_chunk2[i]);
		memcpy(PHONEDEV(xpd).ec_chunk2[i], PHONEDEV(xpd).ec_chunk1[i],
		       DAHDI_CHUNKSIZE);
		memcpy(PHONEDEV(xpd).ec_chunk1[i], chan->writechunk,
		       DAHDI_CHUNKSIZE);
	}
}

#if 0
/* Okay, now we get to the signalling.  You have several options: */

/* Option 1: If you're a T1 like interface, you can just provide a
   rbsbits function and we'll assert robbed bits for you.  Be sure to
   set the DAHDI_FLAG_RBS in this case.  */

/* Opt: If the span uses A/B bits, set them here */
int (*rbsbits) (struct dahdi_chan * chan, int bits);

/* Option 2: If you don't know about sig bits, but do have their
   equivalents (i.e. you can disconnect battery, detect off hook,
   generate ring, etc directly) then you can just specify a
   sethook function, and we'll call you with appropriate hook states
   to set.  Still set the DAHDI_FLAG_RBS in this case as well */
int (*hooksig) (struct dahdi_chan * chan, enum dahdi_txsig hookstate);

/* Option 3: If you can't use sig bits, you can write a function
   which handles the individual hook states  */
int (*sethook) (struct dahdi_chan * chan, int hookstate);
#endif

static bool pcm_valid(xpd_t *xpd, xpacket_t *pack)
{
	xpp_line_t lines = RPACKET_FIELD(pack, GLOBAL, PCM_READ, lines);
	int i;
	int count = 0;
	uint16_t good_len;

	BUG_ON(!pack);
	BUG_ON(XPACKET_OP(pack) != XPROTO_NAME(GLOBAL, PCM_READ));
	/*
	 * Don't use for_each_line(xpd, i) here because for BRI it will
	 * ignore the channels of the other xpd's in the same unit.
	 */
	for (i = 0; i < CHANNELS_PERXPD; i++)
		if (IS_SET(lines, i))
			count++;
	/* FRAMES: include opcode in calculation */
	good_len = RPACKET_HEADERSIZE + sizeof(xpp_line_t) + count * 8;
	if (XPACKET_LEN(pack) != good_len) {
		static int rate_limit;

		XPD_COUNTER(xpd, RECV_ERRORS)++;
		if ((rate_limit++ % 1000) <= 10) {
			XPD_ERR(xpd,
				"BAD PCM REPLY: packet_len=%d "
				"(should be %d), count=%d\n",
				XPACKET_LEN(pack), good_len, count);
			dump_packet("BAD PCM REPLY", pack, 1);
		}
		return 0;
	}
	return 1;
}

static inline void pcm_frame_out(xbus_t *xbus, xframe_t *xframe)
{
	unsigned long flags;
	struct timeval now;
	unsigned long usec;

	spin_lock_irqsave(&xbus->lock, flags);
	do_gettimeofday(&now);
	if (unlikely(disable_pcm || !XBUS_IS(xbus, READY)))
		goto dropit;
	if (XPACKET_ADDR_SYNC((xpacket_t *)xframe->packets)) {
		usec = usec_diff(&now, &xbus->last_tx_sync);
		xbus->last_tx_sync = now;
		/* ignore startup statistics */
		if (likely
		    (atomic_read(&xbus->pcm_rx_counter) > BIG_TICK_INTERVAL)) {
			if (abs(usec - 1000) > TICK_TOLERANCE) {
				static int rate_limit;

				if ((rate_limit++ % 5003) == 0)
					XBUS_DBG(SYNC, xbus,
						"Bad PCM TX timing(%d): "
						"usec=%ld.\n",
						rate_limit, usec);
			}
			if (usec > xbus->max_tx_sync)
				xbus->max_tx_sync = usec;
			if (usec < xbus->min_tx_sync)
				xbus->min_tx_sync = usec;
		}
	}
	spin_unlock_irqrestore(&xbus->lock, flags);
	/* OK, really send it */
	if (debug & DBG_PCM)
		dump_xframe("TX_XFRAME_PCM", xbus, xframe, debug);
	send_pcm_frame(xbus, xframe);
	XBUS_COUNTER(xbus, TX_XFRAME_PCM)++;
	return;
dropit:
	spin_unlock_irqrestore(&xbus->lock, flags);
	FREE_SEND_XFRAME(xbus, xframe);
}

/*
 * Generic implementations of card_pcmfromspan()/card_pcmtospan()
 * For FXS/FXO
 */
void generic_card_pcm_fromspan(xpd_t *xpd, xpacket_t *pack)
{
	__u8 *pcm;
	unsigned long flags;
	xpp_line_t wanted_lines;
	int i;

	BUG_ON(!xpd);
	BUG_ON(!pack);
	wanted_lines = PHONEDEV(xpd).wanted_pcm_mask;
	RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, lines) = wanted_lines;
	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, pcm);
	spin_lock_irqsave(&xpd->lock, flags);
	for (i = 0; i < PHONEDEV(xpd).channels; i++) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);

		if (IS_SET(wanted_lines, i)) {
			if (SPAN_REGISTERED(xpd)) {
#ifdef	DEBUG_PCMTX
				int channo = chan->channo;

				if (pcmtx >= 0 && pcmtx_chan == channo)
					memset((u_char *)pcm, pcmtx,
					       DAHDI_CHUNKSIZE);
				else
#endif
					memcpy((u_char *)pcm, chan->writechunk,
					       DAHDI_CHUNKSIZE);
			} else
				memset((u_char *)pcm, 0x7F, DAHDI_CHUNKSIZE);
			pcm += DAHDI_CHUNKSIZE;
		}
	}
	XPD_COUNTER(xpd, PCM_WRITE)++;
	spin_unlock_irqrestore(&xpd->lock, flags);
}
EXPORT_SYMBOL(generic_card_pcm_fromspan);

void generic_card_pcm_tospan(xpd_t *xpd, xpacket_t *pack)
{
	__u8 *pcm;
	xpp_line_t pcm_mask;
	xpp_line_t pcm_mute;
	unsigned long flags;
	int i;

	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_READ, pcm);
	pcm_mask = RPACKET_FIELD(pack, GLOBAL, PCM_READ, lines);
	spin_lock_irqsave(&xpd->lock, flags);
	/*
	 * Calculate the channels we want to mute
	 */
	pcm_mute = ~(PHONEDEV(xpd).wanted_pcm_mask);
	pcm_mute |= PHONEDEV(xpd).mute_dtmf | PHONEDEV(xpd).silence_pcm;
	if (!SPAN_REGISTERED(xpd))
		goto out;
	for (i = 0; i < PHONEDEV(xpd).channels; i++) {
		volatile u_char *r = XPD_CHAN(xpd, i)->readchunk;
		bool got_data = IS_SET(pcm_mask, i);

		if (got_data && !IS_SET(pcm_mute, i)) {
			/* We have and want real data */
			// memset((u_char *)r, 0x5A, DAHDI_CHUNKSIZE);  // DEBUG
			memcpy((u_char *)r, pcm, DAHDI_CHUNKSIZE);
		} else
		    if (IS_SET
			(PHONEDEV(xpd).wanted_pcm_mask | PHONEDEV(xpd).
			 silence_pcm, i)) {
			/* Inject SILENCE */
			memset((u_char *)r, 0x7F, DAHDI_CHUNKSIZE);
			if (IS_SET(PHONEDEV(xpd).silence_pcm, i)) {
				/*
				 * This will clear the EC buffers until next
				 * tick so we don't have noise residues
				 * from the past.
				 */
				memset(PHONEDEV(xpd).ec_chunk2[i], 0x7F,
				       DAHDI_CHUNKSIZE);
				memset(PHONEDEV(xpd).ec_chunk1[i], 0x7F,
				       DAHDI_CHUNKSIZE);
			}
		}
		if (got_data)
			pcm += DAHDI_CHUNKSIZE;
	}
out:
	XPD_COUNTER(xpd, PCM_READ)++;
	spin_unlock_irqrestore(&xpd->lock, flags);
}
EXPORT_SYMBOL(generic_card_pcm_tospan);

int generic_echocancel_timeslot(xpd_t *xpd, int pos)
{
	return xpd->addr.unit * 32 + pos;
}
EXPORT_SYMBOL(generic_echocancel_timeslot);

int generic_echocancel_setmask(xpd_t *xpd, xpp_line_t ec_mask)
{
	int i;

	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "0x%8X\n", ec_mask);
	if (!ECHOOPS(xpd->xbus)) {
		XPD_DBG(GENERAL, xpd,
			"No echo canceller in XBUS: Doing nothing.\n");
		return -EINVAL;
	}
	for (i = 0; i < PHONEDEV(xpd).channels; i++) {
		int on = BIT(i) & ec_mask;
		CALL_EC_METHOD(ec_set, xpd->xbus, xpd, i, on);
	}
	CALL_EC_METHOD(ec_update, xpd->xbus, xpd->xbus);
	return 0;
}
EXPORT_SYMBOL(generic_echocancel_setmask);

static int copy_pcm_tospan(xbus_t *xbus, xframe_t *xframe)
{
	__u8 *xframe_end;
	xpacket_t *pack;
	__u8 *p;
	int ret = -EPROTO;	/* Assume error */

	if (debug & DBG_PCM)
		dump_xframe("RX_XFRAME_PCM", xbus, xframe, debug);
	/* handle content */

	p = xframe->packets;
	xframe_end = p + XFRAME_LEN(xframe);
	do {
		int len;
		xpd_t *xpd;

		pack = (xpacket_t *)p;
		len = XPACKET_LEN(pack);
		/* Sanity checks */
		if (unlikely(XPACKET_OP(pack) != XPROTO_NAME(GLOBAL, PCM_READ))) {
			static int rate_limit;

			if ((rate_limit++ % 1003) == 0) {
				XBUS_NOTICE(xbus,
					"%s: Non-PCM packet within "
					"a PCM xframe. (%d)\n",
					__func__, rate_limit);
				dump_xframe("In PCM xframe", xbus, xframe,
					    debug);
			}
			goto out;
		}
		p += len;
		if (p > xframe_end || len < RPACKET_HEADERSIZE) {
			static int rate_limit;

			if ((rate_limit++ % 1003) == 0) {
				XBUS_NOTICE(xbus,
					"%s: Invalid packet length %d. (%d)\n",
					__func__, len, rate_limit);
				dump_xframe("BAD LENGTH", xbus, xframe, debug);
			}
			goto out;
		}
		xpd =
		    xpd_byaddr(xbus, XPACKET_ADDR_UNIT(pack),
			       XPACKET_ADDR_SUBUNIT(pack));
		if (unlikely(!xpd)) {
			static int rate_limit;

			if ((rate_limit++ % 1003) == 0) {
				notify_bad_xpd(__func__, xbus,
					       XPACKET_ADDR(pack),
					       "RECEIVE PCM");
				dump_xframe("Unknown XPD addr", xbus, xframe,
					    debug);
			}
			goto out;
		}
		if (!pcm_valid(xpd, pack))
			goto out;
		if (SPAN_REGISTERED(xpd)) {
			XBUS_COUNTER(xbus, RX_PACK_PCM)++;
			CALL_PHONE_METHOD(card_pcm_tospan, xpd, pack);
		}
	} while (p < xframe_end);
	ret = 0;		/* all good */
	XBUS_COUNTER(xbus, RX_XFRAME_PCM)++;
out:
	FREE_RECV_XFRAME(xbus, xframe);
	return ret;
}

int generic_timing_priority(xpd_t *xpd)
{
	return PHONEDEV(xpd).timing_priority;
}
EXPORT_SYMBOL(generic_timing_priority);

static void xbus_tick(xbus_t *xbus)
{
	int i;
	xpd_t *xpd;
	xframe_t *xframe = NULL;
	xpacket_t *pack = NULL;
	bool sent_sync_bit = 0;

	/*
	 * Update dahdi
	 */
	for (i = 0; i < MAX_XPDS; i++) {
		xpd = xpd_of(xbus, i);
		if (xpd && SPAN_REGISTERED(xpd)) {
#ifdef	OPTIMIZE_CHANMUTE
			int j;
			xpp_line_t xmit_mask = PHONEDEV(xpd).wanted_pcm_mask;

			xmit_mask |= PHONEDEV(xpd).silence_pcm;
			xmit_mask |= PHONEDEV(xpd).digital_signalling;
			for_each_line(xpd, j) {
				XPD_CHAN(xpd, j)->chanmute = (optimize_chanmute)
				    ? !IS_SET(xmit_mask, j)
				    : 0;
			}
#endif
			/*
			 * calls to dahdi_transmit should be out of spinlocks,
			 * as it may call back our hook setting methods.
			 */
			dahdi_transmit(&PHONEDEV(xpd).span);
		}
	}
	/*
	 * Fill xframes
	 */
	for (i = 0; i < MAX_XPDS; i++) {
		if ((xpd = xpd_of(xbus, i)) == NULL)
			continue;
		if (!IS_PHONEDEV(xpd))
			continue;
		if (SPAN_REGISTERED(xpd)) {
			size_t pcm_len = PHONEDEV(xpd).pcm_len;

			if (pcm_len && xpd->card_present) {
				do {
#if 0
					/* FORCE single packet frames */
					pack = NULL;
#endif
					if (xframe && !pack) {	/* FULL frame */
						pcm_frame_out(xbus, xframe);
						xframe = NULL;
						XBUS_COUNTER(xbus,
							     TX_PCM_FRAG)++;
					}
					if (!xframe) {	/* Alloc frame */
						xframe =
						    ALLOC_SEND_XFRAME(xbus);
						if (!xframe) {
							static int rate_limit;

							if ((rate_limit++ % 3001) == 0)
								XBUS_ERR(xbus,
									"%s: failed to allocate new xframe\n",
									__func__);
							return;
						}
					}
					pack =
					    xframe_next_packet(xframe, pcm_len);
				} while (!pack);
				XPACKET_INIT(pack, GLOBAL, PCM_WRITE,
					     xpd->xbus_idx, 1, 0);
				XPACKET_LEN(pack) = pcm_len;
				if (!sent_sync_bit) {
					XPACKET_ADDR_SYNC(pack) = 1;
					sent_sync_bit = 1;
				}
				CALL_PHONE_METHOD(card_pcm_fromspan, xpd, pack);
				XBUS_COUNTER(xbus, TX_PACK_PCM)++;
			}
		}
	}
	if (xframe)		/* clean any leftovers */
		pcm_frame_out(xbus, xframe);
	/*
	 * Receive PCM
	 */
	while ((xframe = xframe_dequeue(&xbus->pcm_tospan)) != NULL) {
		copy_pcm_tospan(xbus, xframe);
		if (XPACKET_ADDR_SYNC((xpacket_t *)xframe->packets)) {
			struct timeval now;
			unsigned long usec;

			do_gettimeofday(&now);
			usec = usec_diff(&now, &xbus->last_rx_sync);
			xbus->last_rx_sync = now;
			/* ignore startup statistics */
			if (likely
			    (atomic_read(&xbus->pcm_rx_counter) >
			     BIG_TICK_INTERVAL)) {
				if (abs(usec - 1000) > TICK_TOLERANCE) {
					static int rate_limit;

					if ((rate_limit++ % 5003) == 0)
						XBUS_DBG(SYNC, xbus,
							"Bad PCM RX "
							"timing(%d): "
							"usec=%ld.\n",
							rate_limit, usec);
				}
				if (usec > xbus->max_rx_sync)
					xbus->max_rx_sync = usec;
				if (usec < xbus->min_rx_sync)
					xbus->min_rx_sync = usec;
			}
		}
	}
	for (i = 0; i < MAX_XPDS; i++) {
		xpd = xpd_of(xbus, i);
		if (!xpd || !xpd->card_present)
			continue;
		if (IS_PHONEDEV(xpd)) {
			if (SPAN_REGISTERED(xpd)) {
				do_ec(xpd);
				dahdi_receive(&PHONEDEV(xpd).span);
			}
			/* silence was injected */
			PHONEDEV(xpd).silence_pcm = 0;
		}
		xpd->timer_count = xbus->global_counter;
		/*
		 * Must be called *after* tx/rx so
		 * D-Chan counters may be cleared
		 */
		CALL_XMETHOD(card_tick, xpd);
	}
}

static void do_tick(xbus_t *xbus, const struct timeval *tv_received)
{
	int counter = atomic_read(&xpp_tick_counter);
	unsigned long flags;

	xbus_command_queue_tick(xbus);
	if (global_ticker == xbus)
		global_tick();	/* called from here or dahdi_sync_tick() */
	spin_lock_irqsave(&ref_ticker_lock, flags);
	xpp_drift_step(xbus, tv_received);
	spin_unlock_irqrestore(&ref_ticker_lock, flags);
	if (likely(xbus->self_ticking))
		xbus_tick(xbus);
	xbus->global_counter = counter;
}

void xframe_receive_pcm(xbus_t *xbus, xframe_t *xframe)
{
	if (!xframe_enqueue(&xbus->pcm_tospan, xframe)) {
		static int rate_limit;

		if ((rate_limit++ % 1003) == 0)
			XBUS_DBG(SYNC, xbus,
				 "Failed to enqueue received pcm frame. (%d)\n",
				 rate_limit);
		FREE_RECV_XFRAME(xbus, xframe);
	}
	/*
	 * The sync_master bit is marked at the first packet
	 * of the frame, regardless of the XPD that is sync master.
	 * FIXME: what about PRI split?
	 */
	if (XPACKET_ADDR_SYNC((xpacket_t *)xframe->packets)) {
		do_tick(xbus, &xframe->tv_received);
		atomic_inc(&xbus->pcm_rx_counter);
	} else
		xbus->xbus_frag_count++;
}

int exec_sync_command(const char *buf, size_t count)
{
	int ret = count;
	int xbusno;
	xbus_t *xbus;

	if (strncmp("DAHDI", buf, 6) == 0) {	/* Ignore the newline */
		DBG(SYNC, "DAHDI");
		update_sync_master(NULL, 1);
	} else if (sscanf(buf, "SYNC=%d", &xbusno) == 1) {
		DBG(SYNC, "SYNC=%d\n", xbusno);
		xbus = get_xbus(__func__, xbusno);
		if (xbus == NULL) {
			ERR("No bus %d exists\n", xbusno);
			return -ENXIO;
		}
		update_sync_master(xbus, 0);
		put_xbus(__func__, xbus);
	} else if (sscanf(buf, "QUERY=%d", &xbusno) == 1) {
		DBG(SYNC, "QUERY=%d\n", xbusno);
		xbus = get_xbus(__func__, xbusno);
		if (xbus == NULL) {
			ERR("No bus %d exists\n", xbusno);
			return -ENXIO;
		}
		CALL_PROTO(GLOBAL, SYNC_SOURCE, xbus, NULL, SYNC_MODE_QUERY, 0);
		put_xbus(__func__, xbus);
	} else {
		ERR("%s: cannot parse '%s'\n", __func__, buf);
		ret = -EINVAL;
	}
	return ret;
}

int fill_sync_string(char *buf, size_t count)
{
	int len = 0;

	if (!syncer) {
		len +=
		    snprintf(buf, count, "%s\n",
			     (force_dahdi_sync) ? "DAHDI" : "NO-SYNC");
	} else
		len += snprintf(buf, count, "SYNC=%02d\n", syncer->num);
	return len;
}

int xbus_pcm_init(void *toplevel)
{
	int ret = 0;

#ifdef	OPTIMIZE_CHANMUTE
	INFO("FEATURE: with CHANMUTE optimization (%sactivated)\n",
	     (optimize_chanmute) ? "" : "de");
#endif
#ifdef	DAHDI_SYNC_TICK
	INFO("FEATURE: with sync_tick() from DAHDI\n");
#else
	INFO("FEATURE: without sync_tick() from DAHDI\n");
#endif
	xpp_ticker_init(&global_ticks_series);
	xpp_ticker_init(&dahdi_ticker);
	return ret;
}

void xbus_pcm_shutdown(void)
{
}
