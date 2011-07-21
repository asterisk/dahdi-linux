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

/*
 *	This test sends a set of incrementing byte values out the specified
 * dadhi device.  The device is then read back and the read back characters
 * are verified that they increment as well.
 * 	If there is a break in the incrementing pattern, an error is flagged 
 * and the comparison starts at the last value read. 
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"

#define BLOCK_SIZE	2039
#define DEVICE	"/dev/dahdi/channel"

#define CONTEXT_SIZE	7
/* Prints a set of bytes in hex format */
static void print_packet(unsigned char *buf, int len)
{
	int x;
	printf("{ ");
	for (x=0;x<len;x++)
		printf("%02x ",buf[x]);
	printf("}\n");
}

/* Shows data immediately before and after the specified byte to provide context for an error */
static void show_error_context(unsigned char *buf, int offset, int bufsize)
{
	int low;
	int total = CONTEXT_SIZE;

	if (offset >= bufsize || 0 >= bufsize || 0 > offset ) {
		return;
	}
	
	low = offset - (CONTEXT_SIZE-1)/2;
	if (0 > low) {
		total += low;
		low = 0;
	}
	if (low + total > bufsize) {
		total = bufsize - low;
	}
	buf += low;
	printf("Offset %d  ", low);
	print_packet(buf, total);
	return;
}

/* Shows how the program can be invoked */
static void usage(const char * progname)
{
	printf("%s: Pattern loop test\n", progname);
	printf("Usage:  %s <dahdi device> [-t <secs>] [-r <count>] [-b <count>] [-vh?] \n", progname);
	printf("\t-? - Print this usage summary\n");
	printf("\t-t <secs> - # of seconds for the test to run\n");
	printf("\t-r <count> - # of test loops to run before a summary is printed\n");
	printf("\t-s <count> - # of writes to skip before testing for results\n");
	printf("\t-v - Verbosity (repetitive v's add to the verbosity level e.g. -vvvv)\n");
	printf("\t-b <# buffer bytes> - # of bytes to display from buffers on each pass\n");
	printf("\n\t Also accepts old style usage:\n\t  %s <device name> [<timeout in secs>]\n", progname);
}

int channel_open(const char *name, int *bs)
{
	int	channo, fd;
	struct	dahdi_params tp;
	struct	stat filestat;

	/* stat file, if character device, open it */
	channo = strtoul(name, NULL, 10);
	fd = stat(name, &filestat);
	if (!fd && S_ISCHR(filestat.st_mode)) {
		fd = open(name, O_RDWR, 0600);
		if (fd < 0) {
			perror(name);
			return -1;
		}
	/* try out the dahdi_specify interface */
	} else if (channo > 0) {
		fd = open(DEVICE, O_RDWR, 0600);
		if (fd < 0) {
			perror(DEVICE);
			return -1;
		}
		if (ioctl(fd, DAHDI_SPECIFY, &channo) < 0) {
			perror("DAHDI_SPECIFY ioctl failed");
			return -1;
		}
	/* die */
	} else {
		fprintf(stderr, "Specified channel is not a valid character "
			"device or channel number");
		return -1;
	}

	if (ioctl(fd, DAHDI_SET_BLOCKSIZE, bs) < 0) {
		perror("SET_BLOCKSIZE");
		return -1;
	}

	if (ioctl(fd, DAHDI_GET_PARAMS, &tp)) {
		fprintf(stderr, "Unable to get channel parameters\n");
		return -1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	int fd;
	int res, x;
	int i;
	int bs = BLOCK_SIZE;
	int skipcount = 10;
	unsigned char c=0,c1=0;
	unsigned char inbuf[BLOCK_SIZE];
	unsigned char outbuf[BLOCK_SIZE];
	int setup=0;
	unsigned long bytes=0;
	int timeout=0;
	int loop_errorcount;
	int reportloops = 0;
	int buff_disp = 0; 
	unsigned long currentloop = 0;
	unsigned long total_errorcount = 0;
	int verbose = 0; 
	char * device;
	int opt;
	int oldstyle_cmdline = 1;
	unsigned int event_count = 0;

	/* Parse the command line arguments */
	while((opt = getopt(argc, argv, "b:s:t:r:v?h")) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			usage(argv[0]);
			exit(1);
			break;
		case 'b':
			buff_disp = strtoul(optarg, NULL, 10);
			if (BLOCK_SIZE < buff_disp) {
				buff_disp = BLOCK_SIZE;
			}
			oldstyle_cmdline = 0;
			break;
		case 'r':
			reportloops = strtoul(optarg, NULL, 10);
			oldstyle_cmdline = 0;
			break;
		case 's':
			skipcount = strtoul(optarg, NULL, 10);
			oldstyle_cmdline = 0;
			break;
		case 't':
			timeout = strtoul(optarg, NULL, 10);
			oldstyle_cmdline = 0;
			break;
		case 'v':
			verbose++;
			oldstyle_cmdline = 0;
			break;
		}
	}

	/* If no device was specified */
	if(NULL == argv[optind]) {
		printf("You need to supply a dahdi device to test\n");
		usage(argv[0]);
		exit (1);
	}

	/* Get the dahdi device name */
	if (argv[optind])
		device = argv[optind];

	/* To maintain backward compatibility with previous versions process old style command line */
	if (oldstyle_cmdline && argc > optind +1) {
		timeout = strtoul(argv[optind+1], NULL, 10);
	}
	
	time_t start_time = 0;

	fd = channel_open(device, &bs);
	if (fd < 0)
		exit(1);
	ioctl(fd, DAHDI_GETEVENT);

	i = DAHDI_FLUSH_ALL;
	if (ioctl(fd,DAHDI_FLUSH,&i) == -1) {
		perror("DAHDI_FLUSH");
		exit(255);
	}

	/* Mark time if program has a specified timeout */
	if(0 < timeout){
		start_time = time(NULL);
		printf("Using Timeout of %d Seconds\n",timeout);
	}

	/* ********* MAIN TESTING LOOP ************ */
	for(;;) {
		/* Prep the data and write it out to dahdi device */
		res = bs;
		for (x = 0; x < bs; x++) {
			outbuf[x] = c1++;
		}

write_again:
		res = write(fd,outbuf,bs);
		if (res != bs) {
			if (ELAST == errno) {
				ioctl(fd, DAHDI_GETEVENT, &x);
				if (event_count > 0)
					printf("Event: %d\n", x);
				++event_count;
			} else {
				printf("W: Res is %d: %s\n", res, strerror(errno));
			}
			goto write_again;
		}

		/* If this is the start of the test then skip a number of packets before test results */
		if (skipcount) {
			if (skipcount > 1) {
				res = read(fd,inbuf,bs);
			}
			skipcount--;
			if (!skipcount) {
				printf("Going for it...\n");
			}
			i = 1;
			ioctl(fd,DAHDI_BUFFER_EVENTS, &i);
			continue;
		}

read_again:
		res = read(fd, inbuf, bs);
		if (res < bs) {
			printf("R: Res is %d\n", res);
			ioctl(fd, DAHDI_GETEVENT, &x);
			printf("Event: %d\n", x);
			goto read_again;
		}
		/* If first time through, set byte that is used to test further bytes */
		if (!setup) {
			c = inbuf[0];
			setup++;
		}
		/* Test the packet read back for data pattern */
		loop_errorcount = 0;
		for (x = 0; x < bs; x++)  {
			/* if error */
			if (inbuf[x] != c) {
				total_errorcount++;
				loop_errorcount++;
				if (oldstyle_cmdline) {
					printf("(Error %ld): Unexpected result, %d != %d, %ld bytes since last error.\n", total_errorcount, inbuf[x],c, bytes);
				} else {
					if (1 <= verbose) {
						printf("Error %ld (loop %ld, offset %d, error %d): Unexpected result, Read: 0x%02x, Expected 0x%02x.\n",
							total_errorcount,
							currentloop,
							x,
							loop_errorcount,
							inbuf[x],
							c);
					}
					if (2 <= verbose) {
						show_error_context(inbuf, x, bs);
					}
				}
				/* Reset the expected data to what was just read.  so test can resynch on skipped data */
				c = inbuf[x];
				bytes=0;  /* Reset the count from the last encountered error */
			}
			c++;
			bytes++;
		}
		/* If the user wants to see some of each buffer transaction */
		if (0 < buff_disp) {
			printf("Buffer Display %d (errors =%d)\nIN: ", buff_disp, loop_errorcount);
			print_packet(inbuf, 64);
			printf("OUT:");
			print_packet(outbuf, 64);
		}
		
		currentloop++;
		/* Update stats if the user has specified it */
		if (0 < reportloops && 0 == (currentloop % reportloops)) {
			printf("Status on loop %lu:  Total errors = %lu\n", currentloop, total_errorcount);
			
		}
#if 0
		printf("(%d) Wrote %d bytes\n", packets++, res);
#endif
		if(timeout && (time(NULL)-start_time) > timeout){
			printf("Timeout achieved Ending Program\n");
			printf("Test ran %ld loops of %d bytes/loop with %ld errors\n", currentloop, bs, total_errorcount);
			return total_errorcount;
		}
	}
	
}

