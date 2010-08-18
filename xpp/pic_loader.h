#ifndef	PIC_LOADER_H
#define	PIC_LOADER_H
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
#include "astribank_usb.h"

/*
 * Astribank PIC loading
 */

enum pic_command {
	PIC_DATA_FLAG	= 0x00,
	PIC_START_FLAG	= 0x01,
	PIC_END_FLAG	= 0x02,
	PIC_ENDS_FLAG	= 0x04,
};

#define	PIC_PACK_LEN 	0x0B
#define PIC_LINE_LEN	0x03

int send_picline(struct astribank_device *astribank, uint8_t card_type,
		enum pic_command pcmd, int offs, uint8_t *data, int data_len);
int load_pic(struct astribank_device *astribank, int numfiles, char *filelist[]);

#endif	/* PIC_LOADER_H */
