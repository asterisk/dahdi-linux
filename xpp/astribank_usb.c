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
#include "astribank_usb.h"
#include "debug.h"

static const char rcsid[] = "$Id$";

#define	DBG_MASK	0x01
#define	TIMEOUT	500

#define	TYPE_ENTRY(t,ni,n,ne,out,in,...)	\
	[t] = {					\
		.type_code = (t),		\
		.num_interfaces = (ni),		\
		.my_interface_num = (n),	\
		.num_endpoints = (ne),		\
		.my_ep_in = (in),		\
		.my_ep_out = (out),		\
		.name = #t,			\
		.endpoints = { __VA_ARGS__ },	\
		}

static const struct interface_type interface_types[] = {
	TYPE_ENTRY(USB_11xx,		1, 0, 4, MP_EP_OUT, MP_EP_IN,
		XPP_EP_OUT,
		MP_EP_OUT,
		XPP_EP_IN,
		MP_EP_IN),
	TYPE_ENTRY(USB_FIRMWARE_II,	2, 1, 2, MP_EP_OUT, MP_EP_IN,
		MP_EP_OUT,
		MP_EP_IN),
	TYPE_ENTRY(USB_PIC,		2, 0, 2, XPP_EP_OUT, XPP_EP_IN, 
		XPP_EP_OUT,
		XPP_EP_IN),
	 
};
#undef TYPE_ENTRY

//static int	verbose = LOG_DEBUG;

/*
 * USB handling
 */

/* return 1 if:
 * - str has a number
 * - It is larger than 0
 * - It equals num
 */
static int num_matches(int num, const char* str) {
	int str_val = atoi(str);
	if (str_val <= 0)
		return 0;
	return (str_val == num);
}

struct usb_device *dev_of_path(const char *path)
{
	struct usb_bus		*bus;
	struct usb_device	*dev;
	char			dirname[PATH_MAX];
	char			filename[PATH_MAX];
	const char		*p;
	int			bnum;
	int			dnum;
	int			ret;

	assert(path != NULL);
	if(access(path, F_OK) < 0) {
		perror(path);
		return NULL;
	}
	/* Find last '/' */
	if((p = memrchr(path, '/', strlen(path))) == NULL) {
		ERR("Missing a '/' in %s\n", path);
		return NULL;
	}
	/* Get the device number */
	ret = sscanf(p + 1, "%d", &dnum);
	if(ret != 1) {
		ERR("Path tail is not a device number: '%s'\n", p);
		return NULL;
	}
	/* Search for a '/' before that */
	p = memrchr(path, '/', p - path);
	if(p == NULL)
		p = path;		/* Relative path */
	else
		p++;			/* skip '/' */
	/* Get the bus number */
	ret = sscanf(p, "%d", &bnum);
	if(ret != 1) {
		ERR("Path tail is not a bus number: '%s'\n", p);
		return NULL;
	}
	sprintf(dirname, "%03d", bnum);
	sprintf(filename, "%03d", dnum);
	for (bus = usb_busses; bus; bus = bus->next) {
		if (! num_matches(bnum, bus->dirname))
		//if(strcmp(bus->dirname, dirname) != 0)
			continue;
		for (dev = bus->devices; dev; dev = dev->next) {
			//if(strcmp(dev->filename, filename) == 0)
			if (num_matches(dnum, dev->filename))
				return dev;
		}
	}
	ERR("no usb device match '%s'\n", path);
	return NULL;
}

int get_usb_string(struct astribank_device *astribank, uint8_t item, char *buf, unsigned int len)
{
	char	tmp[BUFSIZ];
	int	ret;

	assert(astribank->handle);
	if (!item)
		return 0;
	ret = usb_get_string_simple(astribank->handle, item, tmp, BUFSIZ);
	if (ret <= 0)
		return ret;
	return snprintf(buf, len, "%s", tmp);
}

static int match_interface(const struct astribank_device *astribank,
	const struct interface_type *itype)
{
	struct usb_interface		*interface;
	struct usb_interface_descriptor	*iface_desc;
	struct usb_config_descriptor	*config_desc;
	int				i = itype - interface_types;
	int				inum;
	int				num_altsetting;

	DBG("Checking[%d]: interfaces=%d interface num=%d endpoints=%d: \"%s\"\n",
			i,
			itype->num_interfaces,
			itype->my_interface_num,
			itype->num_endpoints,
			itype->name);
	config_desc = astribank->dev->config;
	if (!config_desc) {
		ERR("No configuration descriptor: strange USB1 controller?\n");
		return 0;
	}
	if(config_desc->bNumInterfaces <= itype->my_interface_num) {
		DBG("Too little interfaces: have %d need %d\n",
			config_desc->bNumInterfaces, itype->my_interface_num + 1);
		return 0;
	}
	if(astribank->my_interface_num != itype->my_interface_num) {
		DBG("Wrong match -- not my interface num (wanted %d)\n", astribank->my_interface_num);
		return 0;
	}
	inum = itype->my_interface_num;
	interface = &config_desc->interface[inum];
	assert(interface != NULL);
	iface_desc = interface->altsetting;
	num_altsetting = interface->num_altsetting;
	assert(num_altsetting != 0);
	assert(iface_desc != NULL);
	if(iface_desc->bInterfaceClass != 0xFF) {
		DBG("Bad interface class 0x%X\n", iface_desc->bInterfaceClass);
		return 0;
	}
	if(iface_desc->bInterfaceNumber != itype->my_interface_num) {
		DBG("Bad interface number %d\n", iface_desc->bInterfaceNumber);
		return 0;
	}
	if(iface_desc->bNumEndpoints != itype->num_endpoints) {
		DBG("Different number of endpoints %d\n", iface_desc->bNumEndpoints);
		return 0;
	}
	return	1;
}

static int astribank_init(struct astribank_device *astribank)
{
	struct usb_device_descriptor	*dev_desc;
	struct usb_config_descriptor	*config_desc;
	struct usb_interface		*interface;
	struct usb_interface_descriptor	*iface_desc;
	struct usb_endpoint_descriptor	*endpoint;
	const struct interface_type	*fwtype;
	int				i;

	assert(astribank);
	astribank->handle = usb_open(astribank->dev);
	if(!astribank->handle) {
		ERR("Failed to open usb device '%s/%s': %s\n",
			astribank->dev->bus->dirname, astribank->dev->filename, usb_strerror());
		return 0;
	}
	fwtype = astribank->fwtype;
	if(usb_claim_interface(astribank->handle, fwtype->my_interface_num) != 0) {
		ERR("usb_claim_interface: %s\n", usb_strerror());
		return 0;
	}
	dev_desc = &astribank->dev->descriptor;
	config_desc = astribank->dev->config;
	if (!config_desc) {
		ERR("usb interface without a configuration\n");
		return 0;
	}
	DBG("Got config_desc. Looking for interface %d\n", fwtype->my_interface_num);
	interface = &config_desc->interface[fwtype->my_interface_num];
	iface_desc = interface->altsetting;
	endpoint = iface_desc->endpoint;
	astribank->is_usb2 = (endpoint->wMaxPacketSize == 512);
	for(i = 0; i < iface_desc->bNumEndpoints; i++, endpoint++) {
		DBG("Validating endpoint @ %d (interface %d)\n", i, fwtype->my_interface_num);
		if(endpoint->bEndpointAddress != fwtype->endpoints[i]) {
			ERR("Wrong endpoint 0x%X != 0x%X (at index %d)\n",
				endpoint->bEndpointAddress,
				fwtype->endpoints[i],
				i);
			return 0;
		}
		if(endpoint->bEndpointAddress == MP_EP_OUT || endpoint->bEndpointAddress == MP_EP_IN) {
			if(endpoint->wMaxPacketSize > PACKET_SIZE) {
				ERR("Endpoint #%d wMaxPacketSize too large (%d)\n", i, endpoint->wMaxPacketSize);
				return 0;
			}
		}
	}
	astribank->my_ep_in = fwtype->my_ep_in;
	astribank->my_ep_out = fwtype->my_ep_out;
	if(get_usb_string(astribank, dev_desc->iManufacturer, astribank->iManufacturer, BUFSIZ) < 0)
		return 0;
	if(get_usb_string(astribank, dev_desc->iProduct, astribank->iProduct, BUFSIZ) < 0)
		return 0;
	if(get_usb_string(astribank, dev_desc->iSerialNumber, astribank->iSerialNumber, BUFSIZ) < 0)
		return 0;
	if(get_usb_string(astribank, iface_desc->iInterface, astribank->iInterface, BUFSIZ) < 0)
		return 0;
	DBG("ID=%04X:%04X Manufacturer=[%s] Product=[%s] SerialNumber=[%s] Interface=[%s]\n",
		dev_desc->idVendor,
		dev_desc->idProduct,
		astribank->iManufacturer,
		astribank->iProduct,
		astribank->iSerialNumber,
		astribank->iInterface);
	if(usb_clear_halt(astribank->handle, astribank->my_ep_out) != 0) {
		ERR("Clearing output endpoint: %s\n", usb_strerror());
		return 0;
	}
	if(usb_clear_halt(astribank->handle, astribank->my_ep_in) != 0) {
		ERR("Clearing input endpoint: %s\n", usb_strerror());
		return 0;
	}
	if((i = flush_read(astribank)) < 0) {
		ERR("flush_read failed: %d\n", i);
		return 0;
	}
	return 1;
}

struct astribank_device *astribank_open(const char devpath[], int iface_num)
{
	struct astribank_device		*astribank;
	int				i;

	DBG("devpath='%s' iface_num=%d\n", devpath, iface_num);
	if((astribank = malloc(sizeof(*astribank))) == NULL) {
		ERR("Out of memory");
		return NULL;
	}
	memset(astribank, 0, sizeof(*astribank));
	astribank->my_interface_num = iface_num;
	usb_init();
	usb_find_busses();
	usb_find_devices();
	astribank->dev = dev_of_path(devpath);
	if(!astribank->dev) {
		ERR("Bailing out\n");
		goto fail;
	}
	DBG("Scan interface types (astribank has %d interfaces)\n", astribank->dev->config->bNumInterfaces);
	for(i = 0; i < sizeof(interface_types)/sizeof(interface_types[0]); i++) {
		if(match_interface(astribank, &interface_types[i])) {
			DBG("Identified[%d]: interfaces=%d endpoints=%d: \"%s\"\n",
				i,
				interface_types[i].num_interfaces,
				interface_types[i].num_endpoints,
				interface_types[i].name);
			astribank->fwtype = &interface_types[i];
			goto found;
		}
	}
	ERR("Didn't find suitable device\n");
fail:
	free(astribank);
	return NULL;
found:
	if(!astribank_init(astribank))
		goto fail;
	astribank->tx_sequenceno = 1;
	return astribank;
}

/*
 * MP device handling
 */
void show_astribank_info(const struct astribank_device *astribank)
{
	struct usb_device_descriptor	*dev_desc;
	struct usb_device		*dev;

	assert(astribank != NULL);
	dev = astribank->dev;
	dev_desc = &dev->descriptor;
	if(verbose <= LOG_INFO) {
		INFO("usb:%s/%s: ID=%04X:%04X [%s / %s / %s]\n",
			dev->bus->dirname,
			dev->filename,
			dev_desc->idVendor,
			dev_desc->idProduct,
			astribank->iManufacturer,
			astribank->iProduct,
			astribank->iSerialNumber);
	} else {
		printf("USB    Bus/Device:    [%s/%s]\n", dev->bus->dirname, dev->filename);
		printf("USB    Firmware Type: [%s]\n", astribank->fwtype->name);
		printf("USB    iManufacturer: [%s]\n", astribank->iManufacturer);
		printf("USB    iProduct:      [%s]\n", astribank->iProduct);
		printf("USB    iSerialNumber: [%s]\n", astribank->iSerialNumber);
	}
}

void astribank_close(struct astribank_device *astribank, int disconnected)
{
	assert(astribank != NULL);
	if(!astribank->handle)
		return;	/* Nothing to do */
	if(!disconnected) {
		if(usb_release_interface(astribank->handle, astribank->fwtype->my_interface_num) != 0) {
			ERR("Releasing interface: usb: %s\n", usb_strerror());
		}
	}
	if(usb_close(astribank->handle) != 0) {
		ERR("Closing device: usb: %s\n", usb_strerror());
	}
	astribank->tx_sequenceno = 0;
	astribank->handle = NULL;
}

int send_usb(struct astribank_device *astribank, char *buf, int len, int timeout)
{
	int		ret;

	dump_packet(LOG_DEBUG, __FUNCTION__, buf, len);
	if(astribank->my_ep_out & USB_ENDPOINT_IN) {
		ERR("send_usb called with an input endpoint 0x%x\n", astribank->my_ep_out);
		return -EINVAL;
	}
	ret = usb_bulk_write(astribank->handle, astribank->my_ep_out, buf, len, timeout);
	if(ret < 0) {
		/*
		 * If the device was gone, it may be the
		 * result of renumeration. Ignore it.
		 */
		if(ret != -ENODEV) {
			ERR("bulk_write to endpoint 0x%x failed: (%d) %s\n",
				astribank->my_ep_out, ret, usb_strerror());
			dump_packet(LOG_ERR, "send_usb[ERR]", buf, len);
			exit(2);
		} else {
			DBG("bulk_write to endpoint 0x%x got ENODEV\n", astribank->my_ep_out);
			astribank_close(astribank, 1);
		}
		return ret;
	} else if(ret != len) {
		ERR("bulk_write to endpoint 0x%x short write: (%d) %s\n",
			astribank->my_ep_out, ret, usb_strerror());
		dump_packet(LOG_ERR, "send_usb[ERR]", buf, len);
		return -EFAULT;
	}
	return ret;
}

int recv_usb(struct astribank_device *astribank, char *buf, size_t len, int timeout)
{
	int	ret;

	if(astribank->my_ep_in & USB_ENDPOINT_OUT) {
		ERR("recv_usb called with an output endpoint 0x%x\n", astribank->my_ep_in);
		return -EINVAL;
	}
	ret = usb_bulk_read(astribank->handle, astribank->my_ep_in, buf, len, timeout);
	if(ret < 0) {
		DBG("bulk_read from endpoint 0x%x failed: (%d) %s\n",
			astribank->my_ep_in, ret, usb_strerror());
		memset(buf, 0, len);
		return ret;
	}
	dump_packet(LOG_DEBUG, __FUNCTION__, buf, ret);
	return ret;
}

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
		dump_packet(LOG_DEBUG, __FUNCTION__, tmpbuf, ret);
	}
	return 0;
}


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
	struct usb_device_descriptor	*dev_desc;
	uint16_t			product_series;

	assert(astribank != NULL);
	dev_desc = &astribank->dev->descriptor;
	product_series = dev_desc->idProduct;
	product_series &= 0xFFF0;
	if(product_series == 0x1160)	/* New boards */
		return 1;
	return 0;
}

