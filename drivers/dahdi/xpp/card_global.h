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
	XPROTO_NAME(GLOBAL, AB_REQUEST) = 0x07,
	XPROTO_NAME(GLOBAL, AB_DESCRIPTION) = 0x08,
	XPROTO_NAME(GLOBAL, REGISTER_REQUEST) = 0x0F,
	XPROTO_NAME(GLOBAL, REGISTER_REPLY) = 0x10,
	 /**/ XPROTO_NAME(GLOBAL, PCM_WRITE) = 0x11,
	XPROTO_NAME(GLOBAL, PCM_READ) = 0x12,
	 /**/ XPROTO_NAME(GLOBAL, SYNC_SOURCE) = 0x19,
	XPROTO_NAME(GLOBAL, SYNC_REPLY) = 0x1A,
	 /**/ XPROTO_NAME(GLOBAL, ERROR_CODE) = 0x22,
	XPROTO_NAME(GLOBAL, XBUS_RESET) = 0x23,
	XPROTO_NAME(GLOBAL, NULL_REPLY) = 0xFE,
};

struct unit_descriptor {
	struct xpd_addr addr;
	__u8 subtype:4;
	__u8 type:4;
	__u8 numchips;
	__u8 ports_per_chip;
	__u8 port_dir;		/* bitmask: 0 - PSTN, 1 - PHONE */
	__u8 reserved[2];
	struct xpd_addr ec_addr;
};

#define	NUM_UNITS	6

DEF_RPACKET_DATA(GLOBAL, NULL_REPLY);
DEF_RPACKET_DATA(GLOBAL, AB_REQUEST, __u8 rev; __u8 reserved;);
DEF_RPACKET_DATA(GLOBAL, AB_DESCRIPTION, __u8 rev; __u8 reserved[3];
		 struct unit_descriptor unit_descriptor[NUM_UNITS];);
DEF_RPACKET_DATA(GLOBAL, REGISTER_REQUEST, reg_cmd_t reg_cmd;);
DEF_RPACKET_DATA(GLOBAL, PCM_WRITE, xpp_line_t lines; __u8 pcm[PCM_CHUNKSIZE];);
DEF_RPACKET_DATA(GLOBAL, PCM_READ, xpp_line_t lines; __u8 pcm[PCM_CHUNKSIZE];);
DEF_RPACKET_DATA(GLOBAL, SYNC_SOURCE, __u8 sync_mode; __u8 drift;);
DEF_RPACKET_DATA(GLOBAL, SYNC_REPLY, __u8 sync_mode; __u8 drift;);
DEF_RPACKET_DATA(GLOBAL, REGISTER_REPLY, reg_cmd_t regcmd;);
DEF_RPACKET_DATA(GLOBAL, XBUS_RESET, __u8 mask;);
DEF_RPACKET_DATA(GLOBAL, ERROR_CODE, __u8 category_code; __u8 errorbits;
		 __u8 bad_packet[0];);

/* 0x07 */ DECLARE_CMD(GLOBAL, AB_REQUEST);
/* 0x19 */ DECLARE_CMD(GLOBAL, SYNC_SOURCE, enum sync_mode mode, int drift);
/* 0x23 */ DECLARE_CMD(GLOBAL, RESET_SPI);
/* 0x23 */ DECLARE_CMD(GLOBAL, RESET_SYNC_COUNTERS);

int xpp_register_request(xbus_t *xbus, xpd_t *xpd, xportno_t portno,
			 bool writing, __u8 regnum, bool do_subreg, __u8 subreg,
			 __u8 data_low, bool do_datah, __u8 data_high,
			 bool should_reply, bool do_expander);
int send_multibyte_request(xbus_t *xbus, unsigned unit, xportno_t portno,
			   bool eoftx, __u8 *buf, unsigned len);
int xpp_ram_request(xbus_t *xbus, xpd_t *xpd, xportno_t portno,
			 bool writing,
			__u8 addr_low,
			__u8 addr_high,
			__u8 data_0,
			__u8 data_1,
			__u8 data_2,
			__u8 data_3,
			 bool should_reply);
extern xproto_table_t PROTO_TABLE(GLOBAL);
int run_initialize_registers(xpd_t *xpd);
int parse_chip_command(xpd_t *xpd, char *cmdline);
extern charp initdir;

#endif /* CARD_GLOBAL_H */
