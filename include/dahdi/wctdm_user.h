/*
 * Wildcard S100P FXS Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2001-2008, Digium, Inc.
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

#ifndef _WCTDM_H
#define _WCTDM_H

#include <linux/ioctl.h>

#define NUM_REGS	  109
#define NUM_INDIRECT_REGS 105

struct wctdm_stats {
	int tipvolt;	/* TIP voltage (mV) */
	int ringvolt;	/* RING voltage (mV) */
	int batvolt;	/* VBAT voltage (mV) */
};

struct wctdm_regs {
	unsigned char direct[NUM_REGS];
	unsigned short indirect[NUM_INDIRECT_REGS];
};

struct wctdm_regop {
	int indirect;
	unsigned char reg;
	unsigned short val;
};

struct wctdm_echo_coefs {
	unsigned char acim;
	unsigned char coef1;
	unsigned char coef2;
	unsigned char coef3;
	unsigned char coef4;
	unsigned char coef5;
	unsigned char coef6;
	unsigned char coef7;
	unsigned char coef8;
};

#define WCTDM_GET_STATS	_IOR (DAHDI_CODE, 60, struct wctdm_stats)
#define WCTDM_GET_REGS	_IOR (DAHDI_CODE, 61, struct wctdm_regs)
#define WCTDM_SET_REG	_IOW (DAHDI_CODE, 62, struct wctdm_regop)
#define WCTDM_SET_ECHOTUNE _IOW (DAHDI_CODE, 63, struct wctdm_echo_coefs)


#endif /* _WCTDM_H */
