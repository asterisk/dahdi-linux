#ifndef	XUSB_H
#define	XUSB_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2008, Xorcom
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

#include <stdio.h>
#include <stdint.h>
#include <usb.h>
#include <xlist.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * Xorcom usb handling
 */

#define	PACKET_SIZE	512

/*
 * Specify the wanted interface
 */
struct xusb_spec {
	/* Sanity checks so we know it is our device indeed */
	int	num_interfaces;
	int	num_endpoints;
	char	*name;	/* For debug/output purpose */
	/* What we will actually use */
	uint16_t	my_vendor_id;
	uint16_t	my_product_id;
	int		my_interface_num;
	int		my_ep_out;
	int		my_ep_in;
};

void xusb_init_spec(struct xusb_spec *xusb_spec, char *name,
		uint16_t vendor_id, uint16_t product_id,
		int nifaces, int iface, int nep, int ep_out, int ep_in);

struct xusb;

/*
 * Prototypes
 */
typedef int (*xusb_filter_t)(const struct xusb *xusb, void *data);
struct xlist_node *xusb_find_byproduct(const struct xusb_spec *specs, int numspecs, xusb_filter_t filterfunc, void *data);
struct xusb *xusb_find_bypath(const struct xusb_spec *specs, int numspecs, const char *path);
struct xusb *xusb_open_one(const struct xusb_spec *specs, int numspecs, xusb_filter_t filterfunc, void *data);
struct xusb *xusb_find_iface(const char *devpath, int iface_num, int ep_out, int ep_in);

/*
 * A convenience filter
 */
int xusb_filter_bypath(const struct xusb *xusb, void *data);

int xusb_interface(struct xusb *xusb);
int xusb_claim_interface(struct xusb *xusb);
void xusb_destroy(struct xusb *xusb);
int xusb_close(struct xusb *xusb);
size_t xusb_packet_size(const struct xusb *xusb);
void xusb_showinfo(const struct xusb *xusb);
const char *xusb_serial(const struct xusb *xusb);
const char *xusb_manufacturer(const struct xusb *xusb);
const char *xusb_product(const struct xusb *xusb);
uint16_t xusb_vendor_id(const struct xusb *xusb);
uint16_t xusb_product_id(const struct xusb *xusb);
const char *xusb_devpath(const struct xusb *xusb);
const struct xusb_spec *xusb_spec(const struct xusb *xusb);
int xusb_send(struct xusb *xusb, char *buf, int len, int timeout);
int xusb_recv(struct xusb *xusb, char *buf, size_t len, int timeout);
int xusb_flushread(struct xusb *xusb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* XUSB_H */
