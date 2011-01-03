/*
 * Transcoder Interface for DAHDI
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2006-2008, Digium, Inc.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <asm/io.h>

#include <dahdi/kernel.h>

static int debug;
/* The registration list contains transcoders in the order in which they were
 * registered. */
static LIST_HEAD(registration_list);
/* The active list is sorted by the most recently used transcoder is last. This
 * is used as a simplistic way to spread the load amongst the different hardware
 * transcoders in the system. */
static LIST_HEAD(active_list);
static DEFINE_SPINLOCK(translock);

EXPORT_SYMBOL(dahdi_transcoder_register);
EXPORT_SYMBOL(dahdi_transcoder_unregister);
EXPORT_SYMBOL(dahdi_transcoder_alert);
EXPORT_SYMBOL(dahdi_transcoder_alloc);
EXPORT_SYMBOL(dahdi_transcoder_free);

struct dahdi_transcoder *dahdi_transcoder_alloc(int numchans)
{
	struct dahdi_transcoder *tc;
	unsigned int x;
	size_t size = sizeof(*tc) + (sizeof(tc->channels[0]) * numchans);

	if (!(tc = kmalloc(size, GFP_KERNEL)))
		return NULL;

	memset(tc, 0, size);
	strcpy(tc->name, "<unspecified>");
	INIT_LIST_HEAD(&tc->registration_list_node);
	INIT_LIST_HEAD(&tc->active_list_node);
	tc->numchannels = numchans;
	for (x=0; x < tc->numchannels; x++) {
		init_waitqueue_head(&tc->channels[x].ready);
		tc->channels[x].parent = tc;
	}

	WARN_ON(!dahdi_transcode_fops);
	/* Individual transcoders should supply their own file_operations for
	 * write and read.  But they will by default use the file_operations
	 * provided by the dahdi_transcode layer.  */
	memcpy(&tc->fops, dahdi_transcode_fops, sizeof(*dahdi_transcode_fops));
	return tc;
}

void dahdi_transcoder_free(struct dahdi_transcoder *tc)
{
	kfree(tc);
}

/* Returns 1 if the item is on the list pointed to by head, otherwise, returns
 * 0 */
static int is_on_list(struct list_head *entry, struct list_head *head)
{
	struct list_head *cur;
	list_for_each(cur, head) {
		if (cur == entry) return 1;
	}
	return 0;
}

/* Register a transcoder */
int dahdi_transcoder_register(struct dahdi_transcoder *tc)
{
	spin_lock(&translock);
	BUG_ON(is_on_list(&tc->registration_list_node, &registration_list));
	list_add_tail(&tc->registration_list_node, &registration_list);
	list_add_tail(&tc->active_list_node, &active_list);
	spin_unlock(&translock);

	printk(KERN_INFO "%s: Registered codec translator '%s' " \
	       "with %d transcoders (srcs=%08x, dsts=%08x)\n", 
	       THIS_MODULE->name, tc->name, tc->numchannels, 
	       tc->srcfmts, tc->dstfmts);

	return 0;
}

/* Unregister a transcoder */
int dahdi_transcoder_unregister(struct dahdi_transcoder *tc) 
{
	int res = -EINVAL;

	/* \todo Perhaps we should check to make sure there isn't a channel
	 * that is still in use? */

	spin_lock(&translock);
	if (!is_on_list(&tc->registration_list_node, &registration_list)) {
		spin_unlock(&translock);
		printk(KERN_WARNING "%s: Failed to unregister %s, which is " \
		       "not currently registered.\n", THIS_MODULE->name, tc->name);
		return -EINVAL;
	}
	list_del_init(&tc->registration_list_node);
	list_del_init(&tc->active_list_node);
	spin_unlock(&translock);

	printk(KERN_INFO "Unregistered codec translator '%s' with %d " \
	       "transcoders (srcs=%08x, dsts=%08x)\n", 
	       tc->name, tc->numchannels, tc->srcfmts, tc->dstfmts);
	res = 0;

	return res;
}

/* Alert a transcoder */
int dahdi_transcoder_alert(struct dahdi_transcoder_channel *chan)
{
	wake_up_interruptible(&chan->ready);
	return 0;
}

static int dahdi_tc_open(struct inode *inode, struct file *file)
{
	const struct file_operations *original_fops;	
	BUG_ON(!dahdi_transcode_fops);
	original_fops = file->f_op;
	file->f_op = dahdi_transcode_fops;
	file->private_data = NULL;
	/* Under normal operation, this releases the reference on the DAHDI
	 * module that was created when the file was opened. dahdi_open is
	 * responsible for taking a reference out on this module before
	 * calling this function. */
	module_put(original_fops->owner);
	return 0;
}

static void dtc_release(struct dahdi_transcoder_channel *chan)
{
	BUG_ON(!chan);
	if (chan->parent && chan->parent->release) {
		chan->parent->release(chan);
	}
	dahdi_tc_clear_busy(chan);
}

static int dahdi_tc_release(struct inode *inode, struct file *file)
{
	struct dahdi_transcoder_channel *chan = file->private_data;
	/* There will not be a transcoder channel associated with this file if
	 * the ALLOCATE ioctl never succeeded. 
	 */
	if (chan) {
		dtc_release(chan);
	}
	return 0;
}

/* Find a free channel on the transcoder and mark it busy. */
static inline struct dahdi_transcoder_channel *
get_free_channel(struct dahdi_transcoder *tc,
	const struct dahdi_transcoder_formats *fmts)
{
	struct dahdi_transcoder_channel *chan;
	int i;
	/* Should be called with the translock held. */
#ifdef CONFIG_SMP
	WARN_ON(!spin_is_locked(&translock));
#endif

	for (i = 0; i < tc->numchannels; i++) {
		chan = &tc->channels[i];
		if (!dahdi_tc_is_busy(chan)) {
			if (!dahdi_tc_is_built(chan)) {
				dahdi_tc_set_busy(chan);
				return chan;
			} else {
				/* If the channel is already built, we must
				 * make sure that it can support the formats
				 * that we're interested in. */
				if ((fmts->srcfmt|fmts->dstfmt) == chan->built_fmts) {
					dahdi_tc_set_busy(chan);
					return chan;
				}
			}
		}
	}
	return NULL;
}

/* Search the list for a transcoder that supports the specified format, and
 * allocate and return an available channel on it.   
 *
 * Returns either a pointer to the allocated channel, -EBUSY if the format is
 * supported but all the channels are busy, or -ENODEV if there are not any
 * transcoders that support the formats.
 */
static struct dahdi_transcoder_channel *
__find_free_channel(struct list_head *list, const struct dahdi_transcoder_formats *fmts)
{
	struct dahdi_transcoder *tc;
	struct dahdi_transcoder_channel *chan = NULL;
	unsigned int match = 0;

	list_for_each_entry(tc, list, active_list_node) {
		if ((tc->dstfmts & fmts->dstfmt) && (tc->srcfmts & fmts->srcfmt)) {
			/* We found a transcoder that can handle our formats.
			 * Now look for an available channel. */
			match = 1; 
			if ((chan = get_free_channel(tc, fmts))) {
				/* transcoder tc has a free channel.  In order
				 * to spread the load among available
				 * transcoders (when there are more than one
				 * transcoder in the system) we'll move tc 
				 * to the end of the list. */
				list_move_tail(&tc->active_list_node, list);
				return chan;
			}
		}
	}
	return (void*)((long)((match) ? -EBUSY : -ENODEV));
}

static long dahdi_tc_allocate(struct file *file, unsigned long data)
{
	struct dahdi_transcoder_channel *chan = NULL;
	struct dahdi_transcoder_formats fmts;
	
	if (copy_from_user(&fmts, (__user const void *) data, sizeof(fmts))) {
		return -EFAULT;
	}

	spin_lock(&translock);
	chan = __find_free_channel(&active_list, &fmts);
	spin_unlock(&translock);

	if (IS_ERR(chan)) {
		return PTR_ERR(chan);
	}

	/* Every transcoder channel must be associated with a parent
	 * transcoder. */
	BUG_ON(!chan->parent);

	chan->srcfmt = fmts.srcfmt;
	chan->dstfmt = fmts.dstfmt;

	if (file->private_data) {
		/* This open file is moving to a new channel. Cleanup and
		 * close the old channel here.  */
		dtc_release(file->private_data);
	}

	file->private_data = chan;
	if (chan->parent->fops.owner != file->f_op->owner) {
		if (!try_module_get(chan->parent->fops.owner)) {
			/* Failed to get a reference on the driver for the
			 * actual transcoding hardware.  */
			return -EINVAL;
		}
		/* Release the reference on the existing driver. */
		module_put(file->f_op->owner);
		file->f_op = &chan->parent->fops;
	}

	if (file->f_flags & O_NONBLOCK) {
		dahdi_tc_set_nonblock(chan);
	} else {
		dahdi_tc_clear_nonblock(chan); 
	}

	/* Actually reset the transcoder channel */
	if (chan->parent->allocate)
		return chan->parent->allocate(chan);

	return -EINVAL;
}

static long dahdi_tc_getinfo(unsigned long data)
{
	struct dahdi_transcoder_info info;
	struct dahdi_transcoder *cur;
	struct dahdi_transcoder *tc = NULL;
	unsigned int count = 0;

	if (copy_from_user(&info, (__user const void *) data, sizeof(info))) {
		return -EFAULT;
	}

	spin_lock(&translock);
	list_for_each_entry(cur, &registration_list, registration_list_node) {
		if (info.tcnum == count++) {
			tc = cur;
			break;
		} 
	}
	spin_unlock(&translock);

	if (!tc) {
		return -ENOSYS;
	}

	strlcpy(info.name, tc->name, sizeof(info.name));
	info.numchannels = tc->numchannels;
	info.srcfmts = tc->srcfmts;
	info.dstfmts = tc->dstfmts;

	return copy_to_user((__user void *) data, &info, sizeof(info)) ? -EFAULT : 0;
}

static ssize_t dahdi_tc_write(struct file *file, __user const char *usrbuf, size_t count, loff_t *ppos)
{
	if (file->private_data) {
		/* file->private_data will not be NULL if DAHDI_TC_ALLOCATE was
		 * called, and therefore indicates that the transcoder driver
		 * did not export a read function. */
		WARN_ON(1);
		return -ENOSYS;
	} else {
		printk(KERN_INFO "%s: Attempt to write to unallocated " \
		       "channel.\n", THIS_MODULE->name);
		return -EINVAL;
	}
}

static ssize_t dahdi_tc_read(struct file *file, __user char *usrbuf, size_t count, loff_t *ppos)
{
	if (file->private_data) {
		/* file->private_data will not be NULL if DAHDI_TC_ALLOCATE was
		 * called, and therefore indicates that the transcoder driver
		 * did not export a write function. */
		WARN_ON(1);
		return -ENOSYS;
	} else {
		printk(KERN_INFO "%s: Attempt to read from unallocated " \
		       "channel.\n", THIS_MODULE->name);
		return -EINVAL;
	}
}

static long dahdi_tc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	switch (cmd) {
	case DAHDI_TC_ALLOCATE:
		return dahdi_tc_allocate(file, data);
	case DAHDI_TC_GETINFO:
		return dahdi_tc_getinfo(data);
	case DAHDI_TRANSCODE_OP:
		/* This is a deprecated call from the previous transcoder
		 * interface, which was all routed through the dahdi_ioctl in
		 * dahdi-base.c, and this ioctl request was used to indicate
		 * that the call should be forwarded to this function. Now
		 * when the file is opened, the f_ops pointer is updated to
		 * point directly to this function, and we don't need a
		 * general indication that the ioctl is destined for the
		 * transcoder.  
		 *
		 * I'm keeping this ioctl here in order to explain why there
		 * might be a hole in the ioctl numbering scheme in the header
		 * files.
		 */
		printk(KERN_WARNING "%s: DAHDI_TRANSCODE_OP is no longer " \
		   "supported. Please call DAHDI_TC ioctls directly.\n",
		   THIS_MODULE->name);
		return -EINVAL;
	default:
		return -EINVAL;
	};
}

#ifndef HAVE_UNLOCKED_IOCTL
static int dahdi_tc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
	return (int)dahdi_tc_unlocked_ioctl(file, cmd, data);
}
#endif

static unsigned int dahdi_tc_poll(struct file *file, struct poll_table_struct *wait_table)
{
	int ret;
	struct dahdi_transcoder_channel *chan = file->private_data;

	if (!chan) {
		/* This is because the DAHDI_TC_ALLOCATE ioctl was not called
		 * before calling poll, which is invalid. */
		return -EINVAL;
	}

	poll_wait(file, &chan->ready, wait_table);

	ret =  dahdi_tc_is_busy(chan)         ? 0       : POLLPRI;
	ret |= dahdi_tc_is_built(chan)        ? POLLOUT : 0;
	ret |= dahdi_tc_is_data_waiting(chan) ? POLLIN  : 0;
	return ret;
}

static struct file_operations __dahdi_transcode_fops = {
	.owner =   THIS_MODULE,
	.open =    dahdi_tc_open,
	.release = dahdi_tc_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl  = dahdi_tc_unlocked_ioctl,
#else
	.ioctl   = dahdi_tc_ioctl,
#endif
	.read =    dahdi_tc_read,
	.write =   dahdi_tc_write,
	.poll =    dahdi_tc_poll,
};

static struct dahdi_chardev transcode_chardev = {
	.name = "transcode",
	.minor = DAHDI_TRANSCODE,
};

static int dahdi_transcode_init(void)
{
	int res;

	if (dahdi_transcode_fops) {
		printk(KERN_WARNING "dahdi_transcode_fops already set.\n");
		return -EBUSY;
	}

	dahdi_transcode_fops = &__dahdi_transcode_fops;

	if ((res = dahdi_register_chardev(&transcode_chardev)))
		return res;

	printk(KERN_INFO "%s: Loaded.\n", THIS_MODULE->name);
	return 0;
}

static void dahdi_transcode_cleanup(void)
{
	dahdi_unregister_chardev(&transcode_chardev);

	dahdi_transcode_fops = NULL;

	printk(KERN_DEBUG "%s: Unloaded.\n", THIS_MODULE->name);
}

module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_DESCRIPTION("DAHDI Transcoder Support");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

module_init(dahdi_transcode_init);
module_exit(dahdi_transcode_cleanup);
