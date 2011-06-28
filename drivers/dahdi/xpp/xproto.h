#ifndef	XPROTO_H
#define	XPROTO_H
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

#include "xdefs.h"

#ifdef	__KERNEL__
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <dahdi/kernel.h>

/*
 * This must match the firmware protocol version
 */
#define	XPP_PROTOCOL_VERSION	30

struct xpd_addr {
	uint8_t		subunit:SUBUNIT_BITS;
	uint8_t		reserved:1;
	uint8_t		unit:UNIT_BITS;
	uint8_t		sync_master:1;
} PACKED;

#define	MKADDR(p, u, s)	do {	\
				(p)->unit = (u);	\
				(p)->subunit = (s);	\
				(p)->sync_master = 0;	\
			} while(0)

struct xpacket_header {
	uint16_t	packet_len:10;
	uint16_t	reserved:1;
	uint16_t	is_pcm:1;
	uint16_t	pcmslot:4;
	uint8_t		opcode;
	struct xpd_addr	addr;
} PACKED;

#define	XPACKET_OP(p)		((p)->head.opcode)
#define	XPACKET_LEN(p)		((p)->head.packet_len)
#define	XPACKET_IS_PCM(p)	((p)->head.is_pcm)
#define	XPACKET_PCMSLOT(p)	((p)->head.pcmslot)
#define	XPACKET_RESERVED(p)	((p)->head.reserved)
#define	XPACKET_ADDR(p)		((p)->head.addr)
#define	XPACKET_ADDR_UNIT(p)	(XPACKET_ADDR(p).unit)
#define	XPACKET_ADDR_SUBUNIT(p)	(XPACKET_ADDR(p).subunit)
#define	XPACKET_ADDR_SYNC(p)	(XPACKET_ADDR(p).sync_master)
#define	XPACKET_ADDR_RESERVED(p)	(XPACKET_ADDR(p).reserved)

#define	PROTO_TABLE(n)	n ## _protocol_table

/*
 * The LSB of the type number signifies:
 * 	0 - TO_PSTN
 * 	1 - TO_PHONE
 */
#define	XPD_TYPE_FXS		1	// TO_PHONE
#define	XPD_TYPE_FXO		2	// TO_PSTN
#define	XPD_TYPE_BRI		3	// TO_PSTN/TO_PHONE (from hardware)
#define	XPD_TYPE_PRI		4	// TO_PSTN/TO_PHONE (runtime)
#define	XPD_TYPE_ECHO		5	// Octasic echo canceller
#define	XPD_TYPE_NOMODULE	7

typedef	byte	xpd_type_t;

#define	XPD_TYPE_PREFIX	"xpd-type-"

#define	MODULE_ALIAS_XPD(type)	\
	MODULE_ALIAS(XPD_TYPE_PREFIX __stringify(type))

#define	PCM_CHUNKSIZE	(CHANNELS_PERXPD * 8)	/* samples of 8 bytes */

bool valid_xpd_addr(const struct xpd_addr *addr);

#define	XPROTO_NAME(card,op)	card ## _ ## op
#define	XPROTO_HANDLER(card,op)	XPROTO_NAME(card,op ## _handler)
#define	XPROTO_CALLER(card,op)	XPROTO_NAME(card,op ## _send)

#define	HANDLER_DEF(card,op)	\
	static int XPROTO_HANDLER(card,op) (	\
		xbus_t *xbus,			\
		xpd_t *xpd,			\
		const xproto_entry_t *cmd,	\
		xpacket_t *pack)

#define	CALL_PROTO(card,op, ...)	XPROTO_CALLER(card,op)( __VA_ARGS__ )

#define	DECLARE_CMD(card,op, ...)	\
	int CALL_PROTO(card, op, xbus_t *xbus, xpd_t *xpd, ## __VA_ARGS__ )

#define	HOSTCMD(card, op, ...)					\
		DECLARE_CMD(card, op, ## __VA_ARGS__ )

#define	RPACKET_NAME(card,op)	XPROTO_NAME(RPACKET_ ## card, op)
#define	RPACKET_TYPE(card,op)	struct RPACKET_NAME(card, op)

#define	DEF_RPACKET_DATA(card,op, ...)		\
	RPACKET_TYPE(card,op) {			\
		struct xpacket_header	head;	\
		__VA_ARGS__			\
	} PACKED
#define	RPACKET_HEADERSIZE		sizeof(struct xpacket_header)
#define	RPACKET_FIELD(p,card,op,field)	(((RPACKET_TYPE(card,op) *)(p))->field)
#define	RPACKET_SIZE(card,op)		sizeof(RPACKET_TYPE(card,op))

#define	XENTRY(prototab,module,op)			\
	[ XPROTO_NAME(module,op) ] = {			\
		.handler = XPROTO_HANDLER(module,op),	\
		.datalen = RPACKET_SIZE(module,op),	\
		.name = #op,				\
		.table = &PROTO_TABLE(prototab)		\
	}

#define	XPACKET_INIT(p, card, op, to, pcm, pcmslot)		\
		do {						\
			XPACKET_OP(p) = XPROTO_NAME(card,op);	\
			XPACKET_LEN(p) = RPACKET_SIZE(card,op);	\
			XPACKET_IS_PCM(p) = (pcm);		\
			XPACKET_PCMSLOT(p) = (pcmslot);		\
			XPACKET_RESERVED(p) = 0;		\
			XPACKET_ADDR_UNIT(p) = XBUS_UNIT(to);	\
			XPACKET_ADDR_SUBUNIT(p) = XBUS_SUBUNIT(to);	\
			XPACKET_ADDR_SYNC(p) = 0;		\
			XPACKET_ADDR_RESERVED(p) = 0;		\
		} while(0)

#define	XFRAME_NEW_CMD(frm, p, xbus, card, op, to)		\
	do {							\
		int		pack_len = RPACKET_SIZE(card,op);	\
								\
		if(!XBUS_FLAGS(xbus, CONNECTED))		\
			return -ENODEV;				\
		(frm) = ALLOC_SEND_XFRAME(xbus);		\
		if(!(frm))					\
			return -ENOMEM;				\
		(p) = xframe_next_packet(frm, pack_len);	\
		if(!(p))					\
			return -ENOMEM;				\
		XPACKET_INIT(p, card, op, to, 0, 0);		\
		(frm)->usec_towait = 0;				\
	} while(0)

#endif

/*--------------------------- register handling --------------------------------*/

#define	MULTIBYTE_MAX_LEN	5	/* FPGA firmware limitation */

typedef struct reg_cmd {
	byte		bytes:3;		/* Length (for Multibyte)	*/
	byte		eoframe:1;		/* For BRI -- end of frame	*/
	byte		portnum:3;		/* For port specific registers	*/
	byte		is_multibyte:1;
	union {
		struct {
			byte		reserved:4;
			byte		do_datah:1;
			byte		do_subreg:1;
			byte		read_request:1;
			byte		all_ports_broadcast:1;
			byte		regnum;
			byte		subreg;
			byte		data_low;
			byte		data_high;
		} PACKED r;
		/* For Write-Multibyte commands in BRI */
		struct {
			byte	xdata[MULTIBYTE_MAX_LEN];
		} PACKED d;
	} PACKED alt;
} PACKED reg_cmd_t;

/* Shortcut access macros */
#define	REG_FIELD(regptr,member)	((regptr)->alt.r.member)
#define	REG_XDATA(regptr)		((regptr)->alt.d.xdata)

#ifdef __KERNEL__
/*--------------------------- protocol tables ----------------------------------*/

typedef struct xproto_entry	xproto_entry_t;
typedef struct xproto_table	xproto_table_t;

typedef int (*xproto_handler_t)(
		xbus_t *xbus,
		xpd_t *xpd,
		const xproto_entry_t *cmd,
		xpacket_t *pack);

const xproto_table_t *xproto_get(xpd_type_t cardtype);
void xproto_put(const xproto_table_t *xtable);
const xproto_entry_t *xproto_card_entry(const xproto_table_t *table, byte opcode);
xproto_handler_t xproto_card_handler(const xproto_table_t *table, byte opcode);

const xproto_entry_t *xproto_global_entry(byte opcode);
xproto_handler_t xproto_global_handler(byte opcode);

/*
 * XMETHOD() resolve to method pointer (NULL for optional methods)
 * CALL_XMETHOD() calls the method, passing mandatory arguments
 */
#define	XMETHOD(name, xpd)	((xpd)->xops->name)
#define	CALL_XMETHOD(name, xpd, ...)                              \
		(XMETHOD(name, (xpd))((xpd)->xbus, (xpd), ## __VA_ARGS__ ))

/*
 * PHONE_METHOD() resolve to method pointer (NULL for optional methods)
 * CALL_PHONE_METHOD() calls the method, passing mandatory arguments
 */
#define	PHONE_METHOD(name, xpd)	(PHONEDEV(xpd).phoneops->name)
#define	CALL_PHONE_METHOD(name, xpd, ...)                              \
		(PHONE_METHOD(name, (xpd))((xpd), ## __VA_ARGS__ ))

struct phoneops {
	void (*card_pcm_recompute)(xpd_t *xpd, xpp_line_t pcm_mask);
	void (*card_pcm_fromspan)(xpd_t *xpd, xpacket_t *pack);
	void (*card_pcm_tospan)(xpd_t *xpd, xpacket_t *pack);
	int (*echocancel_timeslot)(xpd_t *xpd, int pos);
	int (*echocancel_setmask)(xpd_t *xpd, xpp_line_t ec_mask);
	int (*card_timing_priority)(xpd_t *xpd);
	int (*card_dahdi_preregistration)(xpd_t *xpd, bool on);
	int (*card_dahdi_postregistration)(xpd_t *xpd, bool on);
	int (*card_hooksig)(xpd_t *xpd, int pos, enum dahdi_txsig txsig);
	int (*card_ioctl)(xpd_t *xpd, int pos, unsigned int cmd, unsigned long arg);
	int (*card_open)(xpd_t *xpd, lineno_t pos);
	int (*card_close)(xpd_t *xpd, lineno_t pos);
	int (*card_state)(xpd_t *xpd, bool on);
};

struct xops {
	 xpd_t *(*card_new)(xbus_t *xbus, int unit, int subunit,
		const xproto_table_t *proto_table, byte subtype,
		int subunits, int subunit_ports, bool to_phone);
	int (*card_init)(xbus_t *xbus, xpd_t *xpd);
	int (*card_remove)(xbus_t *xbus, xpd_t *xpd);
	int (*card_tick)(xbus_t *xbus, xpd_t *xpd);
	int (*card_register_reply)(xbus_t *xbus, xpd_t *xpd, reg_cmd_t *reg);
};

struct xproto_entry {
	xproto_handler_t	handler;
	int			datalen;
	const char		*name;
	xproto_table_t		*table;
};

struct xproto_table {
	struct module		*owner;
	xproto_entry_t		entries[256];	/* Indexed by opcode */
	const struct xops	*xops;		/* Card level operations */
	const struct phoneops	*phoneops;	/* DAHDI operations */
	const struct echoops	*echoops;	/* Echo Canceller operations */
	xpd_type_t	type;
	byte		ports_per_subunit;
	const char	*name;
	bool (*packet_is_valid)(xpacket_t *pack);
	void (*packet_dump)(const char *msg, xpacket_t *pack);
};

#include "card_global.h"
#include "card_fxs.h"
#include "card_fxo.h"
#include "card_bri.h"
#include "card_pri.h"


#define	MEMBER(card,op)	RPACKET_TYPE(card,op)	RPACKET_NAME(card,op)

struct xpacket {
	struct xpacket_header	head;
	union {
		MEMBER(GLOBAL, NULL_REPLY);
		MEMBER(GLOBAL, PCM_WRITE);
		MEMBER(GLOBAL, PCM_READ);
		MEMBER(GLOBAL, SYNC_REPLY);
		MEMBER(GLOBAL, ERROR_CODE);

		MEMBER(FXS, SIG_CHANGED);
		MEMBER(FXO, SIG_CHANGED);

		byte	data[0];
	};
	/* Last byte is chksum */
} PACKED;

void dump_packet(const char *msg, const xpacket_t *packet, bool debug);
void dump_reg_cmd(const char msg[], bool writing, xbus_t *xbus, byte unit, xportno_t port, const reg_cmd_t *regcmd);
int xframe_receive(xbus_t *xbus, xframe_t *xframe);
void notify_bad_xpd(const char *funcname, xbus_t *xbus, const struct xpd_addr addr, const char *msg);
int xproto_register(const xproto_table_t *proto_table);
void xproto_unregister(const xproto_table_t *proto_table);
const xproto_entry_t *xproto_global_entry(byte opcode);
const char *xproto_name(xpd_type_t xpd_type);

#endif	/* __KERNEL__ */

#endif	/* XPROTO_H */
