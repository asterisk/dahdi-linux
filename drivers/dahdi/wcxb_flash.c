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

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/sched.h>

#include "wcxb_spi.h"
#include "wcxb_flash.h"

#define FLASH_PAGE_PROGRAM	0x02
#define FLASH_READ		0x03
#define FLASH_READ_STATUS	0x05
#define FLASH_WRITE_ENABLE	0x06
#define FLASH_SECTOR_ERASE	0xd8

static int wcxb_flash_read_status_register(struct wcxb_spi_device *spi,
					   u8 *status)
{
	u8 command[] = {
		FLASH_READ_STATUS,
	};
	struct wcxb_spi_transfer t_cmd = {
		.tx_buf = command,
		.len = sizeof(command),
	};
	struct wcxb_spi_transfer t_serial = {
		.rx_buf = status,
		.len = 1,
	};
	struct wcxb_spi_message m;
	wcxb_spi_message_init(&m);
	wcxb_spi_message_add_tail(&t_cmd, &m);
	wcxb_spi_message_add_tail(&t_serial, &m);
	return wcxb_spi_sync(spi, &m);
}

int wcxb_flash_read(struct wcxb_spi_device *spi, unsigned int address,
		    void *data, size_t len)
{
	u8 command[] = {
		FLASH_READ,
		(address & 0xff0000) >> 16,
		(address & 0xff00) >> 8,
		(address & 0xff)
	};
	struct wcxb_spi_transfer t_cmd = {
		.tx_buf = command,
		.len = sizeof(command),
	};
	struct wcxb_spi_transfer t_serial = {
		.rx_buf = data,
		.len = len,
	};
	struct wcxb_spi_message m;
	wcxb_spi_message_init(&m);
	wcxb_spi_message_add_tail(&t_cmd, &m);
	wcxb_spi_message_add_tail(&t_serial, &m);
	return wcxb_spi_sync(spi, &m);
}

static int wcxb_flash_wait_until_not_busy(struct wcxb_spi_device *spi)
{
	int res;
	u8 status;
	unsigned long stop = jiffies + 5*HZ;
	do {
		res = wcxb_flash_read_status_register(spi, &status);
	} while (!res && (status & 0x1) && time_before(jiffies, stop));
	if (!res)
		return res;
	if (time_after_eq(jiffies, stop))
		return -EIO;
	return 0;
}

static int wcxb_flash_write_enable(struct wcxb_spi_device *spi)
{
	u8 command = FLASH_WRITE_ENABLE;
	return wcxb_spi_write(spi, &command, 1);
}

int wcxb_flash_sector_erase(struct wcxb_spi_device *spi,
				   unsigned int address)
{
	int res;
	u8 command[] = {FLASH_SECTOR_ERASE, (address >> 16)&0xff, 0x00, 0x00};
	/* Sector must be on 64KB boundary. */
	if (address & 0xffff)
		return -EINVAL;
	/* Start the erase. */
	res = wcxb_flash_write_enable(spi);
	if (res)
		return res;
	res = wcxb_spi_write(spi, &command, sizeof(command));
	if (res)
		return res;
	return wcxb_flash_wait_until_not_busy(spi);
}

int wcxb_flash_write(struct wcxb_spi_device *spi, unsigned int address,
		     const void *data, size_t len)
{
	int res;
	const size_t FLASH_PAGE_SIZE = 256;
	u8 command[] = {
		FLASH_PAGE_PROGRAM,
		(address & 0xff0000) >> 16,
		(address & 0xff00) >> 8,
		0x00,
	};
	struct wcxb_spi_transfer t_cmd = {
		.tx_buf = command,
		.len = sizeof(command),
	};
	struct wcxb_spi_transfer t_data = {
		.tx_buf = data,
		.len = len,
	};
	struct wcxb_spi_message m;

	/* We need to write on page size boundaries */
	WARN_ON(address & 0xff);

	wcxb_spi_message_init(&m);
	wcxb_spi_message_add_tail(&t_cmd, &m);
	wcxb_spi_message_add_tail(&t_data, &m);

	while (len) {
		res = wcxb_flash_write_enable(spi);
		if (res)
			return res;
		command[1] = (address >> 16) & 0xff;
		command[2] = (address >> 8) & 0xff;
		t_data.tx_buf = data;
		t_data.len = min(len, FLASH_PAGE_SIZE);
		res = wcxb_spi_sync(spi, &m);
		WARN_ON(res);
		if (res)
			return res;
		res = wcxb_flash_wait_until_not_busy(spi);
		WARN_ON(res);
		if (res)
			return res;
		len -= t_data.len;
		address += t_data.len;
		data += t_data.len;
	}
	return 0;
}
