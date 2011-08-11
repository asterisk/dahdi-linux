/*
 * Voicebus network debug interface
 *
 * Written by Shaun Ruffell <sruffell@digium.com>
 *
 * Copyright (C) 2010-2011 Digium, Inc.
 *
 * All rights reserved.

 * VoiceBus is a registered trademark of Digium.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <dahdi/kernel.h>

#include "voicebus.h"
#include "voicebus_net.h"

#ifdef VOICEBUS_NET_DEBUG

struct voicebus_netdev_priv {
	struct voicebus *vb;
};

static inline struct voicebus *
voicebus_from_netdev(struct net_device *netdev)
{
	struct voicebus_netdev_priv *priv;
	priv = netdev_priv(netdev);
	return priv->vb;
}

static void *
skb_to_vbb(struct voicebus *vb, struct sk_buff *skb)
{
	int res;
	struct vbb *vbb;
	const int COMMON_HEADER = 30;
	dma_addr_t dma_addr;

	if (skb->len != (VOICEBUS_SFRAME_SIZE + COMMON_HEADER)) {
		dev_warn(&vb->pdev->dev, "Packet of length %d is not the "
			 "required %d.\n", skb->len,
			 VOICEBUS_SFRAME_SIZE + COMMON_HEADER);
		return NULL;
	}

	vbb = dma_pool_alloc(vb->pool, GFP_KERNEL, &dma_addr);
	if (!vbb)
		return NULL;

	vbb->dma_addr = dma_addr;
	res = skb_copy_bits(skb, COMMON_HEADER, vbb, VOICEBUS_SFRAME_SIZE);
	if (res) {
		dev_warn(&vb->pdev->dev, "Failed call to skb_copy_bits.\n");
		dma_pool_free(vb->pool, vbb, vbb->dma_addr);
		return NULL;
	}
	return vbb;
}

static int
vb_net_hard_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	void *vbb;

	vbb = skb_to_vbb(vb, skb);
	if (vbb)
		voicebus_transmit(vb, vbb);

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static int vb_net_receive(struct voicebus *vb, int max)
{
	int count = 0;
	struct sk_buff *skb;
	WARN_ON(0 == max);
	while ((skb = skb_dequeue(&vb->captured_packets))) {
		netif_receive_skb(skb);
		if (++count >= max)
			break;
	}
	return count;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int vb_net_poll(struct net_device *netdev, int *budget)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	int count = 0;
	int quota = min(netdev->quota, *budget);

	count = vb_net_receive(vb, quota);

	*budget -=       count;
	netdev->quota -= count;

	if (!skb_queue_len(&vb->captured_packets)) {
		netif_rx_complete(netdev);
		return 0;
	} else {
		return -1;
	}
}
#else
static int vb_net_poll(struct napi_struct *napi, int budget)
{
	struct voicebus *vb = container_of(napi, struct voicebus, napi);
	int count;

	count = vb_net_receive(vb, budget);

	if (!skb_queue_len(&vb->captured_packets)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
		netif_rx_complete(vb->netdev, &vb->napi);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
		netif_rx_complete(&vb->napi);
#else
		napi_complete(&vb->napi);
#endif
	}
	return count;
}
#endif

static void vb_net_set_multi(struct net_device *netdev)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	dev_dbg(&vb->pdev->dev, "%s promiscuity:%d\n",
		__func__, netdev->promiscuity);
}

static int vb_net_up(struct net_device *netdev)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	dev_dbg(&vb->pdev->dev, "%s\n", __func__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_poll_enable(netdev);
#else
	napi_enable(&vb->napi);
#endif
	return 0;
}

static int vb_net_down(struct net_device *netdev)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	dev_dbg(&vb->pdev->dev, "%s\n", __func__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_poll_disable(netdev);
#else
	napi_disable(&vb->napi);
#endif
	return 0;
}

static struct net_device_stats *
vb_net_get_stats(struct net_device *netdev)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	return &vb->net_stats;
}

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops vb_netdev_ops = {
	.ndo_set_multicast_list = &vb_net_set_multi,
	.ndo_open = &vb_net_up,
	.ndo_stop = &vb_net_down,
	.ndo_start_xmit = &vb_net_hard_start_xmit,
	.ndo_get_stats = &vb_net_get_stats,
};
#endif

/**
 * vb_net_register - Register a new network interface.
 * @vb: voicebus card to register the interface for.
 *
 * The network interface is primarily used for debugging in order to watch the
 * traffic between the transcoder and the host.
 *
 */
int vb_net_register(struct voicebus *vb, const char *board_name)
{
	int res;
	struct net_device *netdev;
	struct voicebus_netdev_priv *priv;
	const char our_mac[] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

	netdev = alloc_netdev(sizeof(*priv), board_name, ether_setup);
	if (!netdev)
		return -ENOMEM;
	priv = netdev_priv(netdev);
	priv->vb = vb;
	memcpy(netdev->dev_addr, our_mac, sizeof(our_mac));
#	ifdef HAVE_NET_DEVICE_OPS
	netdev->netdev_ops = &vb_netdev_ops;
#	else
	netdev->set_multicast_list = vb_net_set_multi;
	netdev->open = vb_net_up;
	netdev->stop = vb_net_down;
	netdev->hard_start_xmit = vb_net_hard_start_xmit;
	netdev->get_stats = vb_net_get_stats;
#	endif

	netdev->promiscuity = 0;
	netdev->flags |= IFF_NOARP;
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netdev->poll = vb_net_poll;
	netdev->weight = 64;
#	else
	netif_napi_add(netdev, &vb->napi, vb_net_poll, 64);
#	endif

	skb_queue_head_init(&vb->captured_packets);
	res = register_netdev(netdev);
	if (res) {
		dev_warn(&vb->pdev->dev,
			 "Failed to register network device %s.\n", board_name);
		goto error_sw;
	}

	vb->netdev = netdev;

	dev_dbg(&vb->pdev->dev,
		"Created network device %s for debug.\n", board_name);
	return 0;

error_sw:
	if (netdev)
		free_netdev(netdev);
	return res;
}

void vb_net_unregister(struct voicebus *wc)
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

/* Header format for the voicebus network interface. */
struct voicebus_net_hdr {
	struct ethhdr ethhdr;
	__be16 seq_num;
	__be32 des0;
	__be16 tag;
	__be16 filler[4];
} __attribute__((packed));

static struct sk_buff *
vbb_to_skb(struct net_device *netdev, const void *vbb, const int tx,
	   const u32 des0, const u16 tag)
{
	struct voicebus *vb = voicebus_from_netdev(netdev);
	struct sk_buff *skb;
	struct voicebus_net_hdr *hdr;
	/* 0x88B5 is the local experimental ethertype */
	const u16 VOICEBUS_ETHTYPE = 0x88b5;
	const u8 BOARD_MAC[6] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
	const u8 HOST_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	skb = netdev_alloc_skb(vb->netdev,
		VOICEBUS_SFRAME_SIZE + sizeof(*hdr) + NET_IP_ALIGN);
	if (!skb)
		return NULL;

	skb_reserve(skb, NET_IP_ALIGN);
	skb->dev = netdev;
	hdr = (struct voicebus_net_hdr *)skb_put(skb, VOICEBUS_SFRAME_SIZE +
						 sizeof(*hdr));
	/* Fill in the source and destination mac addresses appropriately
	 * depending on whether this is a packet we are transmitting or a packet
	 * that we have received. */
	if (tx) {
		memcpy(hdr->ethhdr.h_dest, BOARD_MAC, sizeof(BOARD_MAC));
		memcpy(hdr->ethhdr.h_source, HOST_MAC, sizeof(HOST_MAC));
		hdr->seq_num = cpu_to_be16(atomic_inc_return(
			&vb->tx_seqnum));
	} else {
		memcpy(hdr->ethhdr.h_dest, HOST_MAC, sizeof(HOST_MAC));
		memcpy(hdr->ethhdr.h_source, BOARD_MAC, sizeof(BOARD_MAC));
		hdr->seq_num = cpu_to_be16(atomic_inc_return(
			&vb->rx_seqnum));
	}
	memset(hdr->filler, 0, sizeof(hdr->filler));
	hdr->des0 = cpu_to_be32(des0);
	hdr->tag = cpu_to_be16(tag);
	hdr->ethhdr.h_proto = htons(VOICEBUS_ETHTYPE);
	/* copy the rest of the packet. */
	memcpy(skb->data + sizeof(*hdr), vbb, VOICEBUS_SFRAME_SIZE);
	skb->protocol = eth_type_trans(skb, netdev);

	return skb;
}

/**
 * vb_net_capture_cmd - Send a vbb to the network stack.
 * @vb: Interface card received the command.
 * @vbb: Voicebus buffer to pass up..
 * @tx: 1 if this is a vbb that the driver is sending to the card.
 *
 */
void vb_net_capture_vbb(struct voicebus *vb, const void *vbb, const int tx,
			const u32 des0, const u16 tag)
{
	struct sk_buff *skb;
	struct net_device *netdev = vb->netdev;
	const int MAX_CAPTURED_PACKETS = 5000;

	if (!netdev)
		return;

	/* If the interface isn't up, we don't need to capture the packet. */
	if (!(netdev->flags & IFF_UP))
		return;

	if (skb_queue_len(&vb->captured_packets) > MAX_CAPTURED_PACKETS) {
		WARN_ON_ONCE(1);
		return;
	}

	skb = vbb_to_skb(netdev, vbb, tx, des0, tag);
	if (!skb)
		return;

	skb_queue_tail(&vb->captured_packets, skb);
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	netif_rx_schedule(netdev);
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
	netif_rx_schedule(netdev, &vb->napi);
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	netif_rx_schedule(&vb->napi);
#	else
	napi_schedule(&vb->napi);
#	endif
	return;
}

#endif
