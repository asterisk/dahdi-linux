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

#include "xpd.h"
#include "xproto.h"
#include "xpp_dahdi.h"
#include "xbus-core.h"
#include "dahdi_debug.h"
#include <linux/module.h>
#include <linux/delay.h>

static const char rcsid[] = "$Id$";

extern int debug;

static const xproto_table_t *xprotocol_tables[XPD_TYPE_NOMODULE];

#if MAX_UNIT*MAX_SUBUNIT > MAX_XPDS
#error MAX_XPDS is too small
#endif

bool valid_xpd_addr(const struct xpd_addr *addr)
{
	return ((addr->subunit & ~BITMASK(SUBUNIT_BITS)) == 0)
	    && ((addr->unit & ~BITMASK(UNIT_BITS)) == 0);
}
EXPORT_SYMBOL(valid_xpd_addr);

/*------ General Protocol Management ----------------------------*/

const xproto_entry_t *xproto_card_entry(const xproto_table_t *table,
					__u8 opcode)
{
	const xproto_entry_t *xe;

	//DBG(GENERAL, "\n");
	xe = &table->entries[opcode];
	return (xe->handler != NULL) ? xe : NULL;
}
EXPORT_SYMBOL(xproto_card_entry);

const xproto_entry_t *xproto_global_entry(__u8 opcode)
{
	const xproto_entry_t *xe;

	xe = xproto_card_entry(&PROTO_TABLE(GLOBAL), opcode);
	//DBG(GENERAL, "opcode=0x%X xe=%p\n", opcode, xe);
	return xe;
}
EXPORT_SYMBOL(xproto_global_entry);

xproto_handler_t xproto_global_handler(__u8 opcode)
{
	return xproto_card_handler(&PROTO_TABLE(GLOBAL), opcode);
}

static const xproto_table_t *xproto_table(xpd_type_t cardtype)
{
	if (cardtype >= XPD_TYPE_NOMODULE)
		return NULL;
	return xprotocol_tables[cardtype];
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define MODULE_REFCOUNT_FORMAT "%s refcount was %d\n"
#else
#define MODULE_REFCOUNT_FORMAT "%s refcount was %lu\n"
#endif

const xproto_table_t *xproto_get(xpd_type_t cardtype)
{
	const xproto_table_t *xtable;

	if (cardtype >= XPD_TYPE_NOMODULE)
		return NULL;
	xtable = xprotocol_tables[cardtype];
	if (!xtable) {		/* Try to load the relevant module */
		int ret = request_module(XPD_TYPE_PREFIX "%d", cardtype);
		if (ret != 0) {
			NOTICE("%s: Failed to load module for type=%d. "
				"exit status=%d.\n",
				__func__, cardtype, ret);
			/* Drop through: we may be lucky... */
		}
		xtable = xprotocol_tables[cardtype];
	}
	if (xtable) {
		BUG_ON(!xtable->owner);
#ifdef CONFIG_MODULE_UNLOAD
		DBG(GENERAL, MODULE_REFCOUNT_FORMAT, xtable->name,
		    module_refcount(xtable->owner));
#endif
		if (!try_module_get(xtable->owner)) {
			ERR("%s: try_module_get for %s failed.\n", __func__,
			    xtable->name);
			return NULL;
		}
	}
	return xtable;
}

void xproto_put(const xproto_table_t *xtable)
{
	BUG_ON(!xtable);
#ifdef CONFIG_MODULE_UNLOAD
	DBG(GENERAL, MODULE_REFCOUNT_FORMAT, xtable->name,
	    module_refcount(xtable->owner));
	BUG_ON(module_refcount(xtable->owner) <= 0);
#endif
	module_put(xtable->owner);
}

xproto_handler_t xproto_card_handler(const xproto_table_t *table,
	__u8 opcode)
{
	const xproto_entry_t *xe;

	//DBG(GENERAL, "\n");
	xe = xproto_card_entry(table, opcode);
	return xe->handler;
}

void notify_bad_xpd(const char *funcname, xbus_t *xbus,
		    const struct xpd_addr addr, const char *msg)
{
	XBUS_NOTICE(xbus, "%s: non-existing address (%1d%1d): %s\n", funcname,
		    addr.unit, addr.subunit, msg);
}
EXPORT_SYMBOL(notify_bad_xpd);

static int packet_process(xbus_t *xbus, xpacket_t *pack)
{
	__u8 op;
	const xproto_entry_t *xe;
	xproto_handler_t handler;
	xproto_table_t *table;
	xpd_t *xpd;
	int ret = -EPROTO;

	BUG_ON(!pack);
	if (!valid_xpd_addr(&XPACKET_ADDR(pack))) {
		if (printk_ratelimit()) {
			XBUS_NOTICE(xbus, "%s: from %d%d: bad address.\n",
				    __func__, XPACKET_ADDR_UNIT(pack),
				    XPACKET_ADDR_SUBUNIT(pack));
			dump_packet("packet_process -- bad address", pack,
				    debug);
		}
		goto out;
	}
	op = XPACKET_OP(pack);
	xpd =
	    xpd_byaddr(xbus, XPACKET_ADDR_UNIT(pack),
		       XPACKET_ADDR_SUBUNIT(pack));
	/* XPD may be NULL (e.g: during bus polling */
	xe = xproto_global_entry(op);
	/*-------- Validations -----------*/
	if (!xe) {
		const xproto_table_t *xtable;

		if (!xpd) {
			if (printk_ratelimit()) {
				XBUS_NOTICE(xbus,
					"%s: from %d%d opcode=0x%02X: "
					"no such global command.\n",
					__func__, XPACKET_ADDR_UNIT(pack),
					XPACKET_ADDR_SUBUNIT(pack), op);
				dump_packet
				    ("packet_process -- no such global command",
				     pack, 1);
			}
			goto out;
		}
		xtable = xproto_table(xpd->type);
		if (!xtable) {
			if (printk_ratelimit())
				XPD_ERR(xpd,
					"%s: no protocol table (type=%d)\n",
					__func__, xpd->type);
			goto out;
		}
		xe = xproto_card_entry(xtable, op);
		if (!xe) {
			if (printk_ratelimit()) {
				XPD_NOTICE(xpd,
					"%s: bad command (type=%d,opcode=0x%x)\n",
					__func__, xpd->type, op);
				dump_packet("packet_process -- bad command",
					pack, 1);
			}
			goto out;
		}
	}
	table = xe->table;
	BUG_ON(!table);
	if (!table->packet_is_valid(pack)) {
		if (printk_ratelimit()) {
			ERR("xpp: %s: wrong size %d for opcode=0x%02X\n",
			    __func__, XPACKET_LEN(pack), op);
			dump_packet("packet_process -- wrong size", pack,
				    debug);
		}
		goto out;
	}
	ret = 0;		/* All well */
	handler = xe->handler;
	BUG_ON(!handler);
	XBUS_COUNTER(xbus, RX_BYTES) += XPACKET_LEN(pack);
	handler(xbus, xpd, xe, pack);
out:
	return ret;
}

static int xframe_receive_cmd(xbus_t *xbus, xframe_t *xframe)
{
	__u8 *xframe_end;
	xpacket_t *pack;
	__u8 *p;
	int len;
	int ret;

	if (debug & DBG_COMMANDS)
		dump_xframe("RX-CMD", xbus, xframe, DBG_ANY);
	p = xframe->packets;
	xframe_end = p + XFRAME_LEN(xframe);
	do {
		pack = (xpacket_t *)p;
		len = XPACKET_LEN(pack);
		/* Sanity checks */
		if (unlikely(XPACKET_OP(pack) == XPROTO_NAME(GLOBAL, PCM_READ))) {
			static int rate_limit;

			if ((rate_limit++ % 1003) == 0) {
				XBUS_DBG(GENERAL, xbus,
					"A PCM packet within a Non-PCM xframe\n");
				dump_xframe("In Non-PCM xframe",
					xbus, xframe, debug);
			}
			ret = -EPROTO;
			goto out;
		}
		p += len;
		if (p > xframe_end || len < RPACKET_HEADERSIZE) {
			static int rate_limit;

			if ((rate_limit++ % 1003) == 0) {
				XBUS_NOTICE(xbus, "Invalid packet length %d\n",
					    len);
				dump_xframe("BAD LENGTH", xbus, xframe, debug);
			}
			ret = -EPROTO;
			goto out;
		}
		ret = packet_process(xbus, pack);
		if (unlikely(ret < 0))
			break;
	} while (p < xframe_end);
out:
	FREE_RECV_XFRAME(xbus, xframe);
	return ret;
}

int xframe_receive(xbus_t *xbus, xframe_t *xframe)
{
	int ret = 0;
	struct timeval now;
	struct timeval tv_received;
	int usec;

	if (XFRAME_LEN(xframe) < RPACKET_HEADERSIZE) {
		static int rate_limit;

		if ((rate_limit++ % 1003) == 0) {
			XBUS_NOTICE(xbus, "short xframe\n");
			dump_xframe("short xframe", xbus, xframe, debug);
		}
		FREE_RECV_XFRAME(xbus, xframe);
		return -EPROTO;
	}
	if (!XBUS_FLAGS(xbus, CONNECTED)) {
		XBUS_DBG(GENERAL, xbus, "Dropped xframe. Is shutting down.\n");
		return -ENODEV;
	}
	tv_received = xframe->tv_received;
	/*
	 * We want to check that xframes do not mix PCM and other commands
	 */
	if (XPACKET_IS_PCM((xpacket_t *)xframe->packets)) {
		if (!XBUS_IS(xbus, READY))
			FREE_RECV_XFRAME(xbus, xframe);
		else
			xframe_receive_pcm(xbus, xframe);
	} else {
		XBUS_COUNTER(xbus, RX_CMD)++;
		ret = xframe_receive_cmd(xbus, xframe);
	}
	/* Calculate total processing time */
	do_gettimeofday(&now);
	usec =
	    (now.tv_sec - tv_received.tv_sec) * 1000000 + now.tv_usec -
	    tv_received.tv_usec;
	if (usec > xbus->max_rx_process)
		xbus->max_rx_process = usec;
	return ret;
}
EXPORT_SYMBOL(xframe_receive);

#define	VERBOSE_DEBUG		1
#define	ERR_REPORT_LIMIT	20

void dump_packet(const char *msg, const xpacket_t *packet, bool debug)
{
	__u8 op = XPACKET_OP(packet);
	__u8 *addr = (__u8 *)&XPACKET_ADDR(packet);

	if (!debug)
		return;
	printk(KERN_DEBUG "%s: XPD=%1X-%1X%c (0x%X) OP=0x%02X LEN=%d", msg,
	       XPACKET_ADDR_UNIT(packet), XPACKET_ADDR_SUBUNIT(packet),
	       (XPACKET_ADDR_SYNC(packet)) ? '+' : ' ', *addr, op,
	       XPACKET_LEN(packet));
#if VERBOSE_DEBUG
	{
		int i;
		__u8 *p = (__u8 *)packet;

		printk(" BYTES: ");
		for (i = 0; i < XPACKET_LEN(packet); i++) {
			static int limiter;

			if (i >= sizeof(xpacket_t)) {
				if (limiter < ERR_REPORT_LIMIT) {
					ERR("%s: length overflow "
						"i=%d > sizeof(xpacket_t)=%lu\n",
						__func__, i + 1,
						(long)sizeof(xpacket_t));
				} else if (limiter == ERR_REPORT_LIMIT) {
					ERR("%s: error packet #%d... "
						"squelsh reports.\n",
						__func__, limiter);
				}
				limiter++;
				break;
			}
			if (debug)
				printk("%02X ", p[i]);
		}
	}
#endif
	printk("\n");
}
EXPORT_SYMBOL(dump_packet);

void dump_reg_cmd(const char msg[], bool writing, xbus_t *xbus,
	__u8 unit, xportno_t port, const reg_cmd_t *regcmd)
{
	char action;
	char modifier;
	char port_buf[MAX_PROC_WRITE];
	char reg_buf[MAX_PROC_WRITE];
	char data_buf[MAX_PROC_WRITE];

	/* The size byte is not included */
	if (regcmd->bytes > sizeof(*regcmd) - 1) {
		PORT_NOTICE(xbus, unit, port,
			    "%s: %s: Too long: regcmd->bytes = %d\n", __func__,
			    msg, regcmd->bytes);
		return;
	}
	if (regcmd->is_multibyte) {
		char buf[MAX_PROC_WRITE + 1];
		int i;
		int n = 0;
		size_t len = regcmd->bytes;
		const __u8 *p = REG_XDATA(regcmd);

		buf[0] = '\0';
		for (i = 0; i < len && n < MAX_PROC_WRITE; i++)
			n += snprintf(&buf[n], MAX_PROC_WRITE - n, "%02X ",
				      p[i]);
		PORT_DBG(REGS, xbus, unit, port,
			"UNIT-%d PORT-%d: Multibyte(eoframe=%d) "
			"%s[0..%zd]: %s%s\n",
			unit, port, regcmd->eoframe, msg, len - 1, buf,
			(n >= MAX_PROC_WRITE) ? "..." : "");
		return;
	}
	/* The size byte is not included */
	if (regcmd->bytes != sizeof(*regcmd) - 1) {
		PORT_NOTICE(xbus, unit, port,
			    "%s: %s: Wrong size: regcmd->bytes = %d\n",
			    __func__, msg, regcmd->bytes);
		return;
	}
	snprintf(port_buf, MAX_PROC_WRITE, "%d%s", regcmd->portnum,
		 (REG_FIELD(regcmd, all_ports_broadcast)) ? "*" : "");
	action = (REG_FIELD(regcmd, read_request)) ? 'R' : 'W';
	modifier = 'D';
	if (REG_FIELD(regcmd, do_subreg)) {
		snprintf(reg_buf, MAX_PROC_WRITE, "%02X %02X",
			 REG_FIELD(regcmd, regnum), REG_FIELD(regcmd, subreg));
		modifier = 'S';
	} else {
		snprintf(reg_buf, MAX_PROC_WRITE, "%02X",
			 REG_FIELD(regcmd, regnum));
	}
	if (REG_FIELD(regcmd, read_request)) {
		data_buf[0] = '\0';
	} else if (REG_FIELD(regcmd, do_datah)) {
		snprintf(data_buf, MAX_PROC_WRITE, "%02X %02X",
			 REG_FIELD(regcmd, data_low), REG_FIELD(regcmd,
								data_high));
		modifier = 'I';
	} else {
		snprintf(data_buf, MAX_PROC_WRITE, "%02X",
			 REG_FIELD(regcmd, data_low));
	}
	PORT_DBG(REGS, xbus, unit, port, "%s: %s %c%c %s %s\n", msg, port_buf,
		 action, modifier, reg_buf, data_buf);
}
EXPORT_SYMBOL(dump_reg_cmd);

const char *xproto_name(xpd_type_t xpd_type)
{
	const xproto_table_t *proto_table;

	BUG_ON(xpd_type >= XPD_TYPE_NOMODULE);
	proto_table = xprotocol_tables[xpd_type];
	if (!proto_table)
		return NULL;
	return proto_table->name;
}
EXPORT_SYMBOL(xproto_name);

#define	CHECK_XOP(xops, f)	\
		if (!(xops)->f) { \
			ERR("%s: missing xmethod %s [%s (%d)]\n", \
				__func__, #f, name, type);	\
			return -EINVAL;	\
		}

#define	CHECK_PHONEOP(phoneops, f)	\
		if (!(phoneops)->f) { \
			ERR("%s: missing phone method %s [%s (%d)]\n", \
				__func__, #f, name, type);	\
			return -EINVAL;	\
		}

int xproto_register(const xproto_table_t *proto_table)
{
	int type;
	const char *name;
	const struct xops *xops;
	const struct phoneops *phoneops;

	BUG_ON(!proto_table);
	type = proto_table->type;
	name = proto_table->name;
	if (type >= XPD_TYPE_NOMODULE) {
		NOTICE("%s: Bad xproto type %d\n", __func__, type);
		return -EINVAL;
	}
	DBG(GENERAL, "%s (%d)\n", name, type);
	if (xprotocol_tables[type])
		NOTICE("%s: overriding registration of %s (%d)\n", __func__,
		       name, type);
	xops = proto_table->xops;
	CHECK_XOP(xops, card_new);
	CHECK_XOP(xops, card_init);
	CHECK_XOP(xops, card_remove);
	CHECK_XOP(xops, card_tick);
	CHECK_XOP(xops, card_register_reply);

	phoneops = proto_table->phoneops;
	if (phoneops) {
		CHECK_PHONEOP(phoneops, card_pcm_recompute);
		CHECK_PHONEOP(phoneops, card_pcm_fromspan);
		CHECK_PHONEOP(phoneops, card_pcm_tospan);
		CHECK_PHONEOP(phoneops, echocancel_timeslot);
		CHECK_PHONEOP(phoneops, echocancel_setmask);
		CHECK_PHONEOP(phoneops, card_dahdi_preregistration);
		CHECK_PHONEOP(phoneops, card_dahdi_postregistration);
		/* optional method -- call after testing: */
		/*CHECK_PHONEOP(phoneops, card_ioctl); */
	}

	xprotocol_tables[type] = proto_table;
	return 0;
}
EXPORT_SYMBOL(xproto_register);

void xproto_unregister(const xproto_table_t *proto_table)
{
	int type;
	const char *name;

	BUG_ON(!proto_table);
	type = proto_table->type;
	name = proto_table->name;
	DBG(GENERAL, "%s (%d)\n", name, type);
	if (type >= XPD_TYPE_NOMODULE) {
		NOTICE("%s: Bad xproto type %s (%d)\n", __func__, name, type);
		return;
	}
	if (!xprotocol_tables[type])
		NOTICE("%s: xproto type %s (%d) is already unregistered\n",
		       __func__, name, type);
	xprotocol_tables[type] = NULL;
}
EXPORT_SYMBOL(xproto_unregister);
