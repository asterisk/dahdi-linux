/*
 * SpanDSP - a series of DSP components for telephony
 *
 * biquad.h - General telephony bi-quad section routines (currently this just
 *            handles canonic/type 2 form)
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

static inline void biquad2_init (biquad2_state_t *bq,
                	 	 int32_t gain,
		                 int32_t a1,
            	                 int32_t a2,
		                 int32_t b1,
		                 int32_t b2)
{
    bq->gain = gain;
    bq->a1 = a1;
    bq->a2 = a2;
    bq->b1 = b1;
    bq->b2 = b2;
    
    bq->z1 = 0;
    bq->z2 = 0;    
}
/*- End of function --------------------------------------------------------*/
    
static inline int16_t biquad2 (biquad2_state_t *bq, int16_t sample)
{
    int32_t y;
    int32_t z0;
    
    z0 = sample*bq->gain + bq->z1*bq->a1 + bq->z2*bq->a2;
    y = z0 + bq->z1*bq->b1 + bq->z2*bq->b2;

    bq->z2 = bq->z1;
    bq->z1 = z0 >> 15;
    y >>= 15;
    return  y;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
