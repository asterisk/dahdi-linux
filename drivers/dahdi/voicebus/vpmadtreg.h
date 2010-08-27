/*
 * vpmadtreg.h - Registration utility for firmware loaders.
 *
 * Allows drivers for boards that host VPMAD032 modules to initiate firmware
 * loads.
 *
 * Written by Digium Incorporated <support@digium.com>
 *
 * Copyright (C) 2008-2010 Digium, Inc.  All rights reserved.
 *
 * See http://www.asterisk.org for more information about the Asterisk
 * project. Please do not directly contact any of the maintainers of this
 * project for assistance; the project provides a web site, mailing lists and
 * IRC channels for your use.
 *
 * This program is free software, distributed under the terms of the GNU
 * General Public License Version 2 as published by the Free Software
 * Foundation. See the LICENSE file included with this program for more
 * details.
 */
#ifndef __VPMADTREG_H__
#define __VPMADTREG_H__

struct vpmadt_loader {
	struct module *owner;
	struct list_head node;
	int (*load)(struct voicebus *);
};

int vpmadtreg_register(struct vpmadt_loader *loader);
int vpmadtreg_unregister(struct vpmadt_loader *loader);
int vpmadtreg_loadfirmware(struct voicebus *vb);
#endif
