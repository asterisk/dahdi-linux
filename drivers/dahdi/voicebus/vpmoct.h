/*
 * VPMOCT Driver.
 *
 * Written by Russ Meyerriecks <rmeyerriecks@digium.com>
 *
 * Copyright (C) 2010-2011 Digium, Inc.
 *
 * All rights reserved.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#ifndef _VPMOCT_H
#define _VPMOCT_H

#include <linux/list.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include "dahdi/kernel.h"

#include <stdbool.h>

#define VPMOCT_FIRM_HEADER_LEN 32
#define VPMOCT_BOOT_RAM_LEN 128
#define VPMOCT_FLASH_BUF_SECTIONS 4
#define VPMOCT_MAX_CHUNK 7

/* Bootloader commands */
#define VPMOCT_BOOT_FLASH_ERASE 0x01
#define VPMOCT_BOOT_FLASH_COPY 0x02
#define VPMOCT_BOOT_IMAGE_VALIDATE 0x06
#define VPMOCT_BOOT_REBOOT 0x07
#define VPMOCT_BOOT_DECRYPT 0x08
#define VPMOCT_BOOT_FLASHLOAD 0x10

/* Dual use registers */
#define VPMOCT_IDENT 0x00
#define VPMOCT_MAJOR 0x0a
#define VPMOCT_MINOR 0x0b
#define VPMOCT_SERIAL 0x90
#define VPMOCT_SERIAL_SIZE 32

/* Bootloader registers */
#define VPMOCT_BOOT_ERROR 0x0c
#define VPMOCT_BOOT_STATUS 0x10
#define VPMOCT_BOOT_CMD 0x11
#define VPMOCT_BOOT_LEN 0x14
#define VPMOCT_BOOT_ADDRESS1 0x18
#define VPMOCT_BOOT_ADDRESS2 0x1c
#define VPMOCT_BOOT_RAM 0x20

enum vpmoct_mode { UNKNOWN = 0, APPLICATION, BOOTLOADER };

struct vpmoct {
	struct list_head pending_list;
	struct list_head active_list;
	spinlock_t list_lock;
	struct mutex mutex;
	enum vpmoct_mode mode;
	struct device *dev;
	u32 companding;
	u32 echo;
	unsigned int preecho_enabled:1;
	unsigned int echo_update_active:1;
	unsigned int companding_update_active:1;
	u8 preecho_timeslot;
	u8 preecho_buf[8];
	u8 major;
	u8 minor;
};

struct vpmoct_cmd {
	struct list_head node;
	u8 address;
	u8 data[VPMOCT_MAX_CHUNK];
	u8 command;
	u8 chunksize;
	u8 txident;
	struct completion complete;
};

static inline bool is_vpmoct_cmd_read(const struct vpmoct_cmd *cmd)
{
	return (0x60 == (cmd->command & 0xf0));
}

struct vpmoct *vpmoct_alloc(void);
void vpmoct_free(struct vpmoct *vpm);
typedef void (*load_complete_func_t)(struct device *dev, bool operational);
int vpmoct_init(struct vpmoct *vpm, load_complete_func_t load_complete);
int vpmoct_echocan_create(struct vpmoct *vpm,
			   int channo,
			   int companding);
void vpmoct_echocan_free(struct vpmoct *vpm,
			 int channo);
int vpmoct_preecho_enable(struct vpmoct *vpm, int channo);
int vpmoct_preecho_disable(struct vpmoct *vpm, int channo);
#endif
