/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fir.h - General telephony FIR routines
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2002 Steve Underwood
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

#if !defined(_FIR_H_)
#define _FIR_H_

typedef struct
{
    int taps;
    int curr_pos;
    int16_t *coeffs;
    int16_t *history;
} fir16_state_t;

typedef struct
{
    int taps;
    int curr_pos;
    int32_t *coeffs;
    int16_t *history;
} fir32_state_t;

static inline void fir16_create (fir16_state_t *fir,
			         int16_t *coeffs,
    	    	    	         int taps)
{
    fir->taps = taps;
    fir->curr_pos = taps - 1;
    fir->coeffs = coeffs;
    fir->history = kmalloc(taps*sizeof (int16_t), GFP_KERNEL);
    if (fir->history)
        memset (fir->history, '\0', taps*sizeof (int16_t));
}
/*- End of function --------------------------------------------------------*/
    
static inline void fir16_free (fir16_state_t *fir)
{
    kfree(fir->history);
}
/*- End of function --------------------------------------------------------*/
    
static inline int16_t fir16 (fir16_state_t *fir, int16_t sample)
{
    int i;
    int offset1;
    int offset2;
    int32_t y;

    fir->history[fir->curr_pos] = sample;
    offset2 = fir->curr_pos + 1;
    offset1 = fir->taps - offset2;
    y = 0;
    for (i = fir->taps - 1;  i >= offset1;  i--)
        y += fir->coeffs[i]*fir->history[i - offset1];
    for (  ;  i >= 0;  i--)
        y += fir->coeffs[i]*fir->history[i + offset2];
    if (fir->curr_pos <= 0)
    	fir->curr_pos = fir->taps;
    fir->curr_pos--;
    return  y >> 15;
}
/*- End of function --------------------------------------------------------*/

static inline void fir32_create (fir32_state_t *fir,
			         int32_t *coeffs,
    	    	    	         int taps)
{
    fir->taps = taps;
    fir->curr_pos = taps - 1;
    fir->coeffs = coeffs;
    fir->history = kmalloc(taps*sizeof (int16_t), GFP_KERNEL);
    if (fir->history)
    	memset (fir->history, '\0', taps*sizeof (int16_t));
}
/*- End of function --------------------------------------------------------*/
    
static inline void fir32_free (fir32_state_t *fir)
{
    kfree(fir->history);
}
/*- End of function --------------------------------------------------------*/
    
static inline int16_t fir32 (fir32_state_t *fir, int16_t sample)
{
    int i;
    int offset1;
    int offset2;
    int32_t y;

    fir->history[fir->curr_pos] = sample;
    offset2 = fir->curr_pos + 1;
    offset1 = fir->taps - offset2;
    y = 0;
    for (i = fir->taps - 1;  i >= offset1;  i--)
        y += fir->coeffs[i]*fir->history[i - offset1];
    for (  ;  i >= 0;  i--)
        y += fir->coeffs[i]*fir->history[i + offset2];
    if (fir->curr_pos <= 0)
    	fir->curr_pos = fir->taps;
    fir->curr_pos--;
    return  y >> 15;
}
/*- End of function --------------------------------------------------------*/

#endif
/*- End of file ------------------------------------------------------------*/
