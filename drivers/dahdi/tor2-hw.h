/*
 * Tormenta 2  Quad-T1 PCI Driver
 *
 * Written by Mark Spencer <markster@linux-suppot.net>
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001-2008, Digium, Inc.
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

#ifndef _TOR2_HW_H
#define _TOR2_HW_H

/*
 * The Tormenta two consists of the following block architecture:
 *
 *    [ Spartan ]  --- [ DS 21Q352 ] -- Xfrms -- Span 1
 *           | | |      | | |             |
 *               Local Bus                +----- Span 2
 *                | | |                   |
 *             [ PCI 9030 ]               +----- Span 3
 *              | | | | |                 |
 *               PCI BUS                  +----- Span 4
 *
 *  All communicatiosn to the framer (21Q352) are performed
 *  through the PCI 9030 part using memory mapped I/O.  
 *
 *  The Tormenta 2 requires a 2 2k wondows memory space
 *  which is mapped as follows:
 *
 *  First (32 bit) space:
 *
 *  0x0000 -> 0x07FF:  Memory map of Tx and Rx buffers.  They are stored
 *                     with increasing channel number, with each span in
 *                     a byte of a 32-bit long word:
 *                         Bits 31-24: Span 1
 *                         Bits 23-16: Span 2
 *                         Bits 16- 8: Span 3
 *                         Bits  7- 0: Span 4
 *
 *
 *  Second (8 bit) space:
 *
 *  0x0000 -> 0x00FF:  Registers for Transceiver 1
 *  0x0100 -> 0x01FF:  Registers for Transceiver 2
 *  0x0200 -> 0x02FF:  Registers for Transceiver 3
 *  0x0300 -> 0x03FF:  Registers for Transceiver 4
 *
 *  0x400 Write  -> Firmware load location for Xilinx. This is the only valid
 *                     register until the Xilinx is programmed to decode
 *                     the remainder!
 *
 *  0x400 Write  -> clkreg (sync source)
 *	 0=free run, 1=span 1, 2=span 2, 3=span 3, 4=span 4.
 *
 *  0x400 Read  -> statreg
 *	bit 0 - Interrupt Enabled
 *	bit 1 - Interrupt Active
 *	bit 2 - Dallas Interrupt Active
 *
 *  0x401 Write  -> ctlreg as follows:
 *	bit 0 - Interrupt Enable
 *	bit 1 - Drives "TEST1" signal ("Interrupt" outbit)
 *	bit 2 - Dallas Interrupt Enable (Allows DINT signal to drive INT)
 *      bit 3 - External Syncronization Enable (MASTER signal).
 *	bit 4 - Select E1 Divisor Mode (0 for T1, 1 for E1).
 *	bit 5 - Remote serial loopback (When set to 1, TSER is driven from RSER)
 *	bit 6 - Local serial loopback (When set to 1, Rx buffers are driven from Tx buffers)
 *	bit 7 - Interrupt Acknowledge (set to 1 to acknowledge interrupt)
 *
 *  0x402 Write  -> LED register as follows:
 *	bit 0 - Span 1 Green
 * 	bit 1 - Span 1 Red
 *	bit 2 - Span 2 Green
 *	bit 3 - Span 2 Red
 *	bit 4 - Span 3 Green
 *	bit 5 - Span 3 Red
 *	bit 6 - Span 4 Green
 * 	bit 7 - Span 4 Red
 *	NOTE: turning on both red and green yields yellow.
 *
 *  0x403 Write  -> TEST2, writing to bit 0 drives TEST2 pin.
 *
 *  0x404 Write  -> ctlreg1 as follows:
 *	bit 0 - Non-REV.A Mode (Set this bit for Dallas chips later then Rev. A)
 */

#ifdef NEED_PCI_IDS
/*
 * Provide routines for identifying a tormenta card
 */

#define PCI_VENDOR_ID_PLX	0x10b5

#ifdef __KERNEL__
static DEFINE_PCI_DEVICE_TABLE(tor2_pci_ids) =
#else
#define PCI_ANY_ID -1
static struct tor2_pci_id {
	int vendor;
	int device;
	int subvendor;
	int subdevice;
	int class;
	int classmask;
	unsigned long driver_data;
} tor2_pci_ids[] =
#endif /* __KERNEL__ */
{
	{ PCI_VENDOR_ID_PLX, 0x9030, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)"PLX 9030" },	 /* PLX 9030 Development board */
	{ PCI_VENDOR_ID_PLX, 0x3001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)"PLX Development Board" },	 /* PLX 9030 Development board */
	{ PCI_VENDOR_ID_PLX, 0xD00D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)"Tormenta 2 Quad T1/PRI or E1/PRA" }, /* Tormenta 2 */
	{ PCI_VENDOR_ID_PLX, 0x4000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)"Tormenta 2 Quad T1/E1 (non-Digium clone)" }, /* Tormenta 2 clone */
	{ 0, }
};

#ifndef __KERNEL__
/* We provide a simple routine to match the given ID's */
static inline int tor2_pci_match(int vendorid, int deviceid, char **variant)
{
	/* Returns 1 if this is a tormenta card or 0 if it isn't */
	int x;
	for (x = 0; x< sizeof(tor2_pci_ids) / sizeof(tor2_pci_ids[0]); x++)
		if (((tor2_pci_ids[x].vendor == PCI_ANY_ID) || 
			(tor2_pci_ids[x].vendor == vendorid)) &&
		    ((tor2_pci_ids[x].device == PCI_ANY_ID) ||
			(tor2_pci_ids[x].device == deviceid))) {
			*variant = (char *)tor2_pci_ids[x].driver_data;
			return 1;
		}
	if (variant)
		*variant = NULL;
	return 0;
}
#endif /* __KERNEL__ */
#endif /* NEED_PCI_IDS */

/*
 * PLX PCI9030 PCI Configuration Registers
 *
 * This is not an all-inclusive list, just some interesting ones
 * that we need and that are not standard.
 *
 */
#define PLX_PCI_VPD_ADDR		0x4e	/* Set address here */
#define PLX_PCI_VPD_DATA		0x50	/* Read/Write data here */

#define PLX_LOC_WP_BOUNDARY		0x4e	/* Bits 6-0 here */
#define	PLX_LOC_GPIOC			0x54	/* GPIO control register */

/* The 4 GPIO data bits we are interested in */

#define	LOC_GPIOC_GPIO4			0x4000	/* GPIO4 data */
#define	LOC_GPIOC_GPIO5			0x20000	/* GPIO5 data */
#define	LOC_GPIOC_GPIO6			0x100000 /* GPIO6 data */
#define	LOC_GPIOC_GPIO7			0x800000 /* GPIO7 data */

/* define the initialization of the GPIOC register */

#define	LOC_GPIOC_INIT_VALUE		0x2036000	/* GPIO 4&5 in write and
						   both high and GPIO 8 in write low */

/* The defines by what they actually do */

#define	GPIO_WRITE			LOC_GPIOC_GPIO4
#define	GPIO_PROGRAM			LOC_GPIOC_GPIO5
#define	GPIO_INIT			LOC_GPIOC_GPIO6
#define	GPIO_DONE			LOC_GPIOC_GPIO7

#endif /* _TOR2_HW_H */

