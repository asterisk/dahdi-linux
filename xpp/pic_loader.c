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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>
#include "hexfile.h"
#include "pic_loader.h"
#include <debug.h>
#include <xusb.h>

#define	DBG_MASK	0x03
#define	MAX_HEX_LINES	10000
#define	TIMEOUT		500

enum xpp_packet_types {
	PIC_REQ_XOP 	= 0x09,
	PIC_REP_XOP 	= 0x0A
};

struct xpp_packet_header {
	struct {
		uint16_t	len;
		uint8_t		op;
		uint8_t		unit;
	} PACKED header;
	union {
		struct {
			struct {
				uint8_t		flags;
				uint8_t		card_type;
				uint16_t	offs;
			} pic_header;
			uint8_t		data[3];
		} PACKED pic_packet;
	} d;
} PACKED;

int send_picline(struct astribank_device *astribank, uint8_t card_type, enum pic_command pcmd, int offs, uint8_t *data, int data_len)
{
	int				recv_answer = 0;
	char				buf[PACKET_SIZE];
	struct xpp_packet_header	*phead = (struct xpp_packet_header *)buf;
	int				pack_len;
	int				ret;

	assert(astribank != NULL);
	pack_len = data_len + sizeof(phead->header) + sizeof(phead->d.pic_packet.pic_header);
	phead->header.len 		= pack_len;
	phead->header.op 		= PIC_REQ_XOP;
	phead->header.unit 		= 0x00;
	phead->d.pic_packet.pic_header.flags = pcmd; 
	phead->d.pic_packet.pic_header.card_type = card_type;
	phead->d.pic_packet.pic_header.offs = offs;
	if(data)
		memcpy(phead->d.pic_packet.data, data, data_len);
	switch (pcmd) {
		case PIC_START_FLAG:
			break;
		case PIC_DATA_FLAG:
			break;
		case PIC_END_FLAG:
			recv_answer = 1;
			break;
		case PIC_ENDS_FLAG:
			break;
	}

	DBG("PICLINE: pack_len=%d pcmd=%d\n", pack_len, pcmd);
	dump_packet(LOG_DEBUG, DBG_MASK, "dump:picline[W]", (char *)phead, pack_len);

	ret = xusb_send(astribank->xusb, buf, pack_len, TIMEOUT);
	if(ret < 0) {
		ERR("xusb_send failed: %d\n", ret);
		return ret;
	}
	DBG("xusb_send: Written %d bytes\n", ret);
	if (recv_answer) {
		ret = xusb_recv(astribank->xusb, buf, sizeof(buf), TIMEOUT);
		if(ret <= 0) {
			ERR("No USB packs to read\n");
			return ret;
		} else {
			phead = (struct xpp_packet_header *)buf;
			if(phead->header.op != PIC_REP_XOP) {
				ERR("Got unexpected reply OP=0x%02X\n", phead->header.op);
				dump_packet(LOG_ERR, DBG_MASK, "hexline[ERR]", buf, ret);
				return -EINVAL;
			}
			DBG("received OP=0x%02X, checksum=%02X\n", phead->header.op, phead->d.pic_packet.data[0]);
			if(phead->d.pic_packet.data[0] != 0) {
				ERR("PIC burning, bad checksum\n");
				return -EINVAL;
			}
		}
	}
	return 0;
}

static const char *pic_basename(const char *fname, uint8_t *card_type)
{
	const char	*basename;
	regex_t		regex;
	char		ebuf[BUFSIZ];
	const char	re[] = "PIC_TYPE_([0-9]+)\\.hex";
	regmatch_t	pmatch[2];	/* One for the whole match, one for the number */
	int		nmatch = (sizeof(pmatch)/sizeof(pmatch[0]));
	int		len;
	int		ret;

	basename = strrchr(fname, '/');
	if(!basename)
		basename = fname;
	if((ret = regcomp(&regex, re, REG_ICASE | REG_EXTENDED)) != 0) {
		regerror(ret, &regex, ebuf, sizeof(ebuf));
		ERR("regcomp: %s\n", ebuf);
		return NULL;
	}
	if((ret = regexec(&regex, basename, nmatch, pmatch, 0)) != 0) {
		regerror(ret, &regex, ebuf, sizeof(ebuf));
		ERR("regexec: %s\n", ebuf);
		regfree(&regex);
		return NULL;
	}
	/*
	 * Should have both complete match and a parentheses match
	 */
	if(pmatch[0].rm_so == -1 || pmatch[1].rm_so == -1) {
		ERR("pic_basename: Bad match: pmatch[0].rm_so=%d pmatch[1].rm_so=%d\n",
			pmatch[0].rm_so, pmatch[1].rm_so == -1);
		regfree(&regex);
		return NULL;
	}
	len = pmatch[1].rm_eo - pmatch[1].rm_so;
	if(len >= sizeof(ebuf) - 1)
		len = sizeof(ebuf) - 1;
	memcpy(ebuf, basename + pmatch[1].rm_so, len);
	ebuf[len] = '\0';
	DBG("match: %s\n", ebuf);
	ret = atoi(ebuf);
	if(ret <= 0 || ret > 9) {
		ERR("pic_basename: Bad type number %d\n", ret);
		regfree(&regex);
		return NULL;
	}
	*card_type = ret;
	regfree(&regex);
	return basename;
}

/*
 * Returns: true on success, false on failure
 */
static int pic_burn(struct astribank_device *astribank, const struct hexdata *hexdata)
{
	const char		*v = hexdata->version_info;
	const char		*basename;
	uint8_t			*data;
	unsigned char		check_sum = 0;
	uint8_t			card_type;
	int			ret;
	unsigned int		i;
	const char		*devstr;

	v = (v[0]) ? v : "Unknown";
	assert(astribank != NULL);
	assert(hexdata != NULL);
	devstr = xusb_devpath(astribank->xusb);
	if(!astribank->is_usb2) {
		ERR("%s: Skip PIC burning (not USB2)\n", devstr);
		return 0;
	}
	INFO("%s [%s]: Loading PIC Firmware: %s (version %s)\n",
		devstr,
		xusb_serial(astribank->xusb),
		hexdata->fname,
		hexdata->version_info);
	basename = pic_basename(hexdata->fname, &card_type);
	if(!basename) {
		ERR("%s: Bad PIC filename '%s'. Abort.\n", devstr, hexdata->fname);
		return 0;
	}
	DBG("basename=%s card_type=%d maxlines=%d\n",
		basename, card_type, hexdata->maxlines);
	/*
	 * Try to read extra left-overs from USB controller
	 */
	for(i = 2; i; i--) {
		char    buf[PACKET_SIZE];

		if(xusb_recv(astribank->xusb, buf, sizeof(buf), 1) <= 0)
			break;
	}
	if((ret = send_picline(astribank, card_type, PIC_START_FLAG, 0, NULL, 0)) != 0) {
		perror("Failed sending start hexline");
		return 0;
	}
	for(i = 0; i < hexdata->maxlines; i++) { 
		struct hexline	*hexline;
		unsigned int	len;

		hexline = hexdata->lines[i];
		if(!hexline) {
			ERR("%s: hexdata finished early (line %d)", devstr, i);
			return 0;
		}
		if(hexline->d.content.header.tt == TT_DATA) {
			len = hexline->d.content.header.ll;	/* don't send checksum */
			if(len != 3) {
				ERR("%s: Bad line len %d\n", devstr, len);
				return 0;
			}
			data = hexline->d.content.tt_data.data;
			check_sum ^= data[0] ^ data[1] ^ data[2];
			ret = send_picline(astribank, card_type, PIC_DATA_FLAG,
					hexline->d.content.header.offset, data, len);
			if(ret) {
				perror("Failed sending data hexline");
				return 0;
			}
		} else if(hexline->d.content.header.tt == TT_EOF) {
			break;
		} else {
			ERR("%s: Unexpected TT = %d in line %d\n",
					devstr, hexline->d.content.header.tt, i);
			return 0;
		}
	}
	if((ret = send_picline(astribank, card_type, PIC_END_FLAG, 0, &check_sum, 1)) != 0) {
		perror("Failed sending end hexline");
		return 0;
	}
	DBG("Finished...\n");
	return 1;
}

int load_pic(struct astribank_device *astribank, int numfiles, char *filelist[])
{
	int		i;
	const char	*devstr;

	devstr = xusb_devpath(astribank->xusb);
	DBG("%s: Loading %d PIC files...\n", devstr, numfiles);
	for(i = 0; i < numfiles; i++) {
		struct hexdata	*picdata;
		const char	*curr = filelist[i];

		DBG("%s\n", curr);
		if((picdata = parse_hexfile(curr, MAX_HEX_LINES)) == NULL) {
			perror(curr);
			return -errno;
		}
		if(!pic_burn(astribank, picdata)) {
			ERR("%s: PIC %s burning failed\n", devstr, curr);
			return -ENODEV;
		}
		free_hexdata(picdata);
	}
	if((i = send_picline(astribank, 0, PIC_ENDS_FLAG, 0, NULL, 0)) != 0) {
		ERR("%s: PIC end burning failed\n", devstr);
		return -ENODEV;
	}
	return 0;
}
