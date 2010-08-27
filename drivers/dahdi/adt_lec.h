/*
 * ADT Line Echo Canceller Parameter Parsing
 *
 * Copyright (C) 2008 Digium, Inc.
 *
 * Kevin P. Fleming <kpfleming@digium.com>
 *
 * All rights reserved.
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

#ifndef _ADT_LEC_H
#define _ADT_LEC_H

enum adt_lec_nlp_type {
	ADT_LEC_NLP_OFF = 0,
	ADT_LEC_NLP_MUTE,
	ADT_LEC_RANDOM_NOISE,
	ADT_LEC_HOTH_NOISE,
	ADT_LEC_SUPPRESS,
};

enum adt_companding {
	ADT_COMP_ULAW = 0,
	ADT_COMP_ALAW,
};

struct adt_lec_params {
	__u32 tap_length;
	enum adt_lec_nlp_type nlp_type;
	__u32 nlp_threshold;
	__u32 nlp_max_suppress;
	enum adt_companding companding;
};

#endif /* _ADT_LEC_H */
