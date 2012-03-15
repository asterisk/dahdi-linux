#ifndef	ASTRIBANK_USB_H
#define	ASTRIBANK_USB_H
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

#include <stdio.h>
#include <xusb.h>
#include <xtalk.h>
#include "mpp.h"

/*
 * Astribank handling
 */

#define	PACKET_SIZE	512

/* USB Endpoints */
#define	MP_EP_OUT	0x04	/* Managment processor */
#define	MP_EP_IN	0x88	/* Managment processor */

#define	XPP_EP_OUT	0x02	/* XPP */
#define	XPP_EP_IN	0x86	/* XPP */

/* USB firmware types */
#define	USB_11xx	0
#define	USB_FIRMWARE_II	1
#define	USB_PIC		2

struct interface_type {
	int	type_code;
	int	num_interfaces;
	int	my_interface_num;
	int	num_endpoints;
	int	my_ep_out;
	int	my_ep_in;
	char	*name;
	int	endpoints[4];	/* for matching */
};

enum eeprom_burn_state {
	BURN_STATE_NONE		= 0,
	BURN_STATE_STARTED	= 1,
	BURN_STATE_ENDED	= 2,
	BURN_STATE_FAILED	= 3,
};

struct astribank_device {
	struct xusb		*xusb;
	struct xtalk_device	*xtalk_dev;
	usb_dev_handle		*handle;
	int			my_interface_num;
	int			my_ep_out;
	int			my_ep_in;
	char			iInterface[BUFSIZ];
	int			is_usb2;
	enum eeprom_type	eeprom_type;
	enum eeprom_burn_state	burn_state;
	uint8_t			status;
	uint8_t			mpp_proto_version;
	struct eeprom_table	*eeprom;
	struct firmware_versions	fw_versions;
	uint16_t		tx_sequenceno;
};

/*
 * Prototypes
 */
struct astribank_device	*astribank_open(const char devpath[], int iface_num);
void astribank_close(struct astribank_device *astribank, int disconnected);
void show_astribank_info(const struct astribank_device *astribank);
int send_usb(struct astribank_device *astribank, char *buf, int len, int timeout);
int recv_usb(struct astribank_device *astribank, char *buf, size_t len, int timeout);
int flush_read(struct astribank_device *astribank);
int eeprom_fill(struct eeprom_table *eprm,
		const char *vendor,
		const char *product,
		const char *release,
		const char *label);
int astribank_has_twinstar(struct astribank_device *astribank);
int label_isvalid(const char *label);

#define	AB_REPORT(report_type, astribank, fmt, ...) \
	report_type("%s [%s]: " fmt, \
		xusb_devpath((astribank)->xusb), \
		xusb_serial((astribank)->xusb), \
		## __VA_ARGS__)

#define	AB_INFO(astribank, fmt, ...) \
		AB_REPORT(INFO, astribank, fmt, ## __VA_ARGS__)

#define	AB_ERR(astribank, fmt, ...) \
		AB_REPORT(ERR, astribank, fmt, ## __VA_ARGS__)

#endif	/* ASTRIBANK_USB_H */
