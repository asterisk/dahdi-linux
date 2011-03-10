#ifndef	MPP_FUNCS_H
#define	MPP_FUNCS_H
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

#include <stdint.h>
#include <stdio.h>

#include "mpp.h"
#include "astribank_usb.h"

struct astribank_device;
struct eeprom_table;
struct extrainfo;
struct capabilities;
struct capkey;

#define	TIMEOUT	6000

/* high-level */
struct astribank_device *mpp_init(const char devpath[], int iface_num);
void mpp_exit(struct astribank_device *astribank);
int mpp_proto_query(struct astribank_device *astribank);
int mpp_status_query(struct astribank_device *astribank);
int mpp_eeprom_set(struct astribank_device *astribank, const struct eeprom_table *et);
int mpp_renumerate(struct astribank_device *astribank);
int mpp_caps_get(struct astribank_device *astribank,
		struct eeprom_table *et,
		struct capabilities *cap,
		struct capkey *key);
int mpp_caps_set(struct astribank_device *astribank,
		const struct eeprom_table *eeprom_table,
		const struct capabilities *capabilities,
		const struct capkey *key);
int mpp_extrainfo_get(struct astribank_device *astribank, struct extrainfo *info);
int mpp_extrainfo_set(struct astribank_device *astribank, const struct extrainfo *info);
int mpp_eeprom_blk_rd(struct astribank_device *astribank, uint8_t *buf, uint16_t offset, uint16_t len);
int mpp_send_start(struct astribank_device *astribank, int dest, const char *ihex_version);
int mpp_send_end(struct astribank_device *astribank);
int mpp_send_seg(struct astribank_device *astribank, const uint8_t *data, uint16_t offset, uint16_t len);
int mpp_reset(struct astribank_device *astribank, int full_reset);
int mpp_serial_cmd(struct astribank_device *astribank, const uint8_t *in, uint8_t *out, uint16_t len);
void show_eeprom(const struct eeprom_table *eprm, FILE *fp);
void show_capabilities(const struct capabilities *capabilities, FILE *fp);
void show_astribank_status(struct astribank_device *astribank, FILE *fp);
void show_extrainfo(const struct extrainfo *extrainfo, FILE *fp);
int twinstar_show(struct astribank_device *astribank, FILE *fp);

/*
 * Serial commands to FPGA
 */
int mpps_card_info(struct astribank_device *astribank, int unit, uint8_t *card_type, uint8_t *card_status);

/*
 * Twinstar
 */
int mpp_tws_watchdog(struct astribank_device *astribank);
int mpp_tws_setwatchdog(struct astribank_device *astribank, int yes);
int mpp_tws_powerstate(struct astribank_device *astribank);
int mpp_tws_portnum(struct astribank_device *astribank);
int mpp_tws_setportnum(struct astribank_device *astribank, uint8_t portnum);

const char *dev_dest2str(int dest);

#endif	/* MPP_FUNCS_H */
