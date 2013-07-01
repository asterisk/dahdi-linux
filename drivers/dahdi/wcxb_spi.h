/*
 * wcxb SPI library
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

#ifndef __WCXB_SPI_H
#define __WCXB_SPI_H

#include <linux/spi/spi.h>
#include <stdbool.h>

struct wcxb_spi_transfer {
	const void	*tx_buf;
	void		*rx_buf;
	u32		len:16;
	u16		delay_usecs;
	struct list_head node;
};

struct wcxb_spi_message {
	struct list_head transfers;
	struct list_head node;
	struct wcxb_spi_device *spi;
	void (*complete)(void *arg);
	void *arg;
	int status;
};

struct wcxb_spi_master;

struct wcxb_spi_device {
	struct wcxb_spi_master *master;
	u16 chip_select;
};

extern struct wcxb_spi_master *wcxb_spi_master_create(struct device *parent,
					void __iomem *base, bool auto_cs);
extern void wcxb_spi_master_destroy(struct wcxb_spi_master *master);
extern int wcxb_spi_sync(struct wcxb_spi_device *spi,
			 struct wcxb_spi_message *message);
extern int wcxb_spi_async(struct wcxb_spi_device *spi,
			  struct wcxb_spi_message *message);
extern void wcxb_spi_handle_interrupt(struct wcxb_spi_master *master);

static inline struct wcxb_spi_device *
wcxb_spi_device_create(struct wcxb_spi_master *master, u16 chip_select)
{
	struct wcxb_spi_device *spi = kzalloc(sizeof(*spi), GFP_KERNEL);
	if (!spi)
		return NULL;
	spi->master = master;
	spi->chip_select = chip_select;
	return spi;
}

static inline void wcxb_spi_device_destroy(struct wcxb_spi_device *spi)
{
	kfree(spi);
}

static inline void wcxb_spi_message_init(struct wcxb_spi_message *m)
{
	memset(m, 0, sizeof(*m));
	INIT_LIST_HEAD(&m->transfers);
}

static inline void wcxb_spi_message_add_tail(struct wcxb_spi_transfer *t,
					     struct wcxb_spi_message *m)
{
	list_add_tail(&t->node, &m->transfers);
}

static inline int
wcxb_spi_write(struct wcxb_spi_device *spi, const void *buffer, size_t len)
{
	struct wcxb_spi_transfer	t = {
			.tx_buf		= buffer,
			.len		= len,
		};
	struct wcxb_spi_message	m;
	wcxb_spi_message_init(&m);
	wcxb_spi_message_add_tail(&t, &m);
	return wcxb_spi_sync(spi, &m);
}

static inline int
wcxb_spi_read(struct wcxb_spi_device *spi, void *buffer, size_t len)
{
	struct wcxb_spi_transfer	t = {
			.rx_buf		= buffer,
			.len		= len,
		};
	struct wcxb_spi_message	m;
	wcxb_spi_message_init(&m);
	wcxb_spi_message_add_tail(&t, &m);
	return wcxb_spi_sync(spi, &m);
}

#endif
