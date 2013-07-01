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

#ifndef __WCXB_FLASH_H__
#define __WCXB_FLASH_H__

extern int wcxb_flash_read(struct wcxb_spi_device *spi, unsigned int address,
			   void *data, size_t len);

extern int wcxb_flash_sector_erase(struct wcxb_spi_device *spi, unsigned int
				   address);
extern int wcxb_flash_write(struct wcxb_spi_device *spi, unsigned int address,
			    const void *data, size_t len);

#endif
