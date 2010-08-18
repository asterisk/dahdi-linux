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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define FAST_HDLC_NEED_TABLES
#include <dahdi/fasthdlc.h>

#include "dahdi_tools_version.h"

int myread(int fd, unsigned char *buf, int len)
{
	int sofar;
	int res;
	sofar = 0;
	while(sofar < len) {
		res = read(fd, buf + sofar, len - sofar);
		if (res < 0)
			return res;
		sofar += res;
	}
	return sofar;
}

static inline unsigned char nextchar(int fd)
{
	static unsigned char inbuf[2048];
	static int bytes = 0;
	static int pos = 0;
	if (pos >= bytes) {
		pos = 0;
		bytes = read(fd, inbuf, sizeof(inbuf));
		if (bytes < 0) {
			fprintf(stderr, "Unable to read more data: %s\n", strerror(errno));
			exit(1);
		}
		if (bytes == 0) {
			fprintf(stderr, "-- END OF DATA --\n");
			exit(0);
		}
	}
	return inbuf[pos++];
}

int main(int argc, char *argv[])
{
	unsigned char decbuf[1024];
	unsigned char actual[1024];
	int res;
	int datain;
	int hdlcin;
	int hdlccnt;
	int x;
	struct fasthdlc_state receiver;
	
	fasthdlc_precalc();
	
	fasthdlc_init(&receiver, FASTHDLC_MODE_64);
	
	hdlcin = open("random.hdlc", O_RDONLY);
	if (hdlcin < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", "random.hdlc", strerror(errno));
		exit(1);
	}
	datain = open("random.raw", O_RDONLY);
	if (datain < 0) {
		fprintf(stderr, "Unable to open random.raw: %s\n", strerror(errno));
		exit(1);
	}
	hdlccnt = 0;
	for (;;) {
		/* Feed in some input */
		if (fasthdlc_rx_load(&receiver, nextchar(hdlcin))) {
			fprintf(stderr, "Unable to feed receiver :(\n");
			exit(1);
		}
		res = fasthdlc_rx_run(&receiver);
		if (res & RETURN_EMPTY_FLAG)
			continue;
		if (res & RETURN_COMPLETE_FLAG) {
			if (hdlccnt) {
				if (argc > 1)
					printf("Got message of length %d\n", hdlccnt);
				res = myread(datain, actual, hdlccnt);
				if (res != hdlccnt) {
					fprintf(stderr, "Tried to read %d bytes, but read %d instead\n", hdlccnt, res);
					exit(1);
				}
				for (x=0;x<hdlccnt;x++) {
					if (actual[x] != decbuf[x]) {
						fprintf(stderr, "Found discrepancy at offset %d\n", x);
						exit(1);
					}
				}
				/* Reset message receiver */
				hdlccnt = 0;
			}
		} else if (res & RETURN_DISCARD_FLAG) {
			if (1 || hdlccnt) {
				fprintf(stderr, "Detected abort :(\n");
				exit(1);
			}
		} else {
			decbuf[hdlccnt++] = res;
		}
	}
}
