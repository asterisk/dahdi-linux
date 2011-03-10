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
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <execinfo.h>
#include <debug.h>

int	verbose = LOG_INFO;
int	debug_mask = 0;

void log_function(int level, int mask, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if(verbose >= level) {
		if(level < LOG_DEBUG || (mask & debug_mask))
			vfprintf(stderr, msg, ap);
	}
	va_end(ap);
}

void dump_packet(int loglevel, int mask, const char *msg, const char *buf, int len)
{
	int	i;

	if(mask & debug_mask) {
		log_function(loglevel, ~0, "%-15s:", msg);
		for(i = 0; i < len; i++)
			log_function(loglevel, ~0, " %02X", (uint8_t)buf[i]);
		log_function(loglevel, ~0, "\n");
	}
}

/* from glibc info(1) */
void print_backtrace (FILE *fp)
{
	void	*array[10];
	size_t	size;
	char	**strings;
	size_t	i;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);
	for (i = 0; i < size; i++)
		fprintf (fp, "%s\n", strings[i]);
	free (strings);
}
