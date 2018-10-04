/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <dahdi/kernel.h>
#include "dahdi_debug.h"
#include "xdefs.h"

static const char rcsid[] = "$Id$";

#define	P_(x)	[ x ] = { .value = x, .name = #x, }
static struct {
	int value;
	char *name;
} poll_names[] = {
	P_(POLLIN), P_(POLLPRI), P_(POLLOUT), P_(POLLERR), P_(POLLHUP),
	    P_(POLLNVAL), P_(POLLRDNORM), P_(POLLRDBAND), P_(POLLWRNORM),
	    P_(POLLWRBAND), P_(POLLMSG), P_(POLLREMOVE)
};

#undef	P_

void dump_poll(int debug, const char *msg, int poll)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(poll_names); i++) {
		if (poll & poll_names[i].value)
			DBG(GENERAL, "%s: %s\n", msg, poll_names[i].name);
	}
}
EXPORT_SYMBOL(dump_poll);

void alarm2str(int alarm, char *buf, int buflen)
{
	char *p = buf;
	int left = buflen;
	int i;
	int n;

	if (!alarm) {
		snprintf(buf, buflen, "NONE");
		return;
	}
	memset(buf, 0, buflen);
	for (i = 0; i < 8; i++) {
		if (left && (alarm & BIT(i))) {
			n = snprintf(p, left, "%s,", alarmbit2str(i));
			p += n;
			left -= n;
		}
	}
	if (p > buf)		/* kill last comma */
		*(p - 1) = '\0';
}
EXPORT_SYMBOL(alarm2str);
