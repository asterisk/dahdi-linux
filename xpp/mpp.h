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
	uint8_t		reserved[3];
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
	char		text[24];
} PACKED;

enum mpp_command_ops {
	/* MSB of op signifies a reply from device */
	MPP_ACK			= 0x80,

	MPP_PROTO_QUERY	= 0x01,
	MPP_PROTO_REPLY	= 0x81,

	MPP_RENUM		= 0x0B,	/* Trigger USB renumeration */

	MPP_EEPROM_SET		= 0x0D,

	MPP_CAPS_GET		= 0x0E,
	MPP_CAPS_GET_REPLY	= 0x8E,
	MPP_CAPS_SET		= 0x0F,	/* Set AB capabilities	*/

	MPP_DEV_SEND_START	= 0x05,
	MPP_DEV_SEND_SEG	= 0x07,
	MPP_DEV_SEND_END	= 0x09,

	MPP_STATUS_GET		= 0x11,	/* Get Astribank Status	*/
	MPP_STATUS_GET_REPLY	= 0x91,
	MPP_STATUS_GET_REPLY_V13	= 0x91,	/* backward compat */

	MPP_EXTRAINFO_GET	= 0x13,	/* Get extra vendor information	*/
	MPP_EXTRAINFO_GET_REPLY	= 0x93,
	MPP_EXTRAINFO_SET	= 0x15,	/* Set extra vendor information	*/

	MPP_EEPROM_BLK_RD	= 0x27,
	MPP_EEPROM_BLK_RD_REPLY	= 0xA7,

	MPP_SER_SEND		= 0x37,
	MPP_SER_RECV		= 0xB7,

	MPP_RESET		= 0x45,	/* Reset both FPGA and USB firmwares */
	MPP_HALF_RESET		= 0x47,	/* Reset only FPGA firmware */

	/* Twinstar */
	MPP_TWS_WD_MODE_SET	= 0x31,	/* Set watchdog off/on guard	*/
	MPP_TWS_WD_MODE_GET	= 0x32,	/* Current watchdog mode 	*/
	MPP_TWS_WD_MODE_GET_REPLY = 0xB2,	/* Current watchdog mode 	*/
	MPP_TWS_PORT_SET	= 0x34,	/* USB-[0/1]			*/
	MPP_TWS_PORT_GET	= 0x35,	/* USB-[0/1]			*/
	MPP_TWS_PORT_GET_REPLY	= 0xB5,	/* USB-[0/1]			*/
	MPP_TWS_PWR_GET		= 0x36,	/* Power: bits -> USB ports	*/
	MPP_TWS_PWR_GET_REPLY	= 0xB6,	/* Power: bits -> USB ports	*/
};

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

#define	CMD_DEF(name, ...)	struct d_ ## name { __VA_ARGS__ } PACKED d_ ## name

CMD_DEF(ACK,
	uint8_t	stat;
	);

CMD_DEF(PROTO_QUERY,
	uint8_t	proto_version;
	uint8_t	reserved;
	);

CMD_DEF(PROTO_REPLY,
	uint8_t	proto_version;
	uint8_t	reserved;
	);

CMD_DEF(STATUS_GET);

CMD_DEF(STATUS_GET_REPLY_V13,
	uint8_t	i2cs_data;

#define	STATUS_FPGA_LOADED(x)	((x) & 0x01)
	uint8_t	status;		/* BIT(0) - FPGA is loaded */
	);


CMD_DEF(STATUS_GET_REPLY,
	uint8_t	i2cs_data;

#define	STATUS_FPGA_LOADED(x)	((x) & 0x01)
	uint8_t	status;		/* BIT(0) - FPGA is loaded */
	struct firmware_versions fw_versions;
	);

CMD_DEF(EEPROM_SET,
	struct eeprom_table	data;
	);

CMD_DEF(CAPS_GET);

CMD_DEF(CAPS_GET_REPLY,
	struct eeprom_table	data;
	struct capabilities	capabilities;
	struct capkey		key;
	);

CMD_DEF(CAPS_SET,
	struct eeprom_table	data;
	struct capabilities	capabilities;
	struct capkey		key;
	);

CMD_DEF(EXTRAINFO_GET);

CMD_DEF(EXTRAINFO_GET_REPLY,
	struct extrainfo	info;
	);

CMD_DEF(EXTRAINFO_SET,
	struct extrainfo	info;
	);

CMD_DEF(RENUM);

CMD_DEF(EEPROM_BLK_RD,
	uint16_t	offset;
	uint16_t	len;
	);

CMD_DEF(EEPROM_BLK_RD_REPLY,
	uint16_t	offset;
	uint8_t		data[0];
	);

CMD_DEF(DEV_SEND_START,
	uint8_t		dest;
	char		ihex_version[VERSION_LEN];
	);

CMD_DEF(DEV_SEND_END);

CMD_DEF(DEV_SEND_SEG,
	uint16_t	offset;
	uint8_t		data[0];
	);

CMD_DEF(RESET);
CMD_DEF(HALF_RESET);

CMD_DEF(SER_SEND,
	uint8_t	data[0];
	);

CMD_DEF(SER_RECV,
	uint8_t	data[0];
	);

CMD_DEF(TWS_WD_MODE_SET,
	uint8_t		wd_active;
	);

CMD_DEF(TWS_WD_MODE_GET);
CMD_DEF(TWS_WD_MODE_GET_REPLY,
	uint8_t		wd_active;
	);

CMD_DEF(TWS_PORT_SET,
	uint8_t		portnum;
	);

CMD_DEF(TWS_PORT_GET);
CMD_DEF(TWS_PORT_GET_REPLY,
	uint8_t		portnum;
	);

CMD_DEF(TWS_PWR_GET);
CMD_DEF(TWS_PWR_GET_REPLY,
	uint8_t		power;
	);

#undef	CMD_DEF

#define	MEMBER(n)	struct d_ ## n d_ ## n

struct mpp_command {
	struct mpp_header	header;
	union {
		MEMBER(ACK);
		MEMBER(PROTO_QUERY);
		MEMBER(PROTO_REPLY);
		MEMBER(STATUS_GET);
		MEMBER(STATUS_GET_REPLY_V13);
		MEMBER(STATUS_GET_REPLY);
		MEMBER(EEPROM_SET);
		MEMBER(CAPS_GET);
		MEMBER(CAPS_GET_REPLY);
		MEMBER(CAPS_SET);
		MEMBER(EXTRAINFO_GET);
		MEMBER(EXTRAINFO_GET_REPLY);
		MEMBER(EXTRAINFO_SET);
		MEMBER(RENUM);
		MEMBER(EEPROM_BLK_RD);
		MEMBER(EEPROM_BLK_RD_REPLY);
		MEMBER(DEV_SEND_START);
		MEMBER(DEV_SEND_SEG);
		MEMBER(DEV_SEND_END);
		MEMBER(RESET);
		MEMBER(HALF_RESET);
		MEMBER(SER_SEND);
		MEMBER(SER_RECV);
		/* Twinstar */
		MEMBER(TWS_WD_MODE_SET);
		MEMBER(TWS_WD_MODE_GET);
		MEMBER(TWS_WD_MODE_GET_REPLY);
		MEMBER(TWS_PORT_SET);
		MEMBER(TWS_PORT_GET);
		MEMBER(TWS_PORT_GET_REPLY);
		MEMBER(TWS_PWR_GET);
		MEMBER(TWS_PWR_GET_REPLY);
		uint8_t	raw_data[0];
	} PACKED alt;
} PACKED;
#undef MEMBER

#define	CMD_FIELD(cmd, name, field)	((cmd)->alt.d_ ## name.field)

enum mpp_ack_stat {
	STAT_OK		= 0x00,	/* acknowledges previous command	*/
	STAT_FAIL	= 0x01,	/* Last command failed		*/
	STAT_RESET_FAIL	= 0x02,	/* reset failed				*/
	STAT_NODEST	= 0x03,	/* No destination is selected		*/
	STAT_MISMATCH	= 0x04,	/* Data mismatch			*/
	STAT_NOACCESS	= 0x05,	/* No access				*/
	STAT_BAD_CMD	= 0x06,	/* Bad command				*/
	STAT_TOO_SHORT	= 0x07,	/* Packet is too short			*/
	STAT_ERROFFS	= 0x08,	/* Offset error				*/
	STAT_NOCODE	= 0x09,	/* Source was not burned before		*/
	STAT_NO_LEEPROM	= 0x0A,	/* Large EEPROM was not found		*/
	STAT_NO_EEPROM	= 0x0B,	/* No EEPROM was found			*/
	STAT_WRITE_FAIL	= 0x0C,	/* Writing to device failed		*/
	STAT_FPGA_ERR	= 0x0D,	/* FPGA error				*/
	STAT_KEY_ERR	= 0x0E,	/* Bad Capabilities Key			*/
	STAT_NOCAPS_ERR	= 0x0F,	/* No matching capability		*/
	STAT_NOPWR_ERR	= 0x10,	/* No power on USB connector		*/
	STAT_CAPS_FPGA_ERR	= 0x11,	/* Setting of the capabilities while FPGA is loaded */
};

enum eeprom_type {	/* EEPROM_QUERY: i2cs(ID1, ID0) */
	EEPROM_TYPE_NONE	= 0,
	EEPROM_TYPE_SMALL	= 1,
	EEPROM_TYPE_LARGE	= 2,
	EEPROM_TYPE_UNUSED	= 3,
};

enum dev_dest {
	DEST_NONE	= 0x00,
	DEST_FPGA	= 0x01,
	DEST_EEPROM	= 0x02,
};

#endif	/* MPP_H */
