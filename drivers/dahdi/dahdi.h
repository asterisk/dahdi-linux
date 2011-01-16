#ifndef _DAHDI_H
#define _DAHDI_H

/* dahdi.h: headers intended only for the dahdi.ko module.
 * Not to be included elsewhere
 *
 * Written by Tzafrir Cohen <tzafrir.cohen@xorcom.com>
 * Copyright (C) 2011, Xorcom
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

extern int debug;

int dahdi_register_chardev(struct dahdi_chardev *dev);
int dahdi_unregister_chardev(struct dahdi_chardev *dev);
int span_sysfs_create(struct dahdi_span *span);
void span_sysfs_remove(struct dahdi_span *span);
int __init dahdi_sysfs_init(const struct file_operations *dahdi_fops);
void dahdi_sysfs_exit(void);

#endif /* _DAHDI_H */
