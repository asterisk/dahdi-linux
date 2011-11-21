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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <arpa/inet.h>
#include <debug.h>
#include "hexfile.h"
#include "mpptalk.h"
#include "pic_loader.h"
#include "echo_loader.h"
#include "astribank_usb.h"
#include "../autoconfig.h"

#define	DBG_MASK	0x80
#define	MAX_HEX_LINES	64000
#define HAVE_OCTASIC	1

static char	*progname;

static void usage()
{
	fprintf(stderr, "Usage: %s [options...] -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev> hexfile...\n", progname);
	fprintf(stderr, "\tOptions: {-F|-p}\n");
	fprintf(stderr, "\t\t[-E]               # Burn to EEPROM\n");
#if HAVE_OCTASIC
	fprintf(stderr, "\t\t[-O]               # Load Octasic firmware\n");
	fprintf(stderr, "\t\t[-o]               # Show Octasic version\n");
#endif
	fprintf(stderr, "\t\t[-F]               # Load FPGA firmware\n");
	fprintf(stderr, "\t\t[-p]               # Load PIC firmware\n");
	fprintf(stderr, "\t\t[-v]               # Increase verbosity\n");
	fprintf(stderr, "\t\t[-A]               # Set A-Law for 1st module\n");
	fprintf(stderr, "\t\t[-d mask]          # Debug mask (0xFF for everything)\n");
	exit(1);
}

int handle_hexline(struct astribank_device *astribank, struct hexline *hexline)
{
	uint16_t	len;
	uint16_t	offset_dummy;
	uint8_t		*data;
	int		ret;

	assert(hexline);
	assert(astribank);
	if(hexline->d.content.header.tt != TT_DATA) {
		DBG("Non data record type = %d\n", hexline->d.content.header.tt);
		return 0;
	}
	len = hexline->d.content.header.ll;
	offset_dummy = hexline->d.content.header.offset;
	data = hexline->d.content.tt_data.data;
	if((ret = mpp_send_seg(astribank, data, offset_dummy, len)) < 0) {
		ERR("Failed hexfile send line: %d\n", ret);
		return -EINVAL;
	}
	return 0;
}

void print_parse_errors(int level, const char *msg, ...)
{
	va_list ap;

	if (verbose > level) {
		va_start (ap, msg);
		vfprintf (stderr, msg, ap);
		va_end (ap);
	}
}

static int load_hexfile(struct astribank_device *astribank, const char *hexfile, enum dev_dest dest)
{
	struct hexdata		*hexdata = NULL;
	int			finished = 0;
	int			ret;
	unsigned		i;
	char			star[] = "+\\+|+/+-";
	const char		*devstr;

	parse_hexfile_set_reporting(print_parse_errors);
	if((hexdata  = parse_hexfile(hexfile, MAX_HEX_LINES)) == NULL) {
		perror(hexfile);
		return -errno;
	}
	devstr = xusb_devpath(astribank->xusb);
	INFO("%s [%s]: Loading %s Firmware: %s (version %s)\n",
		devstr,
		xusb_serial(astribank->xusb),
		dev_dest2str(dest),
		hexdata->fname, hexdata->version_info);
	if((ret = mpp_send_start(astribank, dest, hexdata->version_info)) < 0) {
		ERR("%s: Failed hexfile send start: %d\n", devstr, ret);
		return ret;
	}
	for(i = 0; i < hexdata->maxlines; i++) {
		struct hexline	*hexline = hexdata->lines[i];

		if(!hexline)
			break;
		if(verbose > LOG_INFO) {
			printf("Sending: %4d%%    %c\r", (100 * i) / hexdata->last_line, star[i % sizeof(star)]);
			fflush(stdout);
		}
		if(finished) {
			ERR("%s: Extra data after End Of Data Record (line %d)\n", devstr, i);
			return 0;
		}
		if(hexline->d.content.header.tt == TT_EOF) {
			DBG("End of data\n");
			finished = 1;
			continue;
		}
		if((ret = handle_hexline(astribank, hexline)) < 0) {
			ERR("%s: Failed hexfile sending in lineno %d (ret=%d)\n", devstr, i, ret);;
			return ret;
		}
	}
	if(verbose > LOG_INFO) {
		putchar('\n');
		fflush(stdout);
	}
	if((ret = mpp_send_end(astribank)) < 0) {
		ERR("%s: Failed hexfile send end: %d\n", devstr, ret);
		return ret;
	}
#if 0
	fclose(fp);
#endif
	free_hexdata(hexdata);
	DBG("hexfile loaded successfully\n");
	return 0;
}

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	int			opt_pic = 0;
	int			opt_echo = 0;
	int			opt_ecver = 0;
#if HAVE_OCTASIC
	int			opt_alaw = 0;
#endif
	int			opt_dest = 0;
	int			opt_sum = 0;
	enum dev_dest		dest = DEST_NONE;
	const char		options[] = "vd:D:EFOopA";
	int			iface_num;
	int			ret;

	progname = argv[0];
	while (1) {
		int	c;

		c = getopt (argc, argv, options);
		if (c == -1)
			break;

		switch (c) {
			case 'D':
				devpath = optarg;
				break;
			case 'E':
				if(dest != DEST_NONE) {
					ERR("The -F and -E options are mutually exclusive.\n");
					usage();
				}
				opt_dest++;
				dest = DEST_EEPROM;
				break;
			case 'F':
				if(dest != DEST_NONE) {
					ERR("The -F and -E options are mutually exclusive.\n");
					usage();
				}
				opt_dest++;
				dest = DEST_FPGA;
				break;
#if HAVE_OCTASIC
			case 'O':
				opt_echo = 1;
				break;
			case 'o':
				opt_ecver = 1;
				break;
			case 'A':
				opt_alaw = 1;
				break;
#endif
			case 'p':
				opt_pic = 1;
				break;
			case 'v':
				verbose++;
				break;
			case 'd':
				debug_mask = strtoul(optarg, NULL, 0);
				break;
			case 'h':
			default:
				ERR("Unknown option '%c'\n", c);
				usage();
		}
	}
	opt_sum = opt_dest + opt_pic + opt_echo;
	if(opt_sum > 1 || (opt_sum == 0 && opt_ecver == 0)) {
		ERR("The -F, -E"
#if HAVE_OCTASIC
			", -O"
#endif
			" and -p options are mutually exclusive, if neither is used then -o should present\n");
		usage();
	}
	iface_num = (opt_dest) ? 1 : 0;
	if(!opt_pic && !opt_ecver) {
		if(optind != argc - 1) {
			ERR("Got %d hexfile names (Need exactly one hexfile)\n",
				argc - 1 - optind);
			usage();
		}
	}
	if(!devpath) {
		ERR("Missing device path.\n");
		usage();
	}
	if(opt_dest) {
		/*
		 * MPP Interface
		 */
		struct astribank_device *astribank;

		if((astribank = mpp_init(devpath, iface_num)) == NULL) {
			ERR("%s: Opening astribank failed\n", devpath);
			return 1;
		}
		//show_astribank_info(astribank);
		if(load_hexfile(astribank, argv[optind], dest) < 0) {
			ERR("%s: Loading firmware to %s failed\n", devpath, dev_dest2str(dest));
			return 1;
		}
		astribank_close(astribank, 0);
	} else if(opt_pic || opt_echo || opt_ecver) {
		/*
		 * XPP Interface
		 */
		struct astribank_device *astribank;

		if((astribank = astribank_open(devpath, iface_num)) == NULL) {
			ERR("%s: Opening astribank failed\n", devpath);
			return 1;
		}
		//show_astribank_info(astribank);
#if HAVE_OCTASIC
		if (opt_ecver) {
			if((ret = echo_ver(astribank)) < 0) {
				ERR("%s: Get Octasic version failed (Is Echo canceller card connected?)\n", devpath);
				return 1;
			} else 
				INFO("Octasic version: 0x%0X\n", ret);
		}
#endif
		if (opt_pic) {
			if ((ret = load_pic(astribank, argc - optind, argv + optind)) < 0) {
				ERR("%s: Loading PIC's failed\n", devpath);
				return 1;
			}
#if HAVE_OCTASIC
		} else if (opt_echo) {
			if((ret = load_echo(astribank, argv[optind], opt_alaw)) < 0) {
				ERR("%s: Loading ECHO's failed\n", devpath);
				return 1;
			}
#endif
		}
		astribank_close(astribank, 0);
	}
	return 0;
}
