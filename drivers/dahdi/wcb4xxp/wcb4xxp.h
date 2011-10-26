/*
 * Wilcard B410P Quad-BRI Interface Driver for Zapata Telephony interface
 * Written by Andrew Kohlsmith <akohlsmith@mixdown.ca>
 */

#ifndef _B4XX_H_
#define _B4XX_H_

#include <linux/ioctl.h>

#define HFC_NR_FIFOS	32
#define HFC_ZMIN	0x80			/* from datasheet */
#define HFC_ZMAX	0x1ff
#define HFC_FMIN	0x00
#define HFC_FMAX	0x0f

/*
 * yuck. Any reg which is not mandated read/write or read-only is write-only.
 * Also, there are dozens of registers with the same address.
 * Additionally, there are array registers (A_) which have an index register
 * These A_ registers require an index register to be written to indicate WHICH in the array you want.
 */

#define R_CIRM			0x00	/* WO       */
#define R_CTRL			0x01	/* WO       */
#define R_BRG_PCM_CFG		0x02	/* WO       */
#define A_Z12			0x04	/*    RO    */
#define A_Z1L			0x04	/*    RO    */
#define A_Z1			0x04	/*    RO    */
#define A_Z1H			0x05	/*    RO    */
#define A_Z2L			0x06	/*    RO    */
#define A_Z2			0x06	/*    RO    */
#define A_Z2H			0x07	/*    RO    */
#define R_RAM_ADDR0		0x08	/* WO       */
#define R_RAM_ADDR1		0x09	/* WO       */
#define R_RAM_ADDR2		0x0a	/* WO       */
#define R_FIRST_FIFO		0x0b	/* WO       */
#define R_RAM_MISC		0x0c	/* WO       */
#define A_F1			0x0c	/*    RO    */
#define A_F12			0x0c	/*    RO    */
#define R_FIFO_MD		0x0d	/* WO       */
#define A_F2			0x0d	/*    RO    */
#define A_INC_RES_FIFO		0x0e	/* WO       */
#define R_FSM_IDX		0x0f	/* WO       */
#define R_FIFO			0x0f	/* WO       */

#define R_SLOT			0x10	/* WO       */
#define R_IRQ_OVIEW		0x10	/*    RO    */
#define R_IRQMSK_MISC		0x11	/* WO       */
#define R_IRQ_MISC		0x11	/*    RO    */
#define R_SCI_MSK		0x12	/* WO       */
#define R_SCI			0x12	/*    RO    */
#define R_IRQ_CTRL		0x13	/* WO       */
#define R_PCM_MD0		0x14	/* WO       */
#define R_CONF_OFLOW		0x14	/*    RO    */
#define R_PCM_MD1		0x15	/* WO       */
#define R_PCM_MD2		0x15	/* WO       */
#define R_SH0H			0x15	/* WO       */
#define R_SH1H			0x15	/* WO       */
#define R_SH0L			0x15	/* WO       */
#define R_SH1L			0x15	/* WO       */
#define R_SL_SEL0		0x15	/* WO       */
#define R_SL_SEL1		0x15	/* WO       */
#define R_SL_SEL2		0x15	/* WO       */
#define R_SL_SEL3		0x15	/* WO       */
#define R_SL_SEL4		0x15	/* WO       */
#define R_SL_SEL5		0x15	/* WO       */
#define R_SL_SEL6		0x15	/* WO       */
#define R_SL_SEL7		0x15	/* WO       */
#define R_RAM_USE		0x15	/*    RO    */
#define R_ST_SEL		0x16	/* WO       */
#define R_CHIP_ID		0x16	/*    RO    */
#define R_ST_SYNC		0x17	/* WO       */
#define R_BERT_STA		0x17	/*    RO    */
#define R_CONF_EN		0x18	/* WO       */
#define R_F0_CNTL		0x18	/*    RO    */
#define R_F0_CNTH		0x19	/*    RO    */
#define R_TI_WD			0x1a	/* WO       */
#define R_BERT_ECL		0x1a	/*    RO    */
#define R_BERT_WD_MD		0x1b	/* WO       */
#define R_BERT_ECH		0x1b	/*    RO    */
#define R_DTMF			0x1c	/* WO       */
#define R_STATUS		0x1c	/*    RO    */
#define R_DTMF_N		0x1d	/* WO       */
#define R_CHIP_RV		0x1f	/*    RO    */

#define A_ST_WR_STA		0x30	/* WO       */
#define A_ST_RD_STA		0x30	/*    RO    */
#define A_ST_CTRL0		0x31	/* WO       */
#define A_ST_CTRL1		0x32	/* WO       */
#define A_ST_CTRL2		0x33	/* WO       */
#define A_ST_SQ_WR		0x34	/* WO       */
#define A_ST_SQ_RD		0x34	/*    RO    */
#define A_ST_CLK_DLY		0x37	/* WO       */

#define R_PWM0			0x38	/* WO       */
#define R_PWM1			0x39	/* WO       */

#define A_ST_B1_TX		0x3c	/* WO       */
#define A_ST_B1_RX		0x3c	/*    RO    */
#define A_ST_B2_TX		0x3d	/* WO       */
#define A_ST_B2_RX		0x3d	/*    RO    */
#define A_ST_D_TX		0x3e	/* WO       */
#define A_ST_D_RX		0x3e	/*    RO    */
#define A_ST_E_RX		0x3f	/*    RO    */

#define R_GPIO_OUT0		0x40	/* WO       */
#define R_GPIO_IN0		0x40	/*    RO    */
#define R_GPIO_OUT1		0x41	/* WO       */
#define R_GPIO_IN1		0x41	/*    RO    */
#define R_GPIO_EN0		0x42	/* WO       */
#define R_GPIO_EN1		0x43	/* WO       */
#define R_GPIO_SEL		0x44	/* WO       */
#define R_GPI_IN0		0x44	/*    RO    */
#define R_GPI_IN1		0x45	/*    RO    */
#define R_PWM_MD		0x46	/* WO       */
#define R_GPI_IN2		0x46	/*    RO    */
#define R_GPI_IN3		0x47	/*    RO    */

#define A_FIFO_DATA2		0x80	/*       RW */
#define A_FIFO_DATA0		0x80	/*       RW */
#define A_FIFO_DATA1		0x80	/*       RW */

#define A_FIFO_DATA2_NOINC	0x84	/* WO       */
#define A_FIFO_DATA0_NOINC	0x84	/* WO       */
#define A_FIFO_DATA1_NOINC	0x84	/* WO       */

#define R_INT_DATA		0x88	/*    RO    */

#define R_RAM_DATA		0xc0	/*       RW */

#define R_IRQ_FIFO_BL0		0xc8	/*    RO    */
#define R_IRQ_FIFO_BL1		0xc9	/*    RO    */
#define R_IRQ_FIFO_BL2		0xca	/*    RO    */
#define R_IRQ_FIFO_BL3		0xcb	/*    RO    */
#define R_IRQ_FIFO_BL4		0xcc	/*    RO    */
#define R_IRQ_FIFO_BL5		0xcd	/*    RO    */
#define R_IRQ_FIFO_BL6		0xce	/*    RO    */
#define R_IRQ_FIFO_BL7		0xcf	/*    RO    */

#define A_SL_CFG		0xd0	/* WO       */
#define A_CONF			0xd1	/* WO       */
#define A_CH_MSK		0xf4	/* WO       */
#define A_CON_HDLC		0xfa	/* WO       */
#define A_SUBCH_CFG		0xfb	/* WO       */
#define A_CHANNEL		0xfc	/* WO       */
#define A_FIFO_SEQ		0xfd	/* WO       */
#define A_IRQ_MSK		0xff	/* WO       */

/* R_CIRM bits */
#define V_SRES			(1 << 3)	/* soft reset (group 0) */
#define V_HFC_RES		(1 << 4)	/* HFC reset (group 1) */
#define V_PCM_RES		(1 << 5)	/* PCM reset (group 2) */
#define V_ST_RES		(1 << 6)	/* S/T reset (group 3) */
#define V_RLD_EPR		(1 << 7)	/* EEPROM reload */
#define HFC_FULL_RESET	(V_SRES | V_HFC_RES | V_PCM_RES | V_ST_RES | V_RLD_EPR)

/* A_IRQ_MSK bits */
#define V_IRQ			(1 << 0)	/* FIFO interrupt enable */
#define V_BERT_EN		(1 << 1)	/* enable BERT */
#define V_MIX_IRQ		(1 << 2)	/* mixed interrupt enable (frame + transparent mode) */

/* R_STATUS bits */
#define V_BUSY			(1 << 0)	/* 1=HFC busy, limited register access */
#define V_PROC			(1 << 1)	/* 1=HFC in processing phase */
#define V_LOST_STA		(1 << 3)	/* 1=frames have been lost */
#define V_SYNC_IN		(1 << 4)	/* level on SYNC_I pin */
#define V_EXT_IRQSTA		(1 << 5)	/* 1=external interrupt */
#define V_MISC_IRQSTA		(1 << 6)	/* 1=misc interrupt has occurred */
#define V_FR_IRQSTA		(1 << 7)	/* 1=fifo interrupt has occured */
#define HFC_INTS	(V_EXT_IRQSTA | V_MISC_IRQSTA | V_FR_IRQSTA)

/* R_SCI/R_SCI_MSK bits */
#define V_SCI_ST0		(1 << 0)	/* state change for port 1 */
#define V_SCI_ST1		(1 << 1)	/* state change for port 2 */
#define V_SCI_ST2		(1 << 2)	/* state change for port 3 */
#define V_SCI_ST3		(1 << 3)	/* state change for port 4 */

/* R_IRQ_FIFO_BLx bits */
#define V_IRQ_FIFOx_TX		(1 << 0)	/* FIFO TX interrupt occurred */
#define V_IRQ_FIFOx_RX		(1 << 1)	/* FIFO RX interrupt occurred */
#define IRQ_FIFOx_TXRX		(V_IRQ_FIFOx_TX | V_IRQ_FIFOx_RX)

/* R_IRQ_MISC / R_IRQMSK_MISC bits */
#define V_TI_IRQ		(1 << 1)	/* timer elapsed */
#define V_IRQ_PROC		(1 << 2)	/* processing/non-processing transition */
#define V_DTMF_IRQ		(1 << 3)	/* DTMF detection completed */
#define V_EXT_IRQ		(1 << 5)	/* external interrupt occured */

/* R_IRQ_CTRL bits */
#define V_FIFO_IRQ		(1 << 0)	/* enable any unmasked FIFO IRQs */
#define V_GLOB_IRQ_EN		(1 << 3)	/* enable any unmasked IRQs */
#define V_IRQ_POL		(1 << 4)	/* 1=IRQ active high */

/* R_BERT_WD_MD bits */
#define V_BERT_ERR		(1 << 3)	/* 1=generate an error bit in BERT stream */
#define V_AUTO_WD_RES		(1 << 5)	/* 1=automatically kick the watchdog */
#define V_WD_RES		(1 << 7)	/* 1=kick the watchdog (bit auto clears) */

/* R_TI_WS bits */
#define V_EV_TS_SHIFT		(0)
#define V_EV_TS_MASK		(0x0f)
#define V_WD_TS_SHIFT		(4)
#define V_WD_TS_MASK		(0xf0)

/* R_BRG_PCM_CFG bits */
#define V_PCM_CLK		(1 << 5)	/* 1=PCM clk = OSC, 0 = PCM clk is 2x OSC */

/* R_PCM_MD0 bits */
#define V_PCM_MD		(1 << 0)	/* 1=PCM master */
#define V_C4_POL		(1 << 1)	/* 1=F0IO sampled on rising edge of C4IO */
#define V_F0_NEG		(1 << 2)	/* 1=negative polarity of F0IO */
#define V_F0_LEN		(1 << 3)	/* 1=F0IO active for 2 C4IO clocks */
#define V_PCM_IDX_SEL0		(0x0 << 4)	/* reg15 = R_SL_SEL0 */
#define V_PCM_IDX_SEL1		(0x1 << 4)	/* reg15 = R_SL_SEL1 */
#define V_PCM_IDX_SEL2		(0x2 << 4)	/* reg15 = R_SL_SEL2 */
#define V_PCM_IDX_SEL3		(0x3 << 4)	/* reg15 = R_SL_SEL3 */
#define V_PCM_IDX_SEL4		(0x4 << 4)	/* reg15 = R_SL_SEL4 */
#define V_PCM_IDX_SEL5		(0x5 << 4)	/* reg15 = R_SL_SEL5 */
#define V_PCM_IDX_SEL6		(0x6 << 4)	/* reg15 = R_SL_SEL6 */
#define V_PCM_IDX_SEL7		(0x7 << 4)	/* reg15 = R_SL_SEL7 */
#define V_PCM_IDX_MD1		(0x9 << 4)	/* reg15 = R_PCM_MD1 */
#define V_PCM_IDX_MD2		(0xa << 4)	/* reg15 = R_PCM_MD2 */
#define V_PCM_IDX_SH0L		(0xc << 4)	/* reg15 = R_SH0L */
#define V_PCM_IDX_SH0H		(0xd << 4)	/* reg15 = R_SH0H */
#define V_PCM_IDX_SH1L		(0xe << 4)	/* reg15 = R_SH1L */
#define V_PCM_IDX_SH1H		(0xf << 4)	/* reg15 = R_SH1H */
#define V_PCM_IDX_MASK		(0xf0)

/* R_PCM_MD1 bits */
#define V_CODEC_MD		(1 << 0)	/* no damn idea */
#define V_PLL_ADJ_00		(0x0 << 2)	/* adj 4 times by 0.5 system clk cycles */
#define V_PLL_ADJ_01		(0x1 << 2)	/* adj 3 times by 0.5 system clk cycles */
#define V_PLL_ADJ_10		(0x2 << 2)	/* adj 2 times by 0.5 system clk cycles */
#define V_PLL_ADJ_11		(0x3 << 2)	/* adj 1 time by 0.5 system clk cycles */
#define V_PCM_DR_2048		(0x0 << 4)	/* 2.048Mbps, 32 timeslots */
#define V_PCM_DR_4096		(0x1 << 4)	/* 4.096Mbps, 64 timeslots */
#define V_PCM_DR_8192		(0x2 << 4)	/* 8.192Mbps, 128 timeslots */
#define V_PCM_LOOP		(1 << 6)	/* 1=internal loopback */
#define V_PLL_ADJ_MASK		(0x3 << 2)
#define V_PCM_DR_MASK		(0x3 << 4)

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
#define V_TRP_IRQ_0		(0x0 << 2)	/* FIFO enabled, no interrupt */
#define V_TRP_IRQ_64		(0x1 << 2)	/* FIFO enabled, int @ 64 bytes */
#define V_TRP_IRQ_128		(0x2 << 2)	/* FIFO enabled, int @ 128 bytes */
#define V_TRP_IRQ_256		(0x3 << 2)	/* FIFO enabled, int @ 256 bytes */
#define V_TRP_IRQ_512		(0x4 << 2)	/* FIFO enabled, int @ 512 bytes */
#define V_TRP_IRQ_1024		(0x5 << 2)	/* FIFO enabled, int @ 1024 bytes */
#define V_TRP_IRQ_2048		(0x6 << 2)	/* FIFO enabled, int @ 2048 bytes */
#define V_TRP_IRQ_4096		(0x7 << 2)	/* FIFO enabled, int @ 4096 bytes */
#define V_TRP_IRQ		(0x1 << 2)	/* FIFO enabled, interrupt at end of frame (HDLC mode) */
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

/* R_FIFO_MD bits */
#define V_FIFO_MD_00		(0x0 << 0)
#define V_FIFO_MD_01		(0x1 << 0)
#define V_FIFO_MD_10		(0x2 << 0)
#define V_FIFO_MD_11		(0x3 << 0)
#define V_DF_MD_SM		(0x0 << 2)	/* simple data flow mode */
#define V_DF_MD_CSM		(0x1 << 2)	/* channel select mode */
#define V_DF_MD_FSM		(0x3 << 2)	/* FIFO sequence mode */
#define V_FIFO_SZ_00		(0x0 << 4)
#define V_FIFO_SZ_01		(0x1 << 4)
#define V_FIFO_SZ_10		(0x2 << 4)
#define V_FIFO_SZ_11		(0x3 << 4)

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

/* R_ST_SYNC bits */
#define V_MAN_SYNC		(1 << 3)	/* 1=manual sync mode */
#define V_SYNC_SEL_MASK		(0x03)

/* A_ST_WR_STA bits */
#define V_ST_SET_STA_MASK	(0x0f)
#define V_ST_LD_STA		(1 << 4)	/* 1=force ST_SET_STA mode, must be manually cleared 6us later */
#define V_ST_ACT_NOP		(0x0 << 5)	/* NOP */
#define V_ST_ACT_DEACTIVATE	(0x2 << 5)	/* start deactivation. auto-clears */
#define V_ST_ACT_ACTIVATE	(0x3 << 5)	/* start activation. auto-clears. */
#define V_SET_G2_G3		(1 << 7)	/* 1=auto G2->G3 in NT mode. auto-clears after transition. */

/* A_ST_RD_STA */
#define V_ST_STA_MASK		(0x0f)
#define V_FR_SYNC		(1 << 4)	/* 1=synchronized */
#define V_T2_EXP		(1 << 5)	/* 1=T2 expired (NT only) */
#define V_INFO0			(1 << 6)	/* 1=INFO0 */
#define V_G2_G3			(1 << 7)	/* 1=allows G2->G3 (NT only, auto-clears) */

/* A_ST_CLK_DLY bits */
#define V_ST_SMPL_SHIFT		(4)

/* A_ST_CTRL0 bits */
#define V_B1_EN			(1 << 0)	/* 1=B1-channel transmit */
#define V_B2_EN			(1 << 1)	/* 1=B2-channel transmit */
#define V_ST_MD			(1 << 2)	/* 0=TE, 1=NT */
#define V_D_PRIO		(1 << 3)	/* D-Chan priority 0=high, 1=low */
#define V_SQ_EN			(1 << 4)	/* S/Q bits transmit (1=enabled) */
#define V_96KHZ			(1 << 5)	/* 1=transmit test signal */
#define V_TX_LI			(1 << 6)	/* 0=capacitive line mode, 1=non-capacitive */
#define V_ST_STOP		(1 << 7)	/* 1=power down */

/* A_ST_CTRL1 bits */
#define V_G2_G3_EN		(1 << 0)	/* 1=G2->G3 allowed without V_SET_G2_G3 */
#define V_D_HI			(1 << 2)	/* 1=D-chan reset */
#define V_E_IGNO		(1 << 3)	/* TE:1=ignore Echan, NT:should always be 1. */
#define V_E_LO			(1 << 4)	/* NT only: 1=force Echan low */
#define V_B12_SWAP		(1 << 7)	/* 1=swap B1/B2 */

/* A_ST_CTRL2 bits */
#define V_B1_RX_EN		(1 << 0)	/* 1=enable B1 RX */
#define V_B2_RX_EN		(1 << 1)	/* 1=enable B2 RX */
#define V_ST_TRI		(1 << 6)	/* 1=tristate S/T output buffer */

#define NUM_REGS 0xff
#define NUM_PCI 12

/* From this point down, things only the kernel needs to know about */
#ifdef __KERNEL__

#define HFC_T1					0
#define HFC_T2					1
#define HFC_T3					2

#define MAX_SPANS_PER_CARD			8

#define WCB4XXP_CHANNELS_PER_SPAN		3	/* 2 B-channels and 1 D-Channel for each BRI span */
#define WCB4XXP_HDLC_BUF_LEN			32	/* arbitrary, just the max # of byts we will send to DAHDI per call */

struct b4xxp_span {
	struct b4xxp *parent;
	int port;				/* which S/T port this span belongs to */

	unsigned char writechunk[WCB4XXP_CHANNELS_PER_SPAN * DAHDI_CHUNKSIZE];
	unsigned char readchunk[WCB4XXP_CHANNELS_PER_SPAN * DAHDI_CHUNKSIZE];

	int sync;				/* sync priority */

	int oldstate;				/* old state machine state */
	int newalarm;				/* alarm to send to zaptel once alarm timer expires */
	unsigned long alarmtimer;

	int te_mode;				/* 1=TE, 0=NT */
	unsigned long hfc_timers[WCB4XXP_CHANNELS_PER_SPAN];		/* T1, T2, T3 */
	int hfc_timer_on[WCB4XXP_CHANNELS_PER_SPAN];			/* 1=timer active */
	int fifos[WCB4XXP_CHANNELS_PER_SPAN];				/* B1, B2, D <--> host fifo numbers */

	/* HDLC controller fields */
	struct dahdi_chan *sigchan;		/* pointer to the signalling channel for this span */
	int sigactive;				/* nonzero means we're in the middle of sending an HDLC frame */
	atomic_t hdlc_pending;			/* hdlc_hard_xmit() increments, hdlc_tx_frame() decrements */
	int frames_out;
	int frames_in;

	struct dahdi_span span;			/* zaptel span info for this span */
	struct dahdi_chan *chans[WCB4XXP_CHANNELS_PER_SPAN]; /* Individual channels */
	struct dahdi_echocan_state ec[WCB4XXP_CHANNELS_PER_SPAN]; /* echocan state for each channel */
	struct dahdi_chan _chans[WCB4XXP_CHANNELS_PER_SPAN]; /* Backing memory */
};

enum cards_ids {	/* Cards ==> Brand & Model 		*/
	B410P = 0,	/* Digium B410P 			*/
	B200P_OV,	/* OpenVox B200P 			*/
	B400P_OV,	/* OpenVox B400P 			*/
	B800P_OV,	/* OpenVox B800P 			*/
	DUOBRI,		/* HFC-2S Junghanns.NET duoBRI PCI 	*/
	QUADBRI,	/* HFC-4S Junghanns.NET quadBRI PCI	*/
	OCTOBRI,	/* HFC-8S Junghanns.NET octoBRI PCI	*/
	BN2S0,		/* BeroNet BN2S0			*/
	BN4S0,		/* Beronet BN4S0			*/
	BN8S0,		/* BeroNet BN8S0			*/
	BSWYX_SX2,	/* Swyx 4xS0 SX2 QuadBri		*/
	QUADBRI_EVAL	/* HFC-4S CCD Eval. Board		*/
	};

/* This structure exists one per card */
struct b4xxp {
	char *variety;
	int chiprev;				/* revision of HFC-4S */

	struct pci_dev *pdev;			/* Pointer to PCI device */
	void __iomem *addr;			/* I/O address (memory mapped) */
	void __iomem *ioaddr;			/* I/O address (index based) */
	int irq;				/* IRQ used by device */

	spinlock_t reglock;			/* lock for all register accesses */
	spinlock_t seqlock;			/* lock for "sequence" accesses that must be ordered */
	spinlock_t fifolock;			/* lock for all FIFO accesses (reglock must be available) */

	volatile unsigned long ticks;

	unsigned long fifo_en_rxint;		/* each bit is the RX int enable for that FIFO */
	unsigned long fifo_en_txint;		/* each bit is the TX int enable for that FIFO */

	unsigned char fifo_irqstatus[8];	/* top-half ORs in new interrupts, bottom-half ANDs them out */
	unsigned char misc_irqstatus;		/* same for this */
	unsigned char st_irqstatus;		/* same for this too */

	unsigned int numspans;

	int blinktimer;				/* for the fancy LED alarms */
	int alarmpos;				/* ditto */

	int cardno;				/* Which card we are */
	int globalconfig;			/* Whether global setup has been done */
	int syncspan;				/* span that HFC uses for sync on this card */
	int running;				/* interrupts are enabled */

	struct b4xxp_span spans[MAX_SPANS_PER_CARD];	/* Individual spans */
	int order;				/* Order */
	int flags;				/* Device flags */
	enum cards_ids card_type;		/* For LED handling mostly */
	int master;				/* Are we master */
	int ledreg;				/* copy of the LED Register */
	unsigned int gpio;
	unsigned int gpioctl;
	int spansstarted;			/* number of spans started */

	/* Flags for our bottom half */
	unsigned int shutdown;			/* 1=bottom half doesn't process anything, just returns */
	struct tasklet_struct b4xxp_tlet;
	struct dahdi_device *ddev;
};

/* CPLD access bits */
#define B4_RDADDR	0
#define B4_WRADDR	1
#define B4_COUNT	2
#define B4_DMACTRL	3	
#define B4_INTR		4
#define B4_VERSION	6
#define B4_LEDS		7
#define B4_GPIOCTL	8
#define B4_GPIO		9
#define B4_LADDR	10
#define B4_LDATA	11

#define B4_LCS		(1 << 11)
#define B4_LCS2		(1 << 12)
#define B4_LALE		(1 << 13)
#define B4_LFRMR_CS	(1 << 10)	/* Framer's ChipSelect signal */
#define B4_ACTIVATE	(1 << 12)
#define B4_LREAD	(1 << 15)
#define B4_LWRITE	(1 << 16)

#define LED_OFF		(0)
#define LED_RED		(1)
#define LED_GREEN	(2)

#define get_F(f1, f2, flen) {				\
	f1 = hfc_readcounter8(b4, A_F1);		\
	f2 = hfc_readcounter8(b4, A_F2);		\
	flen = f1 - f2;					\
							\
	if(flen < 0)					\
		flen += (HFC_FMAX - HFC_FMIN) + 1;	\
	}

#define get_Z(z1, z2, zlen) {				\
	z1 = hfc_readcounter16(b4, A_Z1);		\
	z2 = hfc_readcounter16(b4, A_Z2);		\
	zlen = z1 - z2;					\
							\
	if(zlen < 0)					\
		zlen += (HFC_ZMAX - HFC_ZMIN) + 1;	\
	}

#define flush_pci()	(void)ioread8(b4->addr + R_STATUS)

#endif	/* __KERNEL__ */
#endif	/* _B4XX_H_ */

