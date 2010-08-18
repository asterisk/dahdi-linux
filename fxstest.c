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
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dahdi/user.h>
#include <dahdi/wctdm_user.h>

#include "tonezone.h"
#include "dahdi_tools_version.h"

static int tones[] = {
	DAHDI_TONE_DIALTONE,
	DAHDI_TONE_BUSY,
	DAHDI_TONE_RINGTONE,
	DAHDI_TONE_CONGESTION,
	DAHDI_TONE_DIALRECALL,
};

struct dahdi_vmwi_info mwisend_setting; /*!< Which VMWI methods to use */

/* Use to translate a DTMF character to the value required by the dahdi call */
static int digit_to_dtmfindex(char digit)
{
	if (isdigit(digit))
		return DAHDI_TONE_DTMF_BASE + (digit - '0');
	else if (digit >= 'A' && digit <= 'D')
		return DAHDI_TONE_DTMF_A + (digit - 'A');
	else if (digit >= 'a' && digit <= 'd')
		return DAHDI_TONE_DTMF_A + (digit - 'a');
	else if (digit == '*')
		return DAHDI_TONE_DTMF_s;
	else if (digit == '#')
		return DAHDI_TONE_DTMF_p;
	else
		return -1;
}

/* Place a channel into ringing mode */
static int dahdi_ring_phone(int fd)
{
	int x;
	int res;
	/* Make sure our transmit state is on hook */
	x = 0;
	x = DAHDI_ONHOOK;
	res = ioctl(fd, DAHDI_HOOK, &x);
	do {
		x = DAHDI_RING;
		res = ioctl(fd, DAHDI_HOOK, &x);
		if (res) {
			switch (errno) {
				case EBUSY:
				case EINTR:
					/* Wait just in case */
					fprintf(stderr, "Ring phone is busy:%s\n", strerror(errno));
					usleep(10000);
					continue;
				case EINPROGRESS:
					fprintf(stderr, "Ring In Progress:%s\n", strerror(errno));
					res = 0;
					break;
				default:
					fprintf(stderr, "Couldn't ring the phone: %s\n", strerror(errno));
					res = 0;
			}
		} else {
			fprintf(stderr, "Phone is ringing\n");
		}
	} while (res);
	return res;
}

int main(int argc, char *argv[])
{
	int fd;
	int res;
	int x;
	if (argc < 3) {
		fprintf(stderr, "Usage: fxstest <dahdi device> <cmd>\n"
		       "       where cmd is one of:\n"
		       "       stats - reports voltages\n"
		       "       regdump - dumps ProSLIC registers\n"
		       "       tones - plays a series of tones\n"
		       "       polarity - tests polarity reversal\n"
		       "       ring - rings phone\n"
		       "       vmwi - toggles VMWI LED lamp\n"
		       "       hvdc - toggles VMWI HV lamp\n"
		       "       neon - toggles VMWI NEON lamp\n"
		       "       dtmf <sequence> [<duration>]- Send a sequence of dtmf tones (\"-\" denotes no tone)\n"
		       "       dtmfcid - create a dtmf cid spill without polarity reversal\n");
		exit(1);
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	
	if ( !strcasecmp(argv[2], "neon") || !strcasecmp(argv[2], "vmwi") || !strcasecmp(argv[2], "hvdc")) {
		fprintf(stderr, "Twiddling %s ...\n", argv[2]);

		if ( !strcasecmp(argv[2], "vmwi") ) {
			mwisend_setting.vmwi_type = DAHDI_VMWI_LREV;
		} else if ( !strcasecmp(argv[2], "neon") ) {
			mwisend_setting.vmwi_type = DAHDI_VMWI_HVAC;
		} else if ( !strcasecmp(argv[2], "hvdc") ) {
			mwisend_setting.vmwi_type = DAHDI_VMWI_HVDC;
		}
		res = ioctl(fd, DAHDI_VMWI_CONFIG, &mwisend_setting);

		x = 1;
		res = ioctl(fd, DAHDI_VMWI, &x);
		if (res) {
			fprintf(stderr, "Unable to set %s ...\n", argv[2]);
		} else {
			fprintf(stderr, "Set 1 Voice Message...\n");

			sleep(5);
			x = 2;
			ioctl(fd, DAHDI_VMWI, &x);
			fprintf(stderr, "Set 2 Voice Messages...\n");

			sleep(5);
			x = 0;
			ioctl(fd, DAHDI_VMWI, &x);
			fprintf(stderr, "Set No Voice messages...\n");
			sleep(2);
			mwisend_setting.vmwi_type = 0;
		}
	} else if (!strcasecmp(argv[2], "ring")) {
		fprintf(stderr, "Ringing phone...\n");
		x = DAHDI_RING;
		res = ioctl(fd, DAHDI_HOOK, &x);
		if (res) {
			fprintf(stderr, "Unable to ring phone...\n");
		} else {
			fprintf(stderr, "Phone is ringing...\n");
			sleep(2);
		}
	} else if (!strcasecmp(argv[2], "polarity")) {
		fprintf(stderr, "Twiddling polarity...\n");
		/* Insure that the channel is in active mode */
		x = DAHDI_RING;
		res = ioctl(fd, DAHDI_HOOK, &x);
		usleep(100000);
		x = 0;
		res = ioctl(fd, DAHDI_HOOK, &x);

		x = 0;
		res = ioctl(fd, DAHDI_SETPOLARITY, &x);
		if (res) {
			fprintf(stderr, "Unable to polarity...\n");
		} else {
			fprintf(stderr, "Polarity is forward...\n");
			sleep(2);
			x = 1;
			ioctl(fd, DAHDI_SETPOLARITY, &x);
			fprintf(stderr, "Polarity is reversed...\n");
			sleep(5);
			x = 0;
			ioctl(fd, DAHDI_SETPOLARITY, &x);
			fprintf(stderr, "Polarity is forward...\n");
			sleep(2);
		}
	} else if (!strcasecmp(argv[2], "tones")) {
		int x = 0;
		for (;;) {
			res = tone_zone_play_tone(fd, tones[x]);
			if (res)
				fprintf(stderr, "Unable to play tone %d\n", tones[x]);
			sleep(3);
			x=(x+1) % (sizeof(tones) / sizeof(tones[0]));
		}
	} else if (!strcasecmp(argv[2], "stats")) {
		struct wctdm_stats stats;
		res = ioctl(fd, WCTDM_GET_STATS, &stats);
		if (res) {
			fprintf(stderr, "Unable to get stats on channel %s\n", argv[1]);
		} else {
			printf("TIP: %7.4f Volts\n", (float)stats.tipvolt / 1000.0);
			printf("RING: %7.4f Volts\n", (float)stats.ringvolt / 1000.0);
			printf("VBAT: %7.4f Volts\n", (float)stats.batvolt / 1000.0);
		}
	} else if (!strcasecmp(argv[2], "regdump")) {
		struct wctdm_regs regs;
		int numregs = NUM_REGS;
		memset(&regs, 0, sizeof(regs));
		res = ioctl(fd, WCTDM_GET_REGS, &regs);
		if (res) {
			fprintf(stderr, "Unable to get registers on channel %s\n", argv[1]);
		} else {
			for (x=60;x<NUM_REGS;x++) {
				if (regs.direct[x])
					break;
			}
			if (x == NUM_REGS) 
				numregs = 60;
			printf("Direct registers: \n");
			for (x=0;x<numregs;x++) {
				printf("%3d. %02x  ", x, regs.direct[x]);
				if ((x % 8) == 7)
					printf("\n");
			}
			if (numregs == NUM_REGS) {
				printf("\n\nIndirect registers: \n");
				for (x=0;x<NUM_INDIRECT_REGS;x++) {
					printf("%3d. %04x  ", x, regs.indirect[x]);
					if ((x % 6) == 5)
						printf("\n");
				}
			}
			printf("\n\n");
		}
	} else if (!strcasecmp(argv[2], "setdirect") ||
				!strcasecmp(argv[2], "setindirect")) {
		struct wctdm_regop regop;
		int val;
		int reg;
		if ((argc < 5) || (sscanf(argv[3], "%i", &reg) != 1) ||
			(sscanf(argv[4], "%i", &val) != 1)) {
			fprintf(stderr, "Need a register and value...\n");
		} else {
			regop.reg = reg;
			regop.val = val;
			if (!strcasecmp(argv[2], "setindirect")) {
				regop.indirect = 1;
			} else {
				regop.indirect = 0;
			}
			res = ioctl(fd, WCTDM_SET_REG, &regop);
			if (res) 
				fprintf(stderr, "Unable to get registers on channel %s\n", argv[1]);
			else
				printf("Success.\n");
		}
	} else if (!strcasecmp(argv[2], "dtmf")) {
		int duration = 50;  /* default to 50 mS duration */
		char * outstring = "";
		int dtmftone;

		if(argc < 4) {	/* user supplied string */
			fprintf(stderr, "You must specify a string of dtmf characters to send\n");
		} else {
			outstring = argv[3];
			if(argc >= 5) {
				sscanf(argv[4], "%30i", &duration);
			}
			printf("Going to send a set of DTMF tones >%s<\n", outstring);
			printf("Using a duration of %d mS per tone\n", duration);
					/* Flush any left remaining characs in the buffer and place the channel into on-hook transfer mode */
			x = DAHDI_FLUSH_BOTH;
			res = ioctl(fd, DAHDI_FLUSH, &x);
			x = 500 + strlen(outstring) * duration;
			ioctl(fd, DAHDI_ONHOOKTRANSFER, &x);

			for (x = 0; '\0' != outstring[x]; x++) {
				dtmftone = digit_to_dtmfindex(outstring[x]);
				if (0 > dtmftone) {
					dtmftone = -1;
				}
				res = tone_zone_play_tone(fd, dtmftone);
				if (res) {
					fprintf(stderr, "Unable to play DTMF tone %d (0x%x)\n", dtmftone, dtmftone);
				}
				usleep(duration * 1000);
			}
		}
	} else if (!strcasecmp(argv[2], "dtmfcid")) {
		char * outstring = "A5551212C";  /* Default string using A and C tones to bracket the number */
		int dtmftone;
		
		if(argc >= 4) {	/* Use user supplied string */
			outstring = argv[3];
		}
		printf("Going to send a set of DTMF tones >%s<\n", outstring);
		/* Flush any left remaining characs in the buffer and place the channel into on-hook transfer mode */
		x = DAHDI_FLUSH_BOTH;
		res = ioctl(fd, DAHDI_FLUSH, &x);
		x = 500 + strlen(outstring) * 100;
		ioctl(fd, DAHDI_ONHOOKTRANSFER, &x);

		/* Play the DTMF tones at a 50 mS on and 50 mS off rate which is standard for DTMF CID spills */
		for (x = 0; '\0' != outstring[x]; x++) {

			dtmftone = digit_to_dtmfindex(outstring[x]);
			if (0 > dtmftone) {
				dtmftone = -1;
			}
			res = tone_zone_play_tone(fd, dtmftone);
			if (res) {
				fprintf(stderr, "Unable to play DTMF tone %d (0x%x)\n", dtmftone, dtmftone);
			}
			usleep(50000);
			tone_zone_play_tone(fd, -1);
			usleep(50000);
		}
		/* Wait for 150 mS from end of last tone to initiating the ring */
		usleep(100000);
		dahdi_ring_phone(fd);
		sleep(10);
		printf("Ringing Done\n");
	} else
		fprintf(stderr, "Invalid command\n");
	close(fd);
	return 0;
}
