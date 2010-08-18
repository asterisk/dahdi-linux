/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2006-2008, Xorcom
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <usb.h>
#include "hexfile.h"

static const char rcsid[] = "$Id$";

#define	ERR(fmt, arg...) do { \
				if(verbose >= LOG_ERR) \
					fprintf(stderr, "%s: ERROR (%d): " fmt, \
						progname, __LINE__, ## arg); \
			} while(0);
#define	INFO(fmt, arg...) do { \
				if(verbose >= LOG_INFO) \
					fprintf(stderr, "%s: " fmt, \
						progname, ## arg); \
			} while(0);
#define	DBG(fmt, arg...) do { \
				if(verbose >= LOG_DEBUG) \
					fprintf(stderr, "%s: DBG: " fmt, \
						progname, ## arg); \
			} while(0);

static int	verbose = LOG_WARNING;
static char	*progname;
static int	disconnected = 0;

#define	MAX_HEX_LINES	10000
#define	PACKET_SIZE	512
#define	EEPROM_SIZE	16
#define	LABEL_SIZE	8
#define	TIMEOUT		5000


/* My device parameters */
#define	MY_EP_OUT	0x04
#define	MY_EP_IN	0x88

#define	FPGA_EP_OUT	0x02
#define	FPGA_EP_IN	0x86

/* USB firmware types */
#define	USB_11xx	0
#define	USB_FIRMWARE_II	1

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

static const struct astribank_type {
	int	type_code;
	int	num_interfaces;
	int	my_interface_num;
	int	num_endpoints;
	int	my_ep_out;
	int	my_ep_in;
	char	*name;
	int	endpoints[4];	/* for matching */
} astribank_types[] = {
	TYPE_ENTRY(USB_11xx,		1, 0, 4, MY_EP_OUT, MY_EP_IN,
		FPGA_EP_OUT,
		MY_EP_OUT,
		FPGA_EP_IN,
		MY_EP_IN),
	TYPE_ENTRY(USB_FIRMWARE_II,	2, 1, 2, MY_EP_OUT, MY_EP_IN,
		MY_EP_OUT,
		MY_EP_IN),
};
#undef TYPE_ENTRY

enum fpga_load_packet_types {
	PT_STATUS_REPLY	= 0x01,
	PT_DATA_PACKET	= 0x01,
#ifdef	XORCOM_INTERNAL
	PT_EEPROM_SET	= 0x04,
#endif
	PT_EEPROM_GET	= 0x08,
	PT_RENUMERATE	= 0x10,
	PT_RESET	= 0x20,
	PT_BAD_COMMAND	= 0xAA
};

struct myeeprom {
	uint8_t		source;
	uint16_t	vendor;
	uint16_t	product;
	uint8_t		release_major;
	uint8_t		release_minor;
	uint8_t		reserved;
	uint8_t		label[LABEL_SIZE];
} PACKED;

struct fpga_packet_header {
	struct {
		uint8_t		op;
	} PACKED header;
	union {
		struct {
			uint16_t	seq;
			uint8_t		status;
		} PACKED status_reply;
		struct {
			uint16_t	seq;
			uint8_t		reserved;
			uint8_t		data[ZERO_SIZE];
		} PACKED data_packet;
		struct {
			struct myeeprom		data;
		} PACKED eeprom_set;
		struct {
			struct myeeprom		data;
		} PACKED eeprom_get;
	} d;
} PACKED;

enum fpga_load_status {
	FW_FAIL_RESET	= 1,
	FW_FAIL_TRANS	= 2,
	FW_TRANS_OK	= 4,
	FW_CONFIG_DONE	= 8
};

struct my_usb_device {
	struct usb_device	*dev;
	usb_dev_handle		*handle;
	int			my_interface_num;
	int			my_ep_out;
	int			my_ep_in;
	char			iManufacturer[BUFSIZ];
	char			iProduct[BUFSIZ];
	char			iSerialNumber[BUFSIZ];
	char			iInterface[BUFSIZ];
	int			is_usb2;
	struct myeeprom		eeprom;
	const struct astribank_type	*abtype;
};

const char *load_status2str(enum fpga_load_status s)
{
	switch(s) {
		case FW_FAIL_RESET: return "FW_FAIL_RESET";
		case FW_FAIL_TRANS: return "FW_FAIL_TRANS";
		case FW_TRANS_OK: return "FW_TRANS_OK";
		case FW_CONFIG_DONE: return "FW_CONFIG_DONE";
		default: return "UNKNOWN";
	}
}

/* return 1 if:
 * - str has a number
 * - It is larger than 0
 * - It equals num
 */
int num_matches(int num, const char* str) {
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
	if((p = (const char *)memrchr(path, '/', strlen(path))) == NULL) {
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
	p = (const char *)memrchr(path, '/', p - path);
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

int get_usb_string(char *buf, unsigned int len, uint16_t item, usb_dev_handle *handle)
{
	char	tmp[BUFSIZ];
	int	ret;

	if (!item)
		return 0;
	ret = usb_get_string_simple(handle, item, tmp, BUFSIZ);
	if (ret <= 0)
		return ret;
	return snprintf(buf, len, "%s", tmp);
}

void my_usb_device_cleanup(struct my_usb_device *mydev)
{
	assert(mydev != NULL);
	if(!mydev->handle) {
		return;	/* Nothing to do */
	}
	if(!disconnected) {
		if(usb_release_interface(mydev->handle, mydev->abtype->my_interface_num) != 0) {
			ERR("Releasing interface: usb: %s\n", usb_strerror());
		}
	}
	if(usb_close(mydev->handle) != 0) {
		ERR("Closing device: usb: %s\n", usb_strerror());
	}
	disconnected = 1;
	mydev->handle = NULL;
}

static void show_device_info(const struct my_usb_device *mydev)
{
	const struct myeeprom	*eeprom;
	uint8_t		data[LABEL_SIZE + 1];

	assert(mydev != NULL);
	eeprom = &mydev->eeprom;
	memset(data, 0, LABEL_SIZE + 1);
	memcpy(data, eeprom->label, LABEL_SIZE);
	printf("USB    Firmware Type: [%s]\n", mydev->abtype->name);
	printf("USB    iManufacturer: [%s]\n", mydev->iManufacturer);
	printf("USB    iProduct:      [%s]\n", mydev->iProduct);
	printf("USB    iSerialNumber: [%s]\n", mydev->iSerialNumber);
	printf("EEPROM Source:        0x%02X\n", eeprom->source);
	printf("EEPROM Vendor:        0x%04X\n", eeprom->vendor);
	printf("EEPROM Product:       0x%04X\n", eeprom->product);
	printf("EEPROM Release:       %d.%03d\n", eeprom->release_major, eeprom->release_minor);
	printf("EEPROM Label:        HEX(%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X) [%s]\n",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7], data); 
}

void dump_packet(const char *msg, const char *buf, int len)
{
	int	i;

	for(i = 0; i < len; i++)
		INFO("%s: %2d> 0x%02X\n", msg, i, (uint8_t)buf[i]);
}

int send_usb(const char *msg, struct my_usb_device *mydev, struct fpga_packet_header *phead, int len, int timeout)
{
	char	*p = (char *)phead;
	int	ret;

	if(verbose >= LOG_DEBUG)
		dump_packet(msg, p, len);
	if(mydev->my_ep_out & USB_ENDPOINT_IN) {
		ERR("send_usb called with an input endpoint 0x%x\n", mydev->my_ep_out);
		return -EINVAL;
	}
	ret = usb_bulk_write(mydev->handle, mydev->my_ep_out, p, len, timeout);
	if(ret < 0) {
		/*
		 * If the device was gone, it may be the
		 * result of renumeration. Ignore it.
		 */
		if(ret != -ENODEV) {
			ERR("bulk_write to endpoint 0x%x failed: %s\n", mydev->my_ep_out, usb_strerror());
			dump_packet("send_usb[ERR]", p, len);
		} else {
			disconnected = 1;
			my_usb_device_cleanup(mydev);
		}
		return ret;
	} else if(ret != len) {
		ERR("bulk_write to endpoint 0x%x short write: %s\n", mydev->my_ep_out, usb_strerror());
		dump_packet("send_usb[ERR]", p, len);
		return -EFAULT;
	}
	return ret;
}

int recv_usb(const char *msg, struct my_usb_device *mydev, char *buf, size_t len, int timeout)
{
	int	ret;

	if(mydev->my_ep_in & USB_ENDPOINT_OUT) {
		ERR("recv_usb called with an output endpoint 0x%x\n", mydev->my_ep_in);
		return -EINVAL;
	}
	ret = usb_bulk_read(mydev->handle, mydev->my_ep_in, buf, len, timeout);
	if(ret < 0) {
		ERR("bulk_read from endpoint 0x%x failed: %s\n", mydev->my_ep_in, usb_strerror());
		return ret;
	}
	if(verbose >= LOG_DEBUG)
		dump_packet(msg, buf, ret);
	return ret;
}

int flush_read(struct my_usb_device *mydev)
{
	char		tmpbuf[BUFSIZ];
	int		ret;

	memset(tmpbuf, 0, BUFSIZ);
	ret = recv_usb("flush_read", mydev, tmpbuf, sizeof(tmpbuf), TIMEOUT);
	if(ret < 0 && ret != -ETIMEDOUT) {
		ERR("ret=%d\n", ret);
		return ret;
	} else if(ret > 0) {
		DBG("Got %d bytes:\n", ret);
		dump_packet(__FUNCTION__, tmpbuf, ret);
	}
	return 0;
}

#ifdef	XORCOM_INTERNAL
int eeprom_set(struct my_usb_device *mydev, const struct myeeprom *eeprom)
{
	int				ret;
	int				len;
	char				buf[PACKET_SIZE];
	struct fpga_packet_header	*phead = (struct fpga_packet_header *)buf;

	DBG("%s Start...\n", __FUNCTION__);
	assert(mydev != NULL);
	phead->header.op = PT_EEPROM_SET;
	memcpy(&phead->d.eeprom_set.data, eeprom, EEPROM_SIZE);
	len = sizeof(phead->d.eeprom_set) + sizeof(phead->header.op);
	ret = send_usb("eeprom_set[W]", mydev, phead, len, TIMEOUT);
	if(ret < 0)
		return ret;
	ret = recv_usb("eeprom_set[R]", mydev, buf, sizeof(buf), TIMEOUT);
	if(ret <= 0)
		return ret;
	phead = (struct fpga_packet_header *)buf;
	if(phead->header.op == PT_BAD_COMMAND) {
		ERR("Firmware rejected PT_EEPROM_SET command\n");
		return -EINVAL;
	} else if(phead->header.op != PT_EEPROM_SET) {
		ERR("Got unexpected reply op=%d\n", phead->header.op);
		return -EINVAL;
	}
	return 0;
}
#endif

int eeprom_get(struct my_usb_device *mydev)
{
	int				ret;
	int				len;
	char				buf[PACKET_SIZE];
	struct fpga_packet_header	*phead = (struct fpga_packet_header *)buf;
	struct myeeprom			*eeprom;

	assert(mydev != NULL);
	eeprom = &mydev->eeprom;
	DBG("%s Start...\n", __FUNCTION__);
	phead->header.op = PT_EEPROM_GET;
	len = sizeof(phead->header.op);		/* warning: sending small packet */
	ret = send_usb("eeprom_get[W]", mydev, phead, len, TIMEOUT);
	if(ret < 0)
		return ret;
	ret = recv_usb("eeprom_get[R]", mydev, buf, sizeof(buf), TIMEOUT);
	if(ret <= 0)
		return ret;
	phead = (struct fpga_packet_header *)buf;
	if(phead->header.op == PT_BAD_COMMAND) {
		ERR("PT_BAD_COMMAND\n");
		return -EINVAL;
	} else if(phead->header.op != PT_EEPROM_GET) {
		ERR("Got unexpected reply op=%d\n", phead->header.op);
		return -EINVAL;
	}
	memcpy(eeprom, &phead->d.eeprom_get.data, EEPROM_SIZE);
	return 0;
}

int send_hexline(struct my_usb_device *mydev, struct hexline *hexline, int seq)
{
	int				ret;
	int				len;
	uint8_t				*data;
	char				buf[PACKET_SIZE];
	struct fpga_packet_header	*phead = (struct fpga_packet_header *)buf;
	enum fpga_load_status		status;

	assert(mydev != NULL);
	assert(hexline != NULL);
	if(hexline->d.content.header.tt != TT_DATA) {
		DBG("Non data record %d type = %d\n", seq, hexline->d.content.header.tt);
		return 0;
	}
	len = hexline->d.content.header.ll;	/* don't send checksum */
	data = hexline->d.content.tt_data.data;
	phead->header.op = PT_DATA_PACKET;
	phead->d.data_packet.seq = seq;
	phead->d.data_packet.reserved = 0x00;
	memcpy(phead->d.data_packet.data, data, len);
	len += sizeof(hexline->d.content.header);
	DBG("%04d+\r", seq);
	ret = send_usb("hexline[W]", mydev, phead, len, TIMEOUT);
	if(ret < 0)
		return ret;
	ret = recv_usb("hexline[R]", mydev, buf, sizeof(buf), TIMEOUT);
	if(ret <= 0)
		return ret;
	DBG("%04d-\r", seq);
	phead = (struct fpga_packet_header *)buf;
	if(phead->header.op != PT_STATUS_REPLY) {
		ERR("Got unexpected reply op=%d\n", phead->header.op);
		dump_packet("hexline[ERR]", buf, ret);
		return -EINVAL;
	}
	status = (enum fpga_load_status)phead->d.status_reply.status;
	switch(status) {
		case FW_TRANS_OK:
		case FW_CONFIG_DONE:
			break;
		case FW_FAIL_RESET:
		case FW_FAIL_TRANS:
			ERR("status reply %s (%d)\n", load_status2str(status), status);
			dump_packet("hexline[ERR]", buf, ret);
			return -EPROTO;
		default:
			ERR("Unknown status reply %d\n", status);
			dump_packet("hexline[ERR]", buf, ret);
			return -EPROTO;
	}
	return 0;
}

//. returns > 0 - ok, the number of lines sent
//. returns < 0 - error number
int send_splited_hexline(struct my_usb_device *mydev, struct hexline *hexline, int seq, uint8_t maxwidth)
{
	struct hexline *extraline;
	int linessent = 0;
	int allocsize;
	int extra_offset = 0;
	unsigned int this_line = 0;
	uint8_t bytesleft = 0;
	
	assert(mydev != NULL);
	if(!hexline) {
		ERR("Bad record %d type = %d\n", seq, hexline->d.content.header.tt);
		return -EINVAL;
	}
	bytesleft = hexline->d.content.header.ll;
	// split the line into several lines
	while (bytesleft > 0) {
		int status;
		this_line = (bytesleft >= maxwidth) ? maxwidth : bytesleft;
		allocsize = sizeof(struct hexline) + this_line + 1;
		// generate the new line
		if((extraline = (struct hexline *)malloc(allocsize)) == NULL) {
			ERR("Not enough memory for spliting the lines\n" );
			return -EINVAL;
		}
		memset(extraline, 0, allocsize);
		extraline->d.content.header.ll		= this_line;
		extraline->d.content.header.offset	= hexline->d.content.header.offset + extra_offset;
		extraline->d.content.header.tt		= hexline->d.content.header.tt;
		memcpy( extraline->d.content.tt_data.data, hexline->d.content.tt_data.data+extra_offset, this_line);
		status = send_hexline(mydev, extraline, seq+linessent );
		// cleanups
		free(extraline);
		extra_offset += this_line;
		bytesleft -= this_line;
		if (status)
			return status;
		linessent++;
	}
	return linessent;
}

int match_usb_device_identity(const struct usb_config_descriptor *config_desc,
	const struct astribank_type *ab)
{
	struct usb_interface		*interface;
	struct usb_interface_descriptor	*iface_desc;

	if(config_desc->bNumInterfaces <= ab->my_interface_num)
		return 0;
	interface = &config_desc->interface[ab->my_interface_num];
	iface_desc = interface->altsetting;
	
	return	iface_desc->bInterfaceClass == 0xFF &&
		iface_desc->bInterfaceNumber == ab->my_interface_num &&
		iface_desc->bNumEndpoints == ab->num_endpoints;
}

const struct astribank_type *my_usb_device_identify(const char devpath[], struct my_usb_device *mydev)
{
	struct usb_device_descriptor	*dev_desc;
	struct usb_config_descriptor	*config_desc;
	int				i;

	assert(mydev != NULL);
	usb_init();
	usb_find_busses();
	usb_find_devices();
	mydev->dev = dev_of_path(devpath);
	if(!mydev->dev) {
		ERR("Bailing out\n");
		return 0;
	}
	dev_desc = &mydev->dev->descriptor;
	config_desc = mydev->dev->config;
	for(i = 0; i < sizeof(astribank_types)/sizeof(astribank_types[0]); i++) {
		if(match_usb_device_identity(config_desc, &astribank_types[i])) {
			DBG("Identified[%d]: interfaces=%d endpoints=%d: \"%s\"\n",
				i,
				astribank_types[i].num_interfaces,
				astribank_types[i].num_endpoints,
				astribank_types[i].name);
			return &astribank_types[i];
		}
	}
	return NULL;
}

int my_usb_device_init(const char devpath[], struct my_usb_device *mydev, const struct astribank_type *abtype)
{
	struct usb_device_descriptor	*dev_desc;
	struct usb_config_descriptor	*config_desc;
	struct usb_interface		*interface;
	struct usb_interface_descriptor	*iface_desc;
	struct usb_endpoint_descriptor	*endpoint;
	int				ret;
	int				i;

	assert(mydev != NULL);
	usb_init();
	usb_find_busses();
	usb_find_devices();
	mydev->dev = dev_of_path(devpath);
	if(!mydev->dev) {
		ERR("Bailing out\n");
		return 0;
	}
	mydev->handle = usb_open(mydev->dev);
	if(!mydev->handle) {
		ERR("Failed to open usb device '%s/%s': %s\n", mydev->dev->bus->dirname, mydev->dev->filename, usb_strerror());
		return 0;
	}
	if(usb_claim_interface(mydev->handle, abtype->my_interface_num) != 0) {
		ERR("usb_claim_interface: %s\n", usb_strerror());
		return 0;
	}
	dev_desc = &mydev->dev->descriptor;
	config_desc = mydev->dev->config;
	if (!config_desc) {
		ERR("usb interface without a configuration\n");
		return 0;
	}
	interface = &config_desc->interface[abtype->my_interface_num];
	iface_desc = interface->altsetting;
	endpoint = iface_desc->endpoint;
	mydev->is_usb2 = (endpoint->wMaxPacketSize == 512);
	for(i = 0; i < iface_desc->bNumEndpoints; i++, endpoint++) {
		if(endpoint->bEndpointAddress != abtype->endpoints[i]) {
			ERR("Wrong endpoint 0x%X (at index %d)\n", endpoint->bEndpointAddress, i);
			return 0;
		}
		if(endpoint->bEndpointAddress == MY_EP_OUT || endpoint->bEndpointAddress == MY_EP_IN) {
			if(endpoint->wMaxPacketSize > PACKET_SIZE) {
				ERR("Endpoint #%d wMaxPacketSize too large (%d)\n", i, endpoint->wMaxPacketSize);
				return 0;
			}
		}
	}
	mydev->abtype = abtype;
	mydev->my_ep_in = abtype->my_ep_in;
	mydev->my_ep_out = abtype->my_ep_out;
	ret = get_usb_string(mydev->iManufacturer, BUFSIZ, dev_desc->iManufacturer, mydev->handle);
	ret = get_usb_string(mydev->iProduct, BUFSIZ, dev_desc->iProduct, mydev->handle);
	ret = get_usb_string(mydev->iSerialNumber, BUFSIZ, dev_desc->iSerialNumber, mydev->handle);
	ret = get_usb_string(mydev->iInterface, BUFSIZ, iface_desc->iInterface, mydev->handle);
	INFO("ID=%04X:%04X Manufacturer=[%s] Product=[%s] SerialNumber=[%s] Interface=[%s]\n",
		dev_desc->idVendor,
		dev_desc->idProduct,
		mydev->iManufacturer,
		mydev->iProduct,
		mydev->iSerialNumber,
		mydev->iInterface);
	if(usb_clear_halt(mydev->handle, mydev->my_ep_out) != 0) {
		ERR("Clearing output endpoint: %s\n", usb_strerror());
		return 0;
	}
	if(usb_clear_halt(mydev->handle, mydev->my_ep_in) != 0) {
		ERR("Clearing input endpoint: %s\n", usb_strerror());
		return 0;
	}
	if(flush_read(mydev) < 0) {
		ERR("flush_read failed\n");
		return 0;
	}
	return 1;
}

int renumerate_device(struct my_usb_device *mydev, enum fpga_load_packet_types pt)
{
	char				buf[PACKET_SIZE];
	struct fpga_packet_header	*phead = (struct fpga_packet_header *)buf;
	int				ret;

	assert(mydev != NULL);
	DBG("Renumerating with 0x%X\n", pt);
	phead->header.op = pt;
	ret = send_usb("renumerate[W]", mydev, phead, 1, TIMEOUT);
	if(ret < 0 && ret != -ENODEV)
			return ret;
#if 0
	/*
	 * FIXME: we count on our USB firmware to reset the device... should we?
	 */
	ret = usb_reset(mydev->handle);
	if(ret < 0) {
		ERR("usb_reset: %s\n", usb_strerror());
		return -ENODEV;
	}
#endif
	return 0;
}

/*
 * Returns: true on success, false on failure
 */
int fpga_load(struct my_usb_device *mydev, const struct hexdata *hexdata)
{
	unsigned int	i;
	unsigned int	j = 0;
	int		ret;
	int		finished = 0;
	const char	*v = hexdata->version_info;
	
	v = (v[0]) ? v : "Unknown";
	assert(mydev != NULL);
	INFO("FPGA_LOAD (version %s)\n", v);
	/*
	 * i - is the line number
	 * j - is the sequence number, on USB 2, i=j, but on
	 *     USB 1 send_splited_hexline may increase the sequence
	 *     number, as it needs 
	 */
	for(i = 0; i < hexdata->maxlines; i++) {
		struct hexline	*hexline = hexdata->lines[i];

		if(!hexline)
			break;
		if(finished) {
			ERR("Extra data after End Of Data Record (line %d)\n", i);
			return 0;
		}
		if(hexline->d.content.header.tt == TT_EOF) {
			DBG("End of data\n");
			finished = 1;
			continue;
		}
		if(mydev->is_usb2) {
			if((ret = send_hexline(mydev, hexline, i)) != 0) {
				perror("Failed sending hexline");
				return 0;
			}
		} else {
			if((ret = send_splited_hexline(mydev, hexline, j, 60)) < 0) {
				perror("Failed sending hexline (splitting did not help)");
				return 0;
			}
			j += ret;
		}
	}
	DBG("Finished...\n");
	return 1;
}

#include <getopt.h>

void usage()
{
	fprintf(stderr, "Usage: %s -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev> [options...]\n", progname);
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\t\t[-r]		# Reset the device\n");
	fprintf(stderr, "\t\t[-b <binfile>]	# Output to <binfile>\n");
	fprintf(stderr, "\t\t[-I <hexfile>]	# Input from <hexfile>\n");
	fprintf(stderr, "\t\t[-H <hexfile>]	# Output to <hexfile> ('-' is stdout)\n");
	fprintf(stderr, "\t\t[-i]		# Show hexfile information\n");
	fprintf(stderr, "\t\t[-g]		# Get eeprom from device\n");
	fprintf(stderr, "\t\t[-v]		# Increase verbosity\n");
#ifdef XORCOM_INTERNAL
	fprintf(stderr, "\t\t[-C srC byte]	# Set Address sourCe (default: C0)\n");
	fprintf(stderr, "\t\t[-V vendorid]	# Set Vendor id on device\n");
	fprintf(stderr, "\t\t[-P productid]	# Set Product id on device\n");
	fprintf(stderr, "\t\t[-R release]	# Set Release. 2 dot separated decimals\n");
	fprintf(stderr, "\t\t[-L label]		# Set label.\n");
#endif
	exit(1);
}

static void parse_report_func(int level, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if(level <= verbose)
		vfprintf(stderr, msg, ap);
	va_end(ap);
}

#ifdef	XORCOM_INTERNAL
static void eeprom_fill(struct myeeprom *myeeprom,
	const char vendor[],
	const char product[],
	const char release[],
	const char label[],
	const char source[])
{
	// FF: address source is from device. C0: from eeprom
	if (source)
		myeeprom->source = strtoul(source, NULL, 0);
	else
		myeeprom->source = 0xC0;
	if(vendor)
		myeeprom->vendor = strtoul(vendor, NULL, 0);
	if(product)
		myeeprom->product = strtoul(product, NULL, 0);
	if(release) {
		int	release_major = 0;
		int	release_minor = 0;

		sscanf(release, "%d.%d", &release_major, &release_minor);
		myeeprom->release_major = release_major;
		myeeprom->release_minor = release_minor;
	}
	if(label) {
		/* padding */
		memset(myeeprom->label, 0, LABEL_SIZE);
		memcpy(myeeprom->label, label, strlen(label));
	}
}
#endif

int main(int argc, char *argv[])
{
	const struct astribank_type	*abtype;
	struct my_usb_device	mydev;
	const char		*devpath = NULL;
	const char		*binfile = NULL;
	const char		*inhexfile = NULL;
	const char		*outhexfile = NULL;
	struct hexdata		*hexdata = NULL;
	int			opt_reset = 0;
	int			opt_info = 0;
	int			opt_read_eeprom = 0;
	int			opt_output_width = 0;
	int			output_is_set = 0;
#ifdef	XORCOM_INTERNAL
	int			opt_write_eeprom = 0;
	char			*vendor = NULL;
	char			*source = NULL;
	char			*product = NULL;
	char			*release = NULL;
	char			*label = NULL;
	const char		options[] = "rib:D:ghH:I:vw:C:V:P:R:S:";
#else
	const char		options[] = "rib:D:ghH:I:vw:";
#endif
	int			ret = 0;

	progname = argv[0];
	assert(sizeof(struct fpga_packet_header) <= PACKET_SIZE);
	assert(sizeof(struct myeeprom) == EEPROM_SIZE);
	while (1) {
		int	c;

		c = getopt (argc, argv, options);
		if (c == -1)
			break;

		switch (c) {
			case 'D':
				devpath = optarg;
				if(output_is_set++) {
					ERR("Cannot set -D. Another output option is already selected\n");
					return 1;
				}
				break;
			case 'r':
				opt_reset = 1;
				break;
			case 'i':
				opt_info = 1;
				break;
			case 'b':
				binfile = optarg;
				if(output_is_set++) {
					ERR("Cannot set -b. Another output option is already selected\n");
					return 1;
				}
				break;
			case 'g':
				opt_read_eeprom = 1;
				break;
			case 'H':
				outhexfile = optarg;
				if(output_is_set++) {
					ERR("Cannot set -H. Another output option is already selected\n");
					return 1;
				}
				break;
			case 'I':
				inhexfile = optarg;
				break;
#ifdef	XORCOM_INTERNAL
			case 'V':
				vendor = optarg;
				break;
			case 'C':
				source = optarg;
				break;
			case 'P':
				product = optarg;
				break;
			case 'R':
				release = optarg;
				break;
			case 'S':
				label = optarg;
				{
					const char	GOOD_CHARS[] =
						"abcdefghijklmnopqrstuvwxyz"
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						"0123456789"
						"-_.";
					int	len = strlen(label);
					int	goodlen = strspn(label, GOOD_CHARS);

					if(len > LABEL_SIZE) {
						ERR("Label too long (%d > %d)\n", len, LABEL_SIZE);
						usage();
					}
					if(goodlen != len) {
						ERR("Bad character in label number (pos=%d)\n", goodlen);
						usage();
					}
				}
				break;
#endif
			case 'w':
				opt_output_width = strtoul(optarg, NULL, 0);
				break;
			case 'v':
				verbose++;
				break;
			case 'h':
			default:
				ERR("Unknown option '%c'\n", c);
				usage();
		}
	}

	if (optind != argc) {
		usage();
	}
	if(inhexfile) {
#ifdef	XORCOM_INTERNAL
		if(vendor || product || release || label || source ) {
			ERR("The -I option is exclusive of -[VPRSC]\n");
			return 1;
		}
#endif
		parse_hexfile_set_reporting(parse_report_func);
		hexdata = parse_hexfile(inhexfile, MAX_HEX_LINES);
		if(!hexdata) {
			ERR("Bailing out\n");
			exit(1);
		}
		if(opt_info) {
			printf("%s: Version=%s Checksum=%d\n",
					inhexfile, hexdata->version_info,
					bsd_checksum(hexdata));
		}
		if(binfile) {
			dump_binary(hexdata, binfile);
			return 0;
		}
		if(outhexfile) {
			if(opt_output_width)
				dump_hexfile2(hexdata, outhexfile, opt_output_width);
			else
				dump_hexfile(hexdata, outhexfile);
			return 0;
		}
	}
#ifdef	XORCOM_INTERNAL
	else if(vendor || product || release || label || source ) {
		if(outhexfile) {
			FILE	*fp;

			if(strcmp(outhexfile, "-") == 0)
				fp = stdout;
			else if((fp = fopen(outhexfile, "w")) == NULL) {
				perror(outhexfile);
				return 1;
			}
			memset(&mydev.eeprom, 0, sizeof(struct myeeprom));
			eeprom_fill(&mydev.eeprom, vendor, product, release, label, source);
			gen_hexline((uint8_t *)&mydev.eeprom, 0, sizeof(mydev.eeprom), fp);
			gen_hexline(NULL, 0, 0, fp);	/* EOF */
			return 0;
		}
	}
#endif
	if(!devpath) {
		ERR("Missing device path\n");
		usage();
	}
	DBG("Startup %s\n", devpath);
	if((abtype = my_usb_device_identify(devpath, &mydev)) == NULL) {
		ERR("Bad device. Does not match our types.\n");
		usage();
	}
	INFO("FIRMWARE: %s (type=%d)\n", abtype->name, abtype->type_code);
	if(!my_usb_device_init(devpath, &mydev, abtype)) {
		ERR("Failed to initialize USB device '%s'\n", devpath);
		ret = -ENODEV;
		goto dev_err;
	}
	ret = eeprom_get(&mydev);
	if(ret < 0) {
		ERR("Failed reading eeprom\n");
		goto dev_err;
	}
#ifdef	XORCOM_INTERNAL
	if(vendor || product || release || label || source ) {
		eeprom_fill(&mydev.eeprom, vendor, product, release, label, source);
		opt_write_eeprom = 1;
		opt_read_eeprom = 1;
	}
#endif
	if(opt_read_eeprom) {
		show_device_info(&mydev);
	}
	if(hexdata) {
		if (!mydev.is_usb2)
			INFO("Warning: working on a low end USB1 backend\n");
		if(!fpga_load(&mydev, hexdata)) {
			ERR("FPGA loading failed\n");
			ret = -ENODEV;
			goto dev_err;
		}
		ret = renumerate_device(&mydev, PT_RENUMERATE);
		if(ret < 0) {
			ERR("Renumeration failed: errno=%d\n", ret);
			goto dev_err;
		}
	}
#ifdef XORCOM_INTERNAL
	else if(opt_write_eeprom) {
		if(abtype->type_code == USB_FIRMWARE_II) {
			ERR("No EEPROM burning command in %s. Use fxload for that\n",
				abtype->name);
			goto dev_err;
		}
		ret = eeprom_set(&mydev, &mydev.eeprom);
		if(ret < 0) {
			ERR("Failed writing eeprom: %s\n", strerror(-ret));
			goto dev_err;
		}
		printf("------- RESULTS -------\n");
		show_device_info(&mydev);
	}
#endif
	if(opt_reset) {
		DBG("Reseting to default\n");
		ret = renumerate_device(&mydev, PT_RESET);
		if(ret < 0) {
			ERR("Renumeration to default failed: errno=%d\n", ret);
			goto dev_err;
		}
	}
	DBG("Exiting\n");
dev_err:
	my_usb_device_cleanup(&mydev);
	return ret;
}
