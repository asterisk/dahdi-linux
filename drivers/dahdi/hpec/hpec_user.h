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

#if !defined(_HPEC_USER_H)
#define _HPEC_USER_H

struct hpec_challenge {
	__u8 challenge[16];
};

struct hpec_license {
	__u32 numchannels;
        __u8 userinfo[256];
	__u8 response[16];
};

#define DAHDI_EC_LICENSE_CHALLENGE _IOR(DAHDI_CODE, 60, struct hpec_challenge)
#define DAHDI_EC_LICENSE_RESPONSE  _IOW(DAHDI_CODE, 61, struct hpec_license)

#endif /* !defined(_HPEC_USER_H) */

