/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ec_disable_detector.h - A detector which should eventually meet the
 *                         G.164/G.165 requirements for detecting the
 *                         2100Hz echo cancellor disable tone.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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

#include "biquad.h"

#define FALSE 0
#define TRUE (!FALSE)

static inline void echo_can_disable_detector_init (echo_can_disable_detector_state_t *det)
{
    /* Elliptic notch */
    /* This is actually centred at 2095Hz, but gets the balance we want, due
       to the asymmetric walls of the notch */
    biquad2_init (&det->notch,
    	     	  (int32_t) (-0.7600000*32768.0),
    	    	  (int32_t) (-0.1183852*32768.0),
    	    	  (int32_t) (-0.5104039*32768.0),
    	    	  (int32_t) ( 0.1567596*32768.0),
    	    	  (int32_t) ( 1.0000000*32768.0));

    det->channel_level = 0;
    det->notch_level = 0;    
    det->tone_present = FALSE;
    det->tone_cycle_duration = 0;
    det->good_cycles = 0;
    det->hit = 0;
}
/*- End of function --------------------------------------------------------*/

static inline int echo_can_disable_detector_update (echo_can_disable_detector_state_t *det,
						    int16_t amp)
{
	int16_t notched;

    	notched = biquad2 (&det->notch, amp);
    	/* Estimate the overall energy in the channel, and the energy in
	   the notch (i.e. overall channel energy - tone energy => noise).
	   Use abs instead of multiply for speed (is it really faster?).
	   Damp the overall energy a little more for a stable result.
	   Damp the notch energy a little less, so we don't damp out the
	   blip every time the phase reverses */
        det->channel_level += ((abs(amp) - det->channel_level) >> 5);
	det->notch_level += ((abs(notched) - det->notch_level) >> 4);
	if (det->channel_level >= 70) {
		/* There is adequate energy in the channel. Is it mostly at 2100Hz? */
		if (det->notch_level*6 < det->channel_level) {
			det->tone_cycle_duration++;
			/* The notch says yes, so we have the tone. */
			if (!det->tone_present) {
				/* Do we get a kick every 450+-25ms? */
				if ((det->tone_cycle_duration >= (425 * 8)) &&
				    (det->tone_cycle_duration <= (475 * 8))) {
					/* It's ANS/PR (CED with polarity reversals), so wait
					   for at least three cycles before returning a hit. */
					det->good_cycles++;
					if (det->good_cycles > 2)
						det->hit = TRUE;
				}
				det->tone_cycle_duration = 0;
				det->tone_present = TRUE;
			} else if (det->tone_cycle_duration >= 600 * 8) {
				/* It's ANS (CED without polarity reversals)
				   so return a hit. */
				det->hit = TRUE;
			}
		} else {
			det->tone_present = FALSE;
		}
	} else {
		det->tone_present = FALSE;
		det->tone_cycle_duration = 0;
		det->good_cycles = 0;
	}

	return  det->hit;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
