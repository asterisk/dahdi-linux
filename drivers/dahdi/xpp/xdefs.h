#ifndef	XDEFS_H
#define	XDEFS_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef	__KERNEL__

#include <linux/kernel.h>
#include <linux/version.h>

#else

/* This is to enable user-space programs to include this. */

#include <stdint.h>
typedef uint8_t __u8;
typedef uint32_t __u32;

#include <stdio.h>

#define	DBG(fmt, ...)		printf("DBG: %s: " fmt, __func__, ## __VA_ARGS__)
#define	INFO(fmt, ...)		printf("INFO: " fmt, ## __VA_ARGS__)
#define	NOTICE(fmt, ...)	printf("NOTICE: " fmt, ## __VA_ARGS__)
#define	ERR(fmt, ...)		printf("ERR: " fmt, ## __VA_ARGS__)
#define	__user

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#endif

#define	PACKED	__attribute__((packed))

#define	ALL_LINES		((lineno_t)-1)

#ifndef	BIT			/* added in 2.6.24 */
#define	BIT(i)		(1UL << (i))
#endif
#define	BIT_SET(x, i)	((x) |= BIT(i))
#define	BIT_CLR(x, i)	((x) &= ~BIT(i))
#define	IS_SET(x, i)	(((x) & BIT(i)) != 0)
#define	BITMASK(i)	(((u64)1 << (i)) - 1)

#define	MAX_PROC_WRITE	100	/* Longest write allowed to our /proc files */
#define	CHANNELS_PERXPD	32	/* Depends on xpp_line_t and protocol fields */

#define	MAX_SPANNAME	20	/* From dahdi/kernel.h */
#define	MAX_SPANDESC	40	/* From dahdi/kernel.h */
#define	MAX_CHANNAME	40	/* From dahdi/kernel.h */

#define	XPD_NAMELEN	10	/* must be <= from maximal workqueue name */
#define	XPD_DESCLEN	20
#define	XBUS_NAMELEN	20	/* must be <= from maximal workqueue name */
#define	XBUS_DESCLEN	40
#define	LABEL_SIZE	20

#define	UNIT_BITS	3	/* Bit for Astribank unit number */
#define	SUBUNIT_BITS	3	/* Bit for Astribank subunit number */

/* 1 FXS + 3 FXS/FXO | 1 BRI + 3 FXS/FXO */
#define	MAX_UNIT	(1 << UNIT_BITS)
#define	MAX_SUBUNIT	(1 << SUBUNIT_BITS)	/* 8 port BRI */

/*
 * Compile time sanity checks
 */
#if MAX_UNIT > BIT(UNIT_BITS)
#error "MAX_UNIT too large"
#endif

#if MAX_SUBUNIT > BIT(SUBUNIT_BITS)
#error "MAX_SUBUNIT too large"
#endif

#define	MAX_XPDS		(MAX_UNIT*MAX_SUBUNIT)

#define	VALID_XPD_NUM(x)	((x) < MAX_XPDS && (x) >= 0)

#define	CHAN_BITS		5	/* 0-31 for E1 */

typedef char *charp;
#ifdef __KERNEL__

/* Kernel versions... */
#define	KMEM_CACHE_T	struct kmem_cache

#define	KZALLOC(size, gfp)	my_kzalloc(size, gfp)
#define	KZFREE(p) \
		do {					\
			memset((p), 0, sizeof(*(p)));	\
			kfree(p);			\
		} while (0);

#define	DEVICE_ATTR_READER(name, dev, buf) \
		ssize_t name(struct device *dev, \
		struct device_attribute *attr, char *buf)
#define	DEVICE_ATTR_WRITER(name, dev, buf, count) \
		ssize_t name(struct device *dev, \
		struct device_attribute *attr, \
		const char *buf, size_t count)
#define	DRIVER_ATTR_READER(name, drv, buf) \
		ssize_t name(struct device_driver *drv, char * buf)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
#define	SET_PROC_DIRENTRY_OWNER(p) \
		do { \
			(p)->owner = THIS_MODULE; \
		} while (0);
#else
#define	SET_PROC_DIRENTRY_OWNER(p) do { } while (0);
#endif

#else
typedef int bool;
#endif /* ifdef __KERNEL__ */
typedef struct xbus xbus_t;
typedef struct xpd xpd_t;
typedef struct xframe xframe_t;
typedef struct xpacket xpacket_t;
typedef __u32 xpp_line_t;	/* at most 31 lines for E1 */
typedef __u8 lineno_t;
typedef __u8 xportno_t;

#define	PORT_BROADCAST		255

#endif /* XDEFS_H */
