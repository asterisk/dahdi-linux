/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2007, Xorcom
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
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#  warning "This module is tested only with 2.6 kernels"
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/parport.h>
#include "parport_debug.h"

static struct parport	*debug_sync_parport = NULL;
static int		parport_toggles[8];	/* 8 bit flip-flop */

void flip_parport_bit(unsigned char bitnum)
{
	static unsigned char	last_value;
	DEFINE_SPINLOCK(lock);
	unsigned long	flags;
	unsigned char	mask;
	unsigned char	value;

	if(!debug_sync_parport) {
		if(printk_ratelimit()) {
			printk(KERN_NOTICE "%s: no debug parallel port\n",
				THIS_MODULE->name);
		}
		return;
	}
	BUG_ON(bitnum > 7);
	mask = 1 << bitnum;
	spin_lock_irqsave(&lock, flags);
	value = last_value & ~mask;
	if(parport_toggles[bitnum] % 2)	/* square wave */
		value |= mask;
	last_value = value;
	parport_toggles[bitnum]++;
	spin_unlock_irqrestore(&lock, flags);
	parport_write_data(debug_sync_parport, value);
}
EXPORT_SYMBOL(flip_parport_bit);

static void parport_attach(struct parport *port)
{
	printk(KERN_INFO "%s: Using %s for debugging\n", THIS_MODULE->name, port->name);
	if(debug_sync_parport) {
		printk(KERN_ERR "%s: Using %s, ignore new attachment %s\n",
			THIS_MODULE->name, debug_sync_parport->name, port->name);
		return;
	}
	parport_get_port(port);
	debug_sync_parport = port;
}

static void parport_detach(struct parport *port)
{
	printk(KERN_INFO "%s: Releasing %s\n", THIS_MODULE->name, port->name);
	if(debug_sync_parport != port) {
		printk(KERN_ERR "%s: Using %s, ignore new detachment %s\n",
			THIS_MODULE->name, debug_sync_parport->name, port->name);
		return;
	}
	parport_put_port(debug_sync_parport);
	debug_sync_parport = NULL;
}

static struct parport_driver	debug_parport_driver = {
	.name = "parport_debug",
	.attach = parport_attach,
	.detach = parport_detach,
};

int __init parallel_dbg_init(void)
{
	int	ret;

	ret = parport_register_driver(&debug_parport_driver);
	return ret;
}

void __exit parallel_dbg_cleanup(void)
{
	parport_unregister_driver(&debug_parport_driver);
}

MODULE_DESCRIPTION("Use parallel port to debug drivers");
MODULE_AUTHOR("Oron Peled <oron@actcom.co.il>");
MODULE_LICENSE("GPL");
MODULE_VERSION("$Id:");

module_init(parallel_dbg_init);
module_exit(parallel_dbg_cleanup);
