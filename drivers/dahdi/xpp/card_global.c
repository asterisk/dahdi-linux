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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kmod.h>
#include "xdefs.h"
#include "xpd.h"
#include "xpp_dahdi.h"
#include "xproto.h"
#include "dahdi_debug.h"
#include "xbus-core.h"
#include "parport_debug.h"

static const char rcsid[] = "$Id$";

DEF_PARM(charp, initdir, "/usr/share/dahdi", 0644,
	 "The directory of card initialization scripts");

#define	CHIP_REGISTERS	"chipregs"

extern int debug;

/*---------------- GLOBAL PROC handling -----------------------------------*/

static int send_magic_request(xbus_t *xbus, unsigned unit, xportno_t portno,
			      bool eoftx)
{
	xframe_t *xframe;
	xpacket_t *pack;
	reg_cmd_t *reg_cmd;
	int ret;

	/*
	 * Zero length multibyte is legal and has special meaning for the
	 * firmware:
	 *   eoftx==1: Start sending us D-channel packets.
	 *   eoftx==0: Stop sending us D-channel packets.
	 */
	XFRAME_NEW_CMD(xframe, pack, xbus, GLOBAL, REGISTER_REQUEST, unit);
	reg_cmd = &RPACKET_FIELD(pack, GLOBAL, REGISTER_REQUEST, reg_cmd);
	reg_cmd->bytes = 0;
	reg_cmd->is_multibyte = 1;
	reg_cmd->portnum = portno;
	reg_cmd->eoframe = eoftx;
	PORT_DBG(REGS, xbus, unit, portno, "Magic Packet (eoftx=%d)\n", eoftx);
	if (debug & DBG_REGS)
		dump_xframe(__func__, xbus, xframe, debug);
	ret = send_cmd_frame(xbus, xframe);
	if (ret < 0)
		PORT_ERR(xbus, unit, portno, "%s: failed sending xframe\n",
			 __func__);
	return ret;
}

static int parse_hexbyte(const char *buf)
{
	char *endp;
	unsigned val;

	val = simple_strtoul(buf, &endp, 16);
	if (*endp != '\0' || val > 0xFF)
		return -EBADR;
	return (__u8)val;
}

static int execute_chip_command(xpd_t *xpd, const int argc, char *argv[])
{
	int argno;
	char num_args;
	int portno;
	bool writing;
	int op;			/* [W]rite, [R]ead */
	int addr_mode;		/* [D]irect, [I]ndirect, [Mm]ulti */
	bool do_subreg = 0;
	int regnum;
	int subreg;
	int data_low;
	bool do_datah;
	int data_high;
	int ret = -EBADR;

	num_args = 2;		/* port + operation */
	if (argc < num_args) {
		XPD_ERR(xpd, "Not enough arguments (%d)\n", argc);
		XPD_ERR(xpd,
			"Any Command is composed of at least %d words "
			"(got only %d)\n",
			num_args, argc);
		goto out;
	}
	/* Process the arguments */
	argno = 0;
	if (strcmp(argv[argno], "*") == 0) {
		portno = PORT_BROADCAST;
		//XPD_DBG(REGS, xpd, "Port broadcast\n");
	} else {
		portno = parse_hexbyte(argv[argno]);
		if (portno < 0 || portno >= 8) {
			XPD_ERR(xpd, "Illegal port number '%s'\n", argv[argno]);
			goto out;
		}
		//XPD_DBG(REGS, xpd, "Port is %d\n", portno);
	}
	argno++;
	if (strlen(argv[argno]) != 2) {
		XPD_ERR(xpd, "Wrong operation codes '%s'\n", argv[argno]);
		goto out;
	}
	op = argv[argno][0];
	switch (op) {
	case 'W':
		writing = 1;
		num_args++;	/* data low */
		//XPD_DBG(REGS, xpd, "WRITING\n");
		break;
	case 'R':
		writing = 0;
		//XPD_DBG(REGS, xpd, "READING\n");
		break;
	default:
		XPD_ERR(xpd, "Unknown operation type '%c'\n", op);
		goto out;
	}
	addr_mode = argv[argno][1];
	switch (addr_mode) {
	case 'I':
		XPD_NOTICE(xpd,
			"'I' is deprecated in register commands. "
			"Use 'S' instead.\n");
		/* fall through */
	case 'S':
		do_subreg = 1;
		num_args += 2;	/* register + subreg */
		//XPD_DBG(REGS, xpd, "SUBREG\n");
		break;
	case 'D':
		do_subreg = 0;
		num_args++;	/* register */
		//XPD_DBG(REGS, xpd, "DIRECT\n");
		break;
	case 'M':
	case 'm':
		if (op != 'W') {
			XPD_ERR(xpd,
				"Can use Multibyte (%c) only with op 'W'\n",
				addr_mode);
			goto out;
		}
		num_args--;	/* No data low */
		//XPD_DBG(REGS, xpd, "Multibyte (%c)\n", addr_mode);
		break;
	default:
		XPD_ERR(xpd, "Unknown addressing type '%c'\n", addr_mode);
		goto out;
	}
	if (argv[argno][2] != '\0') {
		XPD_ERR(xpd, "Bad operation field '%s'\n", argv[argno]);
		goto out;
	}
	if (argc < num_args) {
		XPD_ERR(xpd,
			"Command \"%s\" is composed of at least %d words "
			"(got only %d)\n",
			argv[argno], num_args, argc);
		goto out;
	}
	argno++;
	if (addr_mode == 'M' || addr_mode == 'm') {
		if (argno < argc) {
			XPD_ERR(xpd,
				"Magic-Multibyte(%c) with %d extra arguments\n",
				addr_mode, argc - argno);
			goto out;
		}
		ret =
		    send_magic_request(xpd->xbus, xpd->addr.unit, portno,
				       addr_mode == 'm');
		goto out;
	}
	/* Normal (non-Magic) register commands */
	do_datah = 0;
	if (argno >= argc) {
		XPD_ERR(xpd, "Missing register number\n");
		goto out;
	}
	regnum = parse_hexbyte(argv[argno]);
	if (regnum < 0) {
		XPD_ERR(xpd, "Illegal register number '%s'\n", argv[argno]);
		goto out;
	}
	//XPD_DBG(REGS, xpd, "Register is %X\n", regnum);
	argno++;
	if (do_subreg) {
		if (argno >= argc) {
			XPD_ERR(xpd, "Missing subregister number\n");
			goto out;
		}
		subreg = parse_hexbyte(argv[argno]);
		if (subreg < 0) {
			XPD_ERR(xpd, "Illegal subregister number '%s'\n",
				argv[argno]);
			goto out;
		}
		//XPD_DBG(REGS, xpd, "Subreg is %X\n", subreg);
		argno++;
	} else
		subreg = 0;
	if (writing) {
		if (argno >= argc) {
			XPD_ERR(xpd, "Missing data low number\n");
			goto out;
		}
		data_low = parse_hexbyte(argv[argno]);
		if (data_low < 0) {
			XPD_ERR(xpd, "Illegal data_low number '%s'\n",
				argv[argno]);
			goto out;
		}
		//XPD_DBG(REGS, xpd, "Data Low is %X\n", data_low);
		argno++;
	} else
		data_low = 0;
	if (argno < argc) {
		do_datah = 1;
		if (!argv[argno]) {
			XPD_ERR(xpd, "Missing data high number\n");
			goto out;
		}
		data_high = parse_hexbyte(argv[argno]);
		if (data_high < 0) {
			XPD_ERR(xpd, "Illegal data_high number '%s'\n",
				argv[argno]);
			goto out;
		}
		//XPD_DBG(REGS, xpd, "Data High is %X\n", data_high);
		argno++;
	} else
		data_high = 0;
	if (argno < argc) {
		XPD_ERR(xpd, "Command contains an extra %d argument\n",
			argc - argno);
		goto out;
	}
#if 0
	XPD_DBG(REGS, xpd,
		"portno=%d writing=%d regnum=%d do_subreg=%d subreg=%d "
		"dataL=%d do_datah=%d dataH=%d\n",
		portno,			/* portno       */
		writing,		/* writing      */
		regnum, do_subreg,	/* use subreg   */
		subreg,			/* subreg       */
		data_low, do_datah,	/* use data_high */
		data_high);
#endif
	ret = xpp_register_request(xpd->xbus, xpd, portno,
		writing, regnum, do_subreg, subreg,
		data_low, do_datah, data_high, 1);
out:
	return ret;
}

#define	MAX_ARGS	10

int parse_chip_command(xpd_t *xpd, char *cmdline)
{
	xbus_t *xbus;
	int ret = -EBADR;
	__u8 buf[MAX_PROC_WRITE];
	char *str;
	char *p;
	char *argv[MAX_ARGS + 1];
	int argc;
	int i;

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	if (!XBUS_FLAGS(xbus, CONNECTED)) {
		XBUS_DBG(GENERAL, xbus, "Dropped packet. Disconnected.\n");
		return -EBUSY;
	}
	strlcpy(buf, cmdline, MAX_PROC_WRITE);	/* Save a copy */
	if (buf[0] == '#' || buf[0] == ';')
		XPD_DBG(REGS, xpd, "Note: '%s'\n", buf);
	if ((p = strchr(buf, '#')) != NULL)	/* Truncate comments */
		*p = '\0';
	if ((p = strchr(buf, ';')) != NULL)	/* Truncate comments */
		*p = '\0';
	/* Trim leading whitespace */
	for (p = buf; *p && (*p == ' ' || *p == '\t'); p++)
		;
	str = p;
	for (i = 0; (p = strsep(&str, " \t")) != NULL && i < MAX_ARGS;) {
		if (*p != '\0') {
			argv[i] = p;
			// XPD_DBG(REGS, xpd, "ARG %d = '%s'\n", i, p);
			i++;
		}
	}
	argv[i] = NULL;
	argc = i;
	if (p) {
		XPD_ERR(xpd, "Too many words (%d) to process. Last was '%s'\n",
			i, p);
		goto out;
	}
	if (argc)
		ret = execute_chip_command(xpd, argc, argv);
	else
		ret = 0;	/* empty command - no op */
out:
	return ret;
}

/*---------------- GLOBAL Protocol Commands -------------------------------*/

static bool global_packet_is_valid(xpacket_t *pack);
static void global_packet_dump(const char *msg, xpacket_t *pack);

/*---------------- GLOBAL: HOST COMMANDS ----------------------------------*/

/* 0x07 */ HOSTCMD(GLOBAL, AB_REQUEST)
{
	int ret = -ENODEV;
	xframe_t *xframe;
	xpacket_t *pack;

	if (!xbus) {
		DBG(DEVICES, "NO XBUS\n");
		return -EINVAL;
	}
	if (xbus_check_unique(xbus))
		return -EBUSY;
	XFRAME_NEW_CMD(xframe, pack, xbus, GLOBAL, AB_REQUEST, 0);
	RPACKET_FIELD(pack, GLOBAL, AB_REQUEST, rev) = XPP_PROTOCOL_VERSION;
	RPACKET_FIELD(pack, GLOBAL, AB_REQUEST, reserved) = 0;
	XBUS_DBG(DEVICES, xbus, "Protocol Version %d\n", XPP_PROTOCOL_VERSION);
	if (xbus_setstate(xbus, XBUS_STATE_SENT_REQUEST))
		ret = send_cmd_frame(xbus, xframe);
	return ret;
}

int xpp_register_request(xbus_t *xbus, xpd_t *xpd, xportno_t portno,
			 bool writing, __u8 regnum, bool do_subreg, __u8 subreg,
			 __u8 data_low, bool do_datah, __u8 data_high,
			 bool should_reply)
{
	int ret = 0;
	xframe_t *xframe;
	xpacket_t *pack;
	reg_cmd_t *reg_cmd;

	if (!xbus) {
		DBG(REGS, "NO XBUS\n");
		return -EINVAL;
	}
	XFRAME_NEW_CMD(xframe, pack, xbus, GLOBAL, REGISTER_REQUEST,
		       xpd->xbus_idx);
	LINE_DBG(REGS, xpd, portno, "%c%c %02X %02X %02X %02X\n",
		 (writing) ? 'W' : 'R', (do_subreg) ? 'S' : 'D', regnum, subreg,
		 data_low, data_high);
	reg_cmd = &RPACKET_FIELD(pack, GLOBAL, REGISTER_REQUEST, reg_cmd);
	/* do not count the 'bytes' field */
	reg_cmd->bytes = sizeof(*reg_cmd) - 1;
	reg_cmd->is_multibyte = 0;
	if (portno == PORT_BROADCAST) {
		reg_cmd->portnum = 0;
		REG_FIELD(reg_cmd, all_ports_broadcast) = 1;
	} else {
		reg_cmd->portnum = portno;
		REG_FIELD(reg_cmd, all_ports_broadcast) = 0;
	}
	reg_cmd->eoframe = 0;
	REG_FIELD(reg_cmd, reserved) = 0;	/* force reserved bits to 0 */
	REG_FIELD(reg_cmd, read_request) = (writing) ? 0 : 1;
	REG_FIELD(reg_cmd, do_subreg) = do_subreg;
	REG_FIELD(reg_cmd, regnum) = regnum;
	REG_FIELD(reg_cmd, subreg) = subreg;
	REG_FIELD(reg_cmd, do_datah) = do_datah;
	REG_FIELD(reg_cmd, data_low) = data_low;
	REG_FIELD(reg_cmd, data_high) = data_high;
	if (should_reply)
		xpd->requested_reply = *reg_cmd;
	if (debug & DBG_REGS) {
		dump_reg_cmd("REG_REQ", 1, xbus, xpd->addr.unit,
			     reg_cmd->portnum, reg_cmd);
		dump_packet("REG_REQ", pack, 1);
	}
	if (!xframe->usec_towait) {	/* default processing time of SPI */
		if (subreg)
			xframe->usec_towait = 2000;
		else
			xframe->usec_towait = 1000;
	}
	ret = send_cmd_frame(xbus, xframe);
	return ret;
}
EXPORT_SYMBOL(xpp_register_request);

/*
 * The XPD parameter is totaly ignored by the driver and firmware as well.
 */
/* 0x19 */ HOSTCMD(GLOBAL, SYNC_SOURCE, enum sync_mode mode, int drift)
{
	xframe_t *xframe;
	xpacket_t *pack;
	const char *mode_name;

	BUG_ON(!xbus);
	if ((mode_name = sync_mode_name(mode)) == NULL) {
		XBUS_ERR(xbus, "SYNC_SOURCE: bad sync_mode=0x%X\n", mode);
		return -EINVAL;
	}
	XBUS_DBG(SYNC, xbus, "%s (0x%X), drift=%d\n", mode_name, mode, drift);
	XFRAME_NEW_CMD(xframe, pack, xbus, GLOBAL, SYNC_SOURCE, 0);
	RPACKET_FIELD(pack, GLOBAL, SYNC_SOURCE, sync_mode) = mode;
	RPACKET_FIELD(pack, GLOBAL, SYNC_SOURCE, drift) = drift;
	send_cmd_frame(xbus, xframe);
	return 0;
}

/*
 * Wrapper for different types of xbus reset
 */
static int send_xbus_reset(xbus_t *xbus, uint8_t reset_mask)
{
	xframe_t *xframe;
	xpacket_t *pack;

	BUG_ON(!xbus);
	XFRAME_NEW_CMD(xframe, pack, xbus, GLOBAL, XBUS_RESET, 0);
	RPACKET_FIELD(pack, GLOBAL, XBUS_RESET, mask) = reset_mask;
	send_cmd_frame(xbus, xframe);
	return 0;
}

/* 0x23 */ HOSTCMD(GLOBAL, RESET_SPI)
{
	XBUS_DBG(DEVICES, xbus, "Sending SPI reset\n");
	/* toggle reset line */
	send_xbus_reset(xbus, 0x04);
	send_xbus_reset(xbus, 0x00);
	return 0;
}


/* 0x23 */ HOSTCMD(GLOBAL, RESET_SYNC_COUNTERS)
{
	//XBUS_DBG(SYNC, xbus, "\n");
	return send_xbus_reset(xbus, 0x10);
}

/*---------------- GLOBAL: Astribank Reply Handlers -----------------------*/

HANDLER_DEF(GLOBAL, NULL_REPLY)
{
	XBUS_DBG(GENERAL, xbus, "got len=%d\n", XPACKET_LEN(pack));
	return 0;
}

HANDLER_DEF(GLOBAL, AB_DESCRIPTION)
{				/* 0x08 */
	struct xbus_workqueue *worker;
	__u8 rev;
	struct unit_descriptor *units;
	int count_units;
	int i;
	int ret = 0;

	if (!xbus) {
		NOTICE("%s: xbus is gone!!!\n", __func__);
		goto out;
	}
	rev = RPACKET_FIELD(pack, GLOBAL, AB_DESCRIPTION, rev);
	units = RPACKET_FIELD(pack, GLOBAL, AB_DESCRIPTION, unit_descriptor);
	count_units = XPACKET_LEN(pack) - ((__u8 *)units - (__u8 *)pack);
	count_units /= sizeof(*units);
	if (rev != XPP_PROTOCOL_VERSION) {
		XBUS_NOTICE(xbus, "Bad protocol version %d (should be %d)\n",
			    rev, XPP_PROTOCOL_VERSION);
		ret = -EPROTO;
		goto proto_err;
	}
	if (count_units > NUM_UNITS) {
		XBUS_NOTICE(xbus, "Too many units %d (should be %d)\n",
			    count_units, NUM_UNITS);
		ret = -EPROTO;
		goto proto_err;
	}
	if (count_units <= 0) {
		XBUS_NOTICE(xbus, "Empty astribank? (%d units)\n", count_units);
		ret = -EPROTO;
		goto proto_err;
	}
	if (units[0].addr.unit != 0 || units[0].addr.subunit != 0) {
		XBUS_NOTICE(xbus, "No first module. Astribank unusable.\n");
		ret = -EPROTO;
		goto proto_err;
	}
	if (!xbus_setstate(xbus, XBUS_STATE_RECVD_DESC)) {
		ret = -EPROTO;
		goto proto_err;
	}
	XBUS_INFO(xbus, "DESCRIPTOR: %d cards, protocol revision %d\n",
		  count_units, rev);
	if (xbus_check_unique(xbus))
		return -EBUSY;
	xbus->revision = rev;
	worker = &xbus->worker;
	if (!worker->wq) {
		XBUS_ERR(xbus, "missing worker thread\n");
		ret = -ENODEV;
		goto out;
	}
	for (i = 0; i < count_units; i++) {
		struct unit_descriptor *this_unit = &units[i];
		struct card_desc_struct *card_desc;
		unsigned long flags;

		if ((card_desc =
		     KZALLOC(sizeof(struct card_desc_struct),
			     GFP_ATOMIC)) == NULL) {
			XBUS_ERR(xbus, "Card description allocation failed.\n");
			ret = -ENOMEM;
			goto out;
		}
		card_desc->magic = CARD_DESC_MAGIC;
		INIT_LIST_HEAD(&card_desc->card_list);
		card_desc->type = this_unit->type;
		card_desc->subtype = this_unit->subtype;
		card_desc->xpd_addr = this_unit->addr;
		card_desc->numchips = this_unit->numchips;
		card_desc->ports_per_chip = this_unit->ports_per_chip;
		card_desc->port_dir = this_unit->port_dir;
		card_desc->ports =
		    card_desc->numchips * card_desc->ports_per_chip;
		XBUS_INFO(xbus,
			"    CARD %d type=%d.%d ports=%d (%dx%d), "
			"port-dir=0x%02X\n",
			card_desc->xpd_addr.unit, card_desc->type,
			card_desc->subtype, card_desc->ports,
			card_desc->numchips, card_desc->ports_per_chip,
			card_desc->port_dir);
		spin_lock_irqsave(&worker->worker_lock, flags);
		worker->num_units++;
		XBUS_COUNTER(xbus, UNITS)++;
		list_add_tail(&card_desc->card_list, &worker->card_list);
		spin_unlock_irqrestore(&worker->worker_lock, flags);
	}
	CALL_PROTO(GLOBAL, RESET_SPI, xbus, NULL);
	if (!xbus_process_worker(xbus)) {
		ret = -ENODEV;
		goto out;
	}
	goto out;
proto_err:
	xbus_setstate(xbus, XBUS_STATE_FAIL);
	dump_packet("AB_DESCRIPTION", pack, DBG_ANY);
out:
	return ret;
}

HANDLER_DEF(GLOBAL, REGISTER_REPLY)
{
	reg_cmd_t *reg = &RPACKET_FIELD(pack, GLOBAL, REGISTER_REPLY, regcmd);

	if (!xpd) {
		static int rate_limit;

		if ((rate_limit++ % 1003) < 5)
			notify_bad_xpd(__func__, xbus, XPACKET_ADDR(pack), "");
		return -EPROTO;
	}
	if (debug & DBG_REGS) {
		dump_reg_cmd("REG_REPLY", 0, xbus, xpd->addr.unit, reg->portnum,
			     reg);
		dump_packet("REG_REPLY", pack, 1);
	}
	if (!XMETHOD(card_register_reply, xpd)) {
		XPD_ERR(xpd,
			"REGISTER_REPLY: missing card_register_reply()\n");
		return -EINVAL;
	}
	return CALL_XMETHOD(card_register_reply, xpd, reg);
}

HANDLER_DEF(GLOBAL, SYNC_REPLY)
{
	__u8 mode = RPACKET_FIELD(pack, GLOBAL, SYNC_REPLY, sync_mode);
	__u8 drift = RPACKET_FIELD(pack, GLOBAL, SYNC_REPLY, drift);
	const char *mode_name;

	BUG_ON(!xbus);
	if ((mode_name = sync_mode_name(mode)) == NULL) {
		XBUS_ERR(xbus, "SYNC_REPLY: bad sync_mode=0x%X\n", mode);
		return -EINVAL;
	}
	XBUS_DBG(SYNC, xbus, "%s (0x%X), drift=%d\n", mode_name, mode, drift);
	//dump_packet("SYNC_REPLY", pack, debug & DBG_SYNC);
	got_new_syncer(xbus, mode, drift);
	return 0;
}

#define	TMP_NAME_LEN	(XBUS_NAMELEN + XPD_NAMELEN + 5)

HANDLER_DEF(GLOBAL, ERROR_CODE)
{
	char tmp_name[TMP_NAME_LEN];
	static long rate_limit;
	__u8 category_code;
	__u8 errorbits;

	BUG_ON(!xbus);
	if ((rate_limit++ % 5003) > 200)
		return 0;
	category_code = RPACKET_FIELD(pack, GLOBAL, ERROR_CODE, category_code);
	errorbits = RPACKET_FIELD(pack, GLOBAL, ERROR_CODE, errorbits);
	if (!xpd) {
		snprintf(tmp_name, TMP_NAME_LEN, "%s(%1d%1d)", xbus->busname,
			 XPACKET_ADDR_UNIT(pack), XPACKET_ADDR_SUBUNIT(pack));
	} else {
		snprintf(tmp_name, TMP_NAME_LEN, "%s/%s", xbus->busname,
			 xpd->xpdname);
	}
	NOTICE
	    ("%s: FIRMWARE %s: category=%d errorbits=0x%02X (rate_limit=%ld)\n",
	     tmp_name, cmd->name, category_code, errorbits, rate_limit);
	dump_packet("FIRMWARE: ", pack, 1);
	/*
	 * FIXME: Should implement an error recovery plan
	 */
	return 0;
}

xproto_table_t PROTO_TABLE(GLOBAL) = {
	.entries = {
		/*	Prototable	Card	Opcode		*/
		XENTRY(	GLOBAL,		GLOBAL,	NULL_REPLY	),
		XENTRY(	GLOBAL,		GLOBAL,	AB_DESCRIPTION	),
		XENTRY(	GLOBAL,		GLOBAL,	SYNC_REPLY	),
		XENTRY(	GLOBAL,		GLOBAL,	ERROR_CODE	),
		XENTRY(	GLOBAL,		GLOBAL,	REGISTER_REPLY	),
	},
	.name = "GLOBAL",
	.packet_is_valid = global_packet_is_valid,
	.packet_dump = global_packet_dump,
};

static bool global_packet_is_valid(xpacket_t *pack)
{
	const xproto_entry_t *xe;

	//DBG(GENERAL, "\n");
	xe = xproto_global_entry(XPACKET_OP(pack));
	return xe != NULL;
}

static void global_packet_dump(const char *msg, xpacket_t *pack)
{
	DBG(GENERAL, "%s\n", msg);
}

#define	MAX_PATH_STR	128

#ifndef	UMH_WAIT_PROC
/*
 * - UMH_WAIT_PROC was introduced as enum in 2.6.23
 *   with a value of 1
 * - It was changed to a macro (and it's value was modified) in 3.3.0
 */
#define	UMH_WAIT_PROC	1
#endif

int run_initialize_registers(xpd_t *xpd)
{
	int ret;
	xbus_t *xbus;
	char busstr[MAX_ENV_STR];
	char busnumstr[MAX_ENV_STR];
	char modelstr[MAX_ENV_STR];
	char unitstr[MAX_ENV_STR];
	char subunitsstr[MAX_ENV_STR];
	char typestr[MAX_ENV_STR];
	char directionstr[MAX_ENV_STR];
	char revstr[MAX_ENV_STR];
	char connectorstr[MAX_ENV_STR];
	char xbuslabel[MAX_ENV_STR];
	char init_card[MAX_PATH_STR];
	__u8 direction_mask;
	int i;
	char *argv[] = {
		init_card,
		NULL
	};
	char *envp[] = {
		busstr,
		busnumstr,
		modelstr,
		unitstr,
		subunitsstr,
		typestr,
		directionstr,
		revstr,
		connectorstr,
		xbuslabel,
		NULL
	};

	BUG_ON(!xpd);
	xbus = xpd->xbus;
	if (!initdir || !initdir[0]) {
		XPD_NOTICE(xpd, "Missing initdir parameter\n");
		ret = -EINVAL;
		goto err;
	}
	if (!xpd_setstate(xpd, XPD_STATE_INIT_REGS)) {
		ret = -EINVAL;
		goto err;
	}
	direction_mask = 0;
	for (i = 0; i < xpd->subunits; i++) {
		xpd_t *su = xpd_byaddr(xbus, xpd->addr.unit, i);

		if (!su) {
			XPD_ERR(xpd, "Have %d subunits, but not subunit #%d\n",
				xpd->subunits, i);
			continue;
		}
		direction_mask |=
		    (PHONEDEV(su).direction == TO_PHONE) ? BIT(i) : 0;
	}
	snprintf(busstr, MAX_ENV_STR, "XBUS_NAME=%s", xbus->busname);
	snprintf(busnumstr, MAX_ENV_STR, "XBUS_NUMBER=%d", xbus->num);
	snprintf(modelstr, MAX_ENV_STR, "XBUS_MODEL_STRING=%s",
		 xbus->transport.model_string);
	snprintf(unitstr, MAX_ENV_STR, "UNIT_NUMBER=%d", xpd->addr.unit);
	snprintf(typestr, MAX_ENV_STR, "UNIT_TYPE=%d", xpd->type);
	snprintf(subunitsstr, MAX_ENV_STR, "UNIT_SUBUNITS=%d", xpd->subunits);
	snprintf(directionstr, MAX_ENV_STR, "UNIT_SUBUNITS_DIR=%d",
		 direction_mask);
	snprintf(revstr, MAX_ENV_STR, "XBUS_REVISION=%d", xbus->revision);
	snprintf(connectorstr, MAX_ENV_STR, "XBUS_CONNECTOR=%s",
		 xbus->connector);
	snprintf(xbuslabel, MAX_ENV_STR, "XBUS_LABEL=%s", xbus->label);
	if (snprintf
	    (init_card, MAX_PATH_STR, "%s/init_card_%d_%d", initdir, xpd->type,
	     xbus->revision) > MAX_PATH_STR) {
		XPD_NOTICE(xpd,
			"Cannot initialize. pathname is longer "
			"than %d characters.\n",
			MAX_PATH_STR);
		ret = -E2BIG;
		goto err;
	}
	if (!XBUS_IS(xbus, RECVD_DESC)) {
		XBUS_ERR(xbus,
			 "Skipped register initialization. In state %s.\n",
			 xbus_statename(XBUS_STATE(xbus)));
		ret = -ENODEV;
		goto err;
	}
	XPD_DBG(DEVICES, xpd, "running '%s' for type=%d revision=%d\n",
		init_card, xpd->type, xbus->revision);
	ret = call_usermodehelper(init_card, argv, envp, UMH_WAIT_PROC);
	/*
	 * Carefully report results
	 */
	if (ret == 0)
		XPD_DBG(DEVICES, xpd, "'%s' finished OK\n", init_card);
	else if (ret < 0) {
		XPD_ERR(xpd, "Failed running '%s' (errno %d)\n", init_card,
			ret);
	} else {
		__u8 exitval = ((unsigned)ret >> 8) & 0xFF;
		__u8 sigval = ret & 0xFF;

		if (!exitval) {
			XPD_ERR(xpd, "'%s' killed by signal %d\n", init_card,
				sigval);
		} else {
			XPD_ERR(xpd, "'%s' aborted with exitval %d\n",
				init_card, exitval);
		}
		ret = -EINVAL;
	}
err:
	return ret;
}
EXPORT_SYMBOL(run_initialize_registers);
