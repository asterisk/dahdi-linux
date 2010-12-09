/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.c - An echo cancellor, suitable for electrical and acoustic
 *	    cancellation. This code does not currently comply with
 *	    any relevant standards (e.g. G.164/5/7/8). One day....
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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

/* TODO:
   Finish the echo suppressor option, however nasty suppression may be
   Add an option to reintroduce side tone at -24dB under appropriate conditions.
   Improve double talk detector (iterative!)
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

static int debug;

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)
#define debug_printk(level, fmt, args...) if (debug >= level) printk(KERN_DEBUG "%s (%s): " fmt, THIS_MODULE->name, __FUNCTION__, ## args)

#include "fir.h"

#ifndef NULL
#define NULL 0
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define NONUPDATE_DWELL_TIME	600 	/* 600 samples, or 75ms */

/*
 * According to Jim...
 */
#define MIN_TX_POWER_FOR_ADAPTION   512
#define MIN_RX_POWER_FOR_ADAPTION   64

/*
 * According to Steve...
 */
/* #define MIN_TX_POWER_FOR_ADAPTION 4096
#define MIN_RX_POWER_FOR_ADAPTION 64 */

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);
static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size);
static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val);
static const char *name = "SEC2";
static const char *ec_name(const struct dahdi_chan *chan) { return name; }

static const struct dahdi_echocan_factory my_factory = {
	.get_name = ec_name,
	.owner = THIS_MODULE,
	.echocan_create = echo_can_create,
};

static const struct dahdi_echocan_ops my_ops = {
	.echocan_free = echo_can_free,
	.echocan_process = echo_can_process,
	.echocan_traintap = echo_can_traintap,
};

struct ec_pvt {
	struct dahdi_echocan_state dahdi;
	int tx_power;
	int rx_power;
	int clean_rx_power;

	int rx_power_threshold;
	int nonupdate_dwell;

	fir16_state_t fir_state;
	int16_t *fir_taps16;	/* 16-bit version of FIR taps */
	int32_t *fir_taps32;	 /* 32-bit version of FIR taps */

	int curr_pos;

	int taps;
	int tap_mask;
	int use_nlp;
	int use_suppressor;

	int32_t supp_test1;
	int32_t supp_test2;
	int32_t supp1;
	int32_t supp2;

	int32_t latest_correction;  /* Indication of the magnitude of the latest
				       adaption, or a code to indicate why adaption
				       was skipped, for test purposes */
};

#define dahdi_to_pvt(a) container_of(a, struct ec_pvt, dahdi)

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec)
{
	struct ec_pvt *pvt;
	size_t size;

	if (ecp->param_count > 0) {
		printk(KERN_WARNING "SEC2 does not support parameters; failing request\n");
		return -EINVAL;
	}

	size = sizeof(*pvt) + ecp->tap_length * sizeof(int32_t) + ecp->tap_length * 3 * sizeof(int16_t);
	
	pvt = kzalloc(size, GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->dahdi.ops = &my_ops;

	if (ecp->param_count > 0) {
		printk(KERN_WARNING "SEC-2 echo canceler does not support parameters; failing request\n");
		return -EINVAL;
	}

	pvt->taps = ecp->tap_length;
	pvt->curr_pos = ecp->tap_length - 1;
	pvt->tap_mask = ecp->tap_length - 1;
	pvt->fir_taps32 = (int32_t *) (pvt + sizeof(*pvt));
	pvt->fir_taps16 = (int16_t *) (pvt + sizeof(*pvt) + ecp->tap_length * sizeof(int32_t));
	/* Create FIR filter */
	fir16_create(&pvt->fir_state, pvt->fir_taps16, pvt->taps);
	pvt->rx_power_threshold = 10000000;
	pvt->use_suppressor = FALSE;
	/* Non-linear processor - a fancy way to say "zap small signals, to avoid
	   accumulating noise". */
	pvt->use_nlp = FALSE;

	*ec = &pvt->dahdi;
	return 0;
}

static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	fir16_free(&pvt->fir_state);
	kfree(pvt);
}

static inline int16_t sample_update(struct ec_pvt *pvt, int16_t tx, int16_t rx)
{
	int offset1;
	int offset2;
	int32_t echo_value;
	int clean_rx;
	int nsuppr;
	int i;
	int correction;

	/* Evaluate the echo - i.e. apply the FIR filter */
	/* Assume the gain of the FIR does not exceed unity. Exceeding unity
	   would seem like a rather poor thing for an echo cancellor to do :)
	   This means we can compute the result with a total disregard for
	   overflows. 16bits x 16bits -> 31bits, so no overflow can occur in
	   any multiply. While accumulating we may overflow and underflow the
	   32 bit scale often. However, if the gain does not exceed unity,
	   everything should work itself out, and the final result will be
	   OK, without any saturation logic. */
	/* Overflow is very much possible here, and we do nothing about it because
	   of the compute costs */
	/* 16 bit coeffs for the LMS give lousy results (maths good, actual sound
	   bad!), but 32 bit coeffs require some shifting. On balance 32 bit seems
	   best */
	echo_value = fir16 (&pvt->fir_state, tx);

	/* And the answer is..... */
	clean_rx = rx - echo_value;

	/* That was the easy part. Now we need to adapt! */
	if (pvt->nonupdate_dwell > 0)
		pvt->nonupdate_dwell--;

	/* If there is very little being transmitted, any attempt to train is
	   futile. We would either be training on the far end's noise or signal,
	   the channel's own noise, or our noise. Either way, this is hardly good
	   training, so don't do it (avoid trouble). */
	/* If the received power is very low, either we are sending very little or
	   we are already well adapted. There is little point in trying to improve
	   the adaption under these circumstanceson, so don't do it (reduce the
	   compute load). */
	if (pvt->tx_power > MIN_TX_POWER_FOR_ADAPTION && pvt->rx_power > MIN_RX_POWER_FOR_ADAPTION) {
		/* This is a really crude piece of decision logic, but it does OK
		   for now. */
		if (pvt->tx_power > 2*pvt->rx_power) {
			/* There is no far-end speech detected */
			if (pvt->nonupdate_dwell == 0) {
				/* ... and we are not in the dwell time from previous speech. */
				/* nsuppr = saturate((clean_rx << 16)/pvt->tx_power); */
				nsuppr = clean_rx >> 3;

				/* Update the FIR taps */
				offset2 = pvt->curr_pos + 1;
				offset1 = pvt->taps - offset2;
				pvt->latest_correction = 0;
				for (i = pvt->taps - 1; i >= offset1; i--) {
					correction = pvt->fir_state.history[i - offset1]*nsuppr;
					/* Leak to avoid false training on signals with multiple
					   strong correlations. */
					pvt->fir_taps32[i] -= (pvt->fir_taps32[i] >> 12);
					pvt->fir_taps32[i] += correction;
					pvt->fir_state.coeffs[i] = pvt->fir_taps32[i] >> 15;
					pvt->latest_correction += abs(correction);
				}
				for ( ; i >= 0; i--) {
					correction = pvt->fir_state.history[i + offset2]*nsuppr;
					/* Leak to avoid false training on signals with multiple
					   strong correlations. */
					pvt->fir_taps32[i] -= (pvt->fir_taps32[i] >> 12);
					pvt->fir_taps32[i] += correction;
					pvt->fir_state.coeffs[i] = pvt->fir_taps32[i] >> 15;
					pvt->latest_correction += abs(correction);
				}
			} else {
				pvt->latest_correction = -1;
			}
		} else {
			pvt->nonupdate_dwell = NONUPDATE_DWELL_TIME;
			pvt->latest_correction = -2;
		}
	} else {
		pvt->nonupdate_dwell = 0;
		pvt->latest_correction = -3;
	}
	/* Calculate short term power levels using very simple single pole IIRs */
	/* TODO: Is the nasty modulus approach the fastest, or would a real
	   tx*tx power calculation actually be faster? */
	pvt->tx_power += ((abs(tx) - pvt->tx_power) >> 5);
	pvt->rx_power += ((abs(rx) - pvt->rx_power) >> 5);
	pvt->clean_rx_power += ((abs(clean_rx) - pvt->clean_rx_power) >> 5);

#if defined(XYZZY)
	if (pvt->use_suppressor) {
		pvt->supp_test1 += (pvt->fir_state.history[pvt->curr_pos] - pvt->fir_state.history[(pvt->curr_pos - 7) & pvt->tap_mask]);
		pvt->supp_test2 += (pvt->fir_state.history[(pvt->curr_pos - 24) & pvt->tap_mask] - pvt->fir_state.history[(pvt->curr_pos - 31) & pvt->tap_mask]);
		if (pvt->supp_test1 > 42  &&  pvt->supp_test2 > 42)
			supp_change = 25;
		else
			supp_change = 50;
		supp = supp_change + k1*pvt->supp1 + k2*pvt->supp2;
		pvt->supp2 = pvt->supp1;
		pvt->supp1 = supp;
		clean_rx *= (1 - supp);
	}
#endif

	if (pvt->use_nlp  &&  pvt->rx_power < 32)
		clean_rx = 0;

	/* Roll around the rolling buffer */
	if (pvt->curr_pos <= 0)
		pvt->curr_pos = pvt->taps;
	pvt->curr_pos--;

	return clean_rx;
}

static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);
	u32 x;
	short result;

	for (x = 0; x < size; x++) {
		result = sample_update(pvt, *iref, *isig);
		*isig++ = result;
		++iref;
	}
}

static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	/* Reset hang counter to avoid adjustments after
	   initial forced training */
	pvt->nonupdate_dwell = pvt->taps << 1;
	if (pos >= pvt->taps)
		return 1;
	pvt->fir_taps32[pos] = val << 17;
	pvt->fir_taps16[pos] = val << 1;
	if (++pos >= pvt->taps)
		return 1;
	else
		return 0;
}

static int __init mod_init(void)
{
	if (dahdi_register_echocan_factory(&my_factory)) {
		module_printk(KERN_ERR, "could not register with DAHDI core\n");

		return -EPERM;
	}

	module_printk(KERN_NOTICE, "Registered echo canceler '%s'\n",
		      my_factory.get_name(NULL));

	return 0;
}

static void __exit mod_exit(void)
{
	dahdi_unregister_echocan_factory(&my_factory);
}

module_param(debug, int, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("DAHDI 'SEC2' Echo Canceler");
MODULE_AUTHOR("Steve Underwood <steveu@coppice.org>");
MODULE_LICENSE("GPL");

module_init(mod_init);
module_exit(mod_exit);
