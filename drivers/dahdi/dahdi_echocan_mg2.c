/*
 * ECHO_CAN_MG2
 *
 * by Michael Gernoth
 *
 * Based upon kb1ec.h and mec2.h
 * 
 * Copyright (C) 2002, Digium, Inc.
 *
 * Additional background on the techniques used in this code can be found in:
 *
 *  Messerschmitt, David; Hedberg, David; Cole, Christopher; Haoui, Amine; 
 *  Winship, Peter; "Digital Voice Echo Canceller with a TMS32020," 
 *  in Digital Signal Processing Applications with the TMS320 Family, 
 *  pp. 415-437, Texas Instruments, Inc., 1986. 
 *
 * A pdf of which is available by searching on the document title at http://www.ti.com/
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

static int debug;
static int aggressive;

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)
#define debug_printk(level, fmt, args...) if (debug >= level) printk("%s (%s): " fmt, THIS_MODULE->name, __FUNCTION__, ## args)

#define ABS(a) abs(a!=-32768?a:-32767)

#define RESTORE_COEFFS {\
				int x;\
				memcpy(pvt->a_i, pvt->c_i, pvt->N_d*sizeof(int));\
				for (x = 0; x < pvt->N_d; x++) {\
					pvt->a_s[x] = pvt->a_i[x] >> 16;\
				}\
				pvt->backup = BACKUP;\
			}

/* Uncomment to provide summary statistics for overall echo can performance every 4000 samples */ 
/* #define MEC2_STATS 4000 */

/* Uncomment to generate per-sample statistics - this will severely degrade system performance and audio quality */
/* #define MEC2_STATS_DETAILED */

/* Uncomment to generate per-call DC bias offset messages */
/* #define MEC2_DCBIAS_MESSAGE */

/* Get optimized routines for math */
#include "arith.h"

/*
   Important constants for tuning mg2 echo can
 */

/* Convergence (aka. adaptation) speed -- higher means slower */
#define DEFAULT_BETA1_I 2048

/* Constants for various power computations */
#define DEFAULT_SIGMA_LY_I 7
#define DEFAULT_SIGMA_LU_I 7
#define DEFAULT_ALPHA_ST_I 5 		/* near-end speech detection sensitivity factor */
#define DEFAULT_ALPHA_YT_I 5

#define DEFAULT_CUTOFF_I 128

/* Define the near-end speech hangover counter: if near-end speech 
 *  is declared, hcntr is set equal to hangt (see pg. 432)
 */
#define DEFAULT_HANGT 600  		/* in samples, so 600 samples = 75ms */

/* define the residual error suppression threshold */
#define DEFAULT_SUPPR_I 16		/* 16 = -24db */

/* This is the minimum reference signal power estimate level 
 *  that will result in filter adaptation.
 * If this is too low then background noise will cause the filter 
 *  coefficients to constantly be updated.
 */
#define MIN_UPDATE_THRESH_I 2048

/* The number of samples used to update coefficients using the
 *  the block update method (M). It should be related back to the 
 *  length of the echo can.
 * ie. it only updates coefficients when (sample number MOD default_m) = 0
 *
 *  Getting this wrong may cause an oops. Consider yourself warned!
 */
#define DEFAULT_M 16		  	/* every 16th sample */

/* If AGGRESSIVE supression is enabled, then we start cancelling residual 
 * echos again even while there is potentially the very end of a near-side 
 *  signal present.
 * This defines how many samples of DEFAULT_HANGT can remain before we
 *  kick back in
 */
#define AGGRESSIVE_HCNTR 160		/* in samples, so 160 samples = 20ms */

/* Treat sample as error if it has a different sign as the
 * input signal and is this number larger in ABS() as
 * the input-signal */
#define MAX_SIGN_ERROR 3000

/* Number of coefficients really used for calculating the
 * simulated echo. The value specifies how many of the
 * biggest coefficients are used for calculating rs.
 * This helps on long echo-tails by artificially limiting
 * the number of coefficients for the calculation and
 * preventing overflows.
 * Comment this to deactivate the code */
#define USED_COEFFS 64

/* Backup coefficients every this number of samples */
#define BACKUP 256

/***************************************************************/
/* The following knobs are not implemented in the current code */

/* we need a dynamic level of suppression varying with the ratio of the 
   power of the echo to the power of the reference signal this is 
   done so that we have a  smoother background. 		
   we have a higher suppression when the power ratio is closer to
   suppr_ceil and reduces logarithmically as we approach suppr_floor.
 */
#define SUPPR_FLOOR -64
#define SUPPR_CEIL -24

/* in a second departure, we calculate the residual error suppression
 * as a percentage of the reference signal energy level. The threshold
 * is defined in terms of dB below the reference signal.
 */
#define RES_SUPR_FACTOR -20

#define DC_NORMALIZE

#ifndef NULL
#define NULL 0
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

/* Generic circular buffer definition */
typedef struct {
	/* Pointer to the relative 'start' of the buffer */
	int idx_d;
	/* The absolute size of the buffer */
	int size_d;			 
	/* The actual sample -  twice as large as we need, however we do store values at idx_d and idx_d+size_d */
	short *buf_d;			
} echo_can_cb_s;

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);
static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size);
static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val);
static void echocan_NLP_toggle(struct dahdi_echocan_state *ec, unsigned int enable);
static const char *name = "MG2";
static const char *ec_name(const struct dahdi_chan *chan) { return name; }

static const struct dahdi_echocan_factory my_factory = {
	.get_name = ec_name,
	.owner = THIS_MODULE,
	.echocan_create = echo_can_create,
};

static const struct dahdi_echocan_features my_features = {
	.NLP_toggle = 1,
};

static const struct dahdi_echocan_ops my_ops = {
	.echocan_free = echo_can_free,
	.echocan_process = echo_can_process,
	.echocan_traintap = echo_can_traintap,
	.echocan_NLP_toggle = echocan_NLP_toggle,
};

struct ec_pvt {
	struct dahdi_echocan_state dahdi;
	/* an arbitrary ID for this echo can - this really should be settable from the calling channel... */
	int id;

	/* absolute time - aka. sample number index - essentially the number of samples since this can was init'ed */
	int i_d;
  
	/* Pre-computed constants */
	/* ---------------------- */
	/* Number of filter coefficents */
	int N_d;
	/* Rate of adaptation of filter */
	int beta2_i;

	/* Accumulators for power computations */
	/* ----------------------------------- */
	/* reference signal power estimate - aka. Average absolute value of y(k) */
	int Ly_i;			
	/* ... */
	int Lu_i;

	/* Accumulators for signal detectors */
	/* --------------------------------- */
	/* Power estimate of the recent past of the near-end hybrid signal - aka. Short-time average of: 2 x |s(i)| */
	int s_tilde_i;		
	/* Power estimate of the recent past of the far-end receive signal - aka. Short-time average of:     |y(i)| */
	int y_tilde_i;

	/* Near end speech detection counter - stores Hangover counter time remaining, in samples */
	int HCNTR_d;			
  
	/* Circular buffers and coefficients */
	/* --------------------------------- */
	/* ... */
	int *a_i;
	/* ... */
	short *a_s;
	/* Backups */
	int *b_i;
	int *c_i;
	/* Reference samples of far-end receive signal */
	echo_can_cb_s y_s;
	/* Reference samples of near-end signal */
	echo_can_cb_s s_s;
	/* Reference samples of near-end signal minus echo estimate */
	echo_can_cb_s u_s;
	/* Reference samples of far-end receive signal used to calculate short-time average */
	echo_can_cb_s y_tilde_s;

	/* Peak far-end receive signal */
	/* --------------------------- */
	/* Highest y_tilde value in the sample buffer */
	short max_y_tilde;
	/* Index of the sample containing the max_y_tilde value */
	int max_y_tilde_pos;

#ifdef MEC2_STATS
	/* Storage for performance statistics */
	int cntr_nearend_speech_frames;
	int cntr_residualcorrected_frames;
	int cntr_residualcorrected_framesskipped;
	int cntr_coeff_updates;
	int cntr_coeff_missedupdates;
 
	int avg_Lu_i_toolow; 
	int avg_Lu_i_ok;
#endif 
	unsigned int aggressive:1;
	short lastsig;
	int lastcount;
	int backup;
#ifdef DC_NORMALIZE
	int dc_estimate;
#endif
	int use_nlp;
};

#define dahdi_to_pvt(a) container_of(a, struct ec_pvt, dahdi)

static inline void init_cb_s(echo_can_cb_s *cb, int len, void *where)
{
	cb->buf_d = (short *)where;
	cb->idx_d = 0;
	cb->size_d = len;
}

static inline void add_cc_s(echo_can_cb_s *cb, short newval)
{
	/* Can't use modulus because N+M isn't a power of two (generally) */
	cb->idx_d--;
	if (cb->idx_d < (int)0) 
		/* Whoops - the pointer to the 'start' wrapped around so reset it to the top of the buffer */
	 	cb->idx_d += cb->size_d;
  	
	/* Load two copies into memory */
	cb->buf_d[cb->idx_d] = newval;
	cb->buf_d[cb->idx_d + cb->size_d] = newval;
}

static inline short get_cc_s(echo_can_cb_s *cb, int pos)
{
	/* Load two copies into memory */
	return cb->buf_d[cb->idx_d + pos];
}

static inline void init_cc(struct ec_pvt *pvt, int N, int maxy, int maxu)
{
	char *ptr = (char *) pvt;
	unsigned long tmp;

	/* Double-word align past end of state */
	ptr += sizeof(*pvt);
	tmp = (unsigned long)ptr;
	tmp += 3;
	tmp &= ~3L;
	ptr = (void *)tmp;

	/* Reset parameters */
	pvt->N_d = N;
	pvt->beta2_i = DEFAULT_BETA1_I;
  
	/* Allocate coefficient memory */
	pvt->a_i = (int *) ptr;
	ptr += (sizeof(int) * pvt->N_d);
	pvt->a_s = (short *) ptr;
	ptr += (sizeof(short) * pvt->N_d);

	/* Allocate backup memory */
	pvt->b_i = (int *) ptr;
	ptr += (sizeof(int) * pvt->N_d);
	pvt->c_i = (int *) ptr;
	ptr += (sizeof(int) * pvt->N_d);

	/* Reset Y circular buffer (short version) */
	init_cb_s(&pvt->y_s, maxy, ptr);
	ptr += (sizeof(short) * (maxy) * 2);
  
	/* Reset Sigma circular buffer (short version for FIR filter) */
	init_cb_s(&pvt->s_s, (1 << DEFAULT_ALPHA_ST_I), ptr);
	ptr += (sizeof(short) * (1 << DEFAULT_ALPHA_ST_I) * 2);

	init_cb_s(&pvt->u_s, maxu, ptr);
	ptr += (sizeof(short) * maxu * 2);

	/* Allocate a buffer for the reference signal power computation */
	init_cb_s(&pvt->y_tilde_s, pvt->N_d, ptr);

	/* Reset the absolute time index */
	pvt->i_d = (int)0;
  
	/* Reset the power computations (for y and u) */
	pvt->Ly_i = DEFAULT_CUTOFF_I;
	pvt->Lu_i = DEFAULT_CUTOFF_I;

#ifdef MEC2_STATS
	/* set the identity */
	pvt->id = (int)&ptr;
  
	/* Reset performance stats */
	pvt->cntr_nearend_speech_frames = (int)0;
	pvt->cntr_residualcorrected_frames = (int)0;
	pvt->cntr_residualcorrected_framesskipped = (int)0;
	pvt->cntr_coeff_updates = (int)0;
	pvt->cntr_coeff_missedupdates = (int)0;

	pvt->avg_Lu_i_toolow = (int)0;
	pvt->avg_Lu_i_ok = (int)0;
#endif

	/* Reset the near-end speech detector */
	pvt->s_tilde_i = (int)0;
	pvt->y_tilde_i = (int)0;
	pvt->HCNTR_d = (int)0;

}

static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

#if defined(DC_NORMALIZE) && defined(MEC2_DCBIAS_MESSAGE)
	printk(KERN_INFO "EC: DC bias calculated: %d V\n", pvt->dc_estimate >> 15);
#endif
	kfree(pvt);
}

#ifdef DC_NORMALIZE
static inline short dc_removal(int *dc_estimate, short samp)
{
	*dc_estimate += ((((int)samp << 15) - *dc_estimate) >> 9);
	return samp - (*dc_estimate >> 15);
}
#endif

static inline short sample_update(struct ec_pvt *pvt, short iref, short isig)
{
	/* Declare local variables that are used more than once */
	/* ... */
	int k;
	/* ... */
	int rs;
	/* ... */
	short u;
	/* ... */
	int Py_i;
	/* ... */
	int two_beta_i;

#ifdef DC_NORMALIZE
	isig = dc_removal(&pvt->dc_estimate, isig);
#endif
	
	/* flow A on pg. 428 */
	/* eq. (16): high-pass filter the input to generate the next value;
	 *           push the current value into the circular buffer
	 *
	 * sdc_im1_d = sdc_d;
	 *     sdc_d = sig;
	 *     s_i_d = sdc_d;
	 *       s_d = s_i_d;
	 *     s_i_d = (float)(1.0 - gamma_d) * s_i_d
	 *	+ (float)(0.5 * (1.0 - gamma_d)) * (sdc_d - sdc_im1_d); 
	 */

	/* Update the Far-end receive signal circular buffers and accumulators */
	/* ------------------------------------------------------------------- */
	/* Delete the oldest sample from the power estimate accumulator */
	pvt->y_tilde_i -= abs(get_cc_s(&pvt->y_s, (1 << DEFAULT_ALPHA_YT_I) - 1)) >> DEFAULT_ALPHA_YT_I;
	/* Add the new sample to the power estimate accumulator */
	pvt->y_tilde_i += abs(iref) >> DEFAULT_ALPHA_ST_I;
	/* Push a copy of the new sample into its circular buffer */
	add_cc_s(&pvt->y_s, iref);
 

	/* eq. (2): compute r in fixed-point */
	rs = CONVOLVE2(pvt->a_s,
		       pvt->y_s.buf_d + pvt->y_s.idx_d,
		       pvt->N_d);
	rs >>= 15;

	if (pvt->lastsig == isig) {
		pvt->lastcount++;
	} else {
		pvt->lastcount = 0;
		pvt->lastsig = isig;
	}

	if (isig == 0) {
		u = 0;
	} else if (pvt->lastcount > 255) {
		/* We have seen the same input-signal more than 255 times,
		 * we should pass it through uncancelled, as we are likely on hold */
		u = isig;
	} else {
		int sign_error;

		if (rs < -32768) {
			rs = -32768;
			pvt->HCNTR_d = DEFAULT_HANGT;
			RESTORE_COEFFS;
		} else if (rs > 32767) {
			rs = 32767;
			pvt->HCNTR_d = DEFAULT_HANGT;
			RESTORE_COEFFS;
		}

		sign_error = ABS(rs) - ABS(isig);

		if (ABS(sign_error) > MAX_SIGN_ERROR)
		{
			rs = 0;
			RESTORE_COEFFS;
		}

		/* eq. (3): compute the output value (see figure 3) and the error
		 * note: the error is the same as the output signal when near-end
		 * speech is not present
		 */
		u = isig - rs;

		if (u / isig < 0)
			u = isig - (rs >> 1);
	}

	/* Push a copy of the output value sample into its circular buffer */
	add_cc_s(&pvt->u_s, u);

	if (!pvt->backup) {
		/* Backup coefficients periodically */
		pvt->backup = BACKUP;
		memcpy(pvt->c_i, pvt->b_i, pvt->N_d*sizeof(int));
		memcpy(pvt->b_i, pvt->a_i, pvt->N_d*sizeof(int));
	} else
		pvt->backup--;


	/* Update the Near-end hybrid signal circular buffers and accumulators */
	/* ------------------------------------------------------------------- */
	/* Delete the oldest sample from the power estimate accumulator */
	pvt->s_tilde_i -= abs(get_cc_s(&pvt->s_s, (1 << DEFAULT_ALPHA_ST_I) - 1));
	/* Add the new sample to the power estimate accumulator */
	pvt->s_tilde_i += abs(isig);
	/* Push a copy of the new sample into it's circular buffer */
	add_cc_s(&pvt->s_s, isig);


	/* Push a copy of the current short-time average of the far-end receive signal into it's circular buffer */
	add_cc_s(&pvt->y_tilde_s, pvt->y_tilde_i);

	/* flow B on pg. 428 */
  
	/* If the hangover timer isn't running then compute the new convergence factor, otherwise set Py_i to 32768 */
	if (!pvt->HCNTR_d) {
		Py_i = (pvt->Ly_i >> DEFAULT_SIGMA_LY_I) * (pvt->Ly_i >> DEFAULT_SIGMA_LY_I);
		Py_i >>= 15;
	} else {
	  	Py_i = (1 << 15);
	}
  
#if 0
	/* Vary rate of adaptation depending on position in the file
	 *  Do not do this for the first (DEFAULT_UPDATE_TIME) secs after speech
	 *  has begun of the file to allow the echo cancellor to estimate the
	 *  channel accurately
	 * Still needs conversion!
	 */

	if (pvt->start_speech_d != 0) {
		if (pvt->i_d > (DEFAULT_T0 + pvt->start_speech_d)*(SAMPLE_FREQ)) {
			pvt->beta2_d = max_cc_float(MIN_BETA, DEFAULT_BETA1 * exp((-1/DEFAULT_TAU)*((pvt->i_d/(float)SAMPLE_FREQ) - DEFAULT_T0 - pvt->start_speech_d)));
		}
	} else {
		pvt->beta2_d = DEFAULT_BETA1;
	}
#endif
  
	/* Fixed point, inverted */
	pvt->beta2_i = DEFAULT_BETA1_I;
  
	/* Fixed point version, inverted */
	two_beta_i = (pvt->beta2_i * Py_i) >> 15;
	if (!two_beta_i)
		two_beta_i++;

	/* Update the Suppressed signal power estimate accumulator */
        /* ------------------------------------------------------- */
        /* Delete the oldest sample from the power estimate accumulator */
	pvt->Lu_i -= abs(get_cc_s(&pvt->u_s, (1 << DEFAULT_SIGMA_LU_I) - 1));
        /* Add the new sample to the power estimate accumulator */
	pvt->Lu_i += abs(u);

	/* Update the Far-end reference signal power estimate accumulator */
        /* -------------------------------------------------------------- */
	/* eq. (10): update power estimate of the reference */
        /* Delete the oldest sample from the power estimate accumulator */
	pvt->Ly_i -= abs(get_cc_s(&pvt->y_s, (1 << DEFAULT_SIGMA_LY_I) - 1)) ;
        /* Add the new sample to the power estimate accumulator */
	pvt->Ly_i += abs(iref);

	if (pvt->Ly_i < DEFAULT_CUTOFF_I)
		pvt->Ly_i = DEFAULT_CUTOFF_I;


	/* Update the Peak far-end receive signal detected */
        /* ----------------------------------------------- */
	if (pvt->y_tilde_i > pvt->max_y_tilde) {
		/* New highest y_tilde with full life */
		pvt->max_y_tilde = pvt->y_tilde_i;
		pvt->max_y_tilde_pos = pvt->N_d - 1;
	} else if (--pvt->max_y_tilde_pos < 0) {
		/* Time to find new max y tilde... */
		pvt->max_y_tilde = MAX16(pvt->y_tilde_s.buf_d + pvt->y_tilde_s.idx_d, pvt->N_d, &pvt->max_y_tilde_pos);
	}

	/* Determine if near end speech was detected in this sample */
	/* -------------------------------------------------------- */
	if (((pvt->s_tilde_i >> (DEFAULT_ALPHA_ST_I - 1)) > pvt->max_y_tilde)
	    && (pvt->max_y_tilde > 0))  {
		/* Then start the Hangover counter */
		pvt->HCNTR_d = DEFAULT_HANGT;
		RESTORE_COEFFS;
#ifdef MEC2_STATS_DETAILED
		printk(KERN_INFO "Reset near end speech timer with: s_tilde_i %d, stmnt %d, max_y_tilde %d\n", pvt->s_tilde_i, (pvt->s_tilde_i >> (DEFAULT_ALPHA_ST_I - 1)), pvt->max_y_tilde);
#endif
#ifdef MEC2_STATS
		++pvt->cntr_nearend_speech_frames;
#endif
	} else if (pvt->HCNTR_d > (int)0) {
  		/* otherwise, if it's still non-zero, decrement the Hangover counter by one sample */
#ifdef MEC2_STATS
		++pvt->cntr_nearend_speech_frames;
#endif
		pvt->HCNTR_d--;
	} 

	/* Update coefficients if no near-end speech in this sample (ie. HCNTR_d = 0)
	 * and we have enough signal to bother trying to update.
	 * --------------------------------------------------------------------------
	 */
	if (!pvt->HCNTR_d && 				/* no near-end speech present */
	    !(pvt->i_d % DEFAULT_M)) {		/* we only update on every DEFAULM_M'th sample from the stream */
		if (pvt->Lu_i > MIN_UPDATE_THRESH_I) {	/* there is sufficient energy above the noise floor to contain meaningful data */
  							/* so loop over all the filter coefficients */
#ifdef USED_COEFFS
			int max_coeffs[USED_COEFFS];
			int *pos;

			if (pvt->N_d > USED_COEFFS)
				memset(max_coeffs, 0, USED_COEFFS*sizeof(int));
#endif
#ifdef MEC2_STATS_DETAILED
			printk(KERN_INFO "updating coefficients with: pvt->Lu_i %9d\n", pvt->Lu_i);
#endif
#ifdef MEC2_STATS
			pvt->avg_Lu_i_ok = pvt->avg_Lu_i_ok + pvt->Lu_i;
			++pvt->cntr_coeff_updates;
#endif
			for (k = 0; k < pvt->N_d; k++) {
				/* eq. (7): compute an expectation over M_d samples */
				int grad2;
				grad2 = CONVOLVE2(pvt->u_s.buf_d + pvt->u_s.idx_d,
						  pvt->y_s.buf_d + pvt->y_s.idx_d + k,
						  DEFAULT_M);
				/* eq. (7): update the coefficient */
				pvt->a_i[k] += grad2 / two_beta_i;
				pvt->a_s[k] = pvt->a_i[k] >> 16;

#ifdef USED_COEFFS
				if (pvt->N_d > USED_COEFFS) {
					if (abs(pvt->a_i[k]) > max_coeffs[USED_COEFFS-1]) {
						/* More or less insertion-sort... */
						pos = max_coeffs;
						while (*pos > abs(pvt->a_i[k]))
							pos++;

						if (*pos > max_coeffs[USED_COEFFS-1])
							memmove(pos+1, pos, (USED_COEFFS-(pos-max_coeffs)-1)*sizeof(int));

						*pos = abs(pvt->a_i[k]);
					}
				}
#endif
			}

#ifdef USED_COEFFS
			/* Filter out irrelevant coefficients */
			if (pvt->N_d > USED_COEFFS)
				for (k = 0; k < pvt->N_d; k++)
					if (abs(pvt->a_i[k]) < max_coeffs[USED_COEFFS-1])
						pvt->a_i[k] = pvt->a_s[k] = 0;
#endif
		} else {
#ifdef MEC2_STATS_DETAILED
			printk(KERN_INFO "insufficient signal to update coefficients pvt->Lu_i %5d < %5d\n", pvt->Lu_i, MIN_UPDATE_THRESH_I);
#endif
#ifdef MEC2_STATS
			pvt->avg_Lu_i_toolow = pvt->avg_Lu_i_toolow + pvt->Lu_i;
			++pvt->cntr_coeff_missedupdates;
#endif
		}
	}
  
	/* paragraph below eq. (15): if no near-end speech in the sample and 
	 * the reference signal power estimate > cutoff threshold
	 * then perform residual error suppression
	 */
#ifdef MEC2_STATS_DETAILED
	if (pvt->HCNTR_d == 0)
		printk(KERN_INFO "possibly correcting frame with pvt->Ly_i %9d pvt->Lu_i %9d and expression %d\n", pvt->Ly_i, pvt->Lu_i, (pvt->Ly_i/(pvt->Lu_i + 1)));
#endif

#ifndef NO_ECHO_SUPPRESSOR
	if (pvt->use_nlp) {
		if (pvt->aggressive) {
			if ((pvt->HCNTR_d < AGGRESSIVE_HCNTR) && (pvt->Ly_i > (pvt->Lu_i << 1))) {
				for (k = 0; k < 2; k++) {
					u = u * (pvt->Lu_i >> DEFAULT_SIGMA_LU_I) / ((pvt->Ly_i >> (DEFAULT_SIGMA_LY_I)) + 1);
				}
#ifdef MEC2_STATS_DETAILED
				printk(KERN_INFO "aggresively correcting frame with pvt->Ly_i %9d pvt->Lu_i %9d expression %d\n", pvt->Ly_i, pvt->Lu_i, (pvt->Ly_i/(pvt->Lu_i + 1)));
#endif
#ifdef MEC2_STATS
				++pvt->cntr_residualcorrected_frames;
#endif
			}
		} else {
			if (pvt->HCNTR_d == 0) {
				if ((pvt->Ly_i/(pvt->Lu_i + 1)) > DEFAULT_SUPPR_I) {
					for (k = 0; k < 1; k++) {
						u = u * (pvt->Lu_i >> DEFAULT_SIGMA_LU_I) / ((pvt->Ly_i >> (DEFAULT_SIGMA_LY_I + 2)) + 1);
					}
#ifdef MEC2_STATS_DETAILED
					printk(KERN_INFO "correcting frame with pvt->Ly_i %9d pvt->Lu_i %9d expression %d\n", pvt->Ly_i, pvt->Lu_i, (pvt->Ly_i/(pvt->Lu_i + 1)));
#endif
#ifdef MEC2_STATS
					++pvt->cntr_residualcorrected_frames;
#endif
				}
#ifdef MEC2_STATS
				else {
					++pvt->cntr_residualcorrected_framesskipped;
				}
#endif
			}
		}
	}
#endif  

#if 0
	/* This will generate a non-linear supression factor, once converted */
	if ((pvt->HCNTR_d == 0) &&
		((pvt->Lu_d/pvt->Ly_d) < DEFAULT_SUPPR) &&
		(pvt->Lu_d/pvt->Ly_d > EC_MIN_DB_VALUE)) {
		suppr_factor = (10 / (float)(SUPPR_FLOOR - SUPPR_CEIL)) * log(pvt->Lu_d/pvt->Ly_d)
			- SUPPR_CEIL / (float)(SUPPR_FLOOR - SUPPR_CEIL);
		u_suppr = pow(10.0, (suppr_factor) * RES_SUPR_FACTOR / 10.0) * u_suppr;
	}
#endif  

#ifdef MEC2_STATS
	/* Periodically dump performance stats */
	if ((pvt->i_d % MEC2_STATS) == 0) {
		/* make sure to avoid div0's! */
		if (pvt->cntr_coeff_missedupdates > 0)
			pvt->avg_Lu_i_toolow = (int)(pvt->avg_Lu_i_toolow / pvt->cntr_coeff_missedupdates);
		else
			pvt->avg_Lu_i_toolow = -1;

		if (pvt->cntr_coeff_updates > 0)
			pvt->avg_Lu_i_ok = (pvt->avg_Lu_i_ok / pvt->cntr_coeff_updates);
		else
			pvt->avg_Lu_i_ok = -1;

		printk(KERN_INFO "%d: Near end speech: %5d Residuals corrected/skipped: %5d/%5d Coefficients updated ok/low sig: %3d/%3d Lu_i avg ok/low sig %6d/%5d\n", 
		       pvt->id,
		       pvt->cntr_nearend_speech_frames,
		       pvt->cntr_residualcorrected_frames, pvt->cntr_residualcorrected_framesskipped,
		       pvt->cntr_coeff_updates, pvt->cntr_coeff_missedupdates,
		       pvt->avg_Lu_i_ok, pvt->avg_Lu_i_toolow);

		pvt->cntr_nearend_speech_frames = 0;
		pvt->cntr_residualcorrected_frames = 0;
		pvt->cntr_residualcorrected_framesskipped = 0;
		pvt->cntr_coeff_updates = 0;
		pvt->cntr_coeff_missedupdates = 0;
		pvt->avg_Lu_i_ok = 0;
		pvt->avg_Lu_i_toolow = 0;
	}
#endif

	/* Increment the sample index and return the corrected sample */
	pvt->i_d++;
	return u;
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

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec)
{
	int maxy;
	int maxu;
	size_t size;
	unsigned int x;
	char *c;
	struct ec_pvt *pvt;

	maxy = ecp->tap_length + DEFAULT_M;
	maxu = DEFAULT_M;
	if (maxy < (1 << DEFAULT_ALPHA_YT_I))
		maxy = (1 << DEFAULT_ALPHA_YT_I);
	if (maxy < (1 << DEFAULT_SIGMA_LY_I))
		maxy = (1 << DEFAULT_SIGMA_LY_I);
	if (maxu < (1 << DEFAULT_SIGMA_LU_I))
		maxu = (1 << DEFAULT_SIGMA_LU_I);
	size = sizeof(**ec) +
		4 + 						/* align */
		sizeof(int) * ecp->tap_length +			/* a_i */
		sizeof(short) * ecp->tap_length + 		/* a_s */
		sizeof(int) * ecp->tap_length +			/* b_i */
		sizeof(int) * ecp->tap_length +			/* c_i */
		2 * sizeof(short) * (maxy) +			/* y_s */
		2 * sizeof(short) * (1 << DEFAULT_ALPHA_ST_I) + /* s_s */
		2 * sizeof(short) * (maxu) +			/* u_s */
		2 * sizeof(short) * ecp->tap_length;		/* y_tilde_s */

	pvt = kzalloc(size, GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->dahdi.ops = &my_ops;

	pvt->aggressive = aggressive;
	pvt->dahdi.features = my_features;

	for (x = 0; x < ecp->param_count; x++) {
		for (c = p[x].name; *c; c++)
			*c = tolower(*c);
		if (!strcmp(p[x].name, "aggressive")) {
			pvt->aggressive = p[x].value ? 1 : 0;
		} else {
			printk(KERN_WARNING "Unknown parameter supplied to MG2 echo canceler: '%s'\n", p[x].name);
			kfree(pvt);

			return -EINVAL;
		}
	}

	init_cc(pvt, ecp->tap_length, maxy, maxu);
	/* Non-linear processor - a fancy way to say "zap small signals, to avoid
	   accumulating noise". */
	pvt->use_nlp = TRUE;

	*ec = &pvt->dahdi;
	return 0;
}

static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	/* Set the hangover counter to the length of the can to 
	 * avoid adjustments occuring immediately after initial forced training 
	 */
	pvt->HCNTR_d = pvt->N_d << 1;

	if (pos >= pvt->N_d) {
		memcpy(pvt->b_i, pvt->a_i, pvt->N_d*sizeof(int));
		memcpy(pvt->c_i, pvt->a_i, pvt->N_d*sizeof(int));
		return 1;
	}

	pvt->a_i[pos] = val << 17;
	pvt->a_s[pos] = val << 1;

	if (++pos >= pvt->N_d) {
		memcpy(pvt->b_i, pvt->a_i, pvt->N_d*sizeof(int));
		memcpy(pvt->c_i, pvt->a_i, pvt->N_d*sizeof(int));
		return 1;
	}

	return 0;
}

static void echocan_NLP_toggle(struct dahdi_echocan_state *ec, unsigned int enable)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	pvt->use_nlp = enable ? 1 : 0;
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
module_param(aggressive, int, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("DAHDI 'MG2' Echo Canceler");
MODULE_AUTHOR("Michael Gernoth");
MODULE_LICENSE("GPL v2");

module_init(mod_init);
module_exit(mod_exit);
