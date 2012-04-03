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

/**
 * struct dahdi_dynamic_loc - For local dynamic spans
 * @monitor_rx_peer:   Indicates the peer span that monitors this span.
 * @peer:	       Indicates the rw peer for this span.
 *
 */
struct dahdi_dynamic_local {
	unsigned short key;
	unsigned short id;
	struct dahdi_dynamic_local *monitor_rx_peer;
	struct dahdi_dynamic_local *peer;
	struct dahdi_span *span;
	struct list_head node;
};

static DEFINE_SPINLOCK(local_lock);
static LIST_HEAD(dynamic_local_list);

static void
dahdi_dynamic_local_transmit(struct dahdi_dynamic *dyn, u8 *msg, size_t msglen)
{
	struct dahdi_dynamic_local *d;
	unsigned long flags;

	spin_lock_irqsave(&local_lock, flags);
	d = dyn->pvt;
	if (d && d->peer && d->peer->span) {
		if (test_bit(DAHDI_FLAGBIT_REGISTERED, &d->peer->span->flags))
			dahdi_dynamic_receive(d->peer->span, msg, msglen);
	}
	if (d && d->monitor_rx_peer && d->monitor_rx_peer->span) {
		if (test_bit(DAHDI_FLAGBIT_REGISTERED,
			     &d->monitor_rx_peer->span->flags))  {
			dahdi_dynamic_receive(d->monitor_rx_peer->span,
					      msg, msglen);
		}
	}
	spin_unlock_irqrestore(&local_lock, flags);
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

static void dahdi_dynamic_local_destroy(struct dahdi_dynamic *dyn)
{
	struct dahdi_dynamic_local *d;
	unsigned long flags;
	struct dahdi_dynamic_local *cur;

	spin_lock_irqsave(&local_lock, flags);
	d = dyn->pvt;
	list_for_each_entry(cur, &dynamic_local_list, node) {
		if (cur->peer == d)
			cur->peer = NULL;
		if (cur->monitor_rx_peer == d)
			cur->monitor_rx_peer = NULL;
	}
	list_del(&d->node);
	dyn->pvt = NULL;
	spin_unlock_irqrestore(&local_lock, flags);

	printk(KERN_INFO "TDMoL: Removed interface for %s, key %d "
		"id %d\n", d->span->name, d->key, d->id);
	kfree(d);
}

static int dahdi_dynamic_local_create(struct dahdi_dynamic *dyn,
				      const char *address)
{
	struct dahdi_dynamic_local *d, *l;
	unsigned long flags;
	int key = -1, id = -1, monitor = -1;
	struct dahdi_span *const span = &dyn->span;

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

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->key = key;
	d->id = id;
	d->span = span;

	spin_lock_irqsave(&local_lock, flags);
	/* Add this peer to any existing spans with same key
	   And add them as peers to this one */
	list_for_each_entry(l, &dynamic_local_list, node) {
		if (l->key != d->key)
			continue;

		if (l->id == d->id) {
			printk(KERN_DEBUG "TDMoL: Duplicate id (%d) for key "
				"%d\n", d->id, d->key);
			goto CLEAR_AND_DEL_FROM_PEERS;
		}
		if (monitor == -1) {
			if (l->peer) {
				printk(KERN_DEBUG "TDMoL: Span with key %d and "
					"id %d already has a R/W peer\n",
					d->key, d->id);
				goto CLEAR_AND_DEL_FROM_PEERS;
			} else {
				l->peer = d;
				d->peer = l;
			}
		}
		if (monitor == l->id) {
			if (l->monitor_rx_peer) {
				printk(KERN_DEBUG "TDMoL: Span with key %d and "
					"id %d already has a monitoring peer\n",
					d->key, d->id);
				goto CLEAR_AND_DEL_FROM_PEERS;
			} else {
				l->monitor_rx_peer = d;
			}
		}
	}
	list_add(&d->node, &dynamic_local_list);
	dyn->pvt = d;
	spin_unlock_irqrestore(&local_lock, flags);

	printk(KERN_INFO "TDMoL: Added new interface for %s, "
	       "key %d id %d\n", span->name, d->key, d->id);

	span->cannot_provide_timing = 1;
	return 0;

CLEAR_AND_DEL_FROM_PEERS:
	list_for_each_entry(l, &dynamic_local_list, node) {
		if (l->peer == d)
			l->peer = NULL;
		if (l->monitor_rx_peer == d)
			l->monitor_rx_peer = NULL;
	}
	kfree(d);
	spin_unlock_irqrestore(&local_lock, flags);
	return -EINVAL;
	
INVALID_ADDRESS:
	printk (KERN_NOTICE "TDMoL: Invalid address %s\n", address);
	return -EINVAL;
}

static struct dahdi_dynamic_driver dahdi_dynamic_local = {
	.owner = THIS_MODULE,
	.name = "loc",
	.desc = "Local",
	.create = dahdi_dynamic_local_create,
	.destroy = dahdi_dynamic_local_destroy,
	.transmit = dahdi_dynamic_local_transmit,
};

static int __init dahdi_dynamic_local_init(void)
{
	dahdi_dynamic_register_driver(&dahdi_dynamic_local);
	return 0;
}

static void __exit dahdi_dynamic_local_exit(void)
{
	dahdi_dynamic_unregister_driver(&dahdi_dynamic_local);
}

module_init(dahdi_dynamic_local_init);
module_exit(dahdi_dynamic_local_exit);

MODULE_LICENSE("GPL v2");
