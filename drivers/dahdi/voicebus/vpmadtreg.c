/*
 * vpmadtreg.c - Registration utility for firmware loaders.
 *
 * Allows drivers for boards that host VPMAD032 modules to initiate firmware
 * loads.
 *
 * Written by Digium Incorporated <support@digium.com>
 *
 * Copyright (C) 2008-2009 Digium, Inc.  All rights reserved.
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

#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/list.h>

#include "voicebus.h"
#include "GpakCust.h"
#include "vpmadtreg.h"

