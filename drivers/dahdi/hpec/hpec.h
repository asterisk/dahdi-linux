/*
 * DAHDI Telephony Interface to Digium High-Performance Echo Canceller
 *
 * Copyright (C) 2006 Digium, Inc.
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

#if !defined(_HPEC_H)
#define _HPEC_H

struct hpec_state;

void __attribute__((regparm(0))) hpec_init(int __attribute__((regparm(0), format(printf, 1, 2))) (*logger)(const char *format, ...),
					   unsigned int debug,
					   unsigned int chunk_size,
					   void * (*memalloc)(size_t len),
					   void (*memfree)(void *ptr));

void __attribute__((regparm(0))) hpec_shutdown(void);

int __attribute__((regparm(0))) hpec_license_challenge(struct hpec_challenge *challenge);

int __attribute__((regparm(0))) hpec_license_check(struct hpec_license *license);

struct hpec_state __attribute__((regparm(0))) *hpec_channel_alloc(unsigned int len);

void __attribute__((regparm(0))) hpec_channel_free(struct hpec_state *channel);

void __attribute__((regparm(0))) hpec_channel_update(struct hpec_state *channel, short *isig, const short *iref);

#endif /* !defined(_HPEC_H) */

