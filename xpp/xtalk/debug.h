#ifndef	DEBUG_H
#define	DEBUG_H
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

#include <syslog.h>
#include <stdio.h>

/*
 * Each module should define a unique DBG_MASK
 */

extern	int	verbose;
extern	int	debug_mask;

/*
 * Logging
 */
void log_function(int level, int mask, const char *msg, ...) __attribute__(( format(printf, 3, 4) ));

#define	ERR(fmt, arg...) log_function(LOG_ERR, 0, "%s:%d: ERROR(%s): " fmt, __FILE__, __LINE__, __FUNCTION__, ## arg)
#define	WARN(fmt, arg...) log_function(LOG_WARNING, 0, "WARNING: " fmt, ## arg)
#define	INFO(fmt, arg...) log_function(LOG_INFO, 0, "INFO: " fmt, ## arg)
#define	DBG(fmt, arg...) log_function(LOG_DEBUG, DBG_MASK,	\
		"%s:%d: DBG(%s): " fmt, __FILE__, __LINE__, __FUNCTION__, ## arg)

void dump_packet(int loglevel, int mask, const char *msg, const char *buf, int len);
void print_backtrace (FILE *fp);

#endif	/* DEBUG_H */
