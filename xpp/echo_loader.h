#ifndef	ECHO_LOADER_H
#define	ECHO_LOADER_H
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

int spi_send(struct astribank_device *astribank, uint16_t addr, uint16_t data, int recv_answer, int ver);
int load_echo(struct astribank_device *astribank, char *filename, int is_alaw);
int echo_ver(struct astribank_device *astribank);

#endif	/* ECHO_LOADER_H */
