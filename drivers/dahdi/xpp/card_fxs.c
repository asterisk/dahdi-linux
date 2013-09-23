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
#include <linux/seq_file.h>
#include "xpd.h"
#include "xproto.h"
#include "xpp_dahdi.h"
#include "card_fxs.h"
#include "dahdi_debug.h"
#include "xbus-core.h"

static const char rcsid[] = "$Id$";

/* must be before dahdi_debug.h */
static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
static DEF_PARM_BOOL(reversepolarity, 0, 0644, "Reverse Line Polarity");
static DEF_PARM_BOOL(dtmf_detection, 1, 0644, "Do DTMF detection in hardware");
#ifdef	POLL_DIGITAL_INPUTS
static DEF_PARM(uint, poll_digital_inputs, 1000, 0644, "Poll Digital Inputs");
#endif

static DEF_PARM_BOOL(vmwi_ioctl, 1, 0644,
		     "Asterisk support VMWI notification via ioctl");
static DEF_PARM_BOOL(ring_trapez, 0, 0664, "Use trapezoid ring type");
static DEF_PARM_BOOL(lower_ringing_noise, 0, 0664,
		"Lower ringing noise (may loose CallerID)");

/* Signaling is opposite (fxo signalling for fxs card) */
#if 1
#define	FXS_DEFAULT_SIGCAP \
		(DAHDI_SIG_FXOKS | DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS)
#else
#define	FXS_DEFAULT_SIGCAP \
		(DAHDI_SIG_SF | DAHDI_SIG_EM)
#endif

#define	VMWI_TYPE(priv, pos, type)	\
	((priv)->vmwisetting[pos].vmwi_type & DAHDI_VMWI_ ## type)
#define	VMWI_NEON(priv, pos)		VMWI_TYPE(priv, pos, HVAC)

#define	LINES_DIGI_OUT	2
#define	LINES_DIGI_INP	4

enum fxs_leds {
	LED_GREEN,
	LED_RED,
	OUTPUT_RELAY,
};

#define	NUM_LEDS	2

/* Shortcuts */
#define	SLIC_WRITE	1
#define	SLIC_READ	0
#define	SLIC_DIRECT_REQUEST(xbus, xpd, port, writing, reg, dL)	\
	xpp_register_request((xbus), (xpd), (port), \
	(writing), (reg), 0, 0, (dL), 0, 0, 0)
#define	SLIC_INDIRECT_REQUEST(xbus, xpd, port, writing, reg, dL, dH)	\
	xpp_register_request((xbus), (xpd), (port), \
	(writing), 0x1E, 1, (reg), (dL), 1, (dH), 0)

#define	VALID_PORT(port) \
		(((port) >= 0 && (port) <= 7) || (port) == PORT_BROADCAST)

#define	REG_DIGITAL_IOCTRL	0x06	/* LED and RELAY control */

/* Values of SLIC linefeed control register (0x40) */
enum fxs_state {
	FXS_LINE_OPEN = 0x00,	/* Open */
	FXS_LINE_ACTIVE = 0x01,	/* Forward active */
	FXS_LINE_OHTRANS = 0x02,	/* Forward on-hook transmission */
	FXS_LINE_TIPOPEN = 0x03,	/* TIP open */
	FXS_LINE_RING = 0x04,	/* Ringing */
	FXS_LINE_REV_ACTIVE = 0x05,	/* Reverse active */
	FXS_LINE_REV_OHTRANS = 0x06,	/* Reverse on-hook transmission */
	FXS_LINE_RING_OPEN = 0x07	/* RING open */
};

#define	FXS_LINE_POL_ACTIVE \
		((reversepolarity) ? FXS_LINE_REV_ACTIVE : FXS_LINE_ACTIVE)
#define	FXS_LINE_POL_OHTRANS \
		((reversepolarity) ? FXS_LINE_REV_OHTRANS : FXS_LINE_OHTRANS)

/*
 * DTMF detection
 */
#define REG_DTMF_DECODE		0x18	/* 24 - DTMF Decode Status */
#define REG_BATTERY		0x42	/* 66 - Battery Feed Control */
#define	REG_BATTERY_BATSL	BIT(1)	/* Battery Feed Select */

/* 68 -  Loop Closure/Ring Trip Detect Status */
#define	REG_LOOPCLOSURE		0x44
#define	REG_LOOPCLOSURE_ZERO	0xF8	/* Loop Closure zero bits. */
#define	REG_LOOPCLOSURE_LCR	BIT(0)	/* Loop Closure Detect Indicator. */

/*---------------- FXS Protocol Commands ----------------------------------*/

static bool fxs_packet_is_valid(xpacket_t *pack);
static void fxs_packet_dump(const char *msg, xpacket_t *pack);
#ifdef CONFIG_PROC_FS
static const struct file_operations proc_fxs_info_ops;
#ifdef	WITH_METERING
static const struct file_operations proc_xpd_metering_ops;
#endif
#endif
static void start_stop_vm_led(xbus_t *xbus, xpd_t *xpd, lineno_t pos);

#define	PROC_REGISTER_FNAME	"slics"
#define	PROC_FXS_INFO_FNAME	"fxs_info"
#ifdef	WITH_METERING
#define	PROC_METERING_FNAME	"metering_gen"
#endif

struct FXS_priv_data {
#ifdef	WITH_METERING
	struct proc_dir_entry *meteringfile;
#endif
	struct proc_dir_entry *fxs_info;
	xpp_line_t ledstate[NUM_LEDS];	/* 0 - OFF, 1 - ON */
	xpp_line_t ledcontrol[NUM_LEDS];	/* 0 - OFF, 1 - ON */
	xpp_line_t search_fsk_pattern;
	xpp_line_t found_fsk_pattern;
	xpp_line_t update_offhook_state;
	xpp_line_t want_dtmf_events;	/* what dahdi want */
	xpp_line_t want_dtmf_mute;	/* what dahdi want */
	xpp_line_t prev_key_down;	/* DTMF down sets the bit */
	xpp_line_t neon_blinking;
	xpp_line_t vbat_h;		/* High voltage */
	struct timeval prev_key_time[CHANNELS_PERXPD];
	int led_counter[NUM_LEDS][CHANNELS_PERXPD];
	int ohttimer[CHANNELS_PERXPD];
#define OHT_TIMER		6000	/* How long after RING to retain OHT */
	/* IDLE changing hook state */
	enum fxs_state idletxhookstate[CHANNELS_PERXPD];
	enum fxs_state lasttxhook[CHANNELS_PERXPD];
	struct dahdi_vmwi_info vmwisetting[CHANNELS_PERXPD];
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

/*---------------- FXS: Static functions ----------------------------------*/
static int do_chan_power(xbus_t *xbus, xpd_t *xpd, lineno_t chan, bool on)
{
	struct FXS_priv_data *priv;
	unsigned long *p;
	int was;

	BUG_ON(!xbus);
	BUG_ON(!xpd);
	priv = xpd->priv;
	p = (unsigned long *)&priv->vbat_h;
	if (on)
		was = test_and_set_bit(chan, p) != 0;
	else
		was = test_and_clear_bit(chan, p) != 0;
	if (was == on) {
		LINE_DBG(SIGNAL, xpd, chan,
			"%s (same, ignored)\n", (on) ? "up" : "down");
		return 0;
	}
	LINE_DBG(SIGNAL, xpd, chan, "%s\n", (on) ? "up" : "down");
	return SLIC_DIRECT_REQUEST(xbus, xpd, chan, SLIC_WRITE, REG_BATTERY,
			(on) ? REG_BATTERY_BATSL : 0x00);
}

static int linefeed_control(xbus_t *xbus, xpd_t *xpd, lineno_t chan,
			    enum fxs_state value)
{
	struct FXS_priv_data *priv;
	bool want_vbat_h;

	priv = xpd->priv;
	/*
	 * Should we drop vbat_h only during actuall ring?
	 *   - It would lower the noise caused to other channels by
	 *     group ringing
	 *   - But it may also stop CallerID from passing through the SLIC
	 */
	want_vbat_h = value == FXS_LINE_RING;
	if (lower_ringing_noise || want_vbat_h)
		do_chan_power(xbus, xpd, chan, want_vbat_h);
	LINE_DBG(SIGNAL, xpd, chan, "value=0x%02X\n", value);
	priv->lasttxhook[chan] = value;
	return SLIC_DIRECT_REQUEST(xbus, xpd, chan, SLIC_WRITE, 0x40, value);
}

static void vmwi_search(xpd_t *xpd, lineno_t pos, bool on)
{
	struct FXS_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!xpd);
	if (VMWI_NEON(priv, pos) && on) {
		LINE_DBG(SIGNAL, xpd, pos, "START\n");
		BIT_SET(priv->search_fsk_pattern, pos);
	} else {
		LINE_DBG(SIGNAL, xpd, pos, "STOP\n");
		BIT_CLR(priv->search_fsk_pattern, pos);
	}
}

/*
 * LED and RELAY control is done via SLIC register 0x06:
 *         7     6     5     4     3     2     1     0
 *  +-----+-----+-----+-----+-----+-----+-----+-----+
 *  | M2  | M1  | M3  | C2  | O1  | O3  | C1  | C3  |
 *  +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 *  Cn	- Control bit (control one digital line)
 *  On	- Output bit (program a digital line for output)
 *  Mn	- Mask bit (only the matching output control bit is affected)
 *
 *  C3	- OUTPUT RELAY (0 - OFF, 1 - ON)
 *  C1	- GREEN LED (0 - OFF, 1 - ON)
 *  O3	- Output RELAY (this line is output)
 *  O1	- Output GREEN (this line is output)
 *  C2	- RED LED (0 - OFF, 1 - ON)
 *  M3	- Mask RELAY. (1 - C3 effect the OUTPUT RELAY)
 *  M2	- Mask RED. (1 - C2 effect the RED LED)
 *  M1	- Mask GREEN. (1 - C1 effect the GREEN LED)
 *
 *  The OUTPUT RELAY (actually a relay out) is connected to line 0 and 4 only.
 */

//                                              GREEN   RED     OUTPUT RELAY
static const int led_register_mask[] = { BIT(7), BIT(6), BIT(5) };
static const int led_register_vals[] = { BIT(4), BIT(1), BIT(0) };

/*
 * pos can be:
 *	- A line number
 *	- ALL_LINES. This is not valid anymore since 8-Jan-2007.
 */
static int do_led(xpd_t *xpd, lineno_t chan, __u8 which, bool on)
{
	int ret = 0;
	struct FXS_priv_data *priv;
	int value;
	xbus_t *xbus;

	BUG_ON(!xpd);
	BUG_ON(chan == ALL_LINES);
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
	LINE_DBG(LEDS, xpd, chan, "LED: which=%d -- %s\n", which,
		 (on) ? "on" : "off");
	value = BIT(2) | BIT(3);
	value |= ((BIT(5) | BIT(6) | BIT(7)) & ~led_register_mask[which]);
	if (on)
		value |= led_register_vals[which];
	ret =
	    SLIC_DIRECT_REQUEST(xbus, xpd, chan, SLIC_WRITE, REG_DIGITAL_IOCTRL,
				value);
out:
	return ret;
}

static void handle_fxs_leds(xpd_t *xpd)
{
	int i;
	const enum fxs_leds colors[] = { LED_GREEN, LED_RED };
	enum fxs_leds color;
	unsigned int timer_count;
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	timer_count = xpd->timer_count;
	for (color = 0; color < ARRAY_SIZE(colors); color++) {
		for_each_line(xpd, i) {
			if (IS_SET
			    (PHONEDEV(xpd).digital_outputs | PHONEDEV(xpd).
			     digital_inputs, i))
				continue;
			/* Blinking? */
			if ((xpd->blink_mode & BIT(i)) || IS_BLINKING(priv, i, color)) {
				int mod_value = LED_COUNTER(priv, i, color);

				if (!mod_value)
					/* safety value */
					mod_value = DEFAULT_LED_PERIOD;
				/* led state is toggled */
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
			} else
			    if (IS_SET
				(priv->ledcontrol[color] & ~priv->
				 ledstate[color], i)) {
				do_led(xpd, i, color, 1);
			} else
			    if (IS_SET
				(~priv->ledcontrol[color] & priv->
				 ledstate[color], i)) {
				do_led(xpd, i, color, 0);
			}

		}
	}
}

static void restore_leds(xpd_t *xpd)
{
	struct FXS_priv_data *priv;
	int i;

	priv = xpd->priv;
	for_each_line(xpd, i) {
		if (IS_OFFHOOK(xpd, i))
			MARK_ON(priv, i, LED_GREEN);
		else
			MARK_OFF(priv, i, LED_GREEN);
	}
}

#ifdef	WITH_METERING
static int metering_gen(xpd_t *xpd, lineno_t chan, bool on)
{
	__u8 value = (on) ? 0x94 : 0x00;

	LINE_DBG(SIGNAL, xpd, chan, "METERING Generate: %s\n",
		 (on) ? "ON" : "OFF");
	return SLIC_DIRECT_REQUEST(xpd->xbus, xpd, chan, SLIC_WRITE, 0x23,
				   value);
}
#endif

/*---------------- FXS: Methods -------------------------------------------*/

static void fxs_proc_remove(xbus_t *xbus, xpd_t *xpd)
{
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
#ifdef	CONFIG_PROC_FS
#ifdef	WITH_METERING
	if (priv->meteringfile) {
		XPD_DBG(PROC, xpd, "Removing xpd metering tone file\n");
		remove_proc_entry(PROC_METERING_FNAME, xpd->proc_xpd_dir);
		priv->meteringfile = NULL;
	}
#endif
	if (priv->fxs_info) {
		XPD_DBG(PROC, xpd, "Removing xpd FXS_INFO file\n");
		remove_proc_entry(PROC_FXS_INFO_FNAME, xpd->proc_xpd_dir);
		priv->fxs_info = NULL;
	}
#endif
}

static int fxs_proc_create(xbus_t *xbus, xpd_t *xpd)
{
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;

#ifdef	CONFIG_PROC_FS
	XPD_DBG(PROC, xpd, "Creating FXS_INFO file\n");
	priv->fxs_info = proc_create_data(PROC_FXS_INFO_FNAME, 0444,
					  xpd->proc_xpd_dir,
					  &proc_fxs_info_ops, xpd);
	if (!priv->fxs_info) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_FXS_INFO_FNAME);
		fxs_proc_remove(xbus, xpd);
		return -EINVAL;
	}
	SET_PROC_DIRENTRY_OWNER(priv->fxs_info);
#ifdef	WITH_METERING
	XPD_DBG(PROC, xpd, "Creating Metering tone file\n");
	priv->meteringfile = proc_create_data(PROC_METERING_FNAME, 0200,
					      xpd->proc_xpd_dir,
					      &proc_xpd_metering_ops, xpd);
	if (!priv->meteringfile) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_METERING_FNAME);
		fxs_proc_remove(xbus, xpd);
		return -EINVAL;
	}
#endif
#endif
	return 0;
}

static xpd_t *FXS_card_new(xbus_t *xbus, int unit, int subunit,
			   const xproto_table_t *proto_table, __u8 subtype,
			   int subunits, int subunit_ports, bool to_phone)
{
	xpd_t *xpd = NULL;
	int channels;
	int regular_channels;
	struct FXS_priv_data *priv;
	int i;
	int d_inputs = 0;
	int d_outputs = 0;

	if (!to_phone) {
		XBUS_NOTICE(xbus,
			    "XPD=%d%d: try to instanciate FXS with reverse direction\n",
			    unit, subunit);
		return NULL;
	}
	if (subtype == 2)
		regular_channels = min(6, subunit_ports);
	else
		regular_channels = min(8, subunit_ports);
	channels = regular_channels;
	/* Calculate digital inputs/outputs */
	if (unit == 0 && subtype != 4) {
		channels += 6;	/* 2 DIGITAL OUTPUTS, 4 DIGITAL INPUTS */
		d_inputs = LINES_DIGI_INP;
		d_outputs = LINES_DIGI_OUT;
	}
	xpd =
	    xpd_alloc(xbus, unit, subunit, subtype, subunits,
		      sizeof(struct FXS_priv_data), proto_table, channels);
	if (!xpd)
		return NULL;
	/* Initialize digital inputs/outputs */
	if (d_inputs) {
		XBUS_DBG(GENERAL, xbus, "Initialize %d digital inputs\n",
			 d_inputs);
		PHONEDEV(xpd).digital_inputs =
		    BITMASK(d_inputs) << (regular_channels + d_outputs);
	} else
		XBUS_DBG(GENERAL, xbus, "No digital inputs\n");
	if (d_outputs) {
		XBUS_DBG(GENERAL, xbus, "Initialize %d digital outputs\n",
			 d_outputs);
		PHONEDEV(xpd).digital_outputs =
		    BITMASK(d_outputs) << regular_channels;
	} else
		XBUS_DBG(GENERAL, xbus, "No digital outputs\n");
	PHONEDEV(xpd).direction = TO_PHONE;
	xpd->type_name = "FXS";
	if (fxs_proc_create(xbus, xpd) < 0)
		goto err;
	priv = xpd->priv;
	for_each_line(xpd, i) {
		priv->idletxhookstate[i] = FXS_LINE_POL_ACTIVE;
	}
	return xpd;
err:
	xpd_free(xpd);
	return NULL;
}

static int FXS_card_init(xbus_t *xbus, xpd_t *xpd)
{
	int ret = 0;
	int i;

	BUG_ON(!xpd);
	/*
	 * Setup ring timers
	 */
	/* Software controled ringing (for CID) */
	/* Ringing Oscilator Control */
	ret = SLIC_DIRECT_REQUEST(xbus, xpd, PORT_BROADCAST, SLIC_WRITE,
		0x22, 0x00);
	if (ret < 0)
		goto err;
	for_each_line(xpd, i) {
		linefeed_control(xbus, xpd, i, FXS_LINE_POL_ACTIVE);
	}
	XPD_DBG(GENERAL, xpd, "done\n");
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 0);
		do_led(xpd, i, LED_RED, 0);
	}
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 1);
		msleep(50);
	}
	for_each_line(xpd, i) {
		do_led(xpd, i, LED_GREEN, 0);
		msleep(50);
	}
	restore_leds(xpd);
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	/*
	 * We should query our offhook state long enough time after we
	 * set the linefeed_control()
	 * So we do this after the LEDs
	 */
	for_each_line(xpd, i) {
		if (IS_SET
		    (PHONEDEV(xpd).digital_outputs | PHONEDEV(xpd).
		     digital_inputs, i))
			continue;
		SLIC_DIRECT_REQUEST(xbus, xpd, i, SLIC_READ, REG_LOOPCLOSURE,
				    0);
	}
	return 0;
err:
	fxs_proc_remove(xbus, xpd);
	XPD_ERR(xpd, "Failed initializing registers (%d)\n", ret);
	return ret;
}

static int FXS_card_remove(xbus_t *xbus, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	fxs_proc_remove(xbus, xpd);
	return 0;
}

static int FXS_card_dahdi_preregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct FXS_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	PHONEDEV(xpd).span.spantype = SPANTYPE_ANALOG_FXS;
	for_each_line(xpd, i) {
		struct dahdi_chan *cur_chan = XPD_CHAN(xpd, i);

		XPD_DBG(GENERAL, xpd, "setting FXS channel %d\n", i);
		if (IS_SET(PHONEDEV(xpd).digital_outputs, i)) {
			snprintf(cur_chan->name, MAX_CHANNAME,
				 "XPP_OUT/%02d/%1d%1d/%d", xbus->num,
				 xpd->addr.unit, xpd->addr.subunit, i);
		} else if (IS_SET(PHONEDEV(xpd).digital_inputs, i)) {
			snprintf(cur_chan->name, MAX_CHANNAME,
				 "XPP_IN/%02d/%1d%1d/%d", xbus->num,
				 xpd->addr.unit, xpd->addr.subunit, i);
		} else {
			snprintf(cur_chan->name, MAX_CHANNAME,
				 "XPP_FXS/%02d/%1d%1d/%d", xbus->num,
				 xpd->addr.unit, xpd->addr.subunit, i);
		}
		cur_chan->chanpos = i + 1;
		cur_chan->pvt = xpd;
		cur_chan->sigcap = FXS_DEFAULT_SIGCAP;
		if (!vmwi_ioctl) {
			/* Old asterisk, assume default VMWI type */
			priv->vmwisetting[i].vmwi_type = DAHDI_VMWI_HVAC;
		}
	}
	for_each_line(xpd, i) {
		MARK_ON(priv, i, LED_GREEN);
		msleep(4);
		MARK_ON(priv, i, LED_RED);
	}
	return 0;
}

static int FXS_card_dahdi_postregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct FXS_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	for_each_line(xpd, i) {
		MARK_OFF(priv, i, LED_GREEN);
		msleep(2);
		MARK_OFF(priv, i, LED_RED);
		msleep(2);
	}
	restore_leds(xpd);
	return 0;
}

/*
 * Called with XPD spinlocked
 */
static void __do_mute_dtmf(xpd_t *xpd, int pos, bool muteit)
{
	struct FXS_priv_data *priv;

	priv = xpd->priv;
	LINE_DBG(SIGNAL, xpd, pos, "%s\n", (muteit) ? "MUTE" : "UNMUTE");
	if (muteit)
		BIT_SET(PHONEDEV(xpd).mute_dtmf, pos);
	else
		BIT_CLR(PHONEDEV(xpd).mute_dtmf, pos);
	/* already spinlocked */
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, priv->search_fsk_pattern);
}

struct ring_reg_param {
	int is_indirect;
	int regno;
	uint8_t h_val;
	uint8_t l_val;
};

enum ring_types {
	RING_TYPE_NEON = 0,
	RING_TYPE_TRAPEZ,
	RING_TYPE_NORMAL,
};

struct byte_pair {
	uint8_t h_val;
	uint8_t l_val;
};

struct ring_reg_params {
	const int is_indirect;
	const int regno;
	struct byte_pair values[1 + RING_TYPE_NORMAL - RING_TYPE_NEON];
};

#define	REG_ENTRY(di, reg, vh1, vl1, vh2, vl2, vh3, vl3) \
	{ (di), (reg), .values = { \
		[RING_TYPE_NEON]	= { .h_val = (vh1), .l_val = (vl1) }, \
		[RING_TYPE_TRAPEZ]	= { .h_val = (vh2), .l_val = (vl2) }, \
		[RING_TYPE_NORMAL]	= { .h_val = (vh3), .l_val = (vl3) }, \
		}, \
	}

static struct ring_reg_params ring_parameters[] = {
	/*        INDIR REG     NEON            TRAPEZ          NORMAL */
	REG_ENTRY(1,	0x16,	0xE8, 0x03,	0xC8, 0x00,	0x00, 0x00),
	REG_ENTRY(1,	0x15,	0xEF, 0x7B,	0xAB, 0x5E,	0x77, 0x01),
	REG_ENTRY(1,	0x14,	0x9F, 0x00,	0x8C, 0x01,	0xFD, 0x7E),

	REG_ENTRY(0,	0x22,	0x00, 0x19,	0x00, 0x01,	0x00, 0x00),

	REG_ENTRY(0,	0x30,	0x00, 0xE0,	0x00, 0x00,	0x00, 0x00),
	REG_ENTRY(0,	0x31,	0x00, 0x01,	0x00, 0x00,	0x00, 0x00),
	REG_ENTRY(0,	0x32,	0x00, 0xF0,	0x00, 0x00,	0x00, 0x00),
	REG_ENTRY(0,	0x33,	0x00, 0x05,	0x00, 0x00,	0x00, 0x00),

	REG_ENTRY(1,	0x1D,	0x00, 0x46,	0x00, 0x36,	0x00, 0x36),
};

static int send_ring_parameters(xbus_t *xbus, xpd_t *xpd, int pos,
		enum ring_types rtype)
{
	const struct ring_reg_params *p;
	const struct byte_pair *v;
	int ret = 0;
	int i;

	if (rtype < RING_TYPE_NEON || rtype > RING_TYPE_NORMAL)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(ring_parameters); i++) {
		p = &ring_parameters[i];
		v = &(p->values[rtype]);
		if (p->is_indirect) {
			LINE_DBG(REGS, xpd, pos,
					"[%d] 0x%02X: I 0x%02X 0x%02X\n",
					i, p->regno, v->h_val, v->l_val);
			ret = SLIC_INDIRECT_REQUEST(xbus, xpd, pos, SLIC_WRITE,
				p->regno, v->h_val, v->l_val);
			if (ret < 0) {
				LINE_ERR(xpd, pos,
					"Failed: 0x%02X: I 0x%02X, 0x%02X\n",
					p->regno, v->h_val, v->l_val);
				break;
			}
		} else {
			LINE_DBG(REGS, xpd, pos, "[%d] 0x%02X: D 0x%02X\n",
				i, p->regno, v->l_val);
			ret = SLIC_DIRECT_REQUEST(xbus, xpd, pos, SLIC_WRITE,
				p->regno, v->l_val);
			if (ret < 0) {
				LINE_ERR(xpd, pos,
					"Failed: 0x%02X: D 0x%02X\n",
					p->regno, v->l_val);
				break;
			}
		}
	}
	return ret;
}

static int set_vm_led_mode(xbus_t *xbus, xpd_t *xpd, int pos,
			   unsigned int msg_waiting)
{
	int ret = 0;
	struct FXS_priv_data *priv;
	BUG_ON(!xbus);
	BUG_ON(!xpd);

	priv = xpd->priv;
	if (VMWI_NEON(priv, pos) && msg_waiting) {
		/* A write to register 0x40 will now turn on/off the VM led */
		LINE_DBG(SIGNAL, xpd, pos, "NEON\n");
		BIT_SET(priv->neon_blinking, pos);
		ret = send_ring_parameters(xbus, xpd, pos, RING_TYPE_NEON);
	} else if (ring_trapez) {
		LINE_DBG(SIGNAL, xpd, pos, "RINGER: Trapez ring\n");
		ret = send_ring_parameters(xbus, xpd, pos, RING_TYPE_TRAPEZ);
	} else {
		/* A write to register 0x40 will now turn on/off the ringer */
		LINE_DBG(SIGNAL, xpd, pos, "RINGER\n");
		BIT_CLR(priv->neon_blinking, pos);
		ret = send_ring_parameters(xbus, xpd, pos, RING_TYPE_NORMAL);
	}
	return (ret ? -EPROTO : 0);
}

static void start_stop_vm_led(xbus_t *xbus, xpd_t *xpd, lineno_t pos)
{
	struct FXS_priv_data *priv;
	unsigned int msgs;

	BUG_ON(!xpd);
	if (IS_SET
	    (PHONEDEV(xpd).digital_outputs | PHONEDEV(xpd).digital_inputs, pos))
		return;
	priv = xpd->priv;
	msgs = PHONEDEV(xpd).msg_waiting[pos];
	LINE_DBG(SIGNAL, xpd, pos, "%s\n", (msgs) ? "ON" : "OFF");
	set_vm_led_mode(xbus, xpd, pos, msgs);
	do_chan_power(xbus, xpd, pos, msgs > 0);
	linefeed_control(xbus, xpd, pos,
			 (msgs >
			  0) ? FXS_LINE_RING : priv->idletxhookstate[pos]);
}

static int relay_out(xpd_t *xpd, int pos, bool on)
{
	int value;
	int which = pos;
	int relay_channels[] = { 0, 4 };

	BUG_ON(!xpd);
	/* map logical position to output port number (0/1) */
	which -= (xpd->subtype == 2) ? 6 : 8;
	LINE_DBG(SIGNAL, xpd, pos, "which=%d -- %s\n", which,
		 (on) ? "on" : "off");
	which = which % ARRAY_SIZE(relay_channels);
	value = BIT(2) | BIT(3);
	value |=
	    ((BIT(5) | BIT(6) | BIT(7)) & ~led_register_mask[OUTPUT_RELAY]);
	if (on)
		value |= led_register_vals[OUTPUT_RELAY];
	return SLIC_DIRECT_REQUEST(xpd->xbus, xpd, relay_channels[which],
				   SLIC_WRITE, REG_DIGITAL_IOCTRL, value);
}

static int send_ring(xpd_t *xpd, lineno_t chan, bool on)
{
	int ret = 0;
	xbus_t *xbus;
	struct FXS_priv_data *priv;
	enum fxs_state value = (on) ? FXS_LINE_RING : FXS_LINE_POL_ACTIVE;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	LINE_DBG(SIGNAL, xpd, chan, "%s\n", (on) ? "on" : "off");
	priv = xpd->priv;
	set_vm_led_mode(xbus, xpd, chan, 0);
	do_chan_power(xbus, xpd, chan, on);	/* Power up (for ring) */
	ret = linefeed_control(xbus, xpd, chan, value);
	if (on) {
		MARK_BLINK(priv, chan, LED_GREEN, LED_BLINK_RING);
	} else {
		if (IS_BLINKING(priv, chan, LED_GREEN))
			MARK_BLINK(priv, chan, LED_GREEN, 0);
	}
	return ret;
}

static int FXS_card_hooksig(xpd_t *xpd, int pos, enum dahdi_txsig txsig)
{
	struct FXS_priv_data *priv;
	int ret = 0;
	struct dahdi_chan *chan = NULL;
	enum fxs_state txhook;
	unsigned long flags;

	LINE_DBG(SIGNAL, xpd, pos, "%s\n", txsig2str(txsig));
	priv = xpd->priv;
	BUG_ON(PHONEDEV(xpd).direction != TO_PHONE);
	if (IS_SET(PHONEDEV(xpd).digital_inputs, pos)) {
		LINE_DBG(SIGNAL, xpd, pos,
			 "Ignoring signal sent to digital input line\n");
		return 0;
	}
	if (SPAN_REGISTERED(xpd))
		chan = XPD_CHAN(xpd, pos);
	switch (txsig) {
	case DAHDI_TXSIG_ONHOOK:
		spin_lock_irqsave(&xpd->lock, flags);
		PHONEDEV(xpd).ringing[pos] = 0;
		oht_pcm(xpd, pos, 0);
		vmwi_search(xpd, pos, 0);
		BIT_CLR(priv->want_dtmf_events, pos);
		BIT_CLR(priv->want_dtmf_mute, pos);
		__do_mute_dtmf(xpd, pos, 0);
		spin_unlock_irqrestore(&xpd->lock, flags);
		if (IS_SET(PHONEDEV(xpd).digital_outputs, pos)) {
			LINE_DBG(SIGNAL, xpd, pos, "%s -> digital output OFF\n",
				 txsig2str(txsig));
			ret = relay_out(xpd, pos, 0);
			return ret;
		}
		if (priv->lasttxhook[pos] == FXS_LINE_OPEN) {
			/*
			 * Restore state after KEWL hangup.
			 */
			LINE_DBG(SIGNAL, xpd, pos, "KEWL STOP\n");
			linefeed_control(xpd->xbus, xpd, pos,
					 FXS_LINE_POL_ACTIVE);
			if (IS_OFFHOOK(xpd, pos))
				MARK_ON(priv, pos, LED_GREEN);
		}
		ret = send_ring(xpd, pos, 0);	// RING off
		if (!IS_OFFHOOK(xpd, pos))
			start_stop_vm_led(xpd->xbus, xpd, pos);
		txhook = priv->lasttxhook[pos];
		if (chan) {
			switch (chan->sig) {
			case DAHDI_SIG_EM:
			case DAHDI_SIG_FXOKS:
			case DAHDI_SIG_FXOLS:
				txhook = priv->idletxhookstate[pos];
				break;
			case DAHDI_SIG_FXOGS:
				txhook = FXS_LINE_TIPOPEN;
				break;
			}
		}
		ret = linefeed_control(xpd->xbus, xpd, pos, txhook);
		break;
	case DAHDI_TXSIG_OFFHOOK:
		if (IS_SET(PHONEDEV(xpd).digital_outputs, pos)) {
			LINE_NOTICE(xpd, pos,
				    "%s -> Is digital output. Ignored\n",
				    txsig2str(txsig));
			return -EINVAL;
		}
		txhook = priv->lasttxhook[pos];
		if (PHONEDEV(xpd).ringing[pos]) {
			oht_pcm(xpd, pos, 1);
			txhook = FXS_LINE_OHTRANS;
		}
		PHONEDEV(xpd).ringing[pos] = 0;
		if (chan) {
			switch (chan->sig) {
			case DAHDI_SIG_EM:
				txhook = FXS_LINE_POL_ACTIVE;
				break;
			default:
				txhook = priv->idletxhookstate[pos];
				break;
			}
		}
		ret = linefeed_control(xpd->xbus, xpd, pos, txhook);
		break;
	case DAHDI_TXSIG_START:
		PHONEDEV(xpd).ringing[pos] = 1;
		oht_pcm(xpd, pos, 0);
		vmwi_search(xpd, pos, 0);
		if (IS_SET(PHONEDEV(xpd).digital_outputs, pos)) {
			LINE_DBG(SIGNAL, xpd, pos, "%s -> digital output ON\n",
				 txsig2str(txsig));
			ret = relay_out(xpd, pos, 1);
			return ret;
		}
		ret = send_ring(xpd, pos, 1);	// RING on
		break;
	case DAHDI_TXSIG_KEWL:
		if (IS_SET(PHONEDEV(xpd).digital_outputs, pos)) {
			LINE_DBG(SIGNAL, xpd, pos,
				 "%s -> Is digital output. Ignored\n",
				 txsig2str(txsig));
			return -EINVAL;
		}
		linefeed_control(xpd->xbus, xpd, pos, FXS_LINE_OPEN);
		MARK_OFF(priv, pos, LED_GREEN);
		break;
	default:
		XPD_NOTICE(xpd, "%s: Can't set tx state to %s (%d)\n", __func__,
			   txsig2str(txsig), txsig);
		ret = -EINVAL;
	}
	return ret;
}

static int set_vmwi(xpd_t *xpd, int pos, unsigned long arg)
{
	struct FXS_priv_data *priv;
	struct dahdi_vmwi_info vmwisetting;
	const int vmwi_flags =
	    DAHDI_VMWI_LREV | DAHDI_VMWI_HVDC | DAHDI_VMWI_HVAC;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (copy_from_user
	    (&vmwisetting, (__user void *)arg, sizeof(vmwisetting)))
		return -EFAULT;
	if ((vmwisetting.vmwi_type & ~vmwi_flags) != 0) {
		LINE_NOTICE(xpd, pos, "Bad DAHDI_VMWI_CONFIG: 0x%X\n",
			    vmwisetting.vmwi_type);
		return -EINVAL;
	}
	LINE_DBG(SIGNAL, xpd, pos, "DAHDI_VMWI_CONFIG: 0x%X\n",
		 vmwisetting.vmwi_type);
	if (VMWI_TYPE(priv, pos, LREV)) {
		LINE_NOTICE(xpd, pos,
			    "%s: VMWI(lrev) is not implemented yet. Ignored.\n",
			    __func__);
	}
	if (VMWI_TYPE(priv, pos, HVDC)) {
		LINE_NOTICE(xpd, pos,
			    "%s: VMWI(hvdc) is not implemented yet. Ignored.\n",
			    __func__);
	}
	if (VMWI_TYPE(priv, pos, HVAC))
		;		/* VMWI_NEON */
	if (priv->vmwisetting[pos].vmwi_type == 0)
		;		/* Disable VMWI */
	priv->vmwisetting[pos] = vmwisetting;
	set_vm_led_mode(xpd->xbus, xpd, pos, PHONEDEV(xpd).msg_waiting[pos]);
	return 0;
}

/*
 * Private ioctl()
 * We don't need it now, since we detect vmwi via FSK patterns
 */
static int FXS_card_ioctl(xpd_t *xpd, int pos, unsigned int cmd,
			  unsigned long arg)
{
	struct FXS_priv_data *priv;
	xbus_t *xbus;
	int val;
	unsigned long flags;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	if (!XBUS_IS(xbus, READY))
		return -ENODEV;
	if (pos < 0 || pos >= PHONEDEV(xpd).channels) {
		XPD_NOTICE(xpd, "Bad channel number %d in %s(), cmd=%u\n", pos,
			   __func__, cmd);
		return -EINVAL;
	}

	switch (cmd) {
	case DAHDI_ONHOOKTRANSFER:
		if (get_user(val, (int __user *)arg))
			return -EFAULT;
		LINE_DBG(SIGNAL, xpd, pos, "DAHDI_ONHOOKTRANSFER (%d millis)\n",
			 val);
		if (IS_SET
		    (PHONEDEV(xpd).digital_inputs | PHONEDEV(xpd).
		     digital_outputs, pos))
			return 0;	/* Nothing to do */
		oht_pcm(xpd, pos, 1);	/* Get ready of VMWI FSK tones */
		if (priv->lasttxhook[pos] == FXS_LINE_POL_ACTIVE
		    || IS_SET(priv->neon_blinking, pos)) {
			priv->ohttimer[pos] = val;
			priv->idletxhookstate[pos] = FXS_LINE_POL_OHTRANS;
			vmwi_search(xpd, pos, 1);
			CALL_PHONE_METHOD(card_pcm_recompute, xpd,
					  priv->search_fsk_pattern);
			LINE_DBG(SIGNAL, xpd, pos,
				 "Start OHT_TIMER. wanted_pcm_mask=0x%X\n",
				 PHONEDEV(xpd).wanted_pcm_mask);
		}
		if (VMWI_NEON(priv, pos) && !IS_OFFHOOK(xpd, pos))
			start_stop_vm_led(xbus, xpd, pos);
		return 0;
	case DAHDI_TONEDETECT:
		if (get_user(val, (int __user *)arg))
			return -EFAULT;
		LINE_DBG(SIGNAL, xpd, pos,
			 "DAHDI_TONEDETECT: %s %s (dtmf_detection=%s)\n",
			 (val & DAHDI_TONEDETECT_ON) ? "ON" : "OFF",
			 (val & DAHDI_TONEDETECT_MUTE) ? "MUTE" : "NO-MUTE",
			 (dtmf_detection ? "YES" : "NO"));
		if (!dtmf_detection) {
			spin_lock_irqsave(&xpd->lock, flags);
			if (IS_SET(priv->want_dtmf_events, pos)) {
				/*
				 * Detection mode changed:
				 * Disable DTMF interrupts
				 */
				SLIC_DIRECT_REQUEST(xbus, xpd, pos, SLIC_WRITE,
						    0x17, 0);
			}
			BIT_CLR(priv->want_dtmf_events, pos);
			BIT_CLR(priv->want_dtmf_mute, pos);
			__do_mute_dtmf(xpd, pos, 0);
			spin_unlock_irqrestore(&xpd->lock, flags);
			return -ENOTTY;
		}
		/*
		 * During natively bridged calls, Asterisk
		 * will request one of the sides to stop sending
		 * dtmf events. Check the requested state.
		 */
		spin_lock_irqsave(&xpd->lock, flags);
		if (val & DAHDI_TONEDETECT_ON) {
			if (!IS_SET(priv->want_dtmf_events, pos)) {
				/*
				 * Detection mode changed:
				 * Enable DTMF interrupts
				 */
				LINE_DBG(SIGNAL, xpd, pos,
					"DAHDI_TONEDETECT: "
					"Enable Hardware DTMF\n");
				SLIC_DIRECT_REQUEST(xbus, xpd, pos, SLIC_WRITE,
						    0x17, 1);
			}
			BIT_SET(priv->want_dtmf_events, pos);
		} else {
			if (IS_SET(priv->want_dtmf_events, pos)) {
				/*
				 * Detection mode changed:
				 * Disable DTMF interrupts
				 */
				LINE_DBG(SIGNAL, xpd, pos,
					"DAHDI_TONEDETECT: "
					"Disable Hardware DTMF\n");
				SLIC_DIRECT_REQUEST(xbus, xpd, pos, SLIC_WRITE,
						    0x17, 0);
			}
			BIT_CLR(priv->want_dtmf_events, pos);
		}
		if (val & DAHDI_TONEDETECT_MUTE) {
			BIT_SET(priv->want_dtmf_mute, pos);
		} else {
			BIT_CLR(priv->want_dtmf_mute, pos);
			__do_mute_dtmf(xpd, pos, 0);
		}
		spin_unlock_irqrestore(&xpd->lock, flags);
		return 0;
	case DAHDI_SETPOLARITY:
		if (get_user(val, (int __user *)arg))
			return -EFAULT;
		/*
		 * Asterisk may send us this if chan_dahdi config
		 * has "hanguponpolarityswitch=yes" to notify
		 * that the other side has hanged up.
		 *
		 * This has no effect on normal phone (but we may
		 * be connected to another FXO equipment).
		 * note that this chan_dahdi settings has different
		 * meaning for FXO, where it signals polarity
		 * reversal *detection* logic.
		 *
		 * It seems that sometimes we get this from
		 * asterisk in wrong state (e.g: while ringing).
		 * In these cases, silently ignore it.
		 */
		if (priv->lasttxhook[pos] == FXS_LINE_RING
		    || priv->lasttxhook[pos] == FXS_LINE_OPEN) {
			LINE_DBG(SIGNAL, xpd, pos,
				"DAHDI_SETPOLARITY: %s Cannot change "
				"when lasttxhook=0x%X\n",
				(val) ? "ON" : "OFF", priv->lasttxhook[pos]);
			return -EINVAL;
		}
		LINE_DBG(SIGNAL, xpd, pos, "DAHDI_SETPOLARITY: %s\n",
			 (val) ? "ON" : "OFF");
		if ((val && !reversepolarity) || (!val && reversepolarity))
			priv->lasttxhook[pos] |= FXS_LINE_RING;
		else
			priv->lasttxhook[pos] &= ~FXS_LINE_RING;
		linefeed_control(xbus, xpd, pos, priv->lasttxhook[pos]);
		return 0;
	case DAHDI_VMWI_CONFIG:
		if (set_vmwi(xpd, pos, arg) < 0)
			return -EINVAL;
		return 0;
	case DAHDI_VMWI:	/* message-waiting led control */
		if (get_user(val, (int __user *)arg))
			return -EFAULT;
		if (!vmwi_ioctl) {
			static bool notified;

			if (!notified++)
				LINE_NOTICE(xpd, pos,
					"Got DAHDI_VMWI notification "
					"but vmwi_ioctl parameter is off. "
					"Ignoring.\n");
			return 0;
		}
		/* Digital inputs/outputs don't have VM leds */
		if (IS_SET
		    (PHONEDEV(xpd).digital_inputs | PHONEDEV(xpd).
		     digital_outputs, pos))
			return 0;
		PHONEDEV(xpd).msg_waiting[pos] = val;
		LINE_DBG(SIGNAL, xpd, pos, "DAHDI_VMWI: %s\n",
			 (val) ? "yes" : "no");
		return 0;
	default:
		report_bad_ioctl(THIS_MODULE->name, xpd, pos, cmd);
	}
	return -ENOTTY;
}

static int FXS_card_open(xpd_t *xpd, lineno_t chan)
{
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (IS_OFFHOOK(xpd, chan))
		LINE_NOTICE(xpd, chan, "Already offhook during open. OK.\n");
	else
		LINE_DBG(SIGNAL, xpd, chan, "is onhook\n");
	/*
	 * Delegate updating dahdi to FXS_card_tick():
	 *   The problem is that dahdi_hooksig() is spinlocking the channel and
	 *   we are called by dahdi with the spinlock already held on the
	 *   same channel.
	 */
	BIT_SET(priv->update_offhook_state, chan);
	return 0;
}

static int FXS_card_close(xpd_t *xpd, lineno_t chan)
{
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	LINE_DBG(GENERAL, xpd, chan, "\n");
	priv = xpd->priv;
	priv->idletxhookstate[chan] = FXS_LINE_POL_ACTIVE;
	return 0;
}

#ifdef	POLL_DIGITAL_INPUTS
/*
 * INPUT polling is done via SLIC register 0x06 (same as LEDS):
 *         7     6     5     4     3     2     1     0
 *	+-----+-----+-----+-----+-----+-----+-----+-----+
 *	| I1  | I3  |     |     | I2  | I4  |     |     |
 *	+-----+-----+-----+-----+-----+-----+-----+-----+
 *
 */
static int input_channels[] = { 6, 7, 2, 3 };	// Slic numbers of input relays

static void poll_inputs(xpd_t *xpd)
{
	int i;

	BUG_ON(xpd->xbus_idx != 0);	// Only unit #0 has digital inputs
	for (i = 0; i < ARRAY_SIZE(input_channels); i++) {
		__u8 pos = input_channels[i];

		SLIC_DIRECT_REQUEST(xpd->xbus, xpd, pos, SLIC_READ, 0x06, 0);
	}
}
#endif

static void handle_linefeed(xpd_t *xpd)
{
	struct FXS_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	for_each_line(xpd, i) {
		if (priv->lasttxhook[i] == FXS_LINE_RING
		    && !IS_SET(priv->neon_blinking, i)) {
			/* RINGing, prepare for OHT */
			priv->ohttimer[i] = OHT_TIMER;
			priv->idletxhookstate[i] = FXS_LINE_POL_OHTRANS;
		} else {
			if (priv->ohttimer[i]) {
				priv->ohttimer[i]--;
				if (!priv->ohttimer[i]) {
					LINE_DBG(SIGNAL, xpd, i,
						 "ohttimer expired\n");
					priv->idletxhookstate[i] =
					    FXS_LINE_POL_ACTIVE;
					oht_pcm(xpd, i, 0);
					vmwi_search(xpd, i, 0);
					if (priv->lasttxhook[i] ==
					    FXS_LINE_POL_OHTRANS) {
						/* Apply the change if appropriate */
						linefeed_control(xpd->xbus, xpd,
								 i,
								 FXS_LINE_POL_ACTIVE);
					}
				}
			}
		}
	}
}

/*
 * Optimized memcmp() like function. Only test for equality (true/false).
 * This optimization reduced the detect_vmwi() runtime by a factor of 3.
 */
static inline bool mem_equal(const char a[], const char b[], size_t len)
{
	int i;

	for (i = 0; i < len; i++)
		if (a[i] != b[i])
			return 0;
	return 1;
}

/*
 * Detect Voice Mail Waiting Indication
 */
static void detect_vmwi(xpd_t *xpd)
{
	struct FXS_priv_data *priv;
	xbus_t *xbus;
	static const __u8 FSK_COMMON_PATTERN[] =
	    { 0xA8, 0x49, 0x22, 0x3B, 0x9F, 0xFF, 0x1F, 0xBB };
	static const __u8 FSK_ON_PATTERN[] =
	    { 0xA2, 0x2C, 0x1F, 0x2C, 0xBB, 0xA1, 0xA5, 0xFF };
	static const __u8 FSK_OFF_PATTERN[] =
	    { 0xA2, 0x2C, 0x28, 0xA5, 0xB1, 0x21, 0x49, 0x9F };
	int i;
	xpp_line_t ignore_mask;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	priv = xpd->priv;
	BUG_ON(!priv);
	ignore_mask =
		PHONEDEV(xpd).offhook_state |
		~(PHONEDEV(xpd).oht_pcm_pass) |
		~(priv->search_fsk_pattern) |
		PHONEDEV(xpd).digital_inputs |
		PHONEDEV(xpd).digital_outputs;
	for_each_line(xpd, i) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);
		__u8 *writechunk = chan->writechunk;

		if (IS_SET(ignore_mask, i))
			continue;
#if 0
		if (writechunk[0] != 0x7F && writechunk[0] != 0) {
			int j;

			LINE_DBG(GENERAL, xpd, i, "MSG:");
			for (j = 0; j < DAHDI_CHUNKSIZE; j++) {
				if (debug)
					printk(" %02X", writechunk[j]);
			}
			if (debug)
				printk("\n");
		}
#endif
		if (unlikely
		    (mem_equal
		     (writechunk, FSK_COMMON_PATTERN, DAHDI_CHUNKSIZE))) {
			LINE_DBG(SIGNAL, xpd, i,
				"Found common FSK pattern. "
				"Start looking for ON/OFF patterns.\n");
			BIT_SET(priv->found_fsk_pattern, i);
		} else if (unlikely(IS_SET(priv->found_fsk_pattern, i))) {
			BIT_CLR(priv->found_fsk_pattern, i);
			oht_pcm(xpd, i, 0);
			if (unlikely
			    (mem_equal
			     (writechunk, FSK_ON_PATTERN, DAHDI_CHUNKSIZE))) {
				LINE_DBG(SIGNAL, xpd, i, "MSG WAITING ON\n");
				PHONEDEV(xpd).msg_waiting[i] = 1;
				start_stop_vm_led(xbus, xpd, i);
			} else
			    if (unlikely
				(mem_equal
				 (writechunk, FSK_OFF_PATTERN,
				  DAHDI_CHUNKSIZE))) {
				LINE_DBG(SIGNAL, xpd, i, "MSG WAITING OFF\n");
				PHONEDEV(xpd).msg_waiting[i] = 0;
				start_stop_vm_led(xbus, xpd, i);
			} else {
				int j;

				LINE_NOTICE(xpd, i, "MSG WAITING Unexpected:");
				for (j = 0; j < DAHDI_CHUNKSIZE; j++)
					printk(" %02X", writechunk[j]);
				printk("\n");
			}
		}
	}
}

static int FXS_card_tick(xbus_t *xbus, xpd_t *xpd)
{
	struct FXS_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
#ifdef	POLL_DIGITAL_INPUTS
	if (poll_digital_inputs && PHONEDEV(xpd).digital_inputs) {
		if ((xpd->timer_count % poll_digital_inputs) == 0)
			poll_inputs(xpd);
	}
#endif
	handle_fxs_leds(xpd);
	handle_linefeed(xpd);
	/*
	 * Hack alert (FIXME):
	 *   Asterisk did FXS_card_open() and we wanted to report
	 *   offhook state. However, the channel is spinlocked by dahdi
	 *   so we marked it in the priv->update_offhook_state mask and
	 *   now we take care of notification to dahdi and Asterisk
	 */
	if (priv->update_offhook_state) {
		enum dahdi_rxsig rxsig;
		int i;

		for_each_line(xpd, i) {
			if (!IS_SET(priv->update_offhook_state, i))
				continue;
			rxsig =
			    IS_OFFHOOK(xpd,
				       i) ? DAHDI_RXSIG_OFFHOOK :
			    DAHDI_RXSIG_ONHOOK;
			/* Notify after open() */
			notify_rxsig(xpd, i, rxsig);
			BIT_CLR(priv->update_offhook_state, i);
		}
	}
	if (SPAN_REGISTERED(xpd)) {
		if (!vmwi_ioctl && priv->search_fsk_pattern)
			detect_vmwi(xpd);	/* Detect via FSK modulation */
	}
	return 0;
}

/*---------------- FXS: HOST COMMANDS -------------------------------------*/

/*---------------- FXS: Astribank Reply Handlers --------------------------*/

/*
 * Should be called with spinlocked XPD
 */
static void process_hookstate(xpd_t *xpd, xpp_line_t offhook,
			      xpp_line_t change_mask)
{
	xbus_t *xbus;
	struct FXS_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	BUG_ON(PHONEDEV(xpd).direction != TO_PHONE);
	xbus = xpd->xbus;
	priv = xpd->priv;
	XPD_DBG(SIGNAL, xpd, "offhook=0x%X change_mask=0x%X\n", offhook,
		change_mask);
	for_each_line(xpd, i) {
		if (IS_SET(PHONEDEV(xpd).digital_outputs, i)
		    || IS_SET(PHONEDEV(xpd).digital_inputs, i))
			continue;
		if (IS_SET(change_mask, i)) {
			PHONEDEV(xpd).ringing[i] = 0;	/* No more ringing... */
#ifdef	WITH_METERING
			metering_gen(xpd, i, 0);	/* Stop metering... */
#endif
			MARK_BLINK(priv, i, LED_GREEN, 0);
			/*
			 * Reset our previous DTMF memories...
			 */
			BIT_CLR(priv->prev_key_down, i);
			priv->prev_key_time[i].tv_sec =
			    priv->prev_key_time[i].tv_usec = 0L;
			if (IS_SET(offhook, i)) {
				LINE_DBG(SIGNAL, xpd, i, "OFFHOOK\n");
				MARK_ON(priv, i, LED_GREEN);
				hookstate_changed(xpd, i, 1);
			} else {
				LINE_DBG(SIGNAL, xpd, i, "ONHOOK\n");
				MARK_OFF(priv, i, LED_GREEN);
				hookstate_changed(xpd, i, 0);
			}
			/*
			 * Must switch to low power. In high power, an ONHOOK
			 * won't be detected.
			 */
			do_chan_power(xbus, xpd, i, 0);
		}
	}
}

HANDLER_DEF(FXS, SIG_CHANGED)
{
	xpp_line_t sig_status =
	    RPACKET_FIELD(pack, FXS, SIG_CHANGED, sig_status);
	xpp_line_t sig_toggles =
	    RPACKET_FIELD(pack, FXS, SIG_CHANGED, sig_toggles);
	unsigned long flags;

	BUG_ON(!xpd);
	BUG_ON(PHONEDEV(xpd).direction != TO_PHONE);
	XPD_DBG(SIGNAL, xpd, "(PHONE) sig_toggles=0x%04X sig_status=0x%04X\n",
		sig_toggles, sig_status);
#if 0
	Is this needed ? for_each_line(xpd, i) {
		// Power down (prevent overheating!!!)
		if (IS_SET(sig_toggles, i))
			do_chan_power(xpd->xbus, xpd, BIT(i), 0);
	}
#endif
	spin_lock_irqsave(&xpd->lock, flags);
	process_hookstate(xpd, sig_status, sig_toggles);
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

#ifdef	POLL_DIGITAL_INPUTS
static void process_digital_inputs(xpd_t *xpd, const reg_cmd_t *info)
{
	int i;
	bool offhook = (REG_FIELD(info, data_low) & 0x1) == 0;
	xpp_line_t lines = BIT(info->portnum);

	/* Sanity check */
	if (!PHONEDEV(xpd).digital_inputs) {
		XPD_NOTICE(xpd, "%s called without digital inputs. Ignored\n",
			   __func__);
		return;
	}
	/* Map SLIC number into line number */
	for (i = 0; i < ARRAY_SIZE(input_channels); i++) {
		int channo = input_channels[i];
		int newchanno;

		if (IS_SET(lines, channo)) {
			newchanno = PHONEDEV(xpd).channels - LINES_DIGI_INP + i;
			BIT_CLR(lines, channo);
			BIT_SET(lines, newchanno);
			/* Stop ringing. No leds for digital inputs. */
			PHONEDEV(xpd).ringing[newchanno] = 0;
			if (offhook && !IS_OFFHOOK(xpd, newchanno)) {
				LINE_DBG(SIGNAL, xpd, newchanno, "OFFHOOK\n");
				hookstate_changed(xpd, newchanno, 1);
			} else if (!offhook && IS_OFFHOOK(xpd, newchanno)) {
				LINE_DBG(SIGNAL, xpd, newchanno, "ONHOOK\n");
				hookstate_changed(xpd, newchanno, 0);
			}
		}
	}
}
#endif

static const char dtmf_digits[] = {
	'D', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '#',
	'A', 'B', 'C'
};

/*
 * This function is called with spinlocked XPD
 */
static void process_dtmf(xpd_t *xpd, uint portnum, __u8 val)
{
	__u8 digit;
	bool key_down = val & 0x10;
	bool want_mute;
	bool want_event;
	struct FXS_priv_data *priv;
	struct timeval now;
	int msec = 0;

	if (!dtmf_detection)
		return;
	if (!SPAN_REGISTERED(xpd))
		return;
	priv = xpd->priv;
	val &= 0xF;
	digit = dtmf_digits[val];
	want_mute = IS_SET(priv->want_dtmf_mute, portnum);
	want_event = IS_SET(priv->want_dtmf_events, portnum);
	if (!IS_SET(priv->prev_key_down, portnum) && !key_down)
		LINE_NOTICE(xpd, portnum, "DTMF: duplicate UP (%c)\n", digit);
	if (key_down)
		BIT_SET(priv->prev_key_down, portnum);
	else
		BIT_CLR(priv->prev_key_down, portnum);
	do_gettimeofday(&now);
	if (priv->prev_key_time[portnum].tv_sec != 0)
		msec = usec_diff(&now, &priv->prev_key_time[portnum]) / 1000;
	priv->prev_key_time[portnum] = now;
	LINE_DBG(SIGNAL, xpd, portnum,
		"[%lu.%06lu] DTMF digit %-4s '%c' "
		"(val=%d, want_mute=%s want_event=%s, delta=%d msec)\n",
		now.tv_sec, now.tv_usec, (key_down) ? "DOWN" : "UP", digit,
		val, (want_mute) ? "yes" : "no", (want_event) ? "yes" : "no",
		msec);
	/*
	 * FIXME: we currently don't use the want_dtmf_mute until
	 * we are sure about the logic in Asterisk native bridging.
	 * Meanwhile, simply mute it on button press.
	 */
	if (key_down && want_mute)
		__do_mute_dtmf(xpd, portnum, 1);
	else
		__do_mute_dtmf(xpd, portnum, 0);
	if (want_event) {
		int event =
		    (key_down) ? DAHDI_EVENT_DTMFDOWN : DAHDI_EVENT_DTMFUP;

		dahdi_qevent_lock(XPD_CHAN(xpd, portnum), event | digit);
	}
}

static int FXS_card_register_reply(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *info)
{
	unsigned long flags;
	struct FXS_priv_data *priv;
	__u8 regnum;
	bool indirect;

	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	BUG_ON(!priv);
	indirect = (REG_FIELD(info, regnum) == 0x1E);
	regnum = (indirect) ? REG_FIELD(info, subreg) : REG_FIELD(info, regnum);
	XPD_DBG(REGS, xpd, "%s reg_num=0x%X, dataL=0x%X dataH=0x%X\n",
		(indirect) ? "I" : "D", regnum, REG_FIELD(info, data_low),
		REG_FIELD(info, data_high));
	if (!indirect && regnum == REG_DTMF_DECODE) {
		__u8 val = REG_FIELD(info, data_low);

		process_dtmf(xpd, info->portnum, val);
	}
#ifdef	POLL_DIGITAL_INPUTS
	/*
	 * Process digital inputs polling results
	 */
	else if (!indirect && regnum == REG_DIGITAL_IOCTRL)
		process_digital_inputs(xpd, info);
#endif
	else if (!indirect && regnum == REG_LOOPCLOSURE) {	/* OFFHOOK ? */
		__u8 val = REG_FIELD(info, data_low);
		xpp_line_t mask = BIT(info->portnum);
		xpp_line_t offhook;

		/*
		 * Validate reply. Non-existing/disabled ports
		 * will reply with 0xFF. Ignore these.
		 */
		if ((val & REG_LOOPCLOSURE_ZERO) == 0) {
			offhook = (val & REG_LOOPCLOSURE_LCR) ? mask : 0;
			LINE_DBG(SIGNAL, xpd, info->portnum,
				"REG_LOOPCLOSURE: dataL=0x%X "
				"(offhook=0x%X mask=0x%X)\n",
				val, offhook, mask);
			process_hookstate(xpd, offhook, mask);
		}
	} else {
#if 0
		XPD_NOTICE(xpd,
			"Spurious register reply(ignored): "
			"%s reg_num=0x%X, dataL=0x%X dataH=0x%X\n",
			(indirect) ? "I" : "D",
			regnum, REG_FIELD(info, data_low),
			REG_FIELD(info, data_high));
#endif
	}
	/*
	 * Update /proc info only if reply relate to the last slic
	 * read request
	 */
	if (REG_FIELD(&xpd->requested_reply, regnum) ==
			REG_FIELD(info, regnum)
		&& REG_FIELD(&xpd->requested_reply, do_subreg) ==
			REG_FIELD(info, do_subreg)
		&& REG_FIELD(&xpd->requested_reply, subreg) ==
			REG_FIELD(info, subreg)) {
		xpd->last_reply = *info;
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static int FXS_card_state(xpd_t *xpd, bool on)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	return 0;
}

static const struct xops fxs_xops = {
	.card_new = FXS_card_new,
	.card_init = FXS_card_init,
	.card_remove = FXS_card_remove,
	.card_tick = FXS_card_tick,
	.card_register_reply = FXS_card_register_reply,
};

static const struct phoneops fxs_phoneops = {
	.card_dahdi_preregistration = FXS_card_dahdi_preregistration,
	.card_dahdi_postregistration = FXS_card_dahdi_postregistration,
	.card_hooksig = FXS_card_hooksig,
	.card_pcm_recompute = generic_card_pcm_recompute,
	.card_pcm_fromspan = generic_card_pcm_fromspan,
	.card_pcm_tospan = generic_card_pcm_tospan,
	.card_timing_priority = generic_timing_priority,
	.echocancel_timeslot = generic_echocancel_timeslot,
	.echocancel_setmask = generic_echocancel_setmask,
	.card_open = FXS_card_open,
	.card_close = FXS_card_close,
	.card_ioctl = FXS_card_ioctl,
	.card_state = FXS_card_state,
};

static xproto_table_t PROTO_TABLE(FXS) = {
	.owner = THIS_MODULE,
	.entries = {
		/*      Prototable      Card    Opcode          */
		XENTRY(	FXS,		FXS,	SIG_CHANGED	),
	},
	.name = "FXS",	/* protocol name */
	.ports_per_subunit = 8,
	.type = XPD_TYPE_FXS,
	.xops = &fxs_xops,
	.phoneops = &fxs_phoneops,
	.packet_is_valid = fxs_packet_is_valid,
	.packet_dump = fxs_packet_dump,
};

static bool fxs_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe;

	// DBG(GENERAL, "\n");
	xe = xproto_card_entry(&PROTO_TABLE(FXS), XPACKET_OP(pack));
	return xe != NULL;
}

static void fxs_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

/*------------------------- SLIC Handling --------------------------*/

#ifdef	CONFIG_PROC_FS
static int proc_fxs_info_show(struct seq_file *sfile, void *not_used)
{
	unsigned long flags;
	xpd_t *xpd = sfile->private;
	struct FXS_priv_data *priv;
	int i;
	int led;

	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	BUG_ON(!priv);
	seq_printf(sfile, "%-12s", "Channel:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d", i);
	}
	seq_printf(sfile, "\n%-12s", "");
	for_each_line(xpd, i) {
		char *chan_type;

		if (IS_SET(PHONEDEV(xpd).digital_outputs, i))
			chan_type = "out";
		else if (IS_SET(PHONEDEV(xpd).digital_inputs, i))
			chan_type = "in";
		else
			chan_type = "";
		seq_printf(sfile, "%4s", chan_type);
	}
	seq_printf(sfile, "\n%-12s", "idletxhook:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d", priv->idletxhookstate[i]);
	}
	seq_printf(sfile, "\n%-12s", "lasttxhook:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d", priv->lasttxhook[i]);
	}
	seq_printf(sfile, "\n%-12s", "ohttimer:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d", priv->ohttimer[i]);
	}
	seq_printf(sfile, "\n%-12s", "neon_blink:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d",
			    IS_SET(priv->neon_blinking, i));
	}
	seq_printf(sfile, "\n%-12s", "search_fsk:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d",
			    IS_SET(priv->search_fsk_pattern, i));
	}
	seq_printf(sfile, "\n%-12s", "vbat_h:");
	for_each_line(xpd, i) {
		seq_printf(sfile, "%4d",
			test_bit(i, (unsigned long *)&priv->vbat_h));
	}
	seq_printf(sfile, "\n");
	for (led = 0; led < NUM_LEDS; led++) {
		seq_printf(sfile, "\nLED #%d\t%-12s: ",
			led, "ledstate");
		for_each_line(xpd, i) {
			if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
			    && !IS_SET(PHONEDEV(xpd).digital_inputs, i))
				seq_printf(sfile, "%d ",
					    IS_SET(priv->ledstate[led], i));
		}
		seq_printf(sfile, "\nLED #%d\t%-12s: ",
			led, "ledcontrol");
		for_each_line(xpd, i) {
			if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
			    && !IS_SET(PHONEDEV(xpd).digital_inputs, i))
				seq_printf(sfile, "%d ",
					    IS_SET(priv->ledcontrol[led], i));
		}
		seq_printf(sfile, "\nLED #%d\t%-12s: ",
			led, "led_counter");
		for_each_line(xpd, i) {
			if (!IS_SET(PHONEDEV(xpd).digital_outputs, i)
			    && !IS_SET(PHONEDEV(xpd).digital_inputs, i))
				seq_printf(sfile, "%d ",
					    LED_COUNTER(priv, i, led));
		}
	}
	seq_printf(sfile, "\n");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static int proc_fxs_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_fxs_info_show, PDE_DATA(inode));
}

static const struct file_operations proc_fxs_info_ops = {
	.owner		= THIS_MODULE,
	.open		= proc_fxs_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef	WITH_METERING
static ssize_t proc_xpd_metering_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *offset)
{
	xpd_t *xpd = file->private_data;
	char buf[MAX_PROC_WRITE];
	lineno_t chan;
	int num;
	int ret;

	if (!xpd)
		return -ENODEV;
	if (count >= MAX_PROC_WRITE - 1) {
		XPD_ERR(xpd, "Metering string too long (%zu)\n", count);
		return -EINVAL;
	}
	if (copy_from_user(&buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';
	ret = sscanf(buf, "%d", &num);
	if (ret != 1) {
		XPD_ERR(xpd, "Metering value should be number. Got '%s'\n",
			buf);
		return -EINVAL;
	}
	chan = num;
	if (chan != PORT_BROADCAST && chan > xpd->channels) {
		XPD_ERR(xpd, "Metering tone: bad channel number %d\n", chan);
		return -EINVAL;
	}
	if ((ret = metering_gen(xpd, chan, 1)) < 0) {
		XPD_ERR(xpd, "Failed sending metering tone\n");
		return ret;
	}
	return count;
}

static int proc_xpd_metering_open(struct inode *inode, struct file *file)
{
	file->private_data = PDE_DATA(inode);
}

static const struct file_operations proc_xpd_metering_ops = {
	.owner		= THIS_MODULE,
	.open		= proc_xpd_metering_open,
	.write		= proc_xpd_metering_write,
	.release	= single_release,
};
#endif
#endif

static DEVICE_ATTR_READER(fxs_ring_registers_show, dev, buf)
{
	xpd_t *xpd;
	struct FXS_priv_data *priv;
	unsigned long flags;
	const struct ring_reg_params *p;
	const struct byte_pair *v;
	enum ring_types rtype;
	int len = 0;
	int i;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	len += sprintf(buf + len, "#   Reg#: D/I\tNEON     \tTRAPEZ   \tNORMAL   \n");
	for (i = 0; i < ARRAY_SIZE(ring_parameters); i++) {
		p = &ring_parameters[i];
		len += sprintf(buf + len, "[%d] 0x%02X: %c",
			i, p->regno, (p->is_indirect) ? 'I' : 'D');
		for (rtype = RING_TYPE_NEON; rtype <= RING_TYPE_NORMAL; rtype++) {
			v = &(p->values[rtype]);
			if (p->is_indirect)
				len += sprintf(buf + len, "\t0x%02X 0x%02X",
					v->h_val, v->l_val);
			else
				len += sprintf(buf + len, "\t0x%02X ----",
					v->l_val);
		}
		len += sprintf(buf + len, "\n");
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(fxs_ring_registers_store, dev, buf, count)
{
	xpd_t *xpd;
	struct FXS_priv_data *priv;
	unsigned long flags;
	char rtype_name[MAX_PROC_WRITE];
	enum ring_types rtype;
	struct ring_reg_params *params;
	struct byte_pair *v;
	int regno;
	int h_val;
	int l_val;
	int ret;
	int i;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	ret = sscanf(buf, "%10s %X %X %X\n",
		rtype_name, &regno, &h_val, &l_val);
	if (ret < 3 || ret > 4) {
		XPD_ERR(xpd, "Bad input: '%s'\n", buf);
		XPD_ERR(xpd, "# Correct input\n");
		XPD_ERR(xpd, "{NEON|TRAPEZ|NORMAL} <regno> <byte> [<byte>]\n");
		goto invalid_input;
	}
	if (strcasecmp("NEON", rtype_name) == 0)
		rtype = RING_TYPE_NEON;
	else if (strcasecmp("TRAPEZ", rtype_name) == 0)
		rtype = RING_TYPE_TRAPEZ;
	else if (strcasecmp("NORMAL", rtype_name) == 0)
		rtype = RING_TYPE_NORMAL;
	else {
		XPD_ERR(xpd, "Unknown ring type '%s' (NEON/TRAPEZ/NORMAL)\n",
			rtype_name);
		goto invalid_input;
	}
	params = NULL;
	for (i = 0; i < ARRAY_SIZE(ring_parameters); i++) {
		if (ring_parameters[i].regno == regno) {
			params = &ring_parameters[i];
			break;
		}
	}
	if (!params) {
		XPD_ERR(xpd, "Bad register 0x%X\n", regno);
		goto invalid_input;
	}
	if (params->is_indirect) {
		if (ret != 4) {
			XPD_ERR(xpd,
				"Missing low-byte (0x%X is indirect register)\n",
				regno);
			goto invalid_input;
		}
		XPD_INFO(xpd, "%s Indirect 0x%X <=== 0x%X 0x%X\n",
			rtype_name, regno, h_val, l_val);
	} else {
		if (ret != 3) {
			XPD_ERR(xpd,
				"Should give exactly one value (0x%X is direct register)\n",
				regno);
			goto invalid_input;
		}
		l_val = h_val;
		h_val = 0;
		XPD_INFO(xpd, "%s Direct 0x%X <=== 0x%X\n",
			rtype_name, regno, h_val);
	}
	spin_lock_irqsave(&xpd->lock, flags);
	v = &(params->values[rtype]);
	v->h_val = h_val;
	v->l_val = l_val;
	spin_unlock_irqrestore(&xpd->lock, flags);
	return count;
invalid_input:
	return -EINVAL;
}

static DEVICE_ATTR(fxs_ring_registers, S_IRUGO | S_IWUSR,
	fxs_ring_registers_show,
	fxs_ring_registers_store);

static int fxs_xpd_probe(struct device *dev)
{
	xpd_t *xpd;
	int ret;

	xpd = dev_to_xpd(dev);
	/* Is it our device? */
	if (xpd->type != XPD_TYPE_FXS) {
		XPD_ERR(xpd, "drop suggestion for %s (%d)\n", dev_name(dev),
			xpd->type);
		return -EINVAL;
	}
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	ret = device_create_file(dev, &dev_attr_fxs_ring_registers);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(fxs_ring_registers) failed: %d\n",
			__func__, ret);
		goto fail_fxs_ring_registers;
	}
	return 0;
fail_fxs_ring_registers:
	return ret;
}

static int fxs_xpd_remove(struct device *dev)
{
	xpd_t *xpd;

	xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	device_remove_file(dev, &dev_attr_fxs_ring_registers);
	return 0;
}

static struct xpd_driver fxs_driver = {
	.type = XPD_TYPE_FXS,
	.driver = {
		   .name = "fxs",
		   .owner = THIS_MODULE,
		   .probe = fxs_xpd_probe,
		   .remove = fxs_xpd_remove}
};

static int __init card_fxs_startup(void)
{
	int ret;

	if ((ret = xpd_driver_register(&fxs_driver.driver)) < 0)
		return ret;

	INFO("revision %s\n", XPP_VERSION);
#ifdef	POLL_DIGITAL_INPUTS
	INFO("FEATURE: with DIGITAL INPUTS support (polled every %d msec)\n",
	     poll_digital_inputs);
#else
	INFO("FEATURE: without DIGITAL INPUTS support\n");
#endif
	INFO("FEATURE: DAHDI_VMWI (HVAC only)\n");
#ifdef	WITH_METERING
	INFO("FEATURE: WITH METERING Generation\n");
#else
	INFO("FEATURE: NO METERING Generation\n");
#endif
	xproto_register(&PROTO_TABLE(FXS));
	return 0;
}

static void __exit card_fxs_cleanup(void)
{
	xproto_unregister(&PROTO_TABLE(FXS));
	xpd_driver_unregister(&fxs_driver.driver);
}

MODULE_DESCRIPTION("XPP FXS Card Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_VERSION(XPP_VERSION);
MODULE_ALIAS_XPD(XPD_TYPE_FXS);

module_init(card_fxs_startup);
module_exit(card_fxs_cleanup);
