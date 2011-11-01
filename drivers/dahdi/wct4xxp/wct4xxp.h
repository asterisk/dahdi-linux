/*
 * Wildcard T400P FXS Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@linux-support.net>
 *
 * Copyright (C) 2001-2010, Digium, Inc.
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

#include <linux/ioctl.h>

#define FRMR_TTR_BASE 0x10
#define FRMR_RTR_BASE 0x0c
#define FRMR_TSEO 0xa0
#define FRMR_TSBS1 0xa1
#define FRMR_CCR1 0x09
#define FRMR_CCR1_ITF 0x08
#define FRMR_CCR1_EITS 0x10
#define FRMR_CCR2 0x0a
#define FRMR_CCR2_RCRC 0x04
#define FRMR_CCR2_RADD 0x10
#define FRMR_MODE 0x03
#define FRMR_MODE_NO_ADDR_CMP 0x80
#define FRMR_MODE_SS7 0x20
#define FRMR_MODE_HRAC 0x08
#define FRMR_IMR0 0x14
#define FRMR_IMR0_RME 0x80
#define FRMR_IMR0_RPF 0x01
#define FRMR_IMR1 0x15
#define FRMR_IMR1_ALLS 0x20
#define FRMR_IMR1_XDU 0x10
#define FRMR_IMR1_XPR 0x01
#define FRMR_XC0 0x22
#define FRMR_XC1 0x23
#define FRMR_RC0 0x24
#define FRMR_RC1 0x25
#define FRMR_SIC1 0x3e
#define FRMR_SIC2 0x3f
#define FRMR_SIC3 0x40
#define FRMR_CMR1 0x44
#define FRMR_CMR2 0x45
/* OctalFALC Only */
#define FRMR_CMR4 0x41
#define FRMR_CMR5 0x42
#define FRMR_CMR6 0x43
#define FRMR_GPC2 0x8a
/* End Octal */
#define FRMR_GCR 0x46
#define FRMR_ISR0 0x68
#define FRMR_ISR0_RME 0x80
#define FRMR_ISR0_RPF 0x01
#define FRMR_ISR1 0x69
#define FRMR_ISR1_ALLS 0x20
#define FRMR_ISR1_XDU 0x10
#define FRMR_ISR1_XPR 0x01
#define FRMR_ISR2 0x6a
#define FRMR_ISR3 0x6b
#define FRMR_ISR4 0x6c
#define FRMR_GIS  0x6e
#define FRMR_GIS_ISR0 0x01
#define FRMR_GIS_ISR1 0x02
#define FRMR_GIS_ISR2 0x04
#define FRMR_GIS_ISR3 0x08
#define FRMR_GIS_ISR4 0x10
#define FRMR_CIS 0x6f
#define FRMR_CIS_GIS1 0x01
#define FRMR_CIS_GIS2 0x02
#define FRMR_CIS_GIS3 0x04
#define FRMR_CIS_GIS4 0x08

/* CIS - Octal falc bits */
#define FRMR_CIS_GIS5 0x10
#define FRMR_CIS_GIS6 0x20
#define FRMR_CIS_GIS7 0x40
#define FRMR_CIS_GIS8 0x80

#define FRMR_CMDR 0x02
#define FRMR_CMDR_SRES 0x01
#define FRMR_CMDR_XRES 0x10
#define FRMR_CMDR_RMC 0x80
#define FRMR_CMDR_XTF 0x04
#define FRMR_CMDR_XHF 0x08
#define FRMR_CMDR_XME 0x02
#define FRMR_RSIS 0x65
#define FRMR_RSIS_VFR 0x80
#define FRMR_RSIS_RDO 0x40
#define FRMR_RSIS_CRC16 0x20
#define FRMR_RSIS_RAB 0x10
#define FRMR_RBCL 0x66
#define FRMR_RBCL_MAX_SIZE 0x1f
#define FRMR_RBCH 0x67
#define FRMR_RXFIFO 0x00
#define FRMR_SIS 0x64
#define FRMR_SIS_XFW 0x40
#define FRMR_TXFIFO 0x00

#define FRS0 0x4c
#define FRS0_LOS (1<<7)
#define FRS0_LFA (1<<5)
#define FRS0_LMFA (1<<1)

#define FRS1 0x4d
#define FRS1_XLS (1<<1)
#define FRS1_XLO (1<<0)

#define NUM_REGS 0xa9
#define NUM_PCI 12

struct t4_regs {
	unsigned int pci[NUM_PCI];
	unsigned char regs[NUM_REGS];
};

struct t4_reg {
	unsigned int reg;
	unsigned int val;
};

#define T4_CHECK_VPM		0
#define T4_LOADING_FW		1
#define T4_STOP_DMA		2
#define T4_CHECK_TIMING		3
#define T4_CHANGE_LATENCY	4
#define T4_IGNORE_LATENCY	5

#define WCT4_GET_REGS	_IOW(DAHDI_CODE, 60, struct t4_regs)
#define WCT4_GET_REG	_IOW(DAHDI_CODE, 61, struct t4_reg)
#define WCT4_SET_REG	_IOW(DAHDI_CODE, 62, struct t4_reg)

