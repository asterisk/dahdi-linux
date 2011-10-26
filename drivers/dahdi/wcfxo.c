/*
 * Wildcard X100P FXO Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 *            Matthew Fredrickson <creslin@digium.com>
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
#include <asm/io.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

/* Uncomment to enable tasklet handling in the FXO driver.  Not recommended
   in general, but may improve interactive performance */

/* #define ENABLE_TASKLETS */

/* Un-comment the following for POTS line support for Japan */
/* #define	JAPAN */

/* Un-comment for lines (eg from and ISDN TA) that remove */
/* phone power during ringing                             */
/* #define ZERO_BATT_RING */

#define WC_MAX_IFACES 128

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

#define WC_AUXFUNC	0x2b
#define WC_SERCTL	0x2d
#define WC_FSCDELAY	0x2f


/* DAA registers */
#define WC_DAA_CTL1  		1
#define WC_DAA_CTL2  		2
#define WC_DAA_DCTL1 		5
#define WC_DAA_DCTL2		6
#define WC_DAA_PLL1_N1	 	7
#define WC_DAA_PLL1_M1	 	8
#define WC_DAA_PLL2_N2_M2 	9
#define WC_DAA_PLL_CTL   	10
#define WC_DAA_CHIPA_REV 	11
#define WC_DAA_LINE_STAT 	12
#define WC_DAA_CHIPB_REV 	13
#define WC_DAA_DAISY_CTL	14
#define WC_DAA_TXRX_GCTL	15
#define WC_DAA_INT_CTL1 	16
#define WC_DAA_INT_CTL2 	17
#define WC_DAA_INT_CTL3 	18
#define WC_DAA_INT_CTL4 	19


#define FLAG_EMPTY	0
#define FLAG_WRITE	1
#define FLAG_READ	2

#ifdef 	ZERO_BATT_RING			/* Need to debounce Off/On hook too */
#define	JAPAN
#endif

#define RING_DEBOUNCE	64		/* Ringer Debounce (in ms) */
#ifdef	JAPAN
#define BATT_DEBOUNCE	30		/* Battery debounce (in ms) */
#define OH_DEBOUNCE	350		/* Off/On hook debounce (in ms) */
#else
#define BATT_DEBOUNCE	80		/* Battery debounce (in ms) */
#endif

#define MINPEGTIME	10 * 8		/* 30 ms peak to peak gets us no more than 100 Hz */
#define PEGTIME		50 * 8		/* 50ms peak to peak gets us rings of 10 Hz or more */
#define PEGCOUNT	5		/* 5 cycles of pegging means RING */

#define	wcfxo_printk(level, span, fmt, ...)	\
	printk(KERN_ ## level "%s-%s: %s: " fmt, #level,	\
		THIS_MODULE->name, (span).name, ## __VA_ARGS__)

#define wcfxo_notice(span, fmt, ...) \
	wcfxo_printk(NOTICE, span, fmt, ## __VA_ARGS__)

#define wcfxo_dbg(span, fmt, ...) \
	((void)((debug) && wcfxo_printk(DEBUG, span, "%s: " fmt, \
			__FUNCTION__, ## __VA_ARGS__) ) )

struct reg {
	unsigned long flags;
	unsigned char index;
	unsigned char reg;
	unsigned char value;
};

static int wecareregs[] = 
{ 
	WC_DAA_DCTL1, WC_DAA_DCTL2, WC_DAA_PLL2_N2_M2, WC_DAA_CHIPA_REV, 
	WC_DAA_LINE_STAT, WC_DAA_CHIPB_REV, WC_DAA_INT_CTL2, WC_DAA_INT_CTL4, 
};

struct wcfxo {
	struct pci_dev *dev;
	char *variety;
	struct dahdi_device *ddev;
	struct dahdi_span span;
	struct dahdi_chan _chan;
	struct dahdi_chan *chan;
	int usecount;
	int dead;
	int pos;
	unsigned long flags;
	int freeregion;
	int ring;
	int offhook;
	int battery;
	int wregcount;
	int readpos;
	int rreadpos;
	unsigned int pegtimer;
	int pegcount;
	int peg;
	int battdebounce;
	int nobatttimer;
	int ringdebounce;
#ifdef	JAPAN
	int ohdebounce;
#endif
	int allread;
	int regoffset;			/* How far off our registers are from what we expect */
	int alt;
	int ignoreread;
	int reset;
	/* Up to 6 register can be written at a time */
	struct reg regs[DAHDI_CHUNKSIZE];
	struct reg oldregs[DAHDI_CHUNKSIZE];
	unsigned char lasttx[DAHDI_CHUNKSIZE];
	/* Up to 32 registers of whatever we most recently read */
	unsigned char readregs[32];
	unsigned long ioaddr;
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile int *writechunk;					/* Double-word aligned write memory */
	volatile int *readchunk;					/* Double-word aligned read memory */
#ifdef ZERO_BATT_RING
	int onhook;
#endif
#ifdef ENABLE_TASKLETS
	int taskletrun;
	int taskletsched;
	int taskletpending;
	int taskletexec;
	int txerrors;
	int ints;
	struct tasklet_struct wcfxo_tlet;
#endif
};

#define FLAG_INVERTSER		(1 << 0)
#define FLAG_USE_XTAL		(1 << 1)
#define FLAG_DOUBLE_CLOCK	(1 << 2)
#define FLAG_RESET_ON_AUX5	(1 << 3)
#define FLAG_NO_I18N_REGS	(1 << 4) /*!< Uses si3035, rather si3034 */

struct wcfxo_desc {
	char *name;
	unsigned long flags;
};


static struct wcfxo_desc wcx100p = { "Wildcard X100P",
		FLAG_INVERTSER | FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

static struct wcfxo_desc wcx101p = { "Wildcard X101P",
		FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

static struct wcfxo_desc generic = { "Generic Clone",
		FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

static struct wcfxo *ifaces[WC_MAX_IFACES];

static void wcfxo_release(struct wcfxo *wc);

static int debug = 0;

static int monitor = 0;

static int quiet = 0;

static int boost = 0;

static int opermode = 0;

static struct fxo_mode {
	char *name;
	int ohs;
	int act;
	int dct;
	int rz;
	int rt;
	int lim;
	int vol;
} fxo_modes[] =
{
	{ "FCC", 0, 0, 2, 0, 0, 0, 0 }, 	/* US */
	{ "CTR21", 0, 0, 3, 0, 0, 3, 0 },	/* Austria, Belgium, Denmark, Finland, France, Germany, 
										   Greece, Iceland, Ireland, Italy, Luxembourg, Netherlands,
										   Norway, Portugal, Spain, Sweden, Switzerland, and UK */
};

static inline void wcfxo_transmitprep(struct wcfxo *wc, unsigned char ints)
{
	volatile int *writechunk;
	int x;
	int written=0;
	unsigned short cmd;

	/* if nothing to transmit, have to do the dahdi_transmit() anyway */
	if (!(ints & 3)) {
		/* Calculate Transmission */
		dahdi_transmit(&wc->span);
		return;
	}

	/* Remember what it was we just sent */
	memcpy(wc->lasttx, wc->chan->writechunk, DAHDI_CHUNKSIZE);

	if (ints & 0x01)  {
		/* Write is at interrupt address.  Start writing from normal offset */
		writechunk = wc->writechunk;
	} else {
		writechunk = wc->writechunk + DAHDI_CHUNKSIZE * 2;
	}

	dahdi_transmit(&wc->span);

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Send a sample, as a 32-bit word, and be sure to indicate that a command follows */
		if (wc->flags & FLAG_INVERTSER)
			writechunk[x << 1] = cpu_to_le32(
				~((unsigned short)(DAHDI_XLAW(wc->chan->writechunk[x], wc->chan))| 0x1) << 16
				);
		else
			writechunk[x << 1] = cpu_to_le32(
				((unsigned short)(DAHDI_XLAW(wc->chan->writechunk[x], wc->chan))| 0x1) << 16
				);

		/* We always have a command to follow our signal */
		if (!wc->regs[x].flags) {
			/* Fill in an empty register command with a read for a potentially useful register  */
			wc->regs[x].flags = FLAG_READ;
			wc->regs[x].reg = wecareregs[wc->readpos];
			wc->regs[x].index = wc->readpos;
			wc->readpos++;
			if (wc->readpos >= (sizeof(wecareregs) / sizeof(wecareregs[0]))) {
				wc->allread = 1;
				wc->readpos = 0;
			}
		}

		/* Prepare the command to follow it */
		switch(wc->regs[x].flags) {
		case FLAG_READ:
			cmd = (wc->regs[x].reg | 0x20) << 8;
			break;
		case FLAG_WRITE:
			cmd = (wc->regs[x].reg << 8) | (wc->regs[x].value & 0xff);
			written = 1;
			/* Wait at least four samples before reading */
			wc->ignoreread = 4;
			break;
		default:
			printk(KERN_DEBUG "wcfxo: Huh?  No read or write??\n");
			cmd = 0;
		}
		/* Setup the write chunk */
		if (wc->flags & FLAG_INVERTSER)
			writechunk[(x << 1) + 1] = cpu_to_le32(~(cmd << 16));
		else
			writechunk[(x << 1) + 1] = cpu_to_le32(cmd << 16);
	}
	if (written)
		wc->readpos = 0;
	wc->wregcount = 0;

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Rotate through registers */
		wc->oldregs[x] = wc->regs[x];
		wc->regs[x].flags = FLAG_EMPTY;
	}

}

static inline void wcfxo_receiveprep(struct wcfxo *wc, unsigned char ints)
{
	volatile int *readchunk;
	int x;
	int realreg;
	int realval;
	int sample;
	if (ints & 0x04)
		/* Read is at interrupt address.  Valid data is available at normal offset */
		readchunk = wc->readchunk;
	else
		readchunk = wc->readchunk + DAHDI_CHUNKSIZE * 2;

	/* Keep track of how quickly our peg alternates */
	wc->pegtimer+=DAHDI_CHUNKSIZE;
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {

		/* We always have a command to follow our signal.  */
		if (wc->oldregs[x].flags == FLAG_READ && !wc->ignoreread) {
			realreg = wecareregs[(wc->regs[x].index + wc->regoffset) %
							(sizeof(wecareregs) / sizeof(wecareregs[0]))];
			realval = (le32_to_cpu(readchunk[(x << 1) +wc->alt]) >> 16) & 0xff;
			if ((realval == 0x89) && (realreg != WC_DAA_PLL2_N2_M2)) {
				/* Some sort of slippage, correct for it */
				while(realreg != WC_DAA_PLL2_N2_M2) {
					/* Find register 9 */
					realreg = wecareregs[(wc->regs[x].index + ++wc->regoffset) %
										 (sizeof(wecareregs) / sizeof(wecareregs[0]))];
					wc->regoffset = wc->regoffset % (sizeof(wecareregs) / sizeof(wecareregs[0]));
				}
				if (debug)
					printk(KERN_DEBUG "New regoffset: %d\n", wc->regoffset);
			}
			/* Receive into the proper register */
			wc->readregs[realreg] = realval;
		}
		/* Look for pegging to indicate ringing */
		sample = (short)(le32_to_cpu(readchunk[(x << 1) + (1 - wc->alt)]) >> 16);
		if ((sample > 32000) && (wc->peg != 1)) {
			if ((wc->pegtimer < PEGTIME) && (wc->pegtimer > MINPEGTIME))
				wc->pegcount++;
			wc->pegtimer = 0;
			wc->peg = 1;
		} else if ((sample < -32000) && (wc->peg != -1)) {
			if ((wc->pegtimer < PEGTIME) && (wc->pegtimer > MINPEGTIME))
				wc->pegcount++;
			wc->pegtimer = 0;
			wc->peg = -1;
		}
		wc->chan->readchunk[x] = DAHDI_LIN2X((sample), (wc->chan));
	}
	if (wc->pegtimer > PEGTIME) {
		/* Reset pegcount if our timer expires */
		wc->pegcount = 0;
	}
	/* Decrement debouncer if appropriate */
	if (wc->ringdebounce)
		wc->ringdebounce--;
	if (!wc->offhook && !wc->ringdebounce) {
		if (!wc->ring && (wc->pegcount > PEGCOUNT)) {
			/* It's ringing */
			if (debug)
				printk(KERN_DEBUG "RING!\n");
			dahdi_hooksig(wc->chan, DAHDI_RXSIG_RING);
			wc->ring = 1;
		}
		if (wc->ring && !wc->pegcount) {
			/* No more ring */
			if (debug)
				printk(KERN_DEBUG "NO RING!\n");
			dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
			wc->ring = 0;
		}
	}
	if (wc->ignoreread)
		wc->ignoreread--;

	/* Do the echo cancellation...  We are echo cancelling against
	   what we sent two chunks ago*/
	dahdi_ec_chunk(wc->chan, wc->chan->readchunk, wc->lasttx);

	/* Receive the result */
	dahdi_receive(&wc->span);
}

#ifdef ENABLE_TASKLETS
static void wcfxo_tasklet(unsigned long data)
{
	struct wcfxo *wc = (struct wcfxo *)data;
	wc->taskletrun++;
	/* Run tasklet */
	if (wc->taskletpending) {
		wc->taskletexec++;
		wcfxo_receiveprep(wc, wc->ints);
		wcfxo_transmitprep(wc, wc->ints);
	}
	wc->taskletpending = 0;
}
#endif

static void wcfxo_stop_dma(struct wcfxo *wc);
static void wcfxo_restart_dma(struct wcfxo *wc);

DAHDI_IRQ_HANDLER(wcfxo_interrupt)
{
	struct wcfxo *wc = dev_id;
	unsigned char ints;
	unsigned char b;
#ifdef DEBUG_RING
	static int oldb = 0;
	static int oldcnt = 0;
#endif

	ints = inb(wc->ioaddr + WC_INTSTAT);


	if (!ints)
		return IRQ_NONE;

	outb(ints, wc->ioaddr + WC_INTSTAT);

	if (ints & 0x0c) {  /* if there is a rx interrupt pending */
#ifdef ENABLE_TASKLETS
		wc->ints = ints;
		if (!wc->taskletpending) {
			wc->taskletpending = 1;
			wc->taskletsched++;
			tasklet_hi_schedule(&wc->wcfxo_tlet);
		} else
			wc->txerrors++;
#else
		wcfxo_receiveprep(wc, ints);
		/* transmitprep looks to see if there is anything to transmit
		   and returns by itself if there is nothing */
		wcfxo_transmitprep(wc, ints);
#endif
	}

	if (ints & 0x10) {
		printk(KERN_INFO "FXO PCI Master abort\n");
		/* Stop DMA andlet the watchdog start it again */
		wcfxo_stop_dma(wc);
		return IRQ_RETVAL(1);
	}

	if (ints & 0x20) {
		printk(KERN_INFO "PCI Target abort\n");
		return IRQ_RETVAL(1);
	}
	if (1 /* !(wc->report % 0xf) */) {
		/* Check for BATTERY from register and debounce for 8 ms */
		b = wc->readregs[WC_DAA_LINE_STAT] & 0xf;
		if (!b) {
			wc->nobatttimer++;
#if 0
			if (wc->battery)
				printk(KERN_DEBUG "Battery loss: %d (%d debounce)\n", b, wc->battdebounce);
#endif
			if (wc->battery && !wc->battdebounce) {
				if (debug)
					printk(KERN_DEBUG "NO BATTERY!\n");
				wc->battery =  0;
#ifdef	JAPAN
				if ((!wc->ohdebounce) && wc->offhook) {
					dahdi_hooksig(wc->chan, DAHDI_RXSIG_ONHOOK);
					if (debug)
						printk(KERN_DEBUG "Signalled On Hook\n");
#ifdef	ZERO_BATT_RING
					wc->onhook++;
#endif
				}
#else
				dahdi_hooksig(wc->chan, DAHDI_RXSIG_ONHOOK);
#endif
				wc->battdebounce = BATT_DEBOUNCE;
			} else if (!wc->battery)
				wc->battdebounce = BATT_DEBOUNCE;
			if ((wc->nobatttimer > 5000) &&
#ifdef	ZERO_BATT_RING
			    !(wc->readregs[WC_DAA_DCTL1] & 0x04) &&
#endif
			    (!wc->span.alarms)) {
				wc->span.alarms = DAHDI_ALARM_RED;
				dahdi_alarm_notify(&wc->span);
			}
		} else if (b == 0xf) {
			if (!wc->battery && !wc->battdebounce) {
				if (debug)
					printk(KERN_DEBUG "BATTERY!\n");
#ifdef	ZERO_BATT_RING
				if (wc->onhook) {
					wc->onhook = 0;
					dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
					if (debug)
						printk(KERN_DEBUG "Signalled Off Hook\n");
				}
#else
				dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
#endif
				wc->battery = 1;
				wc->nobatttimer = 0;
				wc->battdebounce = BATT_DEBOUNCE;
				if (wc->span.alarms) {
					wc->span.alarms = 0;
					dahdi_alarm_notify(&wc->span);
				}
			} else if (wc->battery)
				wc->battdebounce = BATT_DEBOUNCE;
		} else {
			/* It's something else... */
				wc->battdebounce = BATT_DEBOUNCE;
		}

		if (wc->battdebounce)
			wc->battdebounce--;
#ifdef	JAPAN
		if (wc->ohdebounce)
			wc->ohdebounce--;
#endif

	}

	return IRQ_RETVAL(1);
}

static int wcfxo_setreg(struct wcfxo *wc, unsigned char reg, unsigned char value)
{
	int x;
	if (wc->wregcount < DAHDI_CHUNKSIZE) {
		x = wc->wregcount;
		wc->regs[x].reg = reg;
		wc->regs[x].value = value;
		wc->regs[x].flags = FLAG_WRITE;
		wc->wregcount++;
		return 0;
	}
	printk(KERN_NOTICE "wcfxo: Out of space to write register %02x with %02x\n", reg, value);
	return -1;
}

static inline struct wcfxo *wcfxo_from_span(struct dahdi_span *span)
{
	return container_of(span, struct wcfxo, span);
}

static int wcfxo_open(struct dahdi_chan *chan)
{
	struct wcfxo *wc = chan->pvt;
	if (wc->dead)
		return -ENODEV;
	wc->usecount++;
	return 0;
}

static int wcfxo_watchdog(struct dahdi_span *span, int event)
{
	printk(KERN_INFO "FXO: Restarting DMA\n");
	wcfxo_restart_dma(wcfxo_from_span(span));
	return 0;
}

static int wcfxo_close(struct dahdi_chan *chan)
{
	struct wcfxo *wc = chan->pvt;
	wc->usecount--;
	/* If we're dead, release us now */
	if (!wc->usecount && wc->dead)
		wcfxo_release(wc);
	return 0;
}

static int wcfxo_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct wcfxo *wc = chan->pvt;
	int reg=0;
	switch(txsig) {
	case DAHDI_TXSIG_START:
	case DAHDI_TXSIG_OFFHOOK:
		/* Take off hook and enable normal mode reception.  This must
		   be done in two steps because of a hardware bug. */
		reg = wc->readregs[WC_DAA_DCTL1] & ~0x08;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);

		reg = reg | 0x1;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);
		wc->offhook = 1;
#ifdef	JAPAN
		wc->battery = 1;
		wc->battdebounce = BATT_DEBOUNCE;
		wc->ohdebounce = OH_DEBOUNCE;
#endif
		break;
	case DAHDI_TXSIG_ONHOOK:
		/* Put on hook and enable on hook line monitor */
		reg =  wc->readregs[WC_DAA_DCTL1] & 0xfe;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);

		reg = reg | 0x08;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);
		wc->offhook = 0;
		/* Don't accept a ring for another 1000 ms */
		wc->ringdebounce = 1000;
#ifdef	JAPAN
		wc->ohdebounce = OH_DEBOUNCE;
#endif
		break;
	default:
		printk(KERN_NOTICE "wcfxo: Can't set tx state to %d\n", txsig);
	}
	if (debug)
		printk(KERN_DEBUG "Setting hook state to %d (%02x)\n", txsig, reg);
	return 0;
}

static const struct dahdi_span_ops wcfxo_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = wcfxo_hooksig,
	.open = wcfxo_open,
	.close = wcfxo_close,
	.watchdog = wcfxo_watchdog,
};

static int wcfxo_initialize(struct wcfxo *wc)
{
	wc->ddev = dahdi_create_device();

	/* DAHDI stuff */
	sprintf(wc->span.name, "WCFXO/%d", wc->pos);
	snprintf(wc->span.desc, sizeof(wc->span.desc) - 1, "%s Board %d", wc->variety, wc->pos + 1);
	sprintf(wc->chan->name, "WCFXO/%d/%d", wc->pos, 0);
	wc->ddev->location = kasprintf(GFP_KERNEL, "PCI Bus %02d Slot %02d",
				      wc->dev->bus->number,
				      PCI_SLOT(wc->dev->devfn) + 1);
	if (!wc->ddev->location)
		return -ENOMEM;

	wc->ddev->manufacturer = "Digium";
	wc->ddev->devicetype = wc->variety;
	wc->chan->sigcap = DAHDI_SIG_FXSKS | DAHDI_SIG_FXSLS | DAHDI_SIG_SF;
	wc->chan->chanpos = 1;
	wc->span.chans = &wc->chan;
	wc->span.channels = 1;
	wc->span.flags = DAHDI_FLAG_RBS;
	wc->span.deflaw = DAHDI_LAW_MULAW;
#ifdef ENABLE_TASKLETS
	tasklet_init(&wc->wcfxo_tlet, wcfxo_tasklet, (unsigned long)wc);
#endif

	wc->chan->pvt = wc;
	wc->span.ops = &wcfxo_span_ops;
	list_add_tail(&wc->span.device_node, &wc->ddev->spans);
	if (dahdi_register_device(wc->ddev, &wc->dev->dev)) {
		printk(KERN_NOTICE "Unable to register span with DAHDI\n");
		return -1;
	}
	return 0;
}

static int wcfxo_hardware_init(struct wcfxo *wc)
{
	/* Hardware stuff */
	/* Reset PCI Interface chip and registers */
	outb(0x0e, wc->ioaddr + WC_CNTL);

	/* Set all to outputs except AUX 4, which is an input */
	outb(0xef, wc->ioaddr + WC_AUXC);

	/* Reset the DAA (DAA uses AUX5 for reset) */
	outb(0x00, wc->ioaddr + WC_AUXD);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + HZ / 800);

	/* Set hook state to on hook & un-reset the DAA */
	if (wc->flags & FLAG_RESET_ON_AUX5) {
		/* Set hook state to on hook for when we switch.
		   Make sure reset is high */
		outb(0x34, wc->ioaddr + WC_AUXD);
	} else {
		/* Set hook state to on hook for when we switch */
		outb(0x24, wc->ioaddr + WC_AUXD);
	}

	/* Back to normal, with automatic DMA wrap around */
	outb(0x01, wc->ioaddr + WC_CNTL);

	/* Make sure serial port and DMA are out of reset */
	outb(inb(wc->ioaddr + WC_CNTL) & 0xf9, wc->ioaddr + WC_CNTL);

	/* Configure serial port for MSB->LSB operation */
	if (wc->flags & FLAG_DOUBLE_CLOCK)
		outb(0xc1, wc->ioaddr + WC_SERCTL);
	else
		outb(0xc0, wc->ioaddr + WC_SERCTL);

	if (wc->flags & FLAG_USE_XTAL) {
		/* Use the crystal oscillator */
		outb(0x04, wc->ioaddr + WC_AUXFUNC);
	}

	/* Delay FSC by 2 so it's properly aligned */
	outb(0x2, wc->ioaddr + WC_FSCDELAY);

	/* Setup DMA Addresses */
	outl(wc->writedma,                    wc->ioaddr + WC_DMAWS);		/* Write start */
	outl(wc->writedma + DAHDI_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMAWI);		/* Middle (interrupt) */
	outl(wc->writedma + DAHDI_CHUNKSIZE * 16 - 4, wc->ioaddr + WC_DMAWE);			/* End */
	
	outl(wc->readdma,                    	 wc->ioaddr + WC_DMARS);	/* Read start */
	outl(wc->readdma + DAHDI_CHUNKSIZE * 8 - 4, 	 wc->ioaddr + WC_DMARI);	/* Middle (interrupt) */
	outl(wc->readdma + DAHDI_CHUNKSIZE * 16 - 4, wc->ioaddr + WC_DMARE);	/* End */
	
	/* Clear interrupts */
	outb(0xff, wc->ioaddr + WC_INTSTAT);
	return 0;
}

static void wcfxo_enable_interrupts(struct wcfxo *wc)
{
	/* Enable interrupts (we care about all of them) */
	outb(0x3f, wc->ioaddr + WC_MASK0);
	/* No external interrupts */
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void wcfxo_start_dma(struct wcfxo *wc)
{
	/* Reset Master and TDM */
	outb(0x0f, wc->ioaddr + WC_CNTL);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	outb(0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
}

static void wcfxo_restart_dma(struct wcfxo *wc)
{
	/* Reset Master and TDM */
	outb(0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
}


static void wcfxo_stop_dma(struct wcfxo *wc)
{
	outb(0x00, wc->ioaddr + WC_OPER);
}

static void wcfxo_reset_tdm(struct wcfxo *wc)
{
	/* Reset TDM */
	outb(0x0f, wc->ioaddr + WC_CNTL);
}

static void wcfxo_disable_interrupts(struct wcfxo *wc)	
{
	outb(0x00, wc->ioaddr + WC_MASK0);
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void wcfxo_set_daa_mode(struct wcfxo *wc)
{
	/* Set country specific parameters (OHS, ACT, DCT, RZ, RT, LIM, VOL) */
	int reg16 = ((fxo_modes[opermode].ohs & 0x1) << 6) |
				((fxo_modes[opermode].act & 0x1) << 5) |
				((fxo_modes[opermode].dct & 0x3) << 2) |
				((fxo_modes[opermode].rz & 0x1) << 1) |
				((fxo_modes[opermode].rt & 0x1) << 0);
	int reg17 = ((fxo_modes[opermode].lim & 0x3) << 3);
	int reg18 = ((fxo_modes[opermode].vol & 0x3) << 3);

	if (wc->flags & FLAG_NO_I18N_REGS) {
		wcfxo_dbg(wc->span, "This card does not support international settings.\n");
		return;
	}

	wcfxo_setreg(wc, WC_DAA_INT_CTL1, reg16);
	wcfxo_setreg(wc, WC_DAA_INT_CTL2, reg17);
	wcfxo_setreg(wc, WC_DAA_INT_CTL3, reg18);


	/* Wait a couple of jiffies for our writes to finish */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);

	printk(KERN_INFO "wcfxo: DAA mode is '%s'\n", fxo_modes[opermode].name);
}

static int wcfxo_init_daa(struct wcfxo *wc)
{
	/* This must not be called in an interrupt */
	/* We let things settle for a bit */
	unsigned char reg15;
	int chip_revb;
//	set_current_state(TASK_INTERRUPTIBLE);
//	schedule_timeout(10);

	/* Soft-reset it */
	wcfxo_setreg(wc, WC_DAA_CTL1, 0x80);

	/* Let the reset go */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);

	/* We have a clock at 18.432 Mhz, so N1=1, M1=2, CGM=0 */
	wcfxo_setreg(wc, WC_DAA_PLL1_N1, 0x0);	/* This value is N1 - 1 */
	wcfxo_setreg(wc, WC_DAA_PLL1_M1, 0x1);	/* This value is M1 - 1 */
	/* We want to sample at 8khz, so N2 = 9, M2 = 10 (N2-1, M2-1) */
	wcfxo_setreg(wc, WC_DAA_PLL2_N2_M2, 0x89);
	
	/* Wait until the PLL's are locked. Time is between 100 uSec and 1 mSec */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + HZ/1000 + (DAHDI_CHUNKSIZE * HZ) / 800);

	/* No additional ration is applied to the PLL and faster lock times
	 * are possible */
	wcfxo_setreg(wc, WC_DAA_PLL_CTL, 0x0);
	/* Enable off hook pin */
	wcfxo_setreg(wc, WC_DAA_DCTL1, 0x0a);
	if (monitor) {
		/* Enable ISOcap and external speaker and charge pump if present */
		wcfxo_setreg(wc, WC_DAA_DCTL2, 0x80);
	} else {
		/* Enable ISOcap and charge pump if present (leave speaker disabled) */
		wcfxo_setreg(wc, WC_DAA_DCTL2, 0xe0);
	}

	/* Wait a couple of jiffies for our writes to finish */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);
	reg15 = 0x0;
	/* Go ahead and attenuate transmit signal by 6 db */
	if (quiet) {
		printk(KERN_INFO "wcfxo: Attenuating transmit signal for quiet operation\n");
		reg15 |= (quiet & 0x3) << 4;
	}
	if (boost) {
		printk(KERN_INFO "wcfxo: Boosting receive signal\n");
		reg15 |= (boost & 0x3);
	}
	wcfxo_setreg(wc, WC_DAA_TXRX_GCTL, reg15);

	/* REVB: reg. 13, bits 5:2 */ 
	chip_revb = (wc->readregs[WC_DAA_CHIPB_REV] >> 2) & 0xF; 
	wcfxo_dbg(wc->span, "DAA chip REVB is %x\n", chip_revb);
	switch(chip_revb) {
		case 1: case 2: case 3:
			/* This is a si3034. Nothing to do */
			break;
		case 4: case 5: case 7:
			/* This is 3035. Has no support for international registers */
			wc->flags |= FLAG_NO_I18N_REGS;
			break;
		default:
			wcfxo_notice(wc->span, "Unknown DAA chip revision: REVB=%d\n",
					chip_revb);
	}

	/* Didn't get it right.  Register 9 is still garbage */
	if (wc->readregs[WC_DAA_PLL2_N2_M2] != 0x89)
		return -1;
#if 0
	{ int x;
	int y;
	for (y=0;y<100;y++) {
		printk(KERN_DEBUG " reg dump ====== %d ======\n", y);
		for (x=0;x<sizeof(wecareregs) / sizeof(wecareregs[0]);x++) {
			printk(KERN_DEBUG "daa: Reg %d: %02x\n", wecareregs[x], wc->readregs[wecareregs[x]]);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(100);
	} }
#endif	
	return 0;
}

static int __devinit wcfxo_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct wcfxo *wc;
	struct wcfxo_desc *d = (struct wcfxo_desc *)ent->driver_data;
	int x;

	for (x=0;x<WC_MAX_IFACES;x++)
		if (!ifaces[x]) break;
	if (x >= WC_MAX_IFACES) {
		printk(KERN_ERR "Too many interfaces: Found %d, can only handle %d.\n",
				x, WC_MAX_IFACES - 1);
		return -EIO;
	}
	
	if (pci_enable_device(pdev))
		return -EIO;

	wc = kmalloc(sizeof(struct wcfxo), GFP_KERNEL);
	if (!wc) {
		printk(KERN_ERR "wcfxo: Failed initializinf card. Not enough memory.");
		return -ENOMEM;
	}

	ifaces[x] = wc;
	memset(wc, 0, sizeof(struct wcfxo));
	wc->chan = &wc->_chan;
	wc->ioaddr = pci_resource_start(pdev, 0);
	wc->dev = pdev;
	wc->pos = x;
	wc->variety = d->name;
	wc->flags = d->flags;
	/* Keep track of whether we need to free the region */
	if (request_region(wc->ioaddr, 0xff, "wcfxo")) 
		wc->freeregion = 1;

	/* Allocate enough memory for two zt chunks, receive and transmit.  Each sample uses
	   32 bits.  Allocate an extra set just for control too */
	wc->writechunk = (int *)pci_alloc_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, &wc->writedma);
	if (!wc->writechunk) {
		printk(KERN_NOTICE "wcfxo: Unable to allocate DMA-able memory\n");
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		return -ENOMEM;
	}

	wc->readchunk = wc->writechunk + DAHDI_MAX_CHUNKSIZE * 4;	/* in doublewords */
	wc->readdma = wc->writedma + DAHDI_MAX_CHUNKSIZE * 16;		/* in bytes */

	if (wcfxo_initialize(wc)) {
		printk(KERN_NOTICE "wcfxo: Unable to intialize modem\n");
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc);
		return -EIO;
	}

	/* Enable bus mastering */
	pci_set_master(pdev);

	/* Keep track of which device we are */
	pci_set_drvdata(pdev, wc);

	if (request_irq(pdev->irq, wcfxo_interrupt, DAHDI_IRQ_SHARED, "wcfxo", wc)) {
		printk(KERN_NOTICE "wcfxo: Unable to request IRQ %d\n", pdev->irq);
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc);
		return -EIO;
	}


	wcfxo_hardware_init(wc);
	/* Enable interrupts */
	wcfxo_enable_interrupts(wc);
	/* Initialize Write/Buffers to all blank data */
	memset((void *)wc->writechunk,0,DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4);
	/* Start DMA */
	wcfxo_start_dma(wc);

	/* Initialize DAA (after it's started) */
	if (wcfxo_init_daa(wc)) {
		printk(KERN_NOTICE "Failed to initailize DAA, giving up...\n");
		wcfxo_stop_dma(wc);
		wcfxo_disable_interrupts(wc);
		dahdi_unregister_device(wc->ddev);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		outb(0x0e, wc->ioaddr + WC_CNTL);

		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc->ddev->location);
		dahdi_free_device(wc->ddev);
		kfree(wc);
		return -EIO;
	}
	wcfxo_set_daa_mode(wc);
	printk(KERN_INFO "Found a Wildcard FXO: %s\n", wc->variety);

	return 0;
}

static void wcfxo_release(struct wcfxo *wc)
{
	dahdi_unregister_device(wc->ddev);
	if (wc->freeregion)
		release_region(wc->ioaddr, 0xff);
	kfree(wc->ddev->location);
	dahdi_free_device(wc->ddev);
	kfree(wc);
	printk(KERN_INFO "Freed a Wildcard\n");
}

static void __devexit wcfxo_remove_one(struct pci_dev *pdev)
{
	struct wcfxo *wc = pci_get_drvdata(pdev);
	if (wc) {

		/* Stop any DMA */
		wcfxo_stop_dma(wc);
		wcfxo_reset_tdm(wc);

		/* In case hardware is still there */
		wcfxo_disable_interrupts(wc);
		
		/* Immediately free resources */
		pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		outb(0x0e, wc->ioaddr + WC_CNTL);

		/* Release span, possibly delayed */
		if (!wc->usecount)
			wcfxo_release(wc);
		else
			wc->dead = 1;
	}
}

static DEFINE_PCI_DEVICE_TABLE(wcfxo_pci_tbl) = {
	{ 0xe159, 0x0001, 0x8084, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0xe159, 0x0001, 0x8085, PCI_ANY_ID, 0, 0, (unsigned long) &wcx101p },
	{ 0xe159, 0x0001, 0x8086, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0xe159, 0x0001, 0x8087, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0x1057, 0x5608, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcx100p },
	{ 0 }
};

MODULE_DEVICE_TABLE (pci, wcfxo_pci_tbl);

static struct pci_driver wcfxo_driver = {
	.name = "wcfxo",
	.probe = wcfxo_init_one,
	.remove = __devexit_p(wcfxo_remove_one),
	.id_table = wcfxo_pci_tbl,
};

static int __init wcfxo_init(void)
{
	int res;
	int x;
	if ((opermode >= sizeof(fxo_modes) / sizeof(fxo_modes[0])) || (opermode < 0)) {
		printk(KERN_NOTICE "Invalid/unknown operating mode specified.  Please choose one of:\n");
		for (x=0;x<sizeof(fxo_modes) / sizeof(fxo_modes[0]); x++)
			printk(KERN_INFO "%d: %s\n", x, fxo_modes[x].name);
		return -ENODEV;
	}
	res = dahdi_pci_module(&wcfxo_driver);
	if (res)
		return -ENODEV;
	return 0;
}

static void __exit wcfxo_cleanup(void)
{
	pci_unregister_driver(&wcfxo_driver);
}

module_param(debug, int, 0644);
module_param(quiet, int, 0444);
module_param(boost, int, 0444);
module_param(monitor, int, 0444);
module_param(opermode, int, 0444);

MODULE_DESCRIPTION("Wildcard X100P Driver");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(wcfxo_init);
module_exit(wcfxo_cleanup);
