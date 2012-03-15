#ifndef	CARD_GLOBAL_H
#define	CARD_GLOBAL_H
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
#include "xbus-pcm.h"

enum global_opcodes {
	XPROTO_NAME(GLOBAL, AB_REQUEST)		= 0x07,
	XPROTO_NAME(GLOBAL, AB_DESCRIPTION)	= 0x08,
	XPROTO_NAME(GLOBAL, REGISTER_REQUEST)	= 0x0F,
	XPROTO_NAME(GLOBAL, REGISTER_REPLY)	= 0x10,
/**/
	XPROTO_NAME(GLOBAL, PCM_WRITE)		= 0x11,
	XPROTO_NAME(GLOBAL, PCM_READ)		= 0x12,
/**/
	XPROTO_NAME(GLOBAL, SYNC_SOURCE)	= 0x19,
	XPROTO_NAME(GLOBAL, SYNC_REPLY)		= 0x1A,
/**/
	XPROTO_NAME(GLOBAL, ERROR_CODE)		= 0x22,
	XPROTO_NAME(GLOBAL, XBUS_RESET)	= 0x23,
	XPROTO_NAME(GLOBAL, NULL_REPLY)		= 0xFE,
};

struct unit_descriptor {
	struct xpd_addr	addr;
	byte		subtype:4;
	byte		type:4;
	byte		numchips;
	byte		ports_per_chip;
	byte		port_dir;	/* bitmask: 0 - PSTN, 1 - PHONE */
	byte		reserved[2];
	struct xpd_addr	ec_addr;
};

#define	NUM_UNITS	6

DEF_RPACKET_DATA(GLOBAL, NULL_REPLY);
DEF_RPACKET_DATA(GLOBAL, AB_REQUEST,
	byte		rev;
	byte		reserved;
	);
DEF_RPACKET_DATA(GLOBAL, AB_DESCRIPTION,
	byte			rev;
	byte			reserved[3];
	struct unit_descriptor	unit_descriptor[NUM_UNITS];
	);
DEF_RPACKET_DATA(GLOBAL, REGISTER_REQUEST,
	reg_cmd_t	reg_cmd;
	);
DEF_RPACKET_DATA(GLOBAL, PCM_WRITE,
	xpp_line_t	lines;
	byte		pcm[PCM_CHUNKSIZE];
	);
DEF_RPACKET_DATA(GLOBAL, PCM_READ,
	xpp_line_t	lines;
	byte		pcm[PCM_CHUNKSIZE];
	);
DEF_RPACKET_DATA(GLOBAL, SYNC_SOURCE,
	byte		sync_mode;
	byte		drift;
	);
DEF_RPACKET_DATA(GLOBAL, SYNC_REPLY,
	byte		sync_mode;
	byte		drift;
	);
DEF_RPACKET_DATA(GLOBAL, REGISTER_REPLY,
	reg_cmd_t	regcmd;
	);
DEF_RPACKET_DATA(GLOBAL, XBUS_RESET,
	byte		mask;
	);
DEF_RPACKET_DATA(GLOBAL, ERROR_CODE,
	byte		category_code;
	byte		errorbits;
	byte		bad_packet[0];
	);

/* 0x07 */ DECLARE_CMD(GLOBAL, AB_REQUEST);
/* 0x19 */ DECLARE_CMD(GLOBAL, SYNC_SOURCE, enum sync_mode mode, int drift);
/* 0x23 */ DECLARE_CMD(GLOBAL, RESET_SPI);
/* 0x23 */ DECLARE_CMD(GLOBAL, RESET_SYNC_COUNTERS);

int xpp_register_request(xbus_t *xbus, xpd_t *xpd, xportno_t portno,
	bool writing, byte regnum, bool do_subreg, byte subreg,
	byte data_low, bool do_datah, byte data_high, bool should_reply);
int send_multibyte_request(xbus_t *xbus, unsigned unit, xportno_t portno,
	bool eoftx, byte *buf, unsigned len);
extern xproto_table_t PROTO_TABLE(GLOBAL);
int run_initialize_registers(xpd_t *xpd);
int parse_chip_command(xpd_t *xpd, char *cmdline);
extern charp initdir;

#endif	/* CARD_GLOBAL_H */
