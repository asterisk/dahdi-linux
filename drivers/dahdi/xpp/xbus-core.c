/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
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
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#ifdef	PROTOCOL_DEBUG
#include <linux/ctype.h>
#endif
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/delay.h>	/* for msleep() to debug */
#include "xpd.h"
#include "xpp_dahdi.h"
#include "xbus-core.h"
#include "card_global.h"
#ifdef	XPP_DEBUGFS
#include "xpp_log.h"
#endif
#include "dahdi_debug.h"

static const char rcsid[] = "$Id$";

/* Defines */
#define	INITIALIZATION_TIMEOUT	(90*HZ)		/* in jiffies */
#define	PROC_XBUSES		"xbuses"
#define	PROC_XBUS_SUMMARY	"summary"
#ifdef	OLD_PROC
#define	PROC_XBUS_WAITFOR_XPDS	"waitfor_xpds"
#endif

#ifdef	PROTOCOL_DEBUG
#ifdef	CONFIG_PROC_FS
#define	PROC_XBUS_COMMAND	"command"
static int proc_xbus_command_write(struct file *file, const char __user *buffer, unsigned long count, void *data);
#endif
#endif

/* Command line parameters */
extern int debug;
static DEF_PARM(uint, command_queue_length, 800, 0444, "Maximal command queue length");
static DEF_PARM(uint, poll_timeout, 1000, 0644, "Timeout (in jiffies) waiting for units to reply");
static DEF_PARM_BOOL(rx_tasklet, 0, 0644, "Use receive tasklets");

#ifdef	CONFIG_PROC_FS
static int xbus_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
#ifdef	OLD_PROC
static int xbus_read_waitfor_xpds(char *page, char **start, off_t off, int count, int *eof, void *data);
#endif
#endif
static void transport_init(xbus_t *xbus, struct xbus_ops *ops, ushort max_send_size, struct device *transport_device, void *priv);
static void transport_destroy(xbus_t *xbus);

/* Data structures */
static DEFINE_SPINLOCK(xbuses_lock);
#ifdef	CONFIG_PROC_FS
static struct proc_dir_entry	*proc_xbuses = NULL;
#endif

static struct xbus_desc {
	xbus_t			*xbus;
} xbuses_array[MAX_BUSES];

static xbus_t *xbus_byhwid(const char *hwid)
{
	int	i;
	xbus_t	*xbus;

	for (i = 0; i < ARRAY_SIZE(xbuses_array); i++) {
		xbus = xbuses_array[i].xbus;
		if (xbus && strcmp(hwid, xbus->label) == 0)
			return xbus;
	}
	return NULL;
}

int xbus_check_unique(xbus_t *xbus)
{
	if (!xbus)
		return -ENOENT;
	if (xbus->label) {
		xbus_t	*xbus_old;

		XBUS_DBG(DEVICES, xbus, "Checking LABEL='%s'\n", xbus->label);
		xbus_old = xbus_byhwid(xbus->label);
		if (xbus_old && xbus_old != xbus) {
			XBUS_NOTICE(xbus_old,
				"Duplicate LABEL='%s'. Leave %s unused. refcount_xbus=%d\n",
				xbus_old->label,
				xbus->busname,
				refcount_xbus(xbus_old));
			return -EBUSY;
		}
	} else {
		XBUS_NOTICE(xbus, "MISSING BOARD LABEL!!!\n");
	}
	return 0;
}

const char *xbus_statename(enum xbus_state st)
{
	switch(st) {
		case XBUS_STATE_START:		return "START";
		case XBUS_STATE_IDLE:		return "IDLE";
		case XBUS_STATE_SENT_REQUEST:	return "SENT_REQUEST";
		case XBUS_STATE_RECVD_DESC:	return "RECVD_DESC";
		case XBUS_STATE_READY:		return "READY";
		case XBUS_STATE_DEACTIVATING:	return "DEACTIVATING";
		case XBUS_STATE_DEACTIVATED:	return "DEACTIVATED";
		case XBUS_STATE_FAIL:		return "FAIL";
	}
	return NULL;
}

static void init_xbus(uint num, xbus_t *xbus)
{
	struct xbus_desc	*desc;

	BUG_ON(num >= ARRAY_SIZE(xbuses_array));
	desc = &xbuses_array[num];
	desc->xbus = xbus;
}

xbus_t *xbus_num(uint num)
{
	struct xbus_desc	*desc;

	if(num >= ARRAY_SIZE(xbuses_array))
		return NULL;
	desc = &xbuses_array[num];
	return desc->xbus;
}

static void initialize_xbuses_array(void)
{
	int	i;

	for(i = 0; i < ARRAY_SIZE(xbuses_array); i++)
		init_xbus(i, NULL);
}

static void finalize_xbuses_array(void)
{
	int	i;

	for(i = 0; i < ARRAY_SIZE(xbuses_array); i++) {
		if(xbuses_array[i].xbus != NULL) {
			ERR("%s: xbus #%d is not NULL\n", __FUNCTION__, i);
			BUG();
		}
	}
}

/*
 * Called by put_xbus() when XBUS has no more references.
 */
static void xbus_destroy(struct kref *kref)
{
	xbus_t	*xbus;

	xbus = kref_to_xbus(kref);
	XBUS_NOTICE(xbus, "%s\n", __func__);
	xbus_sysfs_remove(xbus);
}

xbus_t *get_xbus(const char *msg, uint num)
{
	unsigned long	flags;
	xbus_t		*xbus;

	spin_lock_irqsave(&xbuses_lock, flags);
	xbus = xbus_num(num);
	if (xbus != NULL) {
		kref_get(&xbus->kref);
		XBUS_DBG(DEVICES, xbus, "%s: refcount_xbus=%d\n",
			msg, refcount_xbus(xbus));
	}
	spin_unlock_irqrestore(&xbuses_lock, flags);
	return xbus;
}

void put_xbus(const char *msg, xbus_t *xbus)
{
	XBUS_DBG(DEVICES, xbus, "%s: refcount_xbus=%d\n",
		msg, refcount_xbus(xbus));
	kref_put(&xbus->kref, xbus_destroy);
}

int refcount_xbus(xbus_t *xbus)
{
	struct kref *kref = &xbus->kref;

	return atomic_read(&kref->refcount);
}

/*------------------------- Debugfs Handling -----------------------*/
#ifdef	XPP_DEBUGFS

#define DEBUGFS_BUFSIZ		4096	/* must be power of two, otherwise POS_IN_BUF will have to use '%' instead of '&' */
#define POS_IN_BUF(x)		((x) & (DEBUGFS_BUFSIZ-1))

struct debugfs_data {
	spinlock_t lock;
	xbus_t *xbus;
	char buffer[DEBUGFS_BUFSIZ];
	unsigned long head, tail;	/* reading and writing are performed at position (head % BUF_SIZ) and (tail % BUF_SIZ) */
	wait_queue_head_t queue;
};

static unsigned long add_to_buf(struct debugfs_data *d, unsigned long tail, const void *buf, unsigned long len)
{
	unsigned long count = min(len, (unsigned long)(DEBUGFS_BUFSIZ - POS_IN_BUF(tail)));
	memcpy(d->buffer + POS_IN_BUF(tail), buf, count);		/* fill starting at position tail */
	memcpy(d->buffer, (u_char *)buf + count, len - count);		/* fill leftover */
	return len;
}

int xbus_log(xbus_t *xbus, xpd_t *xpd, int direction, const void *buf, unsigned long len)
{
	unsigned long tail;
	unsigned long flags;
	struct debugfs_data *d;
	struct log_header header;
	int ret = 0;
	
	BUG_ON(!xbus);
	BUG_ON(!xpd);
	BUG_ON(sizeof(struct log_header) + len > DEBUGFS_BUFSIZ);
	d = xbus->debugfs_data;
	if (!d)			/* no consumer process */
		return ret;
	spin_lock_irqsave(&d->lock, flags);
	if (sizeof(struct log_header) + len > DEBUGFS_BUFSIZ - (d->tail - d->head)) {
		ret = -ENOSPC;
		XPD_DBG(GENERAL, xpd, "Dropping debugfs data of len %lu, free space is %lu\n", sizeof(struct log_header) + len,
				DEBUGFS_BUFSIZ - (d->tail - d->head));
		goto out;
	}
	header.len = sizeof(struct log_header) + len;
	header.time = jiffies_to_msecs(jiffies);
	header.xpd_num = xpd->xbus_idx;
	header.direction = (char)direction;
	tail = d->tail;
	tail += add_to_buf(d, tail, &header, sizeof(header));
	tail += add_to_buf(d, tail, buf, len);
	d->tail = tail;
	wake_up_interruptible(&d->queue);
out:
	spin_unlock_irqrestore(&d->lock, flags);
	return ret;
}

static struct dentry	*debugfs_root = NULL;
static int debugfs_open(struct inode *inode, struct file *file);
static ssize_t debugfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos);
static int debugfs_release(struct inode *inode, struct file *file);

static struct file_operations debugfs_operations = {
	.open		= debugfs_open,
	.read		= debugfs_read,
	.release	= debugfs_release,
};

/*
 * As part of the "inode diet" the private data member of struct inode
 * has changed in 2.6.19. However, Fedore Core 6 adopted this change
 * a bit earlier (2.6.18). If you use such a kernel, Change the 
 * following test from 2,6,19 to 2,6,18.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define	I_PRIVATE(inode)	((inode)->u.generic_ip)
#else
#define	I_PRIVATE(inode)	((inode)->i_private)
#endif

static int debugfs_open(struct inode *inode, struct file *file)
{
	xbus_t	*xbus = I_PRIVATE(inode);
	struct debugfs_data *d;
	struct log_global_header gheader;

	BUG_ON(!xbus);
	XBUS_DBG(GENERAL, xbus, "\n");
	if (xbus->debugfs_data)
		return -EBUSY;
	d = KZALLOC(sizeof(struct debugfs_data), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	try_module_get(THIS_MODULE);
	spin_lock_init(&d->lock);
	d->xbus = xbus;
	d->head = d->tail = 0;
	init_waitqueue_head(&d->queue);
	file->private_data = d;

	gheader.magic = XPP_LOG_MAGIC;
	gheader.version = 1;
	d->tail += add_to_buf(d, d->tail, &gheader, sizeof(gheader));

	xbus->debugfs_data = d;
	return 0;
}

static ssize_t debugfs_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	struct debugfs_data *d = file->private_data;
	size_t len;

	BUG_ON(!d);
	BUG_ON(!d->xbus);
	XBUS_DBG(GENERAL, d->xbus, "\n");
	while (d->head == d->tail) {
		if (wait_event_interruptible(d->queue, d->head != d->tail))
			return -EAGAIN;
	}
	len = min(nbytes, (size_t)(d->tail - d->head));
	if (copy_to_user(buf, d->buffer + POS_IN_BUF(d->head), len))
		return -EFAULT;
	d->head += len;
	/* optimization to avoid future buffer wraparound */
	if (d->head == d->tail) {
		unsigned long flags;
		spin_lock_irqsave(&d->lock, flags);
		if (d->head == d->tail)
			d->head = d->tail = 0;
		spin_unlock_irqrestore(&d->lock, flags);
	}
	return len;
}

static int debugfs_release(struct inode *inode, struct file *file)
{
	struct debugfs_data *d = file->private_data;

	BUG_ON(!d);
	BUG_ON(!d->xbus);
	XBUS_DBG(GENERAL, d->xbus, "\n");
	d->xbus->debugfs_data = NULL;
	KZFREE(d);
	module_put(THIS_MODULE);
	return 0;
}
#endif

/*------------------------- Frame  Handling ------------------------*/

void xframe_init(xbus_t *xbus, xframe_t *xframe, void *buf, size_t maxsize, void *priv)
{
	memset(xframe, 0, sizeof(*xframe));
	INIT_LIST_HEAD(&xframe->frame_list);
	xframe->priv = priv;
	xframe->xbus = xbus;
	xframe->packets = xframe->first_free = buf;
	xframe->frame_maxlen = maxsize;
	atomic_set(&xframe->frame_len, 0);
	do_gettimeofday(&xframe->tv_created);
	xframe->xframe_magic = XFRAME_MAGIC;
}

/*
 * Return pointer to next packet slot in the frame
 * or NULL if the frame is full.
 *
 * FIXME: we do not use atomic_add_return() because kernel-2.6.8
 *        does not have it. This make this code a little racy,
 *        but we currently call xframe_next_packet() only in the
 *        PCM loop (xbus_tick() etc.)
 */
xpacket_t *xframe_next_packet(xframe_t *frm, int len)
{
	int newlen = XFRAME_LEN(frm);

	newlen += len;
//	DBG(GENERAL, "len=%d, newlen=%d, frm->frame_len=%d\n", len, newlen, XFRAME_LEN(frm));
	if (newlen > XFRAME_DATASIZE) {
		return NULL;
	}
	atomic_add(len, &frm->frame_len);
	return (xpacket_t *)(frm->packets + newlen - len);
}

static DEFINE_SPINLOCK(serialize_dump_xframe);

static void do_hexdump(const char msg[], byte *data, uint16_t len)
{
	int	i;
	int	debug = DBG_ANY;	/* mask global debug */

	for(i = 0; i < len; i++)
		DBG(ANY, "%s: %3d> %02X\n", msg, i, data[i]);
}

void dump_xframe(const char msg[], const xbus_t *xbus, const xframe_t *xframe, int debug)
{
	const uint16_t	frm_len = XFRAME_LEN(xframe);
	xpacket_t	*pack;
	uint16_t	pos = 0;
	uint16_t	nextpos;
	int 		num = 1;
	bool		do_print;
	unsigned long	flags;
	
	if(xframe->xframe_magic != XFRAME_MAGIC) {
		XBUS_ERR(xbus, "%s: bad xframe_magic %lX\n",
			__FUNCTION__, xframe->xframe_magic);
		return;
	}
	spin_lock_irqsave(&serialize_dump_xframe, flags);
	do {
		if(pos >= xbus->transport.max_send_size) {
			if(printk_ratelimit()) {
				XBUS_NOTICE(xbus, "%s: xframe overflow (%d bytes)\n",
				    msg, frm_len);
				do_hexdump(msg, xframe->packets, frm_len);
			}
			break;
		}
		if(pos > frm_len) {
			if(printk_ratelimit()) {
				XBUS_NOTICE(xbus, "%s: packet overflow pos=%d frame_len=%d\n",
				    msg, pos, frm_len);
				do_hexdump(msg, xframe->packets, frm_len);
			}
			break;
		}
		pack = (xpacket_t *)&xframe->packets[pos];
		if(XPACKET_LEN(pack) <= 0) {
			if(printk_ratelimit()) {
				XBUS_NOTICE(xbus, "%s: xframe -- bad packet_len=%d pos=%d frame_len=%d\n",
				    msg, XPACKET_LEN(pack), pos, frm_len);
				do_hexdump(msg, xframe->packets, frm_len);
			}
			break;
		}
		nextpos = pos + XPACKET_LEN(pack);
		if(nextpos > frm_len) {
			if(printk_ratelimit()) {
				XBUS_NOTICE(xbus, "%s: packet overflow nextpos=%d frame_len=%d\n",
				    msg, nextpos, frm_len);
				do_hexdump(msg, xframe->packets, frm_len);
			}
			break;
		}
		do_print = 0;
		if(debug == DBG_ANY)
			do_print = 1;
		else if(XPACKET_OP(pack) != XPROTO_NAME(GLOBAL,PCM_READ) &&
			XPACKET_OP(pack) != XPROTO_NAME(GLOBAL,PCM_WRITE))
			do_print = 1;
		else if(debug & DBG_PCM) {
			static int	rate_limit;

			if((rate_limit++ % 1003) == 0)
				do_print = 1;
		}
		if(do_print) {
			if(num == 1) {
				XBUS_DBG(ANY, xbus, "%s: frame_len=%d. %s\n",
						msg, frm_len,
						(XPACKET_IS_PCM(pack))
							? "(IS_PCM)"
							: "");
			}
			XBUS_DBG(ANY, xbus, "  %3d. DATALEN=%d pcm=%d slot=%d OP=0x%02X XPD-%d%d (pos=%d)\n",
				num, XPACKET_LEN(pack),
				XPACKET_IS_PCM(pack), XPACKET_PCMSLOT(pack),
				XPACKET_OP(pack),
				XPACKET_ADDR_UNIT(pack), XPACKET_ADDR_SUBUNIT(pack),
				pos);
			dump_packet("     ", pack, debug);
		}
		num++;
		pos = nextpos;
		if(pos >= frm_len)
			break;
	} while(1);
	spin_unlock_irqrestore(&serialize_dump_xframe, flags);
}

/**
 *
 * Frame is freed:
 * 	- In case of error, by this function.
 * 	- Otherwise, by the underlying sending mechanism
 */
int send_pcm_frame(xbus_t *xbus, xframe_t *xframe)
{
	struct xbus_ops	*ops;
	int		ret = -ENODEV;

	BUG_ON(!xframe);
	if(!XBUS_IS(xbus, READY)) {
		XBUS_ERR(xbus, "Dropped a pcm frame -- hardware is not ready.\n");
		ret = -ENODEV;
		goto error;
	}
	ops = transportops_get(xbus);
	BUG_ON(!ops);
	ret = ops->xframe_send_pcm(xbus, xframe);
	transportops_put(xbus);
	if(ret)
		XBUS_COUNTER(xbus, TX_BYTES) += XFRAME_LEN(xframe);
	return ret;

error:	
	FREE_SEND_XFRAME(xbus, xframe);
	return ret;
}

static int really_send_cmd_frame(xbus_t *xbus, xframe_t *xframe)
{
	struct xbus_ops	*ops;
	int		ret;

	BUG_ON(!xbus);
	BUG_ON(!xframe);
	BUG_ON(xframe->xframe_magic != XFRAME_MAGIC);
	if(!XBUS_FLAGS(xbus, CONNECTED)) {
		XBUS_ERR(xbus, "Dropped command before sending -- hardware deactivated.\n");
		dump_xframe("Dropped", xbus, xframe, DBG_ANY);
		FREE_SEND_XFRAME(xbus, xframe);
		return -ENODEV;
	}
	ops = transportops_get(xbus);
	BUG_ON(!ops);
	if(debug & DBG_COMMANDS)
		dump_xframe("TX-CMD", xbus, xframe, DBG_ANY);
	ret = ops->xframe_send_cmd(xbus, xframe);
	transportops_put(xbus);
	if(ret == 0) {
		XBUS_COUNTER(xbus, TX_CMD)++;
		XBUS_COUNTER(xbus, TX_BYTES) += XFRAME_LEN(xframe);
	}
	return ret;
}

int xbus_command_queue_tick(xbus_t *xbus)
{
	xframe_t	*frm;
	int		ret = 0;
	int		packno;

	xbus->command_tick_counter++;
	xbus->usec_nosend -= 1000;	/* That's our budget */
	for(packno = 0; packno < 3; packno++) {
		if(xbus->usec_nosend > 0)
			break;
		frm = xframe_dequeue(&xbus->command_queue);
		if(!frm) {
			wake_up(&xbus->command_queue_empty);
			break;
		}
		BUG_ON(frm->xframe_magic != XFRAME_MAGIC);
		xbus->usec_nosend += frm->usec_towait;
		ret = really_send_cmd_frame(xbus, frm);
		if(ret < 0) {
			XBUS_ERR(xbus,
				"Failed to send from command_queue (ret=%d)\n",
				ret);
			xbus_setstate(xbus, XBUS_STATE_FAIL);
		}
	}
	if(xbus->usec_nosend < 0)
		xbus->usec_nosend = 0;
	return ret;
}

static void xbus_command_queue_clean(xbus_t *xbus)
{
	xframe_t	*frm;

	XBUS_DBG(DEVICES, xbus, "count=%d\n", xbus->command_queue.count);
	xframe_queue_disable(&xbus->command_queue, 1);
	while((frm = xframe_dequeue(&xbus->command_queue)) != NULL) {
		FREE_SEND_XFRAME(xbus, frm);
	}
}

static int xbus_command_queue_waitempty(xbus_t *xbus)
{
	int		ret;

	XBUS_DBG(DEVICES, xbus, "Waiting for command_queue to empty\n");
	ret = wait_event_interruptible(xbus->command_queue_empty,
				xframe_queue_count(&xbus->command_queue) == 0);
	if(ret) {
		XBUS_ERR(xbus, "waiting for command_queue interrupted!!!\n");
	}
	return ret;
}

int send_cmd_frame(xbus_t *xbus, xframe_t *xframe)
{
	static int	rate_limit;
	int		ret = 0;


	BUG_ON(xframe->xframe_magic != XFRAME_MAGIC);
	if(!XBUS_FLAGS(xbus, CONNECTED)) {
		XBUS_ERR(xbus, "Dropped command before queueing -- hardware deactivated.\n");
		ret = -ENODEV;
		goto err;
	}
	if(debug & DBG_COMMANDS)
		dump_xframe(__FUNCTION__, xbus, xframe, DBG_ANY);
	if(!xframe_enqueue(&xbus->command_queue, xframe)) {
		if((rate_limit++ % 1003) == 0) {
			XBUS_ERR(xbus,
				"Dropped command xframe. Cannot enqueue (%d)\n",
				rate_limit);
			dump_xframe(__FUNCTION__, xbus, xframe, DBG_ANY);
		}
		xbus_setstate(xbus, XBUS_STATE_FAIL);
		ret = -E2BIG;
		goto err;
	}
	return 0;
err:
	FREE_SEND_XFRAME(xbus, xframe);
	return ret;
}

/*------------------------- Receive Tasklet Handling ---------------*/

static void xframe_enqueue_recv(xbus_t *xbus, xframe_t *xframe)
{
	int	cpu = smp_processor_id();

	BUG_ON(!xbus);
	xbus->cpu_rcv_intr[cpu]++;
	if(!xframe_enqueue(&xbus->receive_queue, xframe)) {
		static int	rate_limit;

		if((rate_limit++ % 1003) == 0)
			XBUS_ERR(xbus, "Failed to enqueue for receive_tasklet (%d)\n", rate_limit);
		FREE_RECV_XFRAME(xbus, xframe);	/* return to receive_pool */
		return;
	}
	tasklet_schedule(&xbus->receive_tasklet);
}

/*
 * process frames in the receive_queue in a tasklet
 */
static void receive_tasklet_func(unsigned long data)
{
	xbus_t		*xbus = (xbus_t *)data;
	xframe_t	*xframe = NULL;
	int		cpu = smp_processor_id();

	BUG_ON(!xbus);
	xbus->cpu_rcv_tasklet[cpu]++;
	while((xframe = xframe_dequeue(&xbus->receive_queue)) != NULL) {
		xframe_receive(xbus, xframe);
	}
}

void xbus_receive_xframe(xbus_t *xbus, xframe_t *xframe)
{
	BUG_ON(!xbus);
	if(rx_tasklet) {
		xframe_enqueue_recv(xbus, xframe);
	} else {
		if(likely(XBUS_FLAGS(xbus, CONNECTED)))
			xframe_receive(xbus, xframe);
		else
			FREE_RECV_XFRAME(xbus, xframe);	/* return to receive_pool */
	}
}

/*------------------------- Bus Management -------------------------*/
xpd_t	*xpd_of(const xbus_t *xbus, int xpd_num)
{
	if(!VALID_XPD_NUM(xpd_num))
		return NULL;
	return xbus->xpds[xpd_num];
}

xpd_t	*xpd_byaddr(const xbus_t *xbus, uint unit, uint subunit)
{
	if(unit > MAX_UNIT || subunit > MAX_SUBUNIT)
		return NULL;
	return xbus->xpds[XPD_IDX(unit,subunit)];
}

int xbus_xpd_bind(xbus_t *xbus, xpd_t *xpd, int unit, int subunit)
{
	unsigned int	xpd_num;
	unsigned long	flags;

	BUG_ON(!xbus);
	xpd_num = XPD_IDX(unit,subunit);
	XBUS_DBG(DEVICES, xbus, "XPD #%d\n", xpd_num);
	spin_lock_irqsave(&xbus->lock, flags);
	if(!VALID_XPD_NUM(xpd_num)) {
		XBUS_ERR(xbus, "Bad xpd_num = %d\n", xpd_num);
		BUG();
	}
	if(xbus->xpds[xpd_num] != NULL) {
		xpd_t	*other = xbus->xpds[xpd_num];

		XBUS_ERR(xbus, "xpd_num=%d is occupied by %p (%s)\n",
				xpd_num, other, other->xpdname);
		BUG();
	}
	snprintf(xpd->xpdname, XPD_NAMELEN, "XPD-%1d%1d", unit, subunit);
	MKADDR(&xpd->addr, unit, subunit);
	xpd->xbus_idx = xpd_num;
	xbus->xpds[xpd_num] = xpd;
	xpd->xbus = xbus;
	atomic_inc(&xbus->num_xpds);
	spin_unlock_irqrestore(&xbus->lock, flags);
	/* Must be done out of atomic context */
	if(xpd_device_register(xbus, xpd) < 0) {
		XPD_ERR(xpd, "%s: xpd_device_register() failed\n", __FUNCTION__);
		/* FIXME: What to do? */
	}
	return 0;
}

int xbus_xpd_unbind(xbus_t *xbus, xpd_t *xpd)
{
	unsigned int	xpd_num = xpd->xbus_idx;
	unsigned long	flags;

	XBUS_DBG(DEVICES, xbus, "XPD #%d\n", xpd_num);
	if(!VALID_XPD_NUM(xpd_num)) {
		XBUS_ERR(xbus, "%s: Bad xpd_num = %d\n", __FUNCTION__, xpd_num);
		BUG();
	}
	if(xbus->xpds[xpd_num] == NULL) {
		XBUS_ERR(xbus, "%s: slot xpd_num=%d is empty\n", __FUNCTION__, xpd_num);
		BUG();
	}
	if(xbus->xpds[xpd_num] != xpd) {
		xpd_t	*other = xbus->xpds[xpd_num];

		XBUS_ERR(xbus, "%s: slot xpd_num=%d is occupied by %p (%s)\n",
				__FUNCTION__, xpd_num, other, other->xpdname);
		BUG();
	}
	spin_lock_irqsave(&xbus->lock, flags);
	xpd->xbus = NULL;
	xbus->xpds[xpd_num] = NULL;
	if(atomic_dec_and_test(&xbus->num_xpds))
		xbus_setstate(xbus, XBUS_STATE_IDLE);
	spin_unlock_irqrestore(&xbus->lock, flags);
	return 0;
}

static int new_card(xbus_t *xbus,
		int unit,
		byte type,
		byte subtype,
		byte numchips,
		byte ports_per_chip,
		byte ports,
		byte port_dir)
{
	const xproto_table_t	*proto_table;
	const xops_t		*xops;
	int			i;
	int			subunits;
	int			ret = 0;
	int			remaining_ports;

	proto_table = xproto_get(type);
	if(!proto_table) {
		XBUS_NOTICE(xbus,
			"CARD %d: missing protocol table for type %d. Ignored.\n",
			unit, type);
		return -EINVAL;
	}
	remaining_ports = ports;
	subunits = (ports + proto_table->ports_per_subunit - 1) /
			proto_table->ports_per_subunit;
	XBUS_DBG(DEVICES, xbus, "CARD %d type=%d.%d ports=%d (%dx%d), %d subunits, port-dir=0x%02X\n",
			unit,
			type,
			subtype,
			ports,
			numchips,
			ports_per_chip,
			subunits,
			port_dir
		);
	xops = &proto_table->xops;
	BUG_ON(!xops);
	xbus->worker.num_units += subunits - 1;
	for(i = 0; i < subunits; i++) {
		int	subunit_ports = proto_table->ports_per_subunit;

		if(subunit_ports > remaining_ports)
			subunit_ports = remaining_ports;
		remaining_ports -= proto_table->ports_per_subunit;
		if(subunit_ports <= 0) {
			XBUS_NOTICE(xbus,
				"Subunit XPD=%d%d without ports (%d of %d)\n",
				unit,
				i,
				subunit_ports,
				ports);
			ret = -ENODEV;
			goto out;
		}
		if(!XBUS_IS(xbus, RECVD_DESC)) {
			XBUS_NOTICE(xbus,
				"Cannot create XPD=%d%d in state %s\n",
				unit,
				i,
				xbus_statename(XBUS_STATE(xbus)));
			ret = -ENODEV;
			goto out;
		}
		XBUS_DBG(DEVICES, xbus, "Creating XPD=%d%d type=%d.%d (%d ports)\n",
				unit,
				i,
				type,
				subtype, subunit_ports);
		if(!XBUS_IS(xbus, RECVD_DESC)) {
			XBUS_ERR(xbus, "Aborting creation -- In bad state %s\n",
				xbus_statename(XBUS_STATE(xbus)));
			ret = -ENODEV;
			goto out;
		}
		ret = create_xpd(xbus, proto_table, unit, i, type, subtype, subunits, subunit_ports, port_dir);
		if(ret < 0) {
			XBUS_ERR(xbus, "Creation of XPD=%d%d failed %d\n",
				unit, i, ret);
			goto out;
		}
		xbus->worker.num_units_initialized++;
	}
out:
	xproto_put(proto_table);	/* ref count is inside the xpds now */
	return ret;
}

static void xbus_release_xpds(xbus_t *xbus)
{
	int			i;

	XBUS_DBG(DEVICES, xbus, "[%s] Release XPDS\n", xbus->label);
	for(i = 0; i < MAX_XPDS; i++) {
		xpd_t *xpd = xpd_of(xbus, i);

		if(xpd)
			put_xpd(__func__, xpd);
	}
}

static int xbus_aquire_xpds(xbus_t *xbus)
{
	unsigned long	flags;
	int		i;
	int		ret = 0;
	xpd_t		*xpd;

	XBUS_DBG(DEVICES, xbus, "[%s] Aquire XPDS\n", xbus->label);
	spin_lock_irqsave(&xbus->lock, flags);
	for (i = 0; i < MAX_XPDS; i++) {
		xpd = xpd_of(xbus, i);
		if (xpd) {
			xpd = get_xpd(__func__, xpd);
			if (!xpd)
				goto err;
		}
	}
out:
	spin_unlock_irqrestore(&xbus->lock, flags);
	return ret;
err:
	for (--i ; i >= 0; i--) {
		xpd = xpd_of(xbus, i);
		if (xpd)
			put_xpd(__func__, xpd);
	}
	ret = -EBUSY;
	goto out;
}

static int xpd_initialize(xpd_t *xpd)
{
	int	ret = -ENODEV;

	if(CALL_XMETHOD(card_init, xpd->xbus, xpd) < 0) {
		XPD_ERR(xpd, "Card Initialization failed\n");
		goto out;
	}
	//CALL_XMETHOD(XPD_STATE, xpd->xbus, xpd, 0);	/* Turn off all channels */
	xpd->card_present = 1;
	CALL_XMETHOD(XPD_STATE, xpd->xbus, xpd, 1);		/* Turn on all channels */
	if(!xpd_setstate(xpd, XPD_STATE_READY)) {
		goto out;
	}
	XPD_INFO(xpd, "Initialized: %s\n", xpd->type_name);
	xpd_post_init(xpd);
	ret = 0;
out:
	return ret;
}

static int xbus_initialize(xbus_t *xbus)
{
	int	unit;
	int	subunit;
	xpd_t	*xpd;
	struct timeval	time_start;
	struct timeval	time_end;
	unsigned long	timediff;
	int		res = 0;

	do_gettimeofday(&time_start);
	XBUS_DBG(DEVICES, xbus, "refcount_xbus=%d\n",
			refcount_xbus(xbus));
	if (xbus_aquire_xpds(xbus) < 0)	/* Until end of initialization */
		return -EBUSY;
	for(unit = 0; unit < MAX_UNIT; unit++) {
		xpd = xpd_byaddr(xbus, unit, 0);
		if(!xpd)
			continue;
		if (!XBUS_IS(xbus, RECVD_DESC)) {
			XBUS_NOTICE(xbus,
				"Cannot initialize UNIT=%d in state %s\n",
				unit,
				xbus_statename(XBUS_STATE(xbus)));
			goto err;
		}
		if(run_initialize_registers(xpd) < 0) {
			XBUS_ERR(xbus, "Register Initialization of card #%d failed\n", unit);
			goto err;
		}
		for(subunit = 0; subunit < MAX_SUBUNIT; subunit++) {
			int	ret;

			xpd = xpd_byaddr(xbus, unit, subunit);
			if(!xpd)
				continue;
			if (!XBUS_IS(xbus, RECVD_DESC)) {
				XBUS_ERR(xbus,
					"XPD-%d%d Not in 'RECVD_DESC' state\n",
					unit, subunit);
				goto err;
			}
			ret = xpd_initialize(xpd);
			if(ret < 0)
				goto err;
		}
	}
	do_gettimeofday(&time_end);
	timediff = usec_diff(&time_end, &time_start);
	timediff /= 1000*100;
	XBUS_INFO(xbus, "Initialized in %ld.%1ld sec\n", timediff/10, timediff%10);
out:
	xbus_release_xpds(xbus);	/* Initialization done/failed */
	return res;
err:
	xbus_setstate(xbus, XBUS_STATE_FAIL);
	res = -EINVAL;
	goto out;
}

/*
 * This must be called from synchronous (non-interrupt) context
 * it returns only when all XPD's on the bus are detected and
 * initialized.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void xbus_populate(struct work_struct *work)
{
	struct xbus_workqueue	*worker = container_of(work, struct xbus_workqueue, xpds_init_work);
#else
void xbus_populate(void *data)
{
	struct xbus_workqueue	*worker = data;
#endif
	xbus_t			*xbus;
	struct list_head	*card;
	struct list_head	*next_card;
	unsigned long		flags;
	int			ret = 0;

	xbus = container_of(worker, xbus_t, worker);
	xbus = get_xbus(__func__, xbus->num);	/* return in function end */
	XBUS_DBG(DEVICES, xbus, "Entering %s\n", __FUNCTION__);
	spin_lock_irqsave(&worker->worker_lock, flags);
	list_for_each_safe(card, next_card, &worker->card_list) {
		struct card_desc_struct	*card_desc = list_entry(card, struct card_desc_struct, card_list);

		list_del(card);
		BUG_ON(card_desc->magic != CARD_DESC_MAGIC);
		/* Release/Reacquire locks around blocking calls */
		spin_unlock_irqrestore(&xbus->worker.worker_lock, flags);
		ret = new_card(xbus,
			card_desc->xpd_addr.unit,
			card_desc->type,
			card_desc->subtype,
			card_desc->numchips,
			card_desc->ports_per_chip,
			card_desc->ports,
			card_desc->port_dir);
		spin_lock_irqsave(&xbus->worker.worker_lock, flags);
		KZFREE(card_desc);
		if(ret)
			break;
	}
	spin_unlock_irqrestore(&worker->worker_lock, flags);
	if(xbus_initialize(xbus) < 0) {
		XBUS_NOTICE(xbus, "Initialization failed. Leave unused. refcount_xbus=%d\n",
			refcount_xbus(xbus));
		goto failed;
	}
	if(!xbus_setstate(xbus, XBUS_STATE_READY)) {
		XBUS_NOTICE(xbus, "Illegal transition. Leave unused. refcount_xbus=%d\n",
			refcount_xbus(xbus));
		goto failed;
	}
	worker->xpds_init_done = 1;
	/*
	 * Now request Astribank to start self_ticking.
	 * This is the last initialization command. So
	 * all others will reach the device before it.
	 */
	xbus_request_sync(xbus, SYNC_MODE_PLL);
	elect_syncer("xbus_populate(end)");	/* FIXME: try to do it later */
out:
	XBUS_DBG(DEVICES, xbus, "Leaving\n");
	wake_up_interruptible_all(&worker->wait_for_xpd_initialization);
	XBUS_DBG(DEVICES, xbus, "populate release\n");
	up(&worker->running_initialization);
	put_xbus(__func__, xbus);	/* taken at function entry */
	return;
failed:
	xbus_setstate(xbus, XBUS_STATE_FAIL);
	goto out;
}

int xbus_process_worker(xbus_t *xbus)
{
	struct xbus_workqueue	*worker;

	if(!xbus) {
		ERR("%s: xbus gone -- skip initialization\n", __FUNCTION__);
		return 0;
	}
	worker = &xbus->worker;
	if (down_trylock(&worker->running_initialization)) {
		ERR("%s: xbus is disconnected -- skip initialization\n", __FUNCTION__);
		return 0;
	}
	XBUS_DBG(DEVICES, xbus, "\n");
	/* Initialize the work. (adapt to kernel API changes). */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	INIT_WORK(&worker->xpds_init_work, xbus_populate);
#else
	INIT_WORK(&worker->xpds_init_work, xbus_populate, worker);
#endif
	BUG_ON(!xbus);
	/* Now send it */
	if(!queue_work(worker->wq, &worker->xpds_init_work)) {
		XBUS_ERR(xbus, "Failed to queue xpd initialization work\n");
		up(&worker->running_initialization);
		return 0;
	}
	return 1;
}

static void worker_reset(xbus_t *xbus)
{
	struct xbus_workqueue	*worker;
	struct list_head	*card;
	struct list_head	*next_card;
	unsigned long		flags;
	char			*name;

	BUG_ON(!xbus);
	worker = &xbus->worker;
	name = (xbus) ? xbus->busname : "detached";
	DBG(DEVICES, "%s\n", name);
	if(!worker->xpds_init_done) {
		NOTICE("%s: worker(%s)->xpds_init_done=%d\n",
			__FUNCTION__, name, worker->xpds_init_done);
	}
	spin_lock_irqsave(&worker->worker_lock, flags);
	list_for_each_safe(card, next_card, &worker->card_list) {
		struct card_desc_struct	*card_desc = list_entry(card, struct card_desc_struct, card_list);

		BUG_ON(card_desc->magic != CARD_DESC_MAGIC);
		list_del(card);
		KZFREE(card_desc);
	}
	worker->xpds_init_done = 0;
	worker->num_units = 0;
	worker->num_units_initialized = 0;
	wake_up_interruptible_all(&worker->wait_for_xpd_initialization);
	spin_unlock_irqrestore(&worker->worker_lock, flags);
}

static void worker_destroy(xbus_t *xbus)
{
	struct xbus_workqueue	*worker;

	BUG_ON(!xbus);
	worker = &xbus->worker;
	worker_reset(xbus);
	XBUS_DBG(DEVICES, xbus, "Waiting for worker to finish...\n");
	down(&worker->running_initialization);
	XBUS_DBG(DEVICES, xbus, "Waiting for worker to finish -- done\n");
	if (worker->wq) {
		XBUS_DBG(DEVICES, xbus, "destroying workqueue...\n");
		flush_workqueue(worker->wq);
		destroy_workqueue(worker->wq);
		worker->wq = NULL;
		XBUS_DBG(DEVICES, xbus, "destroying workqueue -- done\n");
	}
#ifdef CONFIG_PROC_FS
#ifdef	OLD_PROC
	if (xbus->proc_xbus_dir && worker->proc_xbus_waitfor_xpds) {
		XBUS_DBG(PROC, xbus, "Removing proc '%s'\n",
				PROC_XBUS_WAITFOR_XPDS);
		remove_proc_entry(PROC_XBUS_WAITFOR_XPDS, xbus->proc_xbus_dir);
		worker->proc_xbus_waitfor_xpds = NULL;
	}
#endif
#endif
	XBUS_DBG(DEVICES, xbus, "detach worker\n");
	put_xbus(__func__, xbus);	/* got from worker_run() */
}

static void worker_init(xbus_t *xbus)
{
	struct xbus_workqueue	*worker;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "\n");
	worker = &xbus->worker;
	/* poll related variables */
	spin_lock_init(&worker->worker_lock);
	INIT_LIST_HEAD(&worker->card_list);
	init_waitqueue_head(&worker->wait_for_xpd_initialization);
	worker->wq = NULL;
	init_MUTEX(&xbus->worker.running_initialization);
}

/*
 * Allocate a worker for the xbus including the nessessary workqueue.
 * May call blocking operations, but only briefly (as we are called
 * from xbus_new() which is called from khubd.
 */
static int worker_run(xbus_t *xbus)
{
	struct xbus_workqueue	*worker;

	xbus = get_xbus(__func__, xbus->num);	/* return in worker_destroy() */
	BUG_ON(!xbus);
	BUG_ON(xbus->busname[0] == '\0');	/* No name? */
	worker = &xbus->worker;
	BUG_ON(worker->wq);	/* Hmmm... nested workers? */
	XBUS_DBG(DEVICES, xbus, "\n");
	/* poll related variables */
#ifdef CONFIG_PROC_FS
#ifdef	OLD_PROC
	if(xbus->proc_xbus_dir) {
		worker->proc_xbus_waitfor_xpds = create_proc_read_entry(
				PROC_XBUS_WAITFOR_XPDS, 0444,
				xbus->proc_xbus_dir,
				xbus_read_waitfor_xpds,
				xbus);
		if (!worker->proc_xbus_waitfor_xpds) {
			XBUS_ERR(xbus, "Failed to create proc file '%s'\n", PROC_XBUS_WAITFOR_XPDS);
			goto err;
		}
		SET_PROC_DIRENTRY_OWNER(worker->proc_xbus_waitfor_xpds);
	}
#endif
#endif
	worker->wq = create_singlethread_workqueue(xbus->busname);
	if(!worker->wq) {
		XBUS_ERR(xbus, "Failed to create worker workqueue.\n");
		goto err;
	}
	return 1;
err:
	worker_destroy(xbus);
	return 0;
}

bool xbus_setflags(xbus_t *xbus, int flagbit, bool on)
{
	unsigned long	flags;

	spin_lock_irqsave(&xbus->transport.state_lock, flags);
	XBUS_DBG(DEVICES, xbus, "%s flag %d\n",
			(on) ? "Set" : "Clear",
			flagbit);
	if(on)
		set_bit(flagbit, &(xbus->transport.transport_flags));
	else
		clear_bit(flagbit, &(xbus->transport.transport_flags));
	spin_unlock_irqrestore(&xbus->transport.state_lock, flags);
	return 1;
}

bool xbus_setstate(xbus_t *xbus, enum xbus_state newstate)
{
	unsigned long	flags;
	bool		ret = 0;
	int		state_flip = 0;

	spin_lock_irqsave(&xbus->transport.state_lock, flags);
	if(newstate == XBUS_STATE(xbus)) {
		XBUS_DBG(DEVICES, xbus, "stay at %s\n",
			xbus_statename(newstate));
		goto out;
	}
	/* Sanity tests */
	switch(newstate) {
		case XBUS_STATE_START:
			goto bad_state;
		case XBUS_STATE_IDLE:
			if(!XBUS_IS(xbus, START) &&
					!XBUS_IS(xbus, DEACTIVATED))
				goto bad_state;
			break;
		case XBUS_STATE_SENT_REQUEST:
			if(!XBUS_IS(xbus, IDLE) &&
					!XBUS_IS(xbus, SENT_REQUEST))
				goto bad_state;
			break;
		case XBUS_STATE_RECVD_DESC:
			if(!XBUS_IS(xbus, SENT_REQUEST))
				goto bad_state;
			break;
		case XBUS_STATE_READY:
			if(!XBUS_IS(xbus, RECVD_DESC))
				goto bad_state;
			state_flip = 1;	/* We are good */
			break;
		case XBUS_STATE_DEACTIVATING:
			if(XBUS_IS(xbus, DEACTIVATING))
				goto bad_state;
			if(XBUS_IS(xbus, DEACTIVATED))
				goto bad_state;
			break;
		case XBUS_STATE_DEACTIVATED:
			if(!XBUS_IS(xbus, DEACTIVATING))
				goto bad_state;
			break;
		case XBUS_STATE_FAIL:
			if(XBUS_IS(xbus, DEACTIVATING))
				goto bad_state;
			if(XBUS_IS(xbus, DEACTIVATED))
				goto bad_state;
			break;
		default:
			XBUS_NOTICE(xbus, "%s: unknown state %d\n", __FUNCTION__, newstate);
			goto out;
	}
	/* All good */
	XBUS_DBG(DEVICES, xbus, "%s -> %s\n",
		xbus_statename(XBUS_STATE(xbus)),
		xbus_statename(newstate));
	if(xbus->transport.xbus_state == XBUS_STATE_READY && newstate != XBUS_STATE_READY)
		state_flip = -1;	/* We became bad */
	xbus->transport.xbus_state = newstate;
	ret = 1;
out:
	spin_unlock_irqrestore(&xbus->transport.state_lock, flags);
	/* Should be sent out of spinlocks */
	if(state_flip > 0)
		astribank_uevent_send(xbus, KOBJ_ONLINE);
	else if(state_flip < 0)
		astribank_uevent_send(xbus, KOBJ_OFFLINE);
	return ret;
bad_state:
	XBUS_NOTICE(xbus, "Bad state transition %s -> %s ignored.\n",
		xbus_statename(XBUS_STATE(xbus)),
		xbus_statename(newstate));
	goto out;
}

int xbus_activate(xbus_t *xbus)
{
	XBUS_INFO(xbus, "[%s] Activating\n", xbus->label);
	xpp_drift_init(xbus);
	xbus_set_command_timer(xbus, 1);
	xframe_queue_disable(&xbus->command_queue, 0);
	xbus_setstate(xbus, XBUS_STATE_IDLE);	/* must be done after transport is valid */
	CALL_PROTO(GLOBAL, AB_REQUEST, xbus, NULL);
	/*
	 * Make sure Astribank knows not to send us ticks.
	 */
	xbus_request_sync(xbus, SYNC_MODE_NONE);
	return 0;
}

int xbus_connect(xbus_t *xbus)
{
	struct xbus_ops		*ops;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "\n");
	ops = transportops_get(xbus);
	BUG_ON(!ops);
	/* Sanity checks */
	BUG_ON(!ops->xframe_send_pcm);
	BUG_ON(!ops->xframe_send_cmd);
	BUG_ON(!ops->alloc_xframe);
	BUG_ON(!ops->free_xframe);
	xbus_setflags(xbus, XBUS_FLAG_CONNECTED, 1);
	xbus_activate(xbus);
	return 0;
}

void xbus_deactivate(xbus_t *xbus)
{
	BUG_ON(!xbus);
	XBUS_INFO(xbus, "[%s] Deactivating\n", xbus->label);
	if(!xbus_setstate(xbus, XBUS_STATE_DEACTIVATING))
		return;
	xbus_request_sync(xbus, SYNC_MODE_NONE);	/* no more ticks */
	elect_syncer("deactivate");
	xbus_request_removal(xbus);
	XBUS_DBG(DEVICES, xbus, "[%s] Waiting for queues\n", xbus->label);
	xbus_command_queue_clean(xbus);
	xbus_command_queue_waitempty(xbus);
	xbus_setstate(xbus, XBUS_STATE_DEACTIVATED);
	worker_reset(xbus);
	xbus_release_xpds(xbus);	/* taken in xpd_alloc() [kref_init] */
}

void xbus_disconnect(xbus_t *xbus)
{
	BUG_ON(!xbus);
	XBUS_INFO(xbus, "[%s] Disconnecting\n", xbus->label);
	xbus_setflags(xbus, XBUS_FLAG_CONNECTED, 0);
	xbus_deactivate(xbus);
	xbus_command_queue_clean(xbus);
	xbus_command_queue_waitempty(xbus);
	tasklet_kill(&xbus->receive_tasklet);
	xframe_queue_clear(&xbus->receive_queue);
	xframe_queue_clear(&xbus->send_pool);
	xframe_queue_clear(&xbus->receive_pool);
	xframe_queue_clear(&xbus->pcm_tospan);
	del_timer_sync(&xbus->command_timer);
	transportops_put(xbus);
	transport_destroy(xbus);
	worker_destroy(xbus);
	XBUS_DBG(DEVICES, xbus, "Deactivated refcount_xbus=%d\n",
		refcount_xbus(xbus));
	xbus_sysfs_transport_remove(xbus);	/* Device-Model */
	put_xbus(__func__, xbus);	/* from xbus_new() [kref_init()] */
}

static xbus_t *xbus_alloc(void)
{
	unsigned long	flags;
	xbus_t		*xbus;
	int		i;

	xbus = KZALLOC(sizeof(xbus_t), GFP_KERNEL);
	if(!xbus) {
		ERR("%s: out of memory\n", __FUNCTION__);
		return NULL;
	}
	spin_lock_irqsave(&xbuses_lock, flags);
	for(i = 0; i < MAX_BUSES; i++)
		if(xbuses_array[i].xbus == NULL)
			break;
	if(i >= MAX_BUSES) {
		ERR("%s: No free slot for new bus. i=%d\n", __FUNCTION__, i);
		KZFREE(xbus);
		xbus = NULL;
		goto out;
	}
	/* Found empty slot */
	xbus->num = i;
	init_xbus(i, xbus);
out:
	spin_unlock_irqrestore(&xbuses_lock, flags);
	return xbus;
}


void xbus_free(xbus_t *xbus)
{
	unsigned long	flags;
	uint		num;

	if(!xbus)
		return;
	XBUS_DBG(DEVICES, xbus, "Free\n");
	spin_lock_irqsave(&xbuses_lock, flags);
	num = xbus->num;
	BUG_ON(!xbuses_array[num].xbus);
	BUG_ON(xbus != xbuses_array[num].xbus);
	spin_unlock_irqrestore(&xbuses_lock, flags);
#ifdef	XPP_DEBUGFS
	if(xbus->debugfs_dir) {
		if(xbus->debugfs_file) {
			XBUS_DBG(GENERAL, xbus, "Removing debugfs file\n");
			debugfs_remove(xbus->debugfs_file);
		}
		XBUS_DBG(GENERAL, xbus, "Removing debugfs directory\n");
		debugfs_remove(xbus->debugfs_dir);
	}
#endif
#ifdef CONFIG_PROC_FS
	if(xbus->proc_xbus_dir) {
		if(xbus->proc_xbus_summary) {
			XBUS_DBG(PROC, xbus, "Removing proc '%s'\n", PROC_XBUS_SUMMARY);
			remove_proc_entry(PROC_XBUS_SUMMARY, xbus->proc_xbus_dir);
			xbus->proc_xbus_summary = NULL;
		}
#ifdef	PROTOCOL_DEBUG
		if(xbus->proc_xbus_command) {
			XBUS_DBG(PROC, xbus, "Removing proc '%s'\n", PROC_XBUS_COMMAND);
			remove_proc_entry(PROC_XBUS_COMMAND, xbus->proc_xbus_dir);
			xbus->proc_xbus_command = NULL;
		}
#endif
		XBUS_DBG(PROC, xbus, "Removing proc directory\n");
		remove_proc_entry(xbus->busname, xpp_proc_toplevel);
		xbus->proc_xbus_dir = NULL;
	}
#endif
	spin_lock_irqsave(&xbuses_lock, flags);
	XBUS_DBG(DEVICES, xbus, "Going to free...\n");
	init_xbus(num, NULL);
	spin_unlock_irqrestore(&xbuses_lock, flags);
	KZFREE(xbus);
}

xbus_t *xbus_new(struct xbus_ops *ops, ushort max_send_size, struct device *transport_device, void *priv)
{
	int			err;
	xbus_t			*xbus = NULL;

	BUG_ON(!ops);
	xbus = xbus_alloc();
	if(!xbus) {
		ERR("%s: Failed allocating new xbus\n", __FUNCTION__);
		module_put(THIS_MODULE);
		return NULL;
	}
	snprintf(xbus->busname, XBUS_NAMELEN, "XBUS-%02d", xbus->num);
	XBUS_DBG(DEVICES, xbus, "\n");
	transport_init(xbus, ops, max_send_size, transport_device, priv);
	spin_lock_init(&xbus->lock);
	init_waitqueue_head(&xbus->command_queue_empty);
	init_timer(&xbus->command_timer);
	atomic_set(&xbus->pcm_rx_counter, 0);
	xbus->min_tx_sync = INT_MAX;
	xbus->min_rx_sync = INT_MAX;
	
	kref_init(&xbus->kref);
	worker_init(xbus);
	atomic_set(&xbus->num_xpds, 0);
	xbus->sync_mode = SYNC_MODE_NONE;
	xbus->sync_mode_default = SYNC_MODE_PLL;
	err = xbus_sysfs_create(xbus);
	if(err) {
		XBUS_ERR(xbus, "SYSFS creation failed: %d\n", err);
		goto nobus;
	}
	err = xbus_sysfs_transport_create(xbus);
	if (err) {
		XBUS_ERR(xbus, "SYSFS transport link creation failed: %d\n",
				err);
		goto nobus;
	}
	xbus_reset_counters(xbus);
#ifdef CONFIG_PROC_FS
	XBUS_DBG(PROC, xbus, "Creating xbus proc directory\n");
	xbus->proc_xbus_dir = proc_mkdir(xbus->busname, xpp_proc_toplevel);
	if(!xbus->proc_xbus_dir) {
		XBUS_ERR(xbus, "Failed to create proc directory\n");
		err = -EIO;
		goto nobus;
	}
	xbus->proc_xbus_summary = create_proc_read_entry(PROC_XBUS_SUMMARY,
			0444, xbus->proc_xbus_dir,
			xbus_read_proc,
			(void *)((unsigned long)(xbus->num)));
	if (!xbus->proc_xbus_summary) {
		XBUS_ERR(xbus, "Failed to create proc file '%s'\n", PROC_XBUS_SUMMARY);
		err = -EIO;
		goto nobus;
	}
	SET_PROC_DIRENTRY_OWNER(xbus->proc_xbus_summary);
#ifdef	PROTOCOL_DEBUG
	xbus->proc_xbus_command = create_proc_entry(PROC_XBUS_COMMAND, 0200, xbus->proc_xbus_dir);
	if (!xbus->proc_xbus_command) {
		XBUS_ERR(xbus, "Failed to create proc file '%s'\n", PROC_XBUS_COMMAND);
		err = -EIO;
		goto nobus;
	}
	xbus->proc_xbus_command->write_proc = proc_xbus_command_write;
	xbus->proc_xbus_command->data = xbus;
	SET_PROC_DIRENTRY_OWNER(xbus->proc_xbus_command);
#endif
#endif
#ifdef	XPP_DEBUGFS
	xbus->debugfs_dir = debugfs_create_dir(xbus->busname, debugfs_root);
	if(!xbus->debugfs_dir) {
		XBUS_ERR(xbus, "Failed to create debugfs directory\n");
		goto nobus;
	}
	xbus->debugfs_file = debugfs_create_file("dchannel", S_IFREG|S_IRUGO|S_IWUSR, xbus->debugfs_dir, xbus, &debugfs_operations);
	if(!xbus->debugfs_file) {
		XBUS_ERR(xbus, "Failed to create dchannel file\n");
		goto nobus;
	}
#endif
	xframe_queue_init(&xbus->command_queue, 10, command_queue_length, "command_queue", xbus);
	xframe_queue_init(&xbus->receive_queue, 10, 50, "receive_queue", xbus);
	xframe_queue_init(&xbus->send_pool, 10, 100, "send_pool", xbus);
	xframe_queue_init(&xbus->receive_pool, 10, 50, "receive_pool", xbus);
	xframe_queue_init(&xbus->pcm_tospan, 5, 10, "pcm_tospan", xbus);
	tasklet_init(&xbus->receive_tasklet, receive_tasklet_func, (unsigned long)xbus);
	/*
	 * Create worker after /proc/XBUS-?? so the directory exists
	 * before /proc/XBUS-??/waitfor_xpds tries to get created.
	 */
	if (!worker_run(xbus)) {
		ERR("Failed to allocate worker\n");
		goto nobus;
	}
	return xbus;
nobus:
	xbus_free(xbus);
	return NULL;
}

/*------------------------- Proc handling --------------------------*/

void xbus_reset_counters(xbus_t *xbus)
{
	int	i;

	XBUS_DBG(GENERAL, xbus, "Reseting counters\n");
	for(i = 0; i < XBUS_COUNTER_MAX; i++) {
		xbus->counters[i] = 0;
	}
}

static bool xpds_done(xbus_t *xbus)
{
	if (XBUS_IS(xbus, FAIL))
		return 1;	/* Nothing to wait for */
	if (!XBUS_IS(xbus, RECVD_DESC))
		return 1;	/* We are not in the initialization phase */
	if (xbus->worker.xpds_init_done)
		return 1;	/* All good */
	/* Keep waiting */
	return 0;
}

int waitfor_xpds(xbus_t *xbus, char *buf)
{
	struct xbus_workqueue	*worker;
	unsigned long		flags;
	int			ret;
	int			len = 0;

	/*
	 * FIXME: worker is created before ?????
	 * So by now it exists and initialized.
	 */
	/* until end of waitfor_xpds_show(): */
	xbus = get_xbus(__func__, xbus->num);
	if (!xbus)
		return -ENODEV;
	worker = &xbus->worker;
	BUG_ON(!worker);
	if (!worker->wq) {
		XBUS_ERR(xbus, "Missing worker thread. Skipping.\n");
		len = -ENODEV;
		goto out;
	}
	if (worker->num_units == 0) {
		XBUS_ERR(xbus, "No cards. Skipping.\n");
		goto out;
	}
	XBUS_DBG(DEVICES, xbus,
		"Waiting for card init of %d XPD's max %d seconds (%p)\n",
		worker->num_units,
		INITIALIZATION_TIMEOUT/HZ,
		&worker->wait_for_xpd_initialization);
	ret = wait_event_interruptible_timeout(
		worker->wait_for_xpd_initialization,
		xpds_done(xbus),
		INITIALIZATION_TIMEOUT);
	if (ret == 0) {
		XBUS_ERR(xbus, "Card Initialization Timeout\n");
		len = -ETIMEDOUT;
		goto out;
	} else if (ret < 0) {
		XBUS_ERR(xbus, "Card Initialization Interrupted %d\n", ret);
		len = ret;
		goto out;
	} else
		XBUS_DBG(DEVICES, xbus,
			"Finished initialization of %d XPD's in %d seconds.\n",
			worker->num_units_initialized,
			(INITIALIZATION_TIMEOUT - ret)/HZ);
	if (XBUS_IS(xbus, FAIL)) {
		len += sprintf(buf, "FAILED: %s\n", xbus->busname);
	} else {
		spin_lock_irqsave(&xbus->lock, flags);
		len += sprintf(buf, "XPDS_READY: %s: %d/%d\n",
			xbus->busname,
			worker->num_units_initialized, worker->num_units);
		spin_unlock_irqrestore(&xbus->lock, flags);
	}
out:
	put_xbus(__func__, xbus);	/* from start of waitfor_xpds_show() */
	return len;
}

#ifdef CONFIG_PROC_FS

static int xbus_fill_proc_queue(char *p, struct xframe_queue *q)
{
	int	len;

	len = sprintf(p,
			"%-15s: counts %3d, %3d, %3d worst %3d, overflows %3d worst_lag %02ld.%ld ms\n",
				q->name,
				q->steady_state_count,
				q->count,
				q->max_count,
				q->worst_count,
				q->overflows,
				q->worst_lag_usec / 1000,
				q->worst_lag_usec % 1000);
	xframe_queue_clearstats(q);
	return len;
}

static int xbus_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	xbus_t			*xbus;
	struct xbus_workqueue	*worker;
	unsigned long		flags;
	int			len = 0;
	int			i = (int)((unsigned long)data);

	xbus = get_xbus(__func__, i);	/* until end of xbus_read_proc */
	if(!xbus)
		goto out;
	spin_lock_irqsave(&xbus->lock, flags);
	worker = &xbus->worker;

	len += sprintf(page + len, "%s: CONNECTOR=%s LABEL=[%s] STATUS=%s\n",
			xbus->busname,
			xbus->connector,
			xbus->label,
			(XBUS_FLAGS(xbus, CONNECTED)) ? "connected" : "missing"
		      );
	len += xbus_fill_proc_queue(page + len, &xbus->send_pool);
	len += xbus_fill_proc_queue(page + len, &xbus->receive_pool);
	len += xbus_fill_proc_queue(page + len, &xbus->command_queue);
	len += xbus_fill_proc_queue(page + len, &xbus->receive_queue);
	len += xbus_fill_proc_queue(page + len, &xbus->pcm_tospan);
	if(rx_tasklet) {
		len += sprintf(page + len, "\ncpu_rcv_intr:    ");
		for_each_online_cpu(i)
			len += sprintf(page + len, "%5d ", xbus->cpu_rcv_intr[i]);
		len += sprintf(page + len, "\ncpu_rcv_tasklet: ");
		for_each_online_cpu(i)
			len += sprintf(page + len, "%5d ", xbus->cpu_rcv_tasklet[i]);
		len += sprintf(page + len, "\n");
	}
	len += sprintf(page + len, "self_ticking: %d (last_tick at %ld)\n",
			xbus->self_ticking, xbus->ticker.last_sample.tv.tv_sec);
	len += sprintf(page + len, "command_tick: %d\n", xbus->command_tick_counter);
	len += sprintf(page + len, "usec_nosend: %d\n", xbus->usec_nosend);
	len += sprintf(page + len, "xbus: pcm_rx_counter = %d, frag = %d\n",
		atomic_read(&xbus->pcm_rx_counter), xbus->xbus_frag_count);
	len += sprintf(page + len, "max_rx_process = %2ld.%ld ms\n",
		xbus->max_rx_process / 1000,
		xbus->max_rx_process % 1000);
	xbus->max_rx_process = 0;
	len += sprintf(page + len, "\nTRANSPORT: max_send_size=%d refcount=%d\n",
			MAX_SEND_SIZE(xbus),
			atomic_read(&xbus->transport.transport_refcount)
			);
	len += sprintf(page + len, "PCM Metrices:\n");
	len += sprintf(page + len, "\tPCM TX: min=%ld  max=%ld\n",
				xbus->min_tx_sync, xbus->max_tx_sync);
	len += sprintf(page + len, "\tPCM RX: min=%ld  max=%ld\n",
				xbus->min_rx_sync, xbus->max_rx_sync);
	len += sprintf(page + len, "COUNTERS:\n");
	for(i = 0; i < XBUS_COUNTER_MAX; i++) {
		len += sprintf(page + len, "\t%-15s = %d\n",
				xbus_counters[i].name, xbus->counters[i]);
	}
	len += sprintf(page + len, "<-- len=%d\n", len);
	spin_unlock_irqrestore(&xbus->lock, flags);
	put_xbus(__FUNCTION__, xbus);	/* from xbus_read_proc() */
out:
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;

}

#ifdef	OLD_PROC
static int xbus_read_waitfor_xpds(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int			len = 0;
	int			i = (int)((unsigned long)data);
	xbus_t			*xbus;

	xbus = get_xbus(__func__, i);
	if (xbus != NULL) {
		XBUS_NOTICE(xbus, "%s: DEPRECATED: %s[%d] read from /proc interface instead of /sys\n",
			__func__, current->comm, current->tgid);
		/* first handle special cases */
		if (count && !off)
			len = waitfor_xpds(xbus, page);
		put_xbus(__func__, xbus);
	}
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;

}
#endif

#ifdef	PROTOCOL_DEBUG
static int proc_xbus_command_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char			*buf;
	xbus_t			*xbus = data;
	char			*p;
	byte			*pack_start;
	byte			*q;
	xframe_t		*xframe;
	size_t			len;
	const size_t		max_len = xbus->transport.max_send_size;
	const size_t		max_text = max_len * 3 + 10;

	if(count > max_text) {
		XBUS_ERR(xbus, "%s: line too long (%ld > %zd)\n", __FUNCTION__, count, max_len);
		return -EFBIG;
	}
	/* 3 bytes per hex-digit and space */
	buf = kmalloc(max_text, GFP_KERNEL);
	if(!buf)
		return -ENOMEM;
	if(copy_from_user(buf, buffer, count)) {
		count = -EINVAL;
		goto out;
	}
	buf[count] = '\0';
	XBUS_DBG(GENERAL, xbus, "count=%ld\n", count);
	/*
	 * We replace the content of buf[] from
	 * ascii representation to packet content
	 * as the binary representation is shorter
	 */
	q = pack_start = buf;
	for(p = buf; *p;) {
		int	val;
		char	hexdigit[3];

		while(*p && isspace(*p))	// skip whitespace
			p++;
		if(!(*p))
			break;
		if(!isxdigit(*p)) {
			XBUS_ERR(xbus, "%s: bad hex value ASCII='0x%X' at position %ld\n",
					__FUNCTION__, *p, (long)(p - buf));
			count = -EINVAL;
			goto out;
		}
		hexdigit[0] = *p++;
		hexdigit[1] = '\0';
		hexdigit[2] = '\0';
		if(isxdigit(*p))
			hexdigit[1] = *p++;
		if(sscanf(hexdigit, "%2X", &val) != 1) {
			XBUS_ERR(xbus, "%s: bad hex value '%s' at position %ld\n",
					__FUNCTION__, hexdigit, (long)(p - buf));
			count = -EINVAL;
			goto out;
		}
		*q++ = val;
		XBUS_DBG(GENERAL, xbus, "%3zd> '%s' val=%d\n", q - pack_start, hexdigit, val);
	}
	len = q - pack_start;
	xframe = ALLOC_SEND_XFRAME(xbus);
	if(!xframe) {
		count = -ENOMEM;
		goto out;
	}
	if(len > max_len)
		len = max_len;
	atomic_set(&xframe->frame_len, len);
	memcpy(xframe->packets, pack_start, len);		/* FIXME: checksum? */
	dump_xframe("COMMAND", xbus, xframe, debug);
	send_cmd_frame(xbus, xframe);
out:
	kfree(buf);
	return count;
}
#endif


static int read_proc_xbuses(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int i;

	for(i = 0; i < MAX_BUSES; i++) {
		xbus_t *xbus = get_xbus(__func__, i);

		if(xbus) {
			len += sprintf(page + len, "%s: CONNECTOR=%s LABEL=[%s] STATUS=%s\n",
					xbus->busname,
					xbus->connector,
					xbus->label,
					(XBUS_FLAGS(xbus, CONNECTED)) ? "connected" : "missing"
				      );
			put_xbus(__func__, xbus);
		}
	}
#if 0
	len += sprintf(page + len, "<-- len=%d\n", len);
#endif
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;

}
#endif

static void transport_init(xbus_t *xbus, struct xbus_ops *ops, ushort max_send_size, struct device *transport_device, void *priv)
{
	BUG_ON(!xbus);
	BUG_ON(!ops);
	BUG_ON(!ops->xframe_send_pcm);
	BUG_ON(!ops->xframe_send_cmd);
	BUG_ON(!ops->alloc_xframe);
	BUG_ON(!ops->free_xframe);
	xbus->transport.ops = ops;
	xbus->transport.max_send_size = max_send_size;
	xbus->transport.transport_device = transport_device;
	xbus->transport.priv = priv;
	xbus->transport.xbus_state = XBUS_STATE_START;
	spin_lock_init(&xbus->transport.state_lock);
	spin_lock_init(&xbus->transport.lock);
	atomic_set(&xbus->transport.transport_refcount, 0);
	xbus_setflags(xbus, XBUS_FLAG_CONNECTED, 0);
	init_waitqueue_head(&xbus->transport.transport_unused);
}

static void transport_destroy(xbus_t *xbus)
{
	int	ret;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "Waiting... (transport_refcount=%d)\n",
		atomic_read(&xbus->transport.transport_refcount));
	ret = wait_event_interruptible(xbus->transport.transport_unused,
			atomic_read(&xbus->transport.transport_refcount) == 0);
	if(ret)
		XBUS_ERR(xbus, "Waiting for transport_refcount interrupted!!!\n");
	xbus->transport.ops = NULL;
	xbus->transport.priv = NULL;
}

struct xbus_ops *transportops_get(xbus_t *xbus)
{
	struct xbus_ops	*ops;

	BUG_ON(!xbus);
	atomic_inc(&xbus->transport.transport_refcount);
	ops = xbus->transport.ops;
	if(!ops)
		atomic_dec(&xbus->transport.transport_refcount);
	/* fall through */
	return ops;
}

void transportops_put(xbus_t *xbus)
{
	struct xbus_ops	*ops;

	BUG_ON(!xbus);
	ops = xbus->transport.ops;
	BUG_ON(!ops);
	if(atomic_dec_and_test(&xbus->transport.transport_refcount))
		wake_up(&xbus->transport.transport_unused);
}

/*------------------------- Initialization -------------------------*/
static void xbus_core_cleanup(void)
{
	finalize_xbuses_array();
#ifdef	XPP_DEBUGFS
	if(debugfs_root) {
		DBG(GENERAL, "Removing xpp from debugfs\n");
		debugfs_remove(debugfs_root);
	}
#endif
#ifdef CONFIG_PROC_FS
	if(proc_xbuses) {
		DBG(PROC, "Removing " PROC_XBUSES " from proc\n");
		remove_proc_entry(PROC_XBUSES, xpp_proc_toplevel);
		proc_xbuses = NULL;
	}
#endif
}

int __init xbus_core_init(void)
{
	int	ret = 0;

	initialize_xbuses_array();
#ifdef PROTOCOL_DEBUG
	INFO("FEATURE: with PROTOCOL_DEBUG\n");
#endif
#ifdef	XPP_DEBUGFS
	INFO("FEATURE: with XPP_DEBUGFS support\n");
#endif
#ifdef CONFIG_PROC_FS
	proc_xbuses = create_proc_read_entry(PROC_XBUSES, 0444, xpp_proc_toplevel, read_proc_xbuses, NULL);
	if (!proc_xbuses) {
		ERR("Failed to create proc file %s\n", PROC_XBUSES);
		ret = -EFAULT;
		goto err;
	}
	SET_PROC_DIRENTRY_OWNER(proc_xbuses);
#endif
#ifdef	XPP_DEBUGFS
	DBG(GENERAL, "Creating debugfs xpp root\n");
	debugfs_root = debugfs_create_dir("xpp", NULL);
	if(!debugfs_root) {
		ERR("Failed to create debugfs root\n");
		ret = -EFAULT;
		goto err;
	}
#endif
	if((ret = xpp_driver_init()) < 0)
		goto err;
	return 0;
err:
	xbus_core_cleanup();
	return ret;
}


void xbus_core_shutdown(void)
{
	int		i;

	for(i = 0; i < MAX_BUSES; i++)
		BUG_ON(xbus_num(i));
	xbus_core_cleanup();
	xpp_driver_exit();
}

EXPORT_SYMBOL(xpd_of);
EXPORT_SYMBOL(xpd_byaddr);
EXPORT_SYMBOL(xbus_num);
EXPORT_SYMBOL(xbus_setstate);
EXPORT_SYMBOL(xbus_statename);
EXPORT_SYMBOL(xbus_new);
EXPORT_SYMBOL(xbus_free);
EXPORT_SYMBOL(xbus_connect);
EXPORT_SYMBOL(xbus_activate);
EXPORT_SYMBOL(xbus_deactivate);
EXPORT_SYMBOL(xbus_disconnect);
EXPORT_SYMBOL(xbus_receive_xframe);
EXPORT_SYMBOL(xbus_reset_counters);
EXPORT_SYMBOL(xframe_next_packet);
EXPORT_SYMBOL(dump_xframe);
EXPORT_SYMBOL(send_pcm_frame);
EXPORT_SYMBOL(send_cmd_frame);
EXPORT_SYMBOL(xframe_init);
EXPORT_SYMBOL(transportops_get);
EXPORT_SYMBOL(transportops_put);
EXPORT_SYMBOL(xbus_command_queue_tick);
#ifdef XPP_DEBUGFS
EXPORT_SYMBOL(xbus_log);
#endif
