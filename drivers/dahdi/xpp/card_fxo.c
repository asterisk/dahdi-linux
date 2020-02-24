/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "xpd.h"
#include "xproto.h"
#include "xpp_dahdi.h"
#include "card_fxo.h"
#include "dahdi_debug.h"
#include "xbus-core.h"

static const char rcsid[] = "$Id$";

static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
static DEF_PARM(uint, poll_battery_interval, 500, 0644,
		"Poll battery interval in milliseconds (0 - disable)");
static DEF_PARM_BOOL(use_polrev_firmware, 1, 0444,
		"Use firmware reports of polarity reversal");
static DEF_PARM_BOOL(squelch_polrev, 0, 0644,
		"Never report polarity reversal");
#ifdef	WITH_METERING
static DEF_PARM(uint, poll_metering_interval, 500, 0644,
		"Poll metering interval in milliseconds (0 - disable)");
#endif
static DEF_PARM(int, ring_debounce, 50, 0644,
		"Number of ticks to debounce a false RING indication");
static DEF_PARM(int, caller_id_style, 0, 0444,
		"Caller-Id detection style: "
		"0 - [BELL], "
		"1 - [ETSI_FSK], "
		"2 - [ETSI_DTMF], "
		"3 - [PASSTHROUGH]");
static DEF_PARM(int, power_denial_safezone, 650, 0644,
		"msec after offhook to ignore power-denial ( (0 - disable power-denial)");
static DEF_PARM(int, power_denial_minlen, 80, 0644,
		"Minimal detected power-denial length (msec) (0 - disable power-denial)");
static DEF_PARM(uint, battery_threshold, 3, 0644,
		"Minimum voltage that shows there is battery");
static DEF_PARM(uint, battery_debounce, 1000, 0644,
		"Minimum interval (msec) for detection of battery off");

enum cid_style {
	CID_STYLE_BELL = 0,	/* E.g: US (Bellcore) */
	CID_STYLE_ETSI_FSK = 1,	/* E.g: UK (British Telecom) */
	CID_STYLE_ETSI_DTMF = 2,	/* E.g: DK, Russia */
	CID_STYLE_PASSTHROUGH = 3,	/* No change: Let asterisk  */
					/* (>= 1.8) DSP handle this */
};

/* Signaling is opposite (fxs signalling for fxo card) */
#if 1
#define	FXO_DEFAULT_SIGCAP	(DAHDI_SIG_FXSKS | DAHDI_SIG_FXSLS)
#else
#define	FXO_DEFAULT_SIGCAP	(DAHDI_SIG_SF)
#endif

enum fxo_leds {
	LED_GREEN,
	LED_RED,
};

#define	NUM_LEDS		2
#define	DELAY_UNTIL_DIALTONE	3000

/*
 * Minimum duration for polarity reversal detection (in ticks)
 * Should be longer than the time to detect a ring, so voltage
 * fluctuation during ring won't trigger false detection.
 */
#define	POLREV_THRESHOLD	200
#define	POWER_DENIAL_CURRENT	3
#define	POWER_DENIAL_DELAY	2500	/* ticks */

/* Shortcuts */
#define	DAA_WRITE	1
#define	DAA_READ	0
#define	DAA_DIRECT_REQUEST(xbus, xpd, port, writing, reg, dL)	\
		xpp_register_request((xbus), (xpd), (port), \
		(writing), (reg), 0, 0, (dL), 0, 0, 0, 0)

/*---------------- FXO Protocol Commands ----------------------------------*/

static bool fxo_packet_is_valid(xpacket_t *pack);
static void fxo_packet_dump(const char *msg, xpacket_t *pack);
#ifdef CONFIG_PROC_FS
#ifdef DAHDI_HAVE_PROC_OPS
static const struct proc_ops proc_fxo_info_ops;
#else
static const struct file_operations proc_fxo_info_ops;
#endif
#ifdef	WITH_METERING
static const struct proc_ops proc_xpd_metering_ops;
#endif
#endif
static void dahdi_report_battery(xpd_t *xpd, lineno_t chan);
static void report_polarity_reversal(xpd_t *xpd, xportno_t portno, char *msg);

#define	PROC_FXO_INFO_FNAME	"fxo_info"
#ifdef	WITH_METERING
#define	PROC_METERING_FNAME	"metering_read"
#endif

#define	REG_INTERRUPT_SRC	0x04	/*  4 -  Interrupt Source  */
#define	REG_INTERRUPT_SRC_POLI	BIT(0)	/*  Polarity Reversal Detect Interrupt*/
#define	REG_INTERRUPT_SRC_RING	BIT(7)	/*  Ring Detect Interrupt */

#define	REG_DAA_CONTROL1	0x05	/*  5 -  DAA Control 1  */
#define	REG_DAA_CONTROL1_OH	BIT(0)	/* Off-Hook.            */
#define	REG_DAA_CONTROL1_ONHM	BIT(3)	/* On-Hook Line Monitor */

#define	DAA_REG_METERING	0x11	/* 17 */
#define	DAA_REG_CURRENT		0x1C	/* 28 */
#define	DAA_REG_VBAT		0x1D	/* 29 */

enum battery_state {
	BATTERY_UNKNOWN = 0,
	BATTERY_ON = 1,
	BATTERY_OFF = -1
};

enum polarity_state {
	POL_UNKNOWN = 0,
	POL_POSITIVE = 1,
	POL_NEGATIVE = -1
};

enum power_state {
	POWER_UNKNOWN = 0,
	POWER_ON = 1,
	POWER_OFF = -1
};

struct FXO_priv_data {
#ifdef	WITH_METERING
	struct proc_dir_entry *meteringfile;
#endif
	struct proc_dir_entry *fxo_info;
	uint poll_counter;
	signed char battery_voltage[CHANNELS_PERXPD];
	signed char battery_current[CHANNELS_PERXPD];
	enum battery_state battery[CHANNELS_PERXPD];
	ushort nobattery_debounce[CHANNELS_PERXPD];
	enum polarity_state polarity[CHANNELS_PERXPD];
	ushort polarity_debounce[CHANNELS_PERXPD];
	int  polarity_last_interval[CHANNELS_PERXPD];
#define	POLARITY_LAST_INTERVAL_NONE	(-1)
#define	POLARITY_LAST_INTERVAL_MAX	40
	enum power_state power[CHANNELS_PERXPD];
	ushort power_denial_delay[CHANNELS_PERXPD];
	ushort power_denial_length[CHANNELS_PERXPD];
	ushort power_denial_safezone[CHANNELS_PERXPD];
	xpp_line_t cidfound;	/* 0 - OFF, 1 - ON */
	unsigned int cidtimer[CHANNELS_PERXPD];
	xpp_line_t ledstate[NUM_LEDS];	/* 0 - OFF, 1 - ON */
	xpp_line_t ledcontrol[NUM_LEDS];	/* 0 - OFF, 1 - ON */
	int led_counter[NUM_LEDS][CHANNELS_PERXPD];
	atomic_t ring_debounce[CHANNELS_PERXPD];
#ifdef	WITH_METERING
	uint metering_count[CHANNELS_PERXPD];
	xpp_line_t metering_tone_state;
#endif
};

/*
 * LED counter values:
 *	n>1	: BLINK every n'th tick
 */
#define	LED_COUNTER(priv, pos, color)	((priv)->led_counter[color][pos])
#define	IS_BLINKING(priv, pos, color)	(LED_COUNTER(priv, pos, color) > 0)
#define	MARK_BLINK(priv, pos, color, t) \
		((priv)->led_counter[color][pos] = (t))
#define	MARK_OFF(priv, pos, color) \
		do { \
			BIT_CLR((priv)->ledcontrol[color], (pos)); \
			MARK_BLINK((priv), (pos), (color), 0); \
		} while (0)
#define	MARK_ON(priv, pos, color) \
		do { \
			BIT_SET((priv)->ledcontrol[color], (pos)); \
			MARK_BLINK((priv), (pos), (color), 0); \
		} while (0)

#define	LED_BLINK_RING			(1000/8)	/* in ticks */

/*---------------- FXO: Static functions ----------------------------------*/

static const char *power2str(enum power_state pw)
{
	switch (pw) {
	case POWER_UNKNOWN:
		return "UNKNOWN";
	case POWER_OFF:
		return "OFF";
	case POWER_ON:
		return "ON";
	}
	return NULL;
}

static void power_change(xpd_t *xpd, int portno, enum power_state pw)
{
	struct FXO_priv_data *priv;

	priv = xpd->priv;
	LINE_DBG(SIGNAL, xpd, portno, "power: %s -> %s\n",
		 power2str(priv->power[portno]), power2str(pw));
	priv->power[portno] = pw;
}

static void reset_battery_readings(xpd_t *xpd, lineno_t pos)
{
	struct FXO_priv_data *priv = xpd->priv;

	priv->nobattery_debounce[pos] = 0;
	priv->power_denial_delay[pos] = 0;
	power_change(xpd, pos, POWER_UNKNOWN);
}

static const int led_register_mask[] = { BIT(7), BIT(6), BIT(5) };

/*
 * LED control is done via DAA register 0x20
 */
static int do_led(xpd_t *xpd, lineno_t chan, __u8 which, bool on)
{
	int ret = 0;
	struct FXO_priv_data *priv;
	xbus_t *xbus;
	__u8 value;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	priv = xpd->priv;
	which = which % NUM_LEDS;
	if (IS_SET(PHONEDEV(xpd).digital_outputs, chan)
	    || IS_SET(PHONEDEV(xpd).digital_inputs, chan))
		goto out;
	if (chan == PORT_BROADCAST) {
		priv->ledstate[which] = (on) ? ~0 : 0;
	} else {
		if (on)
			BIT_SET(priv->ledstate[which], chan);
		else
			BIT_CLR(priv->ledstate[which], chan);
	}
	value = 0;
	value |= ((BIT(5) | BIT(6) | BIT(7)) & ~led_register_mask[which]);
	value |= (on) ? BIT(0) : 0;
	value |= (on) ? BIT(1) : 0;
	LINE_DBG(LEDS, xpd, chan, "LED: which=%d -- %s\n", which,
		 (on) ? "on" : "off");
	ret = DAA_DIRECT_REQUEST(xbus, xpd, chan, DAA_WRITE, 0x20, value);
out:
	return ret;
}

static void handle_fxo_leds(xpd_t *xpd)
{
	int i;
	unsigned long flags;
	const enum fxo_leds colors[] = { LED_GREEN, LED_RED };
	enum fxo_leds color;
	unsigned int timer_count;
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	timer_count = xpd->timer_count;
	for (color = 0; color < ARRAY_SIZE(colors); color++) {
		for_each_line(xpd, i) {
			if (IS_SET(PHONEDEV(xpd).digital_outputs, i)
			    || IS_SET(PHONEDEV(xpd).digital_inputs, i))
				continue;
			/* Blinking? */
			if ((xpd->blink_mode & BIT(i)) || IS_BLINKING(priv, i, color)) {
				int mod_value = LED_COUNTER(priv, i, color);

				if (!mod_value)
					/* safety value */
					mod_value = DEFAULT_LED_PERIOD;
				// led state is toggled
				if ((timer_count % mod_value) == 0) {
					LINE_DBG(LEDS, xpd, i, "ledstate=%s\n",
						 (IS_SET
						  (priv->ledstate[color],
						   i)) ? "ON" : "OFF");
					if (!IS_SET(priv->ledstate[color], i))
						do_led(xpd, i, color, 1);
					else
						do_led(xpd, i, color, 0);
				}
			} else if (IS_SET(priv->ledcontrol[color], i)
				   && !IS_SET(priv->ledstate[color], i)) {
				do_led(xpd, i, color, 1);
			} else if (!IS_SET(priv->ledcontrol[color], i)
				   && IS_SET(priv->ledstate[color], i)) {
				do_led(xpd, i, color, 0);
			}
		}
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
}

static void update_dahdi_ring(xpd_t *xpd, int pos, bool on)
{
	BUG_ON(!xpd);
	if (caller_id_style == CID_STYLE_BELL)
		oht_pcm(xpd, pos, !on);
	/*
	 * We should not spinlock before calling dahdi_hooksig() as
	 * it may call back into our xpp_hooksig() and cause
	 * a nested spinlock scenario
	 */
	notify_rxsig(xpd, pos, (on) ? DAHDI_RXSIG_RING : DAHDI_RXSIG_OFFHOOK);
}

static void mark_ring(xpd_t *xpd, lineno_t pos, bool on, bool update_dahdi)
{
	struct FXO_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!priv);
	atomic_set(&priv->ring_debounce[pos], 0);	/* Stop debouncing */
	/*
	 * We don't want to check battery during ringing
	 * due to voltage fluctuations.
	 */
	reset_battery_readings(xpd, pos);
	if (on && !PHONEDEV(xpd).ringing[pos]) {
		LINE_DBG(SIGNAL, xpd, pos, "START\n");
		PHONEDEV(xpd).ringing[pos] = 1;
		priv->cidtimer[pos] = xpd->timer_count;
		MARK_BLINK(priv, pos, LED_GREEN, LED_BLINK_RING);
		if (update_dahdi)
			update_dahdi_ring(xpd, pos, on);
	} else if (!on && PHONEDEV(xpd).ringing[pos]) {
		LINE_DBG(SIGNAL, xpd, pos, "STOP\n");
		PHONEDEV(xpd).ringing[pos] = 0;
		priv->cidtimer[pos] = xpd->timer_count;
		if (IS_BLINKING(priv, pos, LED_GREEN))
			MARK_BLINK(priv, pos, LED_GREEN, 0);
		if (update_dahdi)
			update_dahdi_ring(xpd, pos, on);
		priv->polarity_last_interval[pos] = POLARITY_LAST_INTERVAL_NONE;
	}
}

static int do_sethook(xpd_t *xpd, int pos, bool to_offhook)
{
	unsigned long flags;
	xbus_t *xbus;
	struct FXO_priv_data *priv;
	int ret = 0;
	__u8 value;

	BUG_ON(!xpd);
	/* We can SETHOOK state only on PSTN */
	BUG_ON(PHONEDEV(xpd).direction == TO_PHONE);
	xbus = xpd->xbus;
	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->battery[pos] != BATTERY_ON && to_offhook) {
		LINE_NOTICE(xpd, pos,
			    "Cannot take offhook while battery is off!\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&xpd->lock, flags);
	mark_ring(xpd, pos, 0, 0);	// No more rings
	value = REG_DAA_CONTROL1_ONHM;	/* Bit 3 is for CID */
	if (to_offhook)
		value |= REG_DAA_CONTROL1_OH;
	LINE_DBG(SIGNAL, xpd, pos, "SETHOOK: value=0x%02X %s\n", value,
		 (to_offhook) ? "OFFHOOK" : "ONHOOK");
	if (to_offhook)
		MARK_ON(priv, pos, LED_GREEN);
	else
		MARK_OFF(priv, pos, LED_GREEN);
	ret =
	    DAA_DIRECT_REQUEST(xbus, xpd, pos, DAA_WRITE, REG_DAA_CONTROL1,
			       value);
	mark_offhook(xpd, pos, to_offhook);
	switch (caller_id_style) {
	case CID_STYLE_ETSI_DTMF:
	case CID_STYLE_PASSTHROUGH:
		break;
	default:
		oht_pcm(xpd, pos, 0);
		break;
	}
#ifdef	WITH_METERING
	priv->metering_count[pos] = 0;
	priv->metering_tone_state = 0L;
	DAA_DIRECT_REQUEST(xbus, xpd, pos, DAA_WRITE, DAA_REG_METERING, 0x2D);
#endif
	/* unstable during hook changes */
	reset_battery_readings(xpd, pos);
	if (to_offhook) {
		priv->power_denial_safezone[pos] = power_denial_safezone;
	} else {
		priv->power_denial_length[pos] = 0;
		priv->power_denial_safezone[pos] = 0;
	}
	priv->cidtimer[pos] = xpd->timer_count;
	spin_unlock_irqrestore(&xpd->lock, flags);
	return ret;
}

/*---------------- FXO: Methods -------------------------------------------*/

static void fxo_proc_remove(xbus_t *xbus, xpd_t *xpd)
{
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	XPD_DBG(PROC, xpd, "\n");
#ifdef	CONFIG_PROC_FS
#ifdef	WITH_METERING
	if (priv->meteringfile) {
		XPD_DBG(PROC, xpd, "Removing xpd metering tone file\n");
		remove_proc_entry(PROC_METERING_FNAME, xpd->proc_xpd_dir);
		priv->meteringfile = NULL;
	}
#endif
	if (priv->fxo_info) {
		XPD_DBG(PROC, xpd, "Removing xpd FXO_INFO file\n");
		remove_proc_entry(PROC_FXO_INFO_FNAME, xpd->proc_xpd_dir);
		priv->fxo_info = NULL;
	}
#endif
}

static int fxo_proc_create(xbus_t *xbus, xpd_t *xpd)
{
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
#ifdef	CONFIG_PROC_FS
	XPD_DBG(PROC, xpd, "Creating FXO_INFO file\n");
	priv->fxo_info = proc_create_data(PROC_FXO_INFO_FNAME, 0444,
					  xpd->proc_xpd_dir,
					  &proc_fxo_info_ops, xpd);
	if (!priv->fxo_info) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_FXO_INFO_FNAME);
		fxo_proc_remove(xbus, xpd);
		return -EINVAL;
	}
	SET_PROC_DIRENTRY_OWNER(priv->fxo_info);
#ifdef	WITH_METERING
	XPD_DBG(PROC, xpd, "Creating Metering tone file\n");
	priv->meteringfile = proc_create_data(PROC_METERING_FNAME, 0444,
					      xpd->proc_xpd_dir,
					      &proc_xpd_metering_ops, xpd);
	if (!priv->meteringfile) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_METERING_FNAME);
		fxo_proc_remove(xbus, xpd);
		return -EINVAL;
	}
	SET_PROC_DIRENTRY_OWNER(priv->meteringfile);
#endif
#endif
	return 0;
}

static xpd_t *FXO_card_new(xbus_t *xbus, int unit, int subunit,
			   const xproto_table_t *proto_table,
			   const struct unit_descriptor *unit_descriptor,
			   bool to_phone)
{
	xpd_t *xpd = NULL;
	int channels;
	int subunit_ports;

	if (to_phone) {
		XBUS_NOTICE(xbus,
			"XPD=%d%d: try to instanciate FXO with "
			"reverse direction\n",
			unit, subunit);
		return NULL;
	}
	subunit_ports = unit_descriptor->numchips * unit_descriptor->ports_per_chip;
	if (unit_descriptor->subtype == 2)
		channels = min(2, subunit_ports);
	else
		channels = min(8, subunit_ports);
	xpd =
	    xpd_alloc(xbus, unit, subunit,
		      sizeof(struct FXO_priv_data), proto_table, unit_descriptor, channels);
	if (!xpd)
		return NULL;
	PHONEDEV(xpd).direction = TO_PSTN;
	xpd->type_name = "FXO";
	if (fxo_proc_create(xbus, xpd) < 0)
		goto err;
	return xpd;
err:
	xpd_free(xpd);
	return NULL;
}

static int FXO_card_init(xbus_t *xbus, xpd_t *xpd)
{
	struct FXO_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	priv = xpd->priv;
	// Hanghup all lines
	for_each_line(xpd, i) {
		do_sethook(xpd, i, 0);
		/* will be updated on next battery sample */
		priv->polarity[i] = POL_UNKNOWN;
		priv->polarity_debounce[i] = 0;
		/* will be updated on next battery sample */
		priv->battery[i] = BATTERY_UNKNOWN;
		/* will be updated on next battery sample */
		priv->power[i] = POWER_UNKNOWN;
		switch (caller_id_style) {
		case CID_STYLE_ETSI_DTMF:
		case CID_STYLE_PASSTHROUGH:
			oht_pcm(xpd, i, 1);
			break;
		}
		priv->polarity_last_interval[i] = POLARITY_LAST_INTERVAL_NONE;
	}
	XPD_DBG(GENERAL, xpd, "done\n");
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 0);
	}
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 1);
		msleep(50);
	}
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 0);
		msleep(50);
	}
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	return 0;
}

static int FXO_card_remove(xbus_t *xbus, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	fxo_proc_remove(xbus, xpd);
	return 0;
}

static int FXO_card_dahdi_preregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct FXO_priv_data *priv;
	int i;
	unsigned int timer_count;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	timer_count = xpd->timer_count;
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "ON" : "OFF");
	PHONEDEV(xpd).span.spantype = SPANTYPE_ANALOG_FXO;
	for_each_line(xpd, i) {
		struct dahdi_chan *cur_chan = XPD_CHAN(xpd, i);

		XPD_DBG(GENERAL, xpd, "setting FXO channel %d\n", i);
		snprintf(cur_chan->name, MAX_CHANNAME, "XPP_FXO/%02d/%1d%1d/%d",
			 xbus->num, xpd->addr.unit, xpd->addr.subunit, i);
		cur_chan->chanpos = i + 1;
		cur_chan->pvt = xpd;
		cur_chan->sigcap = FXO_DEFAULT_SIGCAP;
	}
	for_each_line(xpd, i) {
		MARK_ON(priv, i, LED_GREEN);
		msleep(4);
		MARK_ON(priv, i, LED_RED);
	}
	for_each_line(xpd, i) {
		priv->cidtimer[i] = timer_count;
	}
	return 0;
}

static int FXO_card_dahdi_postregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct FXO_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "ON" : "OFF");
	for_each_line(xpd, i) {
		MARK_OFF(priv, i, LED_GREEN);
		msleep(2);
		MARK_OFF(priv, i, LED_RED);
		msleep(2);
	}
	return 0;
}

static int FXO_span_assigned(xpd_t *xpd)
{
	xbus_t *xbus;
	struct FXO_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "\n");
	for_each_line(xpd, i)
		dahdi_report_battery(xpd, i);
	return 0;
}

static int FXO_card_hooksig(xpd_t *xpd, int pos, enum dahdi_txsig txsig)
{
	struct FXO_priv_data *priv;
	int ret = 0;

	priv = xpd->priv;
	BUG_ON(!priv);
	LINE_DBG(SIGNAL, xpd, pos, "%s\n", txsig2str(txsig));
	BUG_ON(PHONEDEV(xpd).direction != TO_PSTN);
	/* XXX Enable hooksig for FXO XXX */
	switch (txsig) {
	case DAHDI_TXSIG_START:
	case DAHDI_TXSIG_OFFHOOK:
		ret = do_sethook(xpd, pos, 1);
		break;
	case DAHDI_TXSIG_ONHOOK:
		ret = do_sethook(xpd, pos, 0);
		break;
	default:
		XPD_NOTICE(xpd, "Can't set tx state to %s (%d)\n",
			   txsig2str(txsig), txsig);
		return -EINVAL;
	}
	return ret;
}

static void dahdi_report_battery(xpd_t *xpd, lineno_t chan)
{
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (SPAN_REGISTERED(xpd)) {
		switch (priv->battery[chan]) {
		case BATTERY_UNKNOWN:
			/* no-op */
			break;
		case BATTERY_OFF:
			LINE_DBG(SIGNAL, xpd, chan, "Send DAHDI_ALARM_RED\n");
			dahdi_alarm_channel(XPD_CHAN(xpd, chan),
					    DAHDI_ALARM_RED);
			break;
		case BATTERY_ON:
			LINE_DBG(SIGNAL, xpd, chan, "Send DAHDI_ALARM_NONE\n");
			dahdi_alarm_channel(XPD_CHAN(xpd, chan),
					    DAHDI_ALARM_NONE);
			break;
		}
	}
}

static int FXO_card_open(xpd_t *xpd, lineno_t chan)
{
	BUG_ON(!xpd);
	return 0;
}

static void poll_battery(xbus_t *xbus, xpd_t *xpd)
{
	int i;

	for_each_line(xpd, i) {
		DAA_DIRECT_REQUEST(xbus, xpd, i, DAA_READ, DAA_REG_VBAT, 0);
	}
}

#ifdef	WITH_METERING
static void poll_metering(xbus_t *xbus, xpd_t *xpd)
{
	int i;

	for_each_line(xpd, i) {
		if (IS_OFFHOOK(xpd, i))
			DAA_DIRECT_REQUEST(xbus, xpd, i, DAA_READ,
					   DAA_REG_METERING, 0);
	}
}
#endif

static void handle_fxo_ring(xpd_t *xpd)
{
	struct FXO_priv_data *priv;
	int i;

	priv = xpd->priv;
	for_each_line(xpd, i) {
		if (likely(use_polrev_firmware)) {
			int *t = &priv->polarity_last_interval[i];
			if (*t != POLARITY_LAST_INTERVAL_NONE) {
				(*t)++;
				if (*t > POLARITY_LAST_INTERVAL_MAX) {
					LINE_DBG(SIGNAL, xpd, i,
						"polrev(GOOD): %d msec\n", *t);
					*t = POLARITY_LAST_INTERVAL_NONE;
					report_polarity_reversal(xpd,
								i, "firmware");
				}
			}
		}
		if (atomic_read(&priv->ring_debounce[i]) > 0) {
			/* Maybe start ring */
			if (atomic_dec_and_test(&priv->ring_debounce[i]))
				mark_ring(xpd, i, 1, 1);
		} else if (atomic_read(&priv->ring_debounce[i]) < 0) {
			/* Maybe stop ring */
			if (atomic_inc_and_test(&priv->ring_debounce[i]))
				mark_ring(xpd, i, 0, 1);
		}
	}
}

static void handle_fxo_power_denial(xpd_t *xpd)
{
	struct FXO_priv_data *priv;
	int i;

	if (!power_denial_safezone)
		return;		/* Ignore power denials */
	priv = xpd->priv;
	for_each_line(xpd, i) {
		if (PHONEDEV(xpd).ringing[i] || !IS_OFFHOOK(xpd, i)) {
			priv->power_denial_delay[i] = 0;
			continue;
		}
		if (priv->power_denial_safezone[i] > 0) {
			if (--priv->power_denial_safezone[i] == 0) {
				/*
				 * Poll current, prev answers are meaningless
				 */
				DAA_DIRECT_REQUEST(xpd->xbus, xpd, i, DAA_READ,
						   DAA_REG_CURRENT, 0);
			}
			continue;
		}
		if (priv->power_denial_length[i] > 0) {
			priv->power_denial_length[i]--;
			if (priv->power_denial_length[i] <= 0) {
				/*
				 * But maybe the FXS started to ring (and
				 * the firmware haven't detected it yet).
				 * This would cause false power denials so
				 * we just flag it and schedule more ticks
				 * to wait.
				 */
				LINE_DBG(SIGNAL, xpd, i,
					 "Possible Power Denial Hangup\n");
				priv->power_denial_delay[i] =
				    POWER_DENIAL_DELAY;
			}
			continue;
		}
		if (priv->power_denial_delay[i] > 0) {
			/*
			 * Ring detection by the firmware takes some time.
			 * Therefore we delay our decision until we are
			 * sure that no ring has started during this time.
			 */
			priv->power_denial_delay[i]--;
			if (priv->power_denial_delay[i] <= 0) {
				LINE_DBG(SIGNAL, xpd, i,
					 "Power Denial Hangup\n");
				priv->power_denial_delay[i] = 0;
				/*
				 * Let Asterisk decide what to do
				 */
				notify_rxsig(xpd, i, DAHDI_RXSIG_ONHOOK);
			}
		}
	}
}

/*
 * For caller-id CID_STYLE_ETSI_DTMF:
 *   - No indication is passed before the CID
 *   - We try to detect it and send "fake" polarity reversal.
 *   - The chan_dahdi.conf should have cidstart=polarity
 *   - Based on an idea in http://bugs.digium.com/view.php?id=9096
 */
static void check_etsi_dtmf(xpd_t *xpd)
{
	struct FXO_priv_data *priv;
	int portno;
	unsigned int timer_count;

	if (!SPAN_REGISTERED(xpd))
		return;
	priv = xpd->priv;
	BUG_ON(!priv);
	timer_count = xpd->timer_count;
	for_each_line(xpd, portno) {
		/* Skip offhook and ringing ports */
		if (IS_OFFHOOK(xpd, portno) || PHONEDEV(xpd).ringing[portno])
			continue;
		if (IS_SET(priv->cidfound, portno)) {
			if (timer_count > priv->cidtimer[portno] + 4000) {
				/* reset flags if it's been a while */
				priv->cidtimer[portno] = timer_count;
				BIT_CLR(priv->cidfound, portno);
				LINE_DBG(SIGNAL, xpd, portno,
					 "Reset CID flag\n");
			}
			continue;
		}
		if (timer_count > priv->cidtimer[portno] + 400) {
			struct dahdi_chan *chan = XPD_CHAN(xpd, portno);
			int sample;
			int i;

			for (i = 0; i < DAHDI_CHUNKSIZE; i++) {
				sample = DAHDI_XLAW(chan->readchunk[i], chan);
				if (sample > 16000 || sample < -16000) {
					priv->cidtimer[portno] = timer_count;
					BIT_SET(priv->cidfound, portno);
					LINE_DBG(SIGNAL, xpd, portno,
						"Found DTMF CLIP (%d)\n", i);
					report_polarity_reversal(xpd, portno,
							"fake");
					break;
				}
			}
		}
	}
}

static int FXO_card_tick(xbus_t *xbus, xpd_t *xpd)
{
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (poll_battery_interval != 0
	    && (priv->poll_counter % poll_battery_interval) == 0)
		poll_battery(xbus, xpd);
#ifdef	WITH_METERING
	if (poll_metering_interval != 0
	    && (priv->poll_counter % poll_metering_interval) == 0)
		poll_metering(xbus, xpd);
#endif
	handle_fxo_leds(xpd);
	handle_fxo_ring(xpd);
	handle_fxo_power_denial(xpd);
	if (caller_id_style == CID_STYLE_ETSI_DTMF && likely(xpd->card_present))
		check_etsi_dtmf(xpd);
	priv->poll_counter++;
	return 0;
}

#include <dahdi/wctdm_user.h>
/*
 * The first register is the ACIM, the other are coefficient registers.
 * We define the array size explicitly to track possible inconsistencies
 * if the struct is modified.
 */
static const char echotune_regs[sizeof(struct wctdm_echo_coefs)] =
    { 30, 45, 46, 47, 48, 49, 50, 51, 52 };

static int FXO_card_ioctl(xpd_t *xpd, int pos, unsigned int cmd,
			  unsigned long arg)
{
	int i, ret;
	unsigned char echotune_data[ARRAY_SIZE(echotune_regs)];

	BUG_ON(!xpd);
	if (!XBUS_IS(xpd->xbus, READY))
		return -ENODEV;
	switch (cmd) {
	case WCTDM_SET_ECHOTUNE:
		XPD_DBG(GENERAL, xpd, "-- Setting echo registers: \n");
		/* first off: check if this span is fxs. If not: -EINVALID */
		if (copy_from_user
		    (&echotune_data, (void __user *)arg, sizeof(echotune_data)))
			return -EFAULT;

		for (i = 0; i < ARRAY_SIZE(echotune_regs); i++) {
			XPD_DBG(REGS, xpd, "Reg=0x%02X, data=0x%02X\n",
				echotune_regs[i], echotune_data[i]);
			ret =
			    DAA_DIRECT_REQUEST(xpd->xbus, xpd, pos, DAA_WRITE,
					       echotune_regs[i],
					       echotune_data[i]);
			if (ret < 0) {
				LINE_NOTICE(xpd, pos,
					"Couldn't write %0x02X to "
					"register %0x02X\n",
					echotune_data[i], echotune_regs[i]);
				return ret;
			}
			msleep(1);
		}

		XPD_DBG(GENERAL, xpd, "-- Set echo registers successfully\n");
		break;
	case DAHDI_TONEDETECT:
		/*
		 * Asterisk call all span types with this (FXS specific)
		 * call. Silently ignore it.
		 */
		LINE_DBG(GENERAL, xpd, pos,
			 "DAHDI_TONEDETECT (FXO: NOTIMPLEMENTED)\n");
		return -ENOTTY;
	default:
		report_bad_ioctl(THIS_MODULE->name, xpd, pos, cmd);
		return -ENOTTY;
	}
	return 0;
}

/*---------------- FXO: HOST COMMANDS -------------------------------------*/

/*---------------- FXO: Astribank Reply Handlers --------------------------*/

HANDLER_DEF(FXO, SIG_CHANGED)
{
	xpp_line_t sig_status =
	    RPACKET_FIELD(pack, FXO, SIG_CHANGED, sig_status);
	xpp_line_t sig_toggles =
	    RPACKET_FIELD(pack, FXO, SIG_CHANGED, sig_toggles);
	unsigned long flags;
	int i;
	struct FXO_priv_data *priv;

	if (!xpd) {
		notify_bad_xpd(__func__, xbus, XPACKET_ADDR(pack), cmd->name);
		return -EPROTO;
	}
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(SIGNAL, xpd, "(PSTN) sig_toggles=0x%04X sig_status=0x%04X\n",
		sig_toggles, sig_status);
	spin_lock_irqsave(&xpd->lock, flags);
	for_each_line(xpd, i) {
		int debounce;

		if (IS_SET(sig_toggles, i)) {
			if (priv->battery[i] == BATTERY_OFF) {
				/*
				 * With poll_battery_interval==0 we cannot
				 * have BATTERY_OFF so we won't get here
				 */
				LINE_NOTICE(xpd, i,
					"SIG_CHANGED while battery is off. "
					"Ignored.\n");
				continue;
			}
			/* First report false ring alarms */
			debounce = atomic_read(&priv->ring_debounce[i]);
			if (debounce)
				LINE_NOTICE(xpd, i,
					"Ignored a false short ring "
					"(lasted only %dms)\n",
					ring_debounce - debounce);
			/*
			 * Now set a new ring alarm.
			 * It will be checked in handle_fxo_ring()
			 */
			debounce =
			    (IS_SET(sig_status, i)) ? ring_debounce :
			    -ring_debounce;
			atomic_set(&priv->ring_debounce[i], debounce);
		}
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static void report_polarity_reversal(xpd_t *xpd, xportno_t portno, char *msg)
{
	/*
	 * Inform dahdi/Asterisk:
	 * 1. Maybe used for hangup detection during offhook
	 * 2. In some countries used to report caller-id
	 *    during onhook but before first ring.
	 */
	if (caller_id_style == CID_STYLE_ETSI_FSK)
		/* will be cleared on ring/offhook */
		oht_pcm(xpd, portno, 1);
	if (SPAN_REGISTERED(xpd)) {
		LINE_DBG(SIGNAL, xpd, portno,
			"%s DAHDI_EVENT_POLARITY (%s)\n",
			(squelch_polrev) ? "Squelch" : "Send",
			msg);
		if (!squelch_polrev)
			dahdi_qevent_lock(XPD_CHAN(xpd, portno),
				DAHDI_EVENT_POLARITY);
	}
}

static void update_battery_voltage(xpd_t *xpd, __u8 data_low,
	xportno_t portno)
{
	struct FXO_priv_data *priv;
	enum polarity_state pol;
	int msec;
	signed char volts = (signed char)data_low;

	priv = xpd->priv;
	BUG_ON(!priv);
	priv->battery_voltage[portno] = volts;
	if (PHONEDEV(xpd).ringing[portno])
		goto ignore_reading;	/* ring voltage create false alarms */
	if (abs(volts) < battery_threshold) {
		/*
		 * Check for battery voltage fluctuations
		 */
		if (priv->battery[portno] != BATTERY_OFF) {
			int milliseconds;

			milliseconds =
			    priv->nobattery_debounce[portno]++ *
			    poll_battery_interval;
			if (milliseconds > battery_debounce) {
				LINE_DBG(SIGNAL, xpd, portno,
					 "BATTERY OFF voltage=%d\n", volts);
				priv->battery[portno] = BATTERY_OFF;
				dahdi_report_battery(xpd, portno);
				/* What's the polarity ? */
				priv->polarity[portno] = POL_UNKNOWN;
				priv->polarity_debounce[portno] = 0;
				/* What's the current ? */
				power_change(xpd, portno, POWER_UNKNOWN);
				/*
				 * Stop further processing for now
				 */
				goto ignore_reading;
			}

		}
	} else {
		priv->nobattery_debounce[portno] = 0;
		if (priv->battery[portno] != BATTERY_ON) {
			LINE_DBG(SIGNAL, xpd, portno, "BATTERY ON voltage=%d\n",
				 volts);
			priv->battery[portno] = BATTERY_ON;
			dahdi_report_battery(xpd, portno);
		}
	}
#if 0
	/*
	 * Mark FXO ports without battery!
	 */
	if (priv->battery[portno] != BATTERY_ON)
		MARK_ON(priv, portno, LED_RED);
	else
		MARK_OFF(priv, portno, LED_RED);
#endif
	if (priv->battery[portno] != BATTERY_ON) {
		/* What's the polarity ? */
		priv->polarity[portno] = POL_UNKNOWN;
		return;
	}
	/*
	 * Handle reverse polarity
	 */
	if (volts == 0)
		pol = POL_UNKNOWN;
	else if (volts < 0)
		pol = POL_NEGATIVE;
	else
		pol = POL_POSITIVE;
	if (priv->polarity[portno] == pol) {
		/*
		 * Same polarity, reset debounce counter
		 */
		priv->polarity_debounce[portno] = 0;
		return;
	}
	/*
	 * Track polarity reversals and debounce spikes.
	 * Only reversals with long duration count.
	 */
	msec = priv->polarity_debounce[portno]++ * poll_battery_interval;
	if (msec >= POLREV_THRESHOLD) {
		priv->polarity_debounce[portno] = 0;
		if (pol != POL_UNKNOWN && priv->polarity[portno] != POL_UNKNOWN) {
			char *polname = NULL;

			if (pol == POL_POSITIVE)
				polname = "Positive";
			else if (pol == POL_NEGATIVE)
				polname = "Negative";
			else
				BUG();
			LINE_DBG(SIGNAL, xpd, portno,
				 "Polarity changed to %s\n", polname);
			if (!use_polrev_firmware)
				report_polarity_reversal(xpd, portno, polname);
		}
		priv->polarity[portno] = pol;
	}
	return;
ignore_reading:
	/*
	 * Reset debounce counters to prevent false alarms
	 */
	/* unstable during hook changes */
	reset_battery_readings(xpd, portno);
}

static void update_battery_current(xpd_t *xpd, __u8 data_low,
	xportno_t portno)
{
	struct FXO_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!priv);
	priv->battery_current[portno] = data_low;
	/*
	 * During ringing, current is not stable.
	 * During onhook there should not be current anyway.
	 */
	if (PHONEDEV(xpd).ringing[portno] || !IS_OFFHOOK(xpd, portno))
		goto ignore_it;
	/*
	 * Power denial with no battery voltage is meaningless
	 */
	if (priv->battery[portno] != BATTERY_ON)
		goto ignore_it;
	/* Safe zone after offhook */
	if (priv->power_denial_safezone[portno] > 0)
		goto ignore_it;
	if (data_low < POWER_DENIAL_CURRENT) {
		if (priv->power[portno] == POWER_ON) {
			power_change(xpd, portno, POWER_OFF);
			priv->power_denial_length[portno] = power_denial_minlen;
		}
	} else {
		if (priv->power[portno] != POWER_ON) {
			power_change(xpd, portno, POWER_ON);
			priv->power_denial_length[portno] = 0;
			/* We are now OFFHOOK */
			hookstate_changed(xpd, portno, 1);
		}
	}
	return;
ignore_it:
	priv->power_denial_delay[portno] = 0;
}

#ifdef	WITH_METERING
#define	BTD_BIT	BIT(0)

static void update_metering_state(xpd_t *xpd, __u8 data_low, lineno_t portno)
{
	struct FXO_priv_data *priv;
	bool metering_tone = data_low & BTD_BIT;
	bool old_metering_tone;

	priv = xpd->priv;
	BUG_ON(!priv);
	old_metering_tone = IS_SET(priv->metering_tone_state, portno);
	LINE_DBG(SIGNAL, xpd, portno, "METERING: %s [dL=0x%X] (%d)\n",
		 (metering_tone) ? "ON" : "OFF", data_low,
		 priv->metering_count[portno]);
	if (metering_tone && !old_metering_tone) {
		/* Rising edge */
		priv->metering_count[portno]++;
		BIT_SET(priv->metering_tone_state, portno);
	} else if (!metering_tone && old_metering_tone)
		BIT_CLR(priv->metering_tone_state, portno);
	if (metering_tone) {
		/* Clear the BTD bit */
		data_low &= ~BTD_BIT;
		DAA_DIRECT_REQUEST(xpd->xbus, xpd, portno, DAA_WRITE,
				   DAA_REG_METERING, data_low);
	}
}
#endif

static void got_chip_interrupt(xpd_t *xpd, __u8 data_low,
	xportno_t portno)
{
	struct FXO_priv_data *priv;
	int t;

	if (!use_polrev_firmware)
		return;
	priv = xpd->priv;
	LINE_DBG(SIGNAL, xpd, portno, "mask=0x%X\n", data_low);
	if (!(data_low & REG_INTERRUPT_SRC_POLI))
		return;
	t = priv->polarity_last_interval[portno];
	if (PHONEDEV(xpd).ringing[portno]) {
		priv->polarity_last_interval[portno] =
			POLARITY_LAST_INTERVAL_NONE;
		LINE_DBG(SIGNAL, xpd, portno,
			"polrev(false): %d msec (while ringing)\n", t);
	} else if (data_low & REG_INTERRUPT_SRC_RING) {
		priv->polarity_last_interval[portno] =
			POLARITY_LAST_INTERVAL_NONE;
		LINE_DBG(SIGNAL, xpd, portno,
			"polrev(false): %d msec (with chip-interrupt ring)\n",
			t);
	} else if (t == POLARITY_LAST_INTERVAL_NONE) {
		priv->polarity_last_interval[portno] = 0;
		LINE_DBG(SIGNAL, xpd, portno,
			"polrev(start)\n");
	} else if (t < POLARITY_LAST_INTERVAL_MAX) {
		/*
		 * Start counting upward from -POLARITY_LAST_INTERVAL_MAX
		 * Until we reach POLARITY_LAST_INTERVAL_NONE.
		 * This way we filter bursts of false reports we get
		 * during ringing.
		 */
		priv->polarity_last_interval[portno] =
			POLARITY_LAST_INTERVAL_NONE -
			POLARITY_LAST_INTERVAL_MAX;
		LINE_DBG(SIGNAL, xpd, portno,
			"polrev(false): %d msec (interval shorter than %d)\n",
			t, POLARITY_LAST_INTERVAL_MAX);
	}
}

static int FXO_card_register_reply(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *info)
{
	struct FXO_priv_data *priv;
	lineno_t portno;

	priv = xpd->priv;
	BUG_ON(!priv);
	portno = info->h.portnum;
	switch (REG_FIELD(info, regnum)) {
	case REG_INTERRUPT_SRC:
		got_chip_interrupt(xpd, REG_FIELD(info, data_low), portno);
		break;
	case DAA_REG_VBAT:
		update_battery_voltage(xpd, REG_FIELD(info, data_low), portno);
		break;
	case DAA_REG_CURRENT:
		update_battery_current(xpd, REG_FIELD(info, data_low), portno);
		break;
#ifdef	WITH_METERING
	case DAA_REG_METERING:
		update_metering_state(xpd, REG_FIELD(info, data_low), portno);
		break;
#endif
	}
	LINE_DBG(REGS, xpd, portno, "%c reg_num=0x%X, dataL=0x%X dataH=0x%X\n",
		 ((info->h.bytes == 3) ? 'I' : 'D'), REG_FIELD(info, regnum),
		 REG_FIELD(info, data_low), REG_FIELD(info, data_high));
	/* Update /proc info only if reply relate to the last slic read request */
	if (REG_FIELD(&xpd->requested_reply, regnum) ==
			REG_FIELD(info, regnum)
		&& REG_FIELD(&xpd->requested_reply, do_subreg) ==
			REG_FIELD(info, do_subreg)
		&& REG_FIELD(&xpd->requested_reply, subreg) ==
			REG_FIELD(info, subreg)) {
		xpd->last_reply = *info;
	}
	return 0;
}

static int FXO_card_state(xpd_t *xpd, bool on)
{
	int ret = 0;
	struct FXO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	return ret;
}

static const struct xops fxo_xops = {
	.card_new = FXO_card_new,
	.card_init = FXO_card_init,
	.card_remove = FXO_card_remove,
	.card_tick = FXO_card_tick,
	.card_register_reply = FXO_card_register_reply,
};

static const struct phoneops fxo_phoneops = {
	.card_dahdi_preregistration = FXO_card_dahdi_preregistration,
	.card_dahdi_postregistration = FXO_card_dahdi_postregistration,
	.card_hooksig = FXO_card_hooksig,
	.card_pcm_recompute = generic_card_pcm_recompute,
	.card_pcm_fromspan = generic_card_pcm_fromspan,
	.card_pcm_tospan = generic_card_pcm_tospan,
	.card_timing_priority = generic_timing_priority,
	.echocancel_timeslot = generic_echocancel_timeslot,
	.echocancel_setmask = generic_echocancel_setmask,
	.card_ioctl = FXO_card_ioctl,
	.card_open = FXO_card_open,
	.card_state = FXO_card_state,
	.span_assigned = FXO_span_assigned,
};

static xproto_table_t PROTO_TABLE(FXO) = {
	.owner = THIS_MODULE,
	.entries = {
		/*      Prototable      Card    Opcode          */
		XENTRY(	FXO,		FXO,	SIG_CHANGED	),
	},
	.name = "FXO",	/* protocol name */
	.ports_per_subunit = 8,
	.type = XPD_TYPE_FXO,
	.xops = &fxo_xops,
	.phoneops = &fxo_phoneops,
	.packet_is_valid = fxo_packet_is_valid,
	.packet_dump = fxo_packet_dump,
};

static bool fxo_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe;

	//DBG(GENERAL, "\n");
	xe = xproto_card_entry(&PROTO_TABLE(FXO), XPACKET_OP(pack));
	return xe != NULL;
}

static void fxo_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

/*------------------------- DAA Handling --------------------------*/

#ifdef	CONFIG_PROC_FS
static int proc_fxo_info_show(struct seq_file *sfile, void *not_used)
{
	unsigned long flags;
	xpd_t *xpd = sfile->private;
	struct FXO_priv_data *priv;
	int i;

	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	BUG_ON(!priv);
	seq_printf(sfile, "\t%-17s: ", "Channel");
	for_each_line(xpd, i) {
		if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
		    && !IS_SET(PHONEDEV(xpd).digital_inputs, i)) {
			seq_printf(sfile, "%4d ", i % 10);
		}
	}
	seq_printf(sfile, "\nLeds:");
	seq_printf(sfile, "\n\t%-17s: ", "state");
	for_each_line(xpd, i) {
		if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
		    && !IS_SET(PHONEDEV(xpd).digital_inputs, i)) {
			seq_printf(sfile, "  %d%d ",
				   IS_SET(priv->ledstate[LED_GREEN], i),
				   IS_SET(priv->ledstate[LED_RED], i));
		}
	}
	seq_printf(sfile, "\n\t%-17s: ", "blinking");
	for_each_line(xpd, i) {
		if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
		    && !IS_SET(PHONEDEV(xpd).digital_inputs, i)) {
			seq_printf(sfile, "  %d%d ",
				   IS_BLINKING(priv, i, LED_GREEN),
				   IS_BLINKING(priv, i, LED_RED));
		}
	}
	seq_printf(sfile, "\nBattery-Data:");
	seq_printf(sfile, "\n\t%-17s: ", "voltage");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->battery_voltage[i]);
	}
	seq_printf(sfile, "\n\t%-17s: ", "current");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->battery_current[i]);
	}
	seq_printf(sfile, "\nBattery:");
	seq_printf(sfile, "\n\t%-17s: ", "on");
	for_each_line(xpd, i) {
		char *bat;

		if (priv->battery[i] == BATTERY_ON)
			bat = "+";
		else if (priv->battery[i] == BATTERY_OFF)
			bat = "-";
		else
			bat = ".";
		seq_printf(sfile, "%4s ", bat);
	}
	seq_printf(sfile, "\n\t%-17s: ", "debounce");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->nobattery_debounce[i]);
	}
	seq_printf(sfile, "\nPolarity-Reverse:");
	seq_printf(sfile, "\n\t%-17s: ", "polarity");
	for_each_line(xpd, i) {
		char *polname;

		if (priv->polarity[i] == POL_POSITIVE)
			polname = "+";
		else if (priv->polarity[i] == POL_NEGATIVE)
			polname = "-";
		else
			polname = ".";
		seq_printf(sfile, "%4s ", polname);
	}
	seq_printf(sfile, "\n\t%-17s: ", "debounce");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->polarity_debounce[i]);
	}
	seq_printf(sfile, "\nPower-Denial:");
	seq_printf(sfile, "\n\t%-17s: ", "power");
	for_each_line(xpd, i) {
		char *curr;

		if (priv->power[i] == POWER_ON)
			curr = "+";
		else if (priv->power[i] == POWER_OFF)
			curr = "-";
		else
			curr = ".";
		seq_printf(sfile, "%4s ", curr);
	}
	seq_printf(sfile, "\n\t%-17s: ", "safezone");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->power_denial_safezone[i]);
	}
	seq_printf(sfile, "\n\t%-17s: ", "delay");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->power_denial_delay[i]);
	}
#ifdef	WITH_METERING
	seq_printf(sfile, "\nMetering:");
	seq_printf(sfile, "\n\t%-17s: ", "count");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d ", priv->metering_count[i]);
	}
#endif
	seq_printf(sfile, "\n");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static int proc_fxo_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_fxo_info_show, PDE_DATA(inode));
}

#ifdef DAHDI_HAVE_PROC_OPS
static const struct proc_ops proc_fxo_info_ops = {
	.proc_open		= proc_fxo_info_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};
#else
static const struct file_operations proc_fxo_info_ops = {
	.owner			= THIS_MODULE,
	.open			= proc_fxo_info_open,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};
#endif

#ifdef	WITH_METERING
static int proc_xpd_metering_show(struct seq_file *sfile, void *not_used)
{
	unsigned long flags;
	xpd_t *xpd = sfile->private;
	struct FXO_priv_data *priv;
	int i;

	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	seq_printf(sfile, "# Chan\tMeter (since last read)\n");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%d\t%d\n", i, priv->metering_count[i]);
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	/* Zero meters */
	for_each_line(xpd, i)
	    priv->metering_count[i] = 0;
	return 0;
}

static int proc_xpd_metering_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_xpd_metering_show, PDE_DATA(inode));
}

static const struct file_operations proc_xpd_metering_ops = {
	.owner		= THIS_MODULE,
	.open		= proc_xpd_metering_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#endif
#endif

static DEVICE_ATTR_READER(fxo_battery_show, dev, buf)
{
	xpd_t *xpd;
	struct FXO_priv_data *priv;
	unsigned long flags;
	int len = 0;
	int i;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	for_each_line(xpd, i) {
		char bat;

		if (priv->battery[i] == BATTERY_ON)
			bat = '+';
		else if (priv->battery[i] == BATTERY_OFF)
			bat = '-';
		else
			bat = '.';
		len += sprintf(buf + len, "%c ", bat);
	}
	len += sprintf(buf + len, "\n");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(fxo_battery, S_IRUGO, fxo_battery_show, NULL);

static int fxo_xpd_probe(struct device *dev)
{
	xpd_t *xpd;
	int ret;

	xpd = dev_to_xpd(dev);
	/* Is it our device? */
	if (xpd->xpd_type != XPD_TYPE_FXO) {
		XPD_ERR(xpd, "drop suggestion for %s (%d)\n", dev_name(dev),
			xpd->xpd_type);
		return -EINVAL;
	}
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	ret = device_create_file(dev, &dev_attr_fxo_battery);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(fxo_battery) failed: %d\n",
			__func__, ret);
		goto fail_fxo_battery;
	}
	return 0;
fail_fxo_battery:
	return ret;
}

static int fxo_xpd_remove(struct device *dev)
{
	xpd_t *xpd;

	xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	device_remove_file(dev, &dev_attr_fxo_battery);
	return 0;
}

static struct xpd_driver fxo_driver = {
	.xpd_type = XPD_TYPE_FXO,
	.driver = {
		   .name = "fxo",
		   .owner = THIS_MODULE,
		   .probe = fxo_xpd_probe,
		   .remove = fxo_xpd_remove}
};

static int __init card_fxo_startup(void)
{
	int ret;

	if (ring_debounce <= 0) {
		ERR("ring_debounce=%d. Must be positive number of ticks\n",
		    ring_debounce);
		return -EINVAL;
	}
	if ((ret = xpd_driver_register(&fxo_driver.driver)) < 0)
		return ret;
#ifdef	WITH_METERING
	INFO("FEATURE: WITH METERING Detection\n");
#else
	INFO("FEATURE: NO METERING Detection\n");
#endif
	xproto_register(&PROTO_TABLE(FXO));
	return 0;
}

static void __exit card_fxo_cleanup(void)
{
	xproto_unregister(&PROTO_TABLE(FXO));
	xpd_driver_unregister(&fxo_driver.driver);
}

MODULE_DESCRIPTION("XPP FXO Card Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_XPD(XPD_TYPE_FXO);

module_init(card_fxo_startup);
module_exit(card_fxo_cleanup);
