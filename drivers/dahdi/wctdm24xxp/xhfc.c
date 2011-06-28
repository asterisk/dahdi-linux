/*
 * B400M  Quad-BRI module Driver
 * Written by Andrew Kohlsmith <akohlsmith@mixdown.ca>
 *
 * Copyright (C) 2010 Digium, Inc.
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
#include <linux/pci.h>
#include <linux/ppp_defs.h>
#include <linux/delay.h>
#include <linux/sched.h>

#define FAST_HDLC_NEED_TABLES
#include <dahdi/kernel.h>
#include <dahdi/fasthdlc.h>

#include "wctdm24xxp.h"
#include "xhfc.h"

#define HFC_NR_FIFOS	16
#define HFC_ZMIN	0x00			/* from datasheet */
#define HFC_ZMAX	0x7f
#define HFC_FMIN	0x00
#define HFC_FMAX	0x07

/*
 * yuck. Any reg which is not mandated read/write or read-only is write-only.
 * Also, there are dozens of registers with the same address.  Additionally,
 * there are array registers (A_) which have an index register These A_
 * registers require an index register to be written to indicate WHICH in the
 * array you want.
 */

#define R_CIRM			0x00	/* WO       */
#define R_CTRL			0x01	/* WO       */
#define R_CLK_CFG		0x02	/* WO       */
#define A_Z1			0x04	/*    RO    */
#define A_Z2			0x06	/*    RO    */
#define R_RAM_ADDR		0x08	/* WO       */
#define R_RAM_CTRL		0x09	/* WO       */
#define R_FIRST_FIFO		0x0b	/* WO       */
#define R_FIFO_THRES		0x0c	/* WO       */
#define A_F1			0x0c	/*    RO    */
#define R_FIFO_MD		0x0d	/* WO       */
#define A_F2			0x0d	/*    RO    */
#define A_INC_RES_FIFO		0x0e	/* WO       */
#define A_FIFO_STA		0x0e	/*    RO    */
#define R_FSM_IDX		0x0f	/* WO       */
#define R_FIFO			0x0f	/* WO       */

#define R_SLOT			0x10	/* WO       */
#define R_IRQ_OVIEW		0x10	/*    RO    */
#define R_MISC_IRQMSK		0x11	/* WO       */
#define R_MISC_IRQ		0x11	/*    RO    */
#define R_SU_IRQMSK		0x12	/* WO       */
#define R_SU_IRQ		0x12	/*    RO    */
#define R_IRQ_CTRL		0x13	/* WO       */
#define R_AF0_OVIEW		0x13	/*    RO    */
#define R_PCM_MD0		0x14	/* WO       */
#define A_USAGE			0x14	/*    RO    */
#define R_MSS0			0x15	/* WO       */
#define R_MSS1			0x15	/* WO       */
#define R_PCM_MD1		0x15	/* WO       */
#define R_PCM_MD2		0x15	/* WO       */
#define R_SH0H			0x15	/* WO       */
#define R_SH1H			0x15	/* WO       */
#define R_SH0L			0x15	/* WO       */
#define R_SH1L			0x15	/* WO       */
#define R_SL_SEL0		0x15	/* WO       */
#define R_SL_SEL1		0x15	/* WO       */
#define R_SL_SEL7		0x15	/* WO       */
#define R_RAM_USE		0x15	/*    RO    */
#define R_SU_SEL		0x16	/* WO       */
#define R_CHIP_ID		0x16	/*    RO    */
#define R_SU_SYNC		0x17	/* WO       */
#define R_BERT_STA		0x17	/*    RO    */
#define R_F0_CNTL		0x18	/*    RO    */
#define R_F0_CNTH		0x19	/*    RO    */
#define R_TI_WD			0x1a	/* WO       */
#define R_BERT_ECL		0x1a	/*    RO    */
#define R_BERT_WD_MD		0x1b	/* WO       */
#define R_BERT_ECH		0x1b	/*    RO    */
#define R_STATUS		0x1c	/*    RO    */
#define R_SL_MAX		0x1d	/*    RO    */
#define R_PWM_CFG		0x1e	/* WO       */
#define R_CHIP_RV		0x1f	/*    RO    */

#define R_FIFO_BL0_IRQ		0x20	/*    RO    */
#define R_FIFO_BL1_IRQ		0x21	/*    RO    */
#define R_FIFO_BL2_IRQ		0x22	/*    RO    */
#define R_FIFO_BL3_IRQ		0x23	/*    RO    */
#define R_FILL_BL0		0x24	/*    RO    */
#define R_FILL_BL1		0x25	/*    RO    */
#define R_FILL_BL2		0x26	/*    RO    */
#define R_FILL_BL3		0x27	/*    RO    */
#define R_CI_TX			0x28	/* WO       */
#define R_CI_RX			0x28	/*    RO    */
#define R_CGI_CFG0		0x29	/* WO       */
#define R_CGI_STA		0x29	/*    RO    */
#define R_CGI_CFG1		0x2a	/* WO       */
#define R_MON_RX		0x2a	/*    RO    */
#define R_MON_TX		0x2b	/* WO       */

#define A_SU_WR_STA		0x30	/* WO       */
#define A_SU_RD_STA		0x30	/*    RO    */
#define A_SU_CTRL0		0x31	/* WO       */
#define A_SU_DLYL		0x31	/*    RO    */
#define A_SU_CTRL1		0x32	/* WO       */
#define A_SU_DLYH		0x32	/*    RO    */
#define A_SU_CTRL2		0x33	/* WO       */
#define A_MS_TX			0x34	/* WO       */
#define A_MS_RX			0x34	/*    RO    */
#define A_ST_CTRL3		0x35	/* WO       */
#define A_UP_CTRL3		0x35	/* WO       */
#define A_SU_STA		0x35	/*    RO    */
#define A_MS_DF			0x36	/* WO       */
#define A_SU_CLK_DLY		0x37	/* WO       */
#define R_PWM0			0x38	/* WO       */
#define R_PWM1			0x39	/* WO       */
#define A_B1_TX			0x3c	/* WO       */
#define A_B1_RX			0x3c	/*    RO    */
#define A_B2_TX			0x3d	/* WO       */
#define A_B2_RX			0x3d	/*    RO    */
#define A_D_TX			0x3e	/* WO       */
#define A_D_RX			0x3e	/*    RO    */
#define A_BAC_S_TX		0x3f	/* WO       */
#define A_E_RX			0x3f	/*    RO    */

#define R_GPIO_OUT1		0x40	/* WO       */
#define R_GPIO_IN1		0x40	/*    RO    */
#define R_GPIO_OUT3		0x41	/* WO       */
#define R_GPIO_IN3		0x41	/*    RO    */
#define R_GPIO_EN1		0x42	/* WO       */
#define R_GPIO_EN3		0x43	/* WO       */
#define R_GPIO_SEL_BL		0x44	/* WO       */
#define R_GPIO_OUT2		0x45	/* WO       */
#define R_GPIO_IN2		0x45	/*    RO    */
#define R_PWM_MD		0x46	/* WO       */
#define R_GPIO_EN2		0x47	/* WO       */
#define R_GPIO_OUT0		0x48	/* WO       */
#define R_GPIO_IN0		0x48	/*    RO    */
#define R_GPIO_EN0		0x4a	/* WO       */
#define R_GPIO_SEL		0x4c	/* WO       */

#define R_PLL_CTRL		0x50	/* WO       */
#define R_PLL_STA		0x50	/*    RO    */
#define R_PLL_P			0x51	/*       RW */
#define R_PLL_N			0x52	/*       RW */
#define R_PLL_S			0x53	/*       RW */

#define A_FIFO_DATA		0x80	/*       RW */
#define A_FIFO_DATA_NOINC	0x84	/*       RW */

#define R_INT_DATA		0x88	/*    RO    */

#define R_RAM_DATA		0xc0	/*       RW */

#define A_SL_CFG		0xd0	/*       RW */
#define A_CH_MSK		0xf4	/*       RW */
#define A_CON_HDLC		0xfa	/*       RW */
#define A_SUBCH_CFG		0xfb	/*       RW */
#define A_CHANNEL		0xfc	/*       RW */
#define A_FIFO_SEQ		0xfd	/*       RW */
#define A_FIFO_CTRL		0xff	/*       RW */

/* R_CIRM bits */
#define V_CLK_OFF		(1 << 0)	/* 1=internal clocks disabled */
#define V_WAIT_PROC		(1 << 1)	/* 1=additional /WAIT after write access */
#define V_WAIT_REG		(1 << 2)	/* 1=additional /WAIT for internal BUSY phase */
#define V_SRES			(1 << 3)	/* soft reset (group 0) */
#define V_HFC_RES		(1 << 4)	/* HFC reset (group 1) */
#define V_PCM_RES		(1 << 5)	/* PCM reset (group 2) */
#define V_SU_RES		(1 << 6)	/* S/T reset (group 3) */
#define XHFC_FULL_RESET	(V_SRES | V_HFC_RES | V_PCM_RES | V_SU_RES)

/* R_STATUS bits */
#define V_BUSY			(1 << 0)	/* 1=HFC busy, limited register access */
#define V_PROC			(1 << 1)	/* 1=HFC in processing phase */
#define V_LOST_STA		(1 << 3)	/* 1=frames have been lost */
#define V_PCM_INIT		(1 << 4)	/* 1=PCM init in progress */
#define V_WAK_STA		(1 << 5)	/* state of WAKEUP pin wien V_WAK_EN=1 */
#define V_MISC_IRQSTA		(1 << 6)	/* 1=misc interrupt has occurred */
#define V_FR_IRQSTA		(1 << 7)	/* 1=fifo interrupt has occured */
#define XHFC_INTS	(V_MISC_IRQSTA | V_FR_IRQSTA)

/* R_FIFO_BLx_IRQ bits */
#define V_FIFOx_TX_IRQ		(1 << 0)	/* FIFO TX interrupt occurred */
#define V_FIFOx_RX_IRQ		(1 << 1)	/* FIFO RX interrupt occurred */
#define FIFOx_TXRX_IRQ		(V_FIFOx_TX_IRQ | V_FIFOx_RX_IRQ)

/* R_FILL_BLx bits */
#define V_FILL_FIFOx_TX		(1 << 0)	/* TX FIFO reached V_THRES_TX level */
#define V_FILL_FIFOx_RX		(1 << 1)	/* RX FIFO reached V_THRES_RX level */
#define FILL_FIFOx_TXRX		(V_FILL_FIFOx_TX | V_FILL_FIFOx_RX)

/* R_MISC_IRQ / R_MISC_IRQMSK bits */
#define V_SLP_IRQ		(1 << 0)	/* frame sync pulse flips */
#define V_TI_IRQ		(1 << 1)	/* timer elapsed */
#define V_PROC_IRQ		(1 << 2)	/* processing/non-processing transition */
#define V_CI_IRQ		(1 << 4)	/* indication bits changed */
#define V_WAK_IRQ		(1 << 5)	/* WAKEUP pin */
#define V_MON_TX_IRQ		(1 << 6)	/* monitor byte can be written */
#define V_MON_RX_IRQ		(1 << 7)	/* monitor byte received */

/* R_SU_IRQ/R_SU_IRQMSK bits */
#define V_SU0_IRQ		(1 << 0)	/* interrupt/mask port 1 */
#define V_SU1_IRQ		(1 << 1)	/* interrupt/mask port 2 */
#define V_SU2_IRQ		(1 << 2)	/* interrupt/mask port 3 */
#define V_SU3_IRQ		(1 << 3)	/* interrupt/mask port 4 */

/* R_IRQ_CTRL bits */
#define V_FIFO_IRQ_EN		(1 << 0)	/* enable any unmasked FIFO IRQs */
#define V_GLOB_IRQ_EN		(1 << 3)	/* enable any unmasked IRQs */
#define V_IRQ_POL		(1 << 4)	/* 1=IRQ active high */

/* R_BERT_WD_MD bits */
#define V_BERT_ERR		(1 << 3)	/* 1=generate an error bit in BERT stream */
#define V_AUTO_WD_RES		(1 << 5)	/* 1=automatically kick the watchdog */
#define V_WD_RES		(1 << 7)	/* 1=kick the watchdog (bit auto clears) */

/* R_TI_WD bits */
#define V_EV_TS_SHIFT		(0)
#define V_EV_TS_MASK		(0x0f)
#define V_WD_TS_SHIFT		(4)
#define V_WD_TS_MASK		(0xf0)

/* A_FIFO_CTRL bits */
#define V_FIFO_IRQMSK		(1 << 0)	/* 1=FIFO can generate interrupts */
#define V_BERT_EN		(1 << 1)	/* 1=BERT data replaces FIFO data */
#define V_MIX_IRQ		(1 << 2)	/* IRQ when 0=end of frame only, 1=also when Z1==Z2 */
#define V_FR_ABO		(1 << 3)	/* 1=generate frame abort/frame abort detected */
#define V_NO_CRC		(1 << 4)	/* 1=do not send CRC at end of frame */
#define V_NO_REP		(1 << 5)	/* 1=frame deleted after d-chan contention */

/* R_CLK_CFG bits */
#define V_CLK_PLL		(1 << 0)	/* Sysclk select 0=OSC_IN, 1=PLL output */
#define V_CLKO_HI		(1 << 1)	/* CLKOUT selection 0=PLL/8, 1=PLL */
#define V_CLKO_PLL		(1 << 2)	/* CLKOUT source 0=divider or PLL input, 1=PLL output */
#define V_PCM_CLK		(1 << 5)	/* 1=PCM clk = OSC, 0 = PCM clk is 2x OSC */
#define V_CLKO_OFF		(1 << 6)	/* CLKOUT enable 0=enabled */
#define V_CLK_F1		(1 << 7)	/* PLL input pin 0=OSC_IN, 1=F1_1 */

/* R_PCM_MD0 bits */
#define V_PCM_MD		(1 << 0)	/* 1=PCM master */
#define V_C4_POL		(1 << 1)	/* 1=F0IO sampled on rising edge of C4IO */
#define V_F0_NEG		(1 << 2)	/* 1=negative polarity of F0IO */
#define V_F0_LEN		(1 << 3)	/* 1=F0IO active for 2 C4IO clocks */
#define V_PCM_IDX_SEL0		(0x0 << 4)	/* reg15 = R_SL_SEL0 */
#define V_PCM_IDX_SEL1		(0x1 << 4)	/* reg15 = R_SL_SEL1 */
#define V_PCM_IDX_SEL7		(0x7 << 4)	/* reg15 = R_SL_SEL7 */
#define V_PCM_IDX_MSS0		(0x8 << 4)	/* reg15 = R_MSS0 */
#define V_PCM_IDX_MD1		(0x9 << 4)	/* reg15 = R_PCM_MD1 */
#define V_PCM_IDX_MD2		(0xa << 4)	/* reg15 = R_PCM_MD2 */
#define V_PCM_IDX_MSS1		(0xb << 4)	/* reg15 = R_MSS1 */
#define V_PCM_IDX_SH0L		(0xc << 4)	/* reg15 = R_SH0L */
#define V_PCM_IDX_SH0H		(0xd << 4)	/* reg15 = R_SH0H */
#define V_PCM_IDX_SH1L		(0xe << 4)	/* reg15 = R_SH1L */
#define V_PCM_IDX_SH1H		(0xf << 4)	/* reg15 = R_SH1H */
#define V_PCM_IDX_MASK		(0xf0)

/* R_PCM_MD1 bits */
#define V_PLL_ADJ_00		(0x0 << 2)	/* adj 4 times by 0.5 system clk cycles */
#define V_PLL_ADJ_01		(0x1 << 2)	/* adj 3 times by 0.5 system clk cycles */
#define V_PLL_ADJ_10		(0x2 << 2)	/* adj 2 times by 0.5 system clk cycles */
#define V_PLL_ADJ_11		(0x3 << 2)	/* adj 1 time by 0.5 system clk cycles */
#define V_PCM_DR_2048		(0x0 << 4)	/* 2.048Mbps, 32 timeslots */
#define V_PCM_DR_4096		(0x1 << 4)	/* 4.096Mbps, 64 timeslots */
#define V_PCM_DR_8192		(0x2 << 4)	/* 8.192Mbps, 128 timeslots */
#define V_PCM_DR_075		(0x3 << 4)	/* 0.75Mbps, 12 timeslots */
#define V_PCM_LOOP		(1 << 6)	/* 1=internal loopback */
#define V_PCM_SMPL		(1 << 7)	/* 0=sample at middle of bit cell, 1=sample at 3/4 point */
#define V_PLL_ADJ_MASK		(0x3 << 2)
#define V_PCM_DR_MASK		(0x3 << 4)

/* R_PCM_MD2 bits */
#define V_SYNC_OUT1		(1 << 1)	/* SYNC_O source 0=SYNC_I or FSX_RX, 1=512kHz from PLL or multiframe */
#define V_SYNC_SRC		(1 << 2)	/* 0=line interface, 1=SYNC_I */
#define V_SYNC_OUT2		(1 << 3)	/* SYNC_O source 0=rx sync or FSC_RX 1=SYNC_I or received superframe */
#define V_C2O_EN		(1 << 4)	/* C2IO output enable (when V_C2I_EN=0) */
#define V_C2I_EN		(1 << 5)	/* PCM controller clock source 0=C4IO, 1=C2IO */
#define V_PLL_ICR		(1 << 6)	/* 0=reduce PCM frame time, 1=increase */
#define V_PLL_MAN		(1 << 7)	/* 0=auto, 1=manual */

/* A_SL_CFG bits */
#define V_CH_SDIR		(1 << 0)	/* 1=HFC channel receives data from PCM TS */
#define V_ROUT_TX_DIS		(0x0 << 6)	/* disabled, output disabled */
#define V_ROUT_TX_LOOP		(0x1 << 6)	/* internally looped, output disabled */
#define V_ROUT_TX_STIO1		(0x2 << 6)	/* output data to STIO1 */
#define V_ROUT_TX_STIO2		(0x3 << 6)	/* output data to STIO2 */
#define V_ROUT_RX_DIS		(0x0 << 6)	/* disabled, input data ignored */
#define V_ROUT_RX_LOOP		(0x1 << 6)	/* internally looped, input data ignored */
#define V_ROUT_RX_STIO2		(0x2 << 6)	/* channel data comes from STIO1 */
#define V_ROUT_RX_STIO1		(0x3 << 6)	/* channel data comes from STIO2 */
#define V_CH_SNUM_SHIFT		(1)
#define V_CH_SNUM_MASK		(31 << 1)

/* A_CON_HDLC bits */
#define V_IFF			(1 << 0)	/* Inter-Frame Fill: 0=0x7e, 1=0xff */
#define V_HDLC_TRP		(1 << 1)	/* 0=HDLC mode, 1=transparent */
#define V_TRP_DISABLED		(0x0 << 2)	/* FIFO disabled, no interrupt */
#define V_TRP_IRQ_64		(0x1 << 2)	/* FIFO enabled, int @ 8 bytes */
#define V_TRP_IRQ_128		(0x2 << 2)	/* FIFO enabled, int @ 16 bytes */
#define V_TRP_IRQ_256		(0x3 << 2)	/* FIFO enabled, int @ 32 bytes */
#define V_TRP_IRQ_512		(0x4 << 2)	/* FIFO enabled, int @ 64 bytes */
#define V_TRP_IRQ_1024		(0x5 << 2)	/* FIFO enabled, int @ 128 bytes */
#define V_TRP_NO_IRQ		(0x7 << 2)	/* FIFO enabled, no interrupt */
#define V_HDLC_IRQ		(0x3 << 2)	/* HDLC: FIFO enabled, interrupt at end of frame or when FIFO > 16 byte boundary (Mixed IRQ) */
#define V_DATA_FLOW_000		(0x0 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_001		(0x1 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_010		(0x2 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_011		(0x3 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_100		(0x4 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_101		(0x5 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_110		(0x6 << 5)	/* see A_CON_HDLC reg description in datasheet */
#define V_DATA_FLOW_111		(0x7 << 5)	/* see A_CON_HDLC reg description in datasheet */

/* R_FIFO bits */
#define V_FIFO_DIR		(1 << 0)	/* 1=RX FIFO data */
#define V_REV			(1 << 7)	/* 1=MSB first */
#define V_FIFO_NUM_SHIFT	(1)
#define V_FIFO_NUM_MASK		(0x3e)

/* A_CHANNEL bits */
#define V_CH_FDIR		(1 << 0)	/* 1=HFC chan for RX data */
#define V_CH_FNUM_SHIFT		(1)
#define V_CH_FNUM_MASK		(0x3e)

/* R_SLOT bits */
#define V_SL_DIR		(1 << 0)	/* 1=timeslot will RX PCM data from bus */
#define V_SL_NUM_SHIFT		(1)
#define V_SL_NUM_MASK		(0xfe)

/* A_INC_RES_FIFO bits */
#define V_INC_F			(1 << 0)	/* 1=increment FIFO F-counter (bit auto-clears) */
#define V_RES_FIFO		(1 << 1)	/* 1=reset FIFO (bit auto-clears) */
#define V_RES_LOST		(1 << 2)	/* 1=reset LOST error (bit auto-clears) */
#define V_RES_FIFO_ERR		(1 << 3)	/* 1=reset FIFO error (bit auto-clears), check V_ABO_DONE before setting */

/* R_FIFO_MD bits */
#define V_FIFO_MD_00		(0x0 << 0)	/* 16 FIFOs, 64 bytes TX/RX, 128 TX or RX if V_UNIDIR_RX */
#define V_FIFO_MD_01		(0x1 << 0)	/* 8 FIFOs, 128 bytes TX/RX, 256 TX or RX if V_UNIDIR_RX */
#define V_FIFO_MD_10		(0x2 << 0)	/* 4 FIFOs, 256 bytes TX/RX, invalid mode with V_UNIDIR_RX */
#define V_DF_MD_SM		(0x0 << 2)	/* simple data flow mode */
#define V_DF_MD_CSM		(0x1 << 2)	/* channel select mode */
#define V_DF_MD_FSM		(0x3 << 2)	/* FIFO sequence mode */
#define V_UNIDIR_MD		(1 << 4)	/* 1=unidirectional FIFO mode */
#define V_UNIDIR_RX		(1 << 5)	/* 1=unidirection FIFO is RX */

/* A_SUBCH_CFG bits */
#define V_BIT_CNT_8BIT		(0)		/* process 8 bits */
#define V_BIT_CNT_1BIT		(1)		/* process 1 bit */
#define V_BIT_CNT_2BIT		(2)		/* process 2 bits */
#define V_BIT_CNT_3BIT		(3)		/* process 3 bits */
#define V_BIT_CNT_4BIT		(4)		/* process 4 bits */
#define V_BIT_CNT_5BIT		(5)		/* process 5 bits */
#define V_BIT_CNT_6BIT		(6)		/* process 6 bits */
#define V_BIT_CNT_7BIT		(7)		/* process 7 bits */
#define V_LOOP_FIFO		(1 << 6)	/* loop FIFO data */
#define V_INV_DATA		(1 << 7)	/* invert FIFO data */
#define V_START_BIT_SHIFT	(3)
#define V_START_BIT_MASK	(0x38)

/* R_SU_SYNC bits */
#define V_SYNC_SEL_PORT0	(0x0 << 0)	/* sync to TE port 0 */
#define V_SYNC_SEL_PORT1	(0x1 << 0)	/* sync to TE port 1 */
#define V_SYNC_SEL_PORT2	(0x2 << 0)	/* sync to TE port 2 */
#define V_SYNC_SEL_PORT3	(0x3 << 0)	/* sync to TE port 3 */
#define V_SYNC_SEL_SYNCI	(0x4 << 0)	/* sync to SYNC_I */
#define V_MAN_SYNC		(1 << 3)	/* 1=manual sync mode */
#define V_AUTO_SYNCI		(1 << 4)	/* 1=SYNC_I used if FSC_RX not found */
#define V_D_MERGE_TX		(1 << 5)	/* 1=all 4 dchan taken from one byte in TX */
#define V_E_MERGE_RX		(1 << 6)	/* 1=all 4 echan combined in RX direction */
#define V_D_MERGE_RX		(1 << 7)	/* 1=all 4 dchan combined in RX direction */
#define V_SYNC_SEL_MASK		(0x03)

/* A_SU_WR_STA bits */
#define V_SU_SET_STA_MASK	(0x0f)
#define V_SU_LD_STA		(1 << 4)	/* 1=force SU_SET_STA mode, must be manually cleared 6us later */
#define V_SU_ACT_NOP		(0x0 << 5)	/* NOP */
#define V_SU_ACT_DEACTIVATE	(0x2 << 5)	/* start deactivation. auto-clears */
#define V_SU_ACT_ACTIVATE	(0x3 << 5)	/* start activation. auto-clears. */
#define V_SET_G2_G3		(1 << 7)	/* 1=auto G2->G3 in NT mode. auto-clears after transition. */

/* A_SU_RD_STA */
#define V_SU_STA_MASK		(0x0f)
#define V_SU_FR_SYNC		(1 << 4)	/* 1=synchronized */
#define V_SU_T2_EXP		(1 << 5)	/* 1=T2 expired (NT only) */
#define V_SU_INFO0		(1 << 6)	/* 1=INFO0 */
#define V_G2_G3			(1 << 7)	/* 1=allows G2->G3 (NT only, auto-clears) */

/* A_SU_CLK_DLY bits */
#define V_SU_DLY_MASK		(0x0f)
#define V_SU_SMPL_MASK		(0xf0)
#define V_SU_SMPL_SHIFT		(4)

/* A_SU_CTRL0 bits */
#define V_B1_TX_EN		(1 << 0)	/* 1=B1-channel transmit */
#define V_B2_TX_EN		(1 << 1)	/* 1=B2-channel transmit */
#define V_SU_MD			(1 << 2)	/* 0=TE, 1=NT */
#define V_ST_D_LPRIO		(1 << 3)	/* D-Chan priority 0=high, 1=low */
#define V_ST_SQ_EN		(1 << 4)	/* S/Q bits transmit (1=enabled) */
#define V_SU_TST_SIG		(1 << 5)	/* 1=transmit test signal */
#define V_ST_PU_CTRL		(1 << 6)	/* 1=enable end of pulse control */
#define V_SU_STOP		(1 << 7)	/* 1=power down */

/* A_SU_CTRL1 bits */
#define V_G2_G3_EN		(1 << 0)	/* 1=G2->G3 allowed without V_SET_G2_G3 */
#define V_D_RES			(1 << 2)	/* 1=D-chan reset */
#define V_ST_E_IGNO		(1 << 3)	/* TE:1=ignore Echan, NT:should always be 1. */
#define V_ST_E_LO		(1 << 4)	/* NT only: 1=force Echan low */
#define V_BAC_D			(1 << 6)	/* 1=BAC bit controls Dchan TX */
#define V_B12_SWAP		(1 << 7)	/* 1=swap B1/B2 */

/* A_SU_CTRL2 bits */
#define V_B1_RX_EN		(1 << 0)	/* 1=enable B1 RX */
#define V_B2_RX_EN		(1 << 1)	/* 1=enable B2 RX */
#define V_MS_SSYNC2		(1 << 2)	/* 0 normally, see datasheet */
#define V_BAC_S_SEL		(1 << 3)	/* see datasheet */
#define V_SU_SYNC_NT		(1 << 4)	/* 0=sync pulses generated only in TE, 1=in TE and NT */
#define V_SU_2KHZ		(1 << 5)	/* 0=96kHz test tone, 1=2kHz */
#define V_SU_TRI		(1 << 6)	/* 1=tristate output buffer */
#define V_SU_EXCHG		(1 << 7)	/* 1=invert output drivers */

/* R_IRQ_OVIEW bits */
#define V_FIFO_BL0_IRQ		(1 << 0)	/* FIFO 0-3 IRQ */
#define V_FIFO_BL1_IRQ		(1 << 1)	/* FIFO 4-7 IRQ */
#define V_FIFO_BL2_IRQ		(1 << 2)	/* FIFO 8-11 IRQ */
#define V_FIFO_BL3_IRQ		(1 << 3)	/* FIFO 12-15 IRQ */
#define V_MISC_IRQ		(1 << 4)	/* R_MISC_IRQ changed */
#define V_STUP_IRQ		(1 << 5)	/* R_SU_IRQ changed */
#define V_FIFO_BLx_IRQ		(V_FIFO_BL0_IRQ | V_FIFO_BL1_IRQ | V_FIFO_BL2_IRQ | V_FIFO_BL3_IRQ)

/* R_FIRST_FIFO bits */
#define V_FIRST_FIFO_NUM_SHIFT	(1)

/* A_FIFO_SEQ bits */
#define V_NEXT_FIFO_NUM_SHIFT	(1)
#define V_SEQ_END		(1 << 6)

#if (DAHDI_CHUNKSIZE != 8)
#error Sorry, the b400m does not support chunksize != 8
#endif

/* general debug messages */
#define DEBUG_GENERAL 		(1 << 0)
/* emit DTMF detector messages */
#define DEBUG_DTMF 		(1 << 1)
/* emit register read/write, but only if the kernel's DEBUG is defined */
#define DEBUG_REGS 		(1 << 2)
/* emit file operation messages */
#define DEBUG_FOPS  		(1 << 3)
#define DEBUG_ECHOCAN 		(1 << 4)
/* S/T state machine */
#define DEBUG_ST_STATE		(1 << 5)
/* HDLC controller */
#define DEBUG_HDLC		(1 << 6)
/* alarm changes */
#define DEBUG_ALARM		(1 << 7)
/* Timing related changes */
#define DEBUG_TIMING		(1 << 8)

#define DBG			(bri_debug & DEBUG_GENERAL)
#define DBG_DTMF		(bri_debug & DEBUG_DTMF)
#define DBG_REGS		(bri_debug & DEBUG_REGS)
#define DBG_FOPS		(bri_debug & DEBUG_FOPS)
#define DBG_EC			(bri_debug & DEBUG_ECHOCAN)
#define DBG_ST			(bri_debug & DEBUG_ST_STATE)
#define DBG_HDLC		(bri_debug & DEBUG_HDLC)
#define DBG_ALARM		(bri_debug & DEBUG_ALARM)
#define DBG_TIMING		(bri_debug & DEBUG_TIMING)

#define DBG_SPANFILTER		((1 << bspan->port) & bri_spanfilter)

/* #define HARDHDLC_RX */

/* Any static variables not initialized by default should be set
 * to 0 automatically */
int bri_debug;
int bri_spanfilter = 9;
int bri_teignorered = 1;
int bri_alarmdebounce;
int bri_persistentlayer1;
int timingcable;

static int synccard = -1;
static int syncspan = -1;

static const int TIMER_3_MS = 30000;

#define b4_info(b4, format, arg...)         \
	dev_info(&(b4)->wc->vb.pdev->dev , format , ## arg)

/* if defined, swaps ports 2 and 3 on the B400M module */
#define SWAP_PORTS

#define XHFC_T1					0
#define XHFC_T2					1
#define XHFC_T3					2
/* T4 - Special timer, used for debug purposes for monitoring of L1 state during activation attempt. */
#define XHFC_T4					3

#define B400M_CHANNELS_PER_SPAN			3	/* 2 B-channels and 1 D-Channel for each BRI span */
#define B400M_HDLC_BUF_LEN			128	/* arbitrary, just the max # of byts we will send to DAHDI per call */

#define get_F(f1, f2, flen) {				\
	f1 = hfc_readcounter8(b4, A_F1);		\
	f2 = hfc_readcounter8(b4, A_F2);		\
	flen = f1 - f2;					\
							\
	if (flen < 0)					\
		flen += (HFC_FMAX - HFC_FMIN) + 1;	\
	}

#define get_Z(z1, z2, zlen) {				\
	z1 = hfc_readcounter8(b4, A_Z1);		\
	z2 = hfc_readcounter8(b4, A_Z2);		\
	zlen = z1 - z2;					\
							\
	if (zlen < 0)					\
		zlen += (HFC_ZMAX - HFC_ZMIN) + 1;	\
	}

struct b400m_span {
	struct b400m *parent;
	unsigned int port;			/* which S/T port this span belongs to */

	int oldstate;				/* old state machine state */
	int newalarm;				/* alarm to send to DAHDI once alarm timer expires */
	unsigned long alarmtimer;

	unsigned int te_mode:1;			/* 1=TE, 0=NT */
	unsigned int term_on:1;			/* 1= 390 ohm termination enable, 0 = disabled */
	unsigned long hfc_timers[B400M_CHANNELS_PER_SPAN+1];	/* T1, T2, T3 */
	int hfc_timer_on[B400M_CHANNELS_PER_SPAN+1];		/* 1=timer active */
	int fifos[B400M_CHANNELS_PER_SPAN];			/* B1, B2, D <--> host fifo numbers */

	/* HDLC controller fields */
	struct wctdm_span *wspan;  /* pointer to the actual dahdi_span */
	struct dahdi_chan *sigchan;		/* pointer to the signalling channel for this span */
	int sigactive;				/* nonzero means we're in the middle of sending an HDLC frame */
	atomic_t hdlc_pending;			/* hdlc_hard_xmit() increments, hdlc_tx_frame() decrements */
	unsigned int frames_out;
	unsigned int frames_in;

	struct fasthdlc_state rxhdlc;
	int infcs;
	int f_sz;
};

/* This structure exists one per module */
struct b400m {
	char name[10];
	int position;				/* module position in carrier board */
	int b400m_no;				/* 0-based B400M number in system */

	struct wctdm *wc;			/* parent structure */

	spinlock_t reglock;			/* lock for all register accesses */

	unsigned long ticks;

	unsigned long fifo_en_rxint;		/* each bit is the RX int enable for that FIFO */
	unsigned long fifo_en_txint;		/* each bit is the TX int enable for that FIFO */
	unsigned char fifo_irqstatus;		/* top-half ORs in new interrupts, bottom-half ANDs them out */

	int setsyncspan;			/* Span reported from HFC for sync on this card */
	int reportedsyncspan;			/* Span reported from HFC for sync on this card */

	unsigned int running:1;			/* interrupts are enabled */
	unsigned int shutdown:1;		/* 1=bottom half doesn't process anything, just returns */
	unsigned int inited:1;			/* FIXME: temporary */
	unsigned int misc_irq_mask:1;		/* 1= interrupt is valid */

	struct b400m_span spans[4];		/* Individual spans */

	struct workqueue_struct *xhfc_ws;
	struct work_struct xhfc_wq;

	unsigned char irq_oview;		/* copy of r_irq_oview */
	unsigned char fifo_fill;		/* copy of R_FILL_BL0 */

	struct semaphore regsem;		/* lock for low-level register accesses */
	struct semaphore fifosem;		/* lock for fifo accesses */

	unsigned char lastreg;			/* last XHFC register accessed (used to speed up multiple address "hits" */
};

static void hfc_start_st(struct b400m_span *s);
static void hfc_stop_st(struct b400m_span *s);

void b400m_set_dahdi_span(struct b400m *b4, int spanno,
			  struct wctdm_span *wspan)
{
	b4->spans[spanno].wspan = wspan;
	wspan->bspan = &b4->spans[spanno];
}

static inline void flush_hw(void)
{
}

static int xhfc_getreg(struct wctdm *wc, struct wctdm_module *const mod,
		       int addr, u8 *lastreg)
{
	int x;

	if (*lastreg != (unsigned char)addr) {
		wctdm_setreg(wc, mod, 0x60, addr);
		*lastreg = (unsigned char)addr;
	}
	x = wctdm_getreg(wc, mod, 0x80);
	return x;
}

static int xhfc_setreg(struct wctdm *wc, struct wctdm_module *const mod,
		       int addr, int val, u8 *lastreg)
{
	if (*lastreg != (unsigned char)addr) {
		wctdm_setreg(wc, mod, 0x60, addr);
		*lastreg = (unsigned char)addr;
	}
	return wctdm_setreg(wc, mod, 0x00, val);
}

static inline struct wctdm_module *get_mod(struct b400m *b4)
{
	return &b4->wc->mods[b4->position];
}

static int b400m_getreg(struct b400m *b4, int addr)
{
	int x;

	if (down_trylock(&b4->regsem)) {
		if (down_interruptible(&b4->regsem)) {
			b4_info(b4, "b400m_getreg(0x%02x) interrupted\n",
				addr);
			return -1;
		}
	}

	x = xhfc_getreg(b4->wc, get_mod(b4), addr, &b4->lastreg);
	up(&b4->regsem);

	return x;
}

static int b400m_setreg(struct b400m *b4, const int addr, const int val)
{
	int x;

	if (down_trylock(&b4->regsem)) {
		if (down_interruptible(&b4->regsem)) {
			b4_info(b4, "b400m_setreg(0x%02x -> 0x%02x) "
				"interrupted\n", val, addr);
			return -1;
		}
	}

	x = xhfc_setreg(b4->wc, get_mod(b4), addr, val, &b4->lastreg);
	up(&b4->regsem);

	return x;
}


/*
 * A lot of the registers in the XHFC are indexed.
 * this function sets the index, and then writes to the indexed register.
 */
static void b400m_setreg_ra(struct b400m *b4, u8 r, u8 rd, u8 a, u8 ad)
{
	if (down_trylock(&b4->regsem)) {
		if (down_interruptible(&b4->regsem)) {
			b4_info(b4, "b400m_setreg_ra(0x%02x -> 0x%02x) "
				"interrupted\n", a, ad);
			return;
		}
	}

	xhfc_setreg(b4->wc, get_mod(b4), r, rd, &b4->lastreg);
	xhfc_setreg(b4->wc, get_mod(b4), a, ad, &b4->lastreg);
	up(&b4->regsem);
}

static u8 b400m_getreg_ra(struct b400m *b4, u8 r, u8 rd, u8 a)
{
	unsigned char res;
	if (down_trylock(&b4->regsem)) {
		if (down_interruptible(&b4->regsem)) {
			b4_info(b4, "b400m_getreg_ra(0x%02x) interrupted\n",
				a);
			return -1;
		}
	}

	xhfc_setreg(b4->wc, get_mod(b4), r, rd, &b4->lastreg);
	res = xhfc_getreg(b4->wc, get_mod(b4), a, &b4->lastreg);
	up(&b4->regsem);
	return res;
}


/*
 * XHFC-4S GPIO routines
 *
 * the xhfc doesn't use its gpio for anything.  :-)
 */

/*
 * initialize XHFC GPIO.
 * GPIO 0-7 are output, low (unconnected, or used for their primary function).
 */
static void hfc_gpio_init(struct b400m *b4)
{
	/* GPIO0..3,7 are GPIO, 4,5,6 primary function */
	b400m_setreg(b4, R_GPIO_SEL, 0x8f);
	/* GPIO0..7 drivers set low */
	b400m_setreg(b4, R_GPIO_OUT0, 0x00);
	/* GPIO0..7 drivers enabled */
	b400m_setreg(b4, R_GPIO_EN0, 0xff);
	/* all other GPIO set to primary function */
	b400m_setreg(b4, R_GPIO_SEL_BL, 0x00);

}


/* performs a register write and then waits for the HFC "busy" bit to clear
 * NOTE: doesn't actually read status, since busy bit is 1us typically, and
 * we're much, much slower than that. */
static void hfc_setreg_waitbusy(struct b400m *b4, const unsigned int reg,
				const unsigned int val)
{
	b400m_setreg(b4, reg, val);
}

/*
 * reads an 8-bit register over over and over until the same value is read
 * twice, then returns that value.
 */
static unsigned char hfc_readcounter8(struct b400m *b4, const unsigned int reg)
{
	unsigned char r1, r2;
	unsigned long maxwait = 1048576;

	do {
		r1 = b400m_getreg(b4, reg);
		r2 = b400m_getreg(b4, reg);
	} while ((r1 != r2) && maxwait--);

	if (!maxwait) {
		if (printk_ratelimit()) {
			dev_warn(&b4->wc->vb.pdev->dev,
				 "hfc_readcounter8(reg 0x%02x) timed out " \
				 "waiting for data to settle!\n", reg);
		}
	}

	return r1;
}

/* performs a soft-reset of the HFC-4S. */
static void hfc_reset(struct b400m *b4)
{
	unsigned long start;
	int TIMEOUT = HZ;			/* 1s */

	/* Set the FIFOs to 8 128 bytes FIFOs, bidirectional, and set up the
	 * flow controller for channel select mode. */
	/* Note, this reg has to be set *before* the SW reset */
	b400m_setreg(b4, R_FIFO_MD, V_FIFO_MD_01 | V_DF_MD_FSM);

	msleep(1);		/* wait a bit for clock to settle */
/* reset everything, wait 100ms, then allow the XHFC to come out of reset */
	b400m_setreg(b4, R_CIRM, V_SRES);
	flush_hw();

	msleep(100);

	b400m_setreg(b4, R_CIRM, 0x00);
	flush_hw();

	/* wait for XHFC to come out of reset. */
	start = jiffies;
	while (b400m_getreg(b4, R_STATUS) & (V_BUSY | V_PCM_INIT)) {
		if (time_after(jiffies, start + TIMEOUT)) {
			b4_info(b4, "hfc_reset() Module won't come out of "
				"reset... continuing.\n");
			break;
		}
	};

	/* Disable the output clock pin, and also the PLL (it's not needed) */
	b400m_setreg(b4, R_CTRL, 0x00);
}

static void hfc_enable_fifo_irqs(struct b400m *b4)
{
	b400m_setreg(b4, R_IRQ_CTRL, V_FIFO_IRQ_EN | V_GLOB_IRQ_EN);
	flush_hw();
}

static void hfc_enable_interrupts(struct b400m *b4)
{
	b4->running = 1;

	/* mask all misc interrupts */
	b4->misc_irq_mask = 0x01;
	b400m_setreg(b4, R_MISC_IRQMSK, b4->misc_irq_mask);

/* clear any pending interrupts */
	b400m_getreg(b4, R_STATUS);
	b400m_getreg(b4, R_MISC_IRQ);
	b400m_getreg(b4, R_FIFO_BL0_IRQ);
	b400m_getreg(b4, R_FIFO_BL1_IRQ);
	b400m_getreg(b4, R_FIFO_BL2_IRQ);
	b400m_getreg(b4, R_FIFO_BL3_IRQ);

	hfc_enable_fifo_irqs(b4);
}

static inline void hfc_reset_fifo(struct b400m *b4)
{
	hfc_setreg_waitbusy(b4, A_INC_RES_FIFO,
			    V_RES_FIFO | V_RES_LOST | V_RES_FIFO_ERR);
}

static void hfc_setup_fifo(struct b400m *b4, int fifo)
{
	if (fifo < 4) {
		/* TX */
		hfc_setreg_waitbusy(b4, R_FIFO, (fifo << V_FIFO_NUM_SHIFT));
		b400m_setreg(b4, A_CON_HDLC,
			     V_HDLC_IRQ | V_DATA_FLOW_000 | V_IFF);
		hfc_reset_fifo(b4);

		/* RX */
		hfc_setreg_waitbusy(b4, R_FIFO,
				    (fifo << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);
		b400m_setreg(b4, A_CON_HDLC,
			     V_HDLC_IRQ | V_DATA_FLOW_000 | V_IFF);
		hfc_reset_fifo(b4);
	} else {
		/* TX */
		hfc_setreg_waitbusy(b4, R_FIFO, (fifo << V_FIFO_NUM_SHIFT));
		b400m_setreg(b4, A_CON_HDLC,
			     V_HDLC_TRP | V_TRP_NO_IRQ | V_DATA_FLOW_110);
		hfc_reset_fifo(b4);

		/* RX */
		hfc_setreg_waitbusy(b4, R_FIFO,
				    (fifo << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);
		b400m_setreg(b4, A_CON_HDLC,
			     V_HDLC_TRP | V_TRP_NO_IRQ | V_DATA_FLOW_110);
		hfc_reset_fifo(b4);
	}
}

static void hfc_setup_pcm(struct b400m *b4, int port)
{
	int physport;
	int offset;
	int hfc_chan;
	int ts;
#ifdef HARDHDLC_RX
	const int MAX_OFFSET = 2;
#else
	const int MAX_OFFSET = 3;
#endif

#ifdef SWAP_PORTS
	/* swap the middle ports */
	physport = (1 == port) ? 2 : (2 == port) ? 1 : port;
#else
	physport = port;
#endif

	for (offset = 0; offset < MAX_OFFSET; offset++) {
		hfc_chan = (port * 4) + offset;
		ts = (physport * 3) + offset;
		ts += (b4->b400m_no * 12);
		b400m_setreg(b4, R_SLOT, (ts << V_SL_NUM_SHIFT));
		b400m_setreg(b4, A_SL_CFG,
			     (hfc_chan << V_CH_SNUM_SHIFT) |
			     V_ROUT_TX_STIO2);

		if (offset < 2) {
			b400m_setreg(b4, R_SLOT,
				     (ts  << V_SL_NUM_SHIFT) |
				     V_SL_DIR);
			b400m_setreg(b4, A_SL_CFG,
				     (hfc_chan << V_CH_SNUM_SHIFT) |
				     V_ROUT_RX_STIO1 | V_CH_SDIR);
		}
	}
}

#ifdef SWAP_PORTS
#ifdef HARDHDLC_RX
static const int fifos[24] = {0, 0, 2, 2, 1, 1, 3, 3, 4, 4, 4, 4, 6, 6, 6, 6,
			      5, 5, 5, 5, 7, 7, 7, 7 };
#else
static const int fifos[24] = {0, 4, 2, 6, 1, 5, 3, 7, 4, 4, 4, 4, 6, 6, 6, 6,
			      5, 5, 5, 5, 7, 7, 7, 7 };
#endif
static const int hfc_chans[12] = {2, 10, 6, 14, 0, 1, 8, 9, 4, 5, 12, 13 };
#else
#ifdef HARDHDLC_RX
static const int fifos[24] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
			      6, 6, 6, 6, 7, 7, 7, 7 };
#else
static const int fifos[24] = {0, 4, 1, 5, 2, 6, 3, 7, 4, 4, 4, 4, 5, 5, 5, 5,
			      6, 6, 6, 6, 7, 7, 7, 7 };
#endif
static const int hfc_chans[12] = { 2, 6, 10, 14, 0, 1, 4, 5, 8, 9, 12, 13 };
#endif

static void hfc_setup_fifo_arrays(struct b400m *b4, int fifo)
{
	int val;
	if (!fifo) {
		val = (fifos[fifo] << V_FIRST_FIFO_NUM_SHIFT) | (fifo & 1);
		b400m_setreg(b4, R_FIRST_FIFO, val);
	} else {
#ifdef HARDHDLC_RX
		val = (fifos[fifo] << V_NEXT_FIFO_NUM_SHIFT) | (fifo & 1);
#else
		val = (fifo < 8) ? (fifos[fifo] << V_NEXT_FIFO_NUM_SHIFT) :
				   (fifos[fifo] << V_NEXT_FIFO_NUM_SHIFT) |
					(fifo&1);
#endif
		b400m_setreg(b4, A_FIFO_SEQ, val);
	}

	b400m_setreg(b4, R_FSM_IDX, fifo);
	val = (fifo < 8) ? (hfc_chans[fifo>>1] << V_CH_FNUM_SHIFT) :
			   (hfc_chans[fifo>>1] << V_CH_FNUM_SHIFT) |
				(fifo & 1);

	b400m_setreg(b4, A_CHANNEL, val);
	b400m_setreg(b4, A_SUBCH_CFG, 0x02);
}

static void hfc_setup_fsm(struct b400m *b4)
{
	int chan, fifo, port, offset;
#ifdef SWAP_PORTS
	const int chan_to_fifo[12] = { 4, 4, 0, 6, 6, 2, 5, 5, 1, 7, 7, 3 };
#else
	const int chan_to_fifo[12] = { 4, 4, 0, 5, 5, 1, 6, 6, 2, 7, 7, 3 };
#endif

	for (port = 0; port < 4; port++) {
		for (offset = 0; offset < 3; offset++) {
			b4->spans[port].fifos[offset] =
					chan_to_fifo[(port * 3) + offset];
		}
	}

	for (chan = 0; chan < ARRAY_SIZE(fifos); chan++)
		hfc_setup_fifo_arrays(b4, chan);

	b400m_setreg(b4, A_FIFO_SEQ, V_SEQ_END);

	for (fifo = 0; fifo < 8; fifo++)
		hfc_setup_fifo(b4, fifo);

	for (port = 0; port < 4; port++)
		hfc_setup_pcm(b4, port);
}

/* takes a read/write fifo pair and optionally resets it, optionally enabling
 * the rx/tx interrupt */
static void hfc_reset_fifo_pair(struct b400m *b4, int fifo,
				int reset, int force_no_irq)
{
	unsigned char b;

	if (down_interruptible(&b4->fifosem)) {
		b4_info(b4, "Unable to retrieve fifo sem\n");
		return;
	}
	b = (!force_no_irq && b4->fifo_en_txint & (1 << fifo)) ?
		V_FIFO_IRQMSK : 0;
	hfc_setreg_waitbusy(b4, R_FIFO, (fifo << V_FIFO_NUM_SHIFT));

	if (fifo < 4)
		b |= V_MIX_IRQ;

	b400m_setreg(b4, A_FIFO_CTRL, b);

	if (reset)
		hfc_reset_fifo(b4);

	b = (!force_no_irq && b4->fifo_en_rxint & (1 << fifo)) ?
		V_FIFO_IRQMSK : 0;
	hfc_setreg_waitbusy(b4, R_FIFO,
			    (fifo << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);

	if (fifo < 4)
		b |= V_MIX_IRQ;

	b400m_setreg(b4, A_FIFO_CTRL, b);

	if (reset)
		hfc_reset_fifo(b4);

	up(&b4->fifosem);
}

static void xhfc_set_sync_src(struct b400m *b4, int port)
{
	int b;

	/* -2 means we need to go back and try again later */
	if (port == -2)
		return;

	if (port == b4->setsyncspan)
		return;
	else
		b4->setsyncspan = port;

	b4_info(b4, "xhfc_set_sync_src - modpos %d: setting sync to "
		"be port %d\n", b4->position, port);

	if (port == -1) 		/* automatic */
		b = 0;
	else {
#ifdef SWAP_PORTS
		port = (1 == port) ? 2 : (2 == port) ? 1 : port;
#endif
		b = (port & V_SYNC_SEL_MASK) | V_MAN_SYNC;
	}

	b400m_setreg(b4, R_SU_SYNC, b);
}

static void wctdm_change_card_sync_src(struct wctdm *wc, int newsrc, int master)
{
	int newctlreg;

	newctlreg = wc->ctlreg;

	if (master)
		newctlreg |= (1 << 5);
	else
		newctlreg &= ~(1 << 5);

	newctlreg &= 0xfc;

	newctlreg |= newsrc;

	if (DBG_TIMING) {
		dev_info(&wc->vb.pdev->dev,
			 "Final ctlreg before swap: %02x\n", newctlreg);
	}

	wc->ctlreg = newctlreg;
	wc->oldsync = newsrc;

	msleep(10);
}

static void wctdm_change_system_sync_src(int oldsync, int oldspan,
					 int newsync, int newspan)
{
	struct wctdm *wc;
	struct wctdm *oldsyncwc = NULL, *newsyncwc = NULL;
	int newspot;
	int i;
	int max_latency = 0;

	if (oldsync > -1)
		oldsyncwc = ifaces[oldsync];

	if (newsync > -1)
		newsyncwc = ifaces[newsync];

	if (newsync == -1) {
		BUG_ON(!ifaces[0]);

		newsyncwc = ifaces[0];
		newsync = 0;
	}

	newspot = (-1 == newspan) ? 0 : 2 | (newspan >> 2);

	if ((oldsync == newsync) && (oldspan == newspan)) {
		dev_info(&newsyncwc->vb.pdev->dev,
			 "No need for timing change.  All is same\n");
		return;
	}

	/* First we set all sources to local timing */
	for (i = 0; i < WC_MAX_IFACES; i++) {
		wc = ifaces[i];
		if ((wc != oldsyncwc) && wc) {
			wctdm_change_card_sync_src(wc, 0, 0);
			if (voicebus_current_latency(&wc->vb) > max_latency)
				max_latency = voicebus_current_latency(&wc->vb);
		}
	}
	msleep(max_latency << 1);

	/* Set the old sync source to local timing, not driving timing */
	if (oldsyncwc) {
		wctdm_change_card_sync_src(oldsyncwc, 0, 0);
		msleep(voicebus_current_latency(&oldsyncwc->vb) << 1);
	}

	dev_info(&newsyncwc->vb.pdev->dev,
		 "Setting new card %d now to be timing master\n", newsync);
	/* Finally, set the new sync source to broadcast master timing */
	wctdm_change_card_sync_src(newsyncwc, newspot, 1);
	msleep(voicebus_current_latency(&newsyncwc->vb) << 1);

	/* Last we double verify and set all the remaining cards to be timing
	 * slaves */
	for (i = 0; (i < WC_MAX_IFACES) && ifaces[i]; i++) {
		wc = ifaces[i];
		if (i == newsync)
			continue;

		dev_info(&wc->vb.pdev->dev,
			 "Setting card %d to be timing slave\n", i);
		wctdm_change_card_sync_src(wc, 1, 0);
	}
	msleep(max_latency << 1);
	synccard = newsync;
	syncspan = newspan;
}

static int xhfc_find_sync_with_timingcable(struct b400m *b4)
{
	struct wctdm *wc = b4->wc;
	int i, j, osrc, src = -1;
	int lowestprio = 10000;
	int lowestcard = -1;

	if (down_trylock(&ifacelock)) {
		set_bit(WCTDM_CHECK_TIMING, &wc->checkflag);
		return -2;
	}

	for (j = 0; j < WC_MAX_IFACES && ifaces[j]; j++) {
		if (is_initialized(ifaces[j])) {
			set_bit(WCTDM_CHECK_TIMING, &wc->checkflag);
			osrc = -2;
			goto out;
		} else {
			for (i = 0; i < (MAX_SPANS - 1); i++) {
				struct wctdm_span *wspan = ifaces[j]->spans[i];
				if (wspan &&
				    wspan->timing_priority &&
				    !wspan->span.alarms &&
				    (wspan->timing_priority <
				     lowestprio)) {
					src = i;
					lowestprio = wspan->timing_priority;
					lowestcard = j;
				}
			}
		}
	}

	if (lowestcard != synccard) {
		b4_info(b4, "Found new timing master, card "
			"%d.  Old is card %d\n", lowestcard, synccard);
	} else if (src != syncspan) {
		b4_info(b4, "Timing change, but only from %d to %d on "
			"card %d\n", syncspan, src, lowestcard);
	}

	wctdm_change_system_sync_src(synccard, syncspan,
				     lowestcard, src);

	osrc = -1;

	if (wc == ifaces[lowestcard]) {
		if (src < (b4->position + 4) && (src >= b4->position))
			osrc = src - b4->position;
	}

out:
	up(&ifacelock);

	return osrc;
}

static int xhfc_find_sync_without_timingcable(struct b400m *b4)
{
	struct wctdm *wc = b4->wc;
	int i, osrc, src = -1;
	int lowestprio = 10000;
	int newctlregmux;

	if (down_trylock(&wc->syncsem)) {
		set_bit(WCTDM_CHECK_TIMING, &wc->checkflag);
		return -2;
	}

	/* Find lowest slave timing priority on digital spans */
	for (i = 0; i < (MAX_SPANS - 1); i++) {
		struct wctdm_span *const wspan = wc->spans[i];
		if (wspan && wspan->timing_priority &&
		    !wspan->span.alarms &&
		    (wspan->timing_priority < lowestprio)) {
			src = i;
			lowestprio = wspan->timing_priority;
		}
	}

	if (src < 0) {
		if (DBG_TIMING)
			b4_info(b4, "Picked analog span\n");
		osrc = src;
		goto check_card_timing;
	} else {
		if (DBG_TIMING) {
			b4_info(b4, "Picked span offset %d to be timing "
				"source\n", src);
		}
	}

	osrc = ((src < (b4->position + 4)) && (src >= b4->position)) ?
			src - b4->position : -1;

	if (DBG_TIMING) {
		b4_info(b4, "For b4->position %d timing is %d\n",
			b4->position, osrc);
	}

check_card_timing:

	if (src != -1)
		newctlregmux = 2 | (src >> 2);
	else
		newctlregmux = 0;

	if ((newctlregmux & 3) != (wc->ctlreg & 3)) {
		if (DBG_TIMING) {
			b4_info(b4, "!!!Need to change timing "
				"on baseboard to spot %d!!!\n",
				src >> 2);
		}

		wctdm_change_card_sync_src(wc, newctlregmux, 0);
	} else {
		if (DBG_TIMING) {
			dev_info(&b4->wc->vb.pdev->dev, "!!!No need to change timing " \
			       "on baseboard to spot %d, already there!!!\n",
			       src >> 2);

		}
	}


	up(&wc->syncsem);

	return osrc;
}

/*
 * Finds the highest-priority sync span that is not in alarm and returns it.
 * Note: the span #s in b4->spans[].sync are 1-based, and this returns a
 * 0-based span, or -1 if no spans are found.
 */
static inline int xhfc_find_sync(struct b400m *b4)
{
	if (timingcable)
		return xhfc_find_sync_with_timingcable(b4);
	else
		return xhfc_find_sync_without_timingcable(b4);
}

/*
 * allocates memory and pretty-prints a given S/T state engine state to it.
 * calling routine is responsible for freeing the pointer returned!  Performs
 * no hardware access whatsoever, but does use GFP_KERNEL so do not call from
 * IRQ context.  if full == 1, prints a "full" dump; otherwise just prints
 * current state.
 */
static char *hfc_decode_st_state(struct b400m *b4, struct b400m_span *span,
				 unsigned char state, int full)
{
	int nt, sta;
	char s[128], *str;
	const char *ststr[2][16] = {	/* TE, NT */
		{ "RESET",       "?",           "SENSING", "DEACT.",
		  "AWAIT.SIG",   "IDENT.INPUT", "SYNCD",    "ACTIVATED",
		  "LOSTFRAMING", "?",           "?",        "?",
		  "?",           "?",           "?",        "?" },
		{ "RESET",       "DEACT.",      "PEND.ACT", "ACTIVE",
		  "PEND.DEACT",  "?",           "?",        "?",
		  "?",           "?",           "?",        "?",
		  "?",           "?",           "?",        "?" }
	};

	str = kmalloc(256, GFP_KERNEL);
	if (!str) {
		dev_warn(&b4->wc->vb.pdev->dev,
			 "could not allocate mem for ST state decode " \
			 "string!\n");
		return NULL;
	}

	nt = (span->te_mode == 0);
	sta = (state & V_SU_STA_MASK);

	sprintf(str, "P%d: %s state %c%d (%s)", span->port + 1,
		(nt ? "NT" : "TE"), (nt ? 'G' : 'F'), sta,
		ststr[nt][sta]);

	if (full) {
		sprintf(s, " SYNC: %s, RX INFO0: %s",
			((state & V_SU_FR_SYNC) ? "yes" : "no"),
			((state & V_SU_INFO0) ? "yes" : "no"));
		strcat(str, s);

		if (nt) {
			sprintf(s, ", T2 %s, auto G2->G3: %s",
				((state & V_SU_T2_EXP) ? "expired" : "OK"),
				((state & V_G2_G3) ? "yes" : "no"));
			strcat(str, s);
		}
	}

	return str;
}

/*
 * sets an S/T port state machine to a given state.  if 'auto' is nonzero,
 * will put the state machine back in auto mode after setting the state.
 */
static void hfc_handle_state(struct b400m_span *s);
static void hfc_force_st_state(struct b400m *b4, struct b400m_span *s,
			       int state, int resume_auto)
{
	b400m_setreg_ra(b4, R_SU_SEL, s->port, A_SU_WR_STA,
			state | V_SU_LD_STA);

	if (resume_auto)
		b400m_setreg_ra(b4, R_SU_SEL, s->port, A_SU_WR_STA, state);

	if (DBG_ST && ((1 << s->port) & bri_spanfilter)) {
		char *x;

		x = hfc_decode_st_state(b4, s, state, 1);
		b4_info(b4, "forced port %d to state %d (auto: %d), "
			 "new decode: %s\n", s->port + 1, state,
			 resume_auto, x);
		kfree(x);
	}

	/* make sure that we activate any timers/etc needed by this state
	 * change */
	hfc_handle_state(s);
}

/* figures out what to do when an S/T port's timer expires. */
static void hfc_timer_expire(struct b400m_span *s, int t_no)
{
	struct b400m *b4 = s->parent;

	if (DBG_ST && ((1 << s->port) & bri_spanfilter)) {
		b4_info(b4, "%lu: hfc_timer_expire, Port %d T%d "
			"expired (value=%lu ena=%d)\n", b4->ticks,
			s->port + 1, t_no + 1, s->hfc_timers[t_no],
			s->hfc_timer_on[t_no]);
	}
	/*
	 * there are three timers associated with every HFC S/T port.
	 *
	 * T1 is used by the NT state machine, and is the maximum time the NT
	 * side should wait for G3 (active) state.
	 *
	 * T2 is not actually used in the driver, it is handled by the HFC-4S
	 * internally.
	 *
	 * T3 is used by the TE state machine; it is the maximum time the TE
	 * side should wait for the INFO4 (activated) signal.
	 */

	/* First, disable the expired timer; hfc_force_st_state() may activate
	 * it again. */
	s->hfc_timer_on[t_no] = 0;

	switch (t_no) {
	case XHFC_T1:	/* switch to G4 (pending deact.), resume auto mode */
		hfc_force_st_state(b4, s, 4, 1);
		break;
	case XHFC_T2:	/* switch to G1 (deactivated), resume auto mode */
		hfc_force_st_state(b4, s, 1, 1);
		break;
	case XHFC_T3:	/* switch to F3 (deactivated), resume auto mode */
		hfc_stop_st(s);
		if (bri_persistentlayer1)
			hfc_start_st(s);
		break;
	case XHFC_T4:	/* switch to F3 (deactivated), resume auto mode */
		hfc_handle_state(s);
		s->hfc_timers[XHFC_T4] = b4->ticks + 1000;
		s->hfc_timer_on[XHFC_T4] = 1;
		break;
	default:
		if (printk_ratelimit()) {
			dev_warn(&b4->wc->vb.pdev->dev,
				 "hfc_timer_expire found an unknown expired "
				 "timer (%d)??\n", t_no);
		}
	}
}

/*
 * Run through the active timers on a card and deal with any expiries.
 * Also see if the alarm debounce time has expired and if it has, tell DAHDI.
 */
static void hfc_update_st_timers(struct b400m *b4)
{
	int i, j;
	struct b400m_span *s;

	for (i = 0; i < 4; i++) {
		s = &b4->spans[i];

		for (j = XHFC_T1; j <= XHFC_T4; j++) {

			/* we don't really do timer2, it is expired by the
			 * state change handler */
			if (j == XHFC_T2)
				continue;

			if (s->hfc_timer_on[j] &&
			    time_after_eq(b4->ticks, s->hfc_timers[j]))
				hfc_timer_expire(s, j);
		}

		if (s->wspan && s->newalarm != s->wspan->span.alarms &&
		    time_after_eq(b4->ticks, s->alarmtimer)) {
			s->wspan->span.alarms = s->newalarm;
			if ((!s->newalarm && bri_teignorered) || (!bri_teignorered))
				dahdi_alarm_notify(&s->wspan->span);

			if (DBG_ALARM) {
				dev_info(&b4->wc->vb.pdev->dev, "span %d: alarm " \
					 "%d debounced\n", i + 1,
					 s->newalarm);
			}
			set_bit(WCTDM_CHECK_TIMING, &b4->wc->checkflag);
		}
	}

	if (test_and_clear_bit(WCTDM_CHECK_TIMING, &b4->wc->checkflag))
		xhfc_set_sync_src(b4, xhfc_find_sync(b4));
}

/* this is the driver-level state machine for an S/T port */
static void hfc_handle_state(struct b400m_span *s)
{
	struct b400m *b4;
	unsigned char state, sta;
	int nt, newsync, oldalarm;
	unsigned long oldtimer;

	b4 = s->parent;
	nt = !s->te_mode;

	state = b400m_getreg_ra(b4, R_SU_SEL, s->port, A_SU_RD_STA);
	sta = (state & V_SU_STA_MASK);

	if (DBG_ST && ((1 << s->port) & bri_spanfilter)) {
		char *x;

		x = hfc_decode_st_state(b4, s, state, 1);
		b4_info(b4, "port %d A_SU_RD_STA old=0x%02x "
			"now=0x%02x, decoded: %s\n", s->port + 1,
			s->oldstate, state, x);
		kfree(x);
	}

	oldalarm = s->newalarm;
	oldtimer = s->alarmtimer;

	if (nt) {
		switch (sta) {
		default:		/* Invalid NT state */
		case 0x0:		/* NT state G0: Reset */
		case 0x1:		/* NT state G1: Deactivated */
		case 0x4:		/* NT state G4: Pending Deactivation */
			s->newalarm = DAHDI_ALARM_RED;
			break;
		case 0x2:		/* NT state G2: Pending Activation */
			s->newalarm = DAHDI_ALARM_YELLOW;
			break;
		case 0x3:		/* NT state G3: Active */
			s->hfc_timer_on[XHFC_T1] = 0;
			s->newalarm = 0;
			break;
		}
	} else {
		switch (sta) {
		default:		/* Invalid TE state */
		case 0x0:		/* TE state F0: Reset */
		case 0x2:		/* TE state F2: Sensing */
		case 0x3:		/* TE state F3: Deactivated */
		case 0x4:		/* TE state F4: Awaiting Signal */
		case 0x8:		/* TE state F8: Lost Framing */
			s->newalarm = DAHDI_ALARM_RED;
			break;
		case 0x5:		/* TE state F5: Identifying Input */
		case 0x6:		/* TE state F6: Synchronized */
			s->newalarm = DAHDI_ALARM_YELLOW;
			break;
		case 0x7:		/* TE state F7: Activated */
			s->hfc_timer_on[XHFC_T3] = 0;
			s->hfc_timer_on[XHFC_T4] = 0;
			s->newalarm = 0;
			break;
		}
	}

	s->alarmtimer = b4->ticks + bri_alarmdebounce;
	s->oldstate = state;

	if (DBG_ALARM) {
		b4_info(b4, "span %d: old alarm %d expires %ld, "
			"new alarm %d expires %ld\n", s->port + 1, oldalarm,
			oldtimer, s->newalarm, s->alarmtimer);
	}

	/* we only care about T2 expiry in G4. */
	if (nt && (sta == 4) && (state & V_SU_T2_EXP)) {
		if (s->hfc_timer_on[XHFC_T2])
			hfc_timer_expire(s, XHFC_T2);	/* handle T2 expiry */
	}

	/* If we're in F3 and receiving INFO0, start T3 and jump to F4 */
	if (!nt && (sta == 3) && (state & V_SU_INFO0)) {
		if (bri_persistentlayer1) {
			s->hfc_timers[XHFC_T3] = b4->ticks + TIMER_3_MS;
			s->hfc_timer_on[XHFC_T3] = 1;
			if (DBG_ST) {
				b4_info(b4, "port %d: receiving "
					"INFO0 in state 3, setting T3 and "
					"jumping to F4\n", s->port + 1);
			}
			hfc_start_st(s);
		}
	}

	/* read in R_BERT_STA to determine where our current sync source is */
	newsync = b400m_getreg(b4, R_BERT_STA) & 0x07;
	if (newsync != b4->reportedsyncspan) {
		if (DBG_TIMING) {
			if (newsync == 5) {
				b4_info(b4, "new card sync source: SYNC_I\n");
			} else {
				b4_info(b4, "Card position %d: new "
					"sync source: port %d\n",
					b4->position, newsync);
			}
		}

		b4->reportedsyncspan = newsync;
	}
}

static void hfc_stop_all_timers(struct b400m_span *s)
{
	s->hfc_timer_on[XHFC_T4] = 0;
	s->hfc_timer_on[XHFC_T3] = 0;
	s->hfc_timer_on[XHFC_T2] = 0;
	s->hfc_timer_on[XHFC_T1] = 0;
}

static void hfc_stop_st(struct b400m_span *s)
{
	struct b400m *b4 = s->parent;

	hfc_stop_all_timers(s);

	b400m_setreg_ra(b4, R_SU_SEL, s->port, A_SU_WR_STA, V_SU_ACT_DEACTIVATE);
}

/*
 * resets an S/T interface to a given NT/TE mode
 */
static void hfc_reset_st(struct b400m_span *s)
{
	int b;
	struct b400m *b4;

	b4 = s->parent;

	hfc_stop_st(s);

	/* force state G0/F0 (reset), then force state 1/2
	 * (deactivated/sensing) */
	b400m_setreg_ra(b4, R_SU_SEL, s->port, A_SU_WR_STA, V_SU_LD_STA);
	flush_hw();			/* make sure write hit hardware */

	s->wspan->span.alarms = DAHDI_ALARM_RED;
	s->newalarm = DAHDI_ALARM_RED;
	dahdi_alarm_notify(&s->wspan->span);

	/* set up the clock control register.  Must be done before we activate
	 * the interface. */
	if (s->te_mode)
		b = 0x0e;
	else
		b = 0x0c | (6 << V_SU_SMPL_SHIFT);

	b400m_setreg(b4, A_SU_CLK_DLY, b);

	/* set TE/NT mode, enable B and D channels. */
	b400m_setreg(b4, A_SU_CTRL0, V_B1_TX_EN | V_B2_TX_EN |
		     (s->te_mode ? 0 : V_SU_MD) | V_ST_PU_CTRL);
	b400m_setreg(b4, A_SU_CTRL1, V_G2_G3_EN);
	b400m_setreg(b4, A_SU_CTRL2, V_B1_RX_EN | V_B2_RX_EN);
	b400m_setreg(b4, A_ST_CTRL3, (0x7c << 1));

	/* enable the state machine. */
	b400m_setreg(b4, A_SU_WR_STA, 0x00);
	flush_hw();
}

static void hfc_start_st(struct b400m_span *s)
{
	struct b400m *b4 = s->parent;

	b400m_setreg_ra(b4, R_SU_SEL, s->port, A_SU_WR_STA, V_SU_ACT_ACTIVATE);

	/* start T1 if in NT mode, T3 if in TE mode */
	if (s->te_mode) {
		/* 500ms wait first time, TIMER_3_MS afterward. */
		s->hfc_timers[XHFC_T3] = b4->ticks + TIMER_3_MS;
		s->hfc_timer_on[XHFC_T3] = 1;
		s->hfc_timer_on[XHFC_T1] = 0;

		s->hfc_timers[XHFC_T4] = b4->ticks + 1000;
		s->hfc_timer_on[XHFC_T4] = 1;

		if (DBG_ST) {
			b4_info(b4, "setting port %d t3 timer to %lu\n",
				 s->port + 1, s->hfc_timers[XHFC_T3]);
		}
	} else {
		static const int TIMER_1_MS = 2000;
		s->hfc_timers[XHFC_T1] = b4->ticks + TIMER_1_MS;
		s->hfc_timer_on[XHFC_T1] = 1;
		s->hfc_timer_on[XHFC_T3] = 0;
		if (DBG_ST) {
			b4_info(b4, "setting port %d t1 timer to %lu\n",
				 s->port + 1, s->hfc_timers[XHFC_T1]);
		}
	}
}

/*
 * read in the HFC GPIO to determine each port's mode (TE or NT).
 * Then, reset and start the port.
 * the flow controller should be set up before this is called.
 */
static int hdlc_start(struct b400m *b4, int fifo);
static void hfc_init_all_st(struct b400m *b4)
{
	int i;
	struct b400m_span *s;

	for (i = 0; i < 4; i++) {
		s = &b4->spans[i];
		s->parent = b4;

#ifdef SWAP_PORTS
		s->port = (1 == i) ? 2 : (2 == i) ? 1 : i;
#else
		s->port = i;
#endif
		s->te_mode = 1;

		hdlc_start(b4, s->fifos[2]);
	}

}

/* NOTE: assumes fifo lock is held */
#define debug_fz(b4, fifo, prefix, buf) \
do { \
	sprintf(buf, "%s: (fifo %d): f1/f2/flen=%d/%d/%d, " \
		"z1/z2/zlen=%d/%d/%d\n", prefix, fifo, f1, f2, flen, z1, \
		z2, zlen); \
} while (0)

/* enable FIFO RX int and reset the FIFO */
static int hdlc_start(struct b400m *b4, int fifo)
{
	b4->fifo_en_txint |= (1 << fifo);
	b4->fifo_en_rxint |= (1 << fifo);

	hfc_reset_fifo_pair(b4, fifo, 1, 0);
	return 0;
}

#ifdef HARDHDLC_RX

/**
 * hdlc_signal_complete() - Signal dahdi that we have a complete frame.
 *
 * @bpan:	The span which received the frame.
 * @stat: 	The frame status from the XHFC controller.
 *
 */
static void hdlc_signal_complete(struct b400m_span *bspan, u8 stat)
{
	struct b400m *b4 = bspan->parent;

	/* if STAT != 0, indicates bad frame */
	if (stat != 0x00) {
		if (DBG_HDLC && DBG_SPANFILTER) {
			b4_info(b4, "(span %d) STAT=0x%02x indicates " \
				 "frame problem: %s\n", bspan->port + 1, stat,
				 (0xff == stat) ? "HDLC Abort" : "Bad FCS");
		}

		dahdi_hdlc_abort(bspan->sigchan, (0xff == stat) ?
				 DAHDI_EVENT_ABORT : DAHDI_EVENT_BADFCS);
	/* STAT == 0, means frame was OK */
	} else {
		if (DBG_HDLC && DBG_SPANFILTER) {
			b4_info(b4, "(span %d) Frame %d is good!\n",
				 bspan->port + 1, bspan->frames_in);
		}
		dahdi_hdlc_finish(bspan->sigchan);
	}
}

/*
 * Inner loop for D-channel receive function.  Retrieves HDLC data from the
 * hardware.  If the hardware indicates that the frame is complete, we check
 * the HDLC engine's STAT byte and update DAHDI as needed.
 *
 * Returns the number of HDLC frames left in the FIFO, or -1 if we couldn't
 * get the lock.
 */
static int hdlc_rx_frame(struct b400m_span *bspan)
{
	int fifo, i, j, x, zleft;
	int z1, z2, zlen, f1, f2, flen, new_flen;
	unsigned char buf[B400M_HDLC_BUF_LEN];
	char debugbuf[256];
	struct b400m *b4 = bspan->parent;

	fifo = bspan->fifos[2];

	if (DBG_HDLC && DBG_SPANFILTER)
		b4_info(b4, "hdlc_rx_frame fifo %d: start\n", fifo);

	if (down_trylock(&b4->fifosem) && DBG_HDLC && DBG_SPANFILTER) {
		b4_info(b4, "rx_frame: fifo %d 1: couldn't get lock\n",
		       fifo);
		return -1;
	}

	hfc_setreg_waitbusy(b4, R_FIFO,
			    (fifo << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);
	get_F(f1, f2, flen);
	get_Z(z1, z2, zlen);
	debug_fz(b4, fifo, "hdlc_rx_frame", debugbuf);
	up(&b4->fifosem);

	if (DBG_HDLC && DBG_SPANFILTER)
		pr_info("%s", debugbuf);

	/* if we have at least one complete frame, increment zleft to include
	 * status byte */
	zleft = zlen;
	if (flen)
		zleft++;

	do {
		if (zleft > B400M_HDLC_BUF_LEN)
			j = B400M_HDLC_BUF_LEN;
		else
			j = zleft;

		if (down_trylock(&b4->fifosem) && DBG_HDLC && DBG_SPANFILTER) {
			b4_info(b4,
			       "rx_frame fifo %d 2: couldn't get lock\n",
			       fifo);
			return -1;
		}
		hfc_setreg_waitbusy(b4, R_FIFO,
				    (fifo << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);
		for (i = 0; i < j; i++)
			buf[i] = b400m_getreg(b4, A_FIFO_DATA);
		up(&b4->fifosem);

		/* don't send STAT byte to DAHDI */
		x = j;
		if (bspan->sigchan) {
			if ((j != B400M_HDLC_BUF_LEN) && flen)
				x--;
			if (x)
				dahdi_hdlc_putbuf(bspan->sigchan, buf, x);
		}

		zleft -= j;

		if (DBG_HDLC && DBG_SPANFILTER) {
			b4_info(b4, "transmitted %d bytes to dahdi, " \
			       "zleft=%d\n", x, zleft);
		}

		if (DBG_HDLC && DBG_SPANFILTER) {
			/* !!! */
			b4_info(b4, "hdlc_rx_frame(span %d): " \
				 "z1/z2/zlen=%d/%d/%d, zleft=%d\n",
				 bspan->port + 1, z1, z2, zlen, zleft);
			for (i = 0; i < j; i++) {
				b4_info(b4, "%02x%c", buf[i],
				       (i < (j - 1)) ? ' ' : '\n');
			}
		}
	} while (zleft > 0);

	/* Frame received, increment F2 and get an updated count of frames
	 * left */
	if (down_trylock(&b4->fifosem) && DBG_HDLC && DBG_SPANFILTER) {
		b4_info(b4, "rx_frame fifo %d 3: couldn't get lock\n",
		       fifo);
		return 0;
	}

	/* go get the F count again, just in case another frame snuck in while
	 * we weren't looking. */
	if (flen) {
		hfc_setreg_waitbusy(b4, A_INC_RES_FIFO, V_INC_F);
		++bspan->frames_in;
		get_F(f1, f2, new_flen);
	} else
		new_flen = flen;

	up(&b4->fifosem);

	/* If this channel is not configured with a signalling span we don't
	 * need to notify the rest of dahdi about this frame. */
	if (!bspan->sigchan) {
		if (DBG_HDLC && DBG_SPANFILTER) {
			b4_info(b4, "hdlc_rx_frame fifo %d: " \
			       "new_flen %d, early end.\n", fifo, new_flen);
		}
		return new_flen;
	}

	if (flen) {
		/*  disable < 3 check for now */
		if (0 && zlen < 3) {
			if (DBG_HDLC && DBG_SPANFILTER)
				b4_info(b4, "odd, zlen less then 3?\n");
			dahdi_hdlc_abort(bspan->sigchan, DAHDI_EVENT_ABORT);
		} else {
			hdlc_signal_complete(bspan, buf[i - 1]);
		}
	}

	if (DBG_HDLC && DBG_SPANFILTER) {
		b4_info(b4, "hdlc_rx_frame fifo %d: new_flen=%d end.\n",
		       fifo, new_flen);
	}

	return new_flen;
}

#endif /* HARDHDLC_RX */


/*
 * Takes one blob of data from DAHDI and shoots it out to the hardware.  The
 * blob may or may not be a complete HDLC frame.  If it isn't, the D-channel
 * FIFO interrupt handler will take care of pulling the rest.  Returns nonzero
 * if there is still data to send in the current HDLC frame.
 */
static int hdlc_tx_frame(struct b400m_span *bspan)
{
	struct b400m *b4 = bspan->parent;
	int res, i, fifo;
	int z1, z2, zlen;
	int f1 = -1, f2 = -1, flen = -1;
	unsigned char buf[B400M_HDLC_BUF_LEN];
	unsigned int size = ARRAY_SIZE(buf);
	char debugbuf[256];

	/* if we're ignoring TE red alarms and we are in alarm, restart the
	 * S/T state machine */
	if (bspan->te_mode && (bspan->newalarm != 0)) {
		hfc_start_st(bspan);
	}

	fifo = bspan->fifos[2];
	res = dahdi_hdlc_getbuf(bspan->sigchan, buf, &size);

	if (down_interruptible(&b4->fifosem)) {
		static int arg;
		b4_info(b4, "b400m: arg (%d), grabbed data from DAHDI " \
		       "but couldn't grab the lock!\n", ++arg);
		/* TODO: Inform DAHDI that we have grabbed data and can't use
		 * it */
		dahdi_hdlc_abort(bspan->sigchan, DAHDI_EVENT_OVERRUN);
		return 1;		/* return 1 so we keep trying */
	}
	hfc_setreg_waitbusy(b4, R_FIFO, (fifo << V_FIFO_NUM_SHIFT));

	get_Z(z1, z2, zlen);
	debug_fz(b4, fifo, __func__, debugbuf);

	/* TODO: check zlen, etc. */
	if ((HFC_ZMAX-zlen) < size) {
		static int arg;
		b4_info(b4, "b400m: arg (%d), zlen (%d) < what we " \
		       "grabbed from DAHDI (%d)!\n", ++arg, zlen, size);
		size = zlen;
		dahdi_hdlc_abort(bspan->sigchan, DAHDI_EVENT_OVERRUN);
	}

	if (size > 0) {
		bspan->sigactive = 1;

		for (i = 0; i < size; i++)
			b400m_setreg(b4, A_FIFO_DATA, buf[i]);
		/*
		 * If we got a full frame from DAHDI, increment F and
		 * decrement our HDLC pending counter.  Otherwise, select the
		 * FIFO again (to start transmission) and make sure the TX IRQ
		 * is enabled so we will get called again to finish off the
		 * data
		 */
		if (res != 0) {
			++bspan->frames_out;
			bspan->sigactive = 0;
			hfc_setreg_waitbusy(b4, A_INC_RES_FIFO, V_INC_F);
			atomic_dec(&bspan->hdlc_pending);
		} else {
			hfc_setreg_waitbusy(b4, R_FIFO,
					    (fifo << V_FIFO_NUM_SHIFT));
		}
	}

	up(&b4->fifosem);

	if (0 && DBG_HDLC && DBG_SPANFILTER) {
		b4_info(b4, "%s", debugbuf);

		b4_info(b4, "hdlc_tx_frame(span %d): DAHDI gave %d " \
			 "bytes for FIFO %d (res = %d)\n",
			 bspan->port + 1, size, fifo, res);

		for (i = 0; i < size; i++)
			b4_info(b4,
				 "%02x%c\n", buf[i],
				 (i < (size - 1)) ? ' ' : '\n');

		if (size && res != 0) {
			pr_info("Transmitted frame %d on span %d\n",
				bspan->frames_out - 1, bspan->port);
		}
	}

	return (res == 0);
}

/*
 * b400m lowlevel functions These are functions which impact more than just
 * the HFC controller.  (those are named hfc_xxx())
 */

/*
 * Performs a total reset of the card, reinitializes GPIO.  The card is
 * initialized enough to have LEDs running, and that's about it.  Anything to
 * do with audio and enabling any kind of processing is done in stage2.
 */
static void xhfc_init_stage1(struct b400m *b4)
{
	int i;

	hfc_reset(b4);
	hfc_gpio_init(b4);

	/* make sure interrupts are disabled */
	b400m_setreg(b4, R_IRQ_CTRL, 0x00);

	/* make sure write hits hardware */
	flush_hw();

	/* disable all FIFO interrupts */
	for (i = 0; i < HFC_NR_FIFOS; i++) {
		hfc_setreg_waitbusy(b4, R_FIFO, (i << V_FIFO_NUM_SHIFT));
		/* disable the interrupt */
		b400m_setreg(b4, A_FIFO_CTRL, 0x00);
		hfc_setreg_waitbusy(b4, R_FIFO,
				    (i << V_FIFO_NUM_SHIFT) | V_FIFO_DIR);
		/* disable the interrupt */
		b400m_setreg(b4, A_FIFO_CTRL, 0x00);
		flush_hw();
	}

	/* set fill threshhold to 16 bytes */
	b400m_setreg(b4, R_FIFO_THRES, 0x11);

	/* clear any pending FIFO interrupts */
	b400m_getreg(b4, R_FIFO_BL2_IRQ);
	b400m_getreg(b4, R_FIFO_BL3_IRQ);

	b4->misc_irq_mask = 0x00;
	b400m_setreg(b4, R_MISC_IRQMSK, b4->misc_irq_mask);
	b400m_setreg(b4, R_IRQ_CTRL, 0);
}

/*
 * Stage 2 hardware init.  Sets up the flow controller, PCM and FIFOs.
 * Initializes the echo cancellers.  S/T interfaces are not initialized here,
 * that is done later, in hfc_init_all_st().  Interrupts are enabled and once
 * the s/t interfaces are configured, chip should be pretty much operational.
 */
static void xhfc_init_stage2(struct b400m *b4)
{
	/*
	 * set up PCM bus. XHFC is PCM slave C2IO is the clock, auto sync,
	 * SYNC_O follows SYNC_I.  128 timeslots, long frame sync positive
	 * polarity, sample on falling clock edge. STIO2 is transmit-only,
	 * STIO1 is receive-only.
	 */
	b400m_setreg(b4, R_PCM_MD0, V_PCM_IDX_MD1);
	b400m_setreg(b4, R_PCM_MD1, V_PCM_DR_8192 | (0x3 << 2));
	b400m_setreg(b4, R_PCM_MD0, V_PCM_IDX_MD2);
	b400m_setreg(b4, R_PCM_MD2, V_C2I_EN | V_SYNC_OUT1);
	b400m_setreg(b4, R_SU_SYNC, V_SYNC_SEL_PORT0);

	/* Now set up the flow controller. */
	hfc_setup_fsm(b4);

	/*
	 * At this point, everything's set up and ready to go.  Don't actually
	 * enable the global interrupt pin.  DAHDI still needs to start up the
	 * spans, and we don't know exactly when.
	 */
}

static inline struct b400m_span *bspan_from_dspan(struct dahdi_span *span)
{
	return container_of(span, struct wctdm_span, span)->bspan;
}

static int xhfc_startup(struct dahdi_span *span)
{
	struct b400m_span *bspan = bspan_from_dspan(span);
	struct b400m *b4 = bspan->parent;
	if (!b4->running)
		hfc_enable_interrupts(bspan->parent);

	return 0;
}

/* resets all the FIFOs for a given span. Disables IRQs for the span FIFOs */
static void xhfc_reset_span(struct b400m_span *bspan)
{
	int i;
	struct b400m *b4 = bspan->parent;

	/* b4_info(b4, "xhfc_reset_span()\n"); */
	for (i = 0; i < 3; i++)
		hfc_reset_fifo_pair(b4, bspan->fifos[i], (i == 2) ? 1 : 0, 1);
}

static void b400m_enable_workqueues(struct wctdm *wc)
{
	struct b400m *b4s[2];
	int i, numb4s = 0;
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	for (i = 0; i < wc->mods_per_board; i += 4) {
		if (wc->mods[i].type == BRI)
			b4s[numb4s++] = wc->mods[i].mod.bri;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	for (i = 0; i < numb4s; i++) {
		if (b4s[i])
			b4s[i]->shutdown = 0;
	}

}

static void b400m_disable_workqueues(struct wctdm *wc)
{
	struct b400m *b4s[2];
	int i, numb4s = 0;
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	for (i = 0; i < wc->mods_per_board; i += 4) {
		if (wc->mods[i].type == BRI)
			b4s[numb4s++] = wc->mods[i].mod.bri;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	for (i = 0; i < numb4s; i++) {
		if (b4s[i]) {
			down(&wc->syncsem);
			b4s[i]->shutdown = 1;
			up(&wc->syncsem);
			flush_workqueue(b4s[i]->xhfc_ws);
		}
	}
}
/*
 * Software selectable NT and TE mode settings on the B400M.
 *
 * mode - bitwise selection of NT vs TE mode
 *        1 = NT; 0 = TE;
 *        bit 0 is port 0
 *        bit 1 is port 1
 *        ...
 * term - termination resistance
 *        0 = no termination resistance
 *        1 = 390 ohm termination resistance switched on
 */
static int b400m_set_ntte(struct b400m_span *bspan, int te_mode, int term_on)
{
	struct b400m *b4 = bspan->parent;
	unsigned char data;
	unsigned char addr;
	int all_modes = 0, all_terms = 0;
	int i;

	bspan->wspan->span.spantype = (te_mode > 0) ? "TE" : "NT";

	bspan->te_mode = te_mode;
	bspan->term_on = term_on;

	for (i = 0; i < 4; i++) {
		if (!b4->spans[i].te_mode)
			all_modes |= (1 << i);
		if (b4->spans[i].term_on)
			all_terms |= (1 << i);
	}

	data = 0x10 | ((all_terms << 4) & 0xc0) | ((all_terms << 2) & 0x0c);
	addr = 0x10 | all_modes;

	msleep(voicebus_current_latency(&b4->wc->vb) + 2);
	wctdm_setreg(b4->wc, get_mod(b4), addr, data);

	b4->lastreg = 0xff;
	msleep(voicebus_current_latency(&b4->wc->vb) + 2);

	hfc_reset_st(bspan);

	if (bri_persistentlayer1)
		hfc_start_st(bspan);

	return 0;
}

/* spanconfig for us means ...? */
int b400m_spanconfig(struct file *file, struct dahdi_span *span,
		     struct dahdi_lineconfig *lc)
{
	struct b400m_span *bspan;
	struct b400m *b4;
	struct wctdm *wc;
	int te_mode, term;
	int pos;
	int res;

	bspan = bspan_from_dspan(span);
	b4 = bspan->parent;
	wc = b4->wc;

	if ((file->f_flags & O_NONBLOCK) && !is_initialized(wc))
		return -EAGAIN;

	res = wctdm_wait_for_ready(wc);
	if (res)
		return res;

	b400m_disable_workqueues(b4->wc);

	te_mode = (lc->lineconfig & DAHDI_CONFIG_NTTE) ? 0 : 1;

	term = (lc->lineconfig & DAHDI_CONFIG_TERM) ? 1 : 0;

	b4_info(b4, "xhfc: Configuring port %d span %d in %s " \
		 "mode with termination resistance %s\n", bspan->port,
		 span->spanno, (te_mode) ? "TE" : "NT",
		 (term) ? "ENABLED" : "DISABLED");

	b400m_set_ntte(bspan, te_mode, term);
	if (lc->sync < 0) {
		b4_info(b4, "Span %d has invalid sync priority (%d), " \
			 "removing from sync source list\n", span->spanno,
			 lc->sync);
		lc->sync = 0;
	}

	if (span->offset >= 4) {
		pos = span->offset;
	} else {
		/* This is tricky.  Have to figure out if we're slot 1 or slot
		 * 2 */
		pos = span->offset + b4->position;
	}

	if (!te_mode && lc->sync) {
		b4_info(b4, "NT Spans cannot be timing sources.  " \
			 "Span %d requested to be timing source of " \
			 "priority %d.  Changing priority to 0\n", pos,
			 lc->sync);
		lc->sync = 0;
	}

	wc->spans[pos]->timing_priority = lc->sync;

	bspan->wspan = container_of(span, struct wctdm_span, span);
	xhfc_reset_span(bspan);

	/* call startup() manually here, because DAHDI won't call the startup
	 * function unless it receives an IOCTL to do so, and dahdi_cfg
	 * doesn't. */
	xhfc_startup(span);

	span->flags |= DAHDI_FLAG_RUNNING;

	set_bit(WCTDM_CHECK_TIMING, &wc->checkflag);

	b400m_enable_workqueues(b4->wc);

	return 0;
}

/* chanconfig for us means to configure the HDLC controller, if appropriate
 *
 * NOTE: apparently the DAHDI ioctl function calls us with a interrupts
 * disabled.  This means we cannot actually touch the hardware, because all
 * register accesses are wrapped up in a mutex that can sleep.
 *
 * The solution to that is to simply increment the span's "restart" flag, and
 * the driver's workqueue will do the dirty work on our behalf.
 */
int b400m_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	int alreadyrunning;
	struct b400m_span *bspan = bspan_from_dspan(chan->span);
	struct b400m *b4 = bspan->parent;
	int res;

	if ((file->f_flags & O_NONBLOCK) && !is_initialized(b4->wc))
		return -EAGAIN;

	res = wctdm_wait_for_ready(b4->wc);
	if (res)
		return res;

	alreadyrunning = bspan->wspan->span.flags & DAHDI_FLAG_RUNNING;

	if (DBG_FOPS) {
		b4_info(b4, "%s channel %d (%s) sigtype %08x\n",
			 alreadyrunning ? "Reconfigured" : "Configured",
			 chan->channo, chan->name, sigtype);
	}

	switch (sigtype) {
	case DAHDI_SIG_HARDHDLC:
		if (DBG_FOPS) {
			b4_info(b4, "%sonfiguring hardware HDLC on %s\n",
				 ((sigtype == DAHDI_SIG_HARDHDLC) ? "C" :
				 "Unc"), chan->name);
		}
		bspan->sigchan = chan;
		bspan->sigactive = 0;
		atomic_set(&bspan->hdlc_pending, 0);
		res = 0;
		break;
	case DAHDI_SIG_HDLCFCS:
	case DAHDI_SIG_HDLCNET:
	case DAHDI_SIG_HDLCRAW:
		/* Only HARDHDLC is supported for the signalling channel on BRI
		 * spans. */
		res = -EINVAL;
		break;
	default:
		res = 0;
		break;
	};

	return res;
}

int b400m_dchan(struct dahdi_span *span)
{
	struct b400m_span *bspan;
	struct b400m *b4;
	unsigned char *rxb;
	int res;
	int i;

	bspan = bspan_from_dspan(span);
	b4 = bspan->parent;
#ifdef HARDHDLC_RX
	return 0;
#else
#endif

	if (!bspan->sigchan)
		return 0;

	rxb = bspan->sigchan->readchunk;

	if (!rxb) {
		b4_info(b4, "No RXB!\n");
		return 0;
	}

	for (i = 0; i < DAHDI_CHUNKSIZE; i++) {
		fasthdlc_rx_load_nocheck(&bspan->rxhdlc, *(rxb++));
		res = fasthdlc_rx_run(&bspan->rxhdlc);
		/* If there is nothing there, continue */
		if (res & RETURN_EMPTY_FLAG)
			continue;
		else if (res & RETURN_COMPLETE_FLAG) {

			if (!bspan->f_sz)
				continue;

			/* Only count this if it's a non-empty frame */
			if (bspan->infcs != PPP_GOODFCS) {
				dahdi_hdlc_abort(bspan->sigchan,
						 DAHDI_EVENT_BADFCS);
			} else {
				dahdi_hdlc_finish(bspan->sigchan);
			}
			bspan->infcs = PPP_INITFCS;
			bspan->f_sz = 0;
			continue;
		} else if (res & RETURN_DISCARD_FLAG) {

			if (!bspan->f_sz)
				continue;

			dahdi_hdlc_abort(bspan->sigchan, DAHDI_EVENT_ABORT);
			bspan->infcs = PPP_INITFCS;
			bspan->f_sz = 0;
			break;
		} else {
			unsigned char rxc = res;
			bspan->infcs = PPP_FCS(bspan->infcs, rxc);
			bspan->f_sz++;
			dahdi_hdlc_putbuf(bspan->sigchan, &rxc, 1);
		}
	}

	return 0;
}

/* internal functions, not specific to the hardware or DAHDI */

/*
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void xhfc_work(void *data)
{
	struct b400m *b4 = data;
#else
static void xhfc_work(struct work_struct *work)
{
	struct b400m *b4 = container_of(work, struct b400m, xhfc_wq);
#endif
	int i, j, k, fifo;
	unsigned char b, b2;

	if (b4->shutdown || !is_initialized(b4->wc))
		return;

	b4->irq_oview = b400m_getreg(b4, R_IRQ_OVIEW);
	b4->fifo_fill = b400m_getreg(b4, R_FILL_BL0);

	if (b4->irq_oview & V_FIFO_BL0_IRQ) {
		b4->fifo_irqstatus |= b400m_getreg(b4, R_FIFO_BL0_IRQ);
		b4->irq_oview &= ~V_FIFO_BL0_IRQ;
	}

	/* only look at BL0, we put all D channel FIFOs in the first block. */
	b = b2 = b4->fifo_irqstatus;

	for (j = 0; j < 4; j++) {
#ifdef SWAP_PORTS
		fifo = (1 == j) ? 2 : (2 == j) ? 1 : j;
#else
		fifo = j;
#endif

#ifdef HARDHDLC_RX
		if (b & V_FIFOx_RX_IRQ) {
			if (fifo < 4) {		/* d-channel FIFO */

				/*
				 * I have to loop here until hdlc_rx_frame
				 * says there are no more frames waiting.  for
				 * whatever reason, the HFC will not generate
				 * another interrupt if there are still HDLC
				 * frames waiting to be received.  i.e. I get
				 * an int when F1 changes, not when F1 != F2.
				 *
				 */
				do {
					k = hdlc_rx_frame(&b4->spans[fifo]);
				} while (k);
			}
		}

#endif
		b >>= 2;
	}

	/* zero the bits we just processed */
	b4->fifo_irqstatus &= ~b2;
	b4->fifo_fill &= ~b2;


#if 1
	/* All four D channel FIFOs are in BL0. */
	b = b2 = b4->fifo_fill;

	for (j = 0; j < 4; j++) {
#ifdef SWAP_PORTS
		fifo = (1 == j) ? 2 : (2 == j) ? 1 : j;
#else
		fifo = j;
#endif
		if (b4->spans[fifo].sigactive && (b & V_FIFOx_TX_IRQ))
			hdlc_tx_frame(&b4->spans[fifo]);

#ifdef HARDHDLC_RX
		if (b & V_FIFOx_RX_IRQ)
			hdlc_rx_frame(&b4->spans[fifo]);
#endif

		b >>= 2;
	}
#endif

	/* Check for outgoing HDLC frame requests The HFC does not generate TX
	 * interrupts when there is room to send, so I use an atomic counter
	 * that is incremented every time DAHDI wants to send a frame, and
	 * decremented every time I send a frame.  It'd be better if I could
	 * just use the interrupt handler, but the HFC seems to trigger a FIFO
	 * TX IRQ only when it has finished sending a frame, not when one can
	 * be sent.
	 */
	for (i = 0; i < ARRAY_SIZE(b4->spans); i++) {
		struct b400m_span *bspan = &b4->spans[i];

		if (atomic_read(&bspan->hdlc_pending)) {
			do {
				k = hdlc_tx_frame(bspan);
			}  while (k);
		}
	}

	b = b400m_getreg(b4, R_SU_IRQ);

	if (b) {
		for (i = 0; i < ARRAY_SIZE(b4->spans); i++) {
			int physport;

#ifdef SWAP_PORTS
			if (i == 1)
				physport = 2;
			else if (i == 2)
				physport = 1;
			else
				physport = i;
#else
			physport = i;
#endif
			if (b & (1 << i))
				hfc_handle_state(&b4->spans[physport]);
		}
	}

	hfc_update_st_timers(b4);
}

void wctdm_bri_checkisr(struct wctdm *wc, struct wctdm_module *const mod,
			int offset)
{
	struct b400m *b4 = mod->mod.bri;

	/* don't do anything for non-base card slots */
	if (mod->card & 0x03)
		return;

	/* DEFINITELY don't do anything if our structures aren't ready! */
	if (!is_initialized(wc) || !b4 || !b4->inited)
		return;

	if (offset == 0) {
		if (!b4->shutdown)
			queue_work(b4->xhfc_ws, &b4->xhfc_wq);
		b4->ticks++;
	}
	return;
}

/* DAHDI calls this when it has data it wants to send to the HDLC controller */
void wctdm_hdlc_hard_xmit(struct dahdi_chan *chan)
{
	struct b400m *b4;
	struct b400m_span *bspan;
	struct dahdi_span *dspan;
	int span;

	dspan = chan->span;
	bspan = bspan_from_dspan(dspan);
	b4 = bspan->parent;
	span = bspan->port;

	if ((DBG_FOPS || DBG_HDLC) && DBG_SPANFILTER) {
		b4_info(b4, "hdlc_hard_xmit on chan %s (%i/%i), " \
			 "span=%i (sigchan=%p, chan=%p)\n", chan->name,
			 chan->channo, chan->chanpos, span + 1,
			 bspan->sigchan, chan);
	}

	/* Increment the hdlc_pending counter and trigger the bottom-half so
	 * it will be picked up and sent.  */
	if (bspan->sigchan == chan)
		atomic_inc(&bspan->hdlc_pending);
}

static int b400m_probe(struct wctdm *wc, int modpos)
{
	unsigned char id, x;
	struct b400m *b4;
	unsigned long flags;
	int chiprev;

	wctdm_setreg(wc, &wc->mods[modpos], 0x10, 0x10);
	id = xhfc_getreg(wc, &wc->mods[modpos], R_CHIP_ID, &x);

	/* chip ID high 7 bits must be 0x62, see datasheet */
	if ((id & 0xfe) != 0x62)
		return -2;

	b4 = kzalloc(sizeof(struct b400m), GFP_KERNEL);
	if (!b4) {
		dev_err(&wc->vb.pdev->dev,
			"Couldn't allocate memory for b400m structure!\n");
		return -ENOMEM;
	}

	/* card found, enabled and main struct allocated.  Fill it out. */
	b4->wc = wc;
	b4->position = modpos;

	/* which B400M in the system is this one? count all of them found so
	 * far */
	for (x = 0; x < modpos; x += 4) {
		if (wc->mods[x].type == BRI)
			++b4->b400m_no;
	}

	spin_lock_init(&b4->reglock);
	sema_init(&b4->regsem, 1);
	sema_init(&b4->fifosem, 1);

	for (x = 0; x < 4; x++) {
		fasthdlc_init(&b4->spans[x].rxhdlc, FASTHDLC_MODE_16);
		b4->spans[x].infcs = PPP_INITFCS;
	}

	b4->lastreg = 0xff; /* a register we won't hit right off the bat */

	chiprev = b400m_getreg(b4, R_CHIP_RV);

	b4->setsyncspan = -1;		/* sync span is unknown */
	b4->reportedsyncspan = -1;	/* sync span is unknown */

	if (DBG) {
		b4_info(b4, "Identified controller rev %d in module %d.\n",
			chiprev, b4->position);
	}

	xhfc_init_stage1(b4);

	xhfc_init_stage2(b4);
	hfc_init_all_st(b4);

	hfc_enable_interrupts(b4);

	spin_lock_irqsave(&wc->reglock, flags);
	wc->mods[modpos].mod.bri = (void *)b4;
	spin_unlock_irqrestore(&wc->reglock, flags);

	return 0;
}

void b400m_post_init(struct b400m *b4)
{
	snprintf(b4->name, sizeof(b4->name) - 1, "b400m-%d",
		 b4->b400m_no);
	b4->xhfc_ws = create_singlethread_workqueue(b4->name);
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&b4->xhfc_wq, xhfc_work, b4);
#	else
	INIT_WORK(&b4->xhfc_wq, xhfc_work);
#	endif
	b4->inited = 1;
}

/* functions called from the wctdm code */
int wctdm_init_b400m(struct wctdm *wc, int card)
{
	int ret = 0;
	unsigned long flags;

	if (wc->mods[card & 0xfc].type == QRV)
		return -2;

	if (!(card & 0x03)) { /* only init if at lowest port in module */
		spin_lock_irqsave(&wc->reglock, flags);
		wc->mods[card + 0].type = BRI;
		wc->mods[card + 0].mod.bri = NULL;
		wc->mods[card + 1].type = BRI;
		wc->mods[card + 1].mod.bri = NULL;
		wc->mods[card + 2].type = BRI;
		wc->mods[card + 2].mod.bri = NULL;
		wc->mods[card + 3].type = BRI;
		wc->mods[card + 3].mod.bri = NULL;
		spin_unlock_irqrestore(&wc->reglock, flags);

		msleep(20);

		if (b400m_probe(wc, card) != 0) {
			spin_lock_irqsave(&wc->reglock, flags);
			wc->mods[card + 0].type = NONE;
			wc->mods[card + 1].type = NONE;
			wc->mods[card + 2].type = NONE;
			wc->mods[card + 3].type = NONE;
			spin_unlock_irqrestore(&wc->reglock, flags);
			ret = -2;
		}
	} else {	/* for the "sub-cards" */
		if (wc->mods[card & 0xfc].type == BRI) {
			spin_lock_irqsave(&wc->reglock, flags);
			wc->mods[card].type = BRI;
			wc->mods[card].mod.bri = wc->mods[card & 0xfc].mod.bri;
			spin_unlock_irqrestore(&wc->reglock, flags);
		} else {
			ret = -2;
		}
	}

	return ret;
}

void wctdm_unload_b400m(struct wctdm *wc, int card)
{
	struct b400m *b4 = wc->mods[card].mod.bri;
	int i;

	/* TODO: shutdown once won't work if just a single card is hotswapped
	 * out.  But since most of the time this is called because the entire
	 * driver is in the process of unloading, I'll leave it here. */
	static int shutdown_once;


	/* only really unload with the 'base' card number.  base+1/2/3 aren't
	 * real. */
	if (card & 0x03)
		return;

	if (timingcable && !shutdown_once) {
		b4_info(b4, "Disabling all workqueues for B400Ms\n");
		/* Gotta shut down timing change potential during this */
		for (i = 0; i < WC_MAX_IFACES; i++) {
			if (ifaces[i])
				b400m_disable_workqueues(ifaces[i]);
		}
		b4_info(b4, "Forcing sync to card 0\n");
		/* Put the timing configuration in a known state: card 0 is
		 * master */
		wctdm_change_system_sync_src(synccard, syncspan, -1, -1);
		/* Change all other cards in the system to self time before
		 * card 0 is removed */
		b4_info(b4, "Setting all cards to return to self sync\n");
		for (i = 1; i < WC_MAX_IFACES; i++) {
			if (ifaces[i])
				wctdm_change_card_sync_src(ifaces[i], 0, 0);
		}

		b4_info(b4,
			 "Finished preparing timing linked cards for "
			 "shutdown\n");

		shutdown_once = 1;
	}

	if (b4) {
		b4->inited = 0;

		msleep(100);

		/* TODO: wait for tdm24xx driver to unregister the spans */
		/* 	do { ... } while(not_unregistered); */

		/* Change sync source back to base board so we don't freeze up
		 * when we reset the XHFC */
		b400m_disable_workqueues(wc);

		for (i = 0; i < (MAX_SPANS - 1); i++) {
			if (wc->spans[i])
				wc->spans[i]->timing_priority = 0;
		}

		for (i = 0; i < ARRAY_SIZE(b4->spans); i++)
			b4->spans[i].wspan->span.flags &= ~DAHDI_FLAG_RUNNING;

		wctdm_change_card_sync_src(b4->wc, 0, 0);

		xhfc_init_stage1(b4);

		destroy_workqueue(b4->xhfc_ws);

		/* Set these to NONE to ensure that our checkisr
		 * routines are not entered */
		wc->mods[card].type = NONE;
		wc->mods[card + 1].type = NONE;
		wc->mods[card + 2].type = NONE;
		wc->mods[card + 3].type = NONE;

		wc->mods[card].mod.bri = NULL;
		wc->mods[card + 1].mod.bri = NULL;
		wc->mods[card + 2].mod.bri = NULL;
		wc->mods[card + 3].mod.bri = NULL;

		msleep(voicebus_current_latency(&wc->vb) << 1);
		b4_info(b4, "Driver unloaded.\n");
		kfree(b4);
	}

}

void b400m_module_init(void)
{
	fasthdlc_precalc();
}

void b400m_module_cleanup(void)
{
}
