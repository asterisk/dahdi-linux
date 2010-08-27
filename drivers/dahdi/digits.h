/*
 * DAHDI Telephony
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

#ifndef _DIGITS_H
#define _DIGITS_H

#define DEFAULT_DTMF_LENGTH	100 * DAHDI_CHUNKSIZE
#define DEFAULT_MFR1_LENGTH	68 * DAHDI_CHUNKSIZE
#define DEFAULT_MFR2_LENGTH	100 * DAHDI_CHUNKSIZE
#define	PAUSE_LENGTH		500 * DAHDI_CHUNKSIZE

/* At the end of silence, the tone stops */
static struct dahdi_tone dtmf_silence = {
	.tonesamples = DEFAULT_DTMF_LENGTH,
};

/* At the end of silence, the tone stops */
static struct dahdi_tone mfr1_silence = {
	.tonesamples = DEFAULT_MFR1_LENGTH,
};

/* At the end of silence, the tone stops */
static struct dahdi_tone mfr2_silence = {
	.tonesamples = DEFAULT_MFR2_LENGTH,
};

/* A pause in the dialing */
static struct dahdi_tone tone_pause = {
	.tonesamples = PAUSE_LENGTH,
};

#endif 
