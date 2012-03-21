/*
 * VoiceBus(tm) Interface Library.
 *
 * Written by Shaun Ruffell <sruffell@digium.com>
 * and based on previous work by Mark Spencer <markster@digium.com>, 
 * Matthew Fredrickson <creslin@digium.com>, and
 * Michael Spiceland <mspiceland@digium.com>
 * 
 * Copyright (C) 2007-2010 Digium, Inc.
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


#ifndef __VOICEBUS_H__
#define __VOICEBUS_H__

#include <linux/interrupt.h>


#define VOICEBUS_DEFAULT_LATENCY	3U
#define VOICEBUS_DEFAULT_MAXLATENCY	25U
#define VOICEBUS_MAXLATENCY_BUMP	6U

#define VOICEBUS_SFRAME_SIZE 1004U

/*! The number of descriptors in both the tx and rx descriptor ring. */
#define DRING_SIZE	(1 << 7)  /* Must be a power of 2 */
#define DRING_MASK	(DRING_SIZE-1)

/* Define CONFIG_VOICEBUS_SYSFS to create some attributes under the pci device.
 * This is disabled by default because it hasn't been tested on the full range
 * of supported kernels. */
#undef CONFIG_VOICEBUS_SYSFS

/* Do not generate interrupts on this interface, but instead just poll it */
#undef CONFIG_VOICEBUS_TIMER

/* Define this in order to create a debugging network interface. */
#undef VOICEBUS_NET_DEBUG

/* Define this to only run the processing in an interrupt handler
 * (and not tasklet). */
#define CONFIG_VOICEBUS_INTERRUPT

/*
 * Enable the following definition in order to disable Active-State Power
 * Management on the PCIe bridge for PCIe cards. This has been known to work
 * around issues where the BIOS enables it on the cards even though the
 * platform does not support it.
 *
 */
#undef CONFIG_VOICEBUS_DISABLE_ASPM

/* Define this to use a FIFO for the software echocan reference.
 * (experimental) */
#undef CONFIG_VOICEBUS_ECREFERENCE

#ifdef CONFIG_VOICEBUS_ECREFERENCE

struct dahdi_fifo;
unsigned int __dahdi_fifo_put(struct dahdi_fifo *fifo, u8 *data, size_t size);
unsigned int __dahdi_fifo_get(struct dahdi_fifo *fifo, u8 *data, size_t size);
void dahdi_fifo_free(struct dahdi_fifo *fifo);
struct dahdi_fifo *dahdi_fifo_alloc(size_t maxsize, gfp_t alloc_flags);

#endif

#ifdef VOICEBUS_NET_DEBUG
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

struct voicebus;

struct vbb {
	u8 data[VOICEBUS_SFRAME_SIZE];
	struct list_head entry;
	dma_addr_t dma_addr;
};

struct voicebus_operations {
	void (*handle_receive)(struct voicebus *vb, struct list_head *buffers);
	void (*handle_transmit)(struct voicebus *vb, struct list_head *buffers);
	void (*handle_error)(struct voicebus *vb);
};

/**
 * struct voicebus_descriptor_list - A single descriptor list.
 */
struct voicebus_descriptor_list {
	struct voicebus_descriptor *desc;
	unsigned int 	head;
	unsigned int 	tail;
	void  		*pending[DRING_SIZE];
	dma_addr_t	desc_dma;
	unsigned long	count;
	unsigned int	padding;
};

/* Bit definitions for struct voicebus.flags */
#define VOICEBUS_SHUTDOWN			0
#define VOICEBUS_STOP				1
#define VOICEBUS_STOPPED			2
#define VOICEBUS_LATENCY_LOCKED			3
#define VOICEBUS_HARD_UNDERRUN			4

/**
 * voicebus_mode
 *
 * NORMAL:	For non-hx8 boards.  Uses idle_buffers.
 * BOOT:	For hx8 boards.  For sending single packets at a time.
 * HX8:		Normal operating mode for Hx8 Boards.  Does not use
 *		idle_buffers.
 */
enum voicebus_mode {
	NORMAL = 0,
	BOOT = 1,
	HX8 = 2,
};

/**
 * struct voicebus - Represents physical interface to voicebus card.
 *
 * @tx_complete: only used in the tasklet to temporarily hold complete tx
 *		 buffers.
 */
struct voicebus {
	struct pci_dev		*pdev;
	spinlock_t		lock;
	struct voicebus_descriptor_list rxd;
	struct voicebus_descriptor_list txd;
	u8			*idle_vbb;
	dma_addr_t		idle_vbb_dma_addr;
	const int		*debug;
	void __iomem 		*iobase;
	struct tasklet_struct 	tasklet;
	enum voicebus_mode	mode;

#if defined(CONFIG_VOICEBUS_INTERRUPT)
	atomic_t		deferred_disabled_count;
#endif

#if defined(CONFIG_VOICEBUS_TIMER)
	struct timer_list	timer;
#endif

	struct work_struct	underrun_work;
	const struct voicebus_operations *ops;
	unsigned long		flags;
	unsigned int		min_tx_buffer_count;
	unsigned int		max_latency;
	struct list_head	tx_complete;
	struct list_head	free_rx;
	struct dma_pool		*pool;

#ifdef VOICEBUS_NET_DEBUG
	struct sk_buff_head captured_packets;
	struct net_device *netdev;
	struct net_device_stats net_stats;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	struct napi_struct napi;
#endif
	atomic_t tx_seqnum;
	atomic_t rx_seqnum;
#endif
};

int __voicebus_init(struct voicebus *vb, const char *board_name,
		    enum voicebus_mode mode);
void voicebus_release(struct voicebus *vb);
int voicebus_start(struct voicebus *vb);
void voicebus_stop(struct voicebus *vb);
void voicebus_quiesce(struct voicebus *vb);
int voicebus_transmit(struct voicebus *vb, struct vbb *vbb);
int voicebus_set_minlatency(struct voicebus *vb, unsigned int milliseconds);
int voicebus_current_latency(struct voicebus *vb);

static inline int voicebus_init(struct voicebus *vb, const char *board_name)
{
	return __voicebus_init(vb, board_name, NORMAL);
}

static inline int
voicebus_boot_init(struct voicebus *vb, const char *board_name)
{
	return __voicebus_init(vb, board_name, BOOT);
}

/**
 * voicebus_lock_latency() - Do not increase the latency during underruns.
 *
 */
static inline void voicebus_lock_latency(struct voicebus *vb)
{
	set_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags);
}

/**
 * voicebus_unlock_latency() - Bump up the latency during underruns.
 *
 */
static inline void voicebus_unlock_latency(struct voicebus *vb)
{
	clear_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags);
}

/**
 * voicebus_is_latency_locked() - Return 1 if latency is currently locked.
 *
 */
static inline int voicebus_is_latency_locked(const struct voicebus *vb)
{
	return test_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags);
}

static inline void voicebus_set_normal_mode(struct voicebus *vb)
{
	vb->mode = NORMAL;
}

static inline void voicebus_set_hx8_mode(struct voicebus *vb)
{
	vb->mode = HX8;
}

/**
 * voicebus_set_max_latency() - Set the maximum number of milliseconds the latency will be able to grow to.
 */
static inline void
voicebus_set_maxlatency(struct voicebus *vb, unsigned int max_latency)
{
	unsigned long flags;
	spin_lock_irqsave(&vb->lock, flags);
	vb->max_latency = clamp(max_latency,
				vb->min_tx_buffer_count,
				VOICEBUS_DEFAULT_MAXLATENCY);
	spin_unlock_irqrestore(&vb->lock, flags);
}
#endif /* __VOICEBUS_H__ */
