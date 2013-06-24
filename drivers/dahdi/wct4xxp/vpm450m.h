/*
 * Copyright (C) 2005-2006 Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * All Rights Reserved
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

#ifndef _VPM450M_H
#define _VPM450M_H

#include <linux/firmware.h>

struct t4;
struct vpm450m;

/* From driver */
unsigned int oct_get_reg(void *data, unsigned int reg);
void oct_set_reg(void *data, unsigned int reg, unsigned int val);

/* From vpm450m */
struct vpm450m *init_vpm450m(struct device *device, int *isalaw,
			     int numspans, const struct firmware *firmware);
unsigned int get_vpm450m_capacity(struct device *device);
void vpm450m_setec(struct vpm450m *instance, int channel, int eclen);
void vpm450m_setdtmf(struct vpm450m *instance, int channel, int dtmfdetect, int dtmfmute);
int vpm450m_checkirq(struct vpm450m *vpm450m);
int vpm450m_getdtmf(struct vpm450m *vpm450m, int *channel, int *tone, int *start);
void release_vpm450m(struct vpm450m *instance);
void vpm450m_set_alaw_companding(struct vpm450m *vpm450m,
				 int channel, bool alaw);

extern int debug;

#endif
