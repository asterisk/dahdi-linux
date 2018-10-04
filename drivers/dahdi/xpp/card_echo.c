/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2011, Xorcom
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
#include "card_echo.h"
#include "xpp_dahdi.h"
#include "dahdi_debug.h"
#include "xpd.h"
#include "xbus-core.h"

static const char rcsid[] = "$Id$";

/* must be before dahdi_debug.h: */
static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");

/*---------------- ECHO Protocol Commands ----------------------------------*/

static bool echo_packet_is_valid(xpacket_t *pack);
static void echo_packet_dump(const char *msg, xpacket_t *pack);

DEF_RPACKET_DATA(ECHO, SET, __u8 timeslots[ECHO_TIMESLOTS];);

DEF_RPACKET_DATA(ECHO, SET_REPLY, __u8 status; __u8 reserved;);

struct ECHO_priv_data {
};

static xproto_table_t PROTO_TABLE(ECHO);

/*---------------- ECHO: Methods -------------------------------------------*/

static xpd_t *ECHO_card_new(xbus_t *xbus, int unit, int subunit,
			    const xproto_table_t *proto_table,
			    const struct unit_descriptor *unit_descriptor,
			    bool to_phone)
{
	xpd_t *xpd = NULL;
	int channels = 0;

	if (unit_descriptor->ports_per_chip != 1) {
		XBUS_ERR(xbus, "Bad subunit_ports=%d\n", unit_descriptor->ports_per_chip);
		return NULL;
	}
	XBUS_DBG(GENERAL, xbus, "\n");
	xpd =
	    xpd_alloc(xbus, unit, subunit,
		      sizeof(struct ECHO_priv_data), proto_table, unit_descriptor, channels);
	if (!xpd)
		return NULL;
	xpd->type_name = "ECHO";
	return xpd;
}

static int ECHO_card_init(xbus_t *xbus, xpd_t *xpd)
{
	int ret = 0;

	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	xpd->xpd_type = XPD_TYPE_ECHO;
	XPD_DBG(DEVICES, xpd, "%s\n", xpd->type_name);
	ret = CALL_EC_METHOD(ec_update, xbus, xbus);
	return ret;
}

static int ECHO_card_remove(xbus_t *xbus, xpd_t *xpd)
{
	BUG_ON(!xpd);
	XPD_DBG(GENERAL, xpd, "\n");
	return 0;
}

static int ECHO_card_tick(xbus_t *xbus, xpd_t *xpd)
{
	struct ECHO_priv_data *priv;

	BUG_ON(!xpd);
	priv = xpd->priv;
	BUG_ON(!priv);
	return 0;
}

static int ECHO_card_register_reply(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *info)
{
	unsigned long flags;
	struct xpd_addr addr;
	xpd_t *orig_xpd;

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
	/* Update /proc info only if reply related to last reg read request */
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

/*---------------- ECHO: HOST COMMANDS -------------------------------------*/

static /* 0x39 */ HOSTCMD(ECHO, SET)
{
	struct xbus_echo_state *es;
	__u8 *ts;
	xframe_t *xframe;
	xpacket_t *pack;
	int ret;
	uint16_t frm_len;
	int xpd_idx;

	BUG_ON(!xbus);
	/*
	 * Find echo canceller XPD address
	 */
	es = &xbus->echo_state;
	xpd_idx = es->xpd_idx;
	XFRAME_NEW_CMD(xframe, pack, xbus, ECHO, SET, xpd_idx);
	ts = RPACKET_FIELD(pack, ECHO, SET, timeslots);
	memcpy(ts, es->timeslots, ECHO_TIMESLOTS);
	frm_len = XFRAME_LEN(xframe);
	XBUS_DBG(GENERAL, xbus, "ECHO SET: (len = %d)\n", frm_len);
	ret = send_cmd_frame(xbus, xframe);
	return ret;
}

static int ECHO_ec_set(xpd_t *xpd, int pos, bool on)
{
	int ts_number;
	int ts_mask;
	__u8 *ts;

	ts = xpd->xbus->echo_state.timeslots;
	/*
	 * ts_number = PCM time slot ("channel number" in the PCM XPP packet)
	 *
	 * Bit 0 is for UNIT=0
	 * PRI: ts_number * 4 + SUBUNIT
	 * BRI: ts_number
	 * FXS/FXO(all units): UNIT * 32 + ts_number
	 *
	 * Bit 1 is for UNIT=1-3: FXS/FXO
	 *
	 */
	ts_mask = (xpd->addr.unit == 0) ? 0x1 : 0x2;	/* Which bit? */
	ts_number = CALL_PHONE_METHOD(echocancel_timeslot, xpd, pos);
	if (ts_number >= ECHO_TIMESLOTS || ts_number < 0) {
		XPD_ERR(xpd, "Bad ts_number=%d\n", ts_number);
		return -EINVAL;
	} else {
		if (on)
			ts[ts_number] |= ts_mask;
		else
			ts[ts_number] &= ~ts_mask;
	}
	LINE_DBG(GENERAL, xpd, pos, "%s = %d -- ts_number=%d ts_mask=0x%X\n",
		 __func__, on, ts_number, ts_mask);
	return 0;
}

static int ECHO_ec_get(xpd_t *xpd, int pos)
{
	int ts_number;
	int ts_mask;
	int is_on;
	__u8 *ts;

	ts = xpd->xbus->echo_state.timeslots;
	ts_mask = (xpd->addr.unit == 0) ? 0x1 : 0x2;	/* Which bit? */
	ts_number = CALL_PHONE_METHOD(echocancel_timeslot, xpd, pos);
	if (ts_number >= ECHO_TIMESLOTS || ts_number < 0) {
		XPD_ERR(xpd, "Bad ts_number=%d\n", ts_number);
		return -EINVAL;
	} else {
		is_on = ts[ts_number] & ts_mask;
	}
#if 0
	LINE_DBG(GENERAL, xpd, pos, "ec_get=%d -- ts_number=%d ts_mask=0x%X\n",
		 is_on, ts_number, ts_mask);
#endif
	return is_on;
}

static void ECHO_ec_dump(xbus_t *xbus)
{
	__u8 *ts;
	int i;

	ts = xbus->echo_state.timeslots;
	for (i = 0; i + 15 < ECHO_TIMESLOTS; i += 16) {
		XBUS_DBG(GENERAL, xbus,
			"EC-DUMP[%03d]: "
			"0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
			"0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			i, ts[i + 0], ts[i + 1], ts[i + 2], ts[i + 3],
			ts[i + 4], ts[i + 5], ts[i + 6], ts[i + 7], ts[i + 8],
			ts[i + 9], ts[i + 10], ts[i + 11], ts[i + 12],
			ts[i + 13], ts[i + 14], ts[i + 15]
		    );
	}
}

static int ECHO_ec_update(xbus_t *xbus)
{
	XBUS_DBG(GENERAL, xbus, "%s\n", __func__);
	//ECHO_ec_dump(xbus);
	return CALL_PROTO(ECHO, SET, xbus, NULL);
}

/*---------------- ECHO: Astribank Reply Handlers --------------------------*/
HANDLER_DEF(ECHO, SET_REPLY)
{
	__u8 status;

	BUG_ON(!xpd);
	status = RPACKET_FIELD(pack, ECHO, SET_REPLY, status);
	XPD_DBG(GENERAL, xpd, "status=0x%X\n", status);
	return 0;
}

static const struct xops echo_xops = {
	.card_new = ECHO_card_new,
	.card_init = ECHO_card_init,
	.card_remove = ECHO_card_remove,
	.card_tick = ECHO_card_tick,
	.card_register_reply = ECHO_card_register_reply,
};

static const struct echoops echoops = {
	.ec_set = ECHO_ec_set,
	.ec_get = ECHO_ec_get,
	.ec_update = ECHO_ec_update,
	.ec_dump = ECHO_ec_dump,
};

static xproto_table_t PROTO_TABLE(ECHO) = {
	.owner = THIS_MODULE,
	.entries = {
		/*      Table   Card    Opcode          */
		XENTRY(	ECHO,	ECHO,	SET_REPLY	),
	},
	.name = "ECHO",
	.ports_per_subunit = 1,
	.type = XPD_TYPE_ECHO,
	.xops = &echo_xops,
	.echoops = &echoops,
	.packet_is_valid = echo_packet_is_valid,
	.packet_dump = echo_packet_dump,
};

static bool echo_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe = NULL;
	// DBG(GENERAL, "\n");
	xe = xproto_card_entry(&PROTO_TABLE(ECHO), XPACKET_OP(pack));
	return xe != NULL;
}

static void echo_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

/*------------------------- sysfs stuff --------------------------------*/
static int echo_xpd_probe(struct device *dev)
{
	xpd_t *ec_xpd;
	int ret = 0;

	ec_xpd = dev_to_xpd(dev);
	/* Is it our device? */
	if (ec_xpd->xpd_type != XPD_TYPE_ECHO) {
		XPD_ERR(ec_xpd, "drop suggestion for %s (%d)\n", dev_name(dev),
			ec_xpd->xpd_type);
		return -EINVAL;
	}
	XPD_DBG(DEVICES, ec_xpd, "SYSFS\n");
	return ret;
}

static int echo_xpd_remove(struct device *dev)
{
	xpd_t *ec_xpd;

	ec_xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, ec_xpd, "SYSFS\n");
	return 0;
}

static struct xpd_driver echo_driver = {
	.xpd_type = XPD_TYPE_ECHO,
	.driver = {
		   .name = "echo",
		   .owner = THIS_MODULE,
		   .probe = echo_xpd_probe,
		   .remove = echo_xpd_remove}
};

static int __init card_echo_startup(void)
{
	int ret;

	ret = xpd_driver_register(&echo_driver.driver);
	if (ret < 0)
		return ret;
	INFO("FEATURE: WITH Octasic echo canceller\n");
	xproto_register(&PROTO_TABLE(ECHO));
	return 0;
}

static void __exit card_echo_cleanup(void)
{
	DBG(GENERAL, "\n");
	xproto_unregister(&PROTO_TABLE(ECHO));
	xpd_driver_unregister(&echo_driver.driver);
}

MODULE_DESCRIPTION("XPP ECHO Card Driver");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_XPD(XPD_TYPE_ECHO);

module_init(card_echo_startup);
module_exit(card_echo_cleanup);
