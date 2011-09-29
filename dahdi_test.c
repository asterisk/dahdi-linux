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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <math.h>
#include <getopt.h>

#include "dahdi_tools_version.h"

#define SIZE 8000

static int verbose;
static int pass = 0;
static float best = 0.0;
static float worst = 100.0;
static double total = 0.0;
static double total_time = 0.0;
static double total_count = 0.0;

static inline float _fmin(float a, float b)
{
	return (a < b) ? a : b;
}

static double calculate_accuracy(double count, double ms)
{
	return ((count - _fmin(count, fabs(count - ms))) / count) * 100.0;
}

void hup_handler(int sig)
{
	double accuracy = calculate_accuracy(total_count, total_time);
	printf("\n--- Results after %d passes ---\n", pass);
	printf("Best: %.3f%% -- Worst: %.3f%% -- Average: %f%%\n",
			best, worst, pass ? total/pass : 100.00);
	printf("Cummulative Accuracy (not per pass): %0.3f\n",
	       pass ? accuracy : 0.0);
	exit(0);
}

static void usage(char *argv0)
{
	char *c;
	c = strrchr(argv0, '/');
	if (!c)
		c = argv0;
	else
		c++;
	fprintf(stderr, 
		"Usage: %s [-c COUNT] [-v]\n"
		"    Valid options are:\n"
		"  -c COUNT    Run just COUNT cycles (otherwise: forever).\n"
		"  -v          More verbose output.\n"
		"  -h          This help text.\n"
	, c);
}

int main(int argc, char *argv[])
{
	int fd;
	int res;
	int c;
	int count = 0;
	int seconds = 0;
	int curarg = 1;
	char buf[8192];
	float ms;
	struct timeval start, now;
	fd = open("/dev/dahdi/pseudo", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open dahdi interface: %s\n", strerror(errno));
		exit(1);
	}
	
	while ((c = getopt(argc, argv, "c:hv")) != -1) {
		switch(c) {
		case 'c':
			seconds = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case '?':
			usage(argv[0]);
			exit(1);
			break;
		case 'v':
			verbose++;
			break;
		}
	}
	while (curarg < argc) {
		if (!strcasecmp(argv[curarg], "-v"))
			verbose++;
		if (!strcasecmp(argv[curarg], "-c") && argc > curarg)
			seconds = atoi(argv[curarg + 1]);
		curarg++;
	}
	printf("Opened pseudo dahdi interface, measuring accuracy...\n");
	signal(SIGHUP, hup_handler);
	signal(SIGINT, hup_handler);
	signal(SIGALRM, hup_handler);
	/* Flush input buffer */
	for (count = 0; count < 4; count++)
		res = read(fd, buf, sizeof(buf));
	count = 0;
	ms = 0; /* Makes the compiler happy */
	if (seconds > 0)
		alarm(seconds + 1); /* This will give 'seconds' cycles */
	for (;;) {
		if (count == 0)
			ms = 0;
		gettimeofday(&start, NULL);
		res = read(fd, buf, sizeof(buf));
		if (res < 0) {
			fprintf(stderr, "Failed to read from pseudo interface: %s\n", strerror(errno));
			exit(1);
		}
		count += res;
		gettimeofday(&now, NULL);
		ms += (now.tv_sec - start.tv_sec) * 8000;
		ms += (now.tv_usec - start.tv_usec) / 125.0;
		if (count >= SIZE) {
			const double percent = calculate_accuracy(count, ms);
			if (verbose) {
				printf("\n%d samples in %0.3f system clock sample intervals (%.3f%%)", 
						count, ms, percent);
			} else if (pass > 0 && (pass % 8) == 0) {
				printf("\n");
			}
			if (percent > best)
				best = percent;
			if (percent < worst)
				worst = percent;
			if (!verbose)
				printf("%.3f%% ", percent);
			total += percent;
			fflush(stdout);
			total_count += count;
			total_time += ms;
			count = 0;
			pass++;
		}
	}
}
