/* 
 * fxotune.c -- A utility for tuning the various settings on the fxo
 * 		modules for the TDM400 cards.
 *
 * by Matthew Fredrickson <creslin@digium.com>
 * 
 * (C) 2004-2008 Digium, Inc.
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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>

#include <dahdi/user.h>
#include <dahdi/wctdm_user.h>

#include "dahdi_tools_version.h"
#include "fxotune.h"

#define TEST_DURATION 2000
#define BUFFER_LENGTH (2 * TEST_DURATION)
#define SKIP_SAMPLES 800
#define SINE_SAMPLES 8000

static float sintable[SINE_SAMPLES];

static const float amplitude = 16384.0;

static char *dahdipath = "/dev/dahdi";
static char *configfile = "/etc/fxotune.conf";

static int audio_dump_fd = -1;

static int printbest = 0;

#define MAX_RESULTS 	(5)
struct result_catalog {
	int 	idx;
	float 	echo;
	float 	freqres;
	struct wctdm_echo_coefs settings;
};

struct {
	struct result_catalog results[MAX_RESULTS];
	int numactive;
}	topresults;

static char *usage =
"Usage: fxotune [-v[vv] (-s | -i <options> | -d <options>)\n"
"\n"
"	-s : set previously calibrated echo settings\n"
"	-i : calibrate echo settings\n"
"		options : [<dialstring>] [-t <calibtype>]\n"
"		[-b <startdev>][-e <stopdev>]\n"
"		[-n <dialstring>][-l <delaytosilence>][-m <silencegoodfor>]\n"
" 	-d : dump input and output waveforms to ./fxotune_dump.vals\n"
"		options : [-b <device>][-w <waveform>]\n"
"		   [-n <dialstring>][-l <delaytosilence>][-m <silencegoodfor>]\n"
"	-v : more output (-vv, -vvv also)\n"
"	-p : print the 5 best candidates for acim and coefficients settings\n"
"	-x : Perform sin/cos functions using table lookup\n"
"	-o <path> : Write the received raw 16-bit signed linear audio that is\n"
"	            used in processing to the file specified by <path>\n"
"	-c <config_file>\n"
"\n"
"		<calibtype>      - type of calibration\n"
"		                   (default 2, old method 1)\n"
"		<startdev>\n"
"		<stopdev>        - defines a range of devices to test\n"
"		                   (default: 1-252)\n"
"		<dialstring>     - string to dial to clear the line\n"
"		                   (default 5)\n"
"		<delaytosilence> - seconds to wait for line to clear (default 0)\n"
"		<silencegoodfor> - seconds before line will no longer be clear\n"
"		                   (default 18)\n"
"		<device>         - the device to perform waveform dump on\n"
"		                   (default 1)\n"
"		<waveform>       - -1 for multitone waveform, or frequency of\n"
"		                   single tone (default -1)\n"
"		<config_file>    - Alternative file to set from / calibrate to.\n"
"				   (Default: /etc/fxotune.conf)\n"
;


#define OUT_OF_BOUNDS(x) ((x) < 0 || (x) > 255)

struct silence_info{
	char *dialstr;
	/** fd of device we are working with */
	int device; 
	/** seconds we should wait after dialing the dialstring before we know for sure we'll have silence */
	int initial_delay;
	/** seconds after which a reset should occur */
	int reset_after;
	/** time of last reset */
	struct timeval last_reset; 
};

static short outbuf[TEST_DURATION];
static int debug = 0;

static FILE *debugoutfile = NULL;

static int use_table = 0;

static int fxotune_read(int fd, void *buffer, int len)
{
	int res;

	res = read(fd, buffer, len);

	if ((res > 0) && (audio_dump_fd != -1)) {
		res = write(audio_dump_fd, buffer, len);
	}

	return res;
}

/**
 * Makes sure that the line is clear.
 * Right now, we do this by relying on the user to specify how long after dialing the
 * dialstring we can rely on the line being silent (before the telco complains about
 * the user not hitting the next digit).
 * 
 * A more robust way to do this would be to actually measure the sound levels on the line,
 * but that's a lot more complicated, and this should work.
 * 
 * @return 0 if succesful (no errors), 1 if unsuccesful
 */
static int ensure_silence(struct silence_info *info)
{
	struct timeval tv;
	long int elapsedms;
	int x = DAHDI_ONHOOK;
	struct dahdi_dialoperation dop;

	gettimeofday(&tv, NULL);
	
	if (info->last_reset.tv_sec == 0) {
		/* this is the first request, we will force it to run */
		elapsedms = -1;
	} else {
		/* this is not the first request, we will compute elapsed time */
		elapsedms = ((tv.tv_sec - info->last_reset.tv_sec) * 1000L + (tv.tv_usec - info->last_reset.tv_usec) / 1000L);
	}
	if (debug > 4) {
		fprintf(stdout, "Reset line request received - elapsed ms = %li / reset after = %ld\n", elapsedms, info->reset_after * 1000L);
	}

	if (elapsedms > 0 && elapsedms < info->reset_after * 1000L)
		return 0;
	
	if (debug > 1){
		fprintf(stdout, "Resetting line\n");
	}
	
	/* do a line reset */
	/* prepare line for silence */
	/* Do line hookstate reset */

	if (ioctl(info->device, DAHDI_HOOK, &x)) {
		fprintf(stderr, "Unable to hang up fd %d\n", info->device);
		return -1;
	}

	sleep(2);
	x = DAHDI_OFFHOOK;
	if (ioctl(info->device, DAHDI_HOOK, &x)) {
		fprintf(stderr, "Cannot bring fd %d off hook\n", info->device);
		return -1;
	}
	sleep(2); /* Added to ensure that dial can actually takes place */

	memset(&dop, 0, sizeof(dop));
	dop.op = DAHDI_DIAL_OP_REPLACE;
	dop.dialstr[0] = 'T';
	dahdi_copy_string(dop.dialstr + 1, info->dialstr, sizeof(dop.dialstr));


	if (ioctl(info->device, DAHDI_DIAL, &dop)) {
		fprintf(stderr, "Unable to dial!\n");
		return -1;
	}
	sleep(1); 
	sleep(info->initial_delay);  
	
	
	gettimeofday(&info->last_reset, NULL);
	
	
	return 0;
}

/**
 * Generates a tone of specified frequency.
 * 
 * @param hz the frequency of the tone to be generated
 * @param idx the current sample
 * 		to begenerated.  For a normal waveform you need to increment
 * 		this every time you execute the function.
 *
 * @return 16bit slinear sample for the specified index
 */
static short inline gentone(int hz, int idx)
{
	return amplitude * sin((idx * 2.0 * M_PI * hz)/8000);
}

/* Using DTMF tones for now since they provide good mid band testing 
 * while not being harmonics of each other */
static int freqs[] = {697, 770, 941, 1209, 1336, 1633};
static int freqcount = 6;

/**
 * Generates a waveform of several frequencies.
 * 
 * @param idx the current sample
 * 		to begenerated.  For a normal waveform you need to increment
 * 		this every time you execute the function.
 *
 * @return 16bit slinear sample for the specified index
 */
static short inline genwaveform(int idx)
{
	int i = 0;
	float response = (float)0;
	for (i = 0; i < freqcount; i++){
		response += sin((idx * 2.0 * M_PI * freqs[i])/8000);
	}
	

	return amplitude * response / freqcount;
}


/**
 *  Calculates the RMS of the waveform buffer of samples in 16bit slinear format.
 *  prebuf the buffer of either shorts or floats
 *  bufsize the number of elements in the prebuf buffer (not the number of bytes!)
 *  short_format 1 if prebuf points to an array of shorts, 0 if it points to an array of floats
 *  
 *  Formula for RMS (http://en.wikipedia.org/wiki/Root_mean_square): 
 *  
 *  Xrms = sqrt(1/N Sum(x1^2, x2^2, ..., xn^2))
 *  
 *  Note:  this isn't really a power calculation - but it gives a good measure of the level of the response
 *  
 *  @param prebuf the buffer containing the values to compute
 *  @param bufsize the size of the buffer
 *  @param short_format 1 if prebuf contains short values, 0 if it contains float values
 */
static float power_of(void *prebuf, int bufsize, int short_format)
{
	float sum_of_squares = 0;
	int numsamples = 0;
	float finalanswer = 0;
	short *sbuf = (short*)prebuf;
	float *fbuf = (float*)prebuf;
	int i = 0;

	if (short_format) {
		/* idiot proof checks */
		if (bufsize <= 0)
			return -1;

		numsamples = bufsize; /* Got rid of divide by 2 - the bufsize parameter should give the number of samples (that's what it does for the float computation, and it should do it here as well) */

		for (i = 0; i < numsamples; i++) {
			sum_of_squares += ((float)sbuf[i] * (float)sbuf[i]);
		}
	} else {
		/* Version for float inputs */
		for (i = 0; i < bufsize; i++) {
			sum_of_squares += (fbuf[i] * fbuf[i]);
		}
	}

	finalanswer = sum_of_squares/(float)bufsize; /* need to divide by the number of elements in the sample for RMS calc */

	if (finalanswer < 0) {
		fprintf(stderr, "Error: Final answer negative number %f\n", finalanswer);
		return -3;
	}

	return sqrtf(finalanswer);
}

/* 
 * In an effort to eliminate as much as possible the effect of outside noise, we use principles
 * from the Fourier Transform to attempt to calculate the return loss of our signal for each setting.
 *
 * To begin, we send our output signal out on the line.  We then receive back the reflected
 * response.  In the Fourier Transform, each evenly distributed frequency within the window
 * is correlated (multiplied against, then the resulting samples are added together) with
 * the real (cos) and imaginary (sin) portions of that frequency base to detect that frequency.
 * 
 * Instead of doing a complete Fourier Transform, we solve the transform for only our signal
 * by multiplying the received signal by the real and imaginary portions of our reference
 * signal.  This then gives us the real and imaginary values that we can use to calculate
 * the return loss of the sinusoids that we sent out on the line.  This is done by finding
 * the magnitude (think polar form) of the vector resulting from the real and imaginary
 * portions calculated above.
 *
 * This essentially filters out any other noise which maybe present on the line which is outside
 * the frequencies used in our test multi-tone.
 */

void init_sinetable(void)
{
	int i;
	if (debug) {
		fprintf(stdout, "Using sine tables with %d samples\n", SINE_SAMPLES);
	}
	for (i = 0; i < SINE_SAMPLES; i++) {
		sintable[i] = sin(((float)i * 2.0 * M_PI )/(float)(SINE_SAMPLES));
	}
}

/* Sine and cosine table lookup to use periodicity of the calculations being done */
float sin_tbl(int arg, int num_per_period)
{
	arg = arg % num_per_period;

	arg = (arg * SINE_SAMPLES)/num_per_period;

	return sintable[arg];
}

float cos_tbl(int arg, int num_per_period)
{
	arg = arg  % num_per_period;

	arg = (arg * SINE_SAMPLES)/num_per_period;

	arg = (arg + SINE_SAMPLES/4) % SINE_SAMPLES;  /* Pi/2 adjustment */

	return sintable[arg];
}


static float db_loss(float measured, float reference)
{
	return 20 * (logf(measured/reference)/logf(10));
}

static void one_point_dft(const short *inbuf, int len, int frequency, float *real, float *imaginary)
{
	float myreal = 0, myimag = 0;
	int i;

	for (i = 0; i < len; i++) {
		if (use_table) {
			myreal += (float) inbuf[i] * cos_tbl(i*frequency, 8000);
			myimag += (float) inbuf[i] * sin_tbl(i*frequency, 8000);
		} else {
			myreal += (float) inbuf[i] * cos((i * 2.0 * M_PI * frequency)/8000);
			myimag += (float) inbuf[i] * sin((i * 2.0 * M_PI * frequency)/8000);
		}
	}

	myimag *= -1;

	*real = myreal / (float) len;
	*imaginary = myimag / (float) len;
}


static float calc_magnitude(short *inbuf, int insamps)
{
	float real, imaginary, magnitude;
	float totalmagnitude = 0;
	int i;

	for (i = 0; i < freqcount; i++) {
		one_point_dft(inbuf, insamps, freqs[i], &real, &imaginary);
		magnitude = sqrtf((real * real) + (imaginary * imaginary));
		totalmagnitude += magnitude;
	}

	return totalmagnitude;
}


/**
 *  dumps input and output buffer contents for the echo test - used to see exactly what's going on
 */
static int maptone(int whichdahdi, int freq, char *dialstr, int delayuntilsilence)
{
	int i = 0;
	int res = 0, x = 0;
	struct dahdi_bufferinfo bi;
	short inbuf[TEST_DURATION]; /* changed from BUFFER_LENGTH - this buffer is for short values, so it should be allocated using the length of the test */
	FILE *outfile = NULL;
	int leadin = 50;
	int trailout = 100;
	struct silence_info sinfo;
	float power_result;
	float power_waveform;
	float echo;

	outfile = fopen("fxotune_dump.vals", "w");
	if (!outfile) {
		fprintf(stdout, "Cannot create fxotune_dump.vals\n");
		return -1;
	}

	x = 1;
	if (ioctl(whichdahdi, DAHDI_SETLINEAR, &x)) {
		fprintf(stderr, "Unable to set channel to signed linear mode.\n");
		return -1;
	}

	memset(&bi, 0, sizeof(bi));
	if (ioctl(whichdahdi, DAHDI_GET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to get buffer information!\n");
		return -1;
	}
	bi.numbufs = 2;
	bi.bufsize = TEST_DURATION; /* KD - changed from BUFFER_LENGTH; */
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	if (ioctl(whichdahdi, DAHDI_SET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to set buffer information!\n");
		return -1;
	}

	/* Fill the output buffers */
	for (i = 0; i < leadin; i++)
		outbuf[i] = 0;
	for (; i < TEST_DURATION - trailout; i++){
		outbuf[i] = freq > 0 ? gentone(freq, i) : genwaveform(i); /* if frequency is negative, use a multi-part waveform instead of a single frequency */
	}
	for (; i < TEST_DURATION; i++)
		outbuf[i] = 0;

	/* Make sure the line is clear */
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.device = whichdahdi;
	sinfo.dialstr = dialstr;
	sinfo.initial_delay = delayuntilsilence;
	sinfo.reset_after = 4; /* doesn't matter - we are only running one test */
	
	if (ensure_silence(&sinfo)){
		fprintf(stderr, "Unable to get a clear outside line\n");
		return -1;
	}

	/* Flush buffers */
	x = DAHDI_FLUSH_READ | DAHDI_FLUSH_WRITE | DAHDI_FLUSH_EVENT;
	if (ioctl(whichdahdi, DAHDI_FLUSH, &x)) {
		fprintf(stderr, "Unable to flush I/O: %s\n", strerror(errno));
		return -1;
	}

	/* send data out on line */
	res = write(whichdahdi, outbuf, BUFFER_LENGTH); /* we are sending a TEST_DURATION length array of shorts (which are 2 bytes each) */
	if (res != BUFFER_LENGTH) { 
		fprintf(stderr, "Could not write all data to line\n");
		return -1;
	}

retry:
		/* read return response */
	res = fxotune_read(whichdahdi, inbuf, BUFFER_LENGTH);
	if (res != BUFFER_LENGTH) {
		int dummy;

		ioctl(whichdahdi, DAHDI_GETEVENT, &dummy);
		goto retry;
	}

	/* write content of output buffer to debug file */
	power_result = power_of(inbuf, TEST_DURATION, 1);
	power_waveform = power_of(outbuf, TEST_DURATION, 1);
	echo = power_result/power_waveform;
	
	fprintf(outfile, "Buffers, freq=%d, outpower=%0.0f, echo=%0.4f\n", freq, power_result, echo);
	fprintf(outfile, "Sample, Input (received from the line), Output (sent to the line)\n");
	for (i = 0; i < TEST_DURATION; i++){
		fprintf(outfile, "%d, %d, %d\n", 
			i,
			inbuf[i],
			outbuf[i]
		);
	}

	fclose(outfile);
	
	fprintf(stdout, "echo ratio = %0.4f (%0.1f / %0.1f)\n", echo, power_result, power_waveform);
	
	return 0;
}


/**
 *  Initialize the data store for storing off best calculated results
 */
static void init_topresults(void)
{
	topresults.numactive = 0;
}


/**
 *  If this is a best result candidate, store in the top results data store
 * 		This is dependent on being the lowest echo value
 *
 *  @param tbleoffset - The offset into the echo_trys table used
 *  @param setting - Pointer to the settings used to achieve the fgiven value
 *  @param echo - The calculated echo return value (in dB)
 *  @param echo - The calculated magnitude of the response
 */
static void set_topresults(int tbloffset, struct wctdm_echo_coefs *setting, float echo, float freqres)
{
	int place;
	int idx;

	for ( place = 0; place < MAX_RESULTS && place < topresults.numactive; place++) {
		if (echo < topresults.results[place].echo) {
			break;
		}
	}

	if (place < MAX_RESULTS) {
		/*  move results to the bottom */
		for (idx = topresults.numactive-2; idx >= place; idx--) {
			topresults.results[idx+1] = topresults.results[idx];
		}
		topresults.results[place].idx = tbloffset;
		topresults.results[place].settings = *setting;
		topresults.results[place].echo = echo;
		topresults.results[place].freqres = freqres;
		if (MAX_RESULTS > topresults.numactive) {
			topresults.numactive++;
		}
	}
}


/**
 *  Prints the top results stored to stdout
 *
 *  @param header - Text that goes in the header of the response
 */
static void print_topresults(char * header)
{
	int item;

	fprintf(stdout, "Top %d results for %s\n", topresults.numactive, header);
	for (item = 0; item < topresults.numactive; item++) {
		fprintf(stdout, "Res #%d: index=%d, %3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d: magnitude = %0.0f, echo = %0.4f dB\n",
				item+1, topresults.results[item].idx, topresults.results[item].settings.acim,
				topresults.results[item].settings.coef1, topresults.results[item].settings.coef2,
				topresults.results[item].settings.coef3, topresults.results[item].settings.coef4,
				topresults.results[item].settings.coef5, topresults.results[item].settings.coef6,
				topresults.results[item].settings.coef7, topresults.results[item].settings.coef8,
				topresults.results[item].freqres, topresults.results[item].echo);
		
	}
}


/**
 * Perform calibration type 2 on the specified device
 * 
 * Determine optimum echo coefficients for the specified device
 * 
 * New tuning strategy.  If we have a number that we can dial that will result in silence from the
 * switch, the tune will be *much* faster (we don't have to keep hanging up and dialing a digit, etc...)
 * The downside is that the user needs to actually find a 'no tone' phone number at their CO's switch - but for
 * really fixing echo problems, this is what it takes.
 *
 * Also, for the purposes of optimizing settings, if we pick a single frequency and test with that,
 * we can try a whole bunch of impedence/echo coefficients.  This should give better results than trying
 * a bunch of frequencies, and we can always do a a frequency sweep to pick between the best 3 or 4
 * impedence/coefficients configurations.
 *   
 * Note:  It may be possible to take this even further and do some pertubation analysis on the echo coefficients
 * 		 themselves (maybe use the 72 entry sweep to find some settings that are close to working well, then
 * 		 deviate the coefficients a bit to see if we can improve things).  A better way to do this would be to
 * 		 use the optimization strategy from silabs.  For reference, here is an application note that describes
 * 		 the echo coefficients (and acim values):
 * 		 
 * 		 http://www.silabs.com/Support%20Documents/TechnicalDocs/an84.pdf
 *
 * 		 See Table 13 in this document for a breakdown of acim values by region.
 *
 * 		 http://www.silabs.com/Support%20Documents/TechnicalDocs/si3050-18-19.pdf
 * 		 
 */
static int acim_tune2(int whichdahdi, int freq, char *dialstr, int delayuntilsilence, int silencegoodfor, struct wctdm_echo_coefs *coefs_out)
{
	int i = 0;
	int res = 0, x = 0;
	int lowesttry = -1;
	float lowesttryresult = 999999999999.0;
	float lowestecho = 999999999999.0;
	struct dahdi_bufferinfo bi;
	short inbuf[TEST_DURATION * 2];
	struct silence_info sinfo;
	int echo_trys_size = 72;
	int trys = 0;
	float waveform_power;
	float freq_result;
	float echo;

	init_topresults();

	if (debug && !debugoutfile) {
		if (!(debugoutfile = fopen("fxotune.vals", "w"))) {
			fprintf(stdout, "Cannot create fxotune.vals\n");
			return -1;
		}
	}

	/* Set echo settings */
	if (ioctl(whichdahdi, WCTDM_SET_ECHOTUNE, &echo_trys[0])) {
		fprintf(stderr, "Unable to set impedance on fd %d\n", whichdahdi);
		return -1;
	}

	x = 1;
	if (ioctl(whichdahdi, DAHDI_SETLINEAR, &x)) {
		fprintf(stderr, "Unable to set channel to signed linear mode.\n");
		return -1;
	}

	memset(&bi, 0, sizeof(bi));
	if (ioctl(whichdahdi, DAHDI_GET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to get buffer information!\n");
		return -1;
	}
	bi.numbufs = 2;
	bi.bufsize = BUFFER_LENGTH;
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	if (ioctl(whichdahdi, DAHDI_SET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to set buffer information!\n");
		return -1;
	}
	x = DAHDI_OFFHOOK;
	if (ioctl(whichdahdi, DAHDI_HOOK, &x)) {
		fprintf(stderr, "Cannot bring fd %d off hook", whichdahdi);
		return -1;
	}


	/* Set up silence settings */
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.device = whichdahdi;
	sinfo.dialstr = dialstr;
	sinfo.initial_delay = delayuntilsilence;
	sinfo.reset_after = silencegoodfor;

	/* Fill the output buffers */
	for (i = 0; i < TEST_DURATION; i++)
		outbuf[i] = freq > 0 ? gentone(freq, i) : genwaveform(i); /* if freq is negative, use a multi-frequency waveform */
	
	/* compute power of input (so we can later compute echo levels relative to input) */
	waveform_power = calc_magnitude(outbuf, TEST_DURATION);

	/* sweep through the various coefficient settings and see how our responses look */

	for (trys = 0; trys < echo_trys_size; trys++){
		
		/* ensure silence on the line */
		if (ensure_silence(&sinfo)){
			fprintf(stderr, "Unable to get a clear outside line\n");
			return -1;
		}
		
		if (ioctl(whichdahdi, WCTDM_SET_ECHOTUNE, &echo_trys[trys])) {
			fprintf(stderr, "Unable to set echo coefficients on fd %d\n", whichdahdi);
			return -1;
		}

		/* Flush buffers */
		x = DAHDI_FLUSH_READ | DAHDI_FLUSH_WRITE | DAHDI_FLUSH_EVENT;
		if (ioctl(whichdahdi, DAHDI_FLUSH, &x)) {
			fprintf(stderr, "Unable to flush I/O: %s\n", strerror(errno));
			return -1;
		}

		/* send data out on line */
		res = write(whichdahdi, outbuf, BUFFER_LENGTH);
		if (res != BUFFER_LENGTH) {
			fprintf(stderr, "Could not write all data to line\n");
			return -1;
		}

retry:
		/* read return response */
		res = fxotune_read(whichdahdi, inbuf, BUFFER_LENGTH * 2);
		if (res != BUFFER_LENGTH * 2) {
			int dummy;

			ioctl(whichdahdi, DAHDI_GETEVENT, &dummy);
			goto retry;
		}

		freq_result = calc_magnitude(inbuf, TEST_DURATION * 2);
		echo = db_loss(freq_result, waveform_power);
		
#if 0
		if (debug > 0)
			fprintf(stdout, "%3d,%d,%d,%d,%d,%d,%d,%d,%d: magnitude = %0.0f, echo = %0.4f dB\n", 
					echo_trys[trys].acim, echo_trys[trys].coef1, echo_trys[trys].coef2,
					echo_trys[trys].coef3, echo_trys[trys].coef4, echo_trys[trys].coef5,
					echo_trys[trys].coef6, echo_trys[trys].coef7, echo_trys[trys].coef8,
					freq_result, echo);
#endif

		if (freq_result < lowesttryresult){
			lowesttry = trys;
			lowesttryresult = freq_result;
			lowestecho = echo;
		}
		if (debug) {
			char result[256];
			snprintf(result, sizeof(result), "%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%f,%f", 
						echo_trys[trys].acim, 
						echo_trys[trys].coef1, 
						echo_trys[trys].coef2, 
						echo_trys[trys].coef3, 
						echo_trys[trys].coef4, 
						echo_trys[trys].coef5, 
						echo_trys[trys].coef6, 
						echo_trys[trys].coef7, 
						echo_trys[trys].coef8, 
						freq_result,
						echo
					);
			
			fprintf(debugoutfile, "%s\n", result);
			fprintf(stdout, "%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d: magnitude = %0.0f, echo = %0.4f dB\n",
					echo_trys[trys].acim, echo_trys[trys].coef1, echo_trys[trys].coef2,
					echo_trys[trys].coef3, echo_trys[trys].coef4, echo_trys[trys].coef5,
					echo_trys[trys].coef6, echo_trys[trys].coef7, echo_trys[trys].coef8,
					freq_result, echo);
		}

		if (printbest) {
			set_topresults(trys, &echo_trys[trys], echo, freq_result);
		}
	}

	if (debug > 0)
		fprintf(stdout, "Config with lowest response = %d, magnitude = %0.0f, echo = %0.4f dB\n", lowesttry, lowesttryresult, lowestecho);

	memcpy(coefs_out, &echo_trys[lowesttry], sizeof(struct wctdm_echo_coefs));
	if (printbest) {
		print_topresults("Acim2_tune Test");
	}

	return 0;
}

/**
 *  Perform calibration type 1 on the specified device.  Only tunes the line impedance.  Look for best response range 
 */
static int acim_tune(int whichdahdi, char *dialstr, int delayuntilsilence, int silencegoodfor, struct wctdm_echo_coefs *coefs_out)
{
	int i = 0, freq = 0, acim = 0;
	int res = 0, x = 0;
	struct dahdi_bufferinfo bi;
	struct wctdm_echo_coefs coefs;
	short inbuf[TEST_DURATION]; /* changed from BUFFER_LENGTH - this buffer is for short values, so it should be allocated using the length of the test */
	int lowest = 0;
	FILE *outfile = NULL;
	float acim_results[16];
	struct silence_info sinfo;

	if (debug) {
		outfile = fopen("fxotune.vals", "w");
		if (!outfile) {
			fprintf(stdout, "Cannot create fxotune.vals\n");
			return -1;
		}
	}

	/* Set up silence settings */
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.device = whichdahdi;
	sinfo.dialstr = dialstr;
	sinfo.initial_delay = delayuntilsilence;
	sinfo.reset_after = silencegoodfor;
	
	/* Set echo settings */
	memset(&coefs, 0, sizeof(coefs));
	if (ioctl(whichdahdi, WCTDM_SET_ECHOTUNE, &coefs)) {
		fprintf(stdout, "Skipping non-TDM / non-FXO\n");
		return -1;
	}

	x = 1;
	if (ioctl(whichdahdi, DAHDI_SETLINEAR, &x)) {
		fprintf(stderr, "Unable to set channel to signed linear mode.\n");
		return -1;
	}

	memset(&bi, 0, sizeof(bi));
	if (ioctl(whichdahdi, DAHDI_GET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to get buffer information!\n");
		return -1;
	}
	bi.numbufs = 2;
	bi.bufsize = BUFFER_LENGTH;
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	if (ioctl(whichdahdi, DAHDI_SET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to set buffer information!\n");
		return -1;
	}

	for (acim = 0; acim < 16; acim++) {
		float freq_results[15];

		coefs.acim = acim;
		if (ioctl(whichdahdi, WCTDM_SET_ECHOTUNE, &coefs)) {
			fprintf(stderr, "Unable to set impedance on fd %d\n", whichdahdi);
			return -1;
		}

		for (freq = 200; freq <=3000; freq+=200) {
			/* Fill the output buffers */
			for (i = 0; i < TEST_DURATION; i++)
				outbuf[i] = gentone(freq, i);

			/* Make sure line is ready for next test iteration */
			if (ensure_silence(&sinfo)){
				fprintf(stderr, "Unable to get a clear line\n");
				return -1;
			}
			

			/* Flush buffers */
			x = DAHDI_FLUSH_READ | DAHDI_FLUSH_WRITE | DAHDI_FLUSH_EVENT;
			if (ioctl(whichdahdi, DAHDI_FLUSH, &x)) {
				fprintf(stderr, "Unable to flush I/O: %s\n", strerror(errno));
				return -1;
			}
	
			/* send data out on line */
			res = write(whichdahdi, outbuf, BUFFER_LENGTH);
			if (res != BUFFER_LENGTH) {
				fprintf(stderr, "Could not write all data to line\n");
				return -1;
			}

			/* read return response */
retry:
			/* read return response */
			res = fxotune_read(whichdahdi, inbuf, BUFFER_LENGTH);
			if (res != BUFFER_LENGTH) {
				int dummy;
	
				ioctl(whichdahdi, DAHDI_GETEVENT, &dummy);
				goto retry;
			}

			/* calculate power of response */
			
			freq_results[(freq/200)-1] = power_of(inbuf+SKIP_SAMPLES, TEST_DURATION-SKIP_SAMPLES, 1); /* changed from inbuf+SKIP_BYTES, BUFFER_LENGTH-SKIP_BYTES, 1 */
			if (debug) fprintf(outfile, "%d,%d,%f\n", acim, freq, freq_results[(freq/200)-1]);
		}
		acim_results[acim] = power_of(freq_results, 15, 0);
	}

	if (debug) {
		for (i = 0; i < 16; i++)
			fprintf(outfile, "acim_results[%d] = %f\n", i, acim_results[i]);
	}
	/* Find out what the "best" impedance is for the line */
	lowest = 0;
	for (i = 0; i < 16; i++) {
		if (acim_results[i] < acim_results[lowest]) {
			lowest = i;
		}
	}

	coefs_out->acim = lowest;
	coefs_out->coef1 = 0;
	coefs_out->coef2 = 0;
	coefs_out->coef3 = 0;
	coefs_out->coef4 = 0;
	coefs_out->coef5 = 0;
	coefs_out->coef6 = 0;
	coefs_out->coef7 = 0;
	coefs_out->coef8 = 0;
	
	return 0;
}

/**
 * Reads echo register settings from the configuration file and pushes them into
 * the appropriate devices
 * 
 * @param configfilename the path of the file that the calibration results should be written to
 * 
 * @return 0 if successful, !0 otherwise
 */	
static int do_set(char *configfilename)
{
	FILE *fp = NULL;
	int res = 0;
	int fd = 0;
		
	fp = fopen(configfile, "r");
	
    if (!fp) {
            fprintf(stdout, "Cannot open %s!\n",configfile);
            return -1;
    }

	
	while (res != EOF) {
		struct wctdm_echo_coefs mycoefs;
		char completedahdipath[56] = "";
		int mydahdi,myacim,mycoef1,mycoef2,mycoef3,mycoef4,mycoef5,mycoef6,mycoef7,mycoef8;


		res = fscanf(fp, "%d=%d,%d,%d,%d,%d,%d,%d,%d,%d",&mydahdi,&myacim,&mycoef1,
				&mycoef2,&mycoef3,&mycoef4,&mycoef5,&mycoef6,&mycoef7,
				&mycoef8);

		if (res == EOF) {
			break;
		}

		/* Check to be sure conversion is done correctly */
		if (OUT_OF_BOUNDS(myacim) || OUT_OF_BOUNDS(mycoef1)||
			OUT_OF_BOUNDS(mycoef2)|| OUT_OF_BOUNDS(mycoef3)||
			OUT_OF_BOUNDS(mycoef4)|| OUT_OF_BOUNDS(mycoef5)||
			OUT_OF_BOUNDS(mycoef6)|| OUT_OF_BOUNDS(mycoef7)|| OUT_OF_BOUNDS(mycoef8)) {

			fprintf(stdout, "Bounds check error on inputs from %s:%d\n", configfile, mydahdi);
			return -1;
		}

		mycoefs.acim = myacim;
		mycoefs.coef1 = mycoef1;
		mycoefs.coef2 = mycoef2;
		mycoefs.coef3 = mycoef3;
		mycoefs.coef4 = mycoef4;
		mycoefs.coef5 = mycoef5;
		mycoefs.coef6 = mycoef6;
		mycoefs.coef7 = mycoef7;
		mycoefs.coef8 = mycoef8;
	
		snprintf(completedahdipath, sizeof(completedahdipath), "%s/%d", dahdipath, mydahdi);
		fd = open(completedahdipath, O_RDWR);

		if (fd < 0) {
			fprintf(stdout, "open error on %s: %s\n", completedahdipath, strerror(errno));
			return -1;
		}

		if (ioctl(fd, WCTDM_SET_ECHOTUNE, &mycoefs)) {
			fprintf(stdout, "%s: %s\n", completedahdipath, strerror(errno));
			return -1;
		}

		close(fd);
	}

	fclose(fp);

	if (debug)
		fprintf(stdout, "fxotune: successfully set echo coeffecients on FXO modules\n");
	return 0;	
}

/**
 * Output waveform information from a single test
 * 
 * Clears the line, then sends a single waveform (multi-tone, or single tone), and listens
 * for the response on the line.  Output is written to fxotune_dump.vals
 * 
 * @param startdev the device to test
 * @param dialstr the string that should be dialed to clear the dialtone from the line
 * @param delayuntilsilence the number of seconds to wait after dialing dialstr before starting the test
 * @param silencegoodfor the number of seconds that the test can run before having to reset the line again
 * 			(this is basically the amount of time it takes before the 'if you'd like to make a call...' message
 * 			kicks in after you dial dialstr.  This test is so short that the value is pretty much ignored.
 * @param waveformtype the type of waveform to use - -1 = multi-tone waveform, otherwise the specified value
 * 			is used as the frequency of a single tone.  A value of 0 will output silence.
 */
static int do_dump(int startdev, char* dialstr, int delayuntilsilence, int silencegoodfor, int waveformtype)
{
	int res = 0;
	int fd;
	char dahdidev[80] = "";
	
	int dahdimodule = startdev;
	snprintf(dahdidev, sizeof(dahdidev), "%s/%d", dahdipath, dahdimodule);

	fd = open(dahdidev, O_RDWR);
	if (fd < 0) {
		fprintf(stdout, "%s absent: %s\n", dahdidev, strerror(errno));
		return -1;
	}

	fprintf(stdout, "Dumping module %s\n", dahdidev);
	res = maptone(fd, waveformtype, dialstr, delayuntilsilence); 

	close(fd);

	if (res) {
		fprintf(stdout, "Failure!\n");
		return res;
	} else {
		fprintf(stdout, "Done!\n");
		return 0;
	}

}	

/**
 * Performs calibration on all specified devices
 * 
 * @param startdev the first device to check
 * @param enddev the last device to check
 * @param calibtype the type of calibration to perform.  1=old style (loops through individual frequencies
 * 			doesn't optimize echo coefficients.  2=new style (uses multi-tone and optimizes echo coefficients
 * 			and acim setting)
 * @param configfilename the path of the file that the calibration results should be written to
 * @param dialstr the string that should be dialed to clear the dialtone from the line
 * @param delayuntilsilence the number of seconds to wait after dialing dialstr before starting the test
 * @param silencegoodfor the number of seconds that the test can run before having to reset the line again
 * 			(this is basically the amount of time it takes before the 'if you'd like to make a call...' message
 * 			kicks in after you dial dialstr
 * 
 * @return 0 if successful, -1 for serious error such as device not available , > 0 indicates the number of channels
 */	
static int do_calibrate(int startdev, int enddev, int calibtype, char* configfilename, char* dialstr, int delayuntilsilence, int silencegoodfor)
{
	int problems = 0;
	int res = 0;
	int configfd, fd;
	int devno = 0;
	char dahdidev[80] = "";
	struct wctdm_echo_coefs coefs;
	
	configfd = open(configfile, O_CREAT|O_TRUNC|O_WRONLY, 0666);

	if (configfd < 0) {
		fprintf(stderr, "Cannot generate config file %s: open: %s\n", configfile, strerror(errno));
		return -1;
	}

	for (devno = startdev; devno <= enddev; devno++) {
		snprintf(dahdidev, sizeof(dahdidev), "%s/%d", dahdipath, devno);

		fd = open(dahdidev, O_RDWR);
		if (fd < 0) {
			fprintf(stdout, "%s absent: %s\n", dahdidev, strerror(errno));
			continue;
		}

		fprintf(stdout, "Tuning module %s\n", dahdidev);
		
		if (1 == calibtype)
			res = acim_tune(fd, dialstr, delayuntilsilence, silencegoodfor, &coefs);
		else
			res = acim_tune2(fd, -1, dialstr, delayuntilsilence, silencegoodfor, &coefs);

		close(fd);
		
		if (res) {
			fprintf(stdout, "Failure!\n");
			problems++;
		} else {
			fprintf(stdout, "Done!\n");
		}

		if (res == 0) {
			
		/* Do output to file */
			int len = 0;
			static char output[255] = "";

			snprintf(output, sizeof(output), "%d=%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
				devno,
				coefs.acim, 
				coefs.coef1, 
				coefs.coef2, 
				coefs.coef3, 
				coefs.coef4, 
				coefs.coef5, 
				coefs.coef6, 
				coefs.coef7, 
				coefs.coef8
			);

			if (debug)
				fprintf(stdout, "Found best echo coefficients: %s\n", output);

			len = strlen(output);
			res = write(configfd, output, strlen(output));
			if (res != len) {
				fprintf(stdout, "Unable to write line \"%s\" to file.\n", output);
				return -1;
			}
		}
	}

	close(configfd);
	
	if (problems)
		fprintf(stdout, "Unable to tune %d devices, even though those devices are present\n", problems);
	
	return problems;
}	
	
int main(int argc , char **argv)
{
	int startdev = 1; /* -b */
	int stopdev = 252; /* -e */
	int calibtype = 2; /* -t */
	int waveformtype = -1; /* -w multi-tone by default.  If > 0, single tone of specified frequency */
	int delaytosilence = 0; /* -l */
	int silencegoodfor = 18; /* -m */
	char* dialstr = "5"; /* -n */
	int res = 0;
	int doset = 0; /* -s */
	int docalibrate = 0; /* -i <dialstr> */
	int dodump = 0; /* -d */
	int i = 0;
	int moreargs;
	
	for (i = 1; i < argc; i++){
		if (!(argv[i][0] == '-' || argv[i][0] == '/') || (strlen(argv[i]) <= 1)){
			fprintf(stdout, "Unknown option : %s\n", argv[i]);
			/* Show usage */
			fputs(usage, stdout);
			return -1;
		}

		moreargs = (i < argc - 1);
		
		switch(argv[i][1]){
			case 's':
				doset=1;
				continue;
			case 'i':
				docalibrate = 1;
				if (moreargs){ /* we need to check for a value after 'i' for backwards compatability with command line options of old fxotune */
					if (argv[i+1][0] != '-' && argv[i+1][0] != '/')
						dialstr = argv[++i];
				}
				continue;
			case 'c':
				configfile = moreargs ? argv[++i] : configfile;
				continue;
			case 'd':
				dodump = 1;
				continue;
			case 'b':
				startdev = moreargs ? atoi(argv[++i]) : startdev;
				break;
			case 'e':
				stopdev = moreargs ? atoi(argv[++i]) : stopdev;
				break;
			case 't':
				calibtype = moreargs ? atoi(argv[++i]) : calibtype;
				break;
			case 'w':
				waveformtype = moreargs ? atoi(argv[++i]) : waveformtype;
				break;
			case 'l':
				delaytosilence = moreargs ? atoi(argv[++i]) : delaytosilence;
				break;
			case 'm':
				silencegoodfor = moreargs ? atoi(argv[++i]) : silencegoodfor;
				break;
			case 'n':
				dialstr = moreargs ? argv[++i] : dialstr;
				break;
			case 'p':
				printbest++;
				break;
			case 'x':
				use_table = 1;
				break;
			case 'v':
				debug = strlen(argv[i])-1;
				break;
			case 'o':
				if (moreargs) {
					audio_dump_fd = open(argv[++i], O_WRONLY|O_CREAT|O_TRUNC, 0666);
					if (audio_dump_fd == -1) {
						fprintf(stdout, "Unable to open file %s: %s\n", argv[i], strerror(errno));
						return -1;
					}
					break;
				} else {
					fprintf(stdout, "No path supplied to -o option!\n");
					return -1;
				}
			default:
				fprintf(stdout, "Unknown option : %s\n", argv[i]);
				/* Show usage */
				fputs(usage, stdout);
				return -1;
				
		}
	}
	
	if (debug > 3){
		fprintf(stdout, "Running with parameters:\n");
		fprintf(stdout, "\tdoset=%d\n", doset);	
		fprintf(stdout, "\tdocalibrate=%d\n", docalibrate);	
		fprintf(stdout, "\tdodump=%d\n", dodump);	
		fprintf(stdout, "\tprint best settings=%d\n", printbest);
		fprintf(stdout, "\tstartdev=%d\n", startdev);
		fprintf(stdout, "\tstopdev=%d\n", stopdev);	
		fprintf(stdout, "\tcalibtype=%d\n", calibtype);	
		fprintf(stdout, "\twaveformtype=%d\n", waveformtype);	
		fprintf(stdout, "\tdelaytosilence=%d\n", delaytosilence);	
		fprintf(stdout, "\tsilencegoodfor=%d\n", silencegoodfor);	
		fprintf(stdout, "\tdialstr=%s\n", dialstr);	
		fprintf(stdout, "\tdebug=%d\n", debug);	
	}

	if(use_table) {
		init_sinetable();
	}
	
	if (docalibrate){
		res = do_calibrate(startdev, stopdev, calibtype, configfile, dialstr, delaytosilence, silencegoodfor);
		if (!res)
			return do_set(configfile);	
		else
			return -1;
	}

	if (doset)
		return do_set(configfile);
				
	if (dodump){
		res = do_dump(startdev, dialstr, delaytosilence, silencegoodfor, waveformtype);
		if (!res)
			return 0;
		else
			return -1;
	}

	fputs(usage, stdout);
	return -1;
}
