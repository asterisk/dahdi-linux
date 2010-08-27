/*
 * Dynamic Span Interface for DAHDI (Local Interface)
 *
 * Written by Nicolas Bougues <nbougues@axialys.net>
 *
 * Copyright (C) 2004, Axialys Interactive
 *
 * All rights reserved.
 *
 * Note : a DAHDI timing source must exist prior to loading this driver
 *
 * Address syntax : 
 * <key>:<id>[:<monitor id>]
 *
 * As of now, keys and ids are single digit only
 *
 * One span may have up to one "normal" peer, and one "monitor" peer
 * 
 * Example :
 * 
 * Say you have two spans cross connected, a third one monitoring RX on the 
 * first one, a fourth one monitoring RX on the second one
 *
 *   1:0
 *   1:1
 *   1:2:0
 *   1:3:1
 * 
 * Contrary to TDMoE, no frame loss can occur.
 *
 * See bug #2021 for more details
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/netdevice.h>
#include <linux/notifier.h>

#include <dahdi/kernel.h>

#ifdef DEFINE_SPINLOCK
static DEFINE_SPINLOCK(zlock);
#else
static spinlock_t zlock = SPIN_LOCK_UNLOCKED;
#endif

static struct ztdlocal {
	unsigned short key;
	unsigned short id;
	struct ztdlocal *monitor_rx_peer; /* Indicates the peer span that monitors this span */
	struct ztdlocal *peer; /* Indicates the rw peer for this span */
	struct dahdi_span *span;
	struct ztdlocal *next;
} *zdevs = NULL;

static int ztdlocal_transmit(void *pvt, unsigned char *msg, int msglen)
{
	struct ztdlocal *z;
	unsigned long flags;

	spin_lock_irqsave(&zlock, flags);
	z = pvt;
	if (z->peer && z->peer->span) {
		dahdi_dynamic_receive(z->peer->span, msg, msglen);
	}
	if (z->monitor_rx_peer && z->monitor_rx_peer->span) {
		dahdi_dynamic_receive(z->monitor_rx_peer->span, msg, msglen);
	}
	spin_unlock_irqrestore(&zlock, flags);
	return 0;
}

static int digit2int(char d)
{
	switch(d) {
	case 'F':
	case 'E':
	case 'D':
	case 'C':
	case 'B':
	case 'A':
		return d - 'A' + 10;
	case 'f':
	case 'e':
	case 'd':
	case 'c':
	case 'b':
	case 'a':
		return d - 'a' + 10;
	case '9':
	case '8':
	case '7':
	case '6':
	case '5':
	case '4':
	case '3':
	case '2':
	case '1':
	case '0':
		return d - '0';
	}
	return -1;
}

static void ztdlocal_destroy(void *pvt)
{
	struct ztdlocal *z = pvt;
	unsigned long flags;
	struct ztdlocal *prev=NULL, *cur;

	spin_lock_irqsave(&zlock, flags);
	cur = zdevs;
	while(cur) {
		if (cur->peer == z)
			cur->peer = NULL;
		if (cur->monitor_rx_peer == z)
			cur->monitor_rx_peer = NULL;
		cur = cur->next;
	}
	cur = zdevs;
	while(cur) {
		if (cur == z) {
			if (prev)
				prev->next = cur->next;
			else
				zdevs = cur->next;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	spin_unlock_irqrestore(&zlock, flags);
	if (cur == z) {
		printk(KERN_INFO "TDMoL: Removed interface for %s, key %d id %d\n", z->span->name, z->key, z->id);
		module_put(THIS_MODULE);
		kfree(z);
	}
}

static void *ztdlocal_create(struct dahdi_span *span, char *address)
{
	struct ztdlocal *z, *l;
	unsigned long flags;
	int key = -1, id = -1, monitor = -1;

	if (strlen(address) >= 3) {
		if (address[1] != ':')
			goto INVALID_ADDRESS;
		key = digit2int(address[0]);
		id = digit2int(address[2]);
	} 
	if (strlen (address) == 5) {
		if (address[3] != ':')
			goto INVALID_ADDRESS;
		monitor = digit2int(address[4]);
	}

	if (key == -1 || id == -1)
		goto INVALID_ADDRESS;

	z = kmalloc(sizeof(struct ztdlocal), GFP_KERNEL);
	if (z) {
		/* Zero it out */
		memset(z, 0, sizeof(struct ztdlocal));

		z->key = key;
		z->id = id;
		z->span = span;
			
		spin_lock_irqsave(&zlock, flags);
		/* Add this peer to any existing spans with same key
		   And add them as peers to this one */
		for (l = zdevs; l; l = l->next)
			if (l->key == z->key) {
				if (l->id == z->id) {
					printk(KERN_DEBUG "TDMoL: Duplicate id (%d) for key %d\n", z->id, z->key);
					goto CLEAR_AND_DEL_FROM_PEERS;
				}
				if (monitor == -1) {
					if (l->peer) {
						printk(KERN_DEBUG "TDMoL: Span with key %d and id %d already has a R/W peer\n", z->key, z->id);
						goto CLEAR_AND_DEL_FROM_PEERS;
					} else {
						l->peer = z;
						z->peer = l;
					}
				}
				if (monitor == l->id) {
					if (l->monitor_rx_peer) {
						printk(KERN_DEBUG "TDMoL: Span with key %d and id %d already has a monitoring peer\n", z->key, z->id);
						goto CLEAR_AND_DEL_FROM_PEERS;
					} else {
						l->monitor_rx_peer = z;
					}
				}
			}
		z->next = zdevs;
		zdevs = z;
		spin_unlock_irqrestore(&zlock, flags);
		if(!try_module_get(THIS_MODULE))
			printk(KERN_DEBUG "TDMoL: Unable to increment module use count\n");

		printk(KERN_INFO "TDMoL: Added new interface for %s, key %d id %d\n", span->name, z->key, z->id);
	}
	return z;

CLEAR_AND_DEL_FROM_PEERS:
	for (l = zdevs; l; l = l->next) {
		if (l->peer == z)
			l->peer = NULL;
		if (l->monitor_rx_peer == z)
			l->monitor_rx_peer = NULL;
	}
	kfree (z);
	spin_unlock_irqrestore(&zlock, flags);
	return NULL;
	
INVALID_ADDRESS:
	printk (KERN_NOTICE "TDMoL: Invalid address %s\n", address);
	return NULL;
}

static struct dahdi_dynamic_driver ztd_local = {
	"loc",
	"Local",
	ztdlocal_create,
	ztdlocal_destroy,
	ztdlocal_transmit,
	NULL	/* flush */
};

static int __init ztdlocal_init(void)
{
	dahdi_dynamic_register(&ztd_local);
	return 0;
}

static void __exit ztdlocal_exit(void)
{
	dahdi_dynamic_unregister(&ztd_local);
}

module_init(ztdlocal_init);
module_exit(ztdlocal_exit);

MODULE_LICENSE("GPL v2");
