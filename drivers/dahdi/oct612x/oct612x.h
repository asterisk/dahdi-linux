/*
 * Octasic OCT6100 Interface
 *
 * Copyright (C) 2013 Digium, Inc.
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
#ifndef __OCT612X_H__
#define __OCT612X_H__

#include <oct6100api/oct6100_api.h>

struct oct612x_context;

/**
 * struct oct612x_ops - Callbacks used by oct612x library to talk to part.
 *
 */
struct oct612x_ops {
	int (*write)(struct oct612x_context *context, u32 address, u16 value);
	int (*read)(struct oct612x_context *context, u32 address, u16 *value);
	int (*write_smear)(struct oct612x_context *context, u32 address,
			   u16 value, size_t count);
	int (*write_burst)(struct oct612x_context *context, u32 address,
			   const u16 *value, size_t count);
	int (*read_burst)(struct oct612x_context *context, u32 address,
			  u16 *value, size_t count);
};

struct oct612x_context {
	const struct oct612x_ops *ops;
	struct device *dev;
};

#endif /* __OCT612X_H__ */
