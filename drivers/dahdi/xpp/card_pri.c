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
#include "xpd.h"
#include "xproto.h"
#include "xpp_dahdi.h"
#include "card_pri.h"
#include "dahdi_debug.h"
#include "xbus-core.h"

static const char rcsid[] = "$Id$";

/* must be before dahdi_debug.h */
static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
static DEF_PARM(uint, poll_interval, 500, 0644,
		"Poll channel state interval in milliseconds (0 - disable)");

#define	PRI_LINES_BITMASK	BITMASK(31)
#define	PRI_SIGCAP	(		\
			DAHDI_SIG_EM		| \
			DAHDI_SIG_CLEAR		| \
			DAHDI_SIG_FXSLS		| \
			DAHDI_SIG_FXSGS		| \
			DAHDI_SIG_FXSKS		| \
			DAHDI_SIG_HARDHDLC	| \
			DAHDI_SIG_MTP2		| \
			DAHDI_SIG_FXOLS		| \
			DAHDI_SIG_FXOGS		| \
			DAHDI_SIG_FXOKS		| \
			DAHDI_SIG_CAS		| \
			DAHDI_SIG_EM_E1		| \
			DAHDI_SIG_DACS_RBS	\
			)

static bool is_sigtype_dchan(int sigtype)
{
	if ((sigtype & DAHDI_SIG_HDLCRAW) == DAHDI_SIG_HDLCRAW)
		return 1;
	if ((sigtype & DAHDI_SIG_HDLCFCS) == DAHDI_SIG_HDLCFCS)
		return 1;
	if ((sigtype & DAHDI_SIG_HARDHDLC) == DAHDI_SIG_HARDHDLC)
		return 1;
	return 0;
}

#define	MAX_SLAVES		4	/* we have MUX of 4 clocks */

#define	PRI_PORT(xpd)	((xpd)->addr.subunit)
#define	CHAN_PER_REGS(p)	(((p)->is_esf) ? 2 : 4)

/*---------------- PRI Protocol Commands ----------------------------------*/

static void dchan_state(xpd_t *xpd, bool up);
static bool pri_packet_is_valid(xpacket_t *pack);
static void pri_packet_dump(const char *msg, xpacket_t *pack);
static int pri_startup(struct file *file, struct dahdi_span *span);
static int pri_shutdown(struct dahdi_span *span);
static int pri_rbsbits(struct dahdi_chan *chan, int bits);
static int pri_lineconfig(xpd_t *xpd, int lineconfig);
static void send_idlebits(xpd_t *xpd, bool saveold);
static int apply_pri_protocol(xpd_t *xpd);

#define	PROC_REGISTER_FNAME	"slics"

enum pri_protocol {
	PRI_PROTO_0 = 0,
	PRI_PROTO_E1 = 1,
	PRI_PROTO_T1 = 2,
	PRI_PROTO_J1 = 3
};

static const char *protocol_names[] = {
	[PRI_PROTO_0] = "??",	/* unknown */
	[PRI_PROTO_E1] = "E1",
	[PRI_PROTO_T1] = "T1",
	[PRI_PROTO_J1] = "J1"
};

static enum spantypes pri_protocol2spantype(enum pri_protocol pri_protocol)
{
	switch (pri_protocol) {
	case PRI_PROTO_E1: return SPANTYPE_DIGITAL_E1;
	case PRI_PROTO_T1: return SPANTYPE_DIGITAL_T1;
	case PRI_PROTO_J1: return SPANTYPE_DIGITAL_J1;
	default:
		return SPANTYPE_INVALID;
	}
}

static const char *pri_protocol_name(enum pri_protocol pri_protocol)
{
	return protocol_names[pri_protocol];
}

static int pri_num_channels(enum pri_protocol pri_protocol)
{
	static int num_channels[] = {
		[PRI_PROTO_0] = 0,
		[PRI_PROTO_E1] = 31,
		[PRI_PROTO_T1] = 24,
		[PRI_PROTO_J1] = 0
	};
	return num_channels[pri_protocol];
}

static const char *type_name(enum pri_protocol pri_protocol)
{
	static const char *names[4] = {
		[PRI_PROTO_0] = "PRI-Unknown",
		[PRI_PROTO_E1] = "E1",
		[PRI_PROTO_T1] = "T1",
		[PRI_PROTO_J1] = "J1"
	};

	return names[pri_protocol];
}

static int pri_linecompat(enum pri_protocol pri_protocol)
{
	static const int linecompat[] = {
		[PRI_PROTO_0] = 0,
		[PRI_PROTO_E1] =
		    /* coding */
		    DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4 |
		    /* framing */
		    DAHDI_CONFIG_AMI | DAHDI_CONFIG_HDB3,
		[PRI_PROTO_T1] =
		    /* coding */
		    DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF |
		    /* framing */
		    DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS,
		[PRI_PROTO_J1] = 0
	};

	DBG(GENERAL, "pri_linecompat: pri_protocol=%d\n", pri_protocol);
	return linecompat[pri_protocol];
}

#define	PRI_DCHAN_IDX(priv)	((priv)->dchan_num - 1)

enum pri_led_state {
	PRI_LED_OFF = 0x0,
	PRI_LED_ON = 0x1,
	/*
	 * We blink by software from driver, so that
	 * if the driver malfunction that blink would stop.
	 */
	// PRI_LED_BLINK_SLOW   = 0x2,  /* 1/2 a second blink cycle */
	// PRI_LED_BLINK_FAST   = 0x3   /* 1/4 a second blink cycle */
};

enum pri_led_selectors {
	BOTTOM_RED_LED = 0,
	BOTTOM_GREEN_LED = 1,
	TOP_RED_LED = 2,
	TOP_GREEN_LED = 3,
};

#define	NUM_LEDS	4

struct pri_leds {
	__u8 state:2;		/* enum pri_led_state */
	__u8 led_sel:2;		/* enum pri_led_selectors */
	__u8 reserved:4;
};

#define	REG_CCB1_T	0x2F	/* Clear Channel Register 1 */

#define	REG_FRS0	0x4C	/* Framer Receive Status Register 0 */
#define	REG_FRS0_T1_FSR	BIT(0)	/* T1 - Frame Search Restart Flag */
#define	REG_FRS0_LMFA	BIT(1)	/* Loss of Multiframe Alignment */
#define	REG_FRS0_E1_NMF	BIT(2)	/* E1 - No Multiframe Alignment Found */
#define	REG_FRS0_RRA	BIT(4)	/* Receive Remote Alarm: T1-YELLOW-Alarm */
#define	REG_FRS0_LFA	BIT(5)	/* Loss of Frame Alignment */
#define	REG_FRS0_AIS	BIT(6)	/* Alarm Indication Signal: T1-BLUE-Alarm */
#define	REG_FRS0_LOS	BIT(7)	/* Los Of Signal: T1-RED-Alarm */

#define	REG_FRS1	0x4D	/* Framer Receive Status Register 1 */

#define	REG_LIM0	0x36
/*
 * Master Mode, DCO-R circuitry is frequency synchronized
 * to the clock supplied by SYNC
 */
#define	REG_LIM0_MAS	BIT(0)
/*
 * Receive Termination Resistance Selection:
 * integrated resistor to create 75 Ohm termination (100 || 300 = 75)
 * 0 = 100 Ohm
 * 1 = 75 Ohm
 */
#define	REG_LIM0_RTRS	BIT(5)
#define	REG_LIM0_LL	BIT(1)	/* LL (Local Loopback) */

#define	REG_FMR0	0x1C
#define	REG_FMR0_E_RC0	BIT(4)	/* Receive Code - LSB */
#define	REG_FMR0_E_RC1	BIT(5)	/* Receive Code - MSB */
#define	REG_FMR0_E_XC0	BIT(6)	/* Transmit Code - LSB */
#define	REG_FMR0_E_XC1	BIT(7)	/* Transmit Code - MSB */

#define	REG_FMR1	0x1D
#define	REG_FMR1_XAIS	BIT(0)	/* Transmit AIS toward transmit end */
#define	REG_FMR1_SSD0	BIT(1)
#define	REG_FMR1_ECM	BIT(2)
#define	REG_FMR1_T_CRC	BIT(3)	/* Enable CRC6 */
#define	REG_FMR1_E_XFS	BIT(3)	/* Transmit Framing Select */
#define	REG_FMR1_PMOD	BIT(4)	/* E1 = 0, T1/J1 = 1 */
#define	REG_FMR1_EDL	BIT(5)
#define	REG_FMR1_AFR	BIT(6)

#define	REG_FMR2	0x1E
#define	REG_FMR2_E_ALMF	BIT(0)	/* Automatic Loss of Multiframe */
#define	REG_FMR2_T_EXZE	BIT(0)	/* Excessive Zeros Detection Enable */
#define	REG_FMR2_E_AXRA	BIT(1)	/* Automatic Transmit Remote Alarm */
#define	REG_FMR2_T_AXRA	BIT(1)	/* Automatic Transmit Remote Alarm */
#define	REG_FMR2_E_PLB	BIT(2)	/* Payload Loop-Back */
#define	REG_FMR2_E_RFS0	BIT(6)	/* Receive Framing Select - LSB */
#define	REG_FMR2_E_RFS1	BIT(7)	/* Receive Framing Select - MSB */
/* Select Synchronization/Resynchronization Procedure */
#define	REG_FMR2_T_SSP	BIT(5)
/* Multiple Candidates Synchronization Procedure */
#define	REG_FMR2_T_MCSP	BIT(6)
/* Automatic Force Resynchronization */
#define	REG_FMR2_T_AFRS	BIT(7)

#define	REG_FMR3	0x31
#define	REG_FMR3_EXTIW	BIT(0)	/* Extended CRC4 to Non-CRC4 Interworking */

#define	REG_FMR4	0x20
#define	REG_FMR4_FM0	BIT(0)
#define	REG_FMR4_FM1	BIT(1)
#define	REG_FMR4_AUTO	BIT(2)
#define	REG_FMR4_SSC0	BIT(3)
#define	REG_FMR4_SSC1	BIT(4)
#define	REG_FMR4_XRA	BIT(5)	/* Transmit Remote Alarm (Yellow Alarm) */
#define	REG_FMR4_TM	BIT(6)
#define	REG_FMR4_AIS3	BIT(7)

#define	REG_XSW_E	0x20
#define	REG_XSW_E_XY4	BIT(0)
#define	REG_XSW_E_XY3	BIT(1)
#define	REG_XSW_E_XY2	BIT(2)
#define	REG_XSW_E_XY1	BIT(3)
#define	REG_XSW_E_XY0	BIT(4)
#define	REG_XSW_E_XRA	BIT(5)	/* Transmit Remote Alarm (Yellow Alarm) */
#define	REG_XSW_E_XTM	BIT(6)
#define	REG_XSW_E_XSIS	BIT(7)

#define REG_XSP_E	0x21
#define REG_FMR5_T	0x21
/* Transmit Spare Bit For International Use (FAS Word)  */
#define	REG_XSP_E_XSIF	BIT(2)
#define	REG_FMR5_T_XTM	BIT(2)	/* Transmit Transparent Mode  */
/* Automatic Transmission of Submultiframe Status  */
#define	REG_XSP_E_AXS	BIT(3)
/* E-Bit Polarity, Si-bit position of every outgoing CRC multiframe  */
#define	REG_XSP_E_EBP	BIT(4)
#define	REG_XSP_E_CASEN	BIT(6)	/* CAS: Channel Associated Signaling Enable  */
#define	REG_FMR5_T_EIBR	BIT(6)	/* CAS: Enable Internal Bit Robbing Access   */

#define REG_XC0_T	0x22	/* Transmit Control 0 */
#define REG_XC0_BRIF	BIT(5)	/* Bit Robbing Idle Function */

#define REG_CMDR_E	0x02	/* Command Register */
#define REG_CMDR_RRES	BIT(6)	/* Receiver    reset */
#define REG_CMDR_XRES	BIT(4)	/* Transmitter reset */

#define	REG_RC0		0x24
#define	REG_RC0_SJR	BIT(7)	/* T1 = 0, J1 = 1 */

#define	REG_CMR1	0x44
#define	REG_CMR1_DRSS	(BIT(7) | BIT(6))
#define	REG_CMR1_RS	(BIT(5) | BIT(4))
#define	REG_CMR1_STF	BIT(2)

#define	REG_RS1_E	0x70	/* Receive CAS Register 1       */
#define	REG_RS2_E	0x71	/* Receive CAS Register 2       */
#define	REG_RS3_E	0x72	/* Receive CAS Register 3       */
#define	REG_RS4_E	0x73	/* Receive CAS Register 4       */
#define	REG_RS5_E	0x74	/* Receive CAS Register 5       */
#define	REG_RS6_E	0x75	/* Receive CAS Register 6       */
#define	REG_RS7_E	0x76	/* Receive CAS Register 7       */
#define	REG_RS8_E	0x77	/* Receive CAS Register 8       */
#define	REG_RS9_E	0x78	/* Receive CAS Register 9       */
#define	REG_RS10_E	0x79	/* Receive CAS Register 10      */
#define	REG_RS11_E	0x7A	/* Receive CAS Register 11      */
#define	REG_RS12_E	0x7B	/* Receive CAS Register 12      */
#define	REG_RS13_E	0x7C	/* Receive CAS Register 13      */
#define	REG_RS14_E	0x7D	/* Receive CAS Register 14      */
#define	REG_RS15_E	0x7E	/* Receive CAS Register 15      */
#define	REG_RS16_E	0x7F	/* Receive CAS Register 16      */

#define	REG_PC2		0x81	/* Port Configuration 2 */
#define	REG_PC3		0x82	/* Port Configuration 3 */
#define	REG_PC4		0x83	/* Port Configuration 4 */

#define	REG_XPM2	0x28	/* Transmit Pulse Mask 2 */

#define	VAL_PC_SYPR	0x00	/* Synchronous Pulse Receive (Input, low active) */
#define	VAL_PC_GPI	0x90	/* General purpose input */
#define	VAL_PC_GPOH	0x0A	/* General Purpose Output, high level */
#define	VAL_PC_GPOL	0x0B	/* General Purpose Output, low level */

#define	NUM_CAS_RS_E	(REG_RS16_E - REG_RS2_E + 1)
/* and of those, the ones used in T1: */
#define	NUM_CAS_RS_T	(REG_RS12_E - REG_RS1_E + 1)

struct PRI_priv_data {
	bool clock_source;
	enum pri_protocol pri_protocol;
	xpp_line_t rbslines;
	int deflaw;
	unsigned int dchan_num;
	bool initialized;
	bool dchan_is_open;
	int is_cas;

	unsigned int chanconfig_dchan;
#define	NO_DCHAN	(0)
#define	DCHAN(p)	((p)->chanconfig_dchan)
#define	VALID_DCHAN(p)	(DCHAN(p) != NO_DCHAN)
#define	SET_DCHAN(p, d)	do { DCHAN(p) = (d); } while (0);

	__u8 cas_rs_e[NUM_CAS_RS_E];
	__u8 cas_ts_e[NUM_CAS_RS_E];
	uint cas_replies;
	bool is_esf;
	bool local_loopback;
	uint poll_noreplies;
	uint layer1_replies;
	__u8 reg_frs0;
	__u8 reg_frs1;
	bool layer1_up;
	int alarms;
	__u8 dchan_tx_sample;
	__u8 dchan_rx_sample;
	uint dchan_tx_counter;
	uint dchan_rx_counter;
	bool dchan_alive;
	uint dchan_alive_ticks;
	enum pri_led_state ledstate[NUM_LEDS];
};

static xproto_table_t PROTO_TABLE(PRI);

DEF_RPACKET_DATA(PRI, SET_LED,	/* Set one of the LED's */
		 struct pri_leds pri_leds;);

static /* 0x33 */ DECLARE_CMD(PRI, SET_LED, enum pri_led_selectors led_sel,
			      enum pri_led_state to_led_state);

#define	DO_LED(xpd, which, tostate)	\
		CALL_PROTO(PRI, SET_LED, (xpd)->xbus, (xpd), (which), (tostate))

/*---------------- PRI: Methods -------------------------------------------*/

static int query_subunit(xpd_t *xpd, __u8 regnum)
{
	XPD_DBG(REGS, xpd, "(%d%d): REG=0x%02X\n", xpd->addr.unit,
		xpd->addr.subunit, regnum);
	return xpp_register_request(xpd->xbus, xpd, PRI_PORT(xpd),	/* portno       */
				    0,	/* writing      */
				    regnum, 0,	/* do_subreg    */
				    0,	/* subreg       */
				    0,	/* data_L       */
				    0,	/* do_datah     */
				    0,	/* data_H       */
				    0	/* should_reply */
	    );
}

static int write_subunit(xpd_t *xpd, __u8 regnum, __u8 val)
{
	XPD_DBG(REGS, xpd, "(%d%d): REG=0x%02X dataL=0x%02X\n", xpd->addr.unit,
		xpd->addr.subunit, regnum, val);
	return xpp_register_request(xpd->xbus, xpd,
			PRI_PORT(xpd),	/* portno       */
			1,		/* writing      */
			regnum, 0,	/* do_subreg    */
			0,		/* subreg       */
			val,		/* data_L       */
			0,		/* do_datah     */
			0,		/* data_H       */
			0		/* should_reply */
	    );
}

static int pri_write_reg(xpd_t *xpd, int regnum, __u8 val)
{
	XPD_DBG(REGS, xpd, "(%d%d): REG=0x%02X dataL=0x%02X\n", xpd->addr.unit,
		xpd->addr.subunit, regnum, val);
	return xpp_register_request(xpd->xbus, xpd, 0,	/* portno=0     */
				    1,	/* writing      */
				    regnum, 0,	/* do_subreg    */
				    0,	/* subreg       */
				    val,	/* data_L       */
				    0,	/* do_datah     */
				    0,	/* data_H       */
				    0	/* should_reply */
	    );
}

static int cas_regbase(xpd_t *xpd)
{
	struct PRI_priv_data *priv;

	priv = xpd->priv;
	switch (priv->pri_protocol) {
	case PRI_PROTO_E1:
		return REG_RS2_E;
	case PRI_PROTO_T1:
		/* fall-through */
	case PRI_PROTO_J1:
		return REG_RS1_E;
	case PRI_PROTO_0:
		/* fall-through */
		;
	}
	BUG();
	return 0;
}

static int cas_numregs(xpd_t *xpd)
{
	struct PRI_priv_data *priv;

	priv = xpd->priv;
	switch (priv->pri_protocol) {
	case PRI_PROTO_E1:
		return NUM_CAS_RS_E;
	case PRI_PROTO_T1:
		/* fall-through */
	case PRI_PROTO_J1:
		return NUM_CAS_RS_T;
	case PRI_PROTO_0:
		/* fall-through */
		;
	}
	BUG();
	return 0;
}

static int write_cas_reg(xpd_t *xpd, int rsnum, __u8 val)
{
	struct PRI_priv_data *priv;
	int regbase = cas_regbase(xpd);
	int num_cas_rs = cas_numregs(xpd);
	int regnum;
	bool is_d4 = 0;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if ((priv->pri_protocol == PRI_PROTO_T1) && !priv->is_esf) {
		/* same data should be copied to RS7..12 in D4 only */
		is_d4 = 1;
	}
	if (rsnum < 0 || rsnum >= num_cas_rs) {
		XPD_ERR(xpd, "RBS(TX): rsnum=%d\n", rsnum);
		BUG();
	}
	regnum = regbase + rsnum;
	priv->cas_ts_e[rsnum] = val;
	XPD_DBG(SIGNAL, xpd, "RBS(TX): reg=0x%X val=0x%02X\n", regnum, val);
	write_subunit(xpd, regbase + rsnum, val);
	if (is_d4) {
		/* same data should be copied to RS7..12 in D4 only */
		regnum = REG_RS7_E + rsnum;
		XPD_DBG(SIGNAL, xpd, "RBS(TX): reg=0x%X val=0x%02X\n", regnum,
			val);
		write_subunit(xpd, regnum, val);
	}
	return 0;
}

static bool valid_pri_modes(const xpd_t *xpd)
{
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (priv->pri_protocol != PRI_PROTO_E1
	    && priv->pri_protocol != PRI_PROTO_T1
	    && priv->pri_protocol != PRI_PROTO_J1)
		return 0;
	return 1;
}

static void PRI_card_pcm_recompute(xpd_t *xpd, xpp_line_t pcm_mask)
{
	struct PRI_priv_data *priv;
	int i;
	int line_count = 0;
	unsigned long flags;
	uint pcm_len;

	BUG_ON(!xpd);
	priv = xpd->priv;
	spin_lock_irqsave(&PHONEDEV(xpd).lock_recompute_pcm, flags);
	//XPD_DBG(SIGNAL, xpd, "pcm_mask=0x%X\n", pcm_mask);
	/* Add/remove all the trivial cases */
	pcm_mask |= PHONEDEV(xpd).offhook_state;
	if (priv->is_cas)
		pcm_mask |= BITMASK(PHONEDEV(xpd).channels);
	for_each_line(xpd, i)
	    if (IS_SET(pcm_mask, i))
		line_count++;
	    else
	if (priv->is_cas) {
		if (priv->pri_protocol == PRI_PROTO_E1) {
			/* CAS: Don't send PCM to D-Channel */
			line_count--;
			pcm_mask &= ~BIT(PRI_DCHAN_IDX(priv));
		}
	}
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

/*
 * Set E1/T1/J1
 * May only be called on unregistered xpd's
 * (the span and channel description are set according to this)
 */
static int set_pri_proto(xpd_t *xpd, enum pri_protocol set_proto)
{
	struct PRI_priv_data *priv;
	int deflaw;
	unsigned int dchan_num;
	int default_lineconfig = 0;
	int ret;
	struct phonedev *phonedev;

	BUG_ON(!xpd);
	priv = xpd->priv;
	phonedev = &PHONEDEV(xpd);
	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &phonedev->span.flags)) {
		XPD_NOTICE(xpd, "%s: %s already assigned as span %d\n",
			   __func__, phonedev->span.name,
			   phonedev->span.spanno);
		return -EBUSY;
	}
	if (priv->pri_protocol != PRI_PROTO_0) {
		if (priv->pri_protocol == set_proto) {
			XPD_NOTICE(xpd, "Already in protocol %s. Ignored\n",
				   pri_protocol_name(set_proto));
			return 0;
		} else {
			XPD_INFO(xpd, "Switching from %s to %s\n",
				 pri_protocol_name(priv->pri_protocol),
				 pri_protocol_name(set_proto));
		}
	}
	switch (set_proto) {
	case PRI_PROTO_E1:
		deflaw = DAHDI_LAW_ALAW;
		dchan_num = 16;
		default_lineconfig =
		    DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4 | DAHDI_CONFIG_HDB3;
		break;
	case PRI_PROTO_T1:
		deflaw = DAHDI_LAW_MULAW;
		dchan_num = 24;
		default_lineconfig = DAHDI_CONFIG_ESF | DAHDI_CONFIG_B8ZS;
		break;
	case PRI_PROTO_J1:
		/*
		 * Check all assumptions
		 */
		deflaw = DAHDI_LAW_MULAW;
		dchan_num = 24;
		default_lineconfig = 0;	/* FIXME: J1??? */
		XPD_NOTICE(xpd, "J1 is not supported yet\n");
		return -ENOSYS;
	default:
		XPD_ERR(xpd, "%s: Unknown pri protocol = %d\n", __func__,
			set_proto);
		return -EINVAL;
	}
	priv->pri_protocol = set_proto;
	priv->is_cas = -1;
	phonedev_alloc_channels(xpd, pri_num_channels(set_proto));
	phonedev->offhook_state = BITMASK(phonedev->channels);
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	priv->deflaw = deflaw;
	priv->dchan_num = dchan_num;
	priv->local_loopback = 0;
	xpd->type_name = type_name(priv->pri_protocol);
	XPD_DBG(GENERAL, xpd, "%s, channels=%d, dchan_num=%d, deflaw=%d\n",
		pri_protocol_name(set_proto), phonedev->channels,
		priv->dchan_num, priv->deflaw);
	/*
	 * Must set default now, so layer1 polling (Register REG_FRS0) would
	 * give reliable results.
	 */
	ret = pri_lineconfig(xpd, default_lineconfig);
	if (ret) {
		XPD_NOTICE(xpd, "Failed setting PRI default line config\n");
		return ret;
	}
	return apply_pri_protocol(xpd);
}

static void dahdi_update_syncsrc(xpd_t *xpd)
{
	struct PRI_priv_data *priv;
	xpd_t *subxpd;
	int best_spanno = 0;
	int i;

	if (!SPAN_REGISTERED(xpd))
		return;
	for (i = 0; i < MAX_SLAVES; i++) {
		subxpd = xpd_byaddr(xpd->xbus, xpd->addr.unit, i);
		if (!subxpd)
			continue;
		priv = subxpd->priv;
		if (priv->clock_source && priv->alarms == 0) {
			if (best_spanno)
				XPD_ERR(xpd,
					"Duplicate XPD with clock_source=1\n");
			best_spanno = PHONEDEV(subxpd).span.spanno;
		}
	}
	for (i = 0; i < MAX_SLAVES; i++) {
		subxpd = xpd_byaddr(xpd->xbus, xpd->addr.unit, i);
		if (!subxpd)
			continue;
		if (PHONEDEV(subxpd).span.syncsrc == best_spanno)
			XPD_DBG(SYNC, xpd, "Setting SyncSource to span %d\n",
				best_spanno);
		else
			XPD_DBG(SYNC, xpd, "Slaving to span %d\n", best_spanno);
		PHONEDEV(subxpd).span.syncsrc = best_spanno;
	}
}

/*
 * Called from:
 *   - set_master_mode() --
 *       As a result of dahdi_cfg
 *   - layer1_state() --
 *       As a result of an alarm.
 */
static void set_clocking(xpd_t *xpd)
{
	xbus_t *xbus;
	xpd_t *best_xpd = NULL;
	int best_subunit = -1;	/* invalid */
	unsigned int best_subunit_prio = INT_MAX;
	int i;

	xbus = xpd->xbus;
	/* Find subunit with best timing priority */
	for (i = 0; i < MAX_SLAVES; i++) {
		struct PRI_priv_data *priv;
		xpd_t *subxpd;

		subxpd = xpd_byaddr(xbus, xpd->addr.unit, i);
		if (!subxpd)
			continue;
		priv = subxpd->priv;
		if (priv->alarms != 0)
			continue;
		if (PHONEDEV(subxpd).timing_priority > 0
		    && PHONEDEV(subxpd).timing_priority < best_subunit_prio) {
			best_xpd = subxpd;
			best_subunit = i;
			best_subunit_prio = PHONEDEV(subxpd).timing_priority;
		}
	}
	/* Now set it */
	if (best_xpd
	    && ((struct PRI_priv_data *)(best_xpd->priv))->clock_source == 0) {
		__u8 reg_pc_init[] = { VAL_PC_GPI, VAL_PC_GPI, VAL_PC_GPI };

		for (i = 0; i < ARRAY_SIZE(reg_pc_init); i++) {
			__u8 reg_pc = reg_pc_init[i];

			reg_pc |=
			    (best_subunit & (1 << i)) ? VAL_PC_GPOH :
			    VAL_PC_GPOL;
			XPD_DBG(SYNC, best_xpd,
				"ClockSource Set: PC%d=0x%02X\n", 2 + i,
				reg_pc);
			pri_write_reg(xpd, REG_PC2 + i, reg_pc);
		}
		((struct PRI_priv_data *)(best_xpd->priv))->clock_source = 1;
	}
	/* clear old clock sources */
	for (i = 0; i < MAX_SLAVES; i++) {
		struct PRI_priv_data *priv;
		xpd_t *subxpd;

		subxpd = xpd_byaddr(xbus, xpd->addr.unit, i);
		if (subxpd && subxpd != best_xpd) {
			XPD_DBG(SYNC, subxpd, "Clearing clock source\n");
			priv = subxpd->priv;
			priv->clock_source = 0;
		}
	}
	dahdi_update_syncsrc(xpd);
}

static void set_reg_lim0(const char *msg, xpd_t *xpd)
{
	struct PRI_priv_data *priv;
	bool is_master_mode;
	bool localloop;
	__u8 lim0 = 0;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	is_master_mode = PHONEDEV(xpd).timing_priority == 0;
	localloop = priv->local_loopback;
	lim0 |= (localloop) ? REG_LIM0_LL : 0;
	if (is_master_mode)
		lim0 |= REG_LIM0_MAS;
	else
		lim0 &= ~REG_LIM0_MAS;
	XPD_DBG(SIGNAL, xpd, "%s(%s): %s, %s\n", __func__, msg,
		(is_master_mode) ? "MASTER" : "SLAVE",
		(localloop) ? "LOCALLOOP" : "NO_LOCALLOOP");
	write_subunit(xpd, REG_LIM0, lim0);
}

/*
 * Normally set by the timing parameter in /etc/dahdi/system.conf
 * If this is called by dahdi_cfg, than it's too late to change
 * dahdi sync priority (we are already registered)
 *
 * Also called from set_localloop()
 */
static int set_master_mode(const char *msg, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(SIGNAL, xpd, "\n");
	set_reg_lim0(__func__, xpd);
	set_clocking(xpd);
	return 0;
}

static int set_localloop(xpd_t *xpd, bool localloop)
{
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (SPAN_REGISTERED(xpd)) {
		XPD_NOTICE(xpd, "Registered as span %d. Cannot do %s\n",
			   PHONEDEV(xpd).span.spanno, __func__);
		return -EBUSY;
	}
	priv->local_loopback = localloop;
	XPD_DBG(SIGNAL, xpd, "%s: %s\n", __func__,
		(localloop) ? "LOCALLOOP" : "NO");
	set_master_mode(__func__, xpd);
	return 0;
}

#define	VALID_CONFIG(bit, flg, str)	[bit] = { .flags = flg, .name = str }

static const struct {
	const char *name;
	const int flags;
} valid_spanconfigs[sizeof(unsigned int) * 8] = {
	/* These apply to T1 */
	VALID_CONFIG(4, DAHDI_CONFIG_D4, "D4"),
	VALID_CONFIG(5, DAHDI_CONFIG_ESF, "ESF"),
	VALID_CONFIG(6, DAHDI_CONFIG_AMI, "AMI"),
	VALID_CONFIG(7, DAHDI_CONFIG_B8ZS, "B8ZS"),
	/* These apply to E1 */
	VALID_CONFIG(8, DAHDI_CONFIG_CCS, "CCS"),
	VALID_CONFIG(9, DAHDI_CONFIG_HDB3, "HDB3"),
	VALID_CONFIG(10, DAHDI_CONFIG_CRC4, "CRC4"),
};

/*
 * Mark the lines as CLEAR or RBS signalling.
 * With T1, we need to mark the CLEAR lines on the REG_CCB1_T registers
 * Should be called only when we are registered to DAHDI
 * The channo parameter:
 *	channo == 0: set lines for the whole span
 *	channo != 0: only set modified lines
 */
static void set_rbslines(xpd_t *xpd, int channo)
{
	struct PRI_priv_data *priv;
	xpp_line_t new_rbslines = 0;
	xpp_line_t modified_lines;
	int i;

	priv = xpd->priv;
	for_each_line(xpd, i) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);

		if (chan->flags & DAHDI_FLAG_CLEAR)
			BIT_CLR(new_rbslines, i);
		else
			BIT_SET(new_rbslines, i);
	}
	new_rbslines &= BITMASK(PHONEDEV(xpd).channels);
	modified_lines = priv->rbslines ^ new_rbslines;
	XPD_DBG(DEVICES, xpd, "RBSLINES-%d(%s): 0x%X\n", channo,
		pri_protocol_name(priv->pri_protocol), new_rbslines);
	if ((priv->pri_protocol == PRI_PROTO_T1)
	    || (priv->pri_protocol == PRI_PROTO_J1)) {
		__u8 clear_lines = 0;	/* Mark clear lines */
		bool reg_changed = 0;

		for_each_line(xpd, i) {
			int bytenum = i / 8;
			int bitnum = i % 8;

			if (!IS_SET(new_rbslines, i))
				BIT_SET(clear_lines, (7 - bitnum));
			if (IS_SET(modified_lines, i))
				reg_changed = 1;
			if (bitnum == 7) {
				if (channo == 0 || reg_changed) {
					bytenum += REG_CCB1_T;
					XPD_DBG(DEVICES, xpd,
						"RBS(%s): modified=0x%X rbslines=0x%X reg=0x%X clear_lines=0x%X\n",
						pri_protocol_name(priv->pri_protocol),
						modified_lines, new_rbslines,
						bytenum, clear_lines);
					write_subunit(xpd, bytenum, clear_lines);
				}
				clear_lines = 0;
				reg_changed = 0;
			}
		}
	}
	priv->rbslines = new_rbslines;
}

static int set_mode_cas(xpd_t *xpd, bool want_cas)
{
	struct PRI_priv_data *priv;

	priv = xpd->priv;
	XPD_INFO(xpd, "Setting TDM to %s\n", (want_cas) ? "CAS" : "PRI");
	if (want_cas) {
		priv->is_cas = 1;
		priv->dchan_alive = 0;
	} else {
		priv->is_cas = 0;
	}
	return 0;
}

static int pri_lineconfig(xpd_t *xpd, int lineconfig)
{
	struct PRI_priv_data *priv;
	const char *framingstr = "";
	const char *codingstr = "";
	const char *crcstr = "";
#ifdef JAPANEZE_SUPPORT
	__u8 rc0 = 0;		/* FIXME: PCM offsets */
#endif
	__u8 fmr0 = 0;
	__u8 fmr1 = REG_FMR1_ECM;
	__u8 fmr2 = 0;
	__u8 fmr3 = 0;		/* write only for CRC4 */
	__u8 fmr4 = 0;
	__u8 cmdr = REG_CMDR_RRES | REG_CMDR_XRES;
	__u8 xsp = 0;
	unsigned int bad_bits;
	bool force_cas = 0;
	int i;

	BUG_ON(!xpd);
	priv = xpd->priv;
	/*
	 * validate
	 */
	bad_bits = lineconfig & pri_linecompat(priv->pri_protocol);
	bad_bits = bad_bits ^ lineconfig;
	for (i = 0; i < ARRAY_SIZE(valid_spanconfigs); i++) {
		unsigned int flags = valid_spanconfigs[i].flags;

		if (bad_bits & BIT(i)) {
			if (flags) {
				XPD_ERR(xpd,
					"Bad config item '%s' for %s. Ignore\n",
					valid_spanconfigs[i].name,
					pri_protocol_name(priv->pri_protocol));
			} else {
				/* we got real garbage */
				XPD_ERR(xpd,
					"Unknown config item 0x%lX for %s. "
					"Ignore.\n",
					BIT(i),
					pri_protocol_name(priv->pri_protocol));
			}
		}
		if (flags && flags != BIT(i)) {
			ERR("%s: BUG: i=%d flags=0x%X\n", __func__, i, flags);
			// BUG();
		}
	}
	if (bad_bits)
		goto bad_lineconfig;
	if (priv->pri_protocol == PRI_PROTO_E1) {
		fmr1 |= REG_FMR1_AFR;
		fmr2 = REG_FMR2_E_AXRA | REG_FMR2_E_ALMF;	/* 0x03 */
		fmr4 = 0x9F;	/*  E1.XSW:  All spare bits = 1 */
		xsp |= REG_XSP_E_EBP | REG_XSP_E_AXS | REG_XSP_E_XSIF;
	} else if (priv->pri_protocol == PRI_PROTO_T1) {
		fmr1 |= REG_FMR1_PMOD | REG_FMR1_T_CRC;
		fmr2 = REG_FMR2_T_SSP | REG_FMR2_T_AXRA;	/* 0x22 */
		fmr4 = 0x0C;
		xsp &= ~REG_FMR5_T_XTM;
		force_cas = 1;	/* T1 - Chip always in CAS mode */
	} else if (priv->pri_protocol == PRI_PROTO_J1) {
		fmr1 |= REG_FMR1_PMOD;
		fmr4 = 0x1C;
		xsp &= ~REG_FMR5_T_XTM;
		force_cas = 1;	/* T1 - Chip always in CAS mode */
		XPD_ERR(xpd, "J1 unsupported yet\n");
		return -ENOSYS;
	}
	if (priv->local_loopback)
		fmr2 |= REG_FMR2_E_PLB;
	/* framing first */
	if (lineconfig & DAHDI_CONFIG_B8ZS) {
		framingstr = "B8ZS";
		fmr0 =
		    REG_FMR0_E_XC1 | REG_FMR0_E_XC0 | REG_FMR0_E_RC1 |
		    REG_FMR0_E_RC0;
	} else if (lineconfig & DAHDI_CONFIG_AMI) {
		framingstr = "AMI";
		fmr0 = REG_FMR0_E_XC1 | REG_FMR0_E_RC1;
		/*
		 * From Infineon Errata Sheet: PEF 22554, Version 3.1
		 * Problem: Incorrect CAS Receiption when
		 *          using AMI receive line code
		 * Workaround: For E1,
		 *               "...The receive line coding HDB3 is
		 *                recommended instead."
		 *             For T1,
		 *               "...in T1 mode it is recommended to
		 *                configure the Rx side to B8ZS coding"
		 * For both cases this is the same bit in FMR0
		 */
		if (priv->pri_protocol == PRI_PROTO_J1)
			XPD_NOTICE(xpd, "J1 is not supported yet\n");
		else
			fmr0 |= REG_FMR0_E_RC0;
	} else if (lineconfig & DAHDI_CONFIG_HDB3) {
		framingstr = "HDB3";
		fmr0 =
		    REG_FMR0_E_XC1 | REG_FMR0_E_XC0 | REG_FMR0_E_RC1 |
		    REG_FMR0_E_RC0;
	} else {
		XPD_NOTICE(xpd,
			   "Bad lineconfig. Not (B8ZS|AMI|HDB3). Ignored.\n");
		return -EINVAL;
	}
	/* then coding */
	priv->is_esf = 0;
	if (lineconfig & DAHDI_CONFIG_ESF) {
		codingstr = "ESF";
		fmr4 |= REG_FMR4_FM1;
		fmr2 |= REG_FMR2_T_AXRA | REG_FMR2_T_MCSP | REG_FMR2_T_SSP;
		priv->is_esf = 1;
	} else if (lineconfig & DAHDI_CONFIG_D4) {
		codingstr = "D4";
	} else if (lineconfig & DAHDI_CONFIG_CCS) {
		codingstr = "CCS";
		/* In E1 we know right from the span statement. */
		set_mode_cas(xpd, 0);
	} else {
		/* In E1 we know right from the span statement. */
		codingstr = "CAS";
		force_cas = 1;
		set_mode_cas(xpd, 1);
	}
	CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
	/*
	 * E1's can enable CRC checking
	 * CRC4 is legal only for E1, and it is checked by pri_linecompat()
	 * in the beginning of the function.
	 */
	if (lineconfig & DAHDI_CONFIG_CRC4) {
		crcstr = "CRC4";
		fmr1 |= REG_FMR1_E_XFS;
		fmr2 |= REG_FMR2_E_RFS1;
		fmr3 |= REG_FMR3_EXTIW;
	}
	XPD_DBG(GENERAL, xpd, "[%s] lineconfig=%s/%s/%s %s (0x%X)\n",
		(priv->clock_source) ? "MASTER" : "SLAVE", framingstr,
		codingstr, crcstr,
		(lineconfig & DAHDI_CONFIG_NOTOPEN) ? "YELLOW" : "",
		lineconfig);
	set_reg_lim0(__func__, xpd);
	XPD_DBG(GENERAL, xpd, "%s: fmr1(0x%02X) = 0x%02X\n", __func__, REG_FMR1,
		fmr1);
	write_subunit(xpd, REG_FMR1, fmr1);
	XPD_DBG(GENERAL, xpd, "%s: fmr2(0x%02X) = 0x%02X\n", __func__, REG_FMR2,
		fmr2);
	write_subunit(xpd, REG_FMR2, fmr2);
	XPD_DBG(GENERAL, xpd, "%s: fmr0(0x%02X) = 0x%02X\n", __func__, REG_FMR0,
		fmr0);
	write_subunit(xpd, REG_FMR0, fmr0);
	XPD_DBG(GENERAL, xpd, "%s: fmr4(0x%02X) = 0x%02X\n", __func__, REG_FMR4,
		fmr4);
	write_subunit(xpd, REG_FMR4, fmr4);
	if (fmr3) {
		XPD_DBG(GENERAL, xpd, "%s: fmr3(0x%02X) = 0x%02X\n", __func__,
			REG_FMR3, fmr3);
		write_subunit(xpd, REG_FMR3, fmr3);
	}
	XPD_DBG(GENERAL, xpd, "%s: cmdr(0x%02X) = 0x%02X\n", __func__,
		REG_CMDR_E, cmdr);
	write_subunit(xpd, REG_CMDR_E, cmdr);
#ifdef JAPANEZE_SUPPORT
	if (rc0) {
		XPD_DBG(GENERAL, xpd, "%s: rc0(0x%02X) = 0x%02X\n", __func__,
			REG_RC0, rc0);
		write_subunit(xpd, REG_RC0, rc0);
	}
#endif
	if (force_cas) {
		if (priv->pri_protocol == PRI_PROTO_E1) {
			int rs1 = 0x0B;

			/*
			 * Set correct X1-X3 bits in the E1 CAS MFAS
			 * They are unused in E1 and should be 1
			 */
			XPD_DBG(GENERAL, xpd, "%s: rs1(0x%02X) = 0x%02X\n",
				__func__, REG_RS1_E, rs1);
			write_subunit(xpd, REG_RS1_E, rs1);
		}
		xsp |= REG_XSP_E_CASEN;	/* Same as REG_FMR5_T_EIBR for T1 */
	}
	XPD_DBG(GENERAL, xpd, "%s: xsp(0x%02X) = 0x%02X\n", __func__, REG_XSP_E,
		xsp);
	write_subunit(xpd, REG_XSP_E, xsp);
	return 0;
bad_lineconfig:
	XPD_ERR(xpd, "Bad lineconfig. Abort\n");
	return -EINVAL;
}

static int pri_set_spantype(struct dahdi_span *span, enum spantypes spantype)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	enum pri_protocol set_proto = PRI_PROTO_0;
	int ret;

	XPD_INFO(xpd, "%s: %s\n", __func__, dahdi_spantype2str(spantype));
	switch (spantype) {
	case SPANTYPE_DIGITAL_E1:
		set_proto = PRI_PROTO_E1;
		break;
	case SPANTYPE_DIGITAL_T1:
		set_proto = PRI_PROTO_T1;
		break;
	case SPANTYPE_DIGITAL_J1:
		set_proto = PRI_PROTO_J1;
		break;
	default:
		XPD_NOTICE(xpd, "%s: bad spantype '%s'\n",
			__func__, dahdi_spantype2str(spantype));
		return -EINVAL;
	}
	ret = set_pri_proto(xpd, set_proto);
	if (ret < 0) {
		XPD_ERR(xpd, "%s: set_pri_proto failed\n", __func__);
		return ret;
	}
	dahdi_init_span(span);
	return 0;
}

static int PRI_card_open(xpd_t *xpd, lineno_t pos)
{
	struct PRI_priv_data *priv;
	int d;

	/*
	 * DAHDI without AUDIO_NOTIFY.
	 * Need to offhook all channels when D-Chan is up
	 */
	priv = xpd->priv;
	d = PRI_DCHAN_IDX(priv);
	BUG_ON(!xpd);
	if (pos == d) {
#ifndef	DAHDI_AUDIO_NOTIFY
		int i;

		LINE_DBG(SIGNAL, xpd, pos, "OFFHOOK the whole span\n");
		for_each_line(xpd, i) {
			if (i != d)
				BIT_SET(PHONEDEV(xpd).offhook_state, i);
		}
		CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
#endif
		priv->dchan_is_open = 1;
	}
	return 0;
}

static int PRI_card_close(xpd_t *xpd, lineno_t pos)
{
	struct PRI_priv_data *priv;
	int d, i;

	priv = xpd->priv;
	d = PRI_DCHAN_IDX(priv);
	BUG_ON(!xpd);
	if (pos == d) {
		LINE_DBG(SIGNAL, xpd, pos, "OFFHOOK the whole span\n");
		for_each_line(xpd, i) {
			if (i != d)
				BIT_CLR(PHONEDEV(xpd).offhook_state, i);
		}
		CALL_PHONE_METHOD(card_pcm_recompute, xpd, 0);
		dchan_state(xpd, 0);
		priv->dchan_is_open = 0;
	} else if (!priv->dchan_is_open)
		mark_offhook(xpd, pos, 0);	/* e.g: patgen/pattest */
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */

static int pri_spanconfig(struct file *file, struct dahdi_span *span,
			  struct dahdi_lineconfig *lc)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct PRI_priv_data *priv;
	int ret;

	BUG_ON(!xpd);
	priv = xpd->priv;
	if (lc->span != PHONEDEV(xpd).span.spanno) {
		XPD_ERR(xpd, "I am span %d but got spanconfig for span %d\n",
			PHONEDEV(xpd).span.spanno, lc->span);
		return -EINVAL;
	}
	/*
	 * FIXME: lc->name is unused by dahdi_cfg and dahdi...
	 *        We currently ignore it also.
	 */
	XPD_DBG(GENERAL, xpd, "[%s] lbo=%d lineconfig=0x%X sync=%d\n",
		(priv->clock_source) ? "MASTER" : "SLAVE", lc->lbo,
		lc->lineconfig, lc->sync);
	ret = pri_lineconfig(xpd, lc->lineconfig);
	if (!ret) {
		span->lineconfig = lc->lineconfig;
		PHONEDEV(xpd).timing_priority = lc->sync;
		set_master_mode("spanconfig", xpd);
		elect_syncer("PRI-master_mode");
	}
	return ret;
}

/*
 * Set signalling type (if appropriate)
 * Called from dahdi with spinlock held on chan. Must not call back
 * dahdi functions.
 */
static int pri_chanconfig(struct file *file, struct dahdi_chan *chan,
			  int sigtype)
{
	struct phonedev *phonedev =
	    container_of(chan->span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	DBG(GENERAL, "channel %d (%s) -> %s\n", chan->channo, chan->name,
	    sig2str(sigtype));
	/*
	 * Some bookkeeping to check if we have DChan defined or not
	 * FIXME: actually use this to prevent duplicate DChan definitions
	 *        and prevent DChan definitions with CAS.
	 */
	if (is_sigtype_dchan(sigtype)) {
		if (VALID_DCHAN(priv) && DCHAN(priv) != chan->channo) {
			ERR("channel %d (%s) marked DChan but "
				"also channel %d.\n",
				chan->channo, chan->name, DCHAN(priv));
			return -EINVAL;
		}
		XPD_DBG(GENERAL, xpd, "channel %d (%s) marked as DChan\n",
			chan->channo, chan->name);
		SET_DCHAN(priv, chan->channo);
		/* In T1, we don't know before-hand */
		if (priv->pri_protocol != PRI_PROTO_E1 && priv->is_cas != 0)
			set_mode_cas(xpd, 0);
	} else {
		if (chan->chanpos == 1) {
			XPD_DBG(GENERAL, xpd,
				"channel %d (%s) marked a not DChan\n",
				chan->channo, chan->name);
			SET_DCHAN(priv, NO_DCHAN);
		}
		/* In T1, we don't know before-hand */
		if (priv->pri_protocol != PRI_PROTO_E1 && priv->is_cas != 1)
			set_mode_cas(xpd, 1);
	}
	if (PHONEDEV(xpd).span.flags & DAHDI_FLAG_RUNNING) {
		XPD_DBG(DEVICES, xpd, "Span is RUNNING. Updating rbslines.\n");
		set_rbslines(xpd, chan->channo);
	}
	// FIXME: sanity checks:
	// - should be supported (within the sigcap)
	// - should not replace fxs <->fxo ??? (covered by previous?)
	return 0;
}

static xpd_t *PRI_card_new(xbus_t *xbus, int unit, int subunit,
			   const xproto_table_t *proto_table, __u8 subtype,
			   int subunits, int subunit_ports, bool to_phone)
{
	xpd_t *xpd = NULL;
	struct PRI_priv_data *priv;
	int channels = min(31, CHANNELS_PERXPD);	/* worst case */

	if (subunit_ports != 1) {
		XBUS_ERR(xbus, "Bad subunit_ports=%d\n", subunit_ports);
		return NULL;
	}
	XBUS_DBG(GENERAL, xbus, "\n");
	xpd =
	    xpd_alloc(xbus, unit, subunit, subtype, subunits,
		      sizeof(struct PRI_priv_data), proto_table, channels);
	if (!xpd)
		return NULL;
	priv = xpd->priv;
	/* Default, changes in set_pri_proto() */
	priv->pri_protocol = PRI_PROTO_0;
	/* Default, changes in set_pri_proto() */
	priv->deflaw = DAHDI_LAW_DEFAULT;
	xpd->type_name = type_name(priv->pri_protocol);
	xbus->sync_mode_default = SYNC_MODE_AB;
	return xpd;
}

static int PRI_card_init(xbus_t *xbus, xpd_t *xpd)
{
	struct PRI_priv_data *priv;
	int ret = 0;

	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	xpd->type = XPD_TYPE_PRI;
	priv = xpd->priv;
	if (priv->pri_protocol == PRI_PROTO_0) {
		/*
		 * init_card_* script didn't set pri protocol
		 * Let's have a default E1
		 */
		ret = set_pri_proto(xpd, PRI_PROTO_E1);
		if (ret < 0)
			goto err;
	}
	SET_DCHAN(priv, NO_DCHAN);
	/*
	 * initialization script should have set correct
	 * operating modes.
	 */
	if (!valid_pri_modes(xpd)) {
		XPD_NOTICE(xpd, "PRI protocol not set\n");
		goto err;
	}
	xpd->type_name = type_name(priv->pri_protocol);
	PHONEDEV(xpd).direction = TO_PSTN;
	XPD_DBG(DEVICES, xpd, "%s\n", xpd->type_name);
	PHONEDEV(xpd).timing_priority = 1;	/* High priority SLAVE */
	set_master_mode(__func__, xpd);
	for (ret = 0; ret < NUM_LEDS; ret++) {
		DO_LED(xpd, ret, PRI_LED_ON);
		msleep(20);
		DO_LED(xpd, ret, PRI_LED_OFF);
	}
	priv->initialized = 1;
	priv->dchan_is_open = 0;
	return 0;
err:
	XPD_ERR(xpd, "Failed initializing registers (%d)\n", ret);
	return ret;
}

static int PRI_card_remove(xbus_t *xbus, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	return 0;
}

#ifdef	DAHDI_AUDIO_NOTIFY
static int pri_audio_notify(struct dahdi_chan *chan, int on)
{
	xpd_t *xpd = chan->pvt;
	int pos = chan->chanpos - 1;

	BUG_ON(!xpd);
	LINE_DBG(SIGNAL, xpd, pos, "PRI-AUDIO: %s\n", (on) ? "on" : "off");
	mark_offhook(xpd, pos, on);
	return 0;
}
#endif

static const struct dahdi_span_ops PRI_span_ops = {
	.owner = THIS_MODULE,
	.set_spantype = pri_set_spantype,
	.spanconfig = pri_spanconfig,
	.chanconfig = pri_chanconfig,
	.startup = pri_startup,
	.shutdown = pri_shutdown,
	.rbsbits = pri_rbsbits,
	.open = xpp_open,
	.close = xpp_close,
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
	.audio_notify = pri_audio_notify,
#endif
};

static int apply_pri_protocol(xpd_t *xpd)
{
	xbus_t *xbus;
	struct PRI_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	priv = xpd->priv;
	BUG_ON(!xbus);
	XPD_DBG(GENERAL, xpd, "\n");
	PHONEDEV(xpd).span.spantype = pri_protocol2spantype(priv->pri_protocol);
	PHONEDEV(xpd).span.linecompat = pri_linecompat(priv->pri_protocol);
	PHONEDEV(xpd).span.deflaw = priv->deflaw;
	PHONEDEV(xpd).span.alarms = DAHDI_ALARM_NONE;
	for_each_line(xpd, i) {
		struct dahdi_chan *cur_chan = XPD_CHAN(xpd, i);
		bool is_dchan = i == PRI_DCHAN_IDX(priv);

		XPD_DBG(GENERAL, xpd, "setting PRI channel %d (%s)\n", i,
			(is_dchan) ? "DCHAN" : "CLEAR");
		snprintf(cur_chan->name, MAX_CHANNAME, "XPP_%s/%02d/%1d%1d/%d",
			 xpd->type_name, xbus->num, xpd->addr.unit,
			 xpd->addr.subunit, i);
		cur_chan->chanpos = i + 1;
		cur_chan->pvt = xpd;
		cur_chan->sigcap = PRI_SIGCAP;
		if (is_dchan && !priv->is_cas) {	/* D-CHAN */
			//FIXME: cur_chan->flags |= DAHDI_FLAG_PRIDCHAN;
			cur_chan->flags &= ~DAHDI_FLAG_HDLC;
		}
	}
	PHONEDEV(xpd).offhook_state = PHONEDEV(xpd).wanted_pcm_mask;
	PHONEDEV(xpd).span.ops = &PRI_span_ops;
	PHONEDEV(xpd).span.channels = PHONEDEV(xpd).channels;
	xpd_set_spanname(xpd);
	return 0;
}

static int PRI_card_dahdi_preregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	priv = xpd->priv;
	BUG_ON(!xbus);
	XPD_DBG(GENERAL, xpd, "%s (proto=%s, channels=%d, deflaw=%d)\n",
		(on) ? "on" : "off", pri_protocol_name(priv->pri_protocol),
		PHONEDEV(xpd).channels, priv->deflaw);
	if (!on) {
		/* Nothing to do yet */
		return 0;
	}
	return apply_pri_protocol(xpd);
}

static int PRI_card_dahdi_postregistration(xpd_t *xpd, bool on)
{
	xbus_t *xbus;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	BUG_ON(!xbus);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	dahdi_update_syncsrc(xpd);
	return (0);
}

static void dchan_state(xpd_t *xpd, bool up)
{
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->is_cas)
		return;
	if (priv->dchan_alive == up)
		return;
	if (!priv->layer1_up)	/* No layer1, kill dchan */
		up = 0;
	if (up) {
		XPD_DBG(SIGNAL, xpd, "STATE CHANGE: D-Channel RUNNING\n");
		priv->dchan_alive = 1;
	} else {
		int d = PRI_DCHAN_IDX(priv);

		if (SPAN_REGISTERED(xpd) && d >= 0
		    && d < PHONEDEV(xpd).channels) {
			__u8 *pcm;

			pcm = (__u8 *)XPD_CHAN(xpd, d)->readchunk;
			pcm[0] = 0x00;
			pcm = (__u8 *)XPD_CHAN(xpd, d)->writechunk;
			pcm[0] = 0x00;
		}
		XPD_DBG(SIGNAL, xpd, "STATE CHANGE: D-Channel STOPPED\n");
		priv->dchan_rx_counter = priv->dchan_tx_counter = 0;
		priv->dchan_alive = 0;
		priv->dchan_alive_ticks = 0;
		priv->dchan_rx_sample = priv->dchan_tx_sample = 0x00;
	}
}

/*
 * LED managment is done by the driver now:
 *   - Turn constant ON RED/GREEN led to indicate MASTER/SLAVE port
 *   - Very fast "Double Blink" to indicate Layer1 alive (without D-Channel)
 *   - Constant blink (1/2 sec cycle) to indicate D-Channel alive.
 */
static void handle_leds(xbus_t *xbus, xpd_t *xpd)
{
	struct PRI_priv_data *priv;
	unsigned int timer_count;
	int which_led;
	int other_led;
	enum pri_led_state ledstate;
	int mod;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (PHONEDEV(xpd).timing_priority == 0) {
		which_led = TOP_RED_LED;
		other_led = BOTTOM_GREEN_LED;
	} else {
		which_led = BOTTOM_GREEN_LED;
		other_led = TOP_RED_LED;
	}
	ledstate = priv->ledstate[which_led];
	timer_count = xpd->timer_count;
	if (xpd->blink_mode) {
		if ((timer_count % DEFAULT_LED_PERIOD) == 0) {
			// led state is toggled
			if (ledstate == PRI_LED_OFF) {
				DO_LED(xpd, which_led, PRI_LED_ON);
				DO_LED(xpd, other_led, PRI_LED_ON);
			} else {
				DO_LED(xpd, which_led, PRI_LED_OFF);
				DO_LED(xpd, other_led, PRI_LED_OFF);
			}
		}
		return;
	}
	if (priv->ledstate[other_led] != PRI_LED_OFF)
		DO_LED(xpd, other_led, PRI_LED_OFF);
	if (priv->dchan_alive) {
		mod = timer_count % 1000;
		switch (mod) {
		case 0:
			DO_LED(xpd, which_led, PRI_LED_ON);
			break;
		case 500:
			DO_LED(xpd, which_led, PRI_LED_OFF);
			break;
		}
	} else if (priv->layer1_up) {
		mod = timer_count % 1000;
		switch (mod) {
		case 0:
		case 100:
			DO_LED(xpd, which_led, PRI_LED_ON);
			break;
		case 50:
		case 150:
			DO_LED(xpd, which_led, PRI_LED_OFF);
			break;
		}
	} else {
		if (priv->ledstate[which_led] != PRI_LED_ON)
			DO_LED(xpd, which_led, PRI_LED_ON);
	}
}

static int PRI_card_tick(xbus_t *xbus, xpd_t *xpd)
{
	struct PRI_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!priv->initialized || !xbus->self_ticking)
		return 0;
	/*
	 * Poll layer1 status (cascade subunits)
	 */
	if (poll_interval != 0 && ((xpd->timer_count % poll_interval) == 0)) {
		priv->poll_noreplies++;
		query_subunit(xpd, REG_FRS0);
		//query_subunit(xpd, REG_FRS1);
	}
	if (priv->dchan_tx_counter >= 1 && priv->dchan_rx_counter > 1) {
		dchan_state(xpd, 1);
		priv->dchan_alive_ticks++;
	}
	handle_leds(xbus, xpd);
	return 0;
}

static int PRI_card_ioctl(xpd_t *xpd, int pos, unsigned int cmd,
			  unsigned long arg)
{
	struct dahdi_chan *chan;

	BUG_ON(!xpd);
	if (!XBUS_IS(xpd->xbus, READY))
		return -ENODEV;
	chan = XPD_CHAN(xpd, pos);
	switch (cmd) {
		/*
		 * Asterisk may send FXS type ioctl()'s to us:
		 *   - Some are sent to everybody (DAHDI_TONEDETECT)
		 *   - Some are sent because we may be in CAS mode
		 *     (FXS signalling)
		 * Ignore them.
		 */
	case DAHDI_TONEDETECT:
		LINE_DBG(SIGNAL, xpd, pos, "PRI: TONEDETECT (%s)\n",
			 (chan->
			  flags & DAHDI_FLAG_AUDIO) ? "AUDIO" : "SILENCE");
		return -ENOTTY;
	case DAHDI_ONHOOKTRANSFER:
		LINE_DBG(SIGNAL, xpd, pos, "PRI: ONHOOKTRANSFER\n");
		return -ENOTTY;
	case DAHDI_VMWI:
		LINE_DBG(SIGNAL, xpd, pos, "PRI: VMWI\n");
		return -ENOTTY;
	case DAHDI_VMWI_CONFIG:
		LINE_DBG(SIGNAL, xpd, pos, "PRI: VMWI_CONFIG\n");
		return -ENOTTY;
	case DAHDI_SETPOLARITY:
		LINE_DBG(SIGNAL, xpd, pos, "PRI: SETPOLARITY\n");
		return -ENOTTY;
		/* report on really bad ioctl()'s */
	default:
		report_bad_ioctl(THIS_MODULE->name, xpd, pos, cmd);
		return -ENOTTY;
	}
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */
static int pri_startup(struct file *file, struct dahdi_span *span)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct PRI_priv_data *priv;

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
	set_rbslines(xpd, 0);
	write_subunit(xpd, REG_XPM2, 0x00);
	return 0;
}

/*
 * Called only for 'span' keyword in /etc/dahdi/system.conf
 */
static int pri_shutdown(struct dahdi_span *span)
{
	struct phonedev *phonedev = container_of(span, struct phonedev, span);
	xpd_t *xpd = container_of(phonedev, struct xpd, phonedev);
	struct PRI_priv_data *priv;

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

static int encode_rbsbits_e1(xpd_t *xpd, int pos, int bits)
{
	struct PRI_priv_data *priv;
	__u8 val;
	int rsnum;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	BUG_ON(priv->pri_protocol != PRI_PROTO_E1);
	if (pos == 15)
		return 0;	/* Don't write dchan in CAS */
	if (pos < 0 || pos > 31) {
		XPD_NOTICE(xpd, "%s: pos=%d out of range. Ignore\n", __func__,
			   pos);
		return 0;
	}
	if (pos >= 16) {
		/* Low nibble */
		rsnum = pos - 16;
		val = (priv->cas_ts_e[rsnum] & 0xF0) | (bits & 0x0F);
	} else {
		/* High nibble */
		rsnum = pos;
		val = (priv->cas_ts_e[rsnum] & 0x0F) | ((bits << 4) & 0xF0);
	}
	LINE_DBG(SIGNAL, xpd, pos, "RBS: TX: bits=0x%X\n", bits);
	write_cas_reg(xpd, rsnum, val);
	return 0;
}

static int encode_rbsbits_t1(xpd_t *xpd, int pos, int bits)
{
	struct PRI_priv_data *priv;
	int rsnum;
	int chan_per_reg;
	int offset;
	int width;
	uint tx_bits = bits;
	uint mask;
	__u8 val;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	BUG_ON(priv->pri_protocol != PRI_PROTO_T1);
	if (pos < 0 || pos >= PHONEDEV(xpd).channels) {
		XPD_ERR(xpd, "%s: Bad pos=%d\n", __func__, pos);
		return 0;
	}
	chan_per_reg = CHAN_PER_REGS(priv);
	width = 8 / chan_per_reg;
	rsnum = pos / chan_per_reg;
	offset = pos % chan_per_reg;
	mask = BITMASK(width) << (chan_per_reg - offset - 1) * width;
	if (!priv->is_esf)
		tx_bits >>= 2;
	tx_bits &= BITMASK(width);
	tx_bits <<= (chan_per_reg - offset - 1) * width;
	val = priv->cas_ts_e[rsnum];
	val &= ~mask;
	val |= tx_bits;
	LINE_DBG(SIGNAL, xpd, pos,
		 "bits=0x%02X RS%02d(%s) offset=%d tx_bits=0x%02X\n", bits,
		 rsnum + 1, (priv->is_esf) ? "esf" : "d4", offset, tx_bits);
	write_cas_reg(xpd, rsnum, val);
	priv->dchan_tx_counter++;
	return 0;
}

static void send_idlebits(xpd_t *xpd, bool saveold)
{
	struct PRI_priv_data *priv;
	__u8 save_rs[NUM_CAS_RS_E];
	int i;

	if (!SPAN_REGISTERED(xpd))
		return;
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(SIGNAL, xpd, "saveold=%d\n", saveold);
	if (saveold)
		memcpy(save_rs, priv->cas_ts_e, sizeof(save_rs));
	for_each_line(xpd, i) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);

		pri_rbsbits(chan, chan->idlebits);
	}
	if (saveold)
		memcpy(priv->cas_ts_e, save_rs, sizeof(save_rs));
}

static void send_oldbits(xpd_t *xpd)
{
	struct PRI_priv_data *priv;
	int i;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(SIGNAL, xpd, "\n");
	for (i = 0; i < cas_numregs(xpd); i++)
		write_cas_reg(xpd, i, priv->cas_ts_e[i]);
}

static int pri_rbsbits(struct dahdi_chan *chan, int bits)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	int pos;

	xpd = chan->pvt;
	BUG_ON(!xpd);
	pos = chan->chanpos - 1;
	priv = xpd->priv;
	BUG_ON(!priv);
	if (!priv->is_cas) {
		XPD_DBG(SIGNAL, xpd, "RBS: TX: not in CAS mode. Ignore.\n");
		return 0;
	}
	if (chan->sig == DAHDI_SIG_NONE) {
		LINE_DBG(SIGNAL, xpd, pos,
			 "RBS: TX: sigtyp=%s. , bits=0x%X. Ignore.\n",
			 sig2str(chan->sig), bits);
		return 0;
	}
	if (!priv->layer1_up)
		XPD_DBG(SIGNAL, xpd, "RBS: TX: No layer1 yet. Keep going.\n");
	if (priv->pri_protocol == PRI_PROTO_E1) {
		if (encode_rbsbits_e1(xpd, pos, bits) < 0)
			return -EINVAL;
	} else if (priv->pri_protocol == PRI_PROTO_T1) {
		if (encode_rbsbits_t1(xpd, pos, bits) < 0)
			return -EINVAL;
	} else {
		XPD_NOTICE(xpd,
			   "%s: protocol %s is not supported yet with CAS\n",
			   __func__, pri_protocol_name(priv->pri_protocol));
		return -EINVAL;
	}
	return 0;
}

/*! Copy PCM chunks from the buffers of the xpd to a new packet
 * \param xbus	xbus of source xpd.
 * \param xpd	source xpd.
 * \param lines	a bitmask of the active channels that need to be copied.
 * \param pack	packet to be filled.
 *
 * On PRI this function is should also shift the lines mask one bit, as
 * channel 0 on the wire is an internal chip control channel. We only
 * send 31 channels to the device, but they should be called 1-31 rather
 * than 0-30 .
 */
static void PRI_card_pcm_fromspan(xpd_t *xpd, xpacket_t *pack)
{
	struct PRI_priv_data *priv;
	__u8 *pcm;
	unsigned long flags;
	int i;
	xpp_line_t wanted_lines;
	int physical_chan;
	int physical_mask = 0;

	BUG_ON(!xpd);
	BUG_ON(!pack);
	priv = xpd->priv;
	BUG_ON(!priv);
	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, pcm);
	spin_lock_irqsave(&xpd->lock, flags);
	wanted_lines = PHONEDEV(xpd).wanted_pcm_mask;
	physical_chan = 0;
	for_each_line(xpd, i) {
		struct dahdi_chan *chan = XPD_CHAN(xpd, i);

		if (priv->pri_protocol == PRI_PROTO_E1) {
			/* In E1 - Only 0'th channel is unused */
			if (i == 0)
				physical_chan++;
		} else if (priv->pri_protocol == PRI_PROTO_T1) {
			/* In T1 - Every 4'th channel is unused */
			if ((i % 3) == 0)
				physical_chan++;
		}
		if (IS_SET(wanted_lines, i)) {
			physical_mask |= BIT(physical_chan);
			if (SPAN_REGISTERED(xpd)) {
#ifdef	DEBUG_PCMTX
				int channo = XPD_CHAN(xpd, i)->channo;

				if (pcmtx >= 0 && pcmtx_chan == channo)
					memset((u_char *)pcm, pcmtx,
					       DAHDI_CHUNKSIZE);
				else
#endif
					memcpy((u_char *)pcm, chan->writechunk,
					       DAHDI_CHUNKSIZE);
				if (i == PRI_DCHAN_IDX(priv)) {
					if (priv->dchan_tx_sample !=
					    chan->writechunk[0]) {
						priv->dchan_tx_sample =
						    chan->writechunk[0];
						priv->dchan_tx_counter++;
					} else if (chan->writechunk[0] == 0xFF)
						dchan_state(xpd, 0);
					else
						/* Clobber for next tick */
						chan->writechunk[0] = 0xFF;
				}
			} else
				memset((u_char *)pcm, DAHDI_XLAW(0, chan),
				       DAHDI_CHUNKSIZE);
			pcm += DAHDI_CHUNKSIZE;
		}
		physical_chan++;
	}
	RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, lines) = physical_mask;
	XPD_COUNTER(xpd, PCM_WRITE)++;
	spin_unlock_irqrestore(&xpd->lock, flags);
}

/*! Copy PCM chunks from the packet we received to the xpd struct.
 * \param xbus	xbus of target xpd.
 * \param xpd	target xpd.
 * \param pack	Source packet.
 *
 * On PRI this function is should also shift the lines back mask one bit, as
 * channel 0 on the wire is an internal chip control channel.
 *
 * \see PRI_card_pcm_fromspan
 */
static void PRI_card_pcm_tospan(xpd_t *xpd, xpacket_t *pack)
{
	struct PRI_priv_data *priv;
	__u8 *pcm;
	xpp_line_t physical_mask;
	unsigned long flags;
	int i;
	int logical_chan;

	if (!SPAN_REGISTERED(xpd))
		return;
	priv = xpd->priv;
	BUG_ON(!priv);
	pcm = RPACKET_FIELD(pack, GLOBAL, PCM_READ, pcm);
	physical_mask = RPACKET_FIELD(pack, GLOBAL, PCM_WRITE, lines);
	spin_lock_irqsave(&xpd->lock, flags);
	logical_chan = 0;
	for (i = 0; i < CHANNELS_PERXPD; i++) {
		volatile u_char *r;

		if (priv->pri_protocol == PRI_PROTO_E1) {
			/* In E1 - Only 0'th channel is unused */
			if (i == 0)
				continue;
		} else if (priv->pri_protocol == PRI_PROTO_T1) {
			/* In T1 - Every 4'th channel is unused */
			if ((i % 4) == 0)
				continue;
		}
		if (logical_chan == PRI_DCHAN_IDX(priv) && !priv->is_cas) {
			if (priv->dchan_rx_sample != pcm[0]) {
				if (debug & DBG_PCM) {
					XPD_INFO(xpd,
						"RX-D-Chan: prev=0x%X now=0x%X\n",
						priv->dchan_rx_sample, pcm[0]);
					dump_packet("RX-D-Chan", pack, 1);
				}
				priv->dchan_rx_sample = pcm[0];
				priv->dchan_rx_counter++;
			} else if (pcm[0] == 0xFF)
				dchan_state(xpd, 0);
		}
		if (IS_SET(physical_mask, i)) {
			struct dahdi_chan *chan = XPD_CHAN(xpd, logical_chan);
			r = chan->readchunk;
			// memset((u_char *)r, 0x5A, DAHDI_CHUNKSIZE);  // DEBUG
			memcpy((u_char *)r, pcm, DAHDI_CHUNKSIZE);
			pcm += DAHDI_CHUNKSIZE;
		}
		logical_chan++;
	}
	XPD_COUNTER(xpd, PCM_READ)++;
	spin_unlock_irqrestore(&xpd->lock, flags);
}

static int PRI_timing_priority(xpd_t *xpd)
{
	struct PRI_priv_data *priv;

	priv = xpd->priv;
	BUG_ON(!priv);
	if (priv->layer1_up)
		return PHONEDEV(xpd).timing_priority;
	XPD_DBG(SYNC, xpd, "No timing priority (no layer1)\n");
	return -ENOENT;
}

static int PRI_echocancel_timeslot(xpd_t *xpd, int pos)
{
	/*
	 * Skip ts=0 (used for PRI sync)
	 */
	return (1 + pos) * 4 + xpd->addr.subunit;
}

static int PRI_echocancel_setmask(xpd_t *xpd, xpp_line_t ec_mask)
{
	struct PRI_priv_data *priv;
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
	for (i = 0; i < PHONEDEV(xpd).channels; i++) {
		int on = BIT(i) & ec_mask;

		if (i == PRI_DCHAN_IDX(priv))
			on = 0;
		CALL_EC_METHOD(ec_set, xpd->xbus, xpd, i, on);
	}
	CALL_EC_METHOD(ec_update, xpd->xbus, xpd->xbus);
	return 0;
}

/*---------------- PRI: HOST COMMANDS -------------------------------------*/

static /* 0x33 */ HOSTCMD(PRI, SET_LED, enum pri_led_selectors led_sel,
			  enum pri_led_state to_led_state)
{
	int ret = 0;
	xframe_t *xframe;
	xpacket_t *pack;
	struct pri_leds *pri_leds;
	struct PRI_priv_data *priv;

	BUG_ON(!xbus);
	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	XPD_DBG(LEDS, xpd, "led_sel=%d to_state=%d\n", led_sel, to_led_state);
	XFRAME_NEW_CMD(xframe, pack, xbus, PRI, SET_LED, xpd->xbus_idx);
	pri_leds = &RPACKET_FIELD(pack, PRI, SET_LED, pri_leds);
	pri_leds->state = to_led_state;
	pri_leds->led_sel = led_sel;
	pri_leds->reserved = 0;
	XPACKET_LEN(pack) = RPACKET_SIZE(PRI, SET_LED);
	ret = send_cmd_frame(xbus, xframe);
	priv->ledstate[led_sel] = to_led_state;
	return ret;
}

/*---------------- PRI: Astribank Reply Handlers --------------------------*/
static void layer1_state(xpd_t *xpd, __u8 data_low)
{
	struct PRI_priv_data *priv;
	int alarms = DAHDI_ALARM_NONE;
	int layer1_up_prev;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	priv->poll_noreplies = 0;
	if (data_low & REG_FRS0_LOS)
		alarms |= DAHDI_ALARM_RED;
	if (data_low & REG_FRS0_AIS)
		alarms |= DAHDI_ALARM_BLUE;
	if (data_low & REG_FRS0_RRA)
		alarms |= DAHDI_ALARM_YELLOW;
	layer1_up_prev = priv->layer1_up;
	priv->layer1_up = alarms == DAHDI_ALARM_NONE;
#if 0
	/*
	 * Some bad bits (e.g: LMFA and NMF have no alarm "colors"
	 * associated. However, layer1 is still not working if they are set.
	 * FIXME: These behave differently in E1/T1, so ignore them for while.
	 */
	if (data_low & (REG_FRS0_LMFA | REG_FRS0_E1_NMF))
		priv->layer1_up = 0;
#endif
	priv->alarms = alarms;
	if (!priv->layer1_up) {
		dchan_state(xpd, 0);
	} else if (priv->is_cas && !layer1_up_prev) {
		int regbase = cas_regbase(xpd);
		int i;

		XPD_DBG(SIGNAL, xpd,
			"Returning From Alarm Refreshing Rx register data \n");
		for (i = 0; i < cas_numregs(xpd); i++)
			query_subunit(xpd, regbase + i);
	}

	if (SPAN_REGISTERED(xpd) && PHONEDEV(xpd).span.alarms != alarms) {
		char str1[MAX_PROC_WRITE];
		char str2[MAX_PROC_WRITE];

		alarm2str(PHONEDEV(xpd).span.alarms, str1, sizeof(str1));
		alarm2str(alarms, str2, sizeof(str2));
		XPD_NOTICE(xpd, "Alarms: 0x%X (%s) => 0x%X (%s)\n",
			   PHONEDEV(xpd).span.alarms, str1, alarms, str2);
		if (priv->is_cas) {
			if (alarms == DAHDI_ALARM_NONE)
				send_oldbits(xpd);
			else if (PHONEDEV(xpd).span.alarms == DAHDI_ALARM_NONE)
				send_idlebits(xpd, 1);
		}
		PHONEDEV(xpd).span.alarms = alarms;
		elect_syncer("LAYER1");
		dahdi_alarm_notify(&PHONEDEV(xpd).span);
		set_clocking(xpd);
	}
	priv->reg_frs0 = data_low;
	priv->layer1_replies++;
	XPD_DBG(REGS, xpd, "subunit=%d data_low=0x%02X\n", xpd->addr.subunit,
		data_low);
}

static int decode_cas_e1(xpd_t *xpd, __u8 regnum, __u8 data_low)
{
	struct PRI_priv_data *priv;
	uint pos = regnum - REG_RS2_E;
	int rsnum = pos + 2;
	int chan1 = pos;
	int chan2 = pos + 16;
	int val1 = (data_low >> 4) & 0xF;
	int val2 = data_low & 0xF;

	priv = xpd->priv;
	BUG_ON(!priv->is_cas);
	BUG_ON(priv->pri_protocol != PRI_PROTO_E1);
	XPD_DBG(SIGNAL, xpd, "RBS: RX: data_low=0x%02X\n", data_low);
	if (pos >= NUM_CAS_RS_E) {
		XPD_ERR(xpd, "%s: got bad pos=%d [0-%d]\n", __func__, pos,
			NUM_CAS_RS_E);
		return -EINVAL;
	}
	if (chan1 < 0 || chan1 > PHONEDEV(xpd).channels) {
		XPD_NOTICE(xpd, "%s: %s CAS: Bad chan1 number (%d)\n", __func__,
			   pri_protocol_name(priv->pri_protocol), chan1);
		return -EINVAL;
	}
	if (chan2 < 0 || chan2 > PHONEDEV(xpd).channels) {
		XPD_NOTICE(xpd, "%s: %s CAS: Bad chan2 number (%d)\n", __func__,
			   pri_protocol_name(priv->pri_protocol), chan2);
		return -EINVAL;
	}
	XPD_DBG(SIGNAL, xpd,
		"RBS: RX: RS%02d (channel %2d, channel %2d): 0x%02X -> 0x%02X\n",
		rsnum, chan1 + 1, chan2 + 1, priv->cas_rs_e[pos], data_low);
	if (SPAN_REGISTERED(xpd)) {
		dahdi_rbsbits(XPD_CHAN(xpd, chan1), val1);
		dahdi_rbsbits(XPD_CHAN(xpd, chan2), val2);
	}
	priv->dchan_rx_counter++;
	priv->cas_rs_e[pos] = data_low;
	return 0;
}

static int decode_cas_t1(xpd_t *xpd, __u8 regnum, __u8 data_low)
{
	struct PRI_priv_data *priv;
	uint rsnum;
	uint chan_per_reg;
	uint width;
	int i;

	priv = xpd->priv;
	BUG_ON(!priv->is_cas);
	BUG_ON(priv->pri_protocol != PRI_PROTO_T1);
	rsnum = regnum - REG_RS1_E;
	if (rsnum >= 12) {
		XPD_ERR(xpd, "Bad rsnum=%d\n", rsnum);
		return 0;
	}
	if (!priv->is_esf)
		rsnum = rsnum % 6;	/* 2 identical banks of 6 registers */
	chan_per_reg = CHAN_PER_REGS(priv);
	width = 8 / chan_per_reg;
	XPD_DBG(SIGNAL, xpd, "RBS: RX(%s,%d): RS%02d data_low=0x%02X\n",
		(priv->is_esf) ? "esf" : "d4", chan_per_reg, rsnum + 1,
		data_low);
	for (i = 0; i < chan_per_reg; i++) {
		uint rxsig = (data_low >> (i * width)) & BITMASK(width);
		int pos;
		struct dahdi_chan *chan;

		if (!priv->is_esf)
			rxsig <<= 2;
		pos = rsnum * chan_per_reg + chan_per_reg - i - 1;
		if (pos < 0 || pos >= PHONEDEV(xpd).channels) {
			XPD_ERR(xpd, "%s: Bad pos=%d\n", __func__, pos);
			continue;
		}
		chan = XPD_CHAN(xpd, pos);
		if (!chan) {
			XPD_ERR(xpd, "%s: Null channel in pos=%d\n", __func__,
				pos);
			continue;
		}
		if (chan->rxsig != rxsig) {
			LINE_DBG(SIGNAL, xpd, pos, "i=%d rxsig=0x%02X\n", i,
				 rxsig);
			dahdi_rbsbits(chan, rxsig);
		}
	}
	priv->cas_rs_e[rsnum] = data_low;
	return 0;
}

static void process_cas_dchan(xpd_t *xpd, __u8 regnum, __u8 data_low)
{
	struct PRI_priv_data *priv;

	priv = xpd->priv;
	if (!priv->is_cas) {
		static int rate_limit;

		if ((rate_limit++ % 10003) == 0)
			XPD_NOTICE(xpd, "RBS: RX: not in CAS mode. Ignore.\n");
		return;
	}
	if (!priv->layer1_up) {
		static int rate_limit;

		if ((rate_limit++ % 10003) == 0)
			XPD_DBG(SIGNAL, xpd, "RBS: RX: No layer1.\n");
	}
	if (!SPAN_REGISTERED(xpd)) {
		static int rate_limit;

		if ((rate_limit++ % 10003) == 0)
			XPD_DBG(SIGNAL, xpd,
				"RBS: RX: Span not registered. Ignore.\n");
		return;
	}
	if (priv->pri_protocol == PRI_PROTO_E1) {
		if (regnum == REG_RS1_E)
			return;	/* Time slot 0: Ignored for E1 */
		if (regnum < REG_RS2_E) {
			/* Should not happen, but harmless. Ignore */
			if (regnum == REG_RS1_E)
				return;

			XPD_NOTICE(xpd,
				"%s: received register 0x%X in protocol %s. "
				"Ignore.\n",
				__func__, regnum,
				pri_protocol_name(priv->pri_protocol));
			return;
		}
		if (decode_cas_e1(xpd, regnum, data_low) < 0)
			return;
	} else if (priv->pri_protocol == PRI_PROTO_T1) {
		if (regnum > REG_RS12_E) {
			XPD_NOTICE(xpd,
				"%s: received register 0x%X in protocol %s. "
				"Ignore.\n",
				__func__, regnum,
				   pri_protocol_name(priv->pri_protocol));
			return;
		}
		if (decode_cas_t1(xpd, regnum, data_low) < 0)
			return;
	} else {
		XPD_NOTICE(xpd,
			   "%s: protocol %s is not supported yet with CAS\n",
			   __func__, pri_protocol_name(priv->pri_protocol));
	}
	priv->cas_replies++;
}

static int PRI_card_register_reply(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *info)
{
	unsigned long flags;
	struct PRI_priv_data *priv;
	struct xpd_addr addr;
	xpd_t *orig_xpd;
	__u8 regnum;
	__u8 data_low;

	/* Map UNIT + PORTNUM to XPD */
	orig_xpd = xpd;
	addr.unit = orig_xpd->addr.unit;
	addr.subunit = info->portnum;
	regnum = REG_FIELD(info, regnum);
	data_low = REG_FIELD(info, data_low);
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
	if (info->is_multibyte) {
		XPD_NOTICE(xpd, "Got Multibyte: %d bytes, eoframe: %d\n",
			   info->bytes, info->eoframe);
		goto end;
	}
	if (regnum == REG_FRS0 && !REG_FIELD(info, do_subreg))
		layer1_state(xpd, data_low);
	else if (regnum == REG_FRS1 && !REG_FIELD(info, do_subreg))
		priv->reg_frs1 = data_low;
	if (priv->is_cas && !REG_FIELD(info, do_subreg)) {
		if (regnum >= REG_RS1_E && regnum <= REG_RS16_E)
			process_cas_dchan(xpd, regnum, data_low);
	}
	/*
	 * Update /proc info only if reply relate to the
	 * last slic read request
	 */
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

static int PRI_card_state(xpd_t *xpd, bool on)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "%s\n", (on) ? "on" : "off");
	return 0;
}

static const struct xops pri_xops = {
	.card_new = PRI_card_new,
	.card_init = PRI_card_init,
	.card_remove = PRI_card_remove,
	.card_tick = PRI_card_tick,
	.card_register_reply = PRI_card_register_reply,
};

static const struct phoneops pri_phoneops = {
	.card_dahdi_preregistration = PRI_card_dahdi_preregistration,
	.card_dahdi_postregistration = PRI_card_dahdi_postregistration,
	.card_pcm_recompute = PRI_card_pcm_recompute,
	.card_pcm_fromspan = PRI_card_pcm_fromspan,
	.card_pcm_tospan = PRI_card_pcm_tospan,
	.echocancel_timeslot = PRI_echocancel_timeslot,
	.echocancel_setmask = PRI_echocancel_setmask,
	.card_timing_priority = PRI_timing_priority,
	.card_ioctl = PRI_card_ioctl,
	.card_open = PRI_card_open,
	.card_close = PRI_card_close,
	.card_state = PRI_card_state,
};

static xproto_table_t PROTO_TABLE(PRI) = {
	.owner = THIS_MODULE,
	.entries = {
		/*      Table   Card    Opcode          */
	},
	.name = "PRI",	/* protocol name */
	.ports_per_subunit = 1,
	.type = XPD_TYPE_PRI,
	.xops = &pri_xops,
	.phoneops = &pri_phoneops,
	.packet_is_valid = pri_packet_is_valid,
	.packet_dump = pri_packet_dump,
};

static bool pri_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe = NULL;
	// DBG(GENERAL, "\n");
	xe = xproto_card_entry(&PROTO_TABLE(PRI), XPACKET_OP(pack));
	return xe != NULL;
}

static void pri_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

/*------------------------- REGISTER Handling --------------------------*/

/*------------------------- sysfs stuff --------------------------------*/
static DEVICE_ATTR_READER(pri_protocol_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	len += sprintf(buf, "%s\n", pri_protocol_name(priv->pri_protocol));
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(pri_protocol_store, dev, buf, count)
{
	xpd_t *xpd;
	enum pri_protocol new_protocol = PRI_PROTO_0;
	int i;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	XPD_DBG(GENERAL, xpd, "%s\n", buf);
	if (!xpd)
		return -ENODEV;
	i = strcspn(buf, " \r\n");
	if (i != 2) {
		XPD_NOTICE(xpd,
			"Protocol name '%s' has %d characters (should be 2). "
			"Ignored.\n",
			buf, i);
		return -EINVAL;
	}
	if (strncasecmp(buf, "E1", 2) == 0)
		new_protocol = PRI_PROTO_E1;
	else if (strncasecmp(buf, "T1", 2) == 0)
		new_protocol = PRI_PROTO_T1;
	else if (strncasecmp(buf, "J1", 2) == 0)
		new_protocol = PRI_PROTO_J1;
	else {
		XPD_NOTICE(xpd,
			"Unknown PRI protocol '%s' (should be E1|T1|J1). "
			"Ignored.\n",
			buf);
		return -EINVAL;
	}
	ret = set_pri_proto(xpd, new_protocol);
	return (ret < 0) ? ret : count;
}

static DEVICE_ATTR(pri_protocol, S_IRUGO | S_IWUSR, pri_protocol_show,
		   pri_protocol_store);

static DEVICE_ATTR_READER(pri_localloop_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	len += sprintf(buf, "%c\n", (priv->local_loopback) ? 'Y' : 'N');
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(pri_localloop_store, dev, buf, count)
{
	xpd_t *xpd;
	bool ll = 0;
	int i;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	XPD_DBG(GENERAL, xpd, "%s\n", buf);
	if (!xpd)
		return -ENODEV;
	if ((i = strcspn(buf, " \r\n")) != 1) {
		XPD_NOTICE(xpd,
			"Value '%s' has %d characters (should be 1). Ignored\n",
			buf, i);
		return -EINVAL;
	}
	if (strchr("1Yy", buf[0]) != NULL)
		ll = 1;
	else if (strchr("0Nn", buf[0]) != NULL)
		ll = 0;
	else {
		XPD_NOTICE(xpd,
			"Unknown value '%s' (should be [1Yy]|[0Nn]). Ignored\n",
			buf);
		return -EINVAL;
	}
	ret = set_localloop(xpd, ll);
	return (ret < 0) ? ret : count;
}

static DEVICE_ATTR(pri_localloop, S_IRUGO | S_IWUSR, pri_localloop_show,
		   pri_localloop_store);

static DEVICE_ATTR_READER(pri_layer1_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	if (priv->poll_noreplies > 1)
		len += sprintf(buf + len, "Unknown[%d]", priv->poll_noreplies);
	else
		len +=
		    sprintf(buf + len, "%-10s",
			    ((priv->layer1_up) ? "UP" : "DOWN"));
	len += sprintf(buf + len, "%d\n", priv->layer1_replies);
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(pri_layer1, S_IRUGO, pri_layer1_show, NULL);

static DEVICE_ATTR_READER(pri_alarms_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;
	static const struct {
		__u8 bits;
		const char *name;
	} alarm_types[] = {
		{
		REG_FRS0_LOS, "RED"}, {
		REG_FRS0_AIS, "BLUE"}, {
	REG_FRS0_RRA, "YELLOW"},};

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	if (priv->poll_noreplies > 1)
		len += sprintf(buf + len, "Unknown[%d]", priv->poll_noreplies);
	else {
		int i;

		for (i = 0; i < ARRAY_SIZE(alarm_types); i++) {
			if (priv->reg_frs0 & alarm_types[i].bits)
				len +=
				    sprintf(buf + len, "%s ",
					    alarm_types[i].name);
		}
	}
	len += sprintf(buf + len, "\n");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(pri_alarms, S_IRUGO, pri_alarms_show, NULL);

static DEVICE_ATTR_READER(pri_cas_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	if (priv->is_cas) {
		int i;

		len +=
		    sprintf(buf + len, "CAS: replies=%d\n", priv->cas_replies);
		len += sprintf(buf + len, "   CAS-TS: ");
		for (i = 0; i < NUM_CAS_RS_E; i++)
			len += sprintf(buf + len, " %02X", priv->cas_ts_e[i]);
		len += sprintf(buf + len, "\n");
		len += sprintf(buf + len, "   CAS-RS: ");
		for (i = 0; i < NUM_CAS_RS_E; i++)
			len += sprintf(buf + len, " %02X", priv->cas_rs_e[i]);
		len += sprintf(buf + len, "\n");
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(pri_cas, S_IRUGO, pri_cas_show, NULL);

static DEVICE_ATTR_READER(pri_dchan_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	len +=
	    sprintf(buf + len,
		    "D-Channel: TX=[%5d] (0x%02X)   RX=[%5d] (0x%02X) ",
		    priv->dchan_tx_counter, priv->dchan_tx_sample,
		    priv->dchan_rx_counter, priv->dchan_rx_sample);
	if (priv->dchan_alive) {
		len +=
		    sprintf(buf + len, "(alive %d K-ticks)\n",
			    priv->dchan_alive_ticks / 1000);
	} else {
		len += sprintf(buf + len, "(dead)\n");
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(pri_dchan, S_IRUGO, pri_dchan_show, NULL);

static DEVICE_ATTR_READER(pri_clocking_show, dev, buf)
{
	xpd_t *xpd;
	struct PRI_priv_data *priv;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	priv = xpd->priv;
	BUG_ON(!priv);
	spin_lock_irqsave(&xpd->lock, flags);
	len +=
	    sprintf(buf + len, "%s\n",
		    (priv->clock_source) ? "MASTER" : "SLAVE");
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR(pri_clocking, S_IRUGO, pri_clocking_show, NULL);

static int pri_xpd_probe(struct device *dev)
{
	xpd_t *xpd;
	int ret = 0;

	xpd = dev_to_xpd(dev);
	/* Is it our device? */
	if (xpd->type != XPD_TYPE_PRI) {
		XPD_ERR(xpd, "drop suggestion for %s (%d)\n", dev_name(dev),
			xpd->type);
		return -EINVAL;
	}
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	ret = device_create_file(dev, &dev_attr_pri_protocol);
	if (ret) {
		XPD_ERR(xpd,
			"%s: device_create_file(pri_protocol) failed: %d\n",
			__func__, ret);
		goto fail_pri_protocol;
	}
	ret = device_create_file(dev, &dev_attr_pri_localloop);
	if (ret) {
		XPD_ERR(xpd,
			"%s: device_create_file(pri_localloop) failed: %d\n",
			__func__, ret);
		goto fail_pri_localloop;
	}
	ret = device_create_file(dev, &dev_attr_pri_layer1);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(pri_layer1) failed: %d\n",
			__func__, ret);
		goto fail_pri_layer1;
	}
	ret = device_create_file(dev, &dev_attr_pri_alarms);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(pri_alarms) failed: %d\n",
			__func__, ret);
		goto fail_pri_alarms;
	}
	ret = device_create_file(dev, &dev_attr_pri_cas);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(pri_cas) failed: %d\n",
			__func__, ret);
		goto fail_pri_cas;
	}
	ret = device_create_file(dev, &dev_attr_pri_dchan);
	if (ret) {
		XPD_ERR(xpd, "%s: device_create_file(pri_dchan) failed: %d\n",
			__func__, ret);
		goto fail_pri_dchan;
	}
	ret = device_create_file(dev, &dev_attr_pri_clocking);
	if (ret) {
		XPD_ERR(xpd,
			"%s: device_create_file(pri_clocking) failed: %d\n",
			__func__, ret);
		goto fail_pri_clocking;
	}
	return 0;
fail_pri_clocking:
	device_remove_file(dev, &dev_attr_pri_dchan);
fail_pri_dchan:
	device_remove_file(dev, &dev_attr_pri_cas);
fail_pri_cas:
	device_remove_file(dev, &dev_attr_pri_alarms);
fail_pri_alarms:
	device_remove_file(dev, &dev_attr_pri_layer1);
fail_pri_layer1:
	device_remove_file(dev, &dev_attr_pri_localloop);
fail_pri_localloop:
	device_remove_file(dev, &dev_attr_pri_protocol);
fail_pri_protocol:
	return ret;
}

static int pri_xpd_remove(struct device *dev)
{
	xpd_t *xpd;

	xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	device_remove_file(dev, &dev_attr_pri_clocking);
	device_remove_file(dev, &dev_attr_pri_dchan);
	device_remove_file(dev, &dev_attr_pri_cas);
	device_remove_file(dev, &dev_attr_pri_alarms);
	device_remove_file(dev, &dev_attr_pri_layer1);
	device_remove_file(dev, &dev_attr_pri_localloop);
	device_remove_file(dev, &dev_attr_pri_protocol);
	return 0;
}

static struct xpd_driver pri_driver = {
	.type = XPD_TYPE_PRI,
	.driver = {
		   .name = "pri",
		   .owner = THIS_MODULE,
		   .probe = pri_xpd_probe,
		   .remove = pri_xpd_remove}
};

static int __init card_pri_startup(void)
{
	int ret;

	if ((ret = xpd_driver_register(&pri_driver.driver)) < 0)
		return ret;
	INFO("revision %s\n", XPP_VERSION);
#ifdef	DAHDI_AUDIO_NOTIFY
	INFO("FEATURE: WITH DAHDI_AUDIO_NOTIFY\n");
#else
	INFO("FEATURE: WITHOUT DAHDI_AUDIO_NOTIFY\n");
#endif
	xproto_register(&PROTO_TABLE(PRI));
	return 0;
}

static void __exit card_pri_cleanup(void)
{
	DBG(GENERAL, "\n");
	xproto_unregister(&PROTO_TABLE(PRI));
	xpd_driver_unregister(&pri_driver.driver);
}

MODULE_DESCRIPTION("XPP PRI Card Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_VERSION(XPP_VERSION);
MODULE_ALIAS_XPD(XPD_TYPE_PRI);

module_init(card_pri_startup);
module_exit(card_pri_cleanup);
