/*
 * B400M  Quad-BRI module Driver
 * Written by Andrew Kohlsmith <akohlsmith@mixdown.ca>
 *
 * Copyright (C) 2010 Digium, Inc.
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


#ifndef _B4XXM_H_
#define _B4XXM_H_

extern int bri_debug;
extern int bri_spanfilter;
extern int bri_teignorered;
extern int bri_alarmdebounce;
extern int bri_persistentlayer1;
extern int timingcable;

struct b400m;

/* probes the given card to see if it's a B400M */
int wctdm_init_b400m(struct wctdm *wc, int card);
void wctdm_bri_checkisr(struct wctdm *wc,
			struct wctdm_module *const mod, int offset);
void wctdm_unload_b400m(struct wctdm *wc, int card);
void wctdm_hdlc_hard_xmit(struct dahdi_chan *chan);
int b400m_spanconfig(struct file *file, struct dahdi_span *span,
		     struct dahdi_lineconfig *lc);
int b400m_dchan(struct dahdi_span *span);
int b400m_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype);
void b400m_post_init(struct b400m *b4);
void b400m_set_dahdi_span(struct b400m *b4, int spanno,
			  struct wctdm_span *wspan);
void b400m_module_init(void);
void b400m_module_cleanup(void);

#endif	/* _B4XX_H_ */
