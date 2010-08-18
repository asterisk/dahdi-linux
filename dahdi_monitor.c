/*
 * Monitor a DAHDI Channel
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001-2008 Digium, Inc.
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

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"
#include "wavformat.h"
#include "autoconfig.h"

#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#else
# ifdef HAVE_LINUX_SOUNDCARD_H
#  include <linux/soundcard.h>
# else
#  error "Your installation appears to be missing soundcard.h which is needed to continue."
# endif
#endif

/*
* defines for file handle numbers
*/
#define MON_BRX		   0	/*!< both channels if multichannel==1 or receive otherwise */
#define MON_TX		   1	/*!< transmit channel */
#define MON_PRE_BRX	   2	/*!< same as MON_BRX but before echo cancellation */
#define MON_PRE_TX	   3	/*!< same as MON_TX but before echo cancellation */
#define MON_STEREO     4	/*!< stereo mix of rx/tx streams */
#define MON_PRE_STEREO 5	/*!< stereo mix of rx/tx before echo can.  This is exactly what is fed into the echo can */

#define BLOCK_SIZE 240

#define BUFFERS 4

#define FRAG_SIZE 8

#define MAX_OFH 6

/* Put the ofh (output file handles) outside the main loop in case we ever add a
 * signal handler.
 */
static FILE *ofh[MAX_OFH];
static int run = 1;

static int stereo;
static int verbose;

/* handler to catch ctrl-c */
void cleanup_and_exit(int signal)
{
	fprintf(stderr, "cntrl-c pressed\n");
	run = 0; /* stop reading */
}

int filename_is_wav(char *filename)
{
	if (NULL != strstr(filename, ".wav"))
		return 1;
	return 0;
}

/*
 * Fill the wav header with default info
 * num_chans - 0 = mono; 1 = stereo
 */
void wavheader_init(struct wavheader *wavheader, int num_chans)
{
	memset(wavheader, 0, sizeof(struct wavheader));

	memcpy(&wavheader->riff_chunk_id, "RIFF", 4);
	memcpy(&wavheader->riff_type, "WAVE", 4);

	memcpy(&wavheader->fmt_chunk_id, "fmt ", 4);
	wavheader->fmt_data_size = 16;
	wavheader->fmt_compression_code = 1;
	wavheader->fmt_num_channels = num_chans;
	wavheader->fmt_sample_rate = 8000;
	wavheader->fmt_avg_bytes_per_sec = 16000;
	wavheader->fmt_block_align = 2;
	wavheader->fmt_significant_bps = 16;

	memcpy(&wavheader->data_chunk_id, "data", 4);
}

int audio_open(void)
{
	int fd;
	int speed = 8000;
	int fmt = AFMT_S16_LE;
	int fragsize = (BUFFERS << 16) | (FRAG_SIZE);
	struct audio_buf_info ispace, ospace;
	fd = open("/dev/dsp", O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open /dev/dsp: %s\n", strerror(errno));
		return -1;
	}
	/* Step 1: Signed linear */
	if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0) {
		fprintf(stderr, "ioctl(SETFMT) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	/* Step 2: Make non-stereo */
	if (ioctl(fd, SNDCTL_DSP_STEREO, &stereo) < 0) {
		fprintf(stderr, "ioctl(STEREO) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (stereo != 0) {
		fprintf(stderr, "Can't turn stereo off :(\n");
	}
	/* Step 3: Make 8000 Hz */
	if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) < 0) {
		fprintf(stderr, "ioctl(SPEED) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (speed != 8000) {
		fprintf(stderr, "Warning: Requested 8000 Hz, got %d\n", speed);
	}
	if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &fragsize)) {
		fprintf(stderr, "Sound card won't let me set fragment size to %u %u-byte buffers (%x)\n"
						"so sound may be choppy: %s.\n", BUFFERS, (1 << FRAG_SIZE), fragsize, strerror(errno));
	}
	bzero(&ispace, sizeof(ispace));
	bzero(&ospace, sizeof(ospace));

	if (ioctl(fd, SNDCTL_DSP_GETISPACE, &ispace)) {
		/* They don't support block size stuff, so just return but notify the user */
		fprintf(stderr, "Sound card won't let me know the input buffering...\n");
	}
	if (ioctl(fd, SNDCTL_DSP_GETOSPACE, &ospace)) {
		/* They don't support block size stuff, so just return but notify the user */
		fprintf(stderr, "Sound card won't let me know the output buffering...\n");
	}
	fprintf(stderr, "New input space:  %d of %d %d byte fragments (%d bytes left)\n",
		ispace.fragments, ispace.fragstotal, ispace.fragsize, ispace.bytes);
	fprintf(stderr, "New output space:  %d of %d %d byte fragments (%d bytes left)\n",
		ospace.fragments, ospace.fragstotal, ospace.fragsize, ospace.bytes);
	return fd;
}

int pseudo_open(void)
{
	int fd;
	int x = 1;
	fd = open("/dev/dahdi/pseudo", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open pseudo channel: %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(fd, DAHDI_SETLINEAR, &x)) {
		fprintf(stderr, "Unable to set linear mode: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	x = BLOCK_SIZE;
	if (ioctl(fd, DAHDI_SET_BLOCKSIZE, &x)) {
		fprintf(stderr, "unable to set sane block size: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

#define barlen 35
#define baroptimal 3250
//define barlevel 200
#define barlevel ((baroptimal/barlen)*2)
#define maxlevel (barlen*barlevel)

void draw_barheader()
{
	char bar[barlen + 4];

	memset(bar, '-', sizeof(bar));
	memset(bar, '<', 1);
	memset(bar + barlen + 2, '>', 1);
	memset(bar + barlen + 3, '\0', 1);

	memcpy(bar + (barlen / 2), "(RX)", 4);
	printf("%s", bar);

	memcpy(bar + (barlen / 2), "(TX)", 4);
	printf(" %s\n", bar);
}

void draw_bar(int avg, int max)
{
	char bar[barlen+5];

	memset(bar, ' ', sizeof(bar));

	max /= barlevel;
	avg /= barlevel;
	if (avg > barlen)
		avg = barlen;
	if (max > barlen)
		max = barlen;

	if (avg > 0)
		memset(bar, '#', avg);
	if (max > 0)
		memset(bar + max, '*', 1);

	bar[barlen+1] = '\0';
	printf("%s", bar);
	fflush(stdout);
}

void visualize(short *tx, short *rx, int cnt)
{
	int x;
	float txavg = 0;
	float rxavg = 0;
	static int txmax = 0;
	static int rxmax = 0;
	static int sametxmax = 0;
	static int samerxmax = 0;
	static int txbest = 0;
	static int rxbest = 0;
	float ms;
	static struct timeval last;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	ms = (tv.tv_sec - last.tv_sec) * 1000.0 + (tv.tv_usec - last.tv_usec) / 1000.0;
	for (x = 0; x < cnt; x++) {
		txavg += abs(tx[x]);
		rxavg += abs(rx[x]);
	}
	txavg = abs(txavg / cnt);
	rxavg = abs(rxavg / cnt);

	if (txavg > txbest)
		txbest = txavg;
	if (rxavg > rxbest)
		rxbest = rxavg;

	/* Update no more than 10 times a second */
	if (ms < 100)
		return;

	/* Save as max levels, if greater */
	if (txbest > txmax) {
		txmax = txbest;
		sametxmax = 0;
	}
	if (rxbest > rxmax) {
		rxmax = rxbest;
		samerxmax = 0;
	}

	memcpy(&last, &tv, sizeof(last));

	/* Clear screen */
	printf("\r ");
	draw_bar(rxbest, rxmax);
	printf("   ");
	draw_bar(txbest, txmax);
	if (verbose)
		printf("   Rx: %5d (%5d) Tx: %5d (%5d)", rxbest, rxmax, txbest, txmax);
	txbest = 0;
	rxbest = 0;

	/* If we have had the same max hits for x times, clear the values */
	sametxmax++;
	samerxmax++;
	if (sametxmax > 6) {
		txmax = 0;
		sametxmax = 0;
	}
	if (samerxmax > 6) {
		rxmax = 0;
		samerxmax = 0;
	}
}

int main(int argc, char *argv[])
{
	int afd = -1;
	int pfd[4] = {-1, -1, -1, -1};
	short buf_brx[BLOCK_SIZE * 2];
	short buf_tx[BLOCK_SIZE * 4];
	short stereobuf[BLOCK_SIZE * 4];
	int res_brx, res_tx;
	int visual = 0;
	int multichannel = 0;
	int ossoutput = 0;
	int preecho = 0;
	int savefile = 0;
	int stereo_output = 0;
	int limit = 0;
	int readcount = 0;
	int x, chan;
	struct dahdi_confinfo zc;
	int opt;
	extern char *optarg;
	struct wavheader wavheaders[MAX_OFH]; /* we have one for each potential filehandle */
	unsigned int bytes_written[MAX_OFH] = {0};
	int file_is_wav[MAX_OFH] = {0};
	int i;

	if ((argc < 2) || (atoi(argv[1]) < 1)) {
		fprintf(stderr, "Usage: dahdi_monitor <channel num> [-v[v]] [-m] [-o] [-l limit] [-f FILE | -s FILE | -r FILE1 -t FILE2] [-F FILE | -S FILE | -R FILE1 -T FILE2]\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "        -v: Visual mode.  Implies -m.\n");
		fprintf(stderr, "        -vv: Visual/Verbose mode.  Implies -m.\n");
		fprintf(stderr, "        -l LIMIT: Stop after reading LIMIT bytes\n");
		fprintf(stderr, "        -m: Separate rx/tx streams.\n");
		fprintf(stderr, "        -o: Output audio via OSS.  Note: Only 'normal' combined rx/tx streams are output via OSS.\n");
		fprintf(stderr, "        -f FILE: Save combined rx/tx stream to mono FILE. Cannot be used with -m.\n");
		fprintf(stderr, "        -r FILE: Save rx stream to FILE. Implies -m.\n");
		fprintf(stderr, "        -t FILE: Save tx stream to FILE. Implies -m.\n");
		fprintf(stderr, "        -s FILE: Save stereo rx/tx stream to FILE. Implies -m.\n");
		fprintf(stderr, "        -F FILE: Save combined pre-echocanceled rx/tx stream to FILE. Cannot be used with -m.\n");
		fprintf(stderr, "        -R FILE: Save pre-echocanceled rx stream to FILE. Implies -m.\n");
		fprintf(stderr, "        -T FILE: Save pre-echocanceled tx stream to FILE. Implies -m.\n");
		fprintf(stderr, "        -S FILE: Save pre-echocanceled stereo rx/tx stream to FILE. Implies -m.\n");
		fprintf(stderr, "Examples:\n");
		fprintf(stderr, "Save a stream to a file\n");
		fprintf(stderr, "        dahdi_monitor 1 -f stream.raw\n");
		fprintf(stderr, "Visualize an rx/tx stream and save them to separate files.\n");
		fprintf(stderr, "        dahdi_monitor 1 -v -r streamrx.raw -t streamtx.raw\n");
		fprintf(stderr, "Play a combined rx/tx stream via OSS and save it to a file\n");
		fprintf(stderr, "        dahdi_monitor 1 -o -f stream.raw\n");
		fprintf(stderr, "Save a combined normal rx/tx stream and a combined 'preecho' rx/tx stream to files\n");
		fprintf(stderr, "        dahdi_monitor 1 -f stream.raw -F streampreecho.raw\n");
		fprintf(stderr, "Save a normal rx/tx stream and a 'preecho' rx/tx stream to separate files\n");
		fprintf(stderr, "        dahdi_monitor 1 -m -r streamrx.raw -t streamtx.raw -R streampreechorx.raw -T streampreechotx.raw\n");
		exit(1);
	}

	chan = atoi(argv[1]);

	while ((opt = getopt(argc, argv, "vmol:f:r:t:s:F:R:T:S:")) != -1) {
		switch (opt) {
		case '?':
			exit(EXIT_FAILURE);
		case 'v':
			if (visual)
				verbose = 1;
			visual = 1;
			multichannel = 1;
			break;
		case 'm':
			multichannel = 1;
			break;
		case 'o':
			ossoutput = 1;
			break;
		case 'l':
			if (sscanf(optarg, "%d", &limit) != 1 || limit < 0)
				limit = 0;
			fprintf(stderr, "Will stop reading after %d bytes\n", limit);
			break;
		case 'f':
			if (multichannel) {
				fprintf(stderr, "'%c' mode cannot be used when multichannel mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_BRX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_BRX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing combined stream to %s\n", optarg);
			file_is_wav[MON_BRX] = filename_is_wav(optarg);
			if (file_is_wav[MON_BRX]) {
				wavheader_init(&wavheaders[MON_BRX], 1);
				if (fwrite(&wavheaders[MON_BRX], 1, sizeof(struct wavheader), ofh[MON_BRX]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			savefile = 1;
			break;
		case 'F':
			if (multichannel) {
				fprintf(stderr, "'%c' mode cannot be used when multichannel mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_PRE_BRX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_PRE_BRX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing pre-echo combined stream to %s\n", optarg);
			preecho = 1;
			savefile = 1;
			break;
		case 'r':
			if (!multichannel && ofh[MON_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_BRX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_BRX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing receive stream to %s\n", optarg);
			file_is_wav[MON_BRX] = filename_is_wav(optarg);
			if (file_is_wav[MON_BRX]) {
				wavheader_init(&wavheaders[MON_BRX], 1);
				if (fwrite(&wavheaders[MON_BRX], 1, sizeof(struct wavheader), ofh[MON_BRX]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			multichannel = 1;
			savefile = 1;
			break;
		case 'R':
			if (!multichannel && ofh[MON_PRE_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_PRE_BRX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_PRE_BRX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing pre-echo receive stream to %s\n", optarg);
			file_is_wav[MON_PRE_BRX] = filename_is_wav(optarg);
			if (file_is_wav[MON_PRE_BRX]) {
				wavheader_init(&wavheaders[MON_PRE_BRX], 1);
				if (fwrite(&wavheaders[MON_PRE_BRX], 1, sizeof(struct wavheader), ofh[MON_PRE_BRX]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			preecho = 1;
			multichannel = 1;
			savefile = 1;
			break;
		case 't':
			if (!multichannel && ofh[MON_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_TX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_TX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing transmit stream to %s\n", optarg);
			file_is_wav[MON_TX] = filename_is_wav(optarg);
			if (file_is_wav[MON_TX]) {
				wavheader_init(&wavheaders[MON_TX], 1);
				if (fwrite(&wavheaders[MON_TX], 1, sizeof(struct wavheader), ofh[MON_TX]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			multichannel = 1;
			savefile = 1;
			break;
		case 'T':
			if (!multichannel && ofh[MON_PRE_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_PRE_TX]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_PRE_TX] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing pre-echo transmit stream to %s\n", optarg);
			file_is_wav[MON_PRE_TX] = filename_is_wav(optarg);
			if (file_is_wav[MON_PRE_TX]) {
				wavheader_init(&wavheaders[MON_PRE_TX], 1);
				if (fwrite(&wavheaders[MON_PRE_TX], 1, sizeof(struct wavheader), ofh[MON_PRE_TX]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			preecho = 1;
			multichannel = 1;
			savefile = 1;
			break;
		case 's':
			if (!multichannel && ofh[MON_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_STEREO]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_STEREO] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing stereo stream to %s\n", optarg);
			file_is_wav[MON_STEREO] = filename_is_wav(optarg);
			if (file_is_wav[MON_STEREO]) {
				wavheader_init(&wavheaders[MON_STEREO], 2);
				if (fwrite(&wavheaders[MON_STEREO], 1, sizeof(struct wavheader), ofh[MON_STEREO]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			multichannel = 1;
			savefile = 1;
			stereo_output = 1;
			break;
		case 'S':
			if (!multichannel && ofh[MON_PRE_BRX]) {
				fprintf(stderr, "'%c' mode cannot be used when combined mode is enabled.\n", opt);
				exit(EXIT_FAILURE);
			}
			if (ofh[MON_PRE_STEREO]) {
				fprintf(stderr, "Cannot specify option '%c' more than once.\n", opt);
				exit(EXIT_FAILURE);
			}
			if ((ofh[MON_PRE_STEREO] = fopen(optarg, "w")) == NULL) {
				fprintf(stderr, "Could not open %s for writing: %s\n", optarg, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "Writing pre-echo stereo stream to %s\n", optarg);
			file_is_wav[MON_PRE_STEREO] = filename_is_wav(optarg);
			if (file_is_wav[MON_PRE_STEREO]) {
				wavheader_init(&wavheaders[MON_PRE_STEREO], 2);
				if (fwrite(&wavheaders[MON_PRE_STEREO], 1, sizeof(struct wavheader), ofh[MON_PRE_STEREO]) != sizeof(struct wavheader)) {
					fprintf(stderr, "Could not write wav header to %s: %s\n", optarg, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			preecho = 1;
			multichannel = 1;
			savefile = 1;
			stereo_output = 1;
			break;
		}
	}

	if (ossoutput) {
		if (multichannel) {
			printf("Multi-channel audio is enabled.  OSS output will be disabled.\n");
			ossoutput = 0;
		} else {
			/* Open audio */
			if ((afd = audio_open()) < 0) {
				printf("Cannot open audio ...\n");
				ossoutput = 0;
			}
		}
	}
	if (!ossoutput && !multichannel && !savefile) {
		fprintf(stderr, "Nothing to do with the stream(s) ...\n");
		exit(1);
	}

	/* Open Pseudo device */
	if ((pfd[MON_BRX] = pseudo_open()) < 0)
		exit(1);
	if (multichannel && ((pfd[MON_TX] = pseudo_open()) < 0))
		exit(1);
	if (preecho) {
		if ((pfd[MON_PRE_BRX] = pseudo_open()) < 0)
			exit(1);
		if (multichannel && ((pfd[MON_PRE_TX] = pseudo_open()) < 0))
			exit(1);
	}
	/* Conference them */
	if (multichannel) {
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		/* Two pseudo's, one for tx, one for rx */
		zc.confmode = DAHDI_CONF_MONITOR;
		if (ioctl(pfd[MON_BRX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		zc.confmode = DAHDI_CONF_MONITORTX;
		if (ioctl(pfd[MON_TX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		if (preecho) {
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			/* Two pseudo's, one for tx, one for rx */
			zc.confmode = DAHDI_CONF_MONITOR_RX_PREECHO;
			if (ioctl(pfd[MON_PRE_BRX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			zc.confmode = DAHDI_CONF_MONITOR_TX_PREECHO;
			if (ioctl(pfd[MON_PRE_TX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
		}
	} else {
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		zc.confmode = DAHDI_CONF_MONITORBOTH;
		if (ioctl(pfd[MON_BRX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		if (preecho) {
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			zc.confmode = DAHDI_CONF_MONITORBOTH_PREECHO;
			if (ioctl(pfd[MON_PRE_BRX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
		}
	}
	if (signal(SIGINT, cleanup_and_exit) == SIG_ERR) {
		fprintf(stderr, "Error registering signal handler: %s\n", strerror(errno));
	}
	if (visual) {
		printf("\nVisual Audio Levels.\n");
		printf("--------------------\n");
		printf(" Use chan_dahdi.conf file to adjust the gains if needed.\n\n");
		printf("( # = Audio Level  * = Max Audio Hit )\n");
		draw_barheader();
	}
	/* Now, copy from pseudo to audio */
	while (run) {
		res_brx = read(pfd[MON_BRX], buf_brx, sizeof(buf_brx));
		if (res_brx < 1)
			break;
		readcount += res_brx;
		if (ofh[MON_BRX])
			bytes_written[MON_BRX] += fwrite(buf_brx, 1, res_brx, ofh[MON_BRX]);

		if (multichannel) {
			res_tx = read(pfd[MON_TX], buf_tx, res_brx);
			if (res_tx < 1)
				break;
			if (ofh[MON_TX])
				bytes_written[MON_TX] += fwrite(buf_tx, 1, res_tx, ofh[MON_TX]);

			if (stereo_output && ofh[MON_STEREO]) {
				for (x = 0; x < res_tx; x++) {
					stereobuf[x*2] = buf_brx[x];
					stereobuf[x*2+1] = buf_tx[x];
				}
				bytes_written[MON_STEREO] += fwrite(stereobuf, 1, res_tx*2, ofh[MON_STEREO]);
			}

			if (visual) {
				if (res_brx == res_tx)
					visualize((short *)buf_tx, (short *)buf_brx, res_brx/2);
				else
					printf("Huh?  res_tx = %d, res_brx = %d?\n", res_tx, res_brx);
			}
		}

		if (preecho) {
			res_brx = read(pfd[MON_PRE_BRX], buf_brx, sizeof(buf_brx));
			if (res_brx < 1)
				break;
			if (ofh[MON_PRE_BRX])
				bytes_written[MON_PRE_BRX] += fwrite(buf_brx, 1, res_brx, ofh[MON_PRE_BRX]);

			if (multichannel) {
				res_tx = read(pfd[MON_PRE_TX], buf_tx, res_brx);
				if (res_tx < 1)
					break;
				if (ofh[MON_PRE_TX])
					bytes_written[MON_PRE_TX] += fwrite(buf_tx, 1, res_tx, ofh[MON_PRE_TX]);

				if (stereo_output && ofh[MON_PRE_STEREO]) {
					for (x = 0; x < res_brx; x++) {
						stereobuf[x*2] = buf_brx[x];
						stereobuf[x*2+1] = buf_tx[x];
					}
					bytes_written[MON_PRE_STEREO] += fwrite(stereobuf, 1, res_brx * 2, ofh[MON_PRE_STEREO]);
				}
			}
		}

		if (ossoutput && afd) {
			if (stereo) {
				for (x = 0; x < res_brx; x++) {
					buf_tx[x << 1] = buf_tx[(x << 1) + 1] = buf_brx[x];
				}
				x = write(afd, buf_tx, res_brx << 1);
			} else {
				x = write(afd, buf_brx, res_brx);
			}
		}

		if (limit && readcount >= limit) {
			/* bail if we've read too much */
			break;
		}
	}
	/* write filesize info */
	for (i = 0; i < MAX_OFH; i++) {
		if (NULL == ofh[i])
			continue;
		if (!(file_is_wav[i]))
			continue;

		rewind(ofh[i]);

		if (fread(&wavheaders[i], 1, sizeof(struct wavheader), ofh[i]) != sizeof(struct wavheader)) {
			fprintf(stderr, "Failed to read in a full wav header.  Expect bad things.\n");
		}

		wavheaders[i].riff_chunk_size = (bytes_written[i]) + sizeof(struct wavheader) - 8; /* filesize - 8 */
		wavheaders[i].data_data_size = bytes_written[i];

		rewind(ofh[i]);
		if (fwrite(&wavheaders[i], 1, sizeof(struct wavheader), ofh[i]) != sizeof(struct wavheader)) {
			fprintf(stderr, "Failed to write out a full wav header.\n");
		}
		fclose(ofh[i]);
	}
	printf("done cleaning up ... exiting.\n");
	return 0;
}
