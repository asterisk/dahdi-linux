#ifndef	MPP_H
#define	MPP_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2008, Xorcom
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

/*
 * MPP - Managment Processor Protocol definitions
 */

#include <mpptalk_defs.h>
#include <stdint.h>
#include <xtalk.h>

#ifdef	__GNUC__
#define	PACKED	__attribute__((packed))
#else
#error "We do not know how your compiler packs structures"
#endif

#define	MK_PROTO_VERSION(major, minor)	(((major) << 4) | (0x0F & (minor)))

#define	MPP_PROTOCOL_VERSION	MK_PROTO_VERSION(1,4)
#define	MPP_SUPPORTED_VERSION(x)	((x) == MK_PROTO_VERSION(1,3) || (x) == MK_PROTO_VERSION(1,4))

/*
 * The eeprom_table is common to all eeprom types.
 */
#define	LABEL_SIZE	8
struct eeprom_table {
	uint8_t		source;		/* C0 - small eeprom, C2 - large eeprom */
	uint16_t	vendor;
	uint16_t	product;
	uint16_t	release;	/* BCD encoded release */
	uint8_t		config_byte;	/* Must be 0 */
	uint8_t		label[LABEL_SIZE];
} PACKED;

#define	VERSION_LEN	6
struct firmware_versions {
	char	usb[VERSION_LEN];
	char	fpga[VERSION_LEN];
	char	eeprom[VERSION_LEN];
} PACKED;

struct capabilities {
	uint8_t		ports_fxs;
	uint8_t		ports_fxo;
	uint8_t		ports_bri;
	uint8_t		ports_pri;
	uint8_t		extra_features;	/* BIT(0) - TwinStar */
	uint8_t		ports_echo;
	uint8_t		reserved[2];
	uint32_t	timestamp;
} PACKED;

#define	CAP_EXTRA_TWINSTAR(c)		((c)->extra_features & 0x01)
#define	CAP_EXTRA_TWINSTAR_SET(c)	do {(c)->extra_features |= 0x01;} while (0)
#define	CAP_EXTRA_TWINSTAR_CLR(c)	do {(c)->extra_features &= ~0x01;} while (0)

#define	KEYSIZE	16

struct capkey {
	uint8_t	k[KEYSIZE];
} PACKED;

struct extrainfo {
	char		text[EXTRAINFO_SIZE];
} PACKED;

struct mpp_header {
	uint16_t	len;
	uint16_t	seq;
	uint8_t		op;	/* MSB: 0 - to device, 1 - from device */
} PACKED;

enum mpp_ser_op {
	SER_CARD_INFO_GET	= 0x1,
	SER_STAT_GET		= 0x3,
};

/* Individual commands structure */

CMD_DEF(MPP, STATUS_GET);


CMD_DEF(MPP, STATUS_GET_REPLY,
	uint8_t	i2cs_data;

#define	STATUS_FPGA_LOADED(x)	((x) & 0x01)
	uint8_t	status;		/* BIT(0) - FPGA is loaded */
	struct firmware_versions fw_versions;
	);

CMD_DEF(MPP, EEPROM_SET,
	struct eeprom_table	data;
	);

CMD_DEF(MPP, CAPS_GET);

CMD_DEF(MPP, CAPS_GET_REPLY,
	struct eeprom_table	data;
	struct capabilities	capabilities;
	struct capkey		key;
	);

CMD_DEF(MPP, CAPS_SET,
	struct eeprom_table	data;
	struct capabilities	capabilities;
	struct capkey		key;
	);

CMD_DEF(MPP, EXTRAINFO_GET);

CMD_DEF(MPP, EXTRAINFO_GET_REPLY,
	struct extrainfo	info;
	);

CMD_DEF(MPP, EXTRAINFO_SET,
	struct extrainfo	info;
	);

CMD_DEF(MPP, RENUM);

CMD_DEF(MPP, EEPROM_BLK_RD,
	uint16_t	offset;
	uint16_t	len;
	);

CMD_DEF(MPP, EEPROM_BLK_RD_REPLY,
	uint16_t	offset;
	uint8_t		data[0];
	);

CMD_DEF(MPP, DEV_SEND_START,
	uint8_t		dest;
	char		ihex_version[VERSION_LEN];
	);

CMD_DEF(MPP, DEV_SEND_END);

CMD_DEF(MPP, DEV_SEND_SEG,
	uint16_t	offset;
	uint8_t		data[0];
	);

CMD_DEF(MPP, RESET);
CMD_DEF(MPP, HALF_RESET);

CMD_DEF(MPP, SER_SEND,
	uint8_t	data[0];
	);

CMD_DEF(MPP, SER_RECV,
	uint8_t	data[0];
	);

CMD_DEF(MPP, TWS_WD_MODE_SET,
	uint8_t		wd_active;
	);

CMD_DEF(MPP, TWS_WD_MODE_GET);
CMD_DEF(MPP, TWS_WD_MODE_GET_REPLY,
	uint8_t		wd_active;
	);

CMD_DEF(MPP, TWS_PORT_SET,
	uint8_t		portnum;
	);

CMD_DEF(MPP, TWS_PORT_GET);
CMD_DEF(MPP, TWS_PORT_GET_REPLY,
	uint8_t		portnum;
	);

CMD_DEF(MPP, TWS_PWR_GET);
CMD_DEF(MPP, TWS_PWR_GET_REPLY,
	uint8_t		power;
	);

#endif	/* MPP_H */
