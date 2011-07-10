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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "astribank_usb.h"
#include "mpptalk.h"
#include <debug.h>
#include <xusb.h>

#define	DBG_MASK	0x80
/* if enabled, adds support for resetting pre-MPP USB firmware - if we 
 * failed opening a device and we were asked to reset it, try also the
 * old protocol.
 */
#define SUPPORT_OLD_RESET

static char	*progname;

static void usage()
{
	fprintf(stderr, "Usage: %s [options] -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev> [operation...]\n", progname);
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\t\t[-v]               # Increase verbosity\n");
	fprintf(stderr, "\t\t[-d mask]          # Debug mask (0xFF for everything)\n");
	fprintf(stderr, "\tOperations:\n");
	fprintf(stderr, "\t\t[-n]               # Renumerate device\n");
	fprintf(stderr, "\t\t[-r kind]          # Reset: kind = {half|full}\n");
	fprintf(stderr, "\t\t[-p port]          # TwinStar: USB port number [0, 1]\n");
	fprintf(stderr, "\t\t[-w (0|1)]         # TwinStar: Watchdog off or on guard\n");
	fprintf(stderr, "\t\t[-Q]               # Query device properties\n");
	exit(1);
}

static int reset_kind(const char *arg)
{
	static const struct {
		const char	*name;
		int		type_code;
	} reset_kinds[] = {
		{ "half",	0 },
		{ "full",	1 },
	};
	int	i;

	for(i = 0; i < sizeof(reset_kinds)/sizeof(reset_kinds[0]); i++) {
		if(strcasecmp(reset_kinds[i].name, arg) == 0)
			return reset_kinds[i].type_code;
	}
	ERR("Uknown reset kind '%s'\n", arg);
	return -1;
}


static int show_hardware(struct astribank_device *astribank)
{
	uint8_t	unit;
	uint8_t	card_status;
	uint8_t	card_type;
	int	ret;
	struct eeprom_table	eeprom_table;
	struct capabilities	capabilities;
	struct extrainfo	extrainfo;

	ret = mpp_caps_get(astribank, &eeprom_table, &capabilities, NULL);
	if(ret < 0)
		return ret;
	show_eeprom(&eeprom_table, stdout);
	show_astribank_status(astribank, stdout);
	if(astribank->eeprom_type == EEPROM_TYPE_LARGE) {
		show_capabilities(&capabilities, stdout);
		if(STATUS_FPGA_LOADED(astribank->status)) {
			for(unit = 0; unit < 5; unit++) {
				ret = mpps_card_info(astribank, unit, &card_type, &card_status);
				if(ret < 0)
					return ret;
				printf("CARD %d: type=%x.%x %s\n", unit,
						((card_type >> 4) & 0xF), (card_type & 0xF),
						((card_status & 0x1) ? "PIC" : "NOPIC"));
			}
		}
		ret = mpp_extrainfo_get(astribank, &extrainfo);
		if(ret < 0)
			return ret;
		show_extrainfo(&extrainfo, stdout);
		if(CAP_EXTRA_TWINSTAR(&capabilities)) {
			twinstar_show(astribank, stdout);
		}
	}
	return 0;
}

#ifdef SUPPORT_OLD_RESET
/* Try to reset a device using USB_FW.hex, up to Xorcom rev. 6885 */
int old_reset(const char* devpath)
{
	struct astribank_device *astribank;
	int ret;
	struct {
		uint8_t		op;
	} PACKED header = {0x20}; /* PT_RESET */
	char *buf = (char*) &header;

	/* Note that the function re-opens the connection to the Astribank
	 * as any reference to the previous connection was lost when mpp_open
	 * returned NULL as the astribank reference. */
	astribank = astribank_open(devpath, 1);
	if (!astribank) {
		DBG("Failed re-opening astribank device for old_reset\n");
		return -ENODEV;
	}
	ret = xusb_send(astribank->xusb, buf, 1, 5000);

	/* If we just had a reenumeration, we may get -ENODEV */
	if(ret < 0 && ret != -ENODEV)
			return ret;
	/* We don't astribank_close(), as it has likely been
	 * reenumerated by now. */
	return 0;
}	
#endif /* SUPPORT_OLD_RESET */

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	struct astribank_device *astribank;
	const char		options[] = "vd:D:nr:p:w:Q";
	int			opt_renumerate = 0;
	char			*opt_port = NULL;
	char			*opt_watchdog = NULL;
	char			*opt_reset = NULL;
	int			opt_query = 0;
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
			case 'n':
				opt_renumerate++;
				break;
			case 'p':
				opt_port = optarg;
				break;
			case 'w':
				opt_watchdog = optarg;
				break;
			case 'r':
				opt_reset = optarg;
				/*
				 * Sanity check so we can reject bad
				 * arguments before device access.
				 */
				if(reset_kind(opt_reset) < 0)
					usage();
				break;
			case 'Q':
				opt_query = 1;
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
	if(!devpath) {
		ERR("Missing device path\n");
		usage();
	}
	DBG("Startup %s\n", devpath);
	if((astribank = mpp_init(devpath, 1)) == NULL) {
		ERR("Failed initializing MPP\n");
#ifdef SUPPORT_OLD_RESET
		DBG("opt_reset = %s\n", opt_reset);
		if (opt_reset) {
			DBG("Trying old reset method\n");
			if ((ret = old_reset(devpath)) != 0) {
				ERR("Old reset method failed as well: %d\n", ret);
			}
		}
#endif /* SUPPORT_OLD_RESET */

		return 1;
	}
	/*
	 * First process reset options. We want to be able
	 * to reset minimal USB firmwares even if they don't
	 * implement the full MPP protocol (e.g: EEPROM_BURN)
	 */
	if(opt_reset) {
		int	full_reset;

		if((full_reset = reset_kind(opt_reset)) < 0) {
			ERR("Bad reset kind '%s'\n", opt_reset);
			return 1;
		}
		DBG("Reseting (%s)\n", opt_reset);
		if((ret = mpp_reset(astribank, full_reset)) < 0) {
			ERR("%s Reseting astribank failed: %d\n",
				(full_reset) ? "Full" : "Half", ret);
		}
		goto out;
	}
	show_astribank_info(astribank);
	if(opt_query) {
		show_hardware(astribank);
	} else if(opt_renumerate) {
		DBG("Renumerate\n");
		if((ret = mpp_renumerate(astribank)) < 0) {
			ERR("Renumerating astribank failed: %d\n", ret);
		}
	} else if(opt_watchdog) {
		int	watchdogstate = strtoul(opt_watchdog, NULL, 0);

		DBG("TWINSTAR: Setting watchdog %s-guard\n",
			(watchdogstate) ? "on" : "off");
		if((ret = mpp_tws_setwatchdog(astribank, watchdogstate)) < 0) {
			ERR("Failed to set watchdog to %d\n", watchdogstate);
			return 1;
		}
	} else if(opt_port) {
		int	new_portnum = strtoul(opt_port, NULL, 0);
		int	tws_portnum = mpp_tws_portnum(astribank);
		char	*msg = (new_portnum == tws_portnum)
					? " Same same, never mind..."
					: "";

		DBG("TWINSTAR: Setting portnum to %d.%s\n", new_portnum, msg);
		if((ret = mpp_tws_setportnum(astribank, new_portnum)) < 0) {
			ERR("Failed to set USB portnum to %d\n", new_portnum);
			return 1;
		}
	}
out:
	mpp_exit(astribank);
	return 0;
}
