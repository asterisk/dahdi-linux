/*
 * wavformat.h -- data structures and associated definitions for wav files
 *
 * By Michael Spiceland (mspiceland@digium.com)
 *
 * (C) 2009 Digium, Inc.
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

#ifndef WAVFORMAT_H
#define WAVFORMAT_H

#include <stdint.h>

struct wavheader {
	/* riff type chunk */
	char riff_chunk_id[4];
	uint32_t riff_chunk_size;
	char riff_type[4];

	/* format chunk */
	char  fmt_chunk_id[4];
	uint32_t  fmt_data_size;
	uint16_t fmt_compression_code;
	uint16_t fmt_num_channels;
	uint32_t  fmt_sample_rate;
	uint32_t  fmt_avg_bytes_per_sec;
	uint16_t fmt_block_align;
	uint16_t fmt_significant_bps;

	/* data chunk */
	char data_chunk_id[4];
	uint32_t data_data_size;
} __attribute__((packed));

#endif
