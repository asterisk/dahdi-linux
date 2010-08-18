/*
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001-2008 Digium, Inc.
 *
 * All rights reserved.
 *
 * Primary Author: Mark Spencer <markster@digium.com>
 * Radio Support by Jim Dixon <jim@lambdatel.com>
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"

int main(int argc, char *argv[])
{
	int fd;
	int chan;
	if ((argc < 2) || (sscanf(argv[1], "%d", &chan) != 1)) {
		fprintf(stderr, "Usage: dahdi_diag <channel>\n");
		exit(1);
	}
	fd = open("/dev/dahdi/ctl", O_RDWR);
	if (fd < 0) {
		perror("open(/dev/dahdi/ctl");
		exit(1);
	}
	if (ioctl(fd, DAHDI_CHANDIAG, &chan)) {
		perror("ioctl(DAHDI_CHANDIAG)");
		exit(1);
	}
	exit(0);
}
