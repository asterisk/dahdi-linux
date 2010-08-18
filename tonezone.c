/*
 * BSD Telephony Of Mexico "Tormenta" Tone Zone Support 2/22/01
 * 
 * Working with the "Tormenta ISA" Card 
 *
 * Primary Author: Mark Spencer <markster@digium.com>
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
 * the GNU Lesser General Public License Version 2.1 as published
 * by the Free Software Foundation. See the LICENSE.LGPL file
 * included with this program for more details.
 *
 * In addition, when this program is distributed with Asterisk in
 * any form that would qualify as a 'combined work' or as a
 * 'derivative work' (but not mere aggregation), you can redistribute
 * and/or modify the combination under the terms of the license
 * provided with that copy of Asterisk, instead of the license
 * terms granted here.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "dahdi/user.h"
#include "tonezone.h"
#include "dahdi_tools_version.h"

#define DEFAULT_DAHDI_DEV "/dev/dahdi/ctl"

#define MAX_SIZE 16384
#define CLIP 32635
#define BIAS 0x84

#if 0
# define PRINT_DEBUG(x, ...) printf(x, __VA_ARGS__)
#else
# define PRINT_DEBUG(x, ...)
#endif

#ifndef ENODATA
#define ENODATA EINVAL
#endif

struct tone_zone *tone_zone_find(char *country)
{
	struct tone_zone *z;
	z = builtin_zones;
	while(z->zone > -1) {
		if (!strcasecmp(country, z->country))
			return z;
		z++;
	}
	return NULL;
}

struct tone_zone *tone_zone_find_by_num(int id)
{
	struct tone_zone *z;
	z = builtin_zones;
	while(z->zone > -1) {
		if (z->zone == id)
			return z;
		z++;
	}
	return NULL;
}

#define LEVEL -10

static int build_tone(void *data, size_t size, struct tone_zone_sound *t, int *count)
{
	char *dup, *s;
	struct dahdi_tone_def *td=NULL;
	int firstnobang = -1;
	int freq1, freq2, time;
	int modulate = 0;
	float gain;
	int used = 0;
	dup = strdup(t->data);
	s = strtok(dup, ",");
	while(s && strlen(s)) {
		/* Handle optional ! which signifies don't start here*/
		if (s[0] == '!') 
			s++;
		else if (firstnobang < 0) {
			PRINT_DEBUG("First no bang: %s\n", s);
			firstnobang = *count;
		}
		if (sscanf(s, "%d+%d/%d", &freq1, &freq2, &time) == 3) {
			/* f1+f2/time format */
			PRINT_DEBUG("f1+f2/time format: %d, %d, %d\n", freq1, freq2, time);
		} else if (sscanf(s, "%d*%d/%d", &freq1, &freq2, &time) == 3) {
			/* f1*f2/time format */
			PRINT_DEBUG("f1+f2/time format: %d, %d, %d\n", freq1, freq2, time);
			modulate = 1;
		} else if (sscanf(s, "%d+%d", &freq1, &freq2) == 2) {
			PRINT_DEBUG("f1+f2 format: %d, %d\n", freq1, freq2);
			time = 0;
		} else if (sscanf(s, "%d*%d", &freq1, &freq2) == 2) {
			PRINT_DEBUG("f1+f2 format: %d, %d\n", freq1, freq2);
			modulate = 1;
			time = 0;
		} else if (sscanf(s, "%d/%d", &freq1, &time) == 2) {
			PRINT_DEBUG("f1/time format: %d, %d\n", freq1, time);
			freq2 = 0;
		} else if (sscanf(s, "%d", &freq1) == 1) {
			PRINT_DEBUG("f1 format: %d\n", freq1);
			firstnobang = *count;
			freq2 = 0;
			time = 0;
		} else {
			fprintf(stderr, "tone component '%s' of '%s' is a syntax error\n", s,t->data);
			return -1;
		}

		PRINT_DEBUG("Using %d samples for %d and %d\n", time * 8, freq1, freq2);

		if (size < sizeof(*td)) {
			fprintf(stderr, "Not enough space for tones\n");
			return -1;
		}
		td = data;

		/* Bring it down -8 dbm */
		gain = pow(10.0, (LEVEL - 3.14) / 20.0) * 65536.0 / 2.0;

		td->fac1 = 2.0 * cos(2.0 * M_PI * (freq1 / 8000.0)) * 32768.0;
		td->init_v2_1 = sin(-4.0 * M_PI * (freq1 / 8000.0)) * gain;
		td->init_v3_1 = sin(-2.0 * M_PI * (freq1 / 8000.0)) * gain;
		
		td->fac2 = 2.0 * cos(2.0 * M_PI * (freq2 / 8000.0)) * 32768.0;
		td->init_v2_2 = sin(-4.0 * M_PI * (freq2 / 8000.0)) * gain;
		td->init_v3_2 = sin(-2.0 * M_PI * (freq2 / 8000.0)) * gain;

		td->modulate = modulate;

		data += sizeof(*td);
		used += sizeof(*td);
		size -= sizeof(*td);
		td->tone = t->toneid;
		if (time) {
			/* We should move to the next tone */
			td->next = *count + 1;
			td->samples = time * 8;
		} else {
			/* Stay with us */
			td->next = *count;
			td->samples = 8000;
		}
		*count += 1;
		s = strtok(NULL, ",");
	}
	if (td && time) {
		/* If we don't end on a solid tone, return */
		td->next = firstnobang;
	}
	if (firstnobang < 0)
		fprintf(stderr, "tone '%s' does not end with a solid tone or silence (all tone components have an exclamation mark)\n", t->data);

	return used;
}

char *tone_zone_tone_name(int id)
{
	static char tmp[80];
	switch(id) {
	case DAHDI_TONE_DIALTONE:
		return "Dialtone";
	case DAHDI_TONE_BUSY:
		return "Busy";
	case DAHDI_TONE_RINGTONE:
		return "Ringtone";
	case DAHDI_TONE_CONGESTION:
		return "Congestion";
	case DAHDI_TONE_CALLWAIT:
		return "Call Waiting";
	case DAHDI_TONE_DIALRECALL:
		return "Dial Recall";
	case DAHDI_TONE_RECORDTONE:
		return "Record Tone";
	case DAHDI_TONE_CUST1:
		return "Custom 1";
	case DAHDI_TONE_CUST2:
		return "Custom 2";
	case DAHDI_TONE_INFO:
		return "Special Information";
	case DAHDI_TONE_STUTTER:
		return "Stutter Dialtone";
	default:
		snprintf(tmp, sizeof(tmp), "Unknown tone %d", id);
		return tmp;
	}
}

#ifdef TONEZONE_DRIVER
static void dump_tone_zone(void *data, int size)
{
	struct dahdi_tone_def_header *z;
	struct dahdi_tone_def *td;
	int x;
	int len = sizeof(*z);

	z = data;
	data += sizeof(*z);
	printf("Header: %d tones, %d bytes of data, zone %d (%s)\n", 
		z->count, size, z->zone, z->name);
	for (x = 0; x < z->count; x++) {
		td = data;
		printf("Tone Fragment %d: tone is %d, next is %d, %d samples\n",
			x, td->tone, td->next, td->samples);
		data += sizeof(*td);
		len += sizeof(*td);
	}
	printf("Total measured bytes of data: %d\n", len);
}
#endif

/* Tone frequency tables */
struct mf_tone {
	int	tone;
	float   f1;     /* first freq */
	float   f2;     /* second freq */
};
 
static struct mf_tone dtmf_tones[] = {
	{ DAHDI_TONE_DTMF_0, 941.0, 1336.0 },
	{ DAHDI_TONE_DTMF_1, 697.0, 1209.0 },
	{ DAHDI_TONE_DTMF_2, 697.0, 1336.0 },
	{ DAHDI_TONE_DTMF_3, 697.0, 1477.0 },
	{ DAHDI_TONE_DTMF_4, 770.0, 1209.0 },
	{ DAHDI_TONE_DTMF_5, 770.0, 1336.0 },
	{ DAHDI_TONE_DTMF_6, 770.0, 1477.0 },
	{ DAHDI_TONE_DTMF_7, 852.0, 1209.0 },
	{ DAHDI_TONE_DTMF_8, 852.0, 1336.0 },
	{ DAHDI_TONE_DTMF_9, 852.0, 1477.0 },
	{ DAHDI_TONE_DTMF_s, 941.0, 1209.0 },
	{ DAHDI_TONE_DTMF_p, 941.0, 1477.0 },
	{ DAHDI_TONE_DTMF_A, 697.0, 1633.0 },
	{ DAHDI_TONE_DTMF_B, 770.0, 1633.0 },
	{ DAHDI_TONE_DTMF_C, 852.0, 1633.0 },
	{ DAHDI_TONE_DTMF_D, 941.0, 1633.0 },
	{ 0, 0, 0 }
};
 
static struct mf_tone mfr1_tones[] = {
	{ DAHDI_TONE_MFR1_0, 1300.0, 1500.0 },
	{ DAHDI_TONE_MFR1_1, 700.0, 900.0 },
	{ DAHDI_TONE_MFR1_2, 700.0, 1100.0 },
	{ DAHDI_TONE_MFR1_3, 900.0, 1100.0 },
	{ DAHDI_TONE_MFR1_4, 700.0, 1300.0 },
	{ DAHDI_TONE_MFR1_5, 900.0, 1300.0 },
	{ DAHDI_TONE_MFR1_6, 1100.0, 1300.0 },
	{ DAHDI_TONE_MFR1_7, 700.0, 1500.0 },
	{ DAHDI_TONE_MFR1_8, 900.0, 1500.0 },
	{ DAHDI_TONE_MFR1_9, 1100.0, 1500.0 },
	{ DAHDI_TONE_MFR1_KP, 1100.0, 1700.0 },	/* KP */
	{ DAHDI_TONE_MFR1_ST, 1500.0, 1700.0 },	/* ST */
	{ DAHDI_TONE_MFR1_STP, 900.0, 1700.0 },	/* KP' or ST' */
	{ DAHDI_TONE_MFR1_ST2P, 1300.0, 1700.0 },	/* KP'' or ST'' */ 
	{ DAHDI_TONE_MFR1_ST3P, 700.0, 1700.0 },	/* KP''' or ST''' */
	{ 0, 0, 0 }
};

static struct mf_tone mfr2_fwd_tones[] = {
	{ DAHDI_TONE_MFR2_FWD_1, 1380.0, 1500.0 },
	{ DAHDI_TONE_MFR2_FWD_2, 1380.0, 1620.0 },
	{ DAHDI_TONE_MFR2_FWD_3, 1500.0, 1620.0 },
	{ DAHDI_TONE_MFR2_FWD_4, 1380.0, 1740.0 },
	{ DAHDI_TONE_MFR2_FWD_5, 1500.0, 1740.0 },
	{ DAHDI_TONE_MFR2_FWD_6, 1620.0, 1740.0 },
	{ DAHDI_TONE_MFR2_FWD_7, 1380.0, 1860.0 },
	{ DAHDI_TONE_MFR2_FWD_8, 1500.0, 1860.0 },
	{ DAHDI_TONE_MFR2_FWD_9, 1620.0, 1860.0 },
	{ DAHDI_TONE_MFR2_FWD_10, 1740.0, 1860.0 },
	{ DAHDI_TONE_MFR2_FWD_11, 1380.0, 1980.0 },
	{ DAHDI_TONE_MFR2_FWD_12, 1500.0, 1980.0 },
	{ DAHDI_TONE_MFR2_FWD_13, 1620.0, 1980.0 },
	{ DAHDI_TONE_MFR2_FWD_14, 1740.0, 1980.0 },
	{ DAHDI_TONE_MFR2_FWD_15, 1860.0, 1980.0 },
	{ 0, 0, 0 }
};

static struct mf_tone mfr2_rev_tones[] = {
	{ DAHDI_TONE_MFR2_REV_1, 1020.0, 1140.0 },
	{ DAHDI_TONE_MFR2_REV_2, 900.0, 1140.0 },
	{ DAHDI_TONE_MFR2_REV_3, 900.0, 1020.0 },
	{ DAHDI_TONE_MFR2_REV_4, 780.0, 1140.0 },
	{ DAHDI_TONE_MFR2_REV_5, 780.0, 1020.0 },
	{ DAHDI_TONE_MFR2_REV_6, 780.0, 900.0 },
	{ DAHDI_TONE_MFR2_REV_7, 660.0, 1140.0 },
	{ DAHDI_TONE_MFR2_REV_8, 660.0, 1020.0 },
	{ DAHDI_TONE_MFR2_REV_9, 660.0, 900.0 },
	{ DAHDI_TONE_MFR2_REV_10, 660.0, 780.0 },
	{ DAHDI_TONE_MFR2_REV_11, 540.0, 1140.0 },
	{ DAHDI_TONE_MFR2_REV_12, 540.0, 1020.0 },
	{ DAHDI_TONE_MFR2_REV_13, 540.0, 900.0 },
	{ DAHDI_TONE_MFR2_REV_14, 540.0, 780.0 },
	{ DAHDI_TONE_MFR2_REV_15, 540.0, 660.0 },
	{ 0, 0, 0 }
};

static int build_mf_tones(void *data, size_t size, int *count, struct mf_tone *tone, int low_tone_level, int high_tone_level)
{
	struct dahdi_tone_def *td;
	float gain;
	int used = 0;

	while (tone->tone) {
		if (size < sizeof(*td)) {
			fprintf(stderr, "Not enough space for samples\n");
			return -1;
		}
		td = data;
		data += sizeof(*td);
		used += sizeof(*td);
		size -= sizeof(*td);
		td->tone = tone->tone;
		*count += 1;

		/* Bring it down 6 dBm */
		gain = pow(10.0, (low_tone_level - 3.14) / 20.0) * 65536.0 / 2.0;
		td->fac1 = 2.0 * cos(2.0 * M_PI * (tone->f1 / 8000.0)) * 32768.0;
		td->init_v2_1 = sin(-4.0 * M_PI * (tone->f1 / 8000.0)) * gain;
		td->init_v3_1 = sin(-2.0 * M_PI * (tone->f1 / 8000.0)) * gain;
		
		gain = pow(10.0, (high_tone_level - 3.14) / 20.0) * 65536.0 / 2.0;
		td->fac2 = 2.0 * cos(2.0 * M_PI * (tone->f2 / 8000.0)) * 32768.0;
		td->init_v2_2 = sin(-4.0 * M_PI * (tone->f2 / 8000.0)) * gain;
		td->init_v3_2 = sin(-2.0 * M_PI * (tone->f2 / 8000.0)) * gain;

		tone++;
	}

	return used;
}

int tone_zone_register_zone(int fd, struct tone_zone *z)
{
	char buf[MAX_SIZE];
	int res;
	int count = 0;
	int x;
	size_t space = MAX_SIZE;
	void *ptr = buf;
	int iopenedit = 1;
	struct dahdi_tone_def_header *h;

	memset(buf, 0, sizeof(buf));

	h = ptr;
	ptr += sizeof(*h);
	space -= sizeof(*h);
	h->zone = z->zone;

	dahdi_copy_string(h->name, z->description, sizeof(h->name));

	for (x = 0; x < DAHDI_MAX_CADENCE; x++) 
		h->ringcadence[x] = z->ringcadence[x];

	for (x = 0; x < DAHDI_TONE_MAX; x++) {
		if (!strlen(z->tones[x].data))
			continue;

		PRINT_DEBUG("Tone: %d, string: %s\n", z->tones[x].toneid, z->tones[x].data);

		if ((res = build_tone(ptr, space, &z->tones[x], &count)) < 0) {
			fprintf(stderr, "Tone %d not built.\n", x);
			return -1;
		}
		ptr += res;
		space -= res;
	}

	if ((res = build_mf_tones(ptr, space, &count, dtmf_tones, z->dtmf_low_level, z->dtmf_high_level)) < 0) {
		fprintf(stderr, "Could not build DTMF tones.\n");
		return -1;
	}
	ptr += res;
	space -= res;

	if ((res = build_mf_tones(ptr, space, &count, mfr1_tones, z->mfr1_level, z->mfr1_level)) < 0) {
		fprintf(stderr, "Could not build MFR1 tones.\n");
		return -1;
	}
	ptr += res;
	space -= res;

	if ((res = build_mf_tones(ptr, space, &count, mfr2_fwd_tones, z->mfr2_level, z->mfr2_level)) < 0) {
		fprintf(stderr, "Could not build MFR2 FWD tones.\n");
		return -1;
	}
	ptr += res;
	space -= res;

	if ((res = build_mf_tones(ptr, space, &count, mfr2_rev_tones, z->mfr2_level, z->mfr2_level)) < 0) {
		fprintf(stderr, "Could not build MFR2 REV tones.\n");
		return -1;
	}
	ptr += res;
	space -= res;

	h->count = count;

	if (fd < 0) {
		if ((fd = open(DEFAULT_DAHDI_DEV, O_RDWR)) < 0) {
			fprintf(stderr, "Unable to open %s and fd not provided\n", DEFAULT_DAHDI_DEV);
			return -1;
		}
		iopenedit = 1;
	}

	x = z->zone;
	if ((res = ioctl(fd, DAHDI_FREEZONE, &x))) {
		if (errno != EBUSY)
			fprintf(stderr, "ioctl(DAHDI_FREEZONE) failed: %s\n", strerror(errno));
		return res;
	}

#if defined(TONEZONE_DRIVER)
	dump_tone_zone(h, MAX_SIZE - space);
#endif

#if defined(__FreeBSD__)
	if ((res = ioctl(fd, DAHDI_LOADZONE, &h))) {
#else
	if ((res = ioctl(fd, DAHDI_LOADZONE, h))) {
#endif
		fprintf(stderr, "ioctl(DAHDI_LOADZONE) failed: %s\n", strerror(errno));
		return res;
	}

	if (iopenedit)
		close(fd);

	return res;
}

int tone_zone_register(int fd, char *country)
{
	struct tone_zone *z;
	z = tone_zone_find(country);
	if (z) {
		return tone_zone_register_zone(-1, z);
	} else {
		return -1;
	}
}

int tone_zone_set_zone(int fd, char *country)
{
	int res=-1;
	struct tone_zone *z;
	if (fd > -1) {
		z = tone_zone_find(country);
		if (z)
			res = ioctl(fd, DAHDI_SETTONEZONE, &z->zone);
		if ((res < 0) && (errno == ENODATA)) {
			tone_zone_register_zone(fd, z);
			res = ioctl(fd, DAHDI_SETTONEZONE, &z->zone);
		}
	}
	return res;
}

int tone_zone_get_zone(int fd)
{
	int x=-1;
	if (fd > -1) {
		ioctl(fd, DAHDI_GETTONEZONE, &x);
		return x;
	}
	return -1;
}

int tone_zone_play_tone(int fd, int tone)
{
	struct tone_zone *z;
	int res = -1;
	int zone;

#if 0
	fprintf(stderr, "Playing tone %d (%s) on %d\n", tone, tone_zone_tone_name(tone), fd);
#endif
	if (fd > -1) {
		res = ioctl(fd, DAHDI_SENDTONE, &tone);
		if ((res < 0) && (errno == ENODATA)) {
			ioctl(fd, DAHDI_GETTONEZONE, &zone);
			z = tone_zone_find_by_num(zone);
			if (z) {
				res = tone_zone_register_zone(fd, z);
				/* Recall the zone */
				ioctl(fd, DAHDI_SETTONEZONE, &zone);
				if (res < 0) {
					fprintf(stderr, "Failed to register zone '%s': %s\n", z->description, strerror(errno));
				} else {
					res = ioctl(fd, DAHDI_SENDTONE, &tone);
				}
			} else
				fprintf(stderr, "Don't know anything about zone %d\n", zone);
		}
	}
	return res;
}
