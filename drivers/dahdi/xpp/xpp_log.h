#ifndef XPP_LOG_H
#define XPP_LOG_H
/*
 * Written by Alexander Landau <landau.alex@gmail.com>
 * Copyright (C) 2004-2007, Xorcom
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

#ifdef	__KERNEL__

#include <linux/kernel.h>
#include <linux/version.h>

#else

/* This is to enable user-space programs to include this. */

#include "xdefs.h"

#endif

#define XPP_LOG_MAGIC	0x10583ADE

struct log_global_header {
	__u32 magic;
	__u32 version;
} __attribute__((packed));

struct log_header {
	__u32 len;
	__u32 time;
	__u8  xpd_num;
	__u8  direction;
} __attribute__((packed));

#endif
