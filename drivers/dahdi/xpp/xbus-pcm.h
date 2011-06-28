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

/*
 * This source module contains all the PCM and SYNC handling code.
 */
#ifndef	XBUS_PCM_H
#define	XBUS_PCM_H

#include "xdefs.h"
#include <linux/proc_fs.h>
#include <dahdi/kernel.h>

#ifdef	__KERNEL__

enum sync_mode {
	SYNC_MODE_NONE	= 0x00,
	SYNC_MODE_AB	= 0x01,		/* Astribank sync */
	SYNC_MODE_PLL	= 0x03,		/* Adjust XPD's PLL according to HOST */
	SYNC_MODE_QUERY	= 0x80,
};

/*
 * Abstract representation of timestamp.
 * It would (eventually) replace the hard-coded
 * timeval structs so we can migrate to better
 * time representations.
 */
struct xpp_timestamp {
	struct timeval	tv;
};

/*
 * A ticker encapsulates the timing information of some
 * abstract tick source. The following tickers are used:
 *   - Each xbus has an embedded ticker.
 *   - There is one global dahdi_ticker to represent ticks
 *     of external dahdi card (in case we want to sync
 *     from other dahdi devices).
 */
struct xpp_ticker {		/* for rate calculation */
	int			count;
	int			cycle;
	struct xpp_timestamp	first_sample;
	struct xpp_timestamp	last_sample;
	int			tick_period;	/* usec/tick */
	spinlock_t		lock;
};

/*
 * xpp_drift represent the measurements of the offset between an
 * xbus ticker to a reference ticker.
 */
struct xpp_drift {
	int			delta_tick;		/* from ref_ticker */
	int			lost_ticks;		/* occurances */
	int			lost_tick_count;
	int			sync_inaccuracy;
	struct xpp_timestamp	last_lost_tick;
	long			delta_sum;
	int			offset_prev;
	int			offset_range;
	int			offset_min;
	int			offset_max;
	int			min_speed;
	int			max_speed;
	spinlock_t		lock;
};

void xpp_drift_init(xbus_t *xbus);

static inline long usec_diff(const struct timeval *tv1, const struct timeval *tv2)
{
	long			diff_sec;
	long			diff_usec;

	diff_sec = tv1->tv_sec - tv2->tv_sec;
	diff_usec = tv1->tv_usec - tv2->tv_usec;
	return diff_sec * 1000000 + diff_usec;
}


int		xbus_pcm_init(void *top);
void		xbus_pcm_shutdown(void);
int		send_pcm_frame(xbus_t *xbus, xframe_t *xframe);
void		pcm_recompute(xpd_t *xpd, xpp_line_t tmp_pcm_mask);
void		xframe_receive_pcm(xbus_t *xbus, xframe_t *xframe);
void		update_wanted_pcm_mask(xpd_t *xpd, xpp_line_t new_mask, uint new_pcm_len);
void		generic_card_pcm_recompute(xpd_t *xpd, xpp_line_t pcm_mask);
void		generic_card_pcm_fromspan(xpd_t *xpd, xpacket_t *pack);
void		generic_card_pcm_tospan(xpd_t *xpd, xpacket_t *pack);
int		generic_timing_priority(xpd_t *xpd);
int		generic_echocancel_timeslot(xpd_t *xpd, int pos);
int		generic_echocancel_setmask(xpd_t *xpd, xpp_line_t ec_mask);
void		fill_beep(u_char *buf, int num, int duration);
const char	*sync_mode_name(enum sync_mode mode);
void		xbus_set_command_timer(xbus_t *xbus, bool on);
void		xbus_request_sync(xbus_t *xbus, enum sync_mode mode);
void		got_new_syncer(xbus_t *xbus, enum sync_mode mode, int drift);
int		xbus_command_queue_tick(xbus_t *xbus);
void		xbus_reset_counters(xbus_t *xbus);
void		elect_syncer(const char *msg);
int		exec_sync_command(const char *buf, size_t count);
int		fill_sync_string(char *buf, size_t count);
#ifdef	DAHDI_SYNC_TICK
void		dahdi_sync_tick(struct dahdi_span *span, int is_master);
#endif

#ifdef	DEBUG_PCMTX
extern int	pcmtx;
extern int	pcmtx_chan;
#endif

#endif	/* __KERNEL__ */

#endif	/* XBUS_PCM_H */

