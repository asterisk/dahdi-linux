/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
 *
 * Parts derived from Cologne demo driver for the chip.
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
#include "card_bri.h"
#include "dahdi_debug.h"
#include "xbus-core.h"

static const char rcsid[] = "$Id$";

#ifndef	DAHDI_SIG_HARDHDLC
#error Cannot build BRI without HARDHDLC supprt
#endif

/* must be before dahdi_debug.h */
static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
static DEF_PARM(uint, poll_interval, 500, 0644,
		"Poll channel state interval in milliseconds (0 - disable)");
static DEF_PARM_BOOL(nt_keepalive, 1, 0644,
		     "Force BRI_NT to keep trying connection");

enum xhfc_states {
	ST_RESET = 0,		/* G/F0 */
	/* TE */
	ST_TE_SENSING = 2,	/* F2   */
	ST_TE_DEACTIVATED = 3,	/* F3   */
	ST_TE_SIGWAIT = 4,	/* F4   */
	ST_TE_IDENT = 5,	/* F5   */
	ST_TE_SYNCED = 6,	/* F6   */
	ST_TE_ACTIVATED = 7,	/* F7   */
	ST_TE_LOST_FRAMING = 8,	/* F8   */
	/* NT */
	ST_NT_DEACTIVATED = 1,	/* G1   */
	ST_NT_ACTIVATING = 2,	/* G2   */
	ST_NT_ACTIVATED = 3,	/* G3   */
	ST_NT_DEACTIVTING = 4,	/* G4   */
};

#ifdef CONFIG_PROC_FS
static const char *xhfc_state_name(bool is_nt, enum xhfc_states state)
{
	const char *p;

#define	_E(x)	[ST_ ## x] = #x
	static const char *te_names[] = {
		_E(RESET),
		_E(TE_SENSING),
		_E(TE_DEACTIVATED),
		_E(TE_SIGWAIT),
		_E(TE_IDENT),
		_E(TE_SYNCED),
		_E(TE_ACTIVATED),
		_E(TE_LOST_FRAMING),
	};
	static const char *nt_names[] = {
		_E(RESET),
		_E(NT_DEACTIVATED),
		_E(NT_ACTIVATING),
		_E(NT_ACTIVATED),
		_E(NT_DEACTIVTING),
	};
#undef	_E
	if (is_nt) {
		if (state > ST_NT_DEACTIVTING)
			p = "NT ???";
		else
			p = nt_names[state];
	} else {
		if (state > ST_TE_LOST_FRAMING)
			p = "TE ???";
		else
			p = te_names[state];
	}
	return p;
}
#endif

/* xhfc Layer1 physical commands */
#define HFC_L1_ACTIVATE_TE		0x01
#define HFC_L1_FORCE_DEACTIVATE_TE	0x02
#define HFC_L1_ACTIVATE_NT		0x03
#define HFC_L1_DEACTIVATE_NT		0x04

#define HFC_L1_ACTIVATING	1
#define HFC_L1_ACTIVATED	2
#define	HFC_TIMER_T1		2500
#define	HFC_TIMER_T3		8000	/* 8s activation timer T3 */
#define	HFC_TIMER_OFF		-1	/* timer disabled */

#define	A_SU_WR_STA		0x30	/* ST/Up state machine register */
#define		V_SU_LD_STA	0x10
#define	V_SU_ACT	0x60	/* start activation/deactivation        */
#define	STA_DEACTIVATE	0x40	/* start deactivation in A_SU_WR_STA */
#define	STA_ACTIVATE	0x60	/* start activation   in A_SU_WR_STA */
#define	V_SU_SET_G2_G3	0x80

#define	A_SU_RD_STA		0x30
typedef union {
	struct {
		__u8 v_su_sta:4;
		__u8 v_su_fr_sync:1;
		__u8 v_su_t2_exp:1;
		__u8 v_su_info0:1;
		__u8 v_g2_g3:1;
	} bits;
	__u8 reg;
} su_rd_sta_t;

#define	REG30_LOST	3	/* in polls */
#define	DCHAN_LOST	15000	/* in ticks */

#define	BRI_DCHAN_SIGCAP	DAHDI_SIG_HARDHDLC
#define	BRI_BCHAN_SIGCAP	(DAHDI_SIG_CLEAR | DAHDI_SIG_DACS)

#define	IS_NT(xpd)		(PHONEDEV(xpd).direction == TO_PHONE)
#define	BRI_PORT(xpd)		((xpd)->addr.subunit)

/* shift in PCM highway */
#define	SUBUNIT_PCM_SHIFT	4
#define	PCM_SHIFT(mask, sunit)	((mask) << (SUBUNIT_PCM_SHIFT * (sunit)))

/*---------------- BRI Protocol Commands ----------------------------------*/

static int write_state_register(xpd_t *xpd, __u8 value);
static bool bri_packet_is_valid(xpacket_t *pack);
static void bri_packet_dump(const char *msg, xpacket_t *pack);
#ifdef	CONFIG_PROC_FS
#ifdef DAHDI_HAVE_PROC_OPS
static const struct proc_ops proc_bri_info_ops;
#else
static const struct file_operations proc_bri_info_ops;
#endif
#endif
static int bri_spanconfig(struct file *file, struct dahdi_span *span,
			  struct dahdi_lineconfig *lc);
static int bri_chanconfig(struct file *file, struct dahdi_chan *chan,
			  int sigtype);
static int bri_startup(struct file *file, struct dahdi_span *span);
static int bri_shutdown(struct dahdi_span *span);

#define	PROC_BRI_INFO_FNAME	"bri_info"

enum led_state {
	BRI_LED_OFF = 0x0,
	BRI_LED_ON = 0x1,
	/*
	 * We blink by software from driver, so that
	 * if the driver malfunction that blink would stop.
	 */
	// BRI_LED_BLINK_SLOW   = 0x2,  /* 1/2 a second blink cycle */
	// BRI_LED_BLINK_FAST   = 0x3   /* 1/4 a second blink cycle */
};

enum bri_led_names {
	GREEN_LED = 0,
	RED_LED = 1
};

#define	NUM_LEDS	2
#define	LED_TICKS	100

struct bri_leds {
	__u8 state:2;
	__u8 led_sel:1;		/* 0 - GREEN, 1 - RED */
	__u8 reserved:5;
};

#ifndef MAX_DFRAME_LEN_L1
#define MAX_DFRAME_LEN_L1 300
#endif

#define	DCHAN_BUFSIZE	MAX_DFRAME_LEN_L1

struct BRI_priv_data {
	struct proc_dir_entry *bri_info;
	su_rd_sta_t state_register;
	bool initialized;
	bool dchan_is_open;
	int t1;			/* timer 1 for NT deactivation */
	int t3;			/* timer 3 for TE activation */
	ulong l1_flags;
	bool reg30_good;
	uint reg30_ticks;
	bool layer1_up;

	/*
	 * D-Chan: buffers + extra state info.
	 */
	atomic_t hdlc_pending;
	bool txframe_begin;

	uint tick_counter;
	uint poll_counter;
	uint dchan_tx_counter;
	uint dchan_rx_counter;
	uint dchan_rx_drops;
	bool dchan_alive;
	uint dchan_alive_ticks;
	uint dchan_notx_ticks;
	uint dchan_norx_ticks;
	enum led_state ledstate[NUM_LEDS];
};

static xproto_table_t PROTO_TABLE(BRI);

DEF_RPACKET_DATA(BRI, SET_LED,	/* Set one of the LED's */
		 struct bri_leds bri_leds;);

static /* 0x33 */ DECLARE_CMD(BRI, SET_LED, enum bri_led_names which_led,
			      enum led_state to_led_state);

#define	DO_LED(xpd, which, tostate) \
		CALL_PROTO(BRI, SET_LED, (xpd)->xbus, (xpd), (which), (tostate))

#define DEBUG_BUF_SIZE (100)
static void dump_hex_buf(xpd_t *xpd, char *msg, __u8 *buf, size_t len)
{
	char debug_buf[DEBUG_BUF_SIZE + 1];
	int i;
	int n = 0;

	debug_buf[0] = '\0';
	for (i = 0; i < len && n < DEBUG_BUF_SIZE; i++)
		n += snprintf(&debug_buf[n], DEBUG_BUF_SIZE - n, "%02X ",
			      buf[i]);
	XPD_NOTICE(xpd, "%s[0..%zd]: %s%s\n", msg, len - 1, debug_buf,
		   (n >= DEBUG_BUF_SIZE) ? "..." : "");
}

static void dump_dchan_packet(xpd_t *xpd, bool transmit, __u8 *buf, int len)
{
	struct BRI_priv_data *priv;
	char msgbuf[MAX_PROC_WRITE];
	char ftype = '?';
	char *direction;
	int frame_begin;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (transmit) {
		direction = "TX";
		frame_begin = priv->txframe_begin;
	} else {
		direction = "RX";
		frame_begin = 1;
	}
	if (frame_begin) {	/* Packet start */
		if (!IS_SET(buf[0], 7))
			ftype = 'I';	/* Information */
		else if (IS_SET(buf[0], 7) && !IS_SET(buf[0], 6))
			ftype = 'S';	/* Supervisory */
		else if (IS_SET(buf[0], 7) && IS_SET(buf[0], 6))
			ftype = 'U';	/* Unnumbered */
		else
			XPD_NOTICE(xpd, "Unknown frame type 0x%X\n", buf[0]);

		snprintf(msgbuf, MAX_PROC_WRITE, "D-Chan %s = (%c) ", direction,
			 ftype);
	} else {
		snprintf(msgbuf, MAX_PROC_WRITE, "D-Chan %s =     ", direction);
	}
	dump_hex_buf(xpd, msgbuf, buf, len);
}

static void set_bri_timer(xpd_t *xpd, const char *name, int *bri_timer,
			  int value)
{
	if (value == HFC_TIMER_OFF)
		XPD_DBG(SIGNAL, xpd, "Timer %s DISABLE\n", name);
	else
		XPD_DBG(SIGNAL, xpd, "Timer %s: set to %d\n", name, value);
	*bri_timer = value;
}

static void dchan_state(xpd_t *xpd, bool up)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->dchan_alive == up)
		return;
	if (up) {
		XPD_DBG(SIGNAL, xpd, "STATE CHANGE: D-Channel RUNNING\n");
		priv->dchan_alive = 1;
	} else {
		XPD_DBG(SIGNAL, xpd, "STATE CHANGE: D-Channel STOPPED\n");
		priv->dchan_rx_counter = priv->dchan_tx_counter =
		    priv->dchan_rx_drops = 0;
		priv->dchan_alive = 0;
		priv->dchan_alive_ticks = 0;
	}
}

static void layer1_state(xpd_t *xpd, bool up)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->layer1_up == up)
		return;
	priv->layer1_up = up;
	XPD_DBG(SIGNAL, xpd, "STATE CHANGE: Layer1 %s\n", (up) ? "UP" : "DOWN");
	if (!up)
		dchan_state(xpd, 0);
}

static void te_activation(xpd_t *xpd, bool on)
{
	struct BRI_priv_data *priv;
	__u8 curr_state;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	curr_state = priv->state_register.bits.v_su_sta;
	XPD_DBG(SIGNAL, xpd, "%s\n", (on) ? "ON" : "OFF");
	if (on) {
		if (curr_state == ST_TE_DEACTIVATED) {
			XPD_DBG(SIGNAL, xpd, "HFC_L1_ACTIVATE_TE\n");
			set_bit(HFC_L1_ACTIVATING, &priv->l1_flags);
			write_state_register(xpd, STA_ACTIVATE);
			set_bri_timer(xpd, "T3", &priv->t3, HFC_TIMER_T3);
		} else {
			XPD_DBG(SIGNAL, xpd,
				"HFC_L1_ACTIVATE_TE (state %d, ignored)\n",
				curr_state);
		}
	} else {		/* happen only because of T3 expiry */
		switch (curr_state) {
		case ST_TE_DEACTIVATED:	/* F3   */
		case ST_TE_SYNCED:	/* F6   */
		case ST_TE_ACTIVATED:	/* F7   */
			XPD_DBG(SIGNAL, xpd,
				"HFC_L1_FORCE_DEACTIVATE_TE "
				"(state %d, ignored)\n",
				curr_state);
			break;
		case ST_TE_SIGWAIT:	/* F4   */
		case ST_TE_IDENT:	/* F5   */
		case ST_TE_LOST_FRAMING:	/* F8   */
			XPD_DBG(SIGNAL, xpd, "HFC_L1_FORCE_DEACTIVATE_TE\n");
			write_state_register(xpd, STA_DEACTIVATE);
			break;
		default:
			XPD_NOTICE(xpd, "Bad TE state: %d\n", curr_state);
			break;
		}
	}
}

static void nt_activation(xpd_t *xpd, bool on)
{
	struct BRI_priv_data *priv;
	__u8 curr_state;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	curr_state = priv->state_register.bits.v_su_sta;
	XPD_DBG(SIGNAL, xpd, "%s\n", (on) ? "ON" : "OFF");
	if (on) {
		switch (curr_state) {
		case ST_RESET:	/* F/G 0 */
		case ST_NT_DEACTIVATED:	/* G1 */
		case ST_NT_DEACTIVTING:	/* G4 */
			XPD_DBG(SIGNAL, xpd, "HFC_L1_ACTIVATE_NT\n");
			set_bri_timer(xpd, "T1", &priv->t1, HFC_TIMER_T1);
			set_bit(HFC_L1_ACTIVATING, &priv->l1_flags);
			write_state_register(xpd, STA_ACTIVATE);
			break;
		case ST_NT_ACTIVATING:	/* G2 */
		case ST_NT_ACTIVATED:	/* G3 */
			XPD_DBG(SIGNAL, xpd,
				"HFC_L1_ACTIVATE_NT (in state %d, ignored)\n",
				curr_state);
			break;
		}
	} else {
		switch (curr_state) {
		case ST_RESET:	/* F/G 0 */
		case ST_NT_DEACTIVATED:	/* G1 */
		case ST_NT_DEACTIVTING:	/* G4 */
			XPD_DBG(SIGNAL, xpd,
				"HFC_L1_DEACTIVATE_NT (in state %d, ignored)\n",
				curr_state);
			break;
		case ST_NT_ACTIVATING:	/* G2 */
		case ST_NT_ACTIVATED:	/* G3 */
			XPD_DBG(SIGNAL, xpd, "HFC_L1_DEACTIVATE_NT\n");
			write_state_register(xpd, STA_DEACTIVATE);
			break;
		default:
			XPD_NOTICE(xpd, "Bad NT state: %d\n", curr_state);
			break;
		}
	}
}

/*
 * D-Chan receive
 */
static int bri_check_stat(xpd_t *xpd, struct dahdi_chan *dchan, __u8 *buf,
			  int len)
{
	struct BRI_priv_data *priv;
	__u8 status;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (len <= 0) {
		XPD_NOTICE(xpd, "D-Chan RX DROP: short frame (len=%d)\n", len);
		dahdi_hdlc_abort(dchan, DAHDI_EVENT_ABORT);
		return -EPROTO;
	}
	status = buf[len - 1];
	if (status) {
		int event = DAHDI_EVENT_ABORT;

		if (status == 0xFF) {
			XPD_NOTICE(xpd, "D-Chan RX DROP: ABORT: %d\n", status);
		} else {
			XPD_NOTICE(xpd, "D-Chan RX DROP: BADFCS: %d\n", status);
			event = DAHDI_EVENT_BADFCS;
		}
		dump_hex_buf(xpd, "D-Chan RX:    current packet", buf, len);
		dahdi_hdlc_abort(dchan, event);
		return -EPROTO;
	}
	return 0;
}

static int rx_dchan(xpd_t *xpd, reg_cmd_t *regcmd)
{
	struct BRI_priv_data *priv;
	__u8 *src;
	struct dahdi_chan *dchan;
	uint len;
	bool eoframe;
	int ret = 0;

	src = REG_XDATA(regcmd);
	len = regcmd->h.bytes;
	eoframe = regcmd->h.eoframe;
	if (len <= 0)
		return 0;
	if (!SPAN_REGISTERED(xpd))	/* Nowhere to copy data */
		return 0;
	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	dchan = XPD_CHAN(xpd, 2);
	if (!IS_OFFHOOK(xpd, 2)) {	/* D-chan is used? */
		static int rate_limit;

		if ((rate_limit++ % 1000) == 0)
			XPD_DBG(SIGNAL, xpd, "D-Chan unused\n");
		goto out;
	}
	XPD_DBG(GENERAL, xpd, "D-Chan RX: eoframe=%d len=%d\n", eoframe, len);
	dahdi_hdlc_putbuf(dchan, src, (eoframe) ? len - 1 : len);
	if (!eoframe)
		goto out;
	if ((ret = bri_check_stat(xpd, dchan, src, len)) < 0)
		goto out;
	/*
	 * Tell Dahdi that we received len-1 bytes.
	 * They include the data and a 2-byte checksum.
	 * The last byte (that we don't pass on) is 0 if the
	 * checksum is correct. If it were wrong, we would drop the
	 * packet in the "if (src[len-1])" above.
	 */
	dahdi_hdlc_finish(dchan);
	priv->dchan_rx_counter++;
	priv->dchan_norx_ticks = 0;
out:
	return ret;
}

/*
 * D-Chan transmit
 */
/*
 * DAHDI calls this when it has data it wants to send to
 * the HDLC controller
 */
static void bri_hdlc_hard_xmit(struct dahdi_chan *chan)
{
	xpd_t *xpd = chan->pvt;
	struct dahdi_chan *dchan;
	struct BRI_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!priv);
	dchan = XPD_CHAN(xpd, 2);
	if (dchan == chan)
		atomic_inc(&priv->hdlc_pending);
}

static int send_dchan_frame(xpd_t *xpd, xframe_t *xframe, bool is_eof)
{
	struct BRI_priv_data *priv;
	int ret;

	XPD_DBG(COMMANDS, xpd, "eoframe=%d\n", is_eof);
	priv = xpd->priv;
	if (!test_bit(HFC_L1_ACTIVATED, &priv->l1_flags)
	    && !test_bit(HFC_L1_ACTIVATING, &priv->l1_flags)) {
		XPD_DBG(SIGNAL, xpd,
			"Want to transmit: Kick D-Channel transmiter\n");
		if (!IS_NT(xpd))
			te_activation(xpd, 1);
		else
			nt_activation(xpd, 1);
	}
	dump_xframe("send_dchan_frame", xpd->xbus, xframe, debug);
	ret = send_cmd_frame(xpd->xbus, xframe);
	if (ret < 0)
		XPD_ERR(xpd, "%s: failed sending xframe\n", __func__);
	if (is_eof) {
		atomic_dec(&priv->hdlc_pending);
		priv->dchan_tx_counter++;
		priv->txframe_begin = 1;
	} else
		priv->txframe_begin = 0;
	priv->dchan_notx_ticks = 0;
	return ret;
}

/*
 * Fill a single multibyte REGISTER_REQUEST
 */
static void fill_multibyte(xpd_t *xpd, xpacket_t *pack,
	bool eoframe, char *buf, int len)
{
	reg_cmd_t *reg_cmd;
	char *p;

	XPACKET_INIT(pack, GLOBAL, REGISTER_REQUEST, xpd->xbus_idx, 0, 0);
	XPACKET_LEN(pack) = XFRAME_CMD_LEN(REG);
	reg_cmd = &RPACKET_FIELD(pack, GLOBAL, REGISTER_REQUEST, reg_cmd);
	reg_cmd->h.bytes = len;
	reg_cmd->h.is_multibyte = 1;
	reg_cmd->h.portnum = xpd->addr.subunit;
	reg_cmd->h.eoframe = eoframe;
	p = REG_XDATA(reg_cmd);
	memcpy(p, buf, len);
	if (debug)
		dump_dchan_packet(xpd, 1, p, len);
}

/*
 * Transmit available D-Channel frames
 *
 * - FPGA firmware expect to get this as a sequence of REGISTER_REQUEST
 *   multibyte commands.
 * - The payload of each command is limited to MULTIBYTE_MAX_LEN bytes.
 * - We batch several REGISTER_REQUEST packets into a single xframe.
 * - The xframe is terminated when we get a bri "end of frame"
 *   or when the xframe is full (should not happen).
 */
static int tx_dchan(xpd_t *xpd)
{
	struct BRI_priv_data *priv;
	xframe_t *xframe;
	xpacket_t *pack;
	int packet_count;
	int eoframe;
	int ret;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (atomic_read(&priv->hdlc_pending) == 0)
		return 0;
	if (!SPAN_REGISTERED(xpd)
	    || !(PHONEDEV(xpd).span.flags & DAHDI_FLAG_RUNNING))
		return 0;
	/* Allocate frame */
	xframe = ALLOC_SEND_XFRAME(xpd->xbus);
	if (!xframe) {
		XPD_NOTICE(xpd, "%s: failed to allocate new xframe\n",
			   __func__);
		return -ENOMEM;
	}
	for (packet_count = 0, eoframe = 0; !eoframe; packet_count++) {
		int packet_len = XFRAME_CMD_LEN(REG);
		char buf[MULTIBYTE_MAX_LEN];
		int len = MULTIBYTE_MAX_LEN;

		/* Reserve packet */
		pack = xframe_next_packet(xframe, packet_len);
		if (!pack) {
			BUG_ON(!packet_count);
			/*
			 * A split. Send what we currently have.
			 */
			XPD_NOTICE(xpd, "%s: xframe is full (%d packets)\n",
				   __func__, packet_count);
			break;
		}
		/* Get data from DAHDI */
		eoframe = dahdi_hdlc_getbuf(XPD_CHAN(xpd, 2), buf, &len);
		if (len <= 0) {
			/*
			 * Already checked priv->hdlc_pending,
			 * should never get here.
			 */
			if (printk_ratelimit())
				XPD_ERR(xpd,
					"%s: hdlc_pending, but nothing "
					"to transmit?\n", __func__);
			FREE_SEND_XFRAME(xpd->xbus, xframe);
			return -EINVAL;
		}
		BUG_ON(len > MULTIBYTE_MAX_LEN);
		fill_multibyte(xpd, pack, eoframe != 0, buf, len);
	}
	return send_dchan_frame(xpd, xframe, eoframe != 0);
	return ret;
}

/*---------------- BRI: Methods -------------------------------------------*/

static void bri_proc_remove(xbus_t *xbus, xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	XPD_DBG(PROC, xpd, "\n");
#ifdef	CONFIG_PROC_FS
	if (priv->bri_info) {
		XPD_DBG(PROC, xpd, "Removing '%s'\n", PROC_BRI_INFO_FNAME);
		remove_proc_entry(PROC_BRI_INFO_FNAME, xpd->proc_xpd_dir);
	}
#endif
}

static int bri_proc_create(xbus_t *xbus, xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	XPD_DBG(PROC, xpd, "\n");
#ifdef	CONFIG_PROC_FS
	XPD_DBG(PROC, xpd, "Creating '%s'\n", PROC_BRI_INFO_FNAME);
	priv->bri_info = proc_create_data(PROC_BRI_INFO_FNAME, 0444,
				 xpd->proc_xpd_dir, &proc_bri_info_ops, xpd);
	if (!priv->bri_info) {
		XPD_ERR(xpd, "Failed to create proc file '%s'\n",
			PROC_BRI_INFO_FNAME);
		bri_proc_remove(xbus, xpd);
		return -EINVAL;
	}
	SET_PROC_DIRENTRY_OWNER(priv->bri_info);
#endif
	return 0;
}

static xpd_t *BRI_card_new(xbus_t *xbus, int unit, int subunit,
			   const xproto_table_t *proto_table,
			   const struct unit_descriptor *unit_descriptor,
			   bool to_phone)
{
	xpd_t *xpd = NULL;
	int channels = min(3, CHANNELS_PERXPD);

	if ((unit_descriptor->ports_per_chip < 1) ||
			(unit_descriptor->ports_per_chip > 4)) {
		XBUS_ERR(xbus, "Bad ports_per_chip=%d\n",
				unit_descriptor->ports_per_chip);
		return NULL;
	}
	if ((unit_descriptor->numchips) < 1 ||
			(unit_descriptor->numchips > 2)) {
		XBUS_ERR(xbus, "Bad numchips=%d\n",
				unit_descriptor->numchips);
		return NULL;
	}
	XBUS_DBG(GENERAL, xbus, "\n");
	xpd =
	    xpd_alloc(xbus, unit, subunit,
		      sizeof(struct BRI_priv_data), proto_table, unit_descriptor, channels);
	if (!xpd)
		return NULL;
	PHONEDEV(xpd).direction = (to_phone) ? TO_PHONE : TO_PSTN;
	xpd->type_name = (to_phone) ? "BRI_NT" : "BRI_TE";
	xbus->sync_mode_default = SYNC_MODE_AB;
	if (bri_proc_create(xbus, xpd) < 0)
		goto err;
	return xpd;
err:
	xpd_free(xpd);
	return NULL;
}

static int BRI_card_init(xbus_t *xbus, xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	priv = xpd->priv;
	DO_LED(xpd, GREEN_LED, BRI_LED_OFF);
	DO_LED(xpd, RED_LED, BRI_LED_OFF);
	set_bri_timer(xpd, "T1", &priv->t1, HFC_TIMER_OFF);
	write_state_register(xpd, 0);	/* Enable L1 state machine */
	priv->initialized = 1;
	return 0;
}

static int BRI_card_remove(xbus_t *xbus, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	bri_proc_remove(xbus, xpd);
	return 0;
}

#ifdef	DAHDI_AUDIO_NOTIFY
static int bri_audio_notify(struct dahdi_chan *chan, int on)
{
	xpd_t *xpd = chan->pvt;
	int pos = chan->chanpos - 1;

	BUG_ON(!xpd);
	LINE_DBG(SIGNAL, xpd, pos, "BRI-AUDIO: %s\n", (on) ? "on" : "off");
	mark_offhook(xpd, pos, on);
	return 0;
}
#endif

static const struct dahdi_span_ops BRI_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = bri_spanconfig,
	.chanconfig = bri_chanconfig,
	.startup = bri_startup,
	.shutdown = bri_shutdown,
	.hdlc_hard_xmit = bri_hdlc_hard_xmit,
	.open = xpp_open,
	.close = xpp_close,
	.hooksig = xpp_hooksig,	/* Only with RBS bits */
	.ioctl = xpp_ioctl,
	.maint = xpp_maint,
	.echocan_create = xpp_echocan_create,
	.echocan_name = xpp_echocan_name,
	.assigned = xpp_span_assigned,
#ifdef	DAHDI_SYNC_TICK
	.sync_tick = dahdi_sync_tick,
#endif
#ifdef	CONFIG_DAHDI_WATCHDOG
	.watchdog = xpp_watchdog,
#endif

#ifdef	DAHDI_AUDIO_NOTIFY
	.audio_notify = bri_audio_notify,
#endif
};

static int BRI_card_dahdi_preregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct BRI_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	priv = xpd->priv;
	BUG_ON(!xbus);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	if (!on) {
		/* Nothing to do yet */
		return 0;
	}
	PHONEDEV(xpd).span.spantype =
		(PHONEDEV(xpd).direction == TO_PHONE)
			? SPANTYPE_DIGITAL_BRI_NT
			: SPANTYPE_DIGITAL_BRI_TE;
	PHONEDEV(xpd).span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_CCS;
	PHONEDEV(xpd).span.deflaw = DAHDI_LAW_ALAW;
	BIT_SET(PHONEDEV(xpd).digital_signalling, 2);	/* D-Channel */
	for_each_line(xpd, i) {
		struct dahdi_chan *cur_chan = XPD_CHAN(xpd, i);

		XPD_DBG(GENERAL, xpd, "setting BRI channel %d\n", i);
		snprintf(cur_chan->name, MAX_CHANNAME, "XPP_%s/%02d/%1d%1d/%d",
			 xpd->type_name, xbus->num, xpd->addr.unit,
			 xpd->addr.subunit, i);
		cur_chan->chanpos = i + 1;
		cur_chan->pvt = xpd;
		if (i == 2) {	/* D-CHAN */
			cur_chan->sigcap = BRI_DCHAN_SIGCAP;
			clear_bit(DAHDI_FLAGBIT_HDLC, &cur_chan->flags);
			priv->txframe_begin = 1;
			atomic_set(&priv->hdlc_pending, 0);
		} else {
			cur_chan->sigcap = BRI_BCHAN_SIGCAP;
		}
	}
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	PHONEDEV(xpd).span.ops = &BRI_span_ops;
	return 0;
}

static int BRI_card_dahdi_postregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	return (0);
}

static int BRI_card_hooksig(xpd_t *xpd, int pos, enum dahdi_txsig txsig)
{
	LINE_DBG(SIGNAL, xpd, pos, "%s\n", txsig2str(txsig));
	return 0;
}

/*
 * LED managment is done by the driver now:
 *   - Turn constant ON RED/GREEN led to indicate NT/TE port
 *   - Very fast "Double Blink" to indicate Layer1 alive (without D-Channel)
 *   - Constant blink (1/2 sec cycle) to indicate D-Channel alive.
 */
static void handle_leds(xbus_t *xbus, xpd_t *xpd)
{
	struct BRI_priv_data *priv;
	unsigned int timer_count;
	int which_led;
	int other_led;
	int mod;

	BUG_ON(!xpd);
	if (IS_NT(xpd)) {
		which_led = RED_LED;
		other_led = GREEN_LED;
	} else {
		which_led = GREEN_LED;
		other_led = RED_LED;
	}
	priv = xpd->priv;
	BUG_ON(!priv);
	timer_count = xpd->timer_count;
	if (xpd->blink_mode) {
		if ((timer_count % DEFAULT_LED_PERIOD) == 0) {
			// led state is toggled
			if (priv->ledstate[which_led] == BRI_LED_OFF) {
				DO_LED(xpd, which_led, BRI_LED_ON);
				DO_LED(xpd, other_led, BRI_LED_ON);
			} else {
				DO_LED(xpd, which_led, BRI_LED_OFF);
				DO_LED(xpd, other_led, BRI_LED_OFF);
			}
		}
		return;
	}
	if (priv->ledstate[other_led] != BRI_LED_OFF)
		DO_LED(xpd, other_led, BRI_LED_OFF);
	if (priv->dchan_alive) {
		mod = timer_count % 1000;
		switch (mod) {
		case 0:
			DO_LED(xpd, which_led, BRI_LED_ON);
			break;
		case 500:
			DO_LED(xpd, which_led, BRI_LED_OFF);
			break;
		}
	} else if (priv->layer1_up) {
		mod = timer_count % 1000;
		switch (mod) {
		case 0:
		case 100:
			DO_LED(xpd, which_led, BRI_LED_ON);
			break;
		case 50:
		case 150:
			DO_LED(xpd, which_led, BRI_LED_OFF);
			break;
		}
	} else {
		if (priv->ledstate[which_led] != BRI_LED_ON)
			DO_LED(xpd, which_led, BRI_LED_ON);
	}
}

static void handle_bri_timers(xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (IS_NT(xpd)) {
		if (priv->t1 > HFC_TIMER_OFF) {
			if (--priv->t1 == 0) {
				set_bri_timer(xpd, "T1", &priv->t1,
					      HFC_TIMER_OFF);
				if (!nt_keepalive) {
					/* G2 */
					if (priv->state_register.bits.v_su_sta == ST_NT_ACTIVATING) {
						XPD_DBG(SIGNAL, xpd,
							"T1 Expired. "
							"Deactivate NT\n");
						clear_bit(HFC_L1_ACTIVATING,
							  &priv->l1_flags);
						/* Deactivate NT */
						nt_activation(xpd, 0);
					} else
						XPD_DBG(SIGNAL, xpd,
							"T1 Expired. "
							"(state %d, ignored)\n",
							priv->state_register.
							bits.v_su_sta);
				}
			}
		}
	} else {
		if (priv->t3 > HFC_TIMER_OFF) {
			/* timer expired ? */
			if (--priv->t3 == 0) {
				XPD_DBG(SIGNAL, xpd,
					"T3 expired. Deactivate TE\n");
				set_bri_timer(xpd, "T3", &priv->t3,
					      HFC_TIMER_OFF);
				clear_bit(HFC_L1_ACTIVATING, &priv->l1_flags);
				te_activation(xpd, 0);	/* Deactivate TE */
			}
		}
	}
}

/* Poll the register ST/Up-State-machine Register, to see if the cable
 * if a cable is connected to the port.
 */
static int BRI_card_tick(xbus_t *xbus, xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!priv->initialized || !xbus->self_ticking)
		return 0;
	if (poll_interval != 0 && (priv->tick_counter % poll_interval) == 0) {
		// XPD_DBG(GENERAL, xpd, "%d\n", priv->tick_counter);
		priv->poll_counter++;
		xpp_register_request(xbus, xpd,
			BRI_PORT(xpd),	/* portno       */
			0,		/* writing      */
			A_SU_RD_STA,	/* regnum       */
			0,		/* do_subreg    */
			0,		/* subreg       */
			0,		/* data_low     */
			0,		/* do_datah     */
			0,		/* data_high    */
			0,		/* should_reply */
			0		/* do_expander  */
		    );

		if (IS_NT(xpd) && nt_keepalive
		    && !test_bit(HFC_L1_ACTIVATED, &priv->l1_flags)
		    && !test_bit(HFC_L1_ACTIVATING, &priv->l1_flags)) {
			XPD_DBG(SIGNAL, xpd, "Kick NT D-Channel\n");
			nt_activation(xpd, 1);
		}
	}
	/* Detect D-Channel disconnect heuristic */
	priv->dchan_notx_ticks++;
	priv->dchan_norx_ticks++;
	priv->dchan_alive_ticks++;
	if (priv->dchan_alive
	    && (priv->dchan_notx_ticks > DCHAN_LOST
		|| priv->dchan_norx_ticks > DCHAN_LOST)) {
		/*
		 * No tx_dchan() or rx_dchan() for many ticks
		 * This D-Channel is probabelly dead.
		 */
		dchan_state(xpd, 0);
	} else if (priv->dchan_rx_counter > 1 && priv->dchan_tx_counter > 1) {
		if (!priv->dchan_alive)
			dchan_state(xpd, 1);
	}
	/* Detect Layer1 disconnect */
	if (priv->reg30_good && priv->reg30_ticks > poll_interval * REG30_LOST) {
		/* No reply for 1/2 a second */
		XPD_ERR(xpd, "Lost state tracking for %d ticks\n",
			priv->reg30_ticks);
		priv->reg30_good = 0;
		layer1_state(xpd, 0);
	}
	handle_leds(xbus, xpd);
	handle_bri_timers(xpd);
	tx_dchan(xpd);
	priv->tick_counter++;
	priv->reg30_ticks++;
	return 0;
}

static int BRI_card_ioctl(xpd_t *xpd, int pos, unsigned int cmd,
			  unsigned long arg)
{
	BUG_ON(!xpd);
	if (!XBUS_IS(xpd->xbus, READY))
		return -ENODEV;
	switch (cmd) {
	case DAHDI_TONEDETECT:
		/*
		 * Asterisk call all span types with this (FXS specific)
		 * call. Silently ignore it.
		 */
		LINE_DBG(SIGNAL, xpd, pos, "BRI: Starting a call\n");
		return -ENOTTY;
	default:
		report_bad_ioctl(THIS_MODULE->name, xpd, pos, cmd);
		return -ENOTTY;
	}
	return 0;
}

static int BRI_card_open(xpd_t *xpd, lineno_t pos)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (pos == 2) {
		priv->dchan_is_open = 1;
		LINE_DBG(SIGNAL, xpd, pos, "OFFHOOK the whole span\n");
		BIT_SET(PHONEDEV(xpd).offhook_state, 0);
		BIT_SET(PHONEDEV(xpd).offhook_state, 1);
		BIT_SET(PHONEDEV(xpd).offhook_state, 2);
		CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	}
	return 0;
}

static int BRI_card_close(xpd_t *xpd, lineno_t pos)
{
	struct BRI_priv_data *priv;

	priv = xpd->priv;
	/* Clear D-Channel pending data */
	if (pos == 2) {
		LINE_DBG(SIGNAL, xpd, pos, "ONHOOK the whole span\n");
		BIT_CLR(PHONEDEV(xpd).offhook_state, 0);
		BIT_CLR(PHONEDEV(xpd).offhook_state, 1);
		BIT_CLR(PHONEDEV(xpd).offhook_state, 2);
		CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
		priv->dchan_is_open = 0;
	} else if (!priv->dchan_is_open)
		mark_offhook(xpd, pos, 0);	/* e.g: patgen/pattest */
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */
static int bri_spanconfig(struct file *file, struct dahdi_span *span,
			  struct dahdi_lineconfig *lc)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	const char *framingstr = "";
	const char *codingstr = "";
	const char *crcstr = "";

	/* framing first */
	if (lc->lineconfig & DAHDI_CONFIG_B8ZS)
		framingstr = "B8ZS";
	else if (lc->lineconfig & DAHDI_CONFIG_AMI)
		framingstr = "AMI";
	else if (lc->lineconfig & DAHDI_CONFIG_HDB3)
		framingstr = "HDB3";
	/* then coding */
	if (lc->lineconfig & DAHDI_CONFIG_ESF)
		codingstr = "ESF";
	else if (lc->lineconfig & DAHDI_CONFIG_D4)
		codingstr = "D4";
	else if (lc->lineconfig & DAHDI_CONFIG_CCS)
		codingstr = "CCS";
	/* E1's can enable CRC checking */
	if (lc->lineconfig & DAHDI_CONFIG_CRC4)
		crcstr = "CRC4";
	XPD_DBG(GENERAL, xpd,
		"[%s]: span=%d (%s) lbo=%d lineconfig=%s/%s/%s (0x%X) sync=%d\n",
		IS_NT(xpd) ? "NT" : "TE", lc->span, lc->name, lc->lbo,
		framingstr, codingstr, crcstr, lc->lineconfig, lc->sync);
	PHONEDEV(xpd).timing_priority = lc->sync;
	elect_syncer("BRI-spanconfig");
	/*
	 * FIXME: validate
	 */
	span->lineconfig = lc->lineconfig;
	return 0;
}

/*
 * Set signalling type (if appropriate)
 * Called from dahdi with spinlock held on chan. Must not call back
 * dahdi functions.
 */
static int bri_chanconfig(struct file *file, struct dahdi_chan *chan,
			  int sigtype)
{
	DBG(GENERAL, "channel %d (%s) -> %s\n", chan->channo, chan->name,
	    sig2str(sigtype));
	// FIXME: sanity checks:
	// - should be supported (within the sigcap)
	// - should not replace fxs <->fxo ??? (covered by previous?)
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */
static int bri_startup(struct file *file, struct dahdi_span *span)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct BRI_priv_data *priv;
	struct dahdi_chan *dchan;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!XBUS_IS(xpd->xbus, READY)) {
		XPD_DBG(GENERAL, xpd,
			"Startup called by dahdi. No Hardware. Ignored\n");
		return -ENODEV;
	}
	XPD_DBG(GENERAL, xpd, "STARTUP\n");
	// Turn on all channels
	CALL_PHONE_METHOD(card_state, xpd, 1);
	if (SPAN_REGISTERED(xpd)) {
		dchan = XPD_CHAN(xpd, 2);
		span->flags |= DAHDI_FLAG_RUNNING;
		/*
		 * Dahdi (wrongly) assume that D-Channel need HDLC decoding
		 * and during dahdi registration override our flags.
		 *
		 * Don't Get Mad, Get Even:  Now we override dahdi :-)
		 */
		clear_bit(DAHDI_FLAGBIT_HDLC, &dchan->flags);
	}
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */
static int bri_shutdown(struct dahdi_span *span)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!XBUS_IS(xpd->xbus, READY)) {
		XPD_DBG(GENERAL, xpd,
			"Shutdown called by dahdi. No Hardware. Ignored\n");
		return -ENODEV;
	}
	XPD_DBG(GENERAL, xpd, "SHUTDOWN\n");
	// Turn off all channels
	CALL_PHONE_METHOD(card_state, xpd, 0);
	return 0;
}

static void BRI_card_pcm_recompute(xpd_t *xpd, xpp_line_t dont_care)
{
	int i;
	int line_count;
	xpp_line_t pcm_mask;
	uint pcm_len;
	xpd_t *main_xpd;
	unsigned long flags;

	BUG_ON(!xpd);
	main_xpd = xpd_byaddr(xpd->xbus, xpd->addr.unit, 0);
	if (!main_xpd) {
		XPD_DBG(DEVICES, xpd,
			"Unit 0 is already gone. Ignore request\n");
		return;
	}
	/*
	 * We calculate all subunits, so use the main lock
	 * as a mutex for the whole operation.
	 */
	spin_lock_irqsave(&PHONEDEV(main_xpd).lock_recompute_pcm, flags);
	line_count = 0;
	pcm_mask = 0;
	for (i = 0; i < MAX_SUBUNIT; i++) {
		xpd_t *sub_xpd = xpd_byaddr(xpd->xbus, main_xpd->addr.unit, i);

		if (sub_xpd) {
			xpp_line_t lines =
			    PHONEDEV(sub_xpd).
			    offhook_state & ~(PHONEDEV(sub_xpd).
					      digital_signalling);

			if (lines) {
				pcm_mask |= PCM_SHIFT(lines, i);
				line_count += 2;
			}
			/* subunits have fake pcm_len and wanted_pcm_mask */
			if (i > 0)
				update_wanted_pcm_mask(sub_xpd, lines, 0);
		}
	}
	/*
	 * FIXME: Workaround a bug in sync code of the Astribank.
	 *        Send dummy PCM for sync.
	 */
	if (main_xpd->addr.unit == 0 && line_count == 0) {
		pcm_mask = BIT(0);
		line_count = 1;
	}
	/*
	 * The main unit account for all subunits (pcm_len and wanted_pcm_mask).
	 */
	pcm_len = (line_count)
	    ? RPACKET_HEADERSIZE + sizeof(xpp_line_t) +
	    line_count * DAHDI_CHUNKSIZE : 0L;
	update_wanted_pcm_mask(main_xpd, pcm_mask, pcm_len);
	spin_unlock_irqrestore(&PHONEDEV(main_xpd).lock_recompute_pcm, flags);
}

static void BRI_card_pcm_fromspan(xpd_t *xpd, xpacket_t *pack)
{
	__u8 *pcm;
	unsigned long flags;
	int i;
	int subunit;
	xpp_line_t pcm_mask = 0;
	xpp_line_t wanted_lines;

	BUG_ON(!xpd);
	BUG_ON(!pack);
	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, pcm);
	for (subunit = 0; subunit < MAX_SUBUNIT; subunit++) {
		xpd_t *tmp_xpd;

		tmp_xpd = xpd_byaddr(xpd->xbus, xpd->addr.unit, subunit);
		if (!tmp_xpd || !tmp_xpd->card_present)
			continue;
		spin_lock_irqsave(&tmp_xpd->lock, flags);
		wanted_lines = PHONEDEV(tmp_xpd).wanted_pcm_mask;
		for_each_line(tmp_xpd, i) {
			struct dahdi_chan *chan = XPD_CHAN(tmp_xpd, i);

			if (IS_SET(wanted_lines, i)) {
				if (SPAN_REGISTERED(tmp_xpd)) {
#ifdef	DEBUG_PCMTX
					int channo = chan->channo;

					if (pcmtx >= 0 && pcmtx_chan == channo)
						memset((u_char *)pcm, pcmtx,
						       DAHDI_CHUNKSIZE);
					else
#endif
						memcpy((u_char *)pcm,
						       chan->writechunk,
						       DAHDI_CHUNKSIZE);
				} else
					memset((u_char *)pcm, 0x7F,
					       DAHDI_CHUNKSIZE);
				pcm += DAHDI_CHUNKSIZE;
			}
		}
		pcm_mask |= PCM_SHIFT(wanted_lines, subunit);
		XPD_COUNTER(tmp_xpd, PCM_WRITE)++;
		spin_unlock_irqrestore(&tmp_xpd->lock, flags);
	}
	RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, lines) = pcm_mask;
}

static void BRI_card_pcm_tospan(xpd_t *xpd, xpacket_t *pack)
{
	__u8 *pcm;
	xpp_line_t pcm_mask;
	unsigned long flags;
	int subunit;
	int i;

	/*
	 * Subunit 0 handle all other subunits
	 */
	if (xpd->addr.subunit != 0)
		return;
	if (!SPAN_REGISTERED(xpd))
		return;
	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_READ, pcm);
	pcm_mask = RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, lines);
	for (subunit = 0; subunit < MAX_SUBUNIT;
	     subunit++, pcm_mask >>= SUBUNIT_PCM_SHIFT) {
		xpd_t *tmp_xpd;

		if (!pcm_mask)
			break;	/* optimize */
		tmp_xpd = xpd_byaddr(xpd->xbus, xpd->addr.unit, subunit);
		if (!tmp_xpd || !tmp_xpd->card_present
		    || !SPAN_REGISTERED(tmp_xpd))
			continue;
		spin_lock_irqsave(&tmp_xpd->lock, flags);
		for (i = 0; i < 2; i++) {
			xpp_line_t tmp_mask = pcm_mask & (BIT(0) | BIT(1));
			volatile u_char *r;

			if (IS_SET(tmp_mask, i)) {
				r = XPD_CHAN(tmp_xpd, i)->readchunk;
#if 0
				/* DEBUG */
				memset((u_char *)r, 0x5A, DAHDI_CHUNKSIZE);
#endif
				memcpy((u_char *)r, pcm, DAHDI_CHUNKSIZE);
				pcm += DAHDI_CHUNKSIZE;
			}
		}
		XPD_COUNTER(tmp_xpd, PCM_READ)++;
		spin_unlock_irqrestore(&tmp_xpd->lock, flags);
	}
}

static int BRI_timing_priority(xpd_t *xpd)
{
	struct BRI_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->layer1_up)
		return PHONEDEV(xpd).timing_priority;
	XPD_DBG(SYNC, xpd, "No timing priority (no layer1)\n");
	return -ENOENT;
}

static int BRI_echocancel_timeslot(xpd_t *xpd, int pos)
{
	return xpd->addr.subunit * 4 + pos;
}

static int BRI_echocancel_setmask(xpd_t *xpd, xpp_line_t ec_mask)
{
	struct BRI_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(GENERAL, xpd, "0x%8X\n", ec_mask);
	if (!ECHOOPS(xpd->xbus)) {
		XPD_DBG(GENERAL, xpd,
			"No echo canceller in XBUS: Doing nothing.\n");
		return -EINVAL;
	}
	for (i = 0; i < PHONEDEV(xpd).channels - 1; i++) {
		int on = BIT(i) & ec_mask;

		CALL_EC_METHOD(ec_set, xpd->xbus, xpd, i, on);
	}
	CALL_EC_METHOD(ec_update, xpd->xbus, xpd->xbus);
	return 0;
}

/*---------------- BRI: HOST COMMANDS -------------------------------------*/

static /* 0x33 */ HOSTCMD(BRI, SET_LED, enum bri_led_names which_led,
			  enum led_state to_led_state)
{
	int ret = 0;
	xframe_t *xframe;
	xpacket_t *pack;
	struct bri_leds *bri_leds;
	struct BRI_priv_data *priv;

	BUG_ON(!xbus);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(LEDS, xpd, "%s -> %d\n", (which_led) ? "RED" : "GREEN",
		to_led_state);
	XFRAME_NEW_CMD(xframe, pack, xbus, BRI, SET_LED, xpd->xbus_idx);
	bri_leds = &RPACKET_FIELD(pack, BRI, SET_LED, bri_leds);
	bri_leds->state = to_led_state;
	bri_leds->led_sel = which_led;
	XPACKET_LEN(pack) = RPACKET_SIZE(BRI, SET_LED);
	ret = send_cmd_frame(xbus, xframe);
	priv->ledstate[which_led] = to_led_state;
	return ret;
}

static int write_state_register(xpd_t *xpd, __u8 value)
{
	int ret;

	XPD_DBG(REGS, xpd, "value = 0x%02X\n", value);
	ret = xpp_register_request(xpd->xbus, xpd,
		BRI_PORT(xpd),	/* portno       */
		1,		/* writing      */
		A_SU_WR_STA,	/* regnum       */
		0,		/* do_subreg    */
		0,		/* subreg       */
		value,		/* data_low     */
		0,		/* do_datah     */
		0,		/* data_high    */
		0,		/* should_reply */
		0		/* do_expander */
	    );
	return ret;
}

/*---------------- BRI: Astribank Reply Handlers --------------------------*/
static void su_new_state(xpd_t *xpd, __u8 reg_x30)
{
	struct BRI_priv_data *priv;
	su_rd_sta_t new_state;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!priv->initialized) {
		XPD_ERR(xpd, "%s called on uninitialized AB\n", __func__);
		return;
	}
	new_state.reg = reg_x30;
	if (new_state.bits.v_su_t2_exp)
		XPD_NOTICE(xpd, "T2 Expired\n");
	priv->reg30_ticks = 0;
	priv->reg30_good = 1;
	if (priv->state_register.bits.v_su_sta == new_state.bits.v_su_sta)
		return;		/* same same */
	XPD_DBG(SIGNAL, xpd, "%02X ---> %02X (info0=%d) (%s%i)\n",
		priv->state_register.reg, reg_x30, new_state.bits.v_su_info0,
		IS_NT(xpd) ? "G" : "F", new_state.bits.v_su_sta);
	if (!IS_NT(xpd)) {
		switch (new_state.bits.v_su_sta) {
		case ST_TE_DEACTIVATED:	/* F3 */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_DEACTIVATED (F3)\n");
			clear_bit(HFC_L1_ACTIVATED, &priv->l1_flags);
			layer1_state(xpd, 0);
			break;
		case ST_TE_SIGWAIT:	/* F4   */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_SIGWAIT (F4)\n");
			layer1_state(xpd, 0);
			break;
		case ST_TE_IDENT:	/* F5   */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_IDENT (F5)\n");
			layer1_state(xpd, 0);
			break;
		case ST_TE_SYNCED:	/* F6   */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_SYNCED (F6)\n");
			layer1_state(xpd, 0);
			break;
		case ST_TE_ACTIVATED:	/* F7 */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_ACTIVATED (F7)\n");
			set_bri_timer(xpd, "T3", &priv->t3, HFC_TIMER_OFF);
			clear_bit(HFC_L1_ACTIVATING, &priv->l1_flags);
			set_bit(HFC_L1_ACTIVATED, &priv->l1_flags);
			layer1_state(xpd, 1);
			update_xpd_status(xpd, DAHDI_ALARM_NONE);
			break;
		case ST_TE_LOST_FRAMING:	/* F8 */
			XPD_DBG(SIGNAL, xpd, "State ST_TE_LOST_FRAMING (F8)\n");
			layer1_state(xpd, 0);
			break;
		default:
			XPD_NOTICE(xpd, "Bad TE state: %d\n",
				   new_state.bits.v_su_sta);
			break;
		}

	} else {
		switch (new_state.bits.v_su_sta) {
		case ST_NT_DEACTIVATED:	/* G1 */
			XPD_DBG(SIGNAL, xpd, "State ST_NT_DEACTIVATED (G1)\n");
			clear_bit(HFC_L1_ACTIVATED, &priv->l1_flags);
			set_bri_timer(xpd, "T1", &priv->t1, HFC_TIMER_OFF);
			layer1_state(xpd, 0);
			break;
		case ST_NT_ACTIVATING:	/* G2 */
			XPD_DBG(SIGNAL, xpd, "State ST_NT_ACTIVATING (G2)\n");
			layer1_state(xpd, 0);
			if (!test_bit(HFC_L1_ACTIVATED, &priv->l1_flags))
				nt_activation(xpd, 1);
			break;
		case ST_NT_ACTIVATED:	/* G3 */
			XPD_DBG(SIGNAL, xpd, "State ST_NT_ACTIVATED (G3)\n");
			clear_bit(HFC_L1_ACTIVATING, &priv->l1_flags);
			set_bit(HFC_L1_ACTIVATED, &priv->l1_flags);
			set_bri_timer(xpd, "T1", &priv->t1, HFC_TIMER_OFF);
			layer1_state(xpd, 1);
			update_xpd_status(xpd, DAHDI_ALARM_NONE);
			break;
		case ST_NT_DEACTIVTING:	/* G4 */
			XPD_DBG(SIGNAL, xpd, "State ST_NT_DEACTIVTING (G4)\n");
			set_bri_timer(xpd, "T1", &priv->t1, HFC_TIMER_OFF);
			layer1_state(xpd, 0);
			break;
		default:
			XPD_NOTICE(xpd, "Bad NT state: %d\n",
				   new_state.bits.v_su_sta);
			break;
		}
	}
	priv->state_register.reg = new_state.reg;
}

static int BRI_card_register_reply(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *info)
{
	unsigned long flags;
	struct BRI_priv_data *priv;
	struct xpd_addr addr;
	xpd_t *orig_xpd;
	int ret;

	/* Map UNIT + PORTNUM to XPD */
	orig_xpd = xpd;
	addr.unit = orig_xpd->addr.unit;
	addr.subunit = info->h.portnum;
	xpd = xpd_byaddr(xbus, addr.unit, addr.subunit);
	if (!xpd) {
		static int rate_limit;

		if ((rate_limit++ % 1003) < 5)
			notify_bad_xpd(__func__, xbus, addr, orig_xpd->xpdname);
		return -EPROTO;
	}
	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (REG_FIELD(info, do_subreg)) {
		XPD_DBG(REGS, xpd, "RI %02X %02X %02X\n",
			REG_FIELD(info, regnum), REG_FIELD(info, subreg),
			REG_FIELD(info, data_low));
	} else {
		if (REG_FIELD(info, regnum) != A_SU_RD_STA)
			XPD_DBG(REGS, xpd, "RD %02X %02X\n",
				REG_FIELD(info, regnum), REG_FIELD(info,
								   data_low));
		else
			XPD_DBG(REGS, xpd, "Got SU_RD_STA=%02X\n",
				REG_FIELD(info, data_low));
	}
	if (info->h.is_multibyte) {
		XPD_DBG(REGS, xpd, "Got Multibyte: %d bytes, eoframe: %d\n",
			info->h.bytes, info->h.eoframe);
		ret = rx_dchan(xpd, info);
		if (ret < 0) {
			priv->dchan_rx_drops++;
			if (atomic_read(&PHONEDEV(xpd).open_counter) > 0)
				XPD_NOTICE(xpd, "Multibyte Drop: errno=%d\n",
					   ret);
		}
		goto end;
	}
	if (REG_FIELD(info, regnum) == A_SU_RD_STA)
		su_new_state(xpd, REG_FIELD(info, data_low));

	/* Update /proc info only if reply relate to the last slic read request */
	if (REG_FIELD(&xpd->requested_reply, regnum) ==
			REG_FIELD(info, regnum)
		&& REG_FIELD(&xpd->requested_reply, do_subreg) ==
			REG_FIELD(info, do_subreg)
		&& REG_FIELD(&xpd->requested_reply, subreg) ==
			REG_FIELD(info, subreg)) {
		xpd->last_reply = *info;
	}

end:
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static int BRI_card_state(xpd_t *xpd, bool on)
{
	struct BRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "ON" : "OFF");
	if (on) {
		if (!test_bit(HFC_L1_ACTIVATED, &priv->l1_flags)) {
			if (!IS_NT(xpd))
				te_activation(xpd, 1);
			else
				nt_activation(xpd, 1);
		}
	} else if (IS_NT(xpd))
		nt_activation(xpd, 0);
	return 0;
}

static const struct xops bri_xops = {
	.card_new = BRI_card_new,
	.card_init = BRI_card_init,
	.card_remove = BRI_card_remove,
	.card_tick = BRI_card_tick,
	.card_register_reply = BRI_card_register_reply,
};

static const struct phoneops bri_phoneops = {
	.card_dahdi_preregistration = BRI_card_dahdi_preregistration,
	.card_dahdi_postregistration = BRI_card_dahdi_postregistration,
	.card_hooksig = BRI_card_hooksig,
	.card_pcm_recompute = BRI_card_pcm_recompute,
	.card_pcm_fromspan = BRI_card_pcm_fromspan,
	.card_pcm_tospan = BRI_card_pcm_tospan,
	.card_timing_priority = BRI_timing_priority,
	.echocancel_timeslot = BRI_echocancel_timeslot,
	.echocancel_setmask = BRI_echocancel_setmask,
	.card_ioctl = BRI_card_ioctl,
	.card_open = BRI_card_open,
	.card_close = BRI_card_close,
	.card_state = BRI_card_state,
};

static xproto_table_t PROTO_TABLE(BRI) = {
	.owner = THIS_MODULE,
	.entries = {
		/*      Table   Card    Opcode          */
	},
	.name = "BRI",	/* protocol name */
	.ports_per_subunit = 1,
	.type = XPD_TYPE_BRI,
	.xops = &bri_xops,
	.phoneops = &bri_phoneops,
	.packet_is_valid = bri_packet_is_valid,
	.packet_dump = bri_packet_dump,
};

static bool bri_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe = NULL;
	// DBG(GENERAL, "\n");
	xe = xproto_card_entry(&PROTO_TABLE(BRI), XPACKET_OP(pack));
	return xe != NULL;
}

static void bri_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

/*------------------------- REGISTER Handling --------------------------*/

#ifdef	CONFIG_PROC_FS
static int proc_bri_info_show(struct seq_file *sfile, void *not_used)
{
	unsigned long flags;
	xpd_t *xpd = sfile->private;
	struct BRI_priv_data *priv;

	DBG(PROC, "\n");
	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	priv = xpd->priv;
	BUG_ON(!priv);
	seq_printf(sfile, "%05d Layer 1: ", priv->poll_counter);
	if (priv->reg30_good) {
		seq_printf(sfile, "%-5s ", (priv->layer1_up) ? "UP" : "DOWN");
		seq_printf(sfile,
			   "%c%d %-15s -- fr_sync=%d t2_exp=%d info0=%d g2_g3=%d\n",
			   IS_NT(xpd) ? 'G' : 'F',
			   priv->state_register.bits.v_su_sta,
			   xhfc_state_name(IS_NT(xpd),
				    priv->state_register.bits.v_su_sta),
			   priv->state_register.bits.v_su_fr_sync,
			   priv->state_register.bits.v_su_t2_exp,
			   priv->state_register.bits.v_su_info0,
			   priv->state_register.bits.v_g2_g3);
	} else {
		seq_printf(sfile, "Unknown\n");
	}
	if (IS_NT(xpd))
		seq_printf(sfile, "T1 Timer: %d\n", priv->t1);
	else
		seq_printf(sfile, "T3 Timer: %d\n", priv->t3);
	seq_printf(sfile, "Tick Counter: %d\n", priv->tick_counter);
	seq_printf(sfile, "Last Poll Reply: %d ticks ago\n",
		    priv->reg30_ticks);
	seq_printf(sfile, "reg30_good=%d\n", priv->reg30_good);
	seq_printf(sfile, "D-Channel: TX=[%5d]    RX=[%5d]    BAD=[%5d] ",
		    priv->dchan_tx_counter, priv->dchan_rx_counter,
		    priv->dchan_rx_drops);
	if (priv->dchan_alive) {
		seq_printf(sfile, "(alive %d K-ticks)\n",
			    priv->dchan_alive_ticks / 1000);
	} else {
		seq_printf(sfile, "(dead)\n");
	}
	seq_printf(sfile, "dchan_notx_ticks: %d\n",
		    priv->dchan_notx_ticks);
	seq_printf(sfile, "dchan_norx_ticks: %d\n",
		    priv->dchan_norx_ticks);
	seq_printf(sfile, "LED: %-10s = %d\n", "GREEN",
		    priv->ledstate[GREEN_LED]);
	seq_printf(sfile, "LED: %-10s = %d\n", "RED",
		    priv->ledstate[RED_LED]);
	seq_printf(sfile, "\nDCHAN:\n");
	seq_printf(sfile, "\n");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return 0;
}

static int proc_bri_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_bri_info_show, PDE_DATA(inode));
}

#ifdef DAHDI_HAVE_PROC_OPS
static const struct proc_ops proc_bri_info_ops = {
	.proc_open		= proc_bri_info_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};
#else
static const struct file_operations proc_bri_info_ops = {
	.owner			= THIS_MODULE,
	.open			= proc_bri_info_open,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};
#endif /* DAHDI_HAVE_PROC_OPS */
#endif

static int bri_xpd_probe(struct device *dev)
{
	xpd_t *xpd;

	xpd = dev_to_xpd(dev);
	/* Is it our device? */
	if (xpd->xpd_type != XPD_TYPE_BRI) {
		XPD_ERR(xpd, "drop suggestion for %s (%d)\n", dev_name(dev),
			xpd->xpd_type);
		return -EINVAL;
	}
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	return 0;
}

static int bri_xpd_remove(struct device *dev)
{
	xpd_t *xpd;

	xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	return 0;
}

static struct xpd_driver bri_driver = {
	.xpd_type = XPD_TYPE_BRI,
	.driver = {
		   .name = "bri",
		   .owner = THIS_MODULE,
		   .probe = bri_xpd_probe,
		   .remove = bri_xpd_remove}
};

static int __init card_bri_startup(void)
{
	int ret;

	if ((ret = xpd_driver_register(&bri_driver.driver)) < 0)
		return ret;
	xproto_register(&PROTO_TABLE(BRI));
	return 0;
}

static void __exit card_bri_cleanup(void)
{
	DBG(GENERAL, "\n");
	xproto_unregister(&PROTO_TABLE(BRI));
	xpd_driver_unregister(&bri_driver.driver);
}

MODULE_DESCRIPTION("XPP BRI Card Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_XPD(XPD_TYPE_BRI);

module_init(card_bri_startup);
module_exit(card_bri_cleanup);
