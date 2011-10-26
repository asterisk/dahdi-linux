/*
 * Digium, Inc.  Wildcard TE110P T1/PRI card Driver
 *
 * Written by Mark Spencer <markster@digium.com>
 *            Matthew Fredrickson <creslin@digium.com>
 *            William Meadows <wmeadows@digium.com>
 *
 * Copyright (C) 2004, Digium, Inc.
 *
 * All rights reserved.
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

/* XXX: fix this */
#include "wct4xxp/wct4xxp.h"	/* For certain definitions */

#define WC_MAX_CARDS	32

/*
#define TEST_REGS
*/

/* Define to get more attention-grabbing but slightly more I/O using
   alarm status */
#define FANCY_ALARM

/* Define to enable the V2.1 errata register settings */
#if 0
#define TRUST_INFINEON_ERRATA
#endif

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

#define BIT_LED1	(1 << 0)
#define BIT_LED0	(1 << 1)
#define BIT_TEST	(1 << 2)

#define FLAG_STARTED 		(1 << 0)
#define FLAG_NMF 			(1 << 1)
#define FLAG_SENDINGYELLOW 	(1 << 2)
#define FLAG_FALC12			(1 << 3)

#define	TYPE_T1	1		/* is a T1 card */
#define	TYPE_E1	2		/* is an E1 card */

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

static int chanmap_e1uc[] =
{ 3,2,1,0,
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

struct t1 {
	struct pci_dev *dev;
	spinlock_t lock;
	int spantype;
	int spanflags;		/* Span flags */
	unsigned char txsigs[16];  /* Copy of tx sig registers */
	int num;
	int alarmcount;			/* How much red alarm we've seen */
	int alarmdebounce;
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
	int checktiming;	/* Set >0 to cause the timing source to be checked */
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
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile unsigned char *writechunk;					/* Double-word aligned write memory */
	volatile unsigned char *readchunk;					/* Double-word aligned read memory */
	unsigned char ec_chunk1[32][DAHDI_CHUNKSIZE];
	unsigned char ec_chunk2[32][DAHDI_CHUNKSIZE];
	unsigned char tempo[33];
	struct dahdi_device *ddev;
	struct dahdi_span span;						/* Span */
	struct dahdi_chan *chans[32];					/* Channels */
};

#define CANARY 0xca1e

static int debug = 0;   /* doesnt do anything */
static int j1mode = 0;
static int alarmdebounce = 0;
static int loopback = 0;
static int clockextra = 0;
static int t1e1override = -1;
static int unchannelized = 0;

static struct t1 *cards[WC_MAX_CARDS];

static inline void start_alarm(struct t1 *wc)
{
#ifdef FANCY_ALARM
	wc->alarmpos = 0;
#endif
	wc->blinktimer = 0;
}

static inline void stop_alarm(struct t1 *wc)
{
#ifdef FANCY_ALARM
	wc->alarmpos = 0;
#endif
	wc->blinktimer = 0;
}

static inline void __select_framer(struct t1 *wc, int reg)
{
	/* Top four bits of address from AUX 6-3 */
	wc->outbyte &= ~BIT_CS;
	wc->outbyte &= ~BIT_ADDR;
	wc->outbyte |= (reg & 0xf0) >> 1;
	outb(wc->outbyte, wc->ioaddr + WC_AUXD);
}

static inline void __select_control(struct t1 *wc)
{
	if (!(wc->outbyte & BIT_CS)) {
		wc->outbyte |= BIT_CS;
		outb(wc->outbyte, wc->ioaddr + WC_AUXD);
	}
}

static int t1xxp_open(struct dahdi_chan *chan)
{
	struct t1 *wc = chan->pvt;
	if (wc->dead)
		return -ENODEV;
	wc->usecount++;

	return 0;
}

static int __control_set_reg(struct t1 *wc, int reg, unsigned char val)
{
	__select_control(wc);
	outb(val, wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return 0;
}

static int control_set_reg(struct t1 *wc, int reg, unsigned char val)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&wc->lock, flags);
	res = __control_set_reg(wc, reg, val);
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

static int __control_get_reg(struct t1 *wc, int reg)
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

static int control_get_reg(struct t1 *wc, int reg)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&wc->lock, flags);
	res = __control_get_reg(wc, reg);
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

static inline unsigned int __t1_framer_in(struct t1 *wc, const unsigned int reg)
{
	unsigned char res;
	__select_framer(wc, reg);
	/* Get value */
	res = inb(wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
	return res;
#if 0
	unsigned int ret;
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | ( 1 << 10) | WC_LREAD);
	ret = __t1_pci_in(wc, WC_LDATA);
	__t1_pci_out(wc, WC_LADDR, 0);
	return ret & 0xff;
#endif	
}

static inline unsigned int t1_framer_in(struct t1 *wc, const unsigned int addr)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&wc->lock, flags);
	ret = __t1_framer_in(wc, addr);
	spin_unlock_irqrestore(&wc->lock, flags);
	return ret;

}

static inline void __t1_framer_out(struct t1 *wc, const unsigned int reg, const unsigned int val)
{
	if (debug > 1)
		printk(KERN_DEBUG "Writing %02x to address %02x\n", val, reg);
	__select_framer(wc, reg);
	/* Send address */
	outb(val, wc->ioaddr + WC_USERREG + ((reg & 0xf) << 2));
#if 0
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));
	__t1_pci_out(wc, WC_LDATA, value);
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | (1 << 10));
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | (1 << 10) | WC_LWRITE);
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | (1 << 10));
	__t1_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));	
	__t1_pci_out(wc, WC_LADDR, 0);
	if (debug) printk(KERN_DEBUG "Write complete\n");
#endif	
#if 0
	{ unsigned int tmp;
	tmp = t1_framer_in(wc, unit, addr);
	if (tmp != value) {
		printk(KERN_DEBUG "Expected %d from unit %d register %d but got %d instead\n", value, unit, addr, tmp);
	} }
#endif	
}

static inline void t1_framer_out(struct t1 *wc, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->lock, flags);
	__t1_framer_out(wc, addr, value);
	spin_unlock_irqrestore(&wc->lock, flags);
}

static void t1xxp_release(struct t1 *wc)
{
	unsigned int x;

	dahdi_unregister_device(wc->ddev);
	for (x = 0; x < (wc->spantype == TYPE_E1 ? 31 : 24); x++) {
		kfree(wc->chans[x]);
	}
	kfree(wc->ddev->location);
	dahdi_free_device(wc->ddev);
	kfree(wc);
	printk(KERN_INFO "Freed a Wildcard\n");
}

static int t1xxp_close(struct dahdi_chan *chan)
{
	struct t1 *wc = chan->pvt;

	wc->usecount--;
	/* If we're dead, release us now */
	if (!wc->usecount && wc->dead) 
		t1xxp_release(wc);
	return 0;
}

static void t1xxp_enable_interrupts(struct t1 *wc)
{
	/* Clear interrupts */
	outb(0xff, wc->ioaddr + WC_INTSTAT);
	/* Enable interrupts (we care about all of them) */
	outb(0x3c /* 0x3f */, wc->ioaddr + WC_MASK0); 
	/* No external interrupts */
	outb(0x00, wc->ioaddr + WC_MASK1);
	if (debug) printk(KERN_DEBUG "Enabled interrupts!\n");
}

static void t1xxp_start_dma(struct t1 *wc)
{
	/* Reset Master and TDM */
	outb(DELAY | 0x0f, wc->ioaddr + WC_CNTL);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	outb(DELAY | 0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
	if (debug) printk(KERN_DEBUG "Started DMA\n");
	outb(0x03, wc->ioaddr + WC_OPER);
	outb(0x01, wc->ioaddr + WC_OPER);
}

static void __t1xxp_stop_dma(struct t1 *wc)
{
	outb(0x00, wc->ioaddr + WC_OPER);
}

static void __t1xxp_disable_interrupts(struct t1 *wc)	
{
	outb(0x00, wc->ioaddr + WC_MASK0);
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void __t1xxp_set_clear(struct t1 *wc)
{
	int i,j;
	unsigned short val=0;
	for (i=0;i<24;i++) {
		j = (i/8);
		if (wc->span.chans[i]->flags & DAHDI_FLAG_CLEAR) 
			val |= 1 << (7 - (i % 8));
		if ((i % 8)==7) {
			if (debug > 1)
				printk(KERN_DEBUG "Putting %d in register %02x\n",
			       val, 0x2f + j);
			__t1_framer_out(wc, 0x2f + j, val);
			val = 0;
		}
	}
}

static int t1xxp_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct t4_regs regs;
	int x;
	struct t1 *wc;
	switch(cmd) {
	case WCT4_GET_REGS:
		wc = chan->pvt;
		for (x=0;x<NUM_PCI;x++)
#if 1
			regs.pci[x] = (inb(wc->ioaddr + (x << 2))) |
				       (inb(wc->ioaddr + (x << 2) + 1) << 8) |
					(inb(wc->ioaddr + (x << 2) + 2) << 16) |
					 (inb(wc->ioaddr + (x << 2) + 3) << 24);
#else
			regs.pci[x] = (inb(wc->ioaddr + x));
#endif

		for (x=0;x<NUM_REGS;x++)
			regs.regs[x] = t1_framer_in(wc, x);
		if (copy_to_user((__user void *)data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static inline struct t1 *t1_from_span(struct dahdi_span *span)
{
	return container_of(span, struct t1, span);
}

static int t1xxp_maint(struct dahdi_span *span, int cmd)
{
	struct t1 *wc = t1_from_span(span);

	if (wc->spantype == TYPE_E1) {
		switch(cmd) {
		case DAHDI_MAINT_NONE:
			printk(KERN_INFO "XXX Turn off local and remote loops E1 XXX\n");
			break;
		case DAHDI_MAINT_LOCALLOOP:
			printk(KERN_INFO "XXX Turn on local loopback E1 XXX\n");
			break;
		case DAHDI_MAINT_REMOTELOOP:
			printk(KERN_INFO "XXX Turn on remote loopback E1 XXX\n");
			break;
		case DAHDI_MAINT_LOOPUP:
			printk(KERN_INFO "XXX Send loopup code E1 XXX\n");
			break;
		case DAHDI_MAINT_LOOPDOWN:
			printk(KERN_INFO "XXX Send loopdown code E1 XXX\n");
			break;
		default:
			printk(KERN_NOTICE "TE110P: Unknown E1 maint command: %d\n", cmd);
			break;
		}
	} else {
		switch(cmd) {
	    case DAHDI_MAINT_NONE:
			printk(KERN_INFO "XXX Turn off local and remote loops T1 XXX\n");
			t1_framer_out(wc, 0x21, 0x40);
			break;
	    case DAHDI_MAINT_LOCALLOOP:
			printk(KERN_INFO "XXX Turn on local loop and no remote loop XXX\n");
			break;
	    case DAHDI_MAINT_REMOTELOOP:
			printk(KERN_INFO "XXX Turn on remote loopup XXX\n");
			break;
	    case DAHDI_MAINT_LOOPUP:
			t1_framer_out(wc, 0x21, 0x50);	/* FMR5: Nothing but RBS mode */
			break;
	    case DAHDI_MAINT_LOOPDOWN:
			t1_framer_out(wc, 0x21, 0x60);	/* FMR5: Nothing but RBS mode */
			break;
	    default:
			printk(KERN_NOTICE "TE110P: Unknown T1 maint command: %d\n", cmd);
			break;
	   }
    }
	return 0;
}

static int t1xxp_rbsbits(struct dahdi_chan *chan, int bits)
{
	u_char m,c;
	int n,b;
	struct t1 *wc = chan->pvt;
	unsigned long flags;
	
	if(debug > 1) printk(KERN_DEBUG "Setting bits to %d on channel %s\n", bits, chan->name);
	spin_lock_irqsave(&wc->lock, flags);	
	if (wc->spantype == TYPE_E1) { /* do it E1 way */
		if (chan->chanpos == 16) {
			spin_unlock_irqrestore(&wc->lock, flags);
			return 0;
		}
		n = chan->chanpos - 1;
		if (chan->chanpos > 15) n--;
		b = (n % 15);
		c = wc->txsigs[b];
		m = (n / 15) << 2; /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		wc->txsigs[b] = c;
		  /* output them to the chip */
		__t1_framer_out(wc,0x71 + b,c); 
	} else if (wc->span.lineconfig & DAHDI_CONFIG_D4) {
		n = chan->chanpos - 1;
		b = (n/4);
		c = wc->txsigs[b];
		m = ((3 - (n % 4)) << 1); /* nibble selector */
		c &= ~(0x3 << m); /* keep the other nibble */
		c |= ((bits >> 2) & 0x3) << m; /* put our new nibble here */
		wc->txsigs[b] = c;
		  /* output them to the chip */
		__t1_framer_out(wc,0x70 + b,c); 
		__t1_framer_out(wc,0x70 + b + 6,c); 
	} else if (wc->span.lineconfig & DAHDI_CONFIG_ESF) {
		n = chan->chanpos - 1;
		b = (n/2);
		c = wc->txsigs[b];
		m = ((n % 2) << 2); /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		wc->txsigs[b] = c;
		  /* output them to the chip */
		__t1_framer_out(wc,0x70 + b,c); 
	} 
	spin_unlock_irqrestore(&wc->lock, flags);
	if (debug > 1)
		printk(KERN_DEBUG "Finished setting RBS bits\n");
	return 0;
}

static void t1_check_sigbits(struct t1 *wc)
{
	int a,i,rxs;
	unsigned long flags;

	if (!(wc->span.flags & DAHDI_FLAG_RUNNING))
		return;

	spin_lock_irqsave(&wc->lock, flags);

	if (wc->spantype == TYPE_E1) {
		for (i = 0; i < 15; i++) {
			a = __t1_framer_in(wc, 0x71 + i);
			/* Get high channel in low bits */
			rxs = (a & 0xf);
			if (!(wc->span.chans[i+16]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i+16]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i+16], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a >> 4) & 0xf;
			if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
		}
	} else if (wc->span.lineconfig & DAHDI_CONFIG_D4) {
		for (i = 0; i < 24; i+=4) {
			a = __t1_framer_in(wc, 0x70 + (i>>2));
			/* Get high channel in low bits */
			rxs = (a & 0x3) << 2;
			if (!(wc->span.chans[i+3]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i+3]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i+3], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a & 0xc);
			if (!(wc->span.chans[i+2]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i+2]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i+2], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a >> 2) & 0xc;
			if (!(wc->span.chans[i+1]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i+1]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i+1], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a >> 4) & 0xc;
			if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
		}
	} else {
		for (i = 0; i < 24; i+=2) {
			a = __t1_framer_in(wc, 0x70 + (i>>1));
			/* Get high channel in low bits */
			rxs = (a & 0xf);
			if (!(wc->span.chans[i+1]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i+1]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i+1], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
			rxs = (a >> 4) & 0xf;
			if (!(wc->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (wc->span.chans[i]->rxsig != rxs) {
					spin_unlock_irqrestore(&wc->lock, flags);
					dahdi_rbsbits(wc->span.chans[i], rxs);
					spin_lock_irqsave(&wc->lock, flags);
				}
			}
		}
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

static void t4_serial_setup(struct t1 *wc)
{
	printk(KERN_INFO "TE110P: Setting up global serial parameters for %s %s\n", 
	       wc->spantype == TYPE_E1 ? (unchannelized ? "Unchannelized E1" : "E1") : "T1", 
		   wc->spanflags & FLAG_FALC12 ? "FALC V1.2" : "FALC V2.2");
	t1_framer_out(wc, 0x85, 0xe0);	/* GPC1: Multiplex mode enabled, FSC is output, active low, RCLK from channel 0 */
	t1_framer_out(wc, 0x08, 0x05);	/* IPC: Interrupt push/pull active low */
	if (wc->spanflags & FLAG_FALC12) {
		t1_framer_out(wc, 0x92, 0x00);	
		t1_framer_out(wc, 0x93, 0x58);
		t1_framer_out(wc, 0x94, 0xd2);
		t1_framer_out(wc, 0x95, 0xc2);
		t1_framer_out(wc, 0x96, 0x03);
		t1_framer_out(wc, 0x97, 0x10);
	} else {
		/* Global clocks (8.192 Mhz CLK) */
		t1_framer_out(wc, 0x92, 0x00);	
		t1_framer_out(wc, 0x93, 0x18);
		t1_framer_out(wc, 0x94, 0xfb);
		t1_framer_out(wc, 0x95, 0x0b);
		t1_framer_out(wc, 0x96, 0x00);
		t1_framer_out(wc, 0x97, 0x0b);
		t1_framer_out(wc, 0x98, 0xdb);
		t1_framer_out(wc, 0x99, 0xdf);
	}
	/* Configure interrupts */	
	t1_framer_out(wc, 0x46, 0x40);	/* GCR: Interrupt on Activation/Deactivation of AIX, LOS */

	/* Configure system interface */
	t1_framer_out(wc, 0x3e, 0x02);	/* SIC1: 4.096 Mhz clock/bus, double buffer receive / transmit, byte interleaved */
	t1_framer_out(wc, 0x3f, 0x00); 	/* SIC2: No FFS, no center receive eliastic buffer, phase 0 */
	t1_framer_out(wc, 0x40, 0x04);	/* SIC3: Edges for capture */
	t1_framer_out(wc, 0x44, 0x30);	/* CMR1: RCLK is at 8.192 Mhz dejittered */
	t1_framer_out(wc, 0x45, 0x00);	/* CMR2: We provide sync and clock for tx and rx. */
	t1_framer_out(wc, 0x22, 0x00);	/* XC0: Normal operation of Sa-bits */
	t1_framer_out(wc, 0x23, 0x04);	/* XC1: 0 offset */
	t1_framer_out(wc, 0x24, 0x07);	/* RC0: Just shy of 255 */
	if (wc->spanflags & FLAG_FALC12)
		t1_framer_out(wc, 0x25, 0x04);	/* RC1: The rest of RC0 */
	else
		t1_framer_out(wc, 0x25, 0x05);	/* RC1: The rest of RC0 */
	
	/* Configure ports */
	t1_framer_out(wc, 0x80, 0x00);	/* PC1: SPYR/SPYX input on RPA/XPA */
	t1_framer_out(wc, 0x81, 0x22);	/* PC2: RMFB/XSIG output/input on RPB/XPB */
	t1_framer_out(wc, 0x82, 0x65);	/* PC3: Some unused stuff */
	t1_framer_out(wc, 0x83, 0x35);	/* PC4: Some more unused stuff */
	t1_framer_out(wc, 0x84, 0x31);	/* PC5: XMFS active low, SCLKR is input, RCLK is output */
	t1_framer_out(wc, 0x86, 0x03);	/* PC6: CLK1 is Tx Clock output, CLK2 is 8.192 Mhz from DCO-R */
	t1_framer_out(wc, 0x3b, 0x00);	/* Clear LCR1 */
	printk(KERN_INFO "TE110P: Successfully initialized serial bus for card\n");
}

static void __t1_configure_t1(struct t1 *wc, int lineconfig, int txlevel)
{
	unsigned int fmr4, fmr2, fmr1, fmr0, lim2;
	char *framing, *line;
	int mytxlevel;
	if ((txlevel > 7) || (txlevel < 4))
		mytxlevel = 0;
	else
		mytxlevel = txlevel - 4;
	fmr1 = 0x1c; /* FMR1: Mode 0, T1 mode, CRC on for ESF, 2.048 Mhz system data rate, no XAIS */
	fmr2 = 0x22; /* FMR2: no payload loopback, auto send yellow alarm */
	if (loopback)
		fmr2 |= 0x4;

	if (j1mode)
		fmr4 = 0x1c;
	else
		fmr4 = 0x0c; /* FMR4: Lose sync on 2 out of 5 framing bits, auto resync */


	lim2 = 0x21; /* LIM2: 50% peak is a "1", Advanced Loss recovery */
	lim2 |= (mytxlevel << 6);	/* LIM2: Add line buildout */
	__t1_framer_out(wc, 0x1d, fmr1);
	__t1_framer_out(wc, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		fmr0 = 0xa0;
	} else {
		line = "B8ZS";
		fmr0 = 0xf0;
	}
	if (lineconfig & DAHDI_CONFIG_D4) {
		framing = "D4";
	} else {
		framing = "ESF";
		fmr4 |= 0x2;
		fmr2 |= 0xc0;
	}
	__t1_framer_out(wc, 0x1c, fmr0);

	__t1_framer_out(wc, 0x20, fmr4);
	__t1_framer_out(wc, 0x21, 0x40);	/* FMR5: Enable RBS mode */

	__t1_framer_out(wc, 0x37, 0xf8);	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No remote loop, no DRS */
	__t1_framer_out(wc, 0x36, 0x08);	/* LIM0: Enable auto long haul mode, no local loop (must be after LIM1) */

	__t1_framer_out(wc, 0x02, 0x50);	/* CMDR: Reset the receiver and transmitter line interface */
	__t1_framer_out(wc, 0x02, 0x00);	/* CMDR: Reset the receiver and transmitter line interface */

	__t1_framer_out(wc, 0x3a, lim2);	/* LIM2: 50% peak amplitude is a "1" */
	__t1_framer_out(wc, 0x38, 0x0a);	/* PCD: LOS after 176 consecutive "zeros" */
	__t1_framer_out(wc, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */

	if (j1mode)
		__t1_framer_out(wc, 0x24, 0x80); /* J1 overide */
		
	/* Generate pulse mask for T1 */
	switch(mytxlevel) {
	case 3:
		__t1_framer_out(wc, 0x26, 0x07);	/* XPM0 */
		__t1_framer_out(wc, 0x27, 0x01);	/* XPM1 */
		__t1_framer_out(wc, 0x28, 0x00);	/* XPM2 */
		break;
	case 2:
		__t1_framer_out(wc, 0x26, 0x8c);	/* XPM0 */
		__t1_framer_out(wc, 0x27, 0x11);	/* XPM1 */
		__t1_framer_out(wc, 0x28, 0x01);	/* XPM2 */
		break;
	case 1:
		__t1_framer_out(wc, 0x26, 0x8c);	/* XPM0 */
		__t1_framer_out(wc, 0x27, 0x01);	/* XPM1 */
		__t1_framer_out(wc, 0x28, 0x00);	/* XPM2 */
		break;
	case 0:
	default:
		__t1_framer_out(wc, 0x26, 0xd7);	/* XPM0 */
		__t1_framer_out(wc, 0x27, 0x22);	/* XPM1 */
		__t1_framer_out(wc, 0x28, 0x01);	/* XPM2 */
		break;
	}
	printk(KERN_INFO "TE110P: Span configured for %s/%s\n", framing, line);
}

static void __t1_configure_e1(struct t1 *wc, int lineconfig)
{
	unsigned int fmr2, fmr1, fmr0;
	unsigned int cas = 0;
	char *crc4 = "";
	char *framing, *line;
	fmr1 = 0x44; /* FMR1: E1 mode, Automatic force resync, PCM30 mode, 8.192 Mhz backplane, no XAIS */
	fmr2 = 0x03; /* FMR2: Auto transmit remote alarm, auto loss of multiframe recovery, no payload loopback */
	if (unchannelized)
		fmr2 |= 0x30;
	if (loopback)
		fmr2 |= 0x4;
	if (lineconfig & DAHDI_CONFIG_CRC4) {
		fmr1 |= 0x08;	/* CRC4 transmit */
		fmr2 |= 0xc0;	/* CRC4 receive */
		crc4 = "/CRC4";
	}
	__t1_framer_out(wc, 0x1d, fmr1);
	__t1_framer_out(wc, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		fmr0 = 0xa0;
	} else {
		line = "HDB3";
		fmr0 = 0xf0;
	}
	if (lineconfig & DAHDI_CONFIG_CCS) {
		framing = "CCS";
	} else {
		framing = "CAS";
		cas = 0x40;
	}
	__t1_framer_out(wc, 0x1c, fmr0);

	if (unchannelized)
		__t1_framer_out(wc, 0x1f, 0x40);

	__t1_framer_out(wc, 0x37, 0xf0 /*| 0x6 */ );	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No remote loop, no DRS */
	__t1_framer_out(wc, 0x36, 0x08);	/* LIM0: Enable auto long haul mode, no local loop (must be after LIM1) */

	__t1_framer_out(wc, 0x02, 0x50);	/* CMDR: Reset the receiver and transmitter line interface */
	__t1_framer_out(wc, 0x02, 0x00);	/* CMDR: Reset the receiver and transmitter line interface */

	/* Condition receive line interface for E1 after reset */
	__t1_framer_out(wc, 0xbb, 0x17);
	__t1_framer_out(wc, 0xbc, 0x55);
	__t1_framer_out(wc, 0xbb, 0x97);
	__t1_framer_out(wc, 0xbb, 0x11);
	__t1_framer_out(wc, 0xbc, 0xaa);
	__t1_framer_out(wc, 0xbb, 0x91);
	__t1_framer_out(wc, 0xbb, 0x12);
	__t1_framer_out(wc, 0xbc, 0x55);
	__t1_framer_out(wc, 0xbb, 0x92);
	__t1_framer_out(wc, 0xbb, 0x0c);
	__t1_framer_out(wc, 0xbb, 0x00);
	__t1_framer_out(wc, 0xbb, 0x8c);
	
	__t1_framer_out(wc, 0x3a, 0x20);	/* LIM2: 50% peak amplitude is a "1" */
	__t1_framer_out(wc, 0x38, 0x0a);	/* PCD: LOS after 176 consecutive "zeros" */
	__t1_framer_out(wc, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */
	
	__t1_framer_out(wc, 0x20, 0x9f);	/* XSW: Spare bits all to 1 */
	if (unchannelized)
		__t1_framer_out(wc, 0x21, 0x3c);
	else
		__t1_framer_out(wc, 0x21, 0x1c|cas);	/* XSP: E-bit set when async. AXS auto, XSIF to 1 */
	
	
	/* Generate pulse mask for E1 */
	__t1_framer_out(wc, 0x26, 0x54);	/* XPM0 */
	__t1_framer_out(wc, 0x27, 0x02);	/* XPM1 */
	__t1_framer_out(wc, 0x28, 0x00);	/* XPM2 */
	printk(KERN_INFO "TE110P: Span configured for %s/%s%s\n", framing, line, crc4);
}

static void t1xxp_framer_start(struct t1 *wc, struct dahdi_span *span)
{
	int alreadyrunning = wc->span.flags & DAHDI_FLAG_RUNNING;
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);

	if (wc->spantype == TYPE_E1) { /* if this is an E1 card */
		__t1_configure_e1(wc, span->lineconfig);
	} else { /* is a T1 card */
		__t1_configure_t1(wc, span->lineconfig, span->txlevel);
		__t1xxp_set_clear(wc);
	}
	
	if (!alreadyrunning) 
		wc->span.flags |= DAHDI_FLAG_RUNNING;

	spin_unlock_irqrestore(&wc->lock, flags);
}


static int t1xxp_startup(struct file *file, struct dahdi_span *span)
{
	struct t1 *wc = t1_from_span(span);

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
	t1xxp_framer_start(wc, span);
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
	struct t1 *wc = t1_from_span(span);
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	__t1xxp_stop_dma(wc);
	__t1xxp_disable_interrupts(wc);
	span->flags &= ~DAHDI_FLAG_RUNNING;
	spin_unlock_irqrestore(&wc->lock, flags);
	return 0;
}


static int
t1xxp_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	struct t1 *wc = chan->pvt;
	unsigned long flags;
	int alreadyrunning = chan->span->flags & DAHDI_FLAG_RUNNING;

	spin_lock_irqsave(&wc->lock, flags);

	if (alreadyrunning && (wc->spantype != TYPE_E1))
		__t1xxp_set_clear(wc);

	spin_unlock_irqrestore(&wc->lock, flags);
	return 0;
}

static int
t1xxp_spanconfig(struct file *file, struct dahdi_span *span,
		 struct dahdi_lineconfig *lc)
{
	struct t1 *wc = t1_from_span(span);

	/* Do we want to SYNC on receive or not */
	wc->sync = (lc->sync) ? 1 : 0;
	/* If already running, apply changes immediately */
	if (span->flags & DAHDI_FLAG_RUNNING)
		return t1xxp_startup(file, span);

	return 0;
}

static const struct dahdi_span_ops t1xxp_span_ops = {
	.owner = THIS_MODULE,
	.startup = t1xxp_startup,
	.shutdown = t1xxp_shutdown,
	.rbsbits = t1xxp_rbsbits,
	.maint = t1xxp_maint,
	.open = t1xxp_open,
	.close = t1xxp_close,
	.spanconfig = t1xxp_spanconfig,
	.chanconfig = t1xxp_chanconfig,
	.ioctl = t1xxp_ioctl,
};

static int t1xxp_software_init(struct t1 *wc)
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

	t4_serial_setup(wc);
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

	if (wc->spantype == TYPE_E1) {
		if (unchannelized)
			wc->span.channels = 32;
		else
			wc->span.channels = 31;
		wc->span.deflaw = DAHDI_LAW_ALAW;
		wc->span.spantype = "E1";
		wc->span.linecompat = DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
	} else {
		wc->span.channels = 24;
		wc->span.deflaw = DAHDI_LAW_MULAW;
		wc->span.spantype = "T1";
		wc->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS | DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
	}
	wc->span.chans = wc->chans;
	wc->span.flags = DAHDI_FLAG_RBS;
	for (x=0;x<wc->span.channels;x++) {
		sprintf(wc->chans[x]->name, "WCT1/%d/%d", wc->num, x + 1);
		wc->chans[x]->sigcap = DAHDI_SIG_EM | DAHDI_SIG_CLEAR | DAHDI_SIG_EM_E1 | 
				      DAHDI_SIG_FXSLS | DAHDI_SIG_FXSGS | DAHDI_SIG_MTP2 |
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

static inline void __handle_leds(struct t1 *wc)
{
	int oldreg;

	if (wc->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE)) {
		/* Red/Blue alarm */
		wc->blinktimer++;
#ifdef FANCY_ALARM
		if (wc->blinktimer == (altab[wc->alarmpos] >> 1)) {
			wc->ledtestreg = (wc->ledtestreg | BIT_LED1) & ~BIT_LED0;
			__control_set_reg(wc, WC_LEDTEST, wc->ledtestreg);
		}
		if (wc->blinktimer >= 0xf) {
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

static void t1xxp_transmitprep(struct t1 *wc, int ints)
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

static void t1xxp_receiveprep(struct t1 *wc, int ints)
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
		if (wc->spantype != TYPE_E1) {
			for (x=3;x<32;x+=4) {
				if (rxbuf[32 * y + ((x + WC_OFFSET) & 0x1f)] == 0x7f) {
					if (wc->offset != (x-3)) {
						/* Resync */
						control_set_reg(wc, WC_CLOCK, 0x06 | wc->sync | clockextra);
						wc->clocktimeout = 100;
#if 1
						if (debug) printk(KERN_DEBUG "T1: Lost our place, resyncing\n");
#endif
					}
				}
			}
		} else if (!unchannelized) {
			if (!wc->clocktimeout && !wc->span.alarms) {
				if ((rxbuf[32 * y + ((3 + WC_OFFSET + wc->offset) & 0x1f)] & 0x7f) != 0x1b) {
					if (wc->miss) {
						if (debug) printk(KERN_DEBUG "Double miss (%d, %d)...\n", wc->misslast, rxbuf[32 * y + ((3 + WC_OFFSET + wc->offset) & 0x1f)]);
						control_set_reg(wc, WC_CLOCK, 0x06 | wc->sync | clockextra);
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

static void t1_check_alarms(struct t1 *wc)
{
	unsigned char c,d;
	int alarms;
	int x,j;
	unsigned long flags;

	if (!(wc->span.flags & DAHDI_FLAG_RUNNING))
		return;

	spin_lock_irqsave(&wc->lock, flags);

	c = __t1_framer_in(wc, 0x4c);
	if (wc->spanflags & FLAG_FALC12)
		d = __t1_framer_in(wc, 0x4f);
	else
		d = __t1_framer_in(wc, 0x4d);

	/* Assume no alarms */
	alarms = 0;

	/* And consider only carrier alarms */
	wc->span.alarms &= (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_NOTOPEN);

	if (wc->spantype == TYPE_E1) {
		if (c & 0x04) {
			/* No multiframe found, force RAI high after 400ms only if
			   we haven't found a multiframe since last loss
			   of frame */
			if (!(wc->spanflags & FLAG_NMF)) {
				__t1_framer_out(wc, 0x20, 0x9f | 0x20);	/* LIM0: Force RAI High */
				wc->spanflags |= FLAG_NMF;
				printk(KERN_DEBUG "NMF workaround on!\n");
			}
			__t1_framer_out(wc, 0x1e, 0xc3);	/* Reset to CRC4 mode */
			__t1_framer_out(wc, 0x1c, 0xf2);	/* Force Resync */
			__t1_framer_out(wc, 0x1c, 0xf0);	/* Force Resync */
		} else if (!(c & 0x02)) {
			if ((wc->spanflags & FLAG_NMF)) {
				__t1_framer_out(wc, 0x20, 0x9f);	/* LIM0: Clear forced RAI */
				wc->spanflags &= ~FLAG_NMF;
				printk(KERN_DEBUG "NMF workaround off!\n");
			}
		}
	} else {
		/* Detect loopup code if we're not sending one */
		if ((!wc->span.mainttimer) && (d & 0x08)) {
			/* Loop-up code detected */
			if ((wc->loopupcnt++ > 80)  && (wc->span.maintstat != DAHDI_MAINT_REMOTELOOP)) {
				__t1_framer_out(wc, 0x36, 0x08);	/* LIM0: Disable any local loop */
				__t1_framer_out(wc, 0x37, 0xf6 );	/* LIM1: Enable remote loop */
				wc->span.maintstat = DAHDI_MAINT_REMOTELOOP;
			}
		} else
			wc->loopupcnt = 0;
		/* Same for loopdown code */
		if ((!wc->span.mainttimer) && (d & 0x10)) {
			/* Loop-down code detected */
			if ((wc->loopdowncnt++ > 80)  && (wc->span.maintstat == DAHDI_MAINT_REMOTELOOP)) {
				__t1_framer_out(wc, 0x36, 0x08);	/* LIM0: Disable any local loop */
				__t1_framer_out(wc, 0x37, 0xf0 );	/* LIM1: Disable remote loop */
				wc->span.maintstat = DAHDI_MAINT_NONE;
			}
		} else
			wc->loopdowncnt = 0;
	}

	if (wc->span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
		for (x=0,j=0;x < wc->span.channels;x++)
			if ((wc->span.chans[x]->flags & DAHDI_FLAG_OPEN) ||
			    dahdi_have_netdev(wc->span.chans[x]))
				j++;
		if (!j)
			alarms |= DAHDI_ALARM_NOTOPEN;
	}

	if (c & 0xa0) {
		if (wc->alarmcount >= alarmdebounce) {
			if (!unchannelized)
				alarms |= DAHDI_ALARM_RED;
		} else
			wc->alarmcount++;
	} else
		wc->alarmcount = 0;
	if (c & 0x4)
		alarms |= DAHDI_ALARM_BLUE;

	if (((!wc->span.alarms) && alarms) || 
	    (wc->span.alarms && (!alarms))) 
		wc->checktiming = 1;

	/* Keep track of recovering */
	if ((!alarms) && wc->span.alarms) 
		wc->alarmtimer = DAHDI_ALARMSETTLE_TIME;
	if (wc->alarmtimer)
		alarms |= DAHDI_ALARM_RECOVER;

	/* If receiving alarms, go into Yellow alarm state */
	if (alarms && !(wc->spanflags & FLAG_SENDINGYELLOW)) {
		unsigned char fmr4;
#if 1
		printk(KERN_INFO "wcte1xxp: Setting yellow alarm\n");
#endif
		/* We manually do yellow alarm to handle RECOVER and NOTOPEN, otherwise it's auto anyway */
		fmr4 = __t1_framer_in(wc, 0x20);
		__t1_framer_out(wc, 0x20, fmr4 | 0x20);
		wc->spanflags |= FLAG_SENDINGYELLOW;
	} else if ((!alarms) && (wc->spanflags & FLAG_SENDINGYELLOW)) {
		unsigned char fmr4;
#if 1
		printk(KERN_INFO "wcte1xxp: Clearing yellow alarm\n");
#endif
		/* We manually do yellow alarm to handle RECOVER  */
		fmr4 = __t1_framer_in(wc, 0x20);
		__t1_framer_out(wc, 0x20, fmr4 & ~0x20);
		wc->spanflags &= ~FLAG_SENDINGYELLOW;
	}

	/* Re-check the timing source when we enter/leave alarm, not withstanding
	   yellow alarm */
	if ((c & 0x10) && !unchannelized)
		alarms |= DAHDI_ALARM_YELLOW;
	if (wc->span.mainttimer || wc->span.maintstat) 
		alarms |= DAHDI_ALARM_LOOPBACK;
	wc->span.alarms = alarms;
	spin_unlock_irqrestore(&wc->lock, flags);
	dahdi_alarm_notify(&wc->span);
}


static void t1_do_counters(struct t1 *wc)
{
	unsigned long flags;

	spin_lock_irqsave(&wc->lock, flags);
	if (wc->alarmtimer) {
		if (!--wc->alarmtimer) {
			wc->span.alarms &= ~(DAHDI_ALARM_RECOVER);
			spin_unlock_irqrestore(&wc->lock, flags);
			dahdi_alarm_notify(&wc->span);
			spin_lock_irqsave(&wc->lock, flags);
		}
	}
	spin_unlock_irqrestore(&wc->lock, flags);
}

DAHDI_IRQ_HANDLER(t1xxp_interrupt)
{
	struct t1 *wc = dev_id;
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
		control_set_reg(wc, WC_CLOCK, 0x04 | wc->sync | clockextra);

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
	t1_do_counters(wc);

	/* Do some things that we don't have to do very often */
	x = wc->intcount & 15 /* 63 */;
	switch(x) {
	case 0:
	case 1:
		break;
	case 2:
		t1_check_sigbits(wc);
		break;
	case 4:
		/* Check alarms 1/4 as frequently */
		if (!(wc->intcount & 0x30))
			t1_check_alarms(wc);
		break;
	}
	
	if (ints & 0x10) 
		printk(KERN_NOTICE "PCI Master abort\n");

	if (ints & 0x20)
		printk(KERN_NOTICE "PCI Target abort\n");

	return IRQ_RETVAL(1);
}

static int t1xxp_hardware_init(struct t1 *wc)
{
	unsigned int falcver;
	unsigned int x;
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

	if (t1e1override > -1) {
		if (t1e1override)
			wc->spantype = TYPE_E1;
		else
			wc->spantype = TYPE_T1;
	} else {
		if (control_get_reg(wc, WC_CLOCK) & 0x20)
			wc->spantype = TYPE_T1;
		else
			wc->spantype = TYPE_E1;
	}

	/* Check out the controller */
	if (debug) printk(KERN_DEBUG "Controller version: %02x\n", control_get_reg(wc, WC_VERSION));


	control_set_reg(wc, WC_LEDTEST, 0x00);

	if (wc->spantype == TYPE_E1) {
		if (unchannelized)
			wc->chanmap = chanmap_e1uc;
		else
			wc->chanmap = chanmap_e1;
	} else
		wc->chanmap = chanmap_t1;
	/* Setup clock appropriately */
	control_set_reg(wc, WC_CLOCK, 0x06 | wc->sync | clockextra);
	wc->clocktimeout = 100;

	/* Perform register test on FALC */	
	for (x=0;x<256;x++) {
		t1_framer_out(wc, 0x14, x);
		if ((falcver = t1_framer_in(wc, 0x14)) != x) 
			printk(KERN_DEBUG "Wrote '%x' but read '%x'\n", x, falcver);
	}
	
	t1_framer_out(wc, 0x4a, 0xaa);
	falcver = t1_framer_in(wc ,0x4a);
	printk(KERN_INFO "FALC version: %08x\n", falcver);
	if (!falcver)
		wc->spanflags |= FLAG_FALC12;
	

	start_alarm(wc);
	return 0;

}

static int __devinit t1xxp_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct t1 *wc;
	unsigned int *canary;
	unsigned int x;
	
	if (pci_enable_device(pdev)) {
		return -EIO;
	}
	if (!(wc = kmalloc(sizeof(*wc), GFP_KERNEL))) {
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
				printk(KERN_NOTICE "wcte11xp: Unable to allocate DMA-able memory\n");
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
	
	if (request_irq(pdev->irq, t1xxp_interrupt, DAHDI_IRQ_SHARED_DISABLED, "wcte11xp", wc)) {
		printk(KERN_NOTICE "wcte11xp: Unable to request IRQ %d\n", pdev->irq);
		kfree(wc);
		return -EIO;
	}
	/* Initialize hardware */
	t1xxp_hardware_init(wc);
	
	/* We now know which version of card we have */
	wc->variety = "Digium Wildcard TE110P T1/E1";
	
	for (x = 0; x < (wc->spantype == TYPE_E1 ? 31 : 24); x++) {
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

static void t1xxp_stop_stuff(struct t1 *wc)
{
	/* Kill clock */
	control_set_reg(wc, WC_CLOCK, 0);

	/* Turn off LED's */
	control_set_reg(wc, WC_LEDTEST, 0);

}

static void __devexit t1xxp_remove_one(struct pci_dev *pdev)
{
	struct t1 *wc = pci_get_drvdata(pdev);
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
	{ 0xe159, 0x0001, 0x71fe, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard TE110P T1/E1 Board" },
	{ 0xe159, 0x0001, 0x79fe, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard TE110P T1/E1 Board" },
	{ 0xe159, 0x0001, 0x795e, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard TE110P T1/E1 Board" },
	{ 0xe159, 0x0001, 0x79de, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard TE110P T1/E1 Board" },
	{ 0xe159, 0x0001, 0x797e, PCI_ANY_ID, 0, 0, (unsigned long) "Digium Wildcard TE110P T1/E1 Board" },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci,t1xxp_pci_tbl);

static struct pci_driver t1xxp_driver = {
	.name =	"wcte11xp",
	.probe = t1xxp_init_one,
	.remove = __devexit_p(t1xxp_remove_one),
	.suspend = NULL,
	.resume = NULL,
	.id_table = t1xxp_pci_tbl,
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

module_param(alarmdebounce, int, 0600);
module_param(loopback, int, 0600);
module_param(t1e1override, int, 0600);
module_param(unchannelized, int, 0600);
module_param(clockextra, int, 0600);
module_param(debug, int, 0600);
module_param(j1mode, int, 0600);

MODULE_DESCRIPTION("Wildcard TE110P Driver");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(t1xxp_init);
module_exit(t1xxp_cleanup);
