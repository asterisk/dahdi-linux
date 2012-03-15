#ifndef	MPPTALK_DEFS_H
#define	MPPTALK_DEFS_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2008,2009,2010 Xorcom
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

#include <xtalk_defs.h>
/*
 * MPP - Managment Processor Protocol definitions
 */

/*
 * OP Codes:
 * MSB of op signifies a reply from device
 */
#define	MPP_RENUM			0x0B	/* Trigger USB renumeration */
#define	MPP_EEPROM_SET			0x0D

/* AB capabilities	*/
#define	MPP_CAPS_GET			0x0E
#define	MPP_CAPS_GET_REPLY		0x8E
#define	MPP_CAPS_SET			0x0F

#define	MPP_DEV_SEND_START		0x05
#define	MPP_DEV_SEND_SEG		0x07
#define	MPP_DEV_SEND_END		0x09

/* Astribank Status	*/
#define	MPP_STATUS_GET			0x11
#define	MPP_STATUS_GET_REPLY		0x91
#define	MPP_STATUS_GET_REPLY_V13	0x91	/* backward compat */

/* Get extra vendor information	*/
#define	MPP_EXTRAINFO_GET		0x13
#define	MPP_EXTRAINFO_GET_REPLY		0x93
#define	MPP_EXTRAINFO_SET		0x15	/* Set extra vendor information	*/

#define	MPP_EEPROM_BLK_RD		0x27
#define	MPP_EEPROM_BLK_RD_REPLY		0xA7

#define	MPP_SER_SEND			0x37
#define	MPP_SER_RECV			0xB7

#define	MPP_RESET			0x45	/* Reset both FPGA and USB firmwares */
#define	MPP_HALF_RESET			0x47	/* Reset only FPGA firmware */

/* Twinstar */
#define	MPP_TWS_WD_MODE_SET		0x31	/* Set watchdog off/on guard	*/
#define	MPP_TWS_WD_MODE_GET		0x32	/* Current watchdog mode 	*/
#define	MPP_TWS_WD_MODE_GET_REPLY	0xB2	/* Current watchdog mode 	*/
#define	MPP_TWS_PORT_SET		0x34	/* USB-[0/1]			*/
#define	MPP_TWS_PORT_GET		0x35	/* USB-[0/1]			*/
#define	MPP_TWS_PORT_GET_REPLY		0xB5	/* USB-[0/1]			*/
#define	MPP_TWS_PWR_GET			0x36	/* Power: bits -> USB ports	*/
#define	MPP_TWS_PWR_GET_REPLY		0xB6	/* Power: bits -> USB ports	*/

/*
 * Statuses
 */
#define	STAT_OK		0x00	/* acknowledges previous command	*/
#define	STAT_FAIL	0x01	/* Last command failed		*/
#define	STAT_RESET_FAIL	0x02	/* reset failed				*/
#define	STAT_NODEST	0x03	/* No destination is selected		*/
#define	STAT_MISMATCH	0x04	/* Data mismatch			*/
#define	STAT_NOACCESS	0x05	/* No access				*/
#define	STAT_BAD_CMD	0x06	/* Bad command				*/
#define	STAT_TOO_SHORT	0x07	/* Packet is too short			*/
#define	STAT_ERROFFS	0x08	/* Offset error				*/
#define	STAT_NOCODE	0x09	/* Source was not burned before		*/
#define	STAT_NO_LEEPROM	0x0A	/* Large EEPROM was not found		*/
#define	STAT_NO_EEPROM	0x0B	/* No EEPROM was found			*/
#define	STAT_WRITE_FAIL	0x0C	/* Writing to device failed		*/
#define	STAT_FPGA_ERR	0x0D	/* FPGA error				*/
#define	STAT_KEY_ERR	0x0E	/* Bad Capabilities Key			*/
#define	STAT_NOCAPS_ERR	0x0F	/* No matching capability		*/
#define	STAT_NOPWR_ERR	0x10	/* No power on USB connector		*/
#define	STAT_CAPS_FPGA_ERR	0x11	/* Setting of the capabilities while FPGA is loaded */

/* EEPROM_QUERY: i2cs(ID1, ID0) */
enum eeprom_type {
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

#define	EXTRAINFO_SIZE	24

#endif	/* MPPTALK_DEFS_H */
