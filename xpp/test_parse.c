/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2006, 2007, 2008, 2009 Xorcom
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
#include <stdarg.h>
#include "hexfile.h"

static void default_report_func(int level, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
}

int main(int argc, char *argv[])
{
	struct hexdata	*hd;
	int		i;

	if(argc < 2) {
		fprintf(stderr, "Usage: program hexfile...\n");
		return 1;
	}
	parse_hexfile_set_reporting(default_report_func);
	for(i = 1; i < argc; i++) {
		hd = parse_hexfile(argv[i], 2000);
		if(!hd) {
			fprintf(stderr, "Parsing failed\n");
			return 1;
		}
		fprintf(stderr, "=== %s === (version: %s)\n", argv[i], hd->version_info);
		dump_hexfile2(hd, "-", 60 );
		free_hexdata(hd);
	}
	return 0;
}
