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

#define	_GNU_SOURCE	/* for memrchr() */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <xusb.h>
#include "astribank_usb.h"
#include <debug.h>

static const char rcsid[] = "$Id$";

#define	DBG_MASK	0x01
#define	TIMEOUT	500

#define	TYPE_ENTRY(t,p,ni,n,ne,out,in,...)	\
		{				\
		.my_vendor_id = 0xe4e4,		\
		.my_product_id = (p),		\
		.name = #t,			\
		.num_interfaces = (ni),		\
		.my_interface_num = (n),	\
		.num_endpoints = (ne),		\
		.my_ep_in = (in),		\
		.my_ep_out = (out),		\
		}

#define	ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))

static const struct xusb_spec	astribank_specs[] = {
	/* OLD Firmwares */
	TYPE_ENTRY("USB-OLDFXS",	0x1131, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("FPGA-OLDFXS",	0x1132, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("USB-BRI",		0x1141, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("FPGA-BRI",		0x1142, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("USB-OLD",		0x1151, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("FPGA-OLD",		0x1152, 2, 1, 2, MP_EP_OUT, MP_EP_IN),

	TYPE_ENTRY("USB-MULTI",		0x1161, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("FPGA-MULTI",	0x1162, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("BURNED-MULTI",	0x1164, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
	TYPE_ENTRY("USB-BURN",		0x1112, 2, 1, 2, MP_EP_OUT, MP_EP_IN),
};

static const struct xusb_spec	astribank_pic_specs[] = {
	TYPE_ENTRY("USB_PIC",		0x1161, 2, 0, 2, XPP_EP_OUT, XPP_EP_IN),
};
#undef TYPE_ENTRY

//static int	verbose = LOG_DEBUG;

/*
 * USB handling
 */
struct astribank_device *astribank_open(const char devpath[], int iface_num)
{
	struct astribank_device	*astribank = NULL;
	struct xusb		*xusb;

	DBG("devpath='%s' iface_num=%d\n", devpath, iface_num);
	if((astribank = malloc(sizeof(struct astribank_device))) == NULL) {
		ERR("Out of memory\n");
		goto fail;
	}
	memset(astribank, 0, sizeof(*astribank));
	if (iface_num) {
		xusb  = xusb_find_bypath(astribank_specs, ARRAY_SIZE(astribank_specs), devpath);
	} else {
		xusb  = xusb_find_bypath(astribank_pic_specs, ARRAY_SIZE(astribank_pic_specs), devpath);
	}
	if (!xusb) {
		ERR("%s: No device found\n", __func__);
		goto fail;
	}
	astribank->xusb = xusb;
	astribank->is_usb2 = (xusb_packet_size(xusb) == 512);
	astribank->my_interface_num = iface_num;
	if (xusb_claim_interface(astribank->xusb) < 0) {
		ERR("xusb_claim_interface failed\n");
		goto fail;
	}
	astribank->tx_sequenceno = 1;
	return astribank;
fail:
	if (astribank) {
		free(astribank);
		astribank = NULL;
	}
	return NULL;
}

/*
 * MP device handling
 */
void show_astribank_info(const struct astribank_device *astribank)
{
	struct xusb			*xusb;

	assert(astribank != NULL);
	xusb = astribank->xusb;
	assert(xusb != NULL);
	if(verbose <= LOG_INFO) {
		xusb_showinfo(xusb);
	} else {
		const struct xusb_spec	*spec;

		spec = xusb_spec(xusb);
		printf("USB    Bus/Device:    [%s]\n", xusb_devpath(xusb));
		printf("USB    Firmware Type: [%s]\n", spec->name);
		printf("USB    iSerialNumber: [%s]\n", xusb_serial(xusb));
		printf("USB    iManufacturer: [%s]\n", xusb_manufacturer(xusb));
		printf("USB    iProduct:      [%s]\n", xusb_product(xusb));
	}
}

void astribank_close(struct astribank_device *astribank, int disconnected)
{
	assert(astribank != NULL);
	if (astribank->xusb) {
		xusb_close(astribank->xusb);
		astribank->xusb = NULL;
	}
	astribank->tx_sequenceno = 0;
}

#if 0
int flush_read(struct astribank_device *astribank)
{
	char		tmpbuf[BUFSIZ];
	int		ret;

	DBG("starting...\n");
	memset(tmpbuf, 0, BUFSIZ);
	ret = recv_usb(astribank, tmpbuf, BUFSIZ, 1);
	if(ret < 0 && ret != -ETIMEDOUT) {
		ERR("ret=%d\n", ret);
		return ret;
	} else if(ret > 0) {
		DBG("Got %d bytes:\n", ret);
		dump_packet(LOG_DEBUG, DBG_MASK, __FUNCTION__, tmpbuf, ret);
	}
	return 0;
}
#endif


int release_isvalid(uint16_t release)
{
	uint8_t	rmajor = (release >> 8) & 0xFF;
	uint8_t	rminor = release & 0xFF;

	return	(rmajor > 0) &&
		(rmajor < 10) &&
		(rminor > 0) &&
		(rminor < 10);
}

int label_isvalid(const char *label)
{
	int		len;
	int		goodlen;
	const char	GOOD_CHARS[] =
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"0123456789"
				"-_.";

	len = strlen(label);
	goodlen = strspn(label, GOOD_CHARS);
	if(len > LABEL_SIZE) {
		ERR("Label too long (%d > %d)\n", len, LABEL_SIZE);
		return 0;
	}
	if(goodlen != len) {
		ERR("Bad character in label (pos=%d)\n", goodlen);
		return 0;
	}
	return 1;
}

int eeprom_fill(struct eeprom_table *eprm,
	const char *vendor,
	const char *product,
	const char *release,
	const char *label)
{
	uint16_t	val;

	eprm->source = 0xC0;
	eprm->config_byte = 0;
	if(vendor) {
		val = strtoul(vendor, NULL, 0);
		if(!val) {
			ERR("Invalid vendor '%s'\n",
				vendor);
			return -EINVAL;
		}
		eprm->vendor = val;
	}
	if(product) {
		val = strtoul(product, NULL, 0);
		if(!val) {
			ERR("Invalid product '%s'\n",
				product);
			return -EINVAL;
		}
		eprm->product = val;
	}
	if(release) {
		int		release_major = 0;
		int		release_minor = 0;
		uint16_t	value;

		if(sscanf(release, "%d.%d", &release_major, &release_minor) != 2) {
			ERR("Failed to parse release number '%s'\n", release);
			return -EINVAL;
		}
		value = (release_major << 8) | release_minor;
		DBG("Parsed release(%d): major=%d, minor=%d\n",
			value, release_major, release_minor);
		if(!release_isvalid(value)) {
			ERR("Invalid release number 0x%X\n", value);
			return -EINVAL;
		}
		eprm->release = value;
	}
	if(label) {
		/* padding */
		if(!label_isvalid(label)) {
			ERR("Invalid label '%s'\n", label);
			return -EINVAL;
		}
		memset(eprm->label, 0, LABEL_SIZE);
		memcpy(eprm->label, label, strlen(label));
	}
	return 0;
}

int astribank_has_twinstar(struct astribank_device *astribank)
{
	uint16_t			product_series;

	assert(astribank != NULL);
	product_series = xusb_product_id(astribank->xusb);
	product_series &= 0xFFF0;
	if(product_series == 0x1160)	/* New boards */
		return 1;
	return 0;
}

