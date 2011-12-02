/* Wildcard TC400B Driver
 *
 * Copyright (C) 2006-2010, Digium, Inc.
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
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/etherdevice.h>
#include <linux/timer.h>

#include <stdbool.h>

#include "dahdi/kernel.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/io.h>
#else
#include <linux/io.h>
#endif

/* COMPILE TIME OPTIONS =================================================== */

#define INTERRUPT 0
#define WORKQUEUE 1
#define TASKLET   2

#ifndef DEFERRED_PROCESSING
#	define DEFERRED_PROCESSING WORKQUEUE
#endif

#if DEFERRED_PROCESSING == INTERRUPT
#	define ALLOC_FLAGS GFP_ATOMIC
#elif DEFERRED_PROCESSING == TASKLET
#	define ALLOC_FLAGS GFP_ATOMIC
#else
#	define ALLOC_FLAGS GFP_KERNEL
#endif

#define WARN_ALWAYS() WARN_ON(1)

#define DTE_DEBUG(_dbgmask, _fmt, _args...)				\
	if ((debug & _dbgmask) == (_dbgmask)) {				\
		dev_info(&(wc)->pdev->dev, _fmt, ## _args);		\
	}								\

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
/* also added in RHEL kernels with the OpenInfiniband backport: */
#if LINUX_VERSION_CODE != KERNEL_VERSION(2, 6, 9) || !defined(DEFINE_SPINLOCK)
typedef	unsigned gfp_t;		/* Added in 2.6.14 */
#endif
#endif


/* define CONFIG_WCTC4XXP_POLLING to operate in a pure polling mode.  This is
 * was placed in as a debugging tool for a particluar system that wasn't
 * routing the interrupt properly. Therefore it is off by default and the
 * driver must be recompiled to enable it. */
#undef CONFIG_WCTC4XXP_POLLING

/* The total number of active channels over which the driver will start polling
 * the card every 10 ms. */
#define POLLING_CALL_THRESHOLD 40

#define INVALID 999 /* Used to mark invalid channels, commands, etc.. */
#define MAX_CHANNEL_PACKETS  5

#define G729_LENGTH	20
#define G723_LENGTH	30

#define G729_SAMPLES	160	/* G.729 */
#define G723_SAMPLES	240 	/* G.723.1 */

#define G729_BYTES	20	/* G.729 */
#define G723_6K_BYTES	24 	/* G.723.1 at 6.3kb/s */
#define G723_5K_BYTES	20	/* G.723.1 at 5.3kb/s */
#define G723_SID_BYTES	4	/* G.723.1 SID frame */

#define MAX_CAPTURED_PACKETS 5000

/* The following bit fields are used to set the various debug levels. */
#define DTE_DEBUG_GENERAL	(1 << 0) /* 1  */
#define DTE_DEBUG_CHANNEL_SETUP	(1 << 1) /* 2  */
#define DTE_DEBUG_RTP_TX	(1 << 2) /* 4  */
#define DTE_DEBUG_RTP_RX	(1 << 3) /* 8  */
#define DTE_DEBUG_RX_TIMEOUT	(1 << 4) /* 16 */
#define DTE_DEBUG_NETWORK_IF	(1 << 5) /* 32 */
#define DTE_DEBUG_NETWORK_EARLY	(1 << 6) /* 64 */

static int debug;
static char *mode;

static spinlock_t wctc4xxp_list_lock;
static struct list_head wctc4xxp_list;

#define ETH_P_CSM_ENCAPS 0x889B

struct rtphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8    csrc_count:4;
	__u8    extension:1;
	__u8    padding:1;
	__u8    ver:2;
	__u8    type:7;
	__u8 	marker:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8    ver:2;
	__u8    padding:1;
	__u8    extension:1;
	__u8    csrc_count:4;
	__u8 	marker:1;
	__u8    type:7;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	__be16	seqno;
	__be32  timestamp;
	__be32  ssrc;
} __attribute__((packed));

struct rtp_packet {
	struct ethhdr ethhdr;
	struct iphdr  iphdr;
	struct udphdr udphdr;
	struct rtphdr rtphdr;
	__u8   payload[0];
} __attribute__((packed));

/* Ethernet packet type for communication control information to the DTE. */
struct csm_encaps_hdr {
	struct ethhdr ethhdr;
	/* CSM_ENCAPS HEADER */
	__be16 op_code;
	__u8   seq_num;
	__u8   control;
	__be16 channel;
	/* COMMON PART OF PAYLOAD HEADER */
	__u8   length;
	__u8   index;
	__u8   type;
	__u8   class;
	__le16 function;
	__le16 reserved;
	__le16 params[0];
} __attribute__((packed));

#define CONTROL_PACKET_OPCODE  0x0001
/* Control bits */
#define LITTLE_ENDIAN	0x01
#define SUPPRESS_ACK	0x40
#define MESSAGE_PACKET	0x80

#define SUPERVISOR_CHANNEL 0xffff

/* Supervisor function codes */
#define SUPVSR_CREATE_CHANNEL	0x0010

#define MONITOR_LIVE_INDICATION_TYPE 0x75
#define CONFIG_CHANGE_TYPE	0x00
#define CONFIG_CHANNEL_CLASS	0x02
#define CONFIG_DEVICE_CLASS	0x06

/* Individual channel config commands */
#define MAX_FRAME_SIZE 1518
#define SFRAME_SIZE MAX_FRAME_SIZE

#define DRING_SIZE (1 << 7) /* Must be a power of two */
#define DRING_MASK (DRING_SIZE-1)
#define MIN_PACKET_LEN  64

/* Transcoder buffer (tcb) */
struct tcb {
	void *data;
	struct list_head node;
	unsigned long timeout;
	unsigned long retries;
	/* NOTE:  these flags aren't bit fields because some of the flags are
	 * combinations of the other ones. */
#define DO_NOT_AUTO_FREE        (1 << 0)
#define TX_COMPLETE             (1 << 1)
#define DO_NOT_CAPTURE          (1 << 2)
#define __WAIT_FOR_ACK          (1 << 3)
#define __WAIT_FOR_RESPONSE     (1 << 4)
#define DTE_CMD_TIMEOUT         (1 << 5)
#define WAIT_FOR_ACK (__WAIT_FOR_ACK | DO_NOT_AUTO_FREE)
#define WAIT_FOR_RESPONSE (__WAIT_FOR_RESPONSE | DO_NOT_AUTO_FREE)
	unsigned long flags;
	struct tcb *response;
	struct completion complete;
	struct timer_list timer;
	/* The number of bytes available in data. */
	int data_len;
	spinlock_t lock;
};

static inline const struct csm_encaps_hdr *
response_header(struct tcb *cmd)
{
	BUG_ON(!cmd->response);
	return (const struct csm_encaps_hdr *)(cmd)->response->data;
}

static inline void
initialize_cmd(struct tcb *cmd, unsigned long cmd_flags)
{
	INIT_LIST_HEAD(&cmd->node);
	init_completion(&cmd->complete);
	cmd->flags = cmd_flags;
	spin_lock_init(&cmd->lock);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
/*! Used to allocate commands to submit to the dte. */
kmem_cache_t *cmd_cache;
#else
/*! Used to allocate commands to submit to the dte. */
static struct kmem_cache *cmd_cache;
#endif

static inline struct tcb *
__alloc_cmd(size_t size, gfp_t alloc_flags, unsigned long cmd_flags)
{
	struct tcb *cmd;

	if (unlikely(size > SFRAME_SIZE))
		return NULL;
	if (size < MIN_PACKET_LEN)
		size = MIN_PACKET_LEN;
	cmd = kmem_cache_alloc(cmd_cache, alloc_flags);
	if (likely(cmd)) {
		memset(cmd, 0, sizeof(*cmd));
		cmd->data = kzalloc(size, alloc_flags);
		if (unlikely(!cmd->data)) {
			kmem_cache_free(cmd_cache, cmd);
			return NULL;
		}
		cmd->data_len = size;
		initialize_cmd(cmd, cmd_flags);
	}
	return cmd;
}

static struct tcb *
alloc_cmd(size_t size)
{
	return __alloc_cmd(size, GFP_KERNEL, 0);
}

static void
__free_cmd(struct tcb *cmd)
{
	if (cmd)
		kfree(cmd->data);
	kmem_cache_free(cmd_cache, cmd);
	return;
}

static void
free_cmd(struct tcb *cmd)
{
	if (cmd->response)
		__free_cmd(cmd->response);
	__free_cmd(cmd);
}

struct channel_stats {
	atomic_t packets_sent;
	atomic_t packets_received;
};

struct channel_pvt {
	spinlock_t lock;	/* Lock for this structure */
	struct wcdte *wc;
	u16 seqno;
	u8 cmd_seqno;
	u8 ssrc;
	u16 timeslot_in_num;	/* DTE timeslot to receive from */
	u16 timeslot_out_num;	/* DTE timeslot to send data to */
	u16 chan_in_num;	/* DTE channel to receive from */
	u16 chan_out_num;	/* DTE channel to send data to */
	u32 timestamp;
	struct {
		u8 encoder:1;	/* If we're an encoder */
	};
	struct channel_stats stats;
	struct list_head rx_queue; /* Transcoded packets for this channel. */
};

struct wcdte {
	char board_name[40];
	const char *variety;
	int pos;
	struct list_head node;
	spinlock_t reglock;
	wait_queue_head_t waitq;
	struct semaphore chansem;
#define DTE_READY	1
#define DTE_SHUTDOWN	2
#define DTE_POLLING	3
	unsigned long flags;

	/* This is a device-global list of commands that are waiting to be
	 * transmited (and did not fit on the transmit descriptor ring) */
	spinlock_t cmd_list_lock;
	struct list_head cmd_list;
	struct list_head waiting_for_response_list;

	spinlock_t rx_list_lock;
	struct list_head rx_list;
	spinlock_t rx_lock;

	unsigned int seq_num;
	int last_rx_seq_num;
	unsigned char numchannels;
	unsigned char complexname[40];

	/* This section contains the members necessary to communicate with the
	 * physical interface to the transcoding engine.  */
	struct pci_dev *pdev;
	unsigned int   intmask;
	void __iomem	*iobase;
	struct wctc4xxp_descriptor_ring *txd;
	struct wctc4xxp_descriptor_ring *rxd;

	struct dahdi_transcoder *uencode;
	struct dahdi_transcoder *udecode;
	struct channel_pvt *encoders;
	struct channel_pvt *decoders;

#if DEFERRED_PROCESSING == WORKQUEUE
	struct work_struct deferred_work;
#endif

	/*
	 * This section contains the members necessary for exporting the
	 * network interface to the host system.  This is only used for
	 * debugging purposes.
	 *
	 */
	struct sk_buff_head captured_packets;
	struct net_device *netdev;
	struct net_device_stats net_stats;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	struct napi_struct napi;
#endif
	struct timer_list watchdog;
	atomic_t open_channels;
	struct timer_list polling;
#if HZ > 100
	unsigned long jiffies_at_last_poll;
#endif
};

struct wcdte_netdev_priv {
	struct wcdte *wc;
};

static inline struct wcdte *
wcdte_from_netdev(struct net_device *netdev)
{
	struct wcdte_netdev_priv *priv;
	priv = netdev_priv(netdev);
	return priv->wc;
}


static inline void wctc4xxp_set_ready(struct wcdte *wc)
{
	set_bit(DTE_READY, &wc->flags);
}

static inline int wctc4xxp_is_ready(struct wcdte *wc)
{
	return test_bit(DTE_READY, &wc->flags);
}

#define DTE_FORMAT_ULAW   0x00
#define DTE_FORMAT_G723_1 0x04
#define DTE_FORMAT_ALAW   0x08
#define DTE_FORMAT_G729A  0x12
#define DTE_FORMAT_UNDEF  0xFF

static inline u8 wctc4xxp_dahdifmt_to_dtefmt(unsigned int fmt)
{
	u8 pt;

	switch (fmt) {
	case DAHDI_FORMAT_G723_1:
		pt = DTE_FORMAT_G723_1;
		break;
	case DAHDI_FORMAT_ULAW:
		pt = DTE_FORMAT_ULAW;
		break;
	case DAHDI_FORMAT_ALAW:
		pt = DTE_FORMAT_ALAW;
		break;
	case DAHDI_FORMAT_G729A:
		pt = DTE_FORMAT_G729A;
		break;
	default:
		pt = DTE_FORMAT_UNDEF;
		break;
	}

	return pt;
}

static struct sk_buff *
tcb_to_skb(struct net_device *netdev, const struct tcb *cmd)
{
	struct sk_buff *skb;
	skb = alloc_skb(cmd->data_len, in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
	if (skb) {
		skb->dev = netdev;
		skb_put(skb, cmd->data_len);
		memcpy(skb->data, cmd->data, cmd->data_len);
		skb->protocol = eth_type_trans(skb, netdev);
	}
	return skb;
}

/**
 * wctc4xxp_skb_to_cmd - Convert a socket buffer (skb) to a tcb
 * @wc: The transcoder that we're going to send this command to.
 * @skb: socket buffer to convert.
 *
 */
static struct tcb *
wctc4xxp_skb_to_cmd(struct wcdte *wc, const struct sk_buff *skb)
{
	const gfp_t alloc_flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	struct tcb *cmd;
	cmd = __alloc_cmd(skb->len, alloc_flags, 0);
	if (cmd) {
		int res;
		cmd->data_len = skb->len;
		res = skb_copy_bits(skb, 0, cmd->data, cmd->data_len);
		if (res) {
			dev_warn(&wc->pdev->dev,
			   "Failed call to skb_copy_bits.\n");
			free_cmd(cmd);
			cmd = NULL;
		}
	}
	return cmd;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
static void
wctc4xxp_net_set_multi(struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "%s promiscuity:%d\n",
	   __func__, netdev->promiscuity);
}
#else
static void
wctc4xxp_set_rx_mode(struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "%s promiscuity:%d\n",
	   __func__, netdev->promiscuity);
}
#endif

static int
wctc4xxp_net_up(struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "%s\n", __func__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_poll_enable(netdev);
#else
	napi_enable(&wc->napi);
#endif
	return 0;
}

static int
wctc4xxp_net_down(struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "%s\n", __func__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_poll_disable(netdev);
#else
	napi_disable(&wc->napi);
#endif
	return 0;
}

static void wctc4xxp_transmit_cmd(struct wcdte *, struct tcb *);

static int
wctc4xxp_net_hard_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	struct tcb *cmd;

	/* We set DO_NOT_CAPTURE because this packet was already captured by
	 * in code higher up in the networking stack.  We don't want to
	 * capture it twice.
	 */
	cmd = wctc4xxp_skb_to_cmd(wc, skb);
	if (cmd) {
		cmd->flags |= DO_NOT_CAPTURE;
		wctc4xxp_transmit_cmd(wc, cmd);
	}

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static int
wctc4xxp_net_receive(struct wcdte *wc, int max)
{
	int count = 0;
	struct sk_buff *skb;
	WARN_ON(0 == max);
	while ((skb = skb_dequeue(&wc->captured_packets))) {
		netif_receive_skb(skb);
		if (++count >= max)
			break;
	}
	return count;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int
wctc4xxp_poll(struct net_device *netdev, int *budget)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	int count = 0;
	int quota = min(netdev->quota, *budget);

	count = wctc4xxp_net_receive(wc, quota);

	*budget -=       count;
	netdev->quota -= count;

	if (!skb_queue_len(&wc->captured_packets)) {
		netif_rx_complete(netdev);
		return 0;
	} else {
		return -1;
	}
}
#else
static int
wctc4xxp_poll(struct napi_struct *napi, int budget)
{
	struct wcdte *wc = container_of(napi, struct wcdte, napi);
	int count;

	count = wctc4xxp_net_receive(wc, budget);

	if (!skb_queue_len(&wc->captured_packets)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
		netif_rx_complete(wc->netdev, &wc->napi);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
		netif_rx_complete(&wc->napi);
#else
		napi_complete(&wc->napi);
#endif
	}
	return count;
}
#endif

static struct net_device_stats *
wctc4xxp_net_get_stats(struct net_device *netdev)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	return &wc->net_stats;
}

/* Wait until this device is put into promiscuous mode, or we timeout. */
static void
wctc4xxp_net_waitfor_promiscuous(struct wcdte *wc)
{
	unsigned int seconds = 15;
	unsigned long start = jiffies;
	struct net_device *netdev = wc->netdev;

	dev_info(&wc->pdev->dev,
	   "Waiting %d seconds for adapter to be placed in " \
	   "promiscuous mode for early trace.\n", seconds);

	while (!netdev->promiscuity) {
		if (signal_pending(current)) {
			dev_info(&wc->pdev->dev,
			   "Aborting wait due to signal.\n");
			break;
		}
		msleep(100);
		if (time_after(jiffies, start + (seconds * HZ))) {
			dev_info(&wc->pdev->dev,
			   "Aborting wait due to timeout.\n");
			break;
		}
	}
}

static int  wctc4xxp_turn_off_booted_led(struct wcdte *wc);
static void wctc4xxp_turn_on_booted_led(struct wcdte *wc);

static int
wctc4xxp_net_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct wcdte *wc = wcdte_from_netdev(netdev);
	switch (cmd) {
	case 0x89f0:
		down(&wc->chansem);
		wctc4xxp_turn_off_booted_led(wc);
		break;
	case 0x89f1:
		wctc4xxp_turn_on_booted_led(wc);
		up(&wc->chansem);
		break;
	default:
		return -EOPNOTSUPP;
	};
	return 0;
}

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops wctc4xxp_netdev_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
	.ndo_set_multicast_list = &wctc4xxp_net_set_multi,
#else
	.ndo_set_rx_mode = &wctc4xxp_set_rx_mode,
#endif
	.ndo_open = &wctc4xxp_net_up,
	.ndo_stop = &wctc4xxp_net_down,
	.ndo_start_xmit = &wctc4xxp_net_hard_start_xmit,
	.ndo_get_stats = &wctc4xxp_net_get_stats,
	.ndo_do_ioctl = &wctc4xxp_net_ioctl,
};
#endif

/**
 * wctc4xxp_net_register - Register a new network interface.
 * @wc: transcoder card to register the interface for.
 *
 * The network interface is primarily used for debugging in order to watch the
 * traffic between the transcoder and the host.
 *
 */
static int
wctc4xxp_net_register(struct wcdte *wc)
{
	int res;
	struct net_device *netdev;
	struct wcdte_netdev_priv *priv;
	const char our_mac[] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

	netdev = alloc_netdev(sizeof(*priv), wc->board_name, ether_setup);
	if (!netdev)
		return -ENOMEM;
	priv = netdev_priv(netdev);
	priv->wc = wc;
	memcpy(netdev->dev_addr, our_mac, sizeof(our_mac));

#	ifdef HAVE_NET_DEVICE_OPS
	netdev->netdev_ops = &wctc4xxp_netdev_ops;
#	else
	netdev->set_multicast_list = &wctc4xxp_net_set_multi;
	netdev->open = &wctc4xxp_net_up;
	netdev->stop = &wctc4xxp_net_down;
	netdev->hard_start_xmit = &wctc4xxp_net_hard_start_xmit;
	netdev->get_stats = &wctc4xxp_net_get_stats;
	netdev->do_ioctl = &wctc4xxp_net_ioctl;
#	endif

	netdev->promiscuity = 0;
	netdev->flags |= IFF_NOARP;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netdev->poll = &wctc4xxp_poll;
	netdev->weight = 64;
#	else
	netif_napi_add(netdev, &wc->napi, &wctc4xxp_poll, 64);
#	endif

	res = register_netdev(netdev);
	if (res) {
		dev_warn(&wc->pdev->dev,
		   "Failed to register network device %s.\n",
		   wc->board_name);
		goto error_sw;
	}

	wc->netdev = netdev;
	skb_queue_head_init(&wc->captured_packets);

	if (debug & DTE_DEBUG_NETWORK_EARLY)
		wctc4xxp_net_waitfor_promiscuous(wc);

	dev_info(&wc->pdev->dev,
	   "Created network device %s for debug.\n", wc->board_name);
	return 0;

error_sw:
	if (netdev)
		free_netdev(netdev);
	return res;
}

static void
wctc4xxp_net_unregister(struct wcdte *wc)
{
	struct sk_buff *skb;

	if (!wc->netdev)
		return;
	unregister_netdev(wc->netdev);
	while ((skb = skb_dequeue(&wc->captured_packets)))
		kfree_skb(skb);
	free_netdev(wc->netdev);
	wc->netdev = NULL;
}


/**
 * wctc4xxp_net_capture_cmd - Send a tcb to the network stack.
 * @wc: transcoder that received the command.
 * @cmd: command to send to network stack.
 *
 */
static void
wctc4xxp_net_capture_cmd(struct wcdte *wc, const struct tcb *cmd)
{
	struct sk_buff *skb;
	struct net_device *netdev = wc->netdev;

	if (!netdev)
		return;

	/* No need to capture if there isn't anyone listening. */
	if (!(netdev->flags & IFF_UP))
		return;

	if (skb_queue_len(&wc->captured_packets) > MAX_CAPTURED_PACKETS) {
		WARN_ON_ONCE(1);
		return;
	}

	skb = tcb_to_skb(netdev, cmd);
	if (!skb)
		return;

	skb_queue_tail(&wc->captured_packets, skb);
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_rx_schedule(netdev);
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
	netif_rx_schedule(netdev, &wc->napi);
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	netif_rx_schedule(&wc->napi);
#	else
	napi_schedule(&wc->napi);
#	endif
	return;
}


/*! In-memory structure shared by the host and the adapter. */
struct wctc4xxp_descriptor {
	__le32 des0;
	__le32 des1;
	__le32 buffer1;
	__le32 container; /* Unused */
} __attribute__((packed));

struct wctc4xxp_descriptor_ring {
	/* Pointer to an array of descriptors to give to hardware. */
	struct wctc4xxp_descriptor *desc;
	/* Read completed buffers from the head. */
	unsigned int 	head;
	/* Write ready buffers to the tail. */
	unsigned int 	tail;
	/* Array to save the kernel virtual address of pending commands. */
	struct tcb *pending[DRING_SIZE];
	/* PCI Bus address of the descriptor list. */
	dma_addr_t	desc_dma;
	/*! either DMA_FROM_DEVICE or DMA_TO_DEVICE */
	unsigned int 	direction;
	/*! The number of buffers currently submitted to the hardware. */
	unsigned int    count;
	/*! The number of bytes to pad each descriptor for cache alignment. */
	unsigned int	padding;
	/*! Protects this structure from concurrent access. */
	spinlock_t      lock;
	/*! PCI device for the card associated with this ring. */
	struct pci_dev  *pdev;
};

/**
 * wctc4xxp_descriptor - Returns the desriptor at index.
 * @dr: The descriptor ring we're using.
 * @index: index of the descriptor we want.
 *
 * We need this function because we do not know what the padding on the
 * descriptors will be.  Otherwise, we would just use an array.
 */
static inline struct wctc4xxp_descriptor *
wctc4xxp_descriptor(struct wctc4xxp_descriptor_ring *dr, int index)
{
	return (struct wctc4xxp_descriptor *)((u8 *)dr->desc +
		((sizeof(*dr->desc) + dr->padding) * index));
}

static int
wctc4xxp_initialize_descriptor_ring(struct pci_dev *pdev,
	struct wctc4xxp_descriptor_ring *dr, u32 des1, unsigned int direction)
{
	int i;
	const u32 END_OF_RING = 0x02000000;
	u8 cache_line_size = 0;
	struct wctc4xxp_descriptor *d;
	int add_padding;

	BUG_ON(!pdev);
	BUG_ON(!dr);

	if (pci_read_config_byte(pdev, 0x0c, &cache_line_size))
		return -EIO;

	memset(dr, 0, sizeof(*dr));

	/*
	 * Add some padding to each descriptor to ensure that they are
	 * aligned on host system cache-line boundaries, but only for the
	 * cache-line sizes that we support.
	 *
	 */
	add_padding =   (0x08 == cache_line_size) ||
			(0x10 == cache_line_size) ||
			(0x20 == cache_line_size);
	if (add_padding)
		dr->padding = (cache_line_size*sizeof(u32)) - sizeof(*d);

	dr->desc = pci_alloc_consistent(pdev,
			(sizeof(*d)+dr->padding)*DRING_SIZE, &dr->desc_dma);

	if (!dr->desc)
		return -ENOMEM;

	memset(dr->desc, 0, (sizeof(*d) + dr->padding) * DRING_SIZE);
	for (i = 0; i < DRING_SIZE; ++i) {
		d = wctc4xxp_descriptor(dr, i);
		d->des1 = cpu_to_le32(des1);
	}

	d->des1 |= cpu_to_le32(END_OF_RING);
	dr->direction = direction;
	spin_lock_init(&dr->lock);
	dr->pdev = pdev;
	return 0;
}

#define OWN_BIT cpu_to_le32(0x80000000)
#define OWNED(_d_) (((_d_)->des0)&OWN_BIT)
#define SET_OWNED(_d_) do { wmb(); (_d_)->des0 |= OWN_BIT; wmb(); } while (0)

static const unsigned int BUFFER1_SIZE_MASK = 0x7ff;

static int
wctc4xxp_submit(struct wctc4xxp_descriptor_ring *dr, struct tcb *c)
{
	volatile struct wctc4xxp_descriptor *d;
	unsigned int len;
	unsigned long flags;

	WARN_ON(!c);
	len = (c->data_len < MIN_PACKET_LEN) ? MIN_PACKET_LEN : c->data_len;
	if (c->data_len > MAX_FRAME_SIZE) {
		WARN_ON_ONCE(!"Invalid command length passed\n");
		c->data_len = MAX_FRAME_SIZE;
	}

	spin_lock_irqsave(&dr->lock, flags);
	d = wctc4xxp_descriptor(dr, dr->tail);
	WARN_ON(!d);
	if (d->buffer1) {
		spin_unlock_irqrestore(&dr->lock, flags);
		/* Do not overwrite a buffer that is still in progress. */
		return -EBUSY;
	}
	d->des1 &= cpu_to_le32(~(BUFFER1_SIZE_MASK));
	d->des1 |= cpu_to_le32(len & BUFFER1_SIZE_MASK);
	d->buffer1 = cpu_to_le32(pci_map_single(dr->pdev, c->data,
			SFRAME_SIZE, dr->direction));

	SET_OWNED(d); /* That's it until the hardware is done with it. */
	dr->pending[dr->tail] = c;
	dr->tail = (dr->tail + 1) & DRING_MASK;
	++dr->count;
	spin_unlock_irqrestore(&dr->lock, flags);
	return 0;
}

static inline struct tcb*
wctc4xxp_retrieve(struct wctc4xxp_descriptor_ring *dr)
{
	volatile struct wctc4xxp_descriptor *d;
	struct tcb *c;
	unsigned int head = dr->head;
	unsigned long flags;
	spin_lock_irqsave(&dr->lock, flags);
	d = wctc4xxp_descriptor(dr, head);
	if (d->buffer1 && !OWNED(d)) {
		pci_unmap_single(dr->pdev, le32_to_cpu(d->buffer1),
			SFRAME_SIZE, dr->direction);
		c = dr->pending[head];
		WARN_ON(!c);
		dr->head = (++head) & DRING_MASK;
		d->buffer1 = 0;
		--dr->count;
		WARN_ON(!c);
		c->data_len = (le32_to_cpu(d->des0) >> 16) & BUFFER1_SIZE_MASK;
		WARN_ON(c->data_len > SFRAME_SIZE);
	} else {
		c = NULL;
	}
	spin_unlock_irqrestore(&dr->lock, flags);
	return c;
}

static inline int wctc4xxp_getcount(struct wctc4xxp_descriptor_ring *dr)
{
	int count;
	unsigned long flags;
	spin_lock_irqsave(&dr->lock, flags);
	count = dr->count;
	spin_unlock_irqrestore(&dr->lock, flags);
	return count;
}

static inline void
__wctc4xxp_setctl(struct wcdte *wc, unsigned int addr, unsigned int val)
{
	writel(val, wc->iobase + addr);
	readl(wc->iobase + addr);
}

static inline unsigned int
__wctc4xxp_getctl(struct wcdte *wc, unsigned int addr)
{
	return readl(wc->iobase + addr);
}

static inline void
wctc4xxp_setctl(struct wcdte *wc, unsigned int addr, unsigned int val)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__wctc4xxp_setctl(wc, addr, val);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void
wctc4xxp_receive_demand_poll(struct wcdte *wc)
{
	__wctc4xxp_setctl(wc, 0x0010, 0x00000000);
}

static inline void
wctc4xxp_transmit_demand_poll(struct wcdte *wc)
{
	return;
# if 0
	__wctc4xxp_setctl(wc, 0x0008, 0x00000000);

	/* \todo Investigate why this register needs to be written twice in
	 * order to get it to poll reliably.  So far, most of the problems
	 * I've seen with timeouts had more to do with an untransmitted
	 * packet sitting in the outbound descriptor list as opposed to any
	 * problem with the dte firmware.
	 */
	__wctc4xxp_setctl(wc, 0x0008, 0x00000000);
#endif
}

/* Returns the size, in bytes, of a CSM_ENCAPS packet, given the number of
 * parameters used. */
#define SIZE_WITH_N_PARAMETERS(__n) (sizeof(struct csm_encaps_hdr) + \
	((__n) * (sizeof(u16))))

/* There are 20 bytes in the ethernet header and the common CSM_ENCAPS header
 * that we don't want in the length of the actual CSM_ENCAPS command */
#define LENGTH_WITH_N_PARAMETERS(__n) (SIZE_WITH_N_PARAMETERS(__n) - 20)

static const u8 dst_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
static const u8 src_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static int wctc4xxp_transmit_cmd_and_wait(struct wcdte *wc, struct tcb *cmd);

static void
setup_common_header(struct wcdte *wc, struct csm_encaps_hdr *hdr)
{
	memcpy(hdr->ethhdr.h_dest, dst_mac, sizeof(dst_mac));
	memcpy(hdr->ethhdr.h_source, src_mac, sizeof(src_mac));
	hdr->ethhdr.h_proto = cpu_to_be16(ETH_P_CSM_ENCAPS);
}

static void
setup_supervisor_header(struct wcdte *wc, struct csm_encaps_hdr *hdr)
{
	setup_common_header(wc, hdr);

	hdr->op_code = cpu_to_be16(CONTROL_PACKET_OPCODE);
	hdr->control = LITTLE_ENDIAN;
	hdr->seq_num = (wc->seq_num++)&0xf;
	hdr->channel = cpu_to_be16(SUPERVISOR_CHANNEL);
}

static void
setup_channel_header(struct channel_pvt *pvt, struct csm_encaps_hdr *hdr)
{
	setup_common_header(pvt->wc, hdr);
	hdr->op_code = cpu_to_be16(CONTROL_PACKET_OPCODE);
	hdr->seq_num = (pvt->cmd_seqno++)&0xf;
	hdr->channel = cpu_to_be16(pvt->chan_in_num);
}

static void
create_supervisor_cmd(struct wcdte *wc, struct tcb *cmd, u8 type, u8 class,
	u16 function, const u16 *parameters, const int num_parameters)
{
	struct csm_encaps_hdr *hdr = cmd->data;
	int i;

	if (cmd->response) {
		free_cmd(cmd->response);
		cmd->response = NULL;
	}

	setup_supervisor_header(wc, hdr);

	hdr->length =		LENGTH_WITH_N_PARAMETERS(num_parameters);
	hdr->index =		0;
	hdr->type =		type;
	hdr->class =		class;
	hdr->function =		cpu_to_le16(function);
	hdr->reserved =		0;

	for (i = 0; i < num_parameters; ++i)
		hdr->params[i] = cpu_to_le16(parameters[i]);

	cmd->flags = WAIT_FOR_RESPONSE;
	cmd->data_len = SIZE_WITH_N_PARAMETERS(num_parameters);
}

static void
create_channel_cmd(struct channel_pvt *pvt, struct tcb *cmd, u8 type, u8 class,
	u16 function, const u16 *parameters, int num_parameters)
{
	int i;
	struct csm_encaps_hdr *hdr = cmd->data;

	if (cmd->response) {
		free_cmd(cmd->response);
		cmd->response = NULL;
	}

	setup_channel_header(pvt, hdr);

	hdr->length =	LENGTH_WITH_N_PARAMETERS(num_parameters);
	hdr->index =	0;
	hdr->type =	type;
	hdr->class =	class;
	hdr->function =	cpu_to_le16(function);
	hdr->reserved =	0;

	for (i = 0; i < num_parameters; ++i)
		hdr->params[i] = cpu_to_le16(parameters[i]);

	cmd->flags = WAIT_FOR_RESPONSE;
	cmd->data_len = SIZE_WITH_N_PARAMETERS(num_parameters);
}

static int
send_create_channel_cmd(struct wcdte *wc, struct tcb *cmd, u16 timeslot,
	u16 *channel_number)
{
	int res;
	const u16 parameters[] = {0x0002, timeslot};

	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, SUPVSR_CREATE_CHANNEL,
		parameters, ARRAY_SIZE(parameters));

	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;

	if (0x0000 != response_header(cmd)->params[0]) {
		if (printk_ratelimit()) {
			dev_warn(&wc->pdev->dev,
				 "Failed to create channel in timeslot " \
				 "%d.  Response from DTE was (%04x).\n",
				 timeslot, response_header(cmd)->params[0]);
		}
		free_cmd(cmd->response);
		cmd->response = NULL;
		return -EIO;
	}

	*channel_number = le16_to_cpu(response_header(cmd)->params[1]);
	free_cmd(cmd->response);
	cmd->response = NULL;
	return 0;
}

static int
send_set_arm_clk_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x012c, 0x0000};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0411, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_set_spu_clk_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x012c, 0x0000};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0412, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_tdm_select_bus_mode_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0004};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0417, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_set_eth_header_cmd(struct wcdte *wc, struct tcb *cmd,
	const u8 *host_mac, const u8 *assigned_mac)
{
	u16 parameters[8];
	u16 *part;

	parameters[0] = 0x0001;
	part = (u16 *)host_mac;
	parameters[1] = part[0];
	parameters[2] = part[1];
	parameters[3] = part[2];
	part = (u16 *)assigned_mac;
	parameters[4] = part[0];
	parameters[5] = part[1];
	parameters[6] = part[2];
	parameters[7] = 0x0008;

	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0100, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_supvsr_setup_tdm_parms(struct wcdte *wc, struct tcb *cmd,
	u8 bus_number)
{
	const u16 parameters[] = {0x8380, 0x0c00, 0, (bus_number << 2)&0xc};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0407, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_ip_service_config_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0200};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0302, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_arp_service_config_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0001};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0105, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_icmp_service_config_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0xff01};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0304, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_device_set_country_code_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0000};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x041b, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_spu_features_control_cmd(struct wcdte *wc, struct tcb *cmd, u16 options)
{
	const u16 parameters[] = {options};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0013, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_tdm_opt_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0000};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0435, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
send_destroy_channel_cmd(struct wcdte *wc, struct tcb *cmd, u16 channel)
{
	int res;
	u16 result;
	const u16 parameters[] = {channel};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0011, parameters,
		ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;
	/* Let's check the response for any error codes.... */
	result = le16_to_cpu(response_header(cmd)->params[0]);
	if (0x0000 != result) {
		dev_err(&wc->pdev->dev,
			"Failed to destroy channel %04d (%04x)\n",
			channel, result);
		return -EIO;
	}
	return 0;
}

static int
send_set_ip_hdr_channel_cmd(struct channel_pvt *pvt, struct tcb *cmd)
{
	int res;
	u16 result;
	struct wcdte *wc = pvt->wc;
	const u16 parameters[] = {0, 0x0045, 0, 0, 0x0040, 0x1180, 0,
		0xa8c0, 0x0309, 0xa8c0, 0x0309,
		swab16(pvt->timeslot_out_num + 0x5000),
		swab16(pvt->timeslot_in_num + 0x5000),
		0, 0};
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x9000, parameters, ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;
	/* Let's check the response for any error codes.... */
	result = le16_to_cpu(response_header(cmd)->params[0]);
	if (0x0000 != result) {
		dev_err(&wc->pdev->dev, "Failure in %s (%04x)\n",
			__func__, result);
		return -EIO;
	}
	return 0;
}

static int
send_voip_vceopt_cmd(struct channel_pvt *pvt, struct tcb *cmd, u16 length)
{
	int res;
	u16 result;
	const u16 parameters[] = {((length << 8)|0x21), 0x1c00, 0x0004, 0, 0};
	struct wcdte *wc = pvt->wc;
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x8001, parameters, ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;
	/* Let's check the response for any error codes.... */
	result = le16_to_cpu(response_header(cmd)->params[0]);
	if (0x0000 != result) {
		dev_err(&wc->pdev->dev, "Failure in %s (%04x)\n",
			__func__, result);
		return -EIO;
	}
	return 0;
}

static int
send_voip_tonectl_cmd(struct channel_pvt *pvt, struct tcb *cmd)
{
	int res;
	u16 result;
	const u16 parameters[] = {0};
	struct wcdte *wc = pvt->wc;
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x805b, parameters, ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;
	/* Let's check the response for any error codes.... */
	result = le16_to_cpu(response_header(cmd)->params[0]);
	if (0x0000 != result) {
		dev_err(&wc->pdev->dev, "Failure in %s (%04x)\n",
			__func__, result);
		return -EIO;
	}
	return 0;
}

static int
send_voip_dtmfopt_cmd(struct channel_pvt *pvt, struct tcb *cmd)
{
	const u16 parameters[] = {0x0008};
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x8002, parameters, ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(pvt->wc, cmd);
}

static int
send_voip_indctrl_cmd(struct channel_pvt *pvt, struct tcb *cmd)
{
	const u16 parameters[] = {0x0007};
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x8084, parameters, ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(pvt->wc, cmd);
}

static int
send_voip_vopena_cmd(struct channel_pvt *pvt, struct tcb *cmd, u8 format)
{
	const u16 parameters[] = {1, ((format<<8)|0x80), 0, 0, 0,
		0x3412, 0x7856};
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x8000, parameters, ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(pvt->wc, cmd);
}

static int
send_voip_vopena_close_cmd(struct channel_pvt *pvt, struct tcb *cmd)
{
	int res;
	const u16 parameters[] = {0};
	create_channel_cmd(pvt, cmd, CONFIG_CHANGE_TYPE, CONFIG_CHANNEL_CLASS,
		0x8000, parameters, ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(pvt->wc, cmd);
	if (res)
		return res;
	/* Let's check the response for any error codes.... */
	if (0x0000 != response_header(cmd)->params[0]) {
		WARN_ON(1);
		return -EIO;
	}
	return 0;
}

static int
send_ip_options_cmd(struct wcdte *wc, struct tcb *cmd)
{
	const u16 parameters[] = {0x0002};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x0306, parameters,
		ARRAY_SIZE(parameters));
	return wctc4xxp_transmit_cmd_and_wait(wc, cmd);
}

static int
_send_trans_connect_cmd(struct wcdte *wc, struct tcb *cmd, u16 enable, u16
	encoder_channel, u16 decoder_channel, u16 encoder_format,
	u16 decoder_format)
{
	int res;
	const u16 parameters[] = {enable, encoder_channel, encoder_format,
		decoder_channel, decoder_format};
	create_supervisor_cmd(wc, cmd, CONFIG_CHANGE_TYPE,
		CONFIG_DEVICE_CLASS, 0x9322, parameters,
		ARRAY_SIZE(parameters));
	res = wctc4xxp_transmit_cmd_and_wait(wc, cmd);
	if (res)
		return res;

	/* Let's check the response for any error codes.... */
	if (0x0000 != response_header(cmd)->params[0]) {
		WARN_ON(1);
		return -EIO;
	}
	return 0;
}

static int
send_trans_connect_cmd(struct wcdte *wc, struct tcb *cmd, const u16
	encoder_channel, const u16 decoder_channel, const u16 encoder_format,
	const u16 decoder_format)
{
	return _send_trans_connect_cmd(wc, cmd, 1, encoder_channel,
		decoder_channel, encoder_format, decoder_format);
}

static int
send_trans_disconnect_cmd(struct wcdte *wc, struct tcb *cmd, const u16
	encoder_channel, const u16 decoder_channel, const u16 encoder_format,
	const u16 decoder_format)
{
	return _send_trans_connect_cmd(wc, cmd, 0, encoder_channel,
		decoder_channel, encoder_format, decoder_format);
}

static struct tcb *
wctc4xxp_create_rtp_cmd(struct wcdte *wc, struct dahdi_transcoder_channel *dtc,
	size_t inbytes)
{
	const struct channel_pvt *cpvt = dtc->pvt;
	struct rtp_packet *packet;
	struct tcb *cmd;

	cmd = alloc_cmd(sizeof(*packet) + inbytes);
	if (!cmd)
		return NULL;

	packet = cmd->data;

	BUG_ON(cmd->data_len < sizeof(*packet));

	/* setup the ethernet header */
	memcpy(packet->ethhdr.h_dest, dst_mac, sizeof(dst_mac));
	memcpy(packet->ethhdr.h_source, src_mac, sizeof(src_mac));
	packet->ethhdr.h_proto = cpu_to_be16(ETH_P_IP);

	/* setup the IP header */
	packet->iphdr.ihl =		5;
	packet->iphdr.version =		4;
	packet->iphdr.tos =		0;
	packet->iphdr.tot_len =		cpu_to_be16(inbytes+40);
	packet->iphdr.id =		0;
	packet->iphdr.frag_off =	cpu_to_be16(0x4000);
	packet->iphdr.ttl =		64;
	packet->iphdr.protocol =	0x11; /* UDP */
	packet->iphdr.check =		0;
	packet->iphdr.saddr =		cpu_to_be32(0xc0a80903);
	packet->iphdr.daddr =		cpu_to_be32(0xc0a80903);

	packet->iphdr.check =	ip_fast_csum((void *)&packet->iphdr,
					packet->iphdr.ihl);

	/* setup the UDP header */
	packet->udphdr.source =	cpu_to_be16(cpvt->timeslot_out_num + 0x5000);
	packet->udphdr.dest =	cpu_to_be16(cpvt->timeslot_in_num + 0x5000);
	packet->udphdr.len  =	cpu_to_be16(inbytes + sizeof(struct rtphdr) +
					sizeof(struct udphdr));
	packet->udphdr.check =	0;

	/* Setup the RTP header */
	packet->rtphdr.ver =	    2;
	packet->rtphdr.padding =    0;
	packet->rtphdr.extension =  0;
	packet->rtphdr.csrc_count = 0;
	packet->rtphdr.marker =	    0;
	packet->rtphdr.type =	    wctc4xxp_dahdifmt_to_dtefmt(dtc->srcfmt);
	packet->rtphdr.seqno =	    cpu_to_be16(cpvt->seqno);
	packet->rtphdr.timestamp =  cpu_to_be32(cpvt->timestamp);
	packet->rtphdr.ssrc =	    cpu_to_be32(cpvt->ssrc);

	WARN_ON(cmd->data_len > SFRAME_SIZE);
	return cmd;
}
static void
wctc4xxp_cleanup_descriptor_ring(struct wctc4xxp_descriptor_ring *dr)
{
	int i;
	struct wctc4xxp_descriptor *d;

	if (!dr || !dr->desc)
		return;

	for (i = 0; i < DRING_SIZE; ++i) {
		d = wctc4xxp_descriptor(dr, i);
		if (d->buffer1) {
			pci_unmap_single(dr->pdev, d->buffer1,
				SFRAME_SIZE, dr->direction);
			d->buffer1 = 0;
			/* Commands will also be sitting on the waiting for
			 * response list, so we want to make sure to delete
			 * them from that list as well. */
			list_del_init(&(dr->pending[i])->node);
			free_cmd(dr->pending[i]);
			dr->pending[i] = NULL;
		}
	}
	dr->head = 0;
	dr->tail = 0;
	dr->count = 0;
	pci_free_consistent(dr->pdev, (sizeof(*d)+dr->padding) * DRING_SIZE,
		dr->desc, dr->desc_dma);
}

static void wctc4xxp_cleanup_command_list(struct wcdte *wc)
{
	struct tcb *cmd;
	unsigned long flags;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_splice_init(&wc->cmd_list, &local_list);
	list_splice_init(&wc->waiting_for_response_list, &local_list);
	list_splice_init(&wc->rx_list, &local_list);
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);

	while (!list_empty(&local_list)) {
		cmd = list_entry(local_list.next, struct tcb, node);
		list_del_init(&cmd->node);
		free_cmd(cmd);
	}
}

/**
 * The command list is used to store commands that couldn't fit in the tx
 * descriptor list when they were requested.
 */
static void
wctc4xxp_add_to_command_list(struct wcdte *wc, struct tcb *cmd)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_add_tail(&cmd->node, &wc->cmd_list);
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);
}

static void
wctc4xxp_add_to_response_list(struct wcdte *wc, struct tcb *cmd)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_add_tail(&cmd->node, &wc->waiting_for_response_list);
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);
}

static void
wctc4xxp_remove_from_response_list(struct wcdte *wc, struct tcb *cmd)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_del_init(&cmd->node);
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);
}

static void
wctc4xxp_transmit_cmd(struct wcdte *wc, struct tcb *cmd)
{
	int res;

	/* If we're shutdown all commands will timeout. Just complete the
	 * command here with the timeout flag */
	if (unlikely(test_bit(DTE_SHUTDOWN, &wc->flags))) {
		if (cmd->flags & DO_NOT_AUTO_FREE) {
			cmd->flags |= DTE_CMD_TIMEOUT;
			list_del_init(&cmd->node);
			complete(&cmd->complete);
		} else {
			list_del(&cmd->node);
			free_cmd(cmd);
		}
		return;
	}

	if (cmd->data_len < MIN_PACKET_LEN) {
		memset((u8 *)(cmd->data) + cmd->data_len, 0,
		       MIN_PACKET_LEN-cmd->data_len);
		cmd->data_len = MIN_PACKET_LEN;
	}
	WARN_ON(cmd->response);
	WARN_ON(cmd->flags & TX_COMPLETE);
	cmd->timeout = jiffies + HZ/4;
	if (cmd->flags & (__WAIT_FOR_ACK | __WAIT_FOR_RESPONSE)) {
		if (cmd->flags & __WAIT_FOR_RESPONSE) {
			/* We don't need both an ACK and a response.  Let's
			 * tell the DTE not to generate an ACK, and we'll just
			 * retry if we do not get the response within the
			 * timeout period. */
			struct csm_encaps_hdr *hdr = cmd->data;
			hdr->control |= SUPPRESS_ACK;
		}
		WARN_ON(!list_empty(&cmd->node));
		wctc4xxp_add_to_response_list(wc, cmd);
		mod_timer(&wc->watchdog, jiffies + HZ/2);
	}
	if (!(cmd->flags & DO_NOT_CAPTURE))
		wctc4xxp_net_capture_cmd(wc, cmd);
	res = wctc4xxp_submit(wc->txd, cmd);
	if (-EBUSY == res) {
		/* Looks like we're out of room in the descriptor
		 * ring.  We'll add this command to the pending list
		 * and the interrupt service routine will pull from
		 * this list as it clears up room in the descriptor
		 * ring. */
		wctc4xxp_remove_from_response_list(wc, cmd);
		wctc4xxp_add_to_command_list(wc, cmd);
	} else if (0 == res) {
		wctc4xxp_transmit_demand_poll(wc);
	} else {
		/* Unknown return value... */
		WARN_ON(1);
	}
}

static int
wctc4xxp_transmit_cmd_and_wait(struct wcdte *wc, struct tcb *cmd)
{
	cmd->flags |= DO_NOT_AUTO_FREE;
	wctc4xxp_transmit_cmd(wc, cmd);
	wait_for_completion(&cmd->complete);
	if (cmd->flags & DTE_CMD_TIMEOUT) {
		DTE_DEBUG(DTE_DEBUG_GENERAL, "Timeout waiting for command.\n");
		return -EIO;
	}
	return 0;
}

static int wctc4xxp_create_channel_pair(struct wcdte *wc,
		struct channel_pvt *cpvt, u8 simple, u8 complicated);
static int wctc4xxp_destroy_channel_pair(struct wcdte *wc,
		struct channel_pvt *cpvt);
static int __wctc4xxp_setup_channels(struct wcdte *wc);

static void
wctc4xxp_init_state(struct channel_pvt *cpvt, int encoder,
	unsigned int channel, struct wcdte *wc)
{
	memset(cpvt, 0, sizeof(*cpvt));
	cpvt->encoder = encoder;
	cpvt->wc = wc;
	cpvt->chan_in_num = INVALID;
	cpvt->chan_out_num = INVALID;
	cpvt->ssrc = 0x78;
	cpvt->timeslot_in_num = channel*2;
	cpvt->timeslot_out_num = channel*2;
	if (encoder)
		++cpvt->timeslot_out_num;
	else
		++cpvt->timeslot_in_num;
	spin_lock_init(&cpvt->lock);
	INIT_LIST_HEAD(&cpvt->rx_queue);
}

static unsigned int
wctc4xxp_getctl(struct wcdte *wc, unsigned int addr)
{
	unsigned int val;
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	val = __wctc4xxp_getctl(wc, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return val;
}

static void
wctc4xxp_cleanup_channel_private(struct wcdte *wc,
	struct dahdi_transcoder_channel *dtc)
{
	struct tcb *cmd, *temp;
	struct channel_pvt *cpvt = dtc->pvt;
	unsigned long flags;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&cpvt->lock, flags);
	list_splice_init(&cpvt->rx_queue, &local_list);
	dahdi_tc_clear_data_waiting(dtc);
	spin_unlock_irqrestore(&cpvt->lock, flags);

	memset(&cpvt->stats, 0, sizeof(cpvt->stats));
	list_for_each_entry_safe(cmd, temp, &local_list, node) {
		list_del(&cmd->node);
		free_cmd(cmd);
	}
}

static int
wctc4xxp_mark_channel_complement_built(struct wcdte *wc,
	struct dahdi_transcoder_channel *dtc)
{
	int index;
	struct channel_pvt *cpvt = dtc->pvt;
	struct dahdi_transcoder_channel *compl_dtc;
	struct channel_pvt *compl_cpvt;

	BUG_ON(!cpvt);
	index = cpvt->timeslot_in_num/2;
	BUG_ON(index >= wc->numchannels);
	if (cpvt->encoder)
		compl_dtc = &(wc->udecode->channels[index]);
	else
		compl_dtc = &(wc->uencode->channels[index]);

	/* It shouldn't already have been built... */
	WARN_ON(dahdi_tc_is_built(compl_dtc));
	compl_dtc->built_fmts = dtc->dstfmt | dtc->srcfmt;
	compl_cpvt = compl_dtc->pvt;
	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
		"dtc: %p is the complement to %p\n", compl_dtc, dtc);
	compl_cpvt->chan_in_num = cpvt->chan_out_num;
	compl_cpvt->chan_out_num = cpvt->chan_in_num;
	dahdi_tc_set_built(compl_dtc);
	wctc4xxp_cleanup_channel_private(wc, dtc);

	return 0;
}

static int
do_channel_allocate(struct dahdi_transcoder_channel *dtc)
{
	struct channel_pvt *cpvt = dtc->pvt;
	struct wcdte *wc = cpvt->wc;
	u8 wctc4xxp_srcfmt; /* Digium Transcoder Engine Source Format */
	u8 wctc4xxp_dstfmt; /* Digium Transcoder Engine Dest Format */
	int res;

#ifndef DEBUG_WCTC4XXP
	down(&wc->chansem);
#else
	if (down_interruptible(&wc->chansem))
		return -EINTR;
#endif

	/* Check again to see if the channel was built after grabbing the
	 * channel semaphore, in case the previous holder of the semaphore
	 * built this channel as a complement to itself. */
	if (dahdi_tc_is_built(dtc)) {
		up(&wc->chansem);
		DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
		  "Allocating channel %p which is already built.\n", dtc);
		return 0;
	}

	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
		"Entering %s for channel %p.\n", __func__, dtc);
	/* Anything on the rx queue now is old news... */
	wctc4xxp_cleanup_channel_private(wc, dtc);
	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
		"Allocating a new channel: %p.\n", dtc);
	wctc4xxp_srcfmt = wctc4xxp_dahdifmt_to_dtefmt(dtc->srcfmt);
	wctc4xxp_dstfmt = wctc4xxp_dahdifmt_to_dtefmt(dtc->dstfmt);
	res = wctc4xxp_create_channel_pair(wc, cpvt, wctc4xxp_srcfmt,
		wctc4xxp_dstfmt);
	if (res) {
		/* There was a problem creating the channel.... */
		up(&wc->chansem);
		return res;
	}
	/* Mark this channel as built */
	dahdi_tc_set_built(dtc);
	dtc->built_fmts = dtc->dstfmt | dtc->srcfmt;
	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
	  "Channel %p has dstfmt=%x and srcfmt=%x\n", dtc, dtc->dstfmt,
	  dtc->srcfmt);
	/* Mark the channel complement (other half of encoder/decoder pair) as
	 * built */
	res = wctc4xxp_mark_channel_complement_built(wc, dtc);
	up(&wc->chansem);
	dahdi_transcoder_alert(dtc);
	return res;
}

static void
wctc4xxp_setintmask(struct wcdte *wc, unsigned int intmask)
{
	wc->intmask = intmask;
	wctc4xxp_setctl(wc, 0x0038, intmask);
}

static void
wctc4xxp_enable_interrupts(struct wcdte *wc)
{
	wctc4xxp_setintmask(wc, 0x000180c0);
}

static void
wctc4xxp_disable_interrupts(struct wcdte *wc)
{
	/* Disable interrupts */
	wctc4xxp_setintmask(wc, 0x00000000);
	wctc4xxp_setctl(wc, 0x0084, 0x00000000);
}

static void
wctc4xxp_enable_polling(struct wcdte *wc)
{
	set_bit(DTE_POLLING, &wc->flags);
	mod_timer(&wc->polling, jiffies + 1);
	wctc4xxp_disable_interrupts(wc);
}

static int
wctc4xxp_operation_allocate(struct dahdi_transcoder_channel *dtc)
{
	struct wcdte *wc = ((struct channel_pvt *)(dtc->pvt))->wc;

	if (unlikely(test_bit(DTE_SHUTDOWN, &wc->flags))) {
		/* The shudown flags can also be set if there is a
		 * catastrophic failure. */
		return -EIO;
	}

	atomic_inc(&wc->open_channels);
	if (atomic_read(&wc->open_channels) > POLLING_CALL_THRESHOLD) {
		if (!test_bit(DTE_POLLING, &wc->flags))
			wctc4xxp_enable_polling(wc);
	}

	if (dahdi_tc_is_built(dtc)) {
		DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
		  "Allocating channel %p which is already built.\n", dtc);
		return 0;
	}
	return do_channel_allocate(dtc);
}

static void
wctc4xxp_disable_polling(struct wcdte *wc)
{
	clear_bit(DTE_POLLING, &wc->flags);
	wctc4xxp_enable_interrupts(wc);
}

static int
wctc4xxp_operation_release(struct dahdi_transcoder_channel *dtc)
{
	int res;
	int index;
	/* This is the 'complimentary channel' to dtc.  I.e., if dtc is an
	 * encoder, compl_dtc is the decoder and vice-versa */
	struct dahdi_transcoder_channel *compl_dtc;
	struct channel_pvt *compl_cpvt;
	struct channel_pvt *cpvt = dtc->pvt;
	struct wcdte *wc = cpvt->wc;
	int packets_received, packets_sent;

	BUG_ON(!cpvt);
	BUG_ON(!wc);

	if (unlikely(test_bit(DTE_SHUTDOWN, &wc->flags))) {
		/* The shudown flags can also be set if there is a
		 * catastrophic failure. */
		return -EIO;
	}

#ifndef DEBUG_WCTC4XXP
	down(&wc->chansem);
#else
	if (down_interruptible(&wc->chansem))
		return -EINTR;
#endif

	atomic_dec(&wc->open_channels);

#if !defined(CONFIG_WCTC4XXP_POLLING)
	if (atomic_read(&wc->open_channels) < POLLING_CALL_THRESHOLD) {
		if (test_bit(DTE_POLLING, &wc->flags))
			wctc4xxp_disable_polling(wc);
	}
#endif

	packets_received = atomic_read(&cpvt->stats.packets_received);
	packets_sent = atomic_read(&cpvt->stats.packets_sent);

	if ((packets_sent - packets_received) > 5) {
		DTE_DEBUG(DTE_DEBUG_GENERAL, "%s channel %d sent %d packets "
			"and received %d packets.\n", (cpvt->encoder) ?
			"encoder" : "decoder", cpvt->chan_out_num,
			packets_sent, packets_received);
	}

	/* Remove any packets that are waiting on the outbound queue. */
	wctc4xxp_cleanup_channel_private(wc, dtc);
	index = cpvt->timeslot_in_num/2;
	BUG_ON(index >= wc->numchannels);
	if (cpvt->encoder)
		compl_dtc = &(wc->udecode->channels[index]);
	else
		compl_dtc = &(wc->uencode->channels[index]);
	BUG_ON(!compl_dtc);
	if (!dahdi_tc_is_built(compl_dtc)) {
		DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
			"Releasing a channel that was never built.\n");
		res = 0;
		goto error_exit;
	}
	/* If the channel complement (other half of the encoder/decoder pair) is
	 * being used. */
	if (dahdi_tc_is_busy(compl_dtc)) {
		res = 0;
		goto error_exit;
	}
	res = wctc4xxp_destroy_channel_pair(wc, cpvt);
	if (res)
		goto error_exit;

	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP, "Releasing channel: %p\n", dtc);
	/* Mark this channel as not built */
	dahdi_tc_clear_built(dtc);
	dtc->built_fmts = 0;
	cpvt->chan_in_num = INVALID;
	cpvt->chan_out_num = INVALID;
	/* Mark the channel complement as not built */
	dahdi_tc_clear_built(compl_dtc);
	compl_dtc->built_fmts = 0;
	compl_cpvt = compl_dtc->pvt;
	compl_cpvt->chan_in_num = INVALID;
	compl_cpvt->chan_out_num = INVALID;
error_exit:
	up(&wc->chansem);
	return res;
}

static inline struct tcb*
get_ready_cmd(struct dahdi_transcoder_channel *dtc)
{
	struct channel_pvt *cpvt = dtc->pvt;
	struct tcb *cmd;
	unsigned long flags;
	spin_lock_irqsave(&cpvt->lock, flags);
	if (!list_empty(&cpvt->rx_queue)) {
		WARN_ON(!dahdi_tc_is_data_waiting(dtc));
		cmd = list_entry(cpvt->rx_queue.next, struct tcb, node);
		list_del_init(&cmd->node);
	} else {
		cmd = NULL;
	}
	if (list_empty(&cpvt->rx_queue))
		dahdi_tc_clear_data_waiting(dtc);
	spin_unlock_irqrestore(&cpvt->lock, flags);
	return cmd;
}

static int
wctc4xxp_handle_receive_ring(struct wcdte *wc)
{
	struct tcb *cmd;
	unsigned long flags;
	unsigned int count = 0;

	/* If we can't grab this lock, another thread must already be checking
	 * the receive ring...so we should just finish up, and we'll try again
	 * later. */
#if defined(spin_trylock_irqsave)
	if (!spin_trylock_irqsave(&wc->rx_lock, flags))
		return 0;
#else
	if (spin_is_locked(&wc->rx_lock))
		return 0;
	spin_lock_irqsave(&wc->rx_lock, flags);
#endif

	while ((cmd = wctc4xxp_retrieve(wc->rxd))) {
		++count;
		spin_lock(&wc->rx_list_lock);
		list_add_tail(&cmd->node, &wc->rx_list);
		spin_unlock(&wc->rx_list_lock);
		cmd = __alloc_cmd(SFRAME_SIZE, GFP_ATOMIC, 0);
		if (!cmd) {
			dev_err(&wc->pdev->dev,
				"Out of memory in %s.\n", __func__);
		} else {
			if (wctc4xxp_submit(wc->rxd, cmd)) {
				dev_err(&wc->pdev->dev, "Failed submit in %s\n",
					__func__);
				free_cmd(cmd);
			}
		}
	}
	spin_unlock_irqrestore(&wc->rx_lock, flags);
	return count;
}

static void
__wctc4xxp_polling(struct wcdte *wc)
{
	if (wctc4xxp_handle_receive_ring(wc))
		schedule_work(&wc->deferred_work);
}

static void
wctc4xxp_polling(unsigned long data)
{
	struct wcdte *wc = (struct wcdte *)data;
	__wctc4xxp_polling(wc);
	if (test_bit(DTE_POLLING, &wc->flags))
		mod_timer(&wc->polling, jiffies + 1);
}


/* Called with a buffer in which to copy a transcoded frame. */
static ssize_t
wctc4xxp_read(struct file *file, char __user *frame, size_t count, loff_t *ppos)
{
	ssize_t ret;
	struct dahdi_transcoder_channel *dtc = file->private_data;
	struct channel_pvt *cpvt = dtc->pvt;
	struct wcdte *wc = cpvt->wc;
	struct tcb *cmd;
	struct rtp_packet *packet;
	ssize_t payload_bytes;
	ssize_t returned_bytes = 0;
	unsigned long flags;

	BUG_ON(!dtc);
	BUG_ON(!cpvt);

	if (unlikely(test_bit(DTE_SHUTDOWN, &wc->flags))) {
		/* The shudown flags can also be set if there is a
		 * catastrophic failure. */
		return -EIO;
	}

	cmd = get_ready_cmd(dtc);
	if (!cmd) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(dtc->ready,
				dahdi_tc_is_data_waiting(dtc));
		if (-ERESTARTSYS == ret)
			return -EINTR;
		/* List went not empty. */
		cmd = get_ready_cmd(dtc);
	}

	do {
		BUG_ON(!cmd);
		packet = cmd->data;

		payload_bytes = be16_to_cpu(packet->udphdr.len) -
					sizeof(struct rtphdr) -
					sizeof(struct udphdr);

		if (count < (payload_bytes + returned_bytes)) {
			if (returned_bytes) {
				/* If we have already returned at least one
				 * packets worth of data, we'll add this next
				 * packet to the head of the receive queue so
				 * it will be picked up next time. */
				spin_lock_irqsave(&cpvt->lock, flags);
				list_add(&cmd->node, &cpvt->rx_queue);
				dahdi_tc_set_data_waiting(dtc);
				spin_unlock_irqrestore(&cpvt->lock, flags);
				return returned_bytes;
			}

			if (printk_ratelimit()) {
				dev_err(&wc->pdev->dev,
				  "Cannot copy %zd bytes into %zd byte user " \
				  "buffer.\n", payload_bytes, count);
			}
			free_cmd(cmd);
			return -EFBIG;
		}

		atomic_inc(&cpvt->stats.packets_received);

		ret = copy_to_user(&frame[returned_bytes],
				   &packet->payload[0], payload_bytes);
		if (unlikely(ret)) {
			dev_err(&wc->pdev->dev, "Failed to copy data in %s\n",
				   __func__);
			free_cmd(cmd);
			return -EFAULT;
		}

		returned_bytes += payload_bytes;

		free_cmd(cmd);

	} while ((cmd = get_ready_cmd(dtc)));

	return returned_bytes;
}

/* Called with a frame in the srcfmt to be transcoded into the dstfmt. */
static ssize_t
wctc4xxp_write(struct file *file, const char __user *frame,
	size_t count, loff_t *ppos)
{
	struct dahdi_transcoder_channel *dtc = file->private_data;
	struct channel_pvt *cpvt = dtc->pvt;
	struct wcdte *wc = cpvt->wc;
	struct tcb *cmd;

	BUG_ON(!cpvt);
	BUG_ON(!wc);

	if (unlikely(test_bit(DTE_SHUTDOWN, &wc->flags)))
		return -EIO;

	if (!test_bit(DAHDI_TC_FLAG_CHAN_BUILT, &dtc->flags))
		return -EAGAIN;

	if (count < 2) {
		DTE_DEBUG(DTE_DEBUG_GENERAL,
		   "Cannot request to transcode a packet that is less than " \
		   "2 bytes.\n");
		return -EINVAL;
	}

	if (unlikely(count > SFRAME_SIZE - sizeof(struct rtp_packet))) {
		DTE_DEBUG(DTE_DEBUG_GENERAL,
		   "Cannot transcode packet of %Zu bytes. This exceeds the " \
		   "maximum size of %Zu bytes.\n", count,
		   SFRAME_SIZE - sizeof(struct rtp_packet));
		return -EINVAL;
	}

	if (DAHDI_FORMAT_G723_1 == dtc->srcfmt) {
		if ((G723_5K_BYTES != count) && (G723_6K_BYTES != count) &&
		    (G723_SID_BYTES != count)) {
			DTE_DEBUG(DTE_DEBUG_GENERAL,
			   "Trying to transcode packet into G723 format " \
			   "that is %Zu bytes instead of the expected " \
			   "%d/%d/%d bytes.\n", count, G723_5K_BYTES,
			   G723_6K_BYTES, G723_SID_BYTES);
			return -EINVAL;
		}
		cpvt->timestamp += G723_SAMPLES;
	} else if (DAHDI_FORMAT_G723_1 == dtc->dstfmt) {
		cpvt->timestamp = G723_SAMPLES;
	} else {
		/* Same for ulaw and alaw */
		cpvt->timestamp += G729_SAMPLES;
	}

	cmd = wctc4xxp_create_rtp_cmd(wc, dtc, count);
	if (!cmd)
		return -ENOMEM;
	/* Copy the data directly from user space into the command buffer. */
	if (copy_from_user(&((struct rtp_packet *)(cmd->data))->payload[0],
		frame, count)) {
		dev_err(&wc->pdev->dev,
			"Failed to copy packet from userspace.\n");
		free_cmd(cmd);
		return -EFAULT;
	}
	cpvt->seqno += 1;

	DTE_DEBUG(DTE_DEBUG_RTP_TX,
	    "Sending packet of %Zu byte on channel (%p).\n", count, dtc);

	atomic_inc(&cpvt->stats.packets_sent);
	wctc4xxp_transmit_cmd(wc, cmd);

	if (test_bit(DTE_POLLING, &wc->flags)) {
#if HZ == 100
		__wctc4xxp_polling(wc);
#else
		if (jiffies != wc->jiffies_at_last_poll) {
			wc->jiffies_at_last_poll = jiffies;
			__wctc4xxp_polling(wc);
		}
#endif
	}


	return count;
}

static void
wctc4xxp_send_ack(struct wcdte *wc, u8 seqno, __be16 channel)
{
	struct tcb *cmd;
	struct csm_encaps_hdr *hdr;
	cmd = __alloc_cmd(sizeof(*hdr), ALLOC_FLAGS, 0);
	if (!cmd) {
		WARN_ON(1);
		return;
	}
	hdr = cmd->data;
	BUG_ON(sizeof(*hdr) > cmd->data_len);
	setup_common_header(wc, hdr);
	hdr->op_code = cpu_to_be16(0x0001);
	hdr->seq_num = seqno;
	hdr->control = 0xe0;
	hdr->channel = channel;

	wctc4xxp_transmit_cmd(wc, cmd);
}

static bool do_rx_response_packet(struct wcdte *wc, struct tcb *cmd)
{
	const struct csm_encaps_hdr *listhdr, *rxhdr;
	struct tcb *pos, *temp;
	unsigned long flags;
	bool handled = false;
	rxhdr = cmd->data;
	if (SUPERVISOR_CHANNEL == rxhdr->channel) {
		/* We received a duplicate response. */
		if (rxhdr->seq_num == wc->last_rx_seq_num) {
			free_cmd(cmd);
			return false;
		}
		wc->last_rx_seq_num = rxhdr->seq_num;
	}

	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_for_each_entry_safe(pos, temp,
		&wc->waiting_for_response_list, node) {
		listhdr = pos->data;
		if ((listhdr->function == rxhdr->function) &&
		    (listhdr->channel == rxhdr->channel)) {

			spin_lock(&pos->lock);
			list_del_init(&pos->node);
			pos->flags &= ~(__WAIT_FOR_RESPONSE);
			pos->response = cmd;
			/* If this isn't TX_COMPLETE yet, then this packet will
			 * be completed in service_tx_ring. */
			if (pos->flags & TX_COMPLETE)
				complete(&pos->complete);
			spin_unlock(&pos->lock);
			handled = true;

			break;
		}
	}
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);

	if (!handled) {
		DTE_DEBUG(DTE_DEBUG_GENERAL,
			"Freeing unhandled response ch:(%04x)\n",
			be16_to_cpu(rxhdr->channel));
		free_cmd(cmd);
		return false;
	}

	return true;
}

static void
do_rx_ack_packet(struct wcdte *wc, struct tcb *cmd)
{
	const struct csm_encaps_hdr *listhdr, *rxhdr;
	struct tcb *pos, *temp;
	unsigned long flags;

	rxhdr = cmd->data;

	spin_lock_irqsave(&wc->cmd_list_lock, flags);
	list_for_each_entry_safe(pos, temp,
		&wc->waiting_for_response_list, node) {
		listhdr = pos->data;
		if (cpu_to_be16(0xefed) == listhdr->ethhdr.h_proto) {
			wc->seq_num = (rxhdr->seq_num + 1) & 0xff;
			WARN_ON(!(pos->flags & DO_NOT_AUTO_FREE));
			WARN_ON(!(pos->flags & TX_COMPLETE));
			list_del_init(&pos->node);
			WARN_ON(!(pos->flags & TX_COMPLETE));
			complete(&pos->complete);
		} else if ((listhdr->seq_num == rxhdr->seq_num) &&
			   (listhdr->channel == rxhdr->channel)) {
			spin_lock(&pos->lock);
			if (pos->flags & __WAIT_FOR_RESPONSE) {
				pos->flags &= ~(__WAIT_FOR_ACK);
				spin_unlock(&pos->lock);
			} else {
				list_del_init(&pos->node);

				if (pos->flags & DO_NOT_AUTO_FREE) {
					WARN_ON(!(pos->flags & TX_COMPLETE));
					complete(&pos->complete);
					spin_unlock(&pos->lock);
				} else {
					spin_unlock(&pos->lock);
					free_cmd(pos);
				}
			}
			break;
		}
	}
	spin_unlock_irqrestore(&wc->cmd_list_lock, flags);

	/* There is never a reason to store up the ack packets. */
	free_cmd(cmd);
}

static inline int
is_response(const struct csm_encaps_hdr *hdr)
{
	return ((0x02 == hdr->type) || (0x04 == hdr->type)) ? 1 : 0;
}

static void
print_command(struct wcdte *wc, const struct tcb *cmd)
{
	int i, curlength;
	const struct csm_encaps_hdr *hdr = cmd->data;
	char *buffer;
	const int BUFFER_SIZE = 1024;
	int parameters = ((hdr->length - 8)/sizeof(__le16));

	buffer = kzalloc(BUFFER_SIZE + 1, GFP_ATOMIC);
	if (!buffer) {
		dev_info(&wc->pdev->dev, "Failed print_command\n");
		return;
	}
	curlength = snprintf(buffer, BUFFER_SIZE,
		"opcode: %04x seq: %02x control: %02x "
		"channel: %04x ", be16_to_cpu(hdr->op_code),
		hdr->seq_num, hdr->control, be16_to_cpu(hdr->channel));
	curlength += snprintf(buffer + curlength, BUFFER_SIZE - curlength,
		"length: %02x index: %02x type: %02x "
		"class: %02x function: %04x",
		hdr->length, hdr->index, hdr->type, hdr->class,
		le16_to_cpu(hdr->function));
	for (i = 0; i < parameters; ++i) {
		curlength += snprintf(buffer + curlength,
			BUFFER_SIZE - curlength, " %04x",
			le16_to_cpu(hdr->params[i]));
	}
	dev_info(&wc->pdev->dev, "%s\n", buffer);
	kfree(buffer);
}

static void
receive_csm_encaps_packet(struct wcdte *wc, struct tcb *cmd)
{
	const struct csm_encaps_hdr *hdr = cmd->data;

	if (!(hdr->control & MESSAGE_PACKET)) {
		const bool suppress_ack = ((hdr->control & SUPPRESS_ACK) > 0);
		if (is_response(hdr)) {
			u8 seq_num = hdr->seq_num;
			__be16 channel = hdr->channel;

			if (do_rx_response_packet(wc, cmd) && !suppress_ack)
				wctc4xxp_send_ack(wc, seq_num, channel);

		} else if (0xc1 == hdr->type) {
			if (!suppress_ack) {
				wctc4xxp_send_ack(wc, hdr->seq_num,
						  hdr->channel);
			}

			if (0x75 == hdr->class) {
				dev_warn(&wc->pdev->dev,
				   "Received alert (0x%04x) from dsp\n",
				   le16_to_cpu(hdr->params[0]));
			}
			free_cmd(cmd);
		} else if (0xd4 == hdr->type) {
			if (!suppress_ack) {
				wctc4xxp_send_ack(wc, hdr->seq_num,
						  hdr->channel);
			}
			if (hdr->params[0] != le16_to_cpu(0xffff)) {
				dev_warn(&wc->pdev->dev,
				   "DTE Failed self test (%04x).\n",
				   le16_to_cpu(hdr->params[0]));
			} else if ((hdr->params[1] != le16_to_cpu(0x000c)) &&
				(hdr->params[1] != le16_to_cpu(0x010c))) {
				dev_warn(&wc->pdev->dev,
				   "Unexpected ERAM status (%04x).\n",
				   le16_to_cpu(hdr->params[1]));
			} else {
				wctc4xxp_set_ready(wc);
				wake_up(&wc->waitq);
			}
			free_cmd(cmd);
		} else if (MONITOR_LIVE_INDICATION_TYPE == hdr->type) {
			if (!suppress_ack) {
				wctc4xxp_send_ack(wc, hdr->seq_num,
						  hdr->channel);
			}
			dev_warn(&wc->pdev->dev,
				 "Received diagnostic message:\n");
			print_command(wc, cmd);
			free_cmd(cmd);
		} else {
			if (!suppress_ack) {
				wctc4xxp_send_ack(wc, hdr->seq_num,
						  hdr->channel);
			}
			dev_warn(&wc->pdev->dev,
			  "Unknown command type received. %02x\n", hdr->type);
			free_cmd(cmd);
		}
	} else {
		do_rx_ack_packet(wc, cmd);
	}
}

static void
queue_rtp_packet(struct wcdte *wc, struct tcb *cmd)
{
	unsigned index;
	struct dahdi_transcoder_channel *dtc;
	struct channel_pvt *cpvt;
	struct rtp_packet *packet = cmd->data;
	unsigned long flags;

	if (unlikely(ip_fast_csum((void *)(&packet->iphdr),
		packet->iphdr.ihl))) {
		DTE_DEBUG(DTE_DEBUG_GENERAL,
			"Invalid checksum in RTP packet %04x\n",
			ip_fast_csum((void *)(&packet->iphdr),
			packet->iphdr.ihl));
		free_cmd(cmd);
		return;
	}

	index = (be16_to_cpu(packet->udphdr.dest) - 0x5000) / 2;
	if (unlikely(!(index < wc->numchannels))) {
		dev_err(&wc->pdev->dev,
		  "Invalid channel number in response from DTE.\n");
		free_cmd(cmd);
		return;
	}

	switch (packet->rtphdr.type) {
	case 0x00:
	case 0x08:
		dtc = &(wc->udecode->channels[index]);
		break;
	case 0x04:
	case 0x12:
		dtc = &(wc->uencode->channels[index]);
		break;
	default:
		dev_err(&wc->pdev->dev, "Unknown codec in packet (0x%02x).\n",\
			packet->rtphdr.type);
		free_cmd(cmd);
		return;
	}

	cpvt = dtc->pvt;
	spin_lock_irqsave(&cpvt->lock, flags);
	list_add_tail(&cmd->node, &cpvt->rx_queue);
	dahdi_tc_set_data_waiting(dtc);
	spin_unlock_irqrestore(&cpvt->lock, flags);
	dahdi_transcoder_alert(dtc);
	return;
}

static inline void
wctc4xxp_receiveprep(struct wcdte *wc, struct tcb *cmd)
{
	const struct ethhdr *ethhdr = (const struct ethhdr *)(cmd->data);

	if (cpu_to_be16(ETH_P_IP) == ethhdr->h_proto) {
		queue_rtp_packet(wc, cmd);
	} else if (cpu_to_be16(ETH_P_CSM_ENCAPS) == ethhdr->h_proto) {
		receive_csm_encaps_packet(wc, cmd);
	} else {
		DTE_DEBUG(DTE_DEBUG_GENERAL,
		   "Unknown packet protocol received: %04x.\n",
		   be16_to_cpu(ethhdr->h_proto));
		free_cmd(cmd);
	}
}

static void service_tx_ring(struct wcdte *wc)
{
	struct tcb *cmd;
	unsigned long flags;
	while ((cmd = wctc4xxp_retrieve(wc->txd))) {
		spin_lock_irqsave(&cmd->lock, flags);
		cmd->flags |= TX_COMPLETE;
		if (!(cmd->flags & (__WAIT_FOR_ACK | __WAIT_FOR_RESPONSE))) {
			/* If we're not waiting for an ACK or Response from
			 * the DTE, this message should not be sitting on any
			 * lists. */
			WARN_ON(!list_empty(&cmd->node));
			if (DO_NOT_AUTO_FREE & cmd->flags) {
				spin_unlock_irqrestore(&cmd->lock, flags);
				WARN_ON(!(cmd->flags & TX_COMPLETE));
				complete(&cmd->complete);
			} else {
				spin_unlock_irqrestore(&cmd->lock, flags);
				free_cmd(cmd);
			}
		} else {
			spin_unlock_irqrestore(&cmd->lock, flags);
		}

		/* We've freed up a spot in the hardware ring buffer.  If
		 * another packet is queued up, let's submit it to the
		 * hardware. */
		spin_lock_irqsave(&wc->cmd_list_lock, flags);
		if (!list_empty(&wc->cmd_list)) {
			cmd = list_entry(wc->cmd_list.next, struct tcb, node);
			list_del_init(&cmd->node);
		} else {
			cmd = NULL;
		}
		spin_unlock_irqrestore(&wc->cmd_list_lock, flags);

		if (cmd)
			wctc4xxp_transmit_cmd(wc, cmd);
	}
}

static void service_rx_ring(struct wcdte *wc)
{
	struct tcb *cmd;
	unsigned long flags;
	LIST_HEAD(local_list);
	spin_lock_irqsave(&wc->rx_list_lock, flags);
	list_splice_init(&wc->rx_list, &local_list);
	spin_unlock_irqrestore(&wc->rx_list_lock, flags);

	/*
	 * Process the received packets
	 */
	while (!list_empty(&local_list)) {
		cmd = container_of(local_list.next, struct tcb, node);
		list_del_init(&cmd->node);

		wctc4xxp_net_capture_cmd(wc, cmd);
		wctc4xxp_receiveprep(wc, cmd);
	}
	wctc4xxp_receive_demand_poll(wc);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void deferred_work_func(void *param)
{
	struct wcdte *wc = param;
#else
static void deferred_work_func(struct work_struct *work)
{
	struct wcdte *wc = container_of(work, struct wcdte, deferred_work);
#endif
	service_tx_ring(wc);
	service_rx_ring(wc);
}

DAHDI_IRQ_HANDLER(wctc4xxp_interrupt)
{
	struct wcdte *wc = dev_id;
	u32 ints;
	u32 reg;
#define TX_COMPLETE_INTERRUPT 0x00000001
#define RX_COMPLETE_INTERRUPT 0x00000040
#define NORMAL_INTERRUPTS (TX_COMPLETE_INTERRUPT | RX_COMPLETE_INTERRUPT)

	/* Read and clear interrupts */
	ints = __wctc4xxp_getctl(wc, 0x0028);

	ints &= wc->intmask;

	if (!ints)
		return IRQ_NONE;

	if (likely(ints & NORMAL_INTERRUPTS)) {
		reg = 0;
		if (ints & TX_COMPLETE_INTERRUPT)
			reg |= TX_COMPLETE_INTERRUPT;

		if (ints & RX_COMPLETE_INTERRUPT) {
			wctc4xxp_handle_receive_ring(wc);
			reg |= RX_COMPLETE_INTERRUPT;
		}

#if DEFERRED_PROCESSING == WORKQUEUE
		schedule_work(&wc->deferred_work);
#elif DEFERRED_PROCESSING == INTERRUPT
#error "You will need to change the locks if you want to run the processing " \
		"in the interrupt handler."
#else
#error "Define a deferred processing function in kernel/wctc4xxp/wctc4xxp.h"
#endif

		__wctc4xxp_setctl(wc, 0x0028, reg);
	} else {
		if ((ints & 0x00008000) && debug)
			dev_info(&wc->pdev->dev, "Abnormal Interrupt.\n");

		if ((ints & 0x00002000) && debug)
			dev_info(&wc->pdev->dev, "Fatal Bus Error INT\n");

		if ((ints & 0x00000100) && debug)
			dev_info(&wc->pdev->dev, "Receive Stopped INT\n");

		if ((ints & 0x00000080) && debug) {
			dev_info(&wc->pdev->dev,
				 "Receive Desciptor Unavailable INT " \
				 "(%d)\n", wctc4xxp_getcount(wc->rxd));
		}

		if ((ints & 0x00000020) && debug)
			dev_info(&wc->pdev->dev, "Transmit Under-flow INT\n");

		if ((ints & 0x00000008) && debug)
			dev_info(&wc->pdev->dev, "Jabber Timer Time-out INT\n");

		if ((ints & 0x00000002) && debug) {
			dev_info(&wc->pdev->dev,
				 "Transmit Processor Stopped INT\n");
		}

		/* Clear all the pending interrupts. */
		__wctc4xxp_setctl(wc, 0x0028, ints);
	}
	return IRQ_HANDLED;
}

static int
wctc4xxp_hardware_init(struct wcdte *wc)
{
	/* Hardware stuff */
	u32 reg;
	unsigned long newjiffies;
	u8 cache_line_size;
	const u32 DEFAULT_PCI_ACCESS = 0xfff80000;

	/* Enable I/O Access */
	pci_read_config_dword(wc->pdev, 0x0004, &reg);
	reg |= 0x00000007;
	pci_write_config_dword(wc->pdev, 0x0004, reg);

	if (pci_read_config_byte(wc->pdev, 0x0c, &cache_line_size))
		return -EIO;

	switch (cache_line_size) {
	case 0x08:
		reg = DEFAULT_PCI_ACCESS | (0x1 << 14);
		break;
	case 0x10:
		reg = DEFAULT_PCI_ACCESS | (0x2 << 14);
		break;
	case 0x20:
		reg = DEFAULT_PCI_ACCESS | (0x3 << 14);
		break;
	default:
		reg = 0xfe584202;
		break;
	}

	reg |= ((wc->txd->padding / sizeof(u32)) << 2) & 0x7c;

	/* Reset the DTE... */
	wctc4xxp_setctl(wc, 0x0000, reg | 1);
	newjiffies = jiffies + HZ; /* One second timeout */
	/* ...and wait for it to come out of reset. */
	while (((wctc4xxp_getctl(wc, 0x0000)) & 0x00000001) &&
		(newjiffies > jiffies))
		msleep(1);

	wctc4xxp_setctl(wc, 0x0000, reg | 0x60000);

	/* Configure watchdogs, access, etc */
	wctc4xxp_setctl(wc, 0x0030, 0x00280040);
	wctc4xxp_setctl(wc, 0x0078, 0x00000013);
	reg = wctc4xxp_getctl(wc, 0x00fc);
	wctc4xxp_setctl(wc, 0x00fc, (reg & ~0x7) | 0x7);
	reg = wctc4xxp_getctl(wc, 0x00fc);
	return 0;
}

static void
wctc4xxp_start_dma(struct wcdte *wc)
{
	int res;
	int i;
	u32 reg;
	struct tcb *cmd;

	for (i = 0; i < DRING_SIZE; ++i) {
		cmd = alloc_cmd(SFRAME_SIZE);
		if (!cmd) {
			WARN_ALWAYS();
			return;
		}
		WARN_ON(SFRAME_SIZE != cmd->data_len);
		res = wctc4xxp_submit(wc->rxd, cmd);
		if (res) {
			/* When we're starting the DMA, we should always be
			 * able to fill the ring....so something is wrong
			 * here. */
			WARN_ALWAYS();
			free_cmd(cmd);
			break;
		}
	}
	wmb();
	wctc4xxp_setctl(wc, 0x0020, wc->txd->desc_dma);
	wctc4xxp_setctl(wc, 0x0018, wc->rxd->desc_dma);

	/* Start receiver/transmitter */
	reg = wctc4xxp_getctl(wc, 0x0030);
	wctc4xxp_setctl(wc, 0x0030, reg | 0x00002002);
	wctc4xxp_receive_demand_poll(wc);
	reg = wctc4xxp_getctl(wc, 0x0028);
	wctc4xxp_setctl(wc, 0x0028, reg);

}

static void
_wctc4xxp_stop_dma(struct wcdte *wc)
{
	/* Disable interrupts and reset */
	unsigned int reg;
	/* Disable interrupts */
	wctc4xxp_setintmask(wc, 0x00000000);
	wctc4xxp_setctl(wc, 0x0084, 0x00000000);
	wctc4xxp_setctl(wc, 0x0048, 0x00000000);
	/* Reset the part to be on the safe side */
	reg = wctc4xxp_getctl(wc, 0x0000);
	reg |= 0x00000001;
	wctc4xxp_setctl(wc, 0x0000, reg);
}

static void
wctc4xxp_stop_dma(struct wcdte *wc)
{
	unsigned long newjiffies;

	_wctc4xxp_stop_dma(wc);
	newjiffies = jiffies + HZ; /* One second timeout */
	/* We'll wait here for the part to come out of reset */
	while (((wctc4xxp_getctl(wc, 0x0000)) & 0x00000001) &&
		(newjiffies > jiffies))
			msleep(1);
}

#define MDIO_SHIFT_CLK		0x10000
#define MDIO_DATA_WRITE1 	0x20000
#define MDIO_ENB		0x00000
#define MDIO_ENB_IN		0x40000
#define MDIO_DATA_READ		0x80000

static int
wctc4xxp_read_phy(struct wcdte *wc, int location)
{
	int i;
	long mdio_addr = 0x0048;
	int read_cmd = (0xf6 << 10) | (1 << 5) | location;
	int retval = 0;

	/* Establish sync by sending at least 32 logic ones. */
	for (i = 32; i >= 0; --i) {
		wctc4xxp_setctl(wc, mdio_addr,
			MDIO_ENB | MDIO_DATA_WRITE1);
		wctc4xxp_getctl(wc, mdio_addr);
		wctc4xxp_setctl(wc, mdio_addr,
			MDIO_ENB | MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}

	/* Shift the read command bits out. */
	for (i = 17; i >= 0; --i) {
		int dataval = (read_cmd & (1 << i)) ? MDIO_DATA_WRITE1 : 0;

		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB | dataval);
		wctc4xxp_getctl(wc, mdio_addr);
		wctc4xxp_setctl(wc, mdio_addr,
			MDIO_ENB | dataval | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; --i) {
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB_IN);
		wctc4xxp_getctl(wc, mdio_addr);
		retval = (retval << 1) |
			((wctc4xxp_getctl(wc, mdio_addr) & MDIO_DATA_READ) ?
			1 : 0);
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB_IN | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}
	retval = (retval>>1) & 0xffff;
	return retval;
}

static void
wctc4xxp_write_phy(struct wcdte *wc, int location, int value)
{
	int i;
	int cmd = (0x5002 << 16) | (1 << 23) | (location<<18) | value;
	long mdio_addr = 0x0048;

	/* Establish sync by sending 32 logic ones. */
	for (i = 32; i >= 0; --i) {
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB | MDIO_DATA_WRITE1);
		wctc4xxp_getctl(wc, mdio_addr);
		wctc4xxp_setctl(wc, mdio_addr,
			MDIO_ENB | MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}
	/* Shift the command bits out. */
	for (i = 31; i >= 0; --i) {
		int dataval = (cmd & (1 << i)) ? MDIO_DATA_WRITE1 : 0;
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB | dataval);
		wctc4xxp_getctl(wc, mdio_addr);
		wctc4xxp_setctl(wc, mdio_addr,
			MDIO_ENB | dataval | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; --i) {
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB_IN);
		wctc4xxp_getctl(wc, mdio_addr);
		wctc4xxp_setctl(wc, mdio_addr, MDIO_ENB_IN | MDIO_SHIFT_CLK);
		wctc4xxp_getctl(wc, mdio_addr);
	}
	return;
}

static int
wctc4xxp_wait_for_link(struct wcdte *wc)
{
	int reg;
	unsigned int delay_count = 0;
	do {
		reg = wctc4xxp_getctl(wc, 0x00fc);
		msleep(2);
		delay_count++;

		if (delay_count >= 5000) {
			dev_err(&wc->pdev->dev,
				"Failed to link to DTE processor!\n");
			return -EIO;
		}
	} while ((reg & 0xE0000000) != 0xE0000000);
	return 0;
}

static int
wctc4xxp_load_firmware(struct wcdte *wc, const struct firmware *firmware)
{
	unsigned int byteloc;
	unsigned int length;
	struct tcb *cmd;

	byteloc = 17;

	cmd = alloc_cmd(SFRAME_SIZE);
	if (!cmd)
		return -ENOMEM;

#if defined(CONFIG_WCTC4XXP_POLLING)
	wctc4xxp_enable_polling(wc);
#endif

	while (byteloc < (firmware->size-20)) {
		length = (firmware->data[byteloc] << 8) |
				firmware->data[byteloc+1];
		byteloc += 2;
		cmd->data_len = length;
		BUG_ON(length > cmd->data_len);
		memcpy(cmd->data, &firmware->data[byteloc], length);
		byteloc += length;
		cmd->flags = WAIT_FOR_ACK;
		wctc4xxp_transmit_cmd(wc, cmd);
		wait_for_completion(&cmd->complete);
		if (cmd->flags & DTE_CMD_TIMEOUT) {
			free_cmd(cmd);
			dev_err(&wc->pdev->dev, "Failed to load firmware.\n");
#if defined(CONFIG_WCTC4XXP_POLLING)
			wctc4xxp_disable_polling(wc);
#endif
			return -EIO;
		}
	}
	free_cmd(cmd);
	if (!wait_event_timeout(wc->waitq, wctc4xxp_is_ready(wc), 15*HZ)) {
		dev_err(&wc->pdev->dev, "Failed to boot firmware.\n");
#if defined(CONFIG_WCTC4XXP_POLLING)
		wctc4xxp_disable_polling(wc);
#endif
		return -EIO;
	}

#if defined(CONFIG_WCTC4XXP_POLLING)
	wctc4xxp_disable_polling(wc);
#endif
	return 0;
}

static int
wctc4xxp_turn_off_booted_led(struct wcdte *wc)
{
	int ret = 0;
	int reg;
	/* Turn off auto negotiation */
	wctc4xxp_write_phy(wc, 0, 0x2100);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "PHY register 0 = %X\n",
	   wctc4xxp_read_phy(wc, 0));

	/* Set reset */
	wctc4xxp_setctl(wc, 0x00A0, 0x04000000);

	/* Wait 4 ms to ensure processor reset */
	msleep(4);

	/* Clear reset */
	wctc4xxp_setctl(wc, 0x00A0, 0x04080000);

	/* Wait for the ethernet link */
	ret = wctc4xxp_wait_for_link(wc);
	if (ret)
		return ret;

	/* Turn off booted LED */
	wctc4xxp_setctl(wc, 0x00A0, 0x04084000);
	reg = wctc4xxp_getctl(wc, 0x00fc);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "LINK STATUS: reg(0xfc) = %X\n", reg);

	reg = wctc4xxp_getctl(wc, 0x00A0);

	return ret;
}

static void
wctc4xxp_turn_on_booted_led(struct wcdte *wc)
{
	wctc4xxp_setctl(wc, 0x00A0, 0x04080000);
}

static int
wctc4xxp_boot_processor(struct wcdte *wc, const struct firmware *firmware)
{
	int ret;

	wctc4xxp_turn_off_booted_led(wc);

	ret = wctc4xxp_load_firmware(wc, firmware);
	if (ret)
		return ret;

	wctc4xxp_turn_on_booted_led(wc);

	DTE_DEBUG(DTE_DEBUG_GENERAL, "Successfully booted DTE processor.\n");
	return 0;
}

static int
setup_half_channel(struct channel_pvt *pvt, struct tcb *cmd, u16 length)
{
	if (send_set_ip_hdr_channel_cmd(pvt, cmd))
		return -EIO;
	if (send_voip_vceopt_cmd(pvt, cmd, length))
		return -EIO;
	if (send_voip_tonectl_cmd(pvt, cmd))
		return -EIO;
	if (send_voip_dtmfopt_cmd(pvt, cmd))
		return -EIO;
	if (send_voip_indctrl_cmd(pvt, cmd))
		return -EIO;
	return 0;
}

static int
wctc4xxp_create_channel_pair(struct wcdte *wc, struct channel_pvt *cpvt,
	u8 simple, u8 complicated)
{
	struct channel_pvt *encoder_pvt, *decoder_pvt;
	u16 encoder_timeslot, decoder_timeslot;
	u16 encoder_channel, decoder_channel;
	u16 length;
	struct tcb *cmd;

	cmd = alloc_cmd(SFRAME_SIZE);
	if (!cmd)
		return -ENOMEM;

	BUG_ON(!wc || !cpvt);
	if (cpvt->encoder) {
		encoder_timeslot = cpvt->timeslot_in_num;
		decoder_timeslot = cpvt->timeslot_out_num;
	} else {
		u8 temp;
		encoder_timeslot = cpvt->timeslot_out_num;
		decoder_timeslot = cpvt->timeslot_in_num;
		temp = simple;
		simple = complicated;
		complicated = temp;
	}

	BUG_ON(encoder_timeslot/2 >= wc->numchannels);
	BUG_ON(decoder_timeslot/2 >= wc->numchannels);
	encoder_pvt = wc->uencode->channels[encoder_timeslot/2].pvt;
	decoder_pvt = wc->udecode->channels[decoder_timeslot/2].pvt;
	BUG_ON(!encoder_pvt);
	BUG_ON(!decoder_pvt);

	WARN_ON(encoder_timeslot == decoder_timeslot);
	/* First, let's create two channels, one for the simple -> complex
	 * encoder and another for the complex->simple decoder. */
	if (send_create_channel_cmd(wc, cmd, encoder_timeslot,
		&encoder_channel))
		goto error_exit;

	if (send_create_channel_cmd(wc, cmd, decoder_timeslot,
		&decoder_channel))
		goto error_exit;

	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
	   "DTE is using the following channels encoder_channel: " \
	   "%d decoder_channel: %d\n", encoder_channel, decoder_channel);

	length = (DTE_FORMAT_G729A == complicated) ? G729_LENGTH :
		(DTE_FORMAT_G723_1 == complicated) ? G723_LENGTH : 0;

	WARN_ON(encoder_channel == decoder_channel);
	/* Now set all the default parameters for the encoder. */
	encoder_pvt->chan_in_num = encoder_channel;
	encoder_pvt->chan_out_num = decoder_channel;
	if (setup_half_channel(encoder_pvt, cmd, length))
		goto error_exit;

	/* And likewise for the decoder. */
	decoder_pvt->chan_in_num = decoder_channel;
	decoder_pvt->chan_out_num = encoder_channel;
	if (setup_half_channel(decoder_pvt, cmd, length))
		goto error_exit;

	if (send_trans_connect_cmd(wc, cmd, encoder_channel,
		decoder_channel, complicated, simple))
		goto error_exit;
	if (send_voip_vopena_cmd(encoder_pvt, cmd, complicated))
		goto error_exit;
	if (send_voip_vopena_cmd(decoder_pvt, cmd, simple))
		goto error_exit;

	DTE_DEBUG(DTE_DEBUG_CHANNEL_SETUP,
	  "DTE has completed setup and connected the " \
	  "two channels together.\n");

	free_cmd(cmd);
	return 0;
error_exit:
	free_cmd(cmd);
	return -EIO;
}

static int
wctc4xxp_destroy_channel_pair(struct wcdte *wc, struct channel_pvt *cpvt)
{
	struct dahdi_transcoder_channel *dtc1, *dtc2;
	struct channel_pvt *encoder_pvt, *decoder_pvt;
	int chan1, chan2, timeslot1, timeslot2;
	struct tcb *cmd;

	cmd = alloc_cmd(SFRAME_SIZE);
	if (!cmd)
		return -ENOMEM;

	if (cpvt->encoder) {
		chan1 = cpvt->chan_in_num;
		timeslot1 = cpvt->timeslot_in_num;
		chan2 = cpvt->chan_out_num;
		timeslot2 = cpvt->timeslot_out_num;
	} else {
		chan1 = cpvt->chan_out_num;
		timeslot1 = cpvt->timeslot_out_num;
		chan2 = cpvt->chan_in_num;
		timeslot2 = cpvt->timeslot_in_num;
	}

	if (timeslot1/2 >= wc->numchannels || timeslot2/2 >= wc->numchannels) {
		dev_warn(&wc->pdev->dev,
		 "Invalid channel numbers in %s. chan1:%d chan2: %d\n",
		 __func__, timeslot1/2, timeslot2/2);
		return 0;
	}

	dtc1 = &(wc->uencode->channels[timeslot1/2]);
	dtc2 = &(wc->udecode->channels[timeslot2/2]);
	encoder_pvt = dtc1->pvt;
	decoder_pvt = dtc2->pvt;

	if (send_voip_vopena_close_cmd(encoder_pvt, cmd))
		goto error_exit;
	if (send_voip_vopena_close_cmd(decoder_pvt, cmd))
		goto error_exit;
	if (send_trans_disconnect_cmd(wc, cmd, chan1, chan2, 0, 0))
		goto error_exit;
	if (send_destroy_channel_cmd(wc, cmd, chan1))
		goto error_exit;
	if (send_destroy_channel_cmd(wc, cmd, chan2))
		goto error_exit;

	free_cmd(cmd);
	return 0;
error_exit:
	free_cmd(cmd);
	return -1;
}


static int
__wctc4xxp_setup_channels(struct wcdte *wc)
{
	struct tcb *cmd;
	int tdm_bus;

	cmd = alloc_cmd(SFRAME_SIZE);
	if (!cmd)
		return -ENOMEM;

	if (send_set_arm_clk_cmd(wc, cmd))
		goto error_exit;

	if (send_set_spu_clk_cmd(wc, cmd))
		goto error_exit;

	if (send_tdm_select_bus_mode_cmd(wc, cmd))
		goto error_exit;

	for (tdm_bus = 0; tdm_bus < 4; ++tdm_bus) {
		if (send_supvsr_setup_tdm_parms(wc, cmd, tdm_bus))
			goto error_exit;
	}

	if (send_set_eth_header_cmd(wc, cmd, src_mac, dst_mac))
		goto error_exit;

	if (send_ip_service_config_cmd(wc, cmd))
		goto error_exit;

	if (send_arp_service_config_cmd(wc, cmd))
		goto error_exit;

	if (send_icmp_service_config_cmd(wc, cmd))
		goto error_exit;

	if (send_device_set_country_code_cmd(wc, cmd))
		goto error_exit;

	if (send_spu_features_control_cmd(wc, cmd, 0x02))
		goto error_exit;

	if (send_ip_options_cmd(wc, cmd))
		goto error_exit;

	if (send_spu_features_control_cmd(wc, cmd, 0x04))
		goto error_exit;

	if (send_tdm_opt_cmd(wc, cmd))
		goto error_exit;

	free_cmd(cmd);
	return 0;
error_exit:
	free_cmd(cmd);
	return -1;
}

static int
wctc4xxp_setup_channels(struct wcdte *wc)
{
	int ret;
#ifndef DEBUG_WCTC4XXP
	down(&wc->chansem);
#else
	if (down_interruptible(&wc->chansem))
		return -EINTR;
#endif
	ret = __wctc4xxp_setup_channels(wc);
	up(&wc->chansem);

	return ret;
}

static void wctc4xxp_setup_file_operations(struct file_operations *fops)
{
	fops->owner = THIS_MODULE;
	fops->read =  wctc4xxp_read;
	fops->write = wctc4xxp_write;
}

static int
initialize_channel_pvt(struct wcdte *wc, int encoder,
	struct channel_pvt **cpvt)
{
	int chan;
	*cpvt = kmalloc(sizeof(struct channel_pvt) * wc->numchannels,
			GFP_KERNEL);
	if (!(*cpvt))
		return -ENOMEM;
	for (chan = 0; chan < wc->numchannels; ++chan)
		wctc4xxp_init_state((*cpvt) + chan, encoder, chan, wc);
	return 0;
}

static int
initialize_transcoder(struct wcdte *wc, unsigned int srcfmts,
	unsigned int dstfmts, struct channel_pvt *pvts,
	struct dahdi_transcoder **zt)
{
	int chan;
	*zt = dahdi_transcoder_alloc(wc->numchannels);
	if (!(*zt))
		return -ENOMEM;
	(*zt)->srcfmts = srcfmts;
	(*zt)->dstfmts = dstfmts;
	(*zt)->allocate = wctc4xxp_operation_allocate;
	(*zt)->release = wctc4xxp_operation_release;
	wctc4xxp_setup_file_operations(&((*zt)->fops));
	for (chan = 0; chan < wc->numchannels; ++chan)
		(*zt)->channels[chan].pvt = &pvts[chan];
	return 0;
}

static int initialize_encoders(struct wcdte *wc, unsigned int complexfmts)
{
	int res;
	res = initialize_channel_pvt(wc, 1, &wc->encoders);
	if (res)
		return res;

	res = initialize_transcoder(wc, DAHDI_FORMAT_ULAW | DAHDI_FORMAT_ALAW,
		complexfmts, wc->encoders, &wc->uencode);
	if (res)
		return res;
	sprintf(wc->uencode->name, "DTE Encoder");
	return res;
}

static int
initialize_decoders(struct wcdte *wc, unsigned int complexfmts)
{
	int res;
	res = initialize_channel_pvt(wc, 0, &wc->decoders);
	if (res)
		return res;

	res = initialize_transcoder(wc, complexfmts,
		DAHDI_FORMAT_ULAW | DAHDI_FORMAT_ALAW,
		wc->decoders, &wc->udecode);
	if (res)
		return res;

	sprintf(wc->udecode->name, "DTE Decoder");
	return res;
}

static void
wctc4xxp_send_commands(struct wcdte *wc, struct list_head *to_send)
{
	struct tcb *cmd;
	while (!list_empty(to_send)) {
		cmd = container_of(to_send->next, struct tcb, node);
		list_del_init(&cmd->node);
		wctc4xxp_transmit_cmd(wc, cmd);
	}
}

static void
wctc4xxp_watchdog(unsigned long data)
{
	struct wcdte *wc = (struct wcdte *)data;
	struct tcb *cmd, *temp;
	LIST_HEAD(cmds_to_retry);
	const int MAX_RETRIES = 5;
	int reschedule_timer = 0;

	service_tx_ring(wc);

	spin_lock(&wc->cmd_list_lock);
	/* Go through the list of messages that are waiting for responses from
	 * the DTE, and complete or retry any that have timed out. */
	list_for_each_entry_safe(cmd, temp,
		&wc->waiting_for_response_list, node) {
		if (time_after(jiffies, cmd->timeout)) {
			if (++cmd->retries > MAX_RETRIES) {
				if (!(cmd->flags & TX_COMPLETE)) {

					cmd->flags |= DTE_CMD_TIMEOUT;
					list_del_init(&cmd->node);
					complete(&cmd->complete);

					set_bit(DTE_SHUTDOWN, &wc->flags);
					spin_unlock(&wc->cmd_list_lock);
					_wctc4xxp_stop_dma(wc);
					dev_err(&wc->pdev->dev,
					  "Board malfunctioning.  " \
					  "Halting operation.\n");
					reschedule_timer = 0;
					spin_lock(&wc->cmd_list_lock);
					break;
				}
				/* ERROR:  We've retried the command and
				 * haven't received the ACK or the response.
				 */
				cmd->flags |= DTE_CMD_TIMEOUT;
				list_del_init(&cmd->node);
				complete(&cmd->complete);
			} else if (cmd->flags & TX_COMPLETE) {
				/* Move this to the local list because we're
				 * going to resend it once we free the locks
				 */
				list_move_tail(&cmd->node, &cmds_to_retry);
				cmd->flags &= ~(TX_COMPLETE);
			} else {
				/* The command is still sitting on the tx
				 * descriptor ring.  We don't want to move it
				 * off any lists, lets just reset the timeout
				 * and tell the hardware to look for another
				 * command . */
				dev_warn(&wc->pdev->dev,
				  "Retrying command that was " \
				  "still on descriptor list.\n");
				cmd->timeout = jiffies + HZ/4;
				wctc4xxp_transmit_demand_poll(wc);
				reschedule_timer = 1;
			}
		}
	}
	spin_unlock(&wc->cmd_list_lock);

	if (list_empty(&cmds_to_retry) && reschedule_timer)
		mod_timer(&wc->watchdog, jiffies + HZ/2);
	else if (!list_empty(&cmds_to_retry))
		wctc4xxp_send_commands(wc, &cmds_to_retry);
}

/**
 * Insert an struct wcdte on the global list in sorted order
 *
 */
static int __devinit
wctc4xxp_add_to_device_list(struct wcdte *wc)
{
	struct wcdte *cur;
	int pos = 0;
	INIT_LIST_HEAD(&wc->node);
	spin_lock(&wctc4xxp_list_lock);
	list_for_each_entry(cur, &wctc4xxp_list, node) {
		if (cur->pos != pos) {
			/* Add the new entry before the one here */
			list_add_tail(&wc->node, &cur->node);
			break;
		} else {
			++pos;
		}
	}
	/* If we didn't already add the new entry to the list, add it now */
	if (list_empty(&wc->node))
		list_add_tail(&wc->node, &wctc4xxp_list);
	spin_unlock(&wctc4xxp_list_lock);
	return pos;
}

static void wctc4xxp_remove_from_device_list(struct wcdte *wc)
{
	spin_lock(&wctc4xxp_list_lock);
	list_del(&wc->node);
	spin_unlock(&wctc4xxp_list_lock);
}

struct wctc4xxp_desc {
	const char *short_name;
	const char *long_name;
};

static struct wctc4xxp_desc wctc400p = {
	.short_name = "tc400b",
	.long_name = "Wildcard TC400P+TC400M",
};

static struct wctc4xxp_desc wctce400 = {
	.short_name = "tce400",
	.long_name = "Wildcard TCE400+TC400M",
};

static int __devinit
wctc4xxp_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int res, reg, position_on_list;
	struct wcdte *wc = NULL;
	struct wctc4xxp_desc *d = (struct wctc4xxp_desc *)ent->driver_data;
	unsigned char g729_numchannels, g723_numchannels, min_numchannels;
	unsigned char wctc4xxp_firmware_ver, wctc4xxp_firmware_ver_minor;
	unsigned int complexfmts;
	struct firmware embedded_firmware;
	const struct firmware *firmware = &embedded_firmware;
#if !defined(HOTPLUG_FIRMWARE)
	extern void _binary_dahdi_fw_tc400m_bin_size;
	extern u8 _binary_dahdi_fw_tc400m_bin_start[];
#else
	static const char tc400m_firmware[] = "dahdi-fw-tc400m.bin";
#endif

	/* ------------------------------------------------------------------
	 * Setup the pure software constructs internal to this driver.
	 * --------------------------------------------------------------- */

	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	position_on_list = wctc4xxp_add_to_device_list(wc);
	snprintf(wc->board_name, sizeof(wc->board_name)-1, "%s%d",
		d->short_name, position_on_list);
	wc->iobase           = pci_iomap(pdev, 1, 0);
	wc->pdev             = pdev;
	wc->pos              = position_on_list;
	wc->variety          = d->long_name;
	wc->last_rx_seq_num  = -1;

	if (!request_mem_region(pci_resource_start(pdev, 1),
	    pci_resource_len(pdev, 1), wc->board_name)) {
		dev_err(&pdev->dev, "IO Registers are in use by another "
			"module.\n");
		wctc4xxp_remove_from_device_list(wc);
		kfree(wc);
		return -EIO;
	}

	sema_init(&wc->chansem, 1);
	spin_lock_init(&wc->reglock);
	spin_lock_init(&wc->cmd_list_lock);
	spin_lock_init(&wc->rx_list_lock);
	spin_lock_init(&wc->rx_lock);
	INIT_LIST_HEAD(&wc->cmd_list);
	INIT_LIST_HEAD(&wc->waiting_for_response_list);
	INIT_LIST_HEAD(&wc->rx_list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&wc->deferred_work, deferred_work_func, wc);
#else
	INIT_WORK(&wc->deferred_work, deferred_work_func);
#endif
	init_waitqueue_head(&wc->waitq);

	if (pci_set_dma_mask(wc->pdev, DMA_BIT_MASK(32))) {
		release_mem_region(pci_resource_start(wc->pdev, 1),
			pci_resource_len(wc->pdev, 1));
		if (wc->iobase)
			pci_iounmap(wc->pdev, wc->iobase);
		dev_warn(&wc->pdev->dev, "No suitable DMA available.\n");
		return -EIO;
	}

	wc->txd = kmalloc(sizeof(*wc->txd), GFP_KERNEL);
	if (!wc->txd) {
		res = -ENOMEM;
		goto error_exit_swinit;
	}

	res = wctc4xxp_initialize_descriptor_ring(wc->pdev, wc->txd,
		0xe0800000, DMA_TO_DEVICE);
	if (res)
		goto error_exit_swinit;

	wc->rxd = kmalloc(sizeof(*wc->rxd), GFP_KERNEL);
	if (!wc->rxd) {
		res = -ENOMEM;
		goto error_exit_swinit;
	}

	res = wctc4xxp_initialize_descriptor_ring(wc->pdev, wc->rxd, 0,
		DMA_FROM_DEVICE);
	if (res)
		goto error_exit_swinit;

#if defined(HOTPLUG_FIRMWARE)
	res = request_firmware(&firmware, tc400m_firmware, &wc->pdev->dev);
	if (res || !firmware) {
		dev_err(&wc->pdev->dev,
		  "Firmware %s not available from userspace. (%d)\n",
		  tc400m_firmware, res);
		goto error_exit_swinit;
	}
#else
	embedded_firmware.data = _binary_dahdi_fw_tc400m_bin_start;
	embedded_firmware.size = (size_t) &_binary_dahdi_fw_tc400m_bin_size;
#endif

	wctc4xxp_firmware_ver = firmware->data[0];
	wctc4xxp_firmware_ver_minor = firmware->data[16];
	g729_numchannels = firmware->data[1];
	g723_numchannels = firmware->data[2];

	min_numchannels = min(g723_numchannels, g729_numchannels);

	if (!mode || strlen(mode) < 4) {
		sprintf(wc->complexname, "G.729a / G.723.1");
		complexfmts = DAHDI_FORMAT_G729A | DAHDI_FORMAT_G723_1;
		wc->numchannels = min_numchannels;
	} else if (mode[3] == '9') {	/* "G.729" */
		sprintf(wc->complexname, "G.729a");
		complexfmts = DAHDI_FORMAT_G729A;
		wc->numchannels = g729_numchannels;
	} else if (mode[3] == '3') {	/* "G.723.1" */
		sprintf(wc->complexname, "G.723.1");
		complexfmts = DAHDI_FORMAT_G723_1;
		wc->numchannels = g723_numchannels;
	} else {
		sprintf(wc->complexname, "G.729a / G.723.1");
		complexfmts = DAHDI_FORMAT_G729A | DAHDI_FORMAT_G723_1;
		wc->numchannels = min_numchannels;
	}

	res = initialize_encoders(wc, complexfmts);
	if (res)
		goto error_exit_swinit;
	res = initialize_decoders(wc, complexfmts);
	if (res)
		goto error_exit_swinit;

	if (DTE_DEBUG_NETWORK_IF & debug) {
		res = wctc4xxp_net_register(wc);
		if (res)
			goto error_exit_swinit;
	}

#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
	wc->watchdog.function = wctc4xxp_watchdog;
	wc->watchdog.data = (unsigned long)wc;
	init_timer(&wc->watchdog);
#	else
	setup_timer(&wc->watchdog, wctc4xxp_watchdog, (unsigned long)wc);
#	endif

#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
	wc->polling.function = wctc4xxp_polling;
	wc->polling.data = (unsigned long)wc;
	init_timer(&wc->polling);
#	else
	setup_timer(&wc->polling, wctc4xxp_polling, (unsigned long)wc);
#	endif

	/* ------------------------------------------------------------------
	 * Load the firmware and start the DTE.
	 * --------------------------------------------------------------- */

	res = pci_enable_device(pdev);
	if (res) {
		dev_err(&wc->pdev->dev, "Failed to enable device.\n");
		goto error_exit_swinit;;
	}
	pci_set_master(pdev);
	pci_set_drvdata(pdev, wc);
	res = request_irq(pdev->irq, wctc4xxp_interrupt,
		DAHDI_IRQ_SHARED, wc->board_name, wc);
	if (res) {
		dev_err(&wc->pdev->dev,
			"Unable to request IRQ %d\n", pdev->irq);
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		goto error_exit_hwinit;
	}
	res = wctc4xxp_hardware_init(wc);
	if (res) {
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		goto error_exit_hwinit;
	}
	wctc4xxp_enable_interrupts(wc);
	wctc4xxp_start_dma(wc);
	res = wctc4xxp_boot_processor(wc, firmware);
	if (firmware != &embedded_firmware)
		release_firmware(firmware);
	if (res)
		goto error_exit_hwinit;

#if defined(CONFIG_WCTC4XXP_POLLING)
	wctc4xxp_enable_polling(wc);
#endif
	res = wctc4xxp_setup_channels(wc);
	if (res)
		goto error_exit_hwinit;

	/* \todo Read firmware version directly from tc400b.*/
	dev_info(&wc->pdev->dev, "(%s) Transcoder support LOADED " \
	   "(firm ver = %d.%d)\n", wc->complexname, wctc4xxp_firmware_ver,
	   wctc4xxp_firmware_ver_minor);

	reg = wctc4xxp_getctl(wc, 0x00fc);

	DTE_DEBUG(DTE_DEBUG_GENERAL,
	   "debug: (post-boot) Reg fc is %08x\n", reg);

	dev_info(&wc->pdev->dev, "Installed a Wildcard TC: %s\n", wc->variety);
	DTE_DEBUG(DTE_DEBUG_GENERAL, "Operating in DEBUG mode.\n");
	dahdi_transcoder_register(wc->uencode);
	dahdi_transcoder_register(wc->udecode);

	return 0;

error_exit_hwinit:
#if defined(CONFIG_WCTC4XXP_POLLING)
	wctc4xxp_disable_polling(wc);
#endif
	wctc4xxp_stop_dma(wc);
	wctc4xxp_cleanup_command_list(wc);
	free_irq(pdev->irq, wc);
	pci_set_drvdata(pdev, NULL);
error_exit_swinit:
	wctc4xxp_net_unregister(wc);
	kfree(wc->encoders);
	kfree(wc->decoders);
	dahdi_transcoder_free(wc->uencode);
	dahdi_transcoder_free(wc->udecode);
	wctc4xxp_cleanup_descriptor_ring(wc->txd);
	kfree(wc->txd);
	wctc4xxp_cleanup_descriptor_ring(wc->rxd);
	kfree(wc->rxd);
	release_mem_region(pci_resource_start(wc->pdev, 1),
		pci_resource_len(wc->pdev, 1));
	if (wc->iobase)
		pci_iounmap(wc->pdev, wc->iobase);
	wctc4xxp_remove_from_device_list(wc);
	kfree(wc);
	return res;
}

static void wctc4xxp_cleanup_channels(struct wcdte *wc)
{
	int i;
	struct dahdi_transcoder_channel *dtc_en, *dtc_de;

	for (i = 0; i < wc->numchannels; ++i) {
		dtc_en = &(wc->uencode->channels[i]);
		wctc4xxp_cleanup_channel_private(wc, dtc_en);

		dtc_de = &(wc->udecode->channels[i]);
		wctc4xxp_cleanup_channel_private(wc, dtc_de);
	}
}

static void __devexit wctc4xxp_remove_one(struct pci_dev *pdev)
{
	struct wcdte *wc = pci_get_drvdata(pdev);

	if (!wc)
		return;

	wctc4xxp_remove_from_device_list(wc);

	set_bit(DTE_SHUTDOWN, &wc->flags);
	if (del_timer_sync(&wc->watchdog))
		del_timer_sync(&wc->watchdog);

	/* This should already be stopped, but it doesn't hurt to make sure. */
	clear_bit(DTE_POLLING, &wc->flags);
	if (del_timer_sync(&wc->polling))
		del_timer_sync(&wc->polling);

	wctc4xxp_net_unregister(wc);

	/* Stop any DMA */
	wctc4xxp_stop_dma(wc);

	/* In case hardware is still there */
	wctc4xxp_disable_interrupts(wc);

	free_irq(pdev->irq, wc);

	/* There isn't anything that would run in the workqueue that will wait
	 * on an interrupt. */

	dahdi_transcoder_unregister(wc->udecode);
	dahdi_transcoder_unregister(wc->uencode);

	/* Free Resources */
	release_mem_region(pci_resource_start(wc->pdev, 1),
		pci_resource_len(wc->pdev, 1));
	if (wc->iobase)
		pci_iounmap(wc->pdev, wc->iobase);
	wctc4xxp_cleanup_descriptor_ring(wc->txd);
	kfree(wc->txd);
	wctc4xxp_cleanup_descriptor_ring(wc->rxd);
	kfree(wc->rxd);

	wctc4xxp_cleanup_command_list(wc);
	wctc4xxp_cleanup_channels(wc);

	pci_set_drvdata(pdev, NULL);

	dahdi_transcoder_free(wc->uencode);
	dahdi_transcoder_free(wc->udecode);
	kfree(wc->encoders);
	kfree(wc->decoders);
	kfree(wc);
}

static DEFINE_PCI_DEVICE_TABLE(wctc4xxp_pci_tbl) = {
	{ 0xd161, 0x3400, PCI_ANY_ID, PCI_ANY_ID,
		0, 0, (unsigned long) &wctc400p }, /* Digium board */
	{ 0xd161, 0x8004, PCI_ANY_ID, PCI_ANY_ID,
		0, 0, (unsigned long) &wctce400 }, /* Digium board */
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, wctc4xxp_pci_tbl);

static int wctc4xxp_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return -ENOSYS;
}

static struct pci_driver wctc4xxp_driver = {
	.name = "wctc4xxp",
	.probe = wctc4xxp_init_one,
	.remove = __devexit_p(wctc4xxp_remove_one),
	.id_table = wctc4xxp_pci_tbl,
	.suspend = wctc4xxp_suspend,
};

static int __init wctc4xxp_init(void)
{
	int res;
	unsigned long cache_flags;

#if defined(CONFIG_SLUB) && (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 22))
	cache_flags = SLAB_HWCACHE_ALIGN | SLAB_STORE_USER | SLAB_DEBUG_FREE;
#else
	cache_flags = SLAB_HWCACHE_ALIGN;
#endif

#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	cmd_cache = kmem_cache_create(THIS_MODULE->name, sizeof(struct tcb),
			0, cache_flags, NULL, NULL);
#	else
	cmd_cache = kmem_cache_create(THIS_MODULE->name, sizeof(struct tcb),
			0, cache_flags, NULL);
#	endif

	if (!cmd_cache)
		return -ENOMEM;
	spin_lock_init(&wctc4xxp_list_lock);
	INIT_LIST_HEAD(&wctc4xxp_list);
	res = dahdi_pci_module(&wctc4xxp_driver);
	if (res) {
		kmem_cache_destroy(cmd_cache);
		return -ENODEV;
	}
	return 0;
}

static void __exit wctc4xxp_cleanup(void)
{
	pci_unregister_driver(&wctc4xxp_driver);
	kmem_cache_destroy(cmd_cache);
}

module_param(debug, int, S_IRUGO | S_IWUSR);
module_param(mode, charp, S_IRUGO);
MODULE_PARM_DESC(mode, "'g729', 'g723.1', or 'any'.  Default 'any'.");
MODULE_DESCRIPTION("Wildcard TC400P+TC400M Driver");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("GPL");

module_init(wctc4xxp_init);
module_exit(wctc4xxp_cleanup);
