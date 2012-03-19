/*
 * Written by Oron Peled <oron@actcom.co.il> and
 *            Alex Landau <alex.landau@xorcom.com>
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
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "mpp.h"
#include "mpptalk.h"
#include <debug.h>
#include "astribank_license.h"

static const char rcsid[] = "$Id$";

#define	DBG_MASK	0x80

static char	*progname;

static void usage()
{
	fprintf(stderr, "Usage: %s [options...] -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev> options\n", progname);
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\t\t[-v]               # Increase verbosity\n");
	fprintf(stderr, "\t\t[-d mask]          # Debug mask (0xFF for everything)\n");
	fprintf(stderr, "\t\t[-w]               # Write capabilities to EEPROM, otherwise read capabilities\n");
	fprintf(stderr, "\t\t[-f filename]      # License filename (stdin/stdout if not specified)\n\n");
	fprintf(stderr, "\t\t[-m num]           # Numeric code of License markers to generate\n");
	license_markers_help("\t", stderr);
	exit(1);
}

static int capabilities_burn(
		struct astribank_device *astribank,
		struct eeprom_table *eeprom_table,
		struct capabilities *capabilities,
		struct capkey *key)
{
	int	ret;

	INFO("Burning capabilities\n");
	ret = mpp_caps_set(astribank, eeprom_table, capabilities, key);
	if(ret < 0) {
		ERR("Capabilities burning failed: %d\n", ret);
		return ret;
	}
	INFO("Done\n");
	return 0;
}

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	struct astribank_device *astribank;
	struct eeprom_table	eeprom_table;
	struct capabilities	caps;
	struct capkey		key;
	const char		options[] = "vd:D:wf:m:";
	int			do_write = 0;
	unsigned int		marker = LICENSE_MARKER_GENERIC;
	FILE			*file;
	char			*filename = NULL;
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
			case 'v':
				verbose++;
				break;
			case 'd':
				debug_mask = strtoul(optarg, NULL, 0);
				break;
			case 'w':
				do_write = 1;
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				marker = strtoul(optarg, NULL, 0);
				if (!license_marker_valid(marker))
					usage();
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
		return 1;
	}
	if(astribank->eeprom_type != EEPROM_TYPE_LARGE) {
		ERR("Cannot use this program with astribank EEPROM type %d (need %d)\n",
			astribank->eeprom_type, EEPROM_TYPE_LARGE);
		return 1;
	}
	ret = mpp_caps_get(astribank, &eeprom_table, &caps, &key);
	if(ret < 0) {
		ERR("Failed to get original capabilities: %d\n", ret);
		return 1;
	}
	if (do_write) {
		unsigned int used_marker;
		/* update capabilities based on input file */
		file = stdin;
		if (filename) {
			file = fopen(filename, "r");
			if (file == NULL) {
				ERR("Can't open file '%s'\n", filename);
				return 1;
			}
		}
		ret = read_from_file(&eeprom_table, &caps, &key, &used_marker, file);
		if (ret < 0) {
			ERR("Failed to read capabilities from file: %d\n", ret);
			return 1;
		}
		show_capabilities(&caps, stderr);
		if (capabilities_burn(astribank, &eeprom_table, &caps, &key) < 0)
			return 1;
		if (file != stdin)
			fclose(file);
	} else {
		/* print capabilities to stdout */
		file = stdout;
		if (filename) {
			file = fopen(filename, "w");
			if (file == NULL) {
				ERR("Can't create file '%s'\n", filename);
				return 1;
			}
		}
		ret = write_to_file(&eeprom_table, &caps, &key, marker, file);
		if (ret < 0) {
			ERR("Failed to write capabilities to file: %d\n", ret);
			return 1;
		}
		if (file != stdout)
			fclose(file);
	}
	mpp_exit(astribank);
	return 0;
}
