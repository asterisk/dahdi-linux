/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2012, Xorcom
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

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <debug.h>
#include "astribank_license.h"

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof((a)[0]))

static const struct boundary {
	const char *name;
	const char *markers[2];
} boundaries[] = {
	[LICENSE_MARKER_NONE] = {	/* Skip 0 */
	},
	[LICENSE_MARKER_XORCOM] = {
		"Xorcom",
		{
			"-----BEGIN XORCOM LICENSE BLOCK-----",
			"-----END XORCOM LICENSE BLOCK-----",
		},
	},
	[LICENSE_MARKER_GENERIC] = {
		"Generic",
		{
			"-----BEGIN TELEPHONY DEVICE LICENSE BLOCK-----",
			"-----END TELEPHONY DEVICE LICENSE BLOCK-----",
		}
	},
};

void license_markers_help(const char *prefix, FILE *fp)
{
	int i;

	fprintf(fp, "%sValid license markers:\n", prefix);
	for (i = LICENSE_MARKER_NONE + 1; i < ARRAY_SIZE(boundaries); i++) {
		const struct boundary *b = &boundaries[i];
		if (b->markers[0] != 0)
			fprintf(fp, "%s\t%d - %s\n", prefix, i, b->name);
	}
}

int license_marker_valid(unsigned int which)
{
	if (which >= ARRAY_SIZE(boundaries))
		return 0;
	if (boundaries[which].markers[0] == NULL)
		return 0;
	return 1;
}

static const char *marker_string(unsigned int which, int end_marker)
{
	int selector = (end_marker) ? 1 : 0;

	if (license_marker_valid(which)) {
		return boundaries[which].markers[selector];
	}
	ERR("gen_marker: invalid marker %d\n", which);
	return NULL;
}

static int marker_find(const char *str, int end_marker)
{
	int selector = (end_marker) ? 1 : 0;
	int i;

	for (i = LICENSE_MARKER_NONE + 1; i < ARRAY_SIZE(boundaries); i++) {
		const struct boundary *b = &boundaries[i];
		const char *marker_str = b->markers[selector];

#if 0
		DBG("marker_find(%s,%d)[%d]: %s\n",
			str, end_marker, i, marker_str);
#endif
		if (!marker_str)
			continue;
		if (strcmp(str, marker_str) == 0)
			return i;
	}
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

int write_to_file(
	struct eeprom_table *eeprom_table,
	struct capabilities *caps,
	struct capkey *key,
	unsigned int marker,
	FILE *f)
{
	fprintf(f, "%s\n", marker_string(marker, 0));
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
	fprintf(f, "%s\n", marker_string(marker, 1));
	return 0;
}

/*
 * Removes whitespace on both sizes of the string.
 * Returns a pointer to the first non-space char. The string
 * is modified in place to trim trailing whitespace.
 * If the whole string is whitespace, returns NULL.
 */
static char *trim(char *s)
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

static int get_key_value(char *line, char **key, char **value)
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

int read_from_file(
	struct eeprom_table *eeprom_table,
	struct capabilities *caps,
	struct capkey *capkey,
	unsigned int *used_marker,
	FILE *f)
{
	char buf[256];
	char *line, *key, *value;
	int lineno = 0;
	unsigned int license_marker_begin = 0;
	unsigned int license_marker_end;
	struct table {
		struct eeprom_table eeprom_table;
		struct capabilities capabilities;
		struct capkey capkey;
	} PACKED table;
	enum PARSE_STATES {
		STATE_BEFORE_LICENSE = 0,
		STATE_EXPECT_VERSION = 1,
		STATE_EXPECT_DATA = 2,
		STATE_READ_DATA = 3,
		STATE_AFTER_LICENSE = 4,
	} state = STATE_BEFORE_LICENSE;

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
			if (state > STATE_BEFORE_LICENSE && state < STATE_AFTER_LICENSE) {
				ERR("Line %d: Empty line\n", lineno);
				return -1;
			}
			else
				continue;
		}
		switch (state) {
			case STATE_BEFORE_LICENSE:
				license_marker_begin = marker_find(line, 0);
				if (license_marker_begin)
					state = STATE_EXPECT_VERSION;
				else {
					ERR("Line %d: Invalid license begin block\n", lineno);
					return -1;
				}
				break;
			case STATE_EXPECT_VERSION:
				if (get_key_value(line, &key, &value) < 0) {
					ERR("Line %d: Can't parse line\n", lineno);
					return -1;
				}
				if (strcmp(key, "Version") == 0) {
					if (strcmp(value, "1.0") == 0) {
						state = STATE_EXPECT_DATA;
					} else {
						ERR("Line %d: Unknown license file version '%s', need version '1.0'\n", lineno, value);
						return -1;
					}
				} else {
					ERR("Line %d: No license file version\n", lineno);
					return -1;
				}
				break;
			case STATE_EXPECT_DATA:
				if (get_key_value(line, &key, &value) < 0) {
					ERR("Line %d: Can't parse line\n", lineno);
					return -1;
				}
				if (strcmp(key, "Data") == 0) {
					state = STATE_READ_DATA;
					break;
				}
				break;
			case STATE_READ_DATA:
				license_marker_end = marker_find(line, 1);
				if (license_marker_end) {
					if (license_marker_end != license_marker_begin) {
						ERR("Line %d: End marker != Begin marker\n", lineno);
						return -1;
					}
					state = STATE_AFTER_LICENSE;
					break;
				}
				if (str_to_bin(line, &table, sizeof(table)) < 0) {
					ERR("Line %d: Error in data block\n", lineno);
					return -1;
				}
				break;
			case STATE_AFTER_LICENSE:
				break;

		}
	}
	if (state != STATE_AFTER_LICENSE) {
		ERR("Invalid license file\n");
		return -1;
	}
	memcpy(eeprom_table, &table.eeprom_table, sizeof(*eeprom_table));
	memcpy(caps, &table.capabilities, sizeof(*caps));
	memcpy(capkey, &table.capkey, sizeof(*capkey));
	return 0;
}

