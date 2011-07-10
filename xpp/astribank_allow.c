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

static int bin_to_file(void *buf, int len, FILE *f)
{
	static int bytes_on_line;
	unsigned char *p = buf;
	if (buf == NULL) {
		if (bytes_on_line != 0) {
			if (fprintf(f, "\n") != 1)
				return -1;
			bytes_on_line = 0;
		}
		return 0;
	}
	int i;
	for (i = 0; i < len; i++) {
		if (fprintf(f, "%02x", *p++) != 2)
			return -1;
		bytes_on_line++;
		if (bytes_on_line >= 16) {
			if (fprintf(f, "\n") != 1)
				return -1;
			bytes_on_line = 0;
		}
	}
	return 0;
}

static int write_to_file(struct eeprom_table *eeprom_table, struct capabilities *caps, struct capkey *key, FILE *f)
{
	fprintf(f, "-----BEGIN XORCOM LICENSE BLOCK-----\n");
	fprintf(f, "Version: 1.0\n");
	fprintf(f, "Timestamp: %u\n", caps->timestamp);
	fprintf(f, "Serial: %.*s\n", LABEL_SIZE, eeprom_table->label);
	fprintf(f, "Capabilities.Port.FXS: %d\n", caps->ports_fxs);
	fprintf(f, "Capabilities.Port.FXO: %d\n", caps->ports_fxo);
	fprintf(f, "Capabilities.Port.BRI: %d\n", caps->ports_bri);
	fprintf(f, "Capabilities.Port.PRI: %d\n", caps->ports_pri);
	fprintf(f, "Capabilities.Port.ECHO: %d\n", caps->ports_echo);
	fprintf(f, "Capabilities.Twinstar: %d\n", CAP_EXTRA_TWINSTAR(caps));
	fprintf(f, "Data:\n");
	bin_to_file(eeprom_table, sizeof(*eeprom_table), f);
	bin_to_file(caps, sizeof(*caps), f);
	bin_to_file(key, sizeof(*key), f);
	bin_to_file(NULL, 0, f);
	fprintf(f, "-----END XORCOM LICENSE BLOCK-----\n");
	return 0;
}

/*
 * Removes whitespace on both sizes of the string.
 * Returns a pointer to the first non-space char. The string
 * is modified in place to trim trailing whitespace.
 * If the whole string is whitespace, returns NULL.
 */
char *trim(char *s)
{
	int len = strlen(s);
	while (len > 0 && isspace(s[len-1])) {
		len--;
	}
	if (len == 0)
		return NULL;
	s[len] = '\0';
	while (isspace(*s))
		s++;
	/* *s is not a space, since in this case we'd return NULL above */
	return s;
}

int get_key_value(char *line, char **key, char **value)
{
	char *p = strchr(line, ':');
	if (p == NULL)
		return -1;
	*p = '\0';
	*key = trim(line);
	*value = trim(p + 1);
	return 0;
}

static int hex_digit_to_int(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

static int str_to_bin(char *line, void *buf, int maxlen)
{
	static int offset;
	unsigned char *p = buf;
	if (strlen(line) % 2 != 0)
		return -1;
	while (offset < maxlen && *line) {
		uint8_t value;
		char c = hex_digit_to_int(*line++);
		if (c < 0 || *line == '\0')
			return -1;
		value = c << 4;
		c = hex_digit_to_int(*line++);
		if (c < 0)
			return -1;
		value |= c;
		p[offset++] = value;
	}
	if (offset == maxlen && *line)
		return -1;
	return offset;
}

static int read_from_file(struct eeprom_table *eeprom_table, struct capabilities *caps, struct capkey *capkey, FILE *f)
{
	char buf[256];
	char *line, *key, *value;
	int state = 0;
	int lineno = 0;
	struct table {
		struct eeprom_table eeprom_table;
		struct capabilities capabilities;
		struct capkey capkey;
	} PACKED table;

	memset(&table, 0, sizeof(struct table));
	/*
	 * states:
	 * 0: start - before BEGIN_LICENSE_BLOCK line. on BEGIN_LICENSE_BLOCK line goto 1.
	 * 1: read Version, goto 2. if not version line then error.
	 * 2: after BEGIN line. split line into key:value. if line is Data:, goto 3.
	 * 3: read binary data. if line is END_LICENSE_BLOCK goto 4.
	 * 4: END_LICENSE_BLOCK - ignore lines.
	 */
	while (fgets(buf, 256, f) != NULL) {
		lineno++;
		int len = strlen(buf);
		if (len > 0 && buf[len-1] != '\n') {
			ERR("Line %d: Line too long\n", lineno);
			return -1;
		}
		line = trim(buf);
		if (line == NULL) {
 			if (state > 0 && state < 4) {
				ERR("Line %d: Empty line\n", lineno);
				return -1;
			}
			else
				continue;
		}
		switch (state) {
			case 0:
				if (strcmp(line, "-----BEGIN XORCOM LICENSE BLOCK-----") == 0)
					state = 1;
				else {
					ERR("Line %d: Invalid license begin block\n", lineno);
					return -1;
				}
				break;
			case 1:
				if (get_key_value(line, &key, &value) < 0) {
					ERR("Line %d: Can't parse line\n", lineno);
					return -1;
				}
				if (strcmp(key, "Version") == 0) {
					if (strcmp(value, "1.0") == 0) {
						state = 2;
					} else {
						ERR("Line %d: Unknown license file version '%s', need version '1.0'\n", lineno, value);
						return -1;
					}
				} else {
					ERR("Line %d: No license file version\n", lineno);
					return -1;
				}
				break;
			case 2:
				if (get_key_value(line, &key, &value) < 0) {
					ERR("Line %d: Can't parse line\n", lineno);
					return -1;
				}
				if (strcmp(key, "Data") == 0) {
					state = 3;
					break;
				}
				break;
			case 3:
				if (strcmp(line, "-----END XORCOM LICENSE BLOCK-----") == 0) {
					state = 4;
					break;
				}
				if (str_to_bin(line, &table, sizeof(table)) < 0) {
					ERR("Line %d: Error in data block\n", lineno);
					return -1;
				}
				break;
			case 4:
				break;

		}
	}
	if (state != 4) {
		ERR("Invalid license file\n");
		return -1;
	}
	memcpy(eeprom_table, &table.eeprom_table, sizeof(*eeprom_table));
	memcpy(caps, &table.capabilities, sizeof(*caps));
	memcpy(capkey, &table.capkey, sizeof(*capkey));
	return 0;
}

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	struct astribank_device *astribank;
	struct eeprom_table	eeprom_table;
	struct capabilities	caps;
	struct capkey		key;
	const char		options[] = "vd:D:wf:";
	int			do_write = 0;
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
		/* update capabilities based on input file */
		file = stdin;
		if (filename) {
			file = fopen(filename, "r");
			if (file == NULL) {
				ERR("Can't open file '%s'\n", filename);
				return 1;
			}
		}
		ret = read_from_file(&eeprom_table, &caps, &key, file);
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
		ret = write_to_file(&eeprom_table, &caps, &key, file);
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
