/*
 * Performance and Maintenance utility
 *
 * Written by Russ Meyerriecks <rmeyerriecks@digium.com>
 *
 * Copyright (C) 2009-2010 Digium, Inc.
 *
 * All rights reserved.
 *
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
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"

#define DAHDI_CTL "/dev/dahdi/ctl"

extern char *optarg;
extern int optind;

void display_help(char *argv0, int exitcode)
{
	char *c;
	c = strrchr(argv0, '/');
	if (!c)
		c = argv0;
	else
		c++;
	fprintf(stderr, "%s\n\n", dahdi_tools_version);
	fprintf(stderr, "Usage: %s -s <span num> <options>\n", c);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "        -h, --help		display help\n");
	fprintf(stderr, "        -s, --span <span num>	specify the span\n");
	fprintf(stderr, "        -l, --loopback <localhost|networkline|"\
						"networkpayload|loopup|"\
						"loopdown|off>\n"\
			"\t\tlocalhost - loop back towards host\n"\
			"\t\tnetworkline - network line loopback\n"\
			"\t\tnetworkpayload - network payload loopback\n"\
			"\t\tloopup - transmit loopup signal\n"\
			"\t\tloopdown - transmit loopdown signal\n"\
			"\t\toff - end loopback mode\n");
	fprintf(stderr, "        -i, --insert <fas|multi|crc|cas|prbs|bipolar>"\
			"\n\t\tinsert an error of a specific type\n");
	fprintf(stderr, "        -r, --reset		"\
			"reset the error counters\n\n");
	fprintf(stderr, "Examples: \n");
	fprintf(stderr, "Enable network line loopback\n");
	fprintf(stderr, "	dahdi_maint -s 1 --loopback networkline\n");
	fprintf(stderr, "Disable network line loopback\n");
	fprintf(stderr, "	dahdi_maint -s 1 --loopback off\n\n");

	exit(exitcode);
}

int main(int argc, char *argv[])
{
	static int ctl = -1;
	int res;

	int doloopback = 0;
	char *larg = NULL;
	int span = 1;
	int iflag = 0;
	char *iarg = NULL;
	int gflag = 0;
	int c;
	int rflag = 0;

	struct dahdi_maintinfo m;
	struct dahdi_spaninfo s;

	static struct option long_options[] = {
		{"help",	no_argument,	   0, 'h'},
		{"loopback",	required_argument, 0, 'l'},
		{"span",	required_argument, 0, 's'},
		{"insert",	required_argument, 0, 'i'},
		{"reset",	no_argument, 	   0, 'r'},
		{0, 0, 0, 0}
	};
	int option_index = 0;

	if (argc < 2) { /* no options */
		display_help(argv[0], 1);
	}

	while ((c = getopt_long(argc, argv, "hj:l:p:s:i:g:r",
				long_options, &option_index)) != -1) {
			switch (c) {
			case 'h':
				display_help(argv[0], 0);
				break;
			case 'l': /* loopback */
				larg = optarg;
				doloopback = 1;
				break;
			case 's': /* specify a span */
				span = atoi(optarg);
				break;
			case 'i': /* insert an error */
				iarg = optarg;
				iflag = 1;
				break;
			case 'g': /* generate psuedo random sequence */
				gflag = 1;
				break;
			case 'r': /* reset the error counters */
				rflag = 1;
				break;
			}
	}

	ctl = open(DAHDI_CTL, O_RDWR);
	if (ctl < 0) {
		fprintf(stderr, "Unable to open %s\n", DAHDI_CTL);
		return -1;
	}

	if (!(doloopback || iflag || gflag || rflag)) {
		s.spanno = span;
		res = ioctl(ctl, DAHDI_SPANSTAT, &s);
		if (res || ((__u32)-1 == s.fecount))
			printf("Error counters not supported by the driver"\
					" for this span\n");
		printf("Span %d:\n", span);
		printf(">Framing Errors : %d:\n", s.fecount);
		printf(">CRC Errors : %d:\n", s.crc4count);
		printf(">Code Violations : %d:\n", s.cvcount);
		printf(">E-bit Count : %d:\n", s.ebitcount);
		printf(">General Errored Seconds : %d:\n", s.errsec);

		return 0;
	}

	m.spanno = span;

	if (doloopback) {
		if (!strcasecmp(larg, "localhost")) {
			printf("Span %d: local host loopback ON\n", span);
			m.command = DAHDI_MAINT_LOCALLOOP;
		} else if (!strcasecmp(larg, "networkline")) {
			printf("Span %d: network line loopback ON\n", span);
			m.command = DAHDI_MAINT_NETWORKLINELOOP;
		} else if (!strcasecmp(larg, "networkpayload")) {
			printf("Span %d: network payload loopback ON\n", span);
			m.command = DAHDI_MAINT_NETWORKPAYLOADLOOP;
		} else if (!strcasecmp(larg, "loopup")) {
			printf("Span %d: transmitting loopup signal\n", span);
			m.command = DAHDI_MAINT_LOOPUP;
		} else if (!strcasecmp(larg, "loopdown")) {
			printf("Span %d: transmitting loopdown signal\n", span);
			m.command = DAHDI_MAINT_LOOPDOWN;
		} else if (!strcasecmp(larg, "off")) {
			printf("Span %d: loopback OFF\n", span);
			m.command = DAHDI_MAINT_NONE;
		} else {
			display_help(argv[0], 1);
		}

		res = ioctl(ctl, DAHDI_MAINT, &m);
		if (res) {
			printf("This type of looping not supported by the"\
					" driver for this span\n");
			return 1;
		}

		/* Leave the loopup/loopdown signal on the line for
		 * five seconds according to AT&T TR 54016
		 */
		if ((m.command == DAHDI_MAINT_LOOPUP) ||
		   (m.command == DAHDI_MAINT_LOOPDOWN)) {
			sleep(5);
			m.command = DAHDI_MAINT_NONE;
			ioctl(ctl, DAHDI_MAINT, &m);
		}
	}

	if (iflag) {
		if (!strcasecmp(iarg, "fas")) {
			m.command = DAHDI_MAINT_FAS_DEFECT;
			printf("Inserting a single FAS defect\n");
		} else if (!strcasecmp(iarg, "multi")) {
			m.command = DAHDI_MAINT_MULTI_DEFECT;
			printf("Inserting a single multiframe defect\n");
		} else if (!strcasecmp(iarg, "crc")) {
			m.command = DAHDI_MAINT_CRC_DEFECT;
			printf("Inserting a single CRC defect\n");
		} else if (!strcasecmp(iarg, "cas")) {
			m.command = DAHDI_MAINT_CAS_DEFECT;
			printf("Inserting a single CAS defect\n");
		} else if (!strcasecmp(iarg, "prbs")) {
			m.command = DAHDI_MAINT_PRBS_DEFECT;
			printf("Inserting a single PRBS defect\n");
		} else if (!strcasecmp(iarg, "bipolar")) {
			m.command = DAHDI_MAINT_BIPOLAR_DEFECT;
			printf("Inserting a single bipolar defect\n");
#ifdef DAHDI_MAINT_ALARM_SIM
		} else if (!strcasecmp(iarg, "sim")) {
			m.command = DAHDI_MAINT_ALARM_SIM;
			printf("Incrementing alarm simulator\n");
#endif
		} else {
			display_help(argv[0], 1);
		}
		res = ioctl(ctl, DAHDI_MAINT, &m);
		if (res)
			printf("This type of error injection is not supported"\
					" by the driver for this span\n");
	}

	if (gflag) {
		printf("Enabled the Pseudo-Random Binary Sequence Generation"\
			" and Monitor\n");
		m.command = DAHDI_MAINT_PRBS;
		res = ioctl(ctl, DAHDI_MAINT, &m);
		if (res) {
			printf("Pseudo-random binary sequence generation is"\
				" not supported by the driver for this span\n");
		}
	}

	if (rflag) {
		printf("Resetting error counters for span %d\n", span);
		m.command = DAHDI_RESET_COUNTERS;
		res = ioctl(ctl, DAHDI_MAINT, &m);
		if (res) {
			printf("Resetting error counters is not supported by"\
					" the driver for this span\n");
		}
	}

	return 0;
}
