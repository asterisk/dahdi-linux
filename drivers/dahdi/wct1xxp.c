/*
 * Digium, Inc.  Wildcard T100P T1/PRI card Driver
 *
 * Written by Mark Spencer <markster@digium.com>
 *            Matthew Fredrickson <creslin@digium.com>
 *            William Meadows <wmeadows@digium.com>
 *
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

#define WC_MAX_CARDS	32

/*
#define TEST_REGS
*/

/* Define to get more attention-grabbing but slightly more I/O using
   alarm status */
#define FANCY_ALARM


#define DELAY	0x0	/* 30 = 15 cycles, 10 = 8 cycles, 0 = 3 cycles */

#define WC_CNTL    	0x00
#define WC_OPER		0x01
#define WC_AUXC    	0x02
#define WC_AUXD    	0x03
#define WC_MASK0   	0x04
#define WC_MASK1   	0x05
#define WC_INTSTAT 	0x06

#define WC_DMAWS	0x08
#define WC_DMAWI	0x0c
#define WC_DMAWE	0x10
#define WC_DMARS	0x18
#define WC_DMARI	0x1c
#define WC_DMARE	0x20
#define WC_CURPOS	0x24

#define WC_SERC		0x2d
#define WC_FSCDELAY	0x2f

#define WC_USERREG	0xc0

#define WC_CLOCK	0x0
#define WC_LEDTEST	0x1
#define WC_VERSION	0x2

/* Offset between transmit and receive */
#define WC_OFFSET	4

#define BIT_CS		(1 << 7)
#define BIT_ADDR	(0xf << 3)

#define BIT_LED0	(1 << 0)
#define BIT_LED1	(1 << 1)
#define BIT_TEST	(1 << 2)

static char *chips[] =
{
	"DS2152",
	"DS21352",
	"DS21552",
	"Unknown Chip (3)",
	"DS2154",
	"DS21354",
	"DS21554",
	"Unknown Chip (7)",
};

static int chanmap_t1[] = 
{ 2,1,0,
  6,5,4,
  10,9,8,
  14,13,12,
  18,17,16,
  22,21,20,
  26,25,24,
  30,29,28 };

static int chanmap_e1[] = 
{ 2,1,0,
  7,6,5,4,
  11,10,9,8,
  15,14,13,12,
  19,18,17,16,
  23,22,21,20,
  27,26,25,24,
  31,30,29,28 };

#ifdef FANCY_ALARM
static int altab[] = {
0, 0, 0, 1, 2, 3, 4, 6, 8, 9, 11, 13, 16, 18, 20, 22, 24, 25, 27, 28, 29, 30, 31, 31, 32, 31, 31, 30, 29, 28, 27, 25, 23, 22, 20, 18, 16, 13, 11, 9, 8, 6, 4, 3, 2, 1, 0, 0, 
};
#endif

struct t1xxp {
	struct pci_dev *dev;
	spinlock_t lock;
	int ise1;
	int num;
	/* Our offset for finding channel 1 */
	int offset;
	char *variety;
	unsigned int intcount;
	int usecount;
	int clocktimeout;
	int sync;
	int dead;
	int blinktimer;
	int alarmtimer;
	int loopupcnt;
	int loopdowncnt;
	int miss;
	int misslast;
	int *chanmap;
#ifdef FANCY_ALARM
	int alarmpos;
#endif
	unsigned char ledtestreg;
	unsigned char outbyte;
	unsigned long ioaddr;
	unsigned short canary;
	/* T1 signalling */
	unsigned char txsiga[3];
	unsigned char txsigb[3];
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile unsigned char *writechunk;					/* Double-word aligned write memory */
	volatile unsigned char *readchunk;					/* Double-word aligned read memory */
	unsigned char ec_chunk1[31][DAHDI_CHUNKSIZE];
	unsigned char ec_chunk2[31][DAHDI_CHUNKSIZE];
	unsigned char tempo[32];
	struct dahdi_span span;						/* Span */
	struct dahdi_device *ddev;
	struct dahdi_chan *chans[31];					/* Channels */
};

#define CANARY 0xca1e

static int debug = 0;   /* doesnt do anything */

static struct t1xxp *cards[WC_MAX_CARDS];

static inline void start_alarm(struct t1xxp *wc)
{
#ifdef FANCY_ALARM
	wc->alarmpos = 0;
#endif
	wc->blinktimer = 0;
}

static inline void stop_alarm(struct t1xxp *wc)
{
#ifdef FANCY_ALARM
	wc->alarmpos = 0;
#endif
	wc->blinktimer = 0;
}

static inline void __select_framer(struct t1xxp *wc, int reg)
{
	/* Top four bits of address from AUX 6-3 */
	wc->outbyte &= ~BIT_CS;
	wc->outbyte &= ~BIT_ADDR;
	wc->outbyte |= (reg & 0xf0) >> 1;
	outb(wc->outbyte, wc->ioaddr + WC_AUXD);
}

static inline void __select_control(struct t1xxp *wc)
{
	if (!(wc->outbyte & BIT_CS)) {
		wc->outbyte |= BIT_CS;
		outb(wc->outbyte, wc->ioaddr + WC_AUXD);
	}
}

static int t1xxp_open(struct dahdi_chan *chan)
{
	struct t1xxp *wc = chan->pvt;
	if (wc->dead)
		return -ENODEV;
	wc->usecount++;
	return 0;
}

static int __t1_get_reg(struct t1xxp *wc, int reg)
{
	unsigned char res;
	__select_framer(wc, reg);
	/* Get value */
	res = inb(wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return res;
}

static int __t1_set_reg(struct t1xxp *wc, int reg, unsigned char val)
{
	__select_framer(wc, reg);
	/* Send address */
	outb(val, wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return 0;
}

static int __control_set_reg(struct t1xxp *wc, int reg, unsigned char val)
{
	__select_control(wc);
	outb(val, wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return 0;
}

static int control_set_reg(struct t1xxp *wc, int reg, unsigned char val)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&wc->lock, flags);
	res = __control_set_reg(wc, reg, val);
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

static int __control_get_reg(struct t1xxp *wc, int reg)
{
	unsigned char res;
	/* The following makes UTTERLY no sense, but what was happening
	   was that reads in some cases were not actually happening
	   on the physical bus. Why, we dunno. But in debugging, we found
	   that writing before reading (in this case to an unused position)
	   seems to get rid of the problem */
	__control_set_reg(wc,3,0x69); /* do magic here */
	/* now get the read byte from the Xilinx part */
	res = inb(wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return res;
}

static int control_get_reg(struct t1xxp *wc, int reg)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&wc->lock, flags);
	res = __control_get_reg(wc, reg);
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

static void t1xxp_release(struct t1xxp *wc)
{
	unsigned int x;

	dahdi_unregister_device(wc->ddev);
	for (x = 0; x < (wc->ise1 ? 31 : 24); x++) {
		kfree(wc->chans[x]);
	}
	dahdi_free_device(wc->ddev);
	kfree(wc);
	printk(KERN_INFO "Freed a Wildcard\n");
}

static int t1xxp_close(struct dahdi_chan *chan)
{
	struct t1xxp *wc = chan->pvt;
	wc->usecount--;
	/* If we're dead, release us now */
	if (!wc->usecount && wc->dead) 
		t1xxp_release(wc);
	return 0;
}

static void t1xxp_enable_interrupts(struct t1xxp *wc)
{
	/* Clear interrupts */
	outb(0xff, wc->ioaddr + WC_INTSTAT);
	/* Enable interrupts (we care about all of them) */
	outb(0x3c /* 0x3f */, wc->ioaddr + WC_MASK0); 
	/* No external interrupts */
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void t1xxp_start_dma(struct t1xxp *wc)
{
	/* Reset Master and TDM */
	outb(DELAY | 0x0f, wc->ioaddr + WC_CNTL);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	outb(DELAY | 0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
	if (debug) printk(KERN_DEBUG "Started DMA\n");
}

static void __t1xxp_stop_dma(struct t1xxp *wc)
{
	outb(0x00, wc->ioaddr + WC_OPER);
}

static void __t1xxp_disable_interrupts(struct t1xxp *wc)	
{
	outb(0x00, wc->ioaddr + WC_MASK0);
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void __t1xxp_set_clear(struct t1xxp *wc)
{
	/* Setup registers */
	int x,y;
	unsigned char b;

	/* No such thing under E1 */
	if (wc->ise1) {
		printk(KERN_NOTICE "Can't set clear mode on an E1!\n");
		return;
	}

	for (x=0;x<3;x++) {
		b = 0;
		for (y=0;y<8;y++)
			if (wc->chans[x * 8 + y]->sig & DAHDI_SIG_CLEAR)
				b |= (1 << y);
		__t1_set_reg(wc, 0x39 + x, b);
	}
}

static void t1xxp_t1_framer_start(struct t1xxp *wc)
{
	int i;
	char *coding, *framing;
	unsigned long endjiffies;
	int alreadyrunning = wc->span.flags & DAHDI_FLAG_RUNNING;
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);

	/* Build up config */
	i = 0x20;
	if (wc->span.lineconfig & DAHDI_CONFIG_ESF) {
		coding = "ESF";
		i = 0x88;
	} else {
		coding = "SF";
	}
	if (wc->span.lineconfig & DAHDI_CONFIG_B8ZS) {
		framing = "B8ZS";
		i |= 0x44;
	} else {
		framing = "AMI";
	}
	__t1_set_reg(wc, 0x38, i);
	if (!(wc->span.lineconfig & DAHDI_CONFIG_ESF)) {
		/* 1c in FDL bit */
		__t1_set_reg(wc, 0x7e, 0x1c);
	} else {
		__t1_set_reg(wc, 0x7e, 0x00);
	}

	/* Set outgoing LBO */
	__t1_set_reg(wc, 0x7c, wc->span.txlevel << 5);

	printk(KERN_DEBUG "Using %s/%s coding/framing\n", coding, framing);
	if (!alreadyrunning) {
		/* Setup the clear channels */
		__t1xxp_set_clear(wc);

		/* Set LIRST bit to 1 */
		__t1_set_reg(wc, 0x0a, 0x80);
		spin_unlock_irqrestore(&wc->lock, flags);

		/* Wait 100ms to give plenty of time for reset */
		endjiffies = jiffies + 10;
		while(endjiffies < jiffies);

		spin_lock_irqsave(&wc->lock, flags);
		
		/* Reset LIRST bit and reset elastic stores */
		__t1_set_reg(wc, 0xa, 0x30);

		wc->span.flags |= DAHDI_FLAG_RUNNING;
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

static void t1xxp_e1_framer_start(struct t1xxp *wc)
{
	int i;
	char *coding, *framing;
	unsigned long endjiffies;
	int alreadyrunning = wc->span.flags & DAHDI_FLAG_RUNNING;
	unsigned long flags;
	char *crcing = "";
	unsigned char ccr1, tcr1, tcr2;

	spin_lock_irqsave(&wc->lock, flags);

	/* Build up config */
	ccr1 = 0;
	tcr1 = 8;
	tcr2 = 0;
	if (wc->span.lineconfig & DAHDI_CONFIG_CCS) {
		coding = "CCS"; /* Receive CCS */
		ccr1 |= 8;
	} else {
		tcr1 |= 0x20;
		coding = "CAS";
	}
	if (wc->span.lineconfig & DAHDI_CONFIG_HDB3) {
		ccr1 |= 0x44;		/* TX/RX HDB3 */
		framing = "HDB3";
	} else {
		framing = "AMI";
	}
	if (wc->span.lineconfig & DAHDI_CONFIG_CRC4) {
		ccr1 |= 0x11;
		tcr2 |= 0x02;
		crcing = " with CRC4";
	}
	__t1_set_reg(wc, 0x12, tcr1);
	__t1_set_reg(wc, 0x13, tcr2);
	__t1_set_reg(wc, 0x14, ccr1);
	__t1_set_reg(wc, 0x18, 0x20);	/* 120 Ohm */
	
	
#if 0	/* XXX Does LBO Matter? XXX */
	/* Set outgoing LBO */
	__t1_set_reg(wc, 0x7c, wc->span.txlevel << 5);
#endif

	printk(KERN_DEBUG "Using %s/%s coding/framing%s 120 Ohms\n", coding, framing,crcing);
	if (!alreadyrunning) {
	
		__t1_set_reg(wc,0x1b,0x8a); /* CCR3: LIRST & TSCLKM */
		__t1_set_reg(wc,0x20,0x1b); /* TAFR */
		__t1_set_reg(wc,0x21,0x5f); /* TNAFR */
		__t1_set_reg(wc,0x40,0xb); /* TSR1 */
		for(i = 0x41; i <= 0x4f; i++) __t1_set_reg(wc,i,0x55);
		for(i = 0x22; i <= 0x25; i++) __t1_set_reg(wc,i,0xff);
		spin_unlock_irqrestore(&wc->lock, flags);

		/* Wait 100ms to give plenty of time for reset */
		endjiffies = jiffies + 10;
		while(endjiffies < jiffies);

		spin_lock_irqsave(&wc->lock, flags);
		
		__t1_set_reg(wc, 0x1b, 0x9a);	/* Set ESR */
		__t1_set_reg(wc, 0x1b, 0x82);	/* TSCLKM only now */

		/* Reset LIRST bit and reset elastic stores */

		wc->span.flags |= DAHDI_FLAG_RUNNING;
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

static int t1xxp_framer_sanity_check(struct t1xxp *wc)
{
	int res;
	int chipid;
	unsigned long flags;
	int x;

	/* Sanity check */
	spin_lock_irqsave(&wc->lock, flags);
	for (x=0x0;x<192;x++)
		__t1_set_reg(wc, x, 0);
	res = __t1_get_reg(wc, 0x0f);
	res = __t1_get_reg(wc, 0x0f);
	chipid = ((res & 0x80) >> 5) | ((res & 0x30) >> 4);
	wc->ise1 = (res & 0x80) ? (1 << 4) : 0;
	spin_unlock_irqrestore(&wc->lock, flags);

	printk(KERN_DEBUG "Framer: %s, Revision: %d (%s)\n", chips[chipid], res & 0xf, wc->ise1 ? "E1" : "T1");
	return 0;
}

static int t1xxp_framer_hard_reset(struct t1xxp *wc)
{
	int x;
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	/* Initialize all registers to 0 */
	for (x=0x0;x<192;x++)
		__t1_set_reg(wc, x, 0);

	if (wc->ise1) {
		/* Set LOTCMC (switch to RCLCK if TCLK fails) */
		__t1_set_reg(wc, 0x1a, 0x04);
		
	 	/* RSYNC is an input */
		__t1_set_reg(wc, 0x10, 0x20);

		/* Rx elastic store enabled, 2.048 Mhz (in theory) */
		__t1_set_reg(wc, 0x11, 0x06);

		/* TSYNC is an input, Tsis mode */
		__t1_set_reg(wc, 0x12, 0x08);

		/* Tx elastic store enabled, 2.048 Mhz (in theory) */
		__t1_set_reg(wc, 0x1b, 0x82);


		
	} else {
		/* Full-on sync required for T1 */
		__t1_set_reg(wc, 0x2b, 0x08);
	 	/* RSYNC is an input */
		__t1_set_reg(wc, 0x2c, 0x08);

		/* Enable tx RBS bits */
		__t1_set_reg(wc, 0x35, 0x10);

		/* TSYNC is output */
		__t1_set_reg(wc, 0x36, 0x04);

		/* Tx and Rx elastic store enabled, 2.048 Mhz (in theory) */
		__t1_set_reg(wc, 0x37, 0x9c);

		/* Setup Loopup / Loopdown codes */
		__t1_set_reg(wc, 0x12, 0x22);
		__t1_set_reg(wc, 0x14, 0x80);
		__t1_set_reg(wc, 0x15, 0x80);
	}

	spin_unlock_irqrestore(&wc->lock, flags);
	return 0;
}

static int t1xxp_rbsbits(struct dahdi_chan *chan, int bits)
{
	struct t1xxp *wc = chan->pvt;
	unsigned long flags;
	int b,o;
	unsigned char mask;
	
	/* Byte offset */
	spin_lock_irqsave(&wc->lock, flags);
	if (wc->ise1) {
                if (chan->chanpos < 16) {
                       mask = ((bits << 4) | wc->chans[chan->chanpos - 1 + 16]->txsig);
                        __t1_set_reg(wc, 0x40 + chan->chanpos, mask);
                }
		else if (chan->chanpos > 16) {
			mask = (bits | (wc->chans[chan->chanpos - 1 - 16]->txsig << 4));
			__t1_set_reg(wc, 0x40 + chan->chanpos - 16, mask);
		}
		wc->chans[chan->chanpos - 1]->txsig = bits;
	} else {
		b = (chan->chanpos - 1) / 8;
		o = (chan->chanpos - 1) % 8;

		mask = (1 << o);

		if (bits & DAHDI_ABIT) {
			/* Set A-bit */
			wc->txsiga[b] |= mask;
		} else {
			/* Clear A-bit */
			wc->txsiga[b] &= ~mask;
		}
		if (bits & DAHDI_BBIT) {
			/* Set B-bit */
			wc->txsigb[b] |= mask;
		} else {
			wc->txsigb[b] &= ~mask;
		}
		/* Output new values */
		__t1_set_reg(wc, 0x70 + b, wc->txsiga[b]);
		__t1_set_reg(wc, 0x73 + b, wc->txsigb[b]);
		__t1_set_reg(wc, 0x76 + b, wc->txsiga[b]);
		__t1_set_reg(wc, 0x79 + b, wc->txsigb[b]);
	}
	spin_unlock_irqrestore(&wc->lock, flags);
	return 0;
}

static int t1xxp_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	switch(cmd) {
	default:
		return -ENOTTY;
	}
}

static inline struct t1xxp *t1xxp_from_span(struct dahdi_span *span)
{
	return container_of(span, struct t1xxp, span);
}

static int t1xxp_startup(struct file *file, struct dahdi_span *span)
{
	struct t1xxp *wc = t1xxp_from_span(span);

	int i,alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

	/* initialize the start value for the entire chunk of last ec buffer */
	for(i = 0; i < span->channels; i++)
	{
		memset(wc->ec_chunk1[i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
		memset(wc->ec_chunk2[i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
	}

	/* Reset framer with proper parameters and start */
	if (wc->ise1)
		t1xxp_e1_framer_start(wc);
	else
		t1xxp_t1_framer_start(wc);
	printk(KERN_INFO "Calling startup (flags is %lu)\n", span->flags);

	if (!alreadyrunning) {
		/* Only if we're not already going */
		t1xxp_enable_interrupts(wc);
		t1xxp_start_dma(wc);
		span->flags |= DAHDI_FLAG_RUNNING;
	}
	return 0;
}

static int t1xxp_shutdown(struct dahdi_span *span)
{
	struct t1xxp *wc = t1xxp_from_span(span);
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	__t1xxp_stop_dma(wc);
	__t1xxp_disable_interrupts(wc);
	span->flags &= ~DAHDI_FLAG_RUNNING;
	spin_unlock_irqrestore(&wc->lock, flags);

	t1xxp_framer_hard_reset(wc);
	return 0;
}

static int t1xxp_maint(struct dahdi_span *span, int cmd)
{
	struct t1xxp *wc = t1xxp_from_span(span);
	int res = 0;
	unsigned long flags;
	spin_lock_irqsave(&wc->lock, flags);
	if (wc->ise1) {
		switch(cmd) {
		case DAHDI_MAINT_NONE:
			__t1_set_reg(wc,0xa8,0); /* no loops */
			break;
		case DAHDI_MAINT_LOCALLOOP:
			__t1_set_reg(wc,0xa8,0x40); /* local loop */
			break;
		case DAHDI_MAINT_REMOTELOOP:
			__t1_set_reg(wc,0xa8,0x80); /* remote loop */
			break;
		case DAHDI_MAINT_LOOPUP:
		case DAHDI_MAINT_LOOPDOWN:
			res = -ENOSYS;
			break;
		default:
			printk(KERN_NOTICE "wct1xxp/E1: Unknown maint command: %d\n", cmd);
			res = -EINVAL;
			break;
		}
	} else {
		switch(cmd) {
	    case DAHDI_MAINT_NONE:
			__t1_set_reg(wc,0x19,0); /* no local loop */
			__t1_set_reg(wc,0x0a,0); /* no remote loop */
			__t1_set_reg(wc, 0x30, 0); /* stop sending loopup code*/
			break;
	    case DAHDI_MAINT_LOCALLOOP:
			__t1_set_reg(wc,0x19,0x40); /* local loop */
			__t1_set_reg(wc,0x0a,0); /* no remote loop */
			break;
	    case DAHDI_MAINT_REMOTELOOP:
			__t1_set_reg(wc,0x1e,0); /* no local loop */
			__t1_set_reg(wc,0x0a,0x40); /* remote loop */
			break;
	    case DAHDI_MAINT_LOOPUP:
			__t1_set_reg(wc,0x30,2); /* send loopup code */
			__t1_set_reg(wc,0x12,0x22); /* send loopup code */
			__t1_set_reg(wc,0x13,0x80); /* send loopup code */
			break;
	    case DAHDI_MAINT_LOOPDOWN:
			__t1_set_reg(wc,0x30,2); /* send loopdown code */
			__t1_set_reg(wc,0x12,0x62); /* send loopdown code */
			__t1_set_reg(wc,0x13,0x90); /* send loopdown code */
			break;
	    default:
			printk(KERN_NOTICE "wct1xxp/T1: Unknown maint command: %d\n", cmd);
			res = -EINVAL;
	   }
	}
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

static int
t1xxp_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	struct t1xxp *wc = chan->pvt;
	unsigned long flags;
	int alreadyrunning = chan->span->flags & DAHDI_FLAG_RUNNING;

	spin_lock_irqsave(&wc->lock, flags);

	if (alreadyrunning && !wc->ise1)
		__t1xxp_set_clear(wc);

	spin_unlock_irqrestore(&wc->lock, flags);
	return 0;
}

static int
t1xxp_spanconfig(struct file *file, struct dahdi_span *span,
		 struct dahdi_lineconfig *lc)
{
	struct t1xxp *wc = t1xxp_from_span(span);

	/* Do we want to SYNC on receive or not */
	wc->sync = (lc->sync) ? 1 : 0;
	/* If already running, apply changes immediately */
	if (span->flags & DAHDI_FLAG_RUNNING)
		return t1xxp_startup(file, span);

	return 0;
}

static const struct dahdi_span_ops t1xxp_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = t1xxp_spanconfig,
	.chanconfig = t1xxp_chanconfig,
	.startup = t1xxp_startup,
	.shutdown = t1xxp_shutdown,
	.rbsbits = t1xxp_rbsbits,
	.maint = t1xxp_maint,
	.open = t1xxp_open,
	.close = t1xxp_close,
	.ioctl = t1xxp_ioctl,
};

static int t1xxp_software_init(struct t1xxp *wc)
{
	int x;
	/* Find position */
	for (x=0;x<WC_MAX_CARDS;x++) {
		if (!cards[x]) {
			cards[x] = wc;
			break;
		}
	}
	if (x >= WC_MAX_CARDS)
		return -1;

	wc->ddev = dahdi_create_device();

	wc->num = x;
	sprintf(wc->span.name, "WCT1/%d", wc->num);
	snprintf(wc->span.desc, sizeof(wc->span.desc) - 1, "%s Card %d", wc->variety, wc->num);
	wc->ddev->manufacturer = "Digium";
	wc->ddev->devicetype = wc->variety;
	wc->ddev->location = kasprintf(GFP_KERNEL, "PCI Bus %02d Slot %02d",
				      wc->dev->bus->number,
				      PCI_SLOT(wc->dev->devfn) + 1);
	if (!wc->ddev->location)
		return -ENOMEM;

	wc->span.chans = wc->chans;
	wc->span.flags = DAHDI_FLAG_RBS;
	if (wc->ise1) {
		wc->span.channels = 31;
		wc->span.deflaw = DAHDI_LAW_ALAW;
		wc->span.linecompat = DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
		wc->span.spantype = "E1";
	} else {
		wc->span.channels = 24;
		wc->span.deflaw = DAHDI_LAW_MULAW;
		wc->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS | DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
		wc->span.spantype = "T1";
	}
	for (x=0;x<wc->span.channels;x++) {
		sprintf(wc->chans[x]->name, "WCT1/%d/%d", wc->num, x + 1);
		wc->chans[x]->sigcap = DAHDI_SIG_EM | DAHDI_SIG_CLEAR | DAHDI_SIG_EM_E1 | 
				      DAHDI_SIG_FXSLS | DAHDI_SIG_FXSGS | 
				      DAHDI_SIG_FXSKS | DAHDI_SIG_FXOLS | DAHDI_SIG_DACS_RBS |
				      DAHDI_SIG_FXOGS | DAHDI_SIG_FXOKS | DAHDI_SIG_CAS | DAHDI_SIG_SF;
		wc->chans[x]->pvt = wc;
		wc->chans[x]->chanpos = x + 1;
	}
	wc->span.ops = &t1xxp_span_ops;
	list_add_tail(&wc->span.device_node, &wc->ddev->spans);
	if (dahdi_register_device(wc->ddev, &wc->dev->dev)) {
		printk(KERN_NOTICE "Unable to register span with DAHDI\n");
		return -1;
	}
	return 0;
}

static inline void __handle_leds(struct t1xxp *wc)
{
	int oldreg;
	wc->blinktimer++;

	if (wc->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE)) {
		/* Red/Blue alarm */
#ifdef FANCY_ALARM
		if (wc->blinktimer == (altab[wc->alarmpos] >> 1)) {
			wc->ledtestreg = (wc->ledtestreg | BIT_LED1) & ~BIT_LED0;
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
		}
		if (wc->blinktimer == 0xf) {
			wc->ledtestreg = wc->ledtestreg & ~(BIT_LED0 | BIT_LED1);
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
			wc->blinktimer = -1;
			wc->alarmpos++;
			if (wc->alarmpos >= (sizeof(altab) / sizeof(altab[0])))
				wc->alarmpos = 0;
		}
#else
		if (wc->blinktimer == 160) {
			wc->ledtestreg = (wc->ledtestreg | BIT_LED1) & ~BIT_LED0;
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
		} else if (wc->blinktimer == 480) {
			wc->ledtestreg = wc->ledtestreg & ~(BIT_LED0 | BIT_LED1);
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
			wc->blinktimer = 0;
		}
#endif
	} else if (wc->span.alarms & DAHDI_ALARM_YELLOW) {
		/* Yellow Alarm */
		if (!(wc->blinktimer % 2)) 
			wc->ledtestreg = (wc->ledtestreg | BIT_LED1) & ~BIT_LED0;
		else
			wc->ledtestreg = (wc->ledtestreg | BIT_LED0) & ~BIT_LED1;
		__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
	} else {
		/* No Alarm */
		oldreg = wc->ledtestreg;
		if (wc->span.maintstat != DAHDI_MAINT_NONE)
			wc->ledtestreg |= BIT_TEST;
		else
			wc->ledtestreg &= ~BIT_TEST;
		if (wc->span.flags & DAHDI_FLAG_RUNNING)
			wc->ledtestreg = (wc->ledtestreg | BIT_LED0) & ~BIT_LED1;
		else
			wc->ledtestreg = wc->ledtestreg & ~(BIT_LED0 | BIT_LED1);
		if (oldreg != wc->ledtestreg)
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
	}
}

static void t1xxp_transmitprep(struct t1xxp *wc, int ints)
{
	volatile unsigned char *txbuf;
	int x,y;
	int pos;
	if (ints & 0x04 /* 0x01 */) {
		/* We just finished sending the first buffer, start filling it
		   now */
		txbuf = wc->writechunk;
	} else {
		/* Just finished sending second buffer, fill it now */
		txbuf = wc->writechunk + 32 * DAHDI_CHUNKSIZE;
	}
	dahdi_transmit(&wc->span);
	for (x=0;x<wc->offset;x++)
		txbuf[x] = wc->tempo[x];
	for (y=0;y<DAHDI_CHUNKSIZE;y++) {
		for (x=0;x<wc->span.channels;x++) {
			pos = y * 32 + wc->chanmap[x] + wc->offset;
			/* Put channel number as outgoing data */
			if (pos < 32 * DAHDI_CHUNKSIZE)
				txbuf[pos] = wc->chans[x]->writechunk[y];
			else
				wc->tempo[pos - 32 * DAHDI_CHUNKSIZE] = wc->chans[x]->writechunk[y];
		}
	}
}

static void t1xxp_receiveprep(struct t1xxp *wc, int ints)
{
	volatile unsigned char *rxbuf;
	volatile unsigned int *canary;
	int x;
	int y;
	unsigned int oldcan;
	if (ints & 0x04) {
		/* Just received first buffer */
		rxbuf = wc->readchunk;
		canary = (unsigned int *)(wc->readchunk + DAHDI_CHUNKSIZE * 64 - 4);
	} else {
		rxbuf = wc->readchunk + DAHDI_CHUNKSIZE * 32;
		canary = (unsigned int *)(wc->readchunk + DAHDI_CHUNKSIZE * 32 - 4);
	}
	oldcan = *canary;
	if (((oldcan & 0xffff0000) >> 16) != CANARY) {
		/* Check top part */
		if (debug) printk(KERN_DEBUG "Expecting top %04x, got %04x\n", CANARY, (oldcan & 0xffff0000) >> 16);
		wc->ddev->irqmisses++;
	} else if ((oldcan & 0xffff) != ((wc->canary - 1) & 0xffff)) {
		if (debug) printk(KERN_DEBUG "Expecting bottom %d, got %d\n", wc->canary - 1, oldcan & 0xffff);
		wc->ddev->irqmisses++;
	}
	for (y=0;y<DAHDI_CHUNKSIZE;y++) {
		for (x=0;x<wc->span.channels;x++) {
			/* XXX Optimize, remove * and + XXX */
			/* Must map received channels into appropriate data */
			wc->chans[x]->readchunk[y] = 
				rxbuf[32 * y + ((wc->chanmap[x] + WC_OFFSET + wc->offset) & 0x1f)];
		}
		if (!wc->ise1) {
			for (x=3;x<32;x+=4) {
				if (rxbuf[32 * y + ((x + WC_OFFSET) & 0x1f)] == 0x7f) {
					if (wc->offset != (x-3)) {
						/* Resync */
						control_set_reg(wc, WC_CLOCK, 0x02 | wc->sync | wc->ise1);
						wc->clocktimeout = 100;
#if 1
						if (debug) printk(KERN_DEBUG "T1: Lost our place, resyncing\n");
#endif
					}
				}
			}
		} else {
			if (!wc->clocktimeout && !wc->span.alarms) {
				if ((rxbuf[32 * y + ((3 + WC_OFFSET + wc->offset) & 0x1f)] & 0x7f) != 0x1b) {
					if (wc->miss) {
						if (debug) printk(KERN_DEBUG "Double miss (%d, %d)...\n", wc->misslast, rxbuf[32 * y + ((3 + WC_OFFSET + wc->offset) & 0x1f)]);
						control_set_reg(wc, WC_CLOCK, 0x02 | wc->sync | wc->ise1);
						wc->clocktimeout = 100;
					} else {
						wc->miss = 1;
						wc->misslast = rxbuf[32 * y + ((3 + WC_OFFSET + wc->offset) & 0x1f)];
					}
				} else {
					wc->miss = 0;
				}
			} else {
				wc->miss = 0;
			}
		} 
	}
	/* Store the next canary */
	canary = (unsigned int *)(rxbuf + DAHDI_CHUNKSIZE * 32 - 4);
	*canary = (wc->canary++) | (CANARY << 16);
	for (x=0;x<wc->span.channels;x++) {
		dahdi_ec_chunk(wc->chans[x], wc->chans[x]->readchunk, wc->ec_chunk2[x]);
		memcpy(wc->ec_chunk2[x],wc->ec_chunk1[x],DAHDI_CHUNKSIZE);
		memcpy(wc->ec_chunk1[x],wc->chans[x]->writechunk,DAHDI_CHUNKSIZE);
	}
	dahdi_receive(&wc->span);
}

static void t1xxp_check_sigbits(struct t1xxp *wc, int x)
{
	int a,b,i,y,rxs;
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	if (wc->ise1) {
		/* Read 5 registers at a time, loading 10 channels at a time */
		for (i = (x * 5); i < (x * 5) + 5; i++) {
			a = __t1_get_reg(wc, 0x31 + i);
			/* Get high channel in low bits */
			rxs = (a & 0xf);
			if (!(wc->chans[i+16]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->chans[i+16]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->chans[i+16], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a >> 4) & 0xf;
			if (!(wc->chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->chans[i]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->chans[i], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
		}
	} else {
		a = __t1_get_reg(wc, 0x60 + x);
		b = __t1_get_reg(wc, 0x63 + x);
		for (y=0;y<8;y++) {
			i = x * 8 + y;
				rxs = 0;
			if (a & (1 << y))
				rxs |= DAHDI_ABIT;
			if (b & (1 << y))
				rxs |= DAHDI_BBIT;
			if (!(wc->chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->chans[i]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->chans[i], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
		}
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

static void t1xxp_check_alarms(struct t1xxp *wc)
{
	unsigned char c,d;
	int alarms;
	int x,j;
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);

	if (wc->ise1) {
		__t1_set_reg(wc, 0x06, 0xff); 
		c = __t1_get_reg(wc, 0x6);
	} else {
		/* Get RIR2 */
		c = __t1_get_reg(wc, 0x31);
		wc->span.rxlevel = c >> 6;
	
		/* Get status register s*/
		__t1_set_reg(wc, 0x20, 0xff); 
		c = __t1_get_reg(wc, 0x20); 
	}

	/* Assume no alarms */
	alarms = 0;

	/* And consider only carrier alarms */
	wc->span.alarms &= (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_NOTOPEN);

	if (wc->ise1) {
		/* XXX Implement me XXX */
	} else {
		/* Detect loopup code if we're not sending one */
		if ((!wc->span.mainttimer) && (c & 0x80)) {
			/* Loop-up code detected */
			if ((wc->loopupcnt++ > 80)  && (wc->span.maintstat != DAHDI_MAINT_REMOTELOOP)) {
				__t1_set_reg(wc, 0x1e, 0);	/* No local loop */
				__t1_set_reg(wc, 0x0a, 0x40);	/* Remote Loop */
				wc->span.maintstat = DAHDI_MAINT_REMOTELOOP;
			}
		} else {
			wc->loopupcnt = 0;
		}
		/* Same for loopdown code */
		if ((!wc->span.mainttimer) && (c & 0x40)) {
			/* Loop-down code detected */
			if ((wc->loopdowncnt++ > 80)  && (wc->span.maintstat == DAHDI_MAINT_REMOTELOOP)) {
				__t1_set_reg(wc, 0x1e, 0);	/* No local loop */
				__t1_set_reg(wc, 0x0a, 0x0);	/* No remote Loop */
				wc->span.maintstat = DAHDI_MAINT_NONE;
			}
		} else
			wc->loopdowncnt = 0;
	}

	if (wc->span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
		for (x=0,j=0;x < wc->span.channels;x++)
			if ((wc->chans[x]->flags & DAHDI_FLAG_OPEN) ||
			    dahdi_have_netdev(wc->chans[x]))
				j++;
		if (!j)
			alarms |= DAHDI_ALARM_NOTOPEN;
	}

	if (wc->ise1) {
		if (c & 0x9) 
			alarms |= DAHDI_ALARM_RED;
		if (c & 0x2)
			alarms |= DAHDI_ALARM_BLUE;
	} else {
		/* Check actual alarm status */
		if (c & 0x3) 
			alarms |= DAHDI_ALARM_RED;
		if (c & 0x8)
			alarms |= DAHDI_ALARM_BLUE;
	}
	/* Keep track of recovering */
	if ((!alarms) && wc->span.alarms)
		wc->alarmtimer = DAHDI_ALARMSETTLE_TIME;

	/* If receiving alarms, go into Yellow alarm state */
	if (alarms && (!wc->span.alarms)) {
#if 0
		printk(KERN_DEBUG "Going into yellow alarm\n");
#endif
		if (wc->ise1)
			__t1_set_reg(wc, 0x21, 0x7f); 
		else
			__t1_set_reg(wc, 0x35, 0x11); 
	}

	if (wc->span.alarms != alarms) {
		d = __control_get_reg(wc, WC_CLOCK); 
		start_alarm(wc);
		if (!(alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_LOOPBACK)) &&
		    wc->sync) {
			/* Use the receive signalling */
			wc->span.syncsrc = wc->span.spanno;
			d |= 1;
		} else {
			wc->span.syncsrc = 0;
			d &= ~1;
		}
		__control_set_reg(wc, WC_CLOCK, d);  
	}
	if (wc->alarmtimer)
		alarms |= DAHDI_ALARM_RECOVER;
	if (c & 0x4)
		alarms |= DAHDI_ALARM_YELLOW;

	wc->span.alarms = alarms;

	spin_unlock_irqrestore(&wc->lock, flags);

	dahdi_alarm_notify(&wc->span);
}

static void t1xxp_do_counters(struct t1xxp *wc)
{
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	if (wc->alarmtimer) {
		if (!--wc->alarmtimer) {
			wc->span.alarms &= ~(DAHDI_ALARM_RECOVER);
			/* Clear yellow alarm */
#if 0
			printk(KERN_DEBUG "Coming out of alarm\n");
#endif
			if (wc->ise1)
				__t1_set_reg(wc, 0x21, 0x5f);
			else
				__t1_set_reg(wc, 0x35, 0x10);
			spin_unlock_irqrestore(&wc->lock, flags);
			dahdi_alarm_notify(&wc->span);
			spin_lock_irqsave(&wc->lock, flags);
		}
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

DAHDI_IRQ_HANDLER(t1xxp_interrupt)
{
	struct t1xxp *wc = dev_id;
	unsigned char ints;
	unsigned long flags;
	int x;

	ints = inb(wc->ioaddr + WC_INTSTAT);
	if (!ints)
		return IRQ_NONE;

	outb(ints, wc->ioaddr + WC_INTSTAT);

	if (!wc->intcount) {
		if (debug) printk(KERN_DEBUG "Got interrupt: 0x%04x\n", ints);
	}
	wc->intcount++;

	if (wc->clocktimeout && !--wc->clocktimeout) 
		control_set_reg(wc, WC_CLOCK, 0x00 | wc->sync | wc->ise1);

	if (ints & 0x0f) {
		t1xxp_receiveprep(wc, ints);
		t1xxp_transmitprep(wc, ints);
	}
	spin_lock_irqsave(&wc->lock, flags);

#if 1
	__handle_leds(wc);
#endif

	spin_unlock_irqrestore(&wc->lock, flags);

	/* Count down timers */
	t1xxp_do_counters(wc);

	/* Do some things that we don't have to do very often */
	x = wc->intcount & 15 /* 63 */;
	switch(x) {
	case 0:
	case 1:
	case 2:
		t1xxp_check_sigbits(wc, x);
		break;
	case 4:
		/* Check alarms 1/4 as frequently */
		if (!(wc->intcount & 0x30))
			t1xxp_check_alarms(wc);
		break;
	}
	
	if (ints & 0x10) 
		printk(KERN_INFO "PCI Master abort\n");

	if (ints & 0x20)
		printk(KERN_INFO "PCI Target abort\n");

	return IRQ_RETVAL(1);
}

static int t1xxp_hardware_init(struct t1xxp *wc)
{
	/* Hardware PCI stuff */
	/* Reset chip and registers */
	outb(DELAY | 0x0e, wc->ioaddr + WC_CNTL);
	/* Set all outputs to 0 */
	outb(0x00, wc->ioaddr + WC_AUXD);
	/* Set all to outputs except AUX1 (TDO). */
	outb(0xfd, wc->ioaddr + WC_AUXC);
	/* Configure the serial port: double clock, 20ns width, no inversion,
	   MSB first */
	outb(0xc8, wc->ioaddr + WC_SERC);

	/* Internally delay FSC by one */
	outb(0x01, wc->ioaddr + WC_FSCDELAY);

	/* Back to normal, with automatic DMA wrap around */
	outb(DELAY | 0x01, wc->ioaddr + WC_CNTL);
	
	/* Make sure serial port and DMA are out of reset */
	outb(inb(wc->ioaddr + WC_CNTL) & 0xf9, WC_CNTL);
	
	/* Setup DMA Addresses */
	/* Start at writedma */
	outl(wc->writedma,                    wc->ioaddr + WC_DMAWS);		/* Write start */
	/* First frame */
	outl(wc->writedma + DAHDI_CHUNKSIZE * 32 - 4, wc->ioaddr + WC_DMAWI);		/* Middle (interrupt) */
	/* Second frame */
	outl(wc->writedma + DAHDI_CHUNKSIZE * 32 * 2 - 4, wc->ioaddr + WC_DMAWE);			/* End */
	
	outl(wc->readdma,                    	 wc->ioaddr + WC_DMARS);	/* Read start */
	/* First frame */
	outl(wc->readdma + DAHDI_CHUNKSIZE * 32 - 4, 	 wc->ioaddr + WC_DMARI);	/* Middle (interrupt) */
	/* Second frame */
	outl(wc->readdma + DAHDI_CHUNKSIZE * 32 * 2 - 4, wc->ioaddr + WC_DMARE);	/* End */
	
	if (debug) printk(KERN_DEBUG "Setting up DMA (write/read = %08lx/%08lx)\n", (long)wc->writedma, (long)wc->readdma);

	/* Check out the controller */
	if (debug) printk(KERN_DEBUG "Controller version: %02x\n", control_get_reg(wc, WC_VERSION));


	control_set_reg(wc, WC_LEDTEST, 0x00);

	/* Sanity check also determines e1 or t1 */
	if (t1xxp_framer_sanity_check(wc))
		return -1;
	if (wc->ise1)
		wc->chanmap = chanmap_e1;
	else
		wc->chanmap = chanmap_t1;
	/* Setup clock appropriately */
	control_set_reg(wc, WC_CLOCK, 0x02 | wc->sync | wc->ise1);
	wc->clocktimeout = 100;
	
	/* Reset the T1 and report */
	t1xxp_framer_hard_reset(wc);
	start_alarm(wc);
	return 0;

}

static int __devinit t1xxp_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct t1xxp *wc;
	unsigned int *canary;
	unsigned int x;
	
	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "%s: pci_enable_device failed\n", __FUNCTION__);
		return -EIO;
	}

	if (!(wc = kmalloc(sizeof(*wc), GFP_KERNEL))) {
		printk(KERN_ERR "%s: Failed allocation a wc\n", __FUNCTION__);
		return -ENOMEM;
	}

	memset(wc, 0x0, sizeof(*wc));
	spin_lock_init(&wc->lock);
	wc->ioaddr = pci_resource_start(pdev, 0);
	wc->dev = pdev;
	wc->offset = 28;	/* And you thought 42 was the answer */
	
	wc->writechunk = 
		/* 32 channels, Double-buffer, Read/Write */
		(unsigned char *)pci_alloc_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 32 * 2 * 2, &wc->writedma);
	if (!wc->writechunk) {
		printk(KERN_NOTICE "wct1xxp: Unable to allocate DMA-able memory\n");
		return -ENOMEM;
	}
	
	/* Read is after the whole write piece (in bytes) */
	wc->readchunk = wc->writechunk + DAHDI_CHUNKSIZE * 32 * 2;
	
	/* Same thing...  */
	wc->readdma = wc->writedma + DAHDI_CHUNKSIZE * 32 * 2;
	
	/* Initialize Write/Buffers to all blank data */
	memset((void *)wc->writechunk,0x00,DAHDI_MAX_CHUNKSIZE * 2 * 2 * 32);
	/* Initialize canary */
	canary = (unsigned int *)(wc->readchunk + DAHDI_CHUNKSIZE * 64 - 4);
	*canary = (CANARY << 16) | (0xffff);
	
	/* Enable bus mastering */
	pci_set_master(pdev);
	
	/* Keep track of which device we are */
	pci_set_drvdata(pdev, wc);
	
	if (request_irq(pdev->irq, t1xxp_interrupt, DAHDI_IRQ_SHARED_DISABLED, "t1xxp", wc)) {
		printk(KERN_NOTICE "t1xxp: Unable to request IRQ %d\n", pdev->irq);
		kfree(wc);
		return -EIO;
	}
	/* Initialize hardware */
	t1xxp_hardware_init(wc);
	
	/* We now know which version of card we have */
	if (wc->ise1) {
		wc->variety = "Digium Wildcard E100P E1/PRA";
	} else {
		wc->variety = "Digium Wildcard T100P T1/PRI";
	}

	for (x = 0; x < (wc->ise1 ? 31 : 24); x++) {
		if (!(wc->chans[x] = kmalloc(sizeof(*wc->chans[x]), GFP_KERNEL))) {
			while (x) {
				kfree(wc->chans[--x]);
			}

			kfree(wc);
			return -ENOMEM;
		}
		memset(wc->chans[x], 0, sizeof(*wc->chans[x]));
	}

	/* Misc. software stuff */
	t1xxp_software_init(wc);
	
	printk(KERN_INFO "Found a Wildcard: %s\n", wc->variety);

	return 0;
}


static void t1xxp_stop_stuff(struct t1xxp *wc)
{
	/* Kill clock */
	control_set_reg(wc, WC_CLOCK, 0);

	/* Turn off LED's */
	control_set_reg(wc, WC_LEDTEST, 0);

	/* Reset the T1 */
	t1xxp_framer_hard_reset(wc);

}

static void __devexit t1xxp_remove_one(struct pci_dev *pdev)
{
	struct t1xxp *wc = pci_get_drvdata(pdev);
	if (wc) {

		/* Stop any DMA */
		__t1xxp_stop_dma(wc);

		/* In case hardware is still there */
		__t1xxp_disable_interrupts(wc);
		
		t1xxp_stop_stuff(wc);

		/* Immediately free resources */
		pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 32 * 4, (void *)wc->writechunk, wc->writedma);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		outb(DELAY | 0x0e, wc->ioaddr + WC_CNTL);

		/* Release span, possibly delayed */
		if (!wc->usecount)
			t1xxp_release(wc);
		else
			wc->dead = 1;
	}
}

static DEFINE_PCI_DEVICE_TABLE(t1xxp_pci_tbl) = {
	{ 0xe159, 0x0001, 0x6159, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard T100P T1/PRI or E100P E1/PRA Board" },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci,t1xxp_pci_tbl);

static struct pci_driver t1xxp_driver = {
	.name =	"t1xxp",
	.probe = t1xxp_init_one,
	.remove = __devexit_p(t1xxp_remove_one),
	.suspend = NULL,
	.resume = NULL,
	. id_table = t1xxp_pci_tbl,
};

static int __init t1xxp_init(void)
{
	int res;
	res = dahdi_pci_module(&t1xxp_driver);
	if (res)
		return -ENODEV;
	return 0;
}

static void __exit t1xxp_cleanup(void)
{
	pci_unregister_driver(&t1xxp_driver);
}

module_param(debug, int, 0600);

MODULE_DESCRIPTION("Wildcard T100P/E100P Driver");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(t1xxp_init);
module_exit(t1xxp_cleanup);
