/*
 * BSD Telephony Of Mexico "Tormenta" Tone Zone Support 2/22/01
 * 
 * Working with the "Tormenta ISA" Card 
 *
 * Copyright (C) 2001-2008, Digium, Inc.
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

#ifndef _TONEZONE_H
#define _TONEZONE_H

#include <dahdi/user.h>

struct tone_zone_sound {
	int toneid;
	char data[256];				/* Actual zone description */
	/* Description is a series of tones of the format:
	   [!]freq1[+freq2][/time] separated by commas.  There
	   are no spaces.  The sequence is repeated back to the 
	   first tone description not preceeded by !.  time is
	   specified in milliseconds */
};

struct tone_zone {
	int zone;				/* Zone number */
	char country[10];			/* Country code */
	char description[40];			/* Description */
	int ringcadence[DAHDI_MAX_CADENCE];	/* Ring cadence */
	struct tone_zone_sound tones[DAHDI_TONE_MAX];
	int dtmf_high_level;			/* Power level of high frequency component
						   of DTMF, expressed in dBm0. */
	int dtmf_low_level;			/* Power level of low frequency component
						   of DTMF, expressed in dBm0. */
	int mfr1_level;				/* Power level of MFR1, expressed in dBm0. */
	int mfr2_level;				/* Power level of MFR2, expressed in dBm0. */
};

extern struct tone_zone builtin_zones[];

/* Register a given two-letter tone zone if we can */
int tone_zone_register(int fd, char *country);

/* Register a given two-letter tone zone if we can */
int tone_zone_register_zone(int fd, struct tone_zone *z);

/* Retrieve a raw tone zone structure */
struct tone_zone *tone_zone_find(char *country);

/* Retrieve a raw tone zone structure by id instead of country*/
struct tone_zone *tone_zone_find_by_num(int id);

/* Retrieve a string name for a given tone id */
char *tone_zone_tone_name(int id);

/* Set a given file descriptor into a given country -- USE THIS
   INTERFACE INSTEAD OF THE IOCTL ITSELF.  Auto-loads tone
   zone if necessary */
int tone_zone_set_zone(int fd, char *country);

/* Get the current tone zone */
int tone_zone_get_zone(int fd);

/* Play a given tone, loading tone zone automatically
   if necessary */
int tone_zone_play_tone(int fd, int toneid);

#endif
