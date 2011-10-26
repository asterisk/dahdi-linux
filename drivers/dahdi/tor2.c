/*
 * Tormenta 2  Quad-T1 PCI Driver
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and archetectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001-2008, Digium, Inc.
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
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>
#define NEED_PCI_IDS
#include "tor2-hw.h"
#include "tor2fw.h"

/*
 * Tasklets provide better system interactive response at the cost of the
 * possibility of losing a frame of data at very infrequent intervals.  If
 * you are more concerned with the performance of your machine, enable the
 * tasklets.  If you are strict about absolutely no drops, then do not enable
 * tasklets.
 */

/* #define ENABLE_TASKLETS */

/* this stuff needs to work for 64 bit systems, however using the macro causes
   it to take twice as long */
/* #define FIXTHISFOR64 */  /* as of now, un-comment for 32 bit only system */

#define SPANS_PER_CARD  4
#define MAX_SPANS       16

#define FLAG_STARTED (1 << 0)

#define	TYPE_T1	1		/* is a T1 card */
#define	TYPE_E1	2		/* is an E1 card */

struct tor2_chan {
	/* Private pointer for channel.  We want to know our
	   channel and span */
	struct tor2 *tor;
	int span;	/* Index from 0 */
};

struct tor2_span {
	/* Private pointer for span.  We want to know our
	   span number and pointer to the tor device */
	struct tor2 *tor;
	int span;	/* Index from 0 */
	struct dahdi_span dahdi_span;
};

struct tor2 {
	/* This structure exists one per card */
	struct pci_dev *pci;		/* Pointer to PCI device */
	int num;			/* Which card we are */
	int syncsrc;			/* active sync source */
	int syncs[SPANS_PER_CARD];	/* sync sources */
	int psyncs[SPANS_PER_CARD];	/* span-relative sync sources */
	int alarmtimer[SPANS_PER_CARD];	/* Alarm timer */
	char *type;			/* Type of tormenta 2 card */
	int irq;			/* IRQ used by device */
	int order;			/* Order */
	int flags;			/* Device flags */
	int syncpos[SPANS_PER_CARD];	/* span-relative sync sources */
	int master;			/* Are we master */
	unsigned long plx_region;	/* phy addr of PCI9030 registers */
	unsigned long plx_len;		/* length of PLX window */
	__iomem volatile unsigned short *plx;	/* Virtual representation of local space */
	unsigned long xilinx32_region;	/* 32 bit Region allocated to Xilinx */
	unsigned long xilinx32_len;	/* Length of 32 bit Xilinx region */
	__iomem volatile unsigned int *mem32;	/* Virtual representation of 32 bit Xilinx memory area */
	unsigned long xilinx8_region;	/* 8 bit Region allocated to Xilinx */
	unsigned long xilinx8_len;	/* Length of 8 bit Xilinx region */
	__iomem volatile unsigned char *mem8;	/* Virtual representation of 8 bit Xilinx memory area */
	struct dahdi_device *ddev;
	struct tor2_span tspans[SPANS_PER_CARD];	/* Span data */
	struct dahdi_chan **chans[SPANS_PER_CARD]; /* Pointers to card channels */
	struct tor2_chan tchans[32 * SPANS_PER_CARD];	/* Channel user data */
	unsigned char txsigs[SPANS_PER_CARD][16];	/* Copy of tx sig registers */
	int loopupcnt[SPANS_PER_CARD];	/* loop up code counter */
	int loopdowncnt[SPANS_PER_CARD];/* loop down code counter */
	int spansstarted;		/* number of spans started */
	spinlock_t lock;		/* lock context */
	unsigned char leds;		/* copy of LED register */
	unsigned char ec_chunk1[SPANS_PER_CARD][32][DAHDI_CHUNKSIZE]; /* first EC chunk buffer */
	unsigned char ec_chunk2[SPANS_PER_CARD][32][DAHDI_CHUNKSIZE]; /* second EC chunk buffer */
#ifdef ENABLE_TASKLETS
	int taskletrun;
	int taskletsched;
	int taskletpending;
	int taskletexec;
	int txerrors;
	struct tasklet_struct tor2_tlet;
#endif
	int cardtype;		/* card type, T1 or E1 */
	unsigned int *datxlt;	/* pointer to datxlt structure */
	unsigned int passno;	/* number of interrupt passes */
};

#define t1out(tor, span, reg, val) \
	writeb(val, &tor->mem8[((span - 1) * 0x100) + reg])
#define t1in(tor, span, reg) readb(&tor->mem8[((span - 1) * 0x100) + reg])

#ifdef ENABLE_TASKLETS
static void tor2_tasklet(unsigned long data);
#endif

#define	GPIOC (PLX_LOC_GPIOC >> 1) /* word-oriented address for PLX GPIOC reg. (32 bit reg.) */
#define	LAS2BRD (0x30 >> 1)
#define	LAS3BRD (0x34 >> 1)
#define	INTCSR (0x4c >> 1)  /* word-oriented address for PLX INTCSR reg. */
#define	PLX_INTENA 0x43 /* enable, hi-going, level trigger */

#define	SYNCREG	0x400
#define	CTLREG	0x401
#define	LEDREG	0x402
#define	STATREG	0x400
#define SWREG	0x401
#define	CTLREG1	0x404

#define	INTENA	(1 + ((loopback & 3) << 5))
#define	OUTBIT	(2 + ((loopback & 3) << 5))
#define	E1DIV	0x10
#define	INTACK	(0x80 + ((loopback & 3) << 5))
#define	INTACTIVE 2
#define MASTER (1 << 3)

/* un-define this if you dont want NON-REV A hardware support */
/* #define	NONREVA 1 */

#define	SYNCSELF 0
#define	SYNC1	1
#define	SYNC2	2
#define	SYNC3	3
#define	SYNC4	4
#define SYNCEXTERN 5

#define	LEDRED	2
#define	LEDGREEN 1

#define MAX_TOR_CARDS 64

static struct tor2 *cards[MAX_TOR_CARDS];

/* signalling bits */
#define	TOR_ABIT 8
#define	TOR_BBIT 4

static int debug;
static int japan;
static int loopback;
static int highestorder;
static int timingcable;

static void set_clear(struct tor2 *tor);
static int tor2_startup(struct file *file, struct dahdi_span *span);
static int tor2_shutdown(struct dahdi_span *span);
static int tor2_rbsbits(struct dahdi_chan *chan, int bits);
static int tor2_maint(struct dahdi_span *span, int cmd);
static int tor2_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data);
DAHDI_IRQ_HANDLER(tor2_intr);

/* translations of data channels for 24 channels in a 32 bit PCM highway */
static unsigned datxlt_t1[] = { 
    1 ,2 ,3 ,5 ,6 ,7 ,9 ,10,11,13,14,15,17,18,19,21,22,23,25,26,27,29,30,31 };

/* translations of data channels for 30/31 channels in a 32 bit PCM highway */
static unsigned datxlt_e1[] = { 
    1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
	25,26,27,28,29,30,31 };

static int tor2_spanconfig(struct file *file, struct dahdi_span *span,
			   struct dahdi_lineconfig *lc)
{
	int i;
	struct tor2_span *p = container_of(span, struct tor2_span, dahdi_span);

	if (debug)
		printk(KERN_INFO "Tor2: Configuring span %d\n", span->spanno);

	if ((lc->sync < 0) || (lc->sync > SPANS_PER_CARD)) {
		printk(KERN_WARNING "%s %d: invalid span timing value %d.\n",
				THIS_MODULE->name, span->spanno, lc->sync);
		return -EINVAL;
	}
	span->syncsrc = p->tor->syncsrc;
	
	/* remove this span number from the current sync sources, if there */
	for (i = 0; i < SPANS_PER_CARD; i++) {
		if (p->tor->syncs[i] == span->spanno) {
			p->tor->syncs[i] = 0;
			p->tor->psyncs[i] = 0;
		}
	}
	p->tor->syncpos[p->span] = lc->sync;
	/* if a sync src, put it in the proper place */
	if (lc->sync) {
		p->tor->syncs[lc->sync - 1] = span->spanno;
		p->tor->psyncs[lc->sync - 1] = p->span + 1;
	}
	/* If we're already running, then go ahead and apply the changes */
	if (span->flags & DAHDI_FLAG_RUNNING)
		return tor2_startup(file, span);

	return 0;
}

static int tor2_chanconfig(struct file *file,
			   struct dahdi_chan *chan, int sigtype)
{
	int alreadyrunning;
	unsigned long flags;
	struct tor2_chan *p = chan->pvt;

	alreadyrunning = chan->span->flags & DAHDI_FLAG_RUNNING;
	if (debug) {
		if (alreadyrunning)
			printk(KERN_INFO "Tor2: Reconfigured channel %d (%s) sigtype %d\n", chan->channo, chan->name, sigtype);
		else
			printk(KERN_INFO "Tor2: Configured channel %d (%s) sigtype %d\n", chan->channo, chan->name, sigtype);
	}		
	/* nothing more to do if an E1 */
	if (p->tor->cardtype == TYPE_E1) return 0;
	spin_lock_irqsave(&p->tor->lock, flags);	
	if (alreadyrunning)
		set_clear(p->tor);
	spin_unlock_irqrestore(&p->tor->lock, flags);	
	return 0;
}

static int tor2_open(struct dahdi_chan *chan)
{
	return 0;
}

static int tor2_close(struct dahdi_chan *chan)
{
	return 0;
}

static const struct dahdi_span_ops tor2_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = tor2_spanconfig,
	.chanconfig = tor2_chanconfig,
	.startup = tor2_startup,
	.shutdown = tor2_shutdown,
	.rbsbits = tor2_rbsbits,
	.maint = tor2_maint,
	.open = tor2_open,
	.close  = tor2_close,
	.ioctl = tor2_ioctl,
};

static void init_spans(struct tor2 *tor)
{
	int x, y, c;
	/* TODO: a debug printk macro */
	for (x = 0; x < SPANS_PER_CARD; x++) {
		struct dahdi_span *const s = &tor->tspans[x].dahdi_span;
		sprintf(s->name, "Tor2/%d/%d", tor->num, x + 1);
		snprintf(s->desc, sizeof(s->desc) - 1,
			 "Tormenta 2 (PCI) Quad %s Card %d Span %d",
			 (tor->cardtype == TYPE_T1)  ?  "T1"  :  "E1", tor->num, x + 1);
		if (tor->cardtype == TYPE_T1) {
			s->channels = 24;
			s->deflaw = DAHDI_LAW_MULAW;
			s->linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_B8ZS | DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF;
			s->spantype = "T1";
		} else {
			s->channels = 31;
			s->deflaw = DAHDI_LAW_ALAW;
			s->linecompat = DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
			s->spantype = "E1";
		}
		s->chans = tor->chans[x];
		s->flags = DAHDI_FLAG_RBS;
		s->ops = &tor2_span_ops;

		tor->tspans[x].tor = tor;
		tor->tspans[x].span = x;
		for (y = 0; y < s->channels; y++) {
			struct dahdi_chan *mychans = tor->chans[x][y];
			sprintf(mychans->name, "Tor2/%d/%d/%d", tor->num, x + 1, y + 1);
			mychans->sigcap = DAHDI_SIG_EM | DAHDI_SIG_CLEAR | DAHDI_SIG_FXSLS | DAHDI_SIG_FXSGS | DAHDI_SIG_FXSKS |
 									 DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS | DAHDI_SIG_FXOKS | DAHDI_SIG_CAS | DAHDI_SIG_SF | DAHDI_SIG_EM_E1;
			c = (x * s->channels) + y;
			mychans->pvt = &tor->tchans[c];
			mychans->chanpos = y + 1;
			tor->tchans[c].span = x;
			tor->tchans[c].tor = tor;
		}
	}
}

static int __devinit tor2_launch(struct tor2 *tor)
{
	int res;
	struct dahdi_span *s;
	int i;

	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &tor->tspans[0].dahdi_span.flags))
		return 0;

	tor->ddev = dahdi_create_device();
	tor->ddev->location = kasprintf(GFP_KERNEL, "PCI Bus %02d Slot %02d",
				       tor->pci->bus->number,
				       PCI_SLOT(tor->pci->devfn) + 1);

	if (!tor->ddev->location)
		return -ENOMEM;

	printk(KERN_INFO "Tor2: Launching card: %d\n", tor->order);
	for (i = 0; i < SPANS_PER_CARD; ++i) {
		s = &tor->tspans[i].dahdi_span;
		list_add_tail(&s->device_node, &tor->ddev->spans);
	}

	res = dahdi_register_device(tor->ddev, &tor->pci->dev);
	if (res) {
		dev_err(&tor->pci->dev, "Unable to register with DAHDI.\n");
		return res;
	}
	writew(PLX_INTENA, &tor->plx[INTCSR]); /* enable PLX interrupt */

#ifdef ENABLE_TASKLETS
	tasklet_init(&tor->tor2_tlet, tor2_tasklet, (unsigned long)tor);
#endif
	return 0;
}

static void free_tor(struct tor2 *tor)
{
	unsigned int x, f;

	for (x = 0; x < SPANS_PER_CARD; x++) {
		for (f = 0; f < (tor->cardtype == TYPE_E1 ? 31 : 24); f++) {
			if (tor->chans[x][f]) {
				kfree(tor->chans[x][f]);
			}
		}
		if (tor->chans[x])
			kfree(tor->chans[x]);
	}
	kfree(tor->ddev->location);
	dahdi_free_device(tor->ddev);
	kfree(tor);
}

static int __devinit tor2_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int res,x,f;
	int ret = -ENODEV;
	struct tor2 *tor;
	unsigned long endjif;
	__iomem volatile unsigned long *gpdata_io, *lasdata_io;
	unsigned long gpdata,lasdata;

	res = pci_enable_device(pdev);
	if (res)
		return res;
	tor = kmalloc(sizeof(struct tor2), GFP_KERNEL);
	if (!tor)
		return -ENOMEM;
	memset(tor,0,sizeof(struct tor2));
	spin_lock_init(&tor->lock);
	/* Load the resources */
	tor->pci = pdev;
	tor->irq = pdev->irq;
	if (tor->irq < 1) {
		printk(KERN_ERR "No IRQ allocated for device\n");
		goto err_out_free_tor;
	}
	tor->plx_region = pci_resource_start(pdev, 0);
	tor->plx_len = pci_resource_len(pdev, 0);
	tor->plx = ioremap(tor->plx_region, tor->plx_len);
	/* We don't use the I/O space, so we dont do anything with section 1 */
	tor->xilinx32_region = pci_resource_start(pdev, 2);
	tor->xilinx32_len = pci_resource_len(pdev, 2);
	tor->mem32 = ioremap(tor->xilinx32_region, tor->xilinx32_len);
	tor->xilinx8_region = pci_resource_start(pdev, 3);
	tor->xilinx8_len = pci_resource_len(pdev, 3);
	tor->mem8 = ioremap(tor->xilinx8_region, tor->xilinx8_len);
	/* Record what type */
	tor->type = (char *)ent->driver_data;
	/* Verify existence and accuracy of resources */
	if (!tor->plx_region || !tor->plx ||
	    (pci_resource_flags(pdev, 0) & IORESOURCE_IO)) {
		printk(KERN_ERR "Invalid PLX 9030 Base resource\n");
		goto err_out_free_tor;
	}
	if (!tor->xilinx32_region || !tor->mem32 ||
	    (pci_resource_flags(pdev, 2) & IORESOURCE_IO)) {
		printk(KERN_ERR "Invalid Xilinx 32 bit Base resource\n");
		goto err_out_free_tor;
	}
	if (!tor->xilinx8_region || !tor->mem8 ||
	    (pci_resource_flags(pdev, 3) & IORESOURCE_IO)) {
		printk(KERN_ERR "Invalid Xilinx 8 bit Base resource\n");
		goto err_out_free_tor;
	}
	/* Request regions */
	if (!request_mem_region(tor->plx_region, tor->plx_len, tor->type)) {
		printk(KERN_ERR "Unable to reserve PLX memory %08lx window at %08lx\n",
		       tor->plx_len, tor->plx_region);
		goto err_out_free_tor;
	}
	if (!request_mem_region(tor->xilinx32_region, tor->xilinx32_len, tor->type)) {
		printk(KERN_ERR "Unable to reserve Xilinx 32 bit memory %08lx window at %08lx\n",
		       tor->xilinx32_len, tor->xilinx32_region);
		goto err_out_release_plx_region;
	}
	if (!request_mem_region(tor->xilinx8_region, tor->xilinx8_len, tor->type)) {
		printk(KERN_ERR "Unable to reserve Xilinx memory %08lx window at %08lx\n",
		       tor->xilinx8_len, tor->xilinx8_region);
		goto err_out_release_plx_region;
	}
	pci_set_drvdata(pdev, tor);
	printk(KERN_INFO "Detected %s at 0x%lx/0x%lx irq %d\n", tor->type, 
		tor->xilinx32_region, tor->xilinx8_region,tor->irq);

	for (x = 0; x < MAX_TOR_CARDS; x++) {
		if (!cards[x]) break;
	}
	if (x >= MAX_TOR_CARDS) {
		printk(KERN_ERR "No cards[] slot available!!\n");
		goto err_out_release_all;
	}
	tor->num = x;
	cards[x] = tor;

	/* start programming mode */
	gpdata_io = (__iomem unsigned long *) &tor->plx[GPIOC];
	gpdata = readl(gpdata_io);

	gpdata |= GPIO_WRITE; /* make sure WRITE is not asserted */
	writel(gpdata, gpdata_io);

	gpdata &= ~GPIO_PROGRAM;  /* activate the PROGRAM signal */
	writel(gpdata, gpdata_io);

	/* wait for INIT and DONE to go low */
	endjif = jiffies + 10;
	while (readl(gpdata_io) & (GPIO_INIT | GPIO_DONE) &&
	      (jiffies <= endjif)) {
		;
	}

	if (endjif < jiffies) {
		printk(KERN_ERR "Timeout waiting for INIT and DONE to go low\n");
		goto err_out_release_all;
	}
	if (debug) printk(KERN_ERR "fwload: Init and done gone to low\n");
	gpdata |= GPIO_PROGRAM;
	writel(gpdata, gpdata_io);  /* de-activate the PROGRAM signal */
	/* wait for INIT to go high (clearing done */
	endjif = jiffies + 10;
	while (!(readl(gpdata_io) & GPIO_INIT) && (jiffies <= endjif))
		;
	if (endjif < jiffies) {
		printk(KERN_ERR "Timeout waiting for INIT to go high\n");
		goto err_out_release_all;
	}

	if (debug) printk(KERN_ERR "fwload: Init went high (clearing done)\nNow loading...\n");
	/* assert WRITE signal */
	gpdata &= ~GPIO_WRITE;
	writel(gpdata, gpdata_io);
	for (x = 0; x < sizeof(tor2fw); x++)
	   {
		  /* write the byte */
		writeb(tor2fw[x], tor->mem8);
		  /* if DONE signal, we're done, exit */
		if (readl(gpdata_io) & GPIO_DONE)
			break;
		  /* if INIT drops, we're screwed, exit */
		if (!(readl(gpdata_io) & GPIO_INIT))
			break;
	   }
	if (debug) printk(KERN_DEBUG "fwload: Transferred %d bytes into chip\n",x);
	/* Wait for FIFO to clear */
	endjif = jiffies + 2;
	while (jiffies < endjif); /* wait */
	  /* de-assert write signal */
	gpdata |= GPIO_WRITE;
	writel(gpdata, gpdata_io);
	if (debug) printk(KERN_DEBUG "fwload: Loading done!\n");	

	/* Wait for FIFO to clear */
	endjif = jiffies + 2;
	while (jiffies < endjif); /* wait */
	if (!(readl(gpdata_io) & GPIO_INIT))
	   {
		printk(KERN_ERR "Drove Init low!! CRC Error!!!\n");
		goto err_out_release_all;
	   }
	if (!(readl(gpdata_io) & GPIO_DONE))
	   {
		printk(KERN_ERR "Did not get DONE signal. Short file maybe??\n");
		goto err_out_release_all;
	   }
	printk(KERN_INFO "Xilinx Chip successfully loaded, configured and started!!\n");

	writeb(0, &tor->mem8[SYNCREG]);
	writeb(0, &tor->mem8[CTLREG]);
	writeb(0, &tor->mem8[CTLREG1]);
	writeb(0, &tor->mem8[LEDREG]);

	/* set the LA2BRD register so that we enable block transfer, read
	   pre-fetch, and set to maximum read pre-fetch size */
	lasdata_io = (__iomem unsigned long *) &tor->plx[LAS2BRD];
	lasdata = readl(lasdata_io);
	lasdata |= 0x39;
	writel(lasdata, lasdata_io);

	/* set the LA3BRD register so that we enable block transfer */
	lasdata_io = (__iomem unsigned long *) &tor->plx[LAS3BRD];
	lasdata = readl(lasdata_io);
	lasdata |= 1;
	writel(lasdata, lasdata_io);

	/* check part revision data */
	x = t1in(tor,1,0xf) & 15;
#ifdef	NONREVA
	if (x > 3)
	{
		tor->mem8[CTLREG1] = NONREVA;
	}
#endif
	for (x = 0; x < 256; x++)
		writel(0x7f7f7f7f, &tor->mem32[x]);

	if (request_irq(tor->irq, tor2_intr, DAHDI_IRQ_SHARED_DISABLED, "tor2", tor)) {
		printk(KERN_ERR "Unable to request tormenta IRQ %d\n", tor->irq);
		goto err_out_release_all;
	}

	if (t1in(tor,1,0xf) & 0x80) {
		printk(KERN_INFO "Tormenta 2 Quad E1/PRA Card\n");
		tor->cardtype = TYPE_E1;
		tor->datxlt = datxlt_e1;
	} else {
		printk(KERN_INFO "Tormenta 2 Quad T1/PRI Card\n");
		tor->cardtype = TYPE_T1;
		tor->datxlt = datxlt_t1;
	}

	for (x = 0; x < SPANS_PER_CARD; x++) {
		int num_chans = tor->cardtype == TYPE_E1 ? 31 : 24;
		
		if (!(tor->chans[x] = kmalloc(num_chans * sizeof(*tor->chans[x]), GFP_KERNEL))) {
			printk(KERN_ERR "tor2: Not enough memory for chans[%d]\n", x);
			ret = -ENOMEM;
			goto err_out_release_all;
		}
		for (f = 0; f < (num_chans); f++) {
			if (!(tor->chans[x][f] = kmalloc(sizeof(*tor->chans[x][f]), GFP_KERNEL))) {
				printk(KERN_ERR "tor2: Not enough memory for chans[%d][%d]\n", 
						x, f);
				ret = -ENOMEM;
				goto err_out_release_all;
			}
			memset(tor->chans[x][f], 0, sizeof(*tor->chans[x][f]));
		}
	}

	init_spans(tor); 

	tor->order = readb(&tor->mem8[SWREG]);
	printk(KERN_INFO "Detected Card number: %d\n", tor->order);

	/* Launch cards as appropriate */
	x = 0;
	for (;;) {
		/* Find a card to activate */
		f = 0;
		for (x=0;cards[x];x++) {
			if (cards[x]->order <= highestorder) {
				tor2_launch(cards[x]);
				if (cards[x]->order == highestorder)
					f = 1;
			}
		}
		/* If we found at least one, increment the highest order and search again, otherwise stop */
		if (f) 
			highestorder++;
		else
			break;
	}

	return 0;

err_out_release_all:
	release_mem_region(tor->xilinx32_region, tor->xilinx32_len);
	release_mem_region(tor->xilinx8_region, tor->xilinx8_len);
err_out_release_plx_region:
	release_mem_region(tor->plx_region, tor->plx_len);
err_out_free_tor:
	if (tor->plx) iounmap(tor->plx);
	if (tor->mem8) iounmap(tor->mem8);
	if (tor->mem32) iounmap(tor->mem32);
	if (tor) {
		free_tor(tor);
	}
	return ret;
}

static struct pci_driver tor2_driver;

static void __devexit tor2_remove(struct pci_dev *pdev)
{
	struct tor2 *tor;

	tor = pci_get_drvdata(pdev);
	if (!tor)
		BUG();
	writeb(0, &tor->mem8[SYNCREG]);
	writeb(0, &tor->mem8[CTLREG]);
	writeb(0, &tor->mem8[LEDREG]);
	writew(0, &tor->plx[INTCSR]);
	free_irq(tor->irq, tor);
	dahdi_unregister_device(tor->ddev);
	release_mem_region(tor->plx_region, tor->plx_len);
	release_mem_region(tor->xilinx32_region, tor->xilinx32_len);
	release_mem_region(tor->xilinx8_region, tor->xilinx8_len);
	if (tor->plx) iounmap(tor->plx);
	if (tor->mem8) iounmap(tor->mem8);
	if (tor->mem32) iounmap(tor->mem32);

	cards[tor->num] = NULL;
	pci_set_drvdata(pdev, NULL);
	free_tor(tor);
}

static struct pci_driver tor2_driver = {
	.name = "tormenta2",
	.probe = tor2_probe,
	.remove = __devexit_p(tor2_remove),
	.id_table = tor2_pci_ids,
};

static int __init tor2_init(void) {
	int res;
	res = dahdi_pci_module(&tor2_driver);
	printk(KERN_INFO "Registered Tormenta2 PCI\n");
	return res;
}

static void __exit tor2_cleanup(void) {
	pci_unregister_driver(&tor2_driver);
	printk(KERN_INFO "Unregistered Tormenta2\n");
}

static void set_clear(struct tor2 *tor)
{
	int i,j,s;
	unsigned short val=0;
	for (s = 0; s < SPANS_PER_CARD; s++) {
		struct dahdi_span *span = &tor->tspans[s].dahdi_span;
		for (i = 0; i < 24; i++) {
			j = (i/8);
			if (span->chans[i]->flags & DAHDI_FLAG_CLEAR)
				val |= 1 << (i % 8);

			if ((i % 8)==7) {
#if 0
				printk(KERN_DEBUG "Putting %d in register %02x on span %d\n",
				       val, 0x39 + j, 1 + s);
#endif
				t1out(tor,1 + s, 0x39 + j, val);
				val = 0;
			}
		}
	}
		
}


static int tor2_rbsbits(struct dahdi_chan *chan, int bits)
{
	u_char m,c;
	int k,n,b;
	struct tor2_chan *p = chan->pvt;
	unsigned long flags;
#if 0
	printk(KERN_DEBUG "Setting bits to %d on channel %s\n", bits, chan->name);
#endif	
	if (p->tor->cardtype == TYPE_E1) { /* do it E1 way */
		if (chan->chanpos == 16) return 0;
		n = chan->chanpos - 1;
		if (chan->chanpos > 16) n--;
		k = p->span;
		b = (n % 15) + 1;
		c = p->tor->txsigs[k][b];
		m = (n / 15) * 4; /* nibble selector */
		c &= (15 << m); /* keep the other nibble */
		c |= (bits & 15) << (4 - m); /* put our new nibble here */
		p->tor->txsigs[k][b] = c;
		  /* output them to the chip */
		t1out(p->tor,k + 1,0x40 + b,c); 
		return 0;
	}						
	n = chan->chanpos - 1;
	k = p->span;
	b = (n / 8); /* get byte number */
	m = 1 << (n & 7); /* get mask */
	c = p->tor->txsigs[k][b];
	c &= ~m;  /* clear mask bit */
	  /* set mask bit, if bit is to be set */
	if (bits & DAHDI_ABIT) c |= m;
	p->tor->txsigs[k][b] = c;
	spin_lock_irqsave(&p->tor->lock, flags);	
	t1out(p->tor,k + 1,0x70 + b,c);
	b += 3; /* now points to b bit stuff */
	  /* get current signalling values */
	c = p->tor->txsigs[k][b];
	c &= ~m;  /* clear mask bit */
	  /* set mask bit, if bit is to be set */
	if (bits & DAHDI_BBIT) c |= m;
	  /* save new signalling values */
	p->tor->txsigs[k][b] = c;
	  /* output them into the chip */
	t1out(p->tor,k + 1,0x70 + b,c);
	b += 3; /* now points to c bit stuff */
	  /* get current signalling values */
	c = p->tor->txsigs[k][b];
	c &= ~m;  /* clear mask bit */
	  /* set mask bit, if bit is to be set */
	if (bits & DAHDI_CBIT) c |= m;
	  /* save new signalling values */
	p->tor->txsigs[k][b] = c;
	  /* output them into the chip */
	t1out(p->tor,k + 1,0x70 + b,c);
	b += 3; /* now points to d bit stuff */
	  /* get current signalling values */
	c = p->tor->txsigs[k][b];
	c &= ~m;  /* clear mask bit */
	  /* set mask bit, if bit is to be set */
	if (bits & DAHDI_DBIT) c |= m;
	  /* save new signalling values */
	p->tor->txsigs[k][b] = c;
	  /* output them into the chip */
	t1out(p->tor,k + 1,0x70 + b,c);
	spin_unlock_irqrestore(&p->tor->lock, flags);
	return 0;
}

static int tor2_shutdown(struct dahdi_span *span)
{
	int i;
	int tspan;
	int wasrunning;
	unsigned long flags;
	struct tor2_span *const p = container_of(span, struct tor2_span, dahdi_span);
	struct tor2 *const tor = p->tor;

	tspan = p->span + 1;
	if (tspan < 0) {
		printk(KERN_DEBUG "Tor2: Span '%d' isn't us?\n", span->spanno);
		return -1;
	}

	spin_lock_irqsave(&tor->lock, flags);
	wasrunning = span->flags & DAHDI_FLAG_RUNNING;

	span->flags &= ~DAHDI_FLAG_RUNNING;
	/* Zero out all registers */
	if (tor->cardtype == TYPE_E1) {
		for (i = 0; i < 192; i++)
			t1out(tor, tspan, i, 0);
	} else {
		for (i = 0; i < 160; i++)
			t1out(tor, tspan, i, 0);
	}
	if (wasrunning)
		tor->spansstarted--;
	spin_unlock_irqrestore(&tor->lock, flags);
	if (!(tor->tspans[0].dahdi_span.flags & DAHDI_FLAG_RUNNING) &&
	    !(tor->tspans[1].dahdi_span.flags & DAHDI_FLAG_RUNNING) &&
	    !(tor->tspans[2].dahdi_span.flags & DAHDI_FLAG_RUNNING) &&
	    !(tor->tspans[3].dahdi_span.flags & DAHDI_FLAG_RUNNING))
		/* No longer in use, disable interrupts */
		writeb(0, &tor->mem8[CTLREG]);
	if (debug)
		printk(KERN_DEBUG"Span %d (%s) shutdown\n", span->spanno, span->name);
	return 0;
}


static int tor2_startup(struct file *file, struct dahdi_span *span)
{
	unsigned long endjif;
	int i;
	int tspan;
	unsigned long flags;
	char *coding;
	char *framing;
	char *crcing;
	int alreadyrunning;
	struct tor2_span *p = container_of(span, struct tor2_span, dahdi_span);

	tspan = p->span + 1;
	if (tspan < 0) {
		printk(KERN_DEBUG "Tor2: Span '%d' isn't us?\n", span->spanno);
		return -1;
	}

	spin_lock_irqsave(&p->tor->lock, flags);

	alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

	/* initialize the start value for the entire chunk of last ec buffer */
	for (i = 0; i < span->channels; i++)
	{
		memset(p->tor->ec_chunk1[p->span][i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
		memset(p->tor->ec_chunk2[p->span][i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
	}
	/* Force re-evaluation of the timing source */
	if (timingcable)
		p->tor->syncsrc = -1;

	if (p->tor->cardtype == TYPE_E1) { /* if this is an E1 card */
		unsigned char tcr1,ccr1,tcr2;
		if (!alreadyrunning) {
			writeb(SYNCSELF, &p->tor->mem8[SYNCREG]);
			writeb(E1DIV, &p->tor->mem8[CTLREG]);
			writeb(0, &p->tor->mem8[LEDREG]);
			/* Force re-evaluation of sync src */
			/* Zero out all registers */
			for (i = 0; i < 192; i++) 
				t1out(p->tor,tspan, i, 0);
		
			/* Set up for Interleaved Serial Bus operation in byte mode */
			/* Set up all the spans every time, so we are sure they are
                           in a consistent state. If we don't, a card without all
                           its spans configured misbehaves in strange ways. */
			t1out(p->tor,1,0xb5,9); 
			t1out(p->tor,2,0xb5,8);
			t1out(p->tor,3,0xb5,8);
			t1out(p->tor,4,0xb5,8);

			t1out(p->tor,tspan,0x1a,4); /* CCR2: set LOTCMC */
			for (i = 0; i <= 8; i++) t1out(p->tor,tspan,i,0);
			for (i = 0x10; i <= 0x4f; i++) if (i != 0x1a) t1out(p->tor,tspan,i,0);
			t1out(p->tor,tspan,0x10,0x20); /* RCR1: Rsync as input */
			t1out(p->tor,tspan,0x11,6); /* RCR2: Sysclk=2.048 Mhz */
			t1out(p->tor,tspan,0x12,9); /* TCR1: TSiS mode */
		}
		ccr1 = 0;
		crcing = "";
		tcr1 = 9; /* base TCR1 value: TSis mode */
		tcr2 = 0;
		if (span->lineconfig & DAHDI_CONFIG_CCS) {
			ccr1 |= 8; /* CCR1: Rx Sig mode: CCS */
			coding = "CCS";
		} else {
			tcr1 |= 0x20; 
			coding = "CAS";
		}
		if (span->lineconfig & DAHDI_CONFIG_HDB3) {
			ccr1 |= 0x44; /* CCR1: TX and RX HDB3 */
			framing = "HDB3";
		} else framing = "AMI";
		if (span->lineconfig & DAHDI_CONFIG_CRC4) {
			ccr1 |= 0x11; /* CCR1: TX and TX CRC4 */
			tcr2 |= 0x02; /* TCR2: CRC4 bit auto */
			crcing = "/CRC4";
		} 
		t1out(p->tor,tspan,0x12,tcr1);
		t1out(p->tor,tspan,0x13,tcr2);
		t1out(p->tor,tspan,0x14,ccr1);
		t1out(p->tor,tspan, 0x18, 0x20); /* 120 Ohm, normal */

		if (!alreadyrunning) {
			t1out(p->tor,tspan,0x1b,0x8a); /* CCR3: LIRST & TSCLKM */
			t1out(p->tor,tspan,0x20,0x1b); /* TAFR */
			t1out(p->tor,tspan,0x21,0x5f); /* TNAFR */
			t1out(p->tor,tspan,0x40,0xb); /* TSR1 */
			for (i = 0x41; i <= 0x4f; i++) t1out(p->tor,tspan,i,0x55);
			for (i = 0x22; i <= 0x25; i++) t1out(p->tor,tspan,i,0xff);
			/* Wait 100 ms */
			endjif = jiffies + 10;
			spin_unlock_irqrestore(&p->tor->lock, flags);
			while (jiffies < endjif); /* wait 100 ms */
			spin_lock_irqsave(&p->tor->lock, flags);
			t1out(p->tor,tspan,0x1b,0x9a); /* CCR3: set also ESR */
			t1out(p->tor,tspan,0x1b,0x82); /* CCR3: TSCLKM only now */
			
			span->flags |= DAHDI_FLAG_RUNNING;
			p->tor->spansstarted++;

			/* enable interrupts */
			writeb(INTENA | E1DIV, &p->tor->mem8[CTLREG]);
		}

		spin_unlock_irqrestore(&p->tor->lock, flags);

		if (debug) {
			if (alreadyrunning) 
				printk(KERN_INFO "Tor2: Reconfigured span %d (%s/%s%s) 120 Ohms\n", span->spanno, coding, framing, crcing);
			else
				printk(KERN_INFO "Tor2: Startup span %d (%s/%s%s) 120 Ohms\n", span->spanno, coding, framing, crcing);
		}
	} else { /* is a T1 card */

		if (!alreadyrunning) {
			writeb(SYNCSELF, &p->tor->mem8[SYNCREG]);
			writeb(0, &p->tor->mem8[CTLREG]);
			writeb(0, &p->tor->mem8[LEDREG]);
			/* Zero out all registers */
			for (i = 0; i < 160; i++) 
				t1out(p->tor,tspan, i, 0);
		
			/* Set up for Interleaved Serial Bus operation in byte mode */
			/* Set up all the spans every time, so we are sure they are
                           in a consistent state. If we don't, a card without all
                           its spans configured misbehaves in strange ways. */
			t1out(p->tor,1,0x94,9); 
			t1out(p->tor,2,0x94,8);
			t1out(p->tor,3,0x94,8);
			t1out(p->tor,4,0x94,8);
			/* Full-on Sync required (RCR1) */
			t1out(p->tor,tspan, 0x2b, 8);	
			/* RSYNC is an input (RCR2) */
			t1out(p->tor,tspan, 0x2c, 8);	
			/* RBS enable (TCR1) */
			t1out(p->tor,tspan, 0x35, 0x10);
			/* TSYNC to be output (TCR2) */
			t1out(p->tor,tspan, 0x36, 4);
			/* Tx & Rx Elastic store, sysclk(s) = 2.048 mhz, loopback controls (CCR1) */
			t1out(p->tor,tspan, 0x37, 0x9c); 
			/* Set up received loopup and loopdown codes */
			t1out(p->tor,tspan, 0x12, 0x22); 
			t1out(p->tor,tspan, 0x14, 0x80); 
			t1out(p->tor,tspan, 0x15, 0x80); 
			/* Setup japanese mode if appropriate */
			t1out(p->tor,tspan,0x19,(japan ? 0x80 : 0x00)); /* no local loop */
			t1out(p->tor,tspan,0x1e,(japan ? 0x80 : 0x00)); /* no local loop */
		}
		/* Enable F bits pattern */
		i = 0x20;
		if (span->lineconfig & DAHDI_CONFIG_ESF)
			i = 0x88;
		if (span->lineconfig & DAHDI_CONFIG_B8ZS)
			i |= 0x44;
		t1out(p->tor,tspan, 0x38, i);
		if (i & 0x80)
			coding = "ESF";
		else
			coding = "SF";
		if (i & 0x40)
			framing = "B8ZS";
		else {
			framing = "AMI";
			t1out(p->tor,tspan,0x7e,0x1c); /* F bits pattern (0x1c) into FDL register */
		}
		t1out(p->tor,tspan, 0x7c, span->txlevel << 5);
	
		if (!alreadyrunning) {	
			/* LIRST to reset line interface */
			t1out(p->tor,tspan, 0x0a, 0x80);
	
			/* Wait 100 ms */
			endjif = jiffies + 10;

			spin_unlock_irqrestore(&p->tor->lock, flags);
	
			while (jiffies < endjif); /* wait 100 ms */

			spin_lock_irqsave(&p->tor->lock, flags);
	
			t1out(p->tor,tspan,0x0a,0x30); /* LIRST back to normal, Resetting elastic stores */

			span->flags |= DAHDI_FLAG_RUNNING;
			p->tor->spansstarted++;

			/* enable interrupts */
			writeb(INTENA, &p->tor->mem8[CTLREG]);
		}

		set_clear(p->tor);

		spin_unlock_irqrestore(&p->tor->lock, flags);

		if (debug) {
			if (alreadyrunning) 
				printk(KERN_INFO "Tor2: Reconfigured span %d (%s/%s) LBO: %s\n", span->spanno, coding, framing, dahdi_lboname(span->txlevel));
			else
				printk(KERN_INFO "Tor2: Startup span %d (%s/%s) LBO: %s\n", span->spanno, coding, framing, dahdi_lboname(span->txlevel));
		}
	}
	if (p->tor->syncs[0] == span->spanno) printk(KERN_INFO "SPAN %d: Primary Sync Source\n",span->spanno);
	if (p->tor->syncs[1] == span->spanno) printk(KERN_INFO "SPAN %d: Secondary Sync Source\n",span->spanno);
	if (p->tor->syncs[2] == span->spanno) printk(KERN_INFO "SPAN %d: Tertiary Sync Source\n",span->spanno);
	if (p->tor->syncs[3] == span->spanno) printk(KERN_INFO "SPAN %d: Quaternary Sync Source\n",span->spanno);
	return 0;
}

static int tor2_maint(struct dahdi_span *span, int cmd)
{
	struct tor2_span *p = container_of(span, struct tor2_span, dahdi_span);

	int tspan = p->span + 1;

	if (p->tor->cardtype == TYPE_E1)
	   {
		switch(cmd) {
		    case DAHDI_MAINT_NONE:
			t1out(p->tor,tspan,0xa8,0); /* no loops */
			break;
		    case DAHDI_MAINT_LOCALLOOP:
			t1out(p->tor,tspan,0xa8,0x40); /* local loop */
			break;
		    case DAHDI_MAINT_REMOTELOOP:
			t1out(p->tor,tspan,0xa8,0x80); /* remote loop */
			break;
		    case DAHDI_MAINT_LOOPUP:
		    case DAHDI_MAINT_LOOPDOWN:
			return -ENOSYS;
		    default:
			printk(KERN_NOTICE "Tor2: Unknown maint command: %d\n", cmd);
			break;
		}
		return 0;
	}
	switch(cmd) {
    case DAHDI_MAINT_NONE:
	t1out(p->tor,tspan,0x19,(japan ? 0x80 : 0x00)); /* no local loop */
	t1out(p->tor,tspan,0x0a,0); /* no remote loop */
	t1out(p->tor, tspan, 0x30, 0);	/* stop sending loopup code */
	break;
    case DAHDI_MAINT_LOCALLOOP:
	t1out(p->tor,tspan,0x19,0x40 | (japan ? 0x80 : 0x00)); /* local loop */
	t1out(p->tor,tspan,0x0a,0); /* no remote loop */
	break;
    case DAHDI_MAINT_REMOTELOOP:
	t1out(p->tor,tspan,0x1e,(japan ? 0x80 : 0x00)); /* no local loop */
	t1out(p->tor,tspan,0x0a,0x40); /* remote loop */
	break;
    case DAHDI_MAINT_LOOPUP:
	t1out(p->tor,tspan,0x30,2); /* send loopup code */
	t1out(p->tor,tspan,0x12,0x22); /* send loopup code */
	t1out(p->tor,tspan,0x13,0x80); /* send loopup code */
	break;
    case DAHDI_MAINT_LOOPDOWN:
	t1out(p->tor,tspan,0x30,2); /* send loopdown code */
	t1out(p->tor,tspan,0x12,0x62); /* send loopdown code */
	t1out(p->tor,tspan,0x13,0x90); /* send loopdown code */
	break;
    default:
	printk(KERN_NOTICE "Tor2: Unknown maint command: %d\n", cmd);
	break;
   }
    return 0;
}

static inline void tor2_run(struct tor2 *tor)
{
	int x,y;
	for (x = 0; x < SPANS_PER_CARD; x++) {
		struct dahdi_span *const s = &tor->tspans[x].dahdi_span;
		if (s->flags & DAHDI_FLAG_RUNNING) {
			/* since the Tormenta 2 PCI is double-buffered, you
			   need to delay the transmit data 2 entire chunks so
			   that the transmit will be in sync with the receive */
			for (y = 0; y < s->channels; y++) {
				dahdi_ec_chunk(s->chans[y],
					       s->chans[y]->readchunk,
					       tor->ec_chunk2[x][y]);
				memcpy(tor->ec_chunk2[x][y],tor->ec_chunk1[x][y],
					DAHDI_CHUNKSIZE);
				memcpy(tor->ec_chunk1[x][y],
					s->chans[y]->writechunk,
						DAHDI_CHUNKSIZE);
			}
			dahdi_receive(s);
		}
	}
	for (x = 0; x < SPANS_PER_CARD; x++) {
		struct dahdi_span *const s = &tor->tspans[x].dahdi_span;
		if (s->flags & DAHDI_FLAG_RUNNING)
			dahdi_transmit(s);
	}
}

#ifdef ENABLE_TASKLETS
static void tor2_tasklet(unsigned long data)
{
	struct tor2 *tor = (struct tor2 *)data;
	tor->taskletrun++;
	if (tor->taskletpending) {
		tor->taskletexec++;
		tor2_run(tor);
	}
	tor->taskletpending = 0;
}
#endif

static int syncsrc = 0;
static int syncnum = 0 /* -1 */;
static int syncspan = 0;
static DEFINE_SPINLOCK(synclock);

static int tor2_findsync(struct tor2 *tor)
{
	int i;
	int x;
	unsigned long flags;
	int p;
	int nonzero;
	int newsyncsrc = 0;			/* DAHDI span number */
	int newsyncnum = 0;			/* tor2 card number */
	int newsyncspan = 0;		/* span on given tor2 card */
	spin_lock_irqsave(&synclock, flags);
#if 1
	if (!tor->num) {
		/* If we're the first card, go through all the motions, up to 8 levels
		   of sync source */
		p = 1;
		while (p < 8) {
			nonzero = 0;
			for (x=0;cards[x];x++) {
				for (i = 0; i < SPANS_PER_CARD; i++) {
					if (cards[x]->syncpos[i]) {
						nonzero = 1;
						if ((cards[x]->syncpos[i] == p) &&
						    !(cards[x]->tspans[i].dahdi_span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_LOOPBACK)) &&
							(cards[x]->tspans[i].dahdi_span.flags & DAHDI_FLAG_RUNNING)) {
								/* This makes a good sync source */
								newsyncsrc = cards[x]->tspans[i].dahdi_span.spanno;
								newsyncnum = x;
								newsyncspan = i + 1;
								/* Jump out */
								goto found;
						}
					}
				}		
			}
			if (nonzero)
				p++;
			else 
				break;
		}
found:		
		if ((syncnum != newsyncnum) || (syncsrc != newsyncsrc) || (newsyncspan != syncspan)) {
			syncnum = newsyncnum;
			syncsrc = newsyncsrc;
			syncspan = newsyncspan;
			if (debug) printk(KERN_DEBUG "New syncnum: %d, syncsrc: %d, syncspan: %d\n", syncnum, syncsrc, syncspan);
		}
	}
#endif	
	/* update sync src info */
	if (tor->syncsrc != syncsrc) {
		tor->syncsrc = syncsrc;
		/* Update sync sources */
		for (i = 0; i < SPANS_PER_CARD; i++) {
			tor->tspans[i].dahdi_span.syncsrc = tor->syncsrc;
		}
		if (syncnum == tor->num) {
#if 1
			/* actually set the sync register */
			writeb(syncspan, &tor->mem8[SYNCREG]);
#endif			
			if (debug) printk(KERN_DEBUG "Card %d, using sync span %d, master\n", tor->num, syncspan);
			tor->master = MASTER;	
		} else {
#if 1
			/* time from the timing cable */
			writeb(SYNCEXTERN, &tor->mem8[SYNCREG]);
#endif			
			tor->master = 0;
			if (debug) printk(KERN_DEBUG "Card %d, using Timing Bus, NOT master\n", tor->num);	
		}
	}
	spin_unlock_irqrestore(&synclock, flags);
	return 0;
}

DAHDI_IRQ_HANDLER(tor2_intr)
{
	int n, i, j, k, newsyncsrc;
	unsigned int rxword,txword;

	unsigned char c, rxc;
	unsigned char abits, bbits;
	struct tor2 *tor = (struct tor2 *) dev_id;	

	  /* make sure its a real interrupt for us */
	if (!(readb(&tor->mem8[STATREG]) & INTACTIVE)) /* if not, just return */
	   {
		return IRQ_NONE;
	   }

	if (tor->cardtype == TYPE_E1) {
		/* set outbit, interrupt enable, and ack interrupt */
		writeb(OUTBIT | INTENA | INTACK | E1DIV | tor->master,
		       &tor->mem8[CTLREG]);
	} else {
		/* set outbit, interrupt enable, and ack interrupt */
		writeb(OUTBIT | INTENA | INTACK | tor->master,
		       &tor->mem8[CTLREG]);
	}

#if	0
	if (!tor->passno)
		printk(KERN_DEBUG "Interrupt handler\n");
#endif

	/* do the transmit output */
	for (n = 0; n < tor->tspans[0].dahdi_span.channels; n++) {
		for (i = 0; i < DAHDI_CHUNKSIZE; i++) {
			/* span 1 */
			txword = tor->tspans[0].dahdi_span.chans[n]->writechunk[i] << 24;
			/* span 2 */
			txword |= tor->tspans[1].dahdi_span.chans[n]->writechunk[i] << 16;
			/* span 3 */
			txword |= tor->tspans[2].dahdi_span.chans[n]->writechunk[i] << 8;
			/* span 4 */
			txword |= tor->tspans[3].dahdi_span.chans[n]->writechunk[i];
			/* write to part */
#ifdef FIXTHISFOR64
			tor->mem32[tor->datxlt[n] + (32 * i)] = txword;
#else
			writel(txword, &tor->mem32[tor->datxlt[n] + (32 * i)]);
#endif
		}
	}

	/* Do the receive input */
	for (n = 0; n < tor->tspans[0].dahdi_span.channels; n++) {
		for (i = 0; i < DAHDI_CHUNKSIZE; i++) {
			/* read from */
#ifdef FIXTHISFOR64
		        rxword = tor->mem32[tor->datxlt[n] + (32 * i)];
#else
			rxword = readl(&tor->mem32[tor->datxlt[n] + (32 * i)]);
#endif
			/* span 1 */
			tor->tspans[0].dahdi_span.chans[n]->readchunk[i] = rxword >> 24;
			/* span 2 */
			tor->tspans[1].dahdi_span.chans[n]->readchunk[i] = (rxword & 0xff0000) >> 16;
			/* span 3 */
			tor->tspans[2].dahdi_span.chans[n]->readchunk[i] = (rxword & 0xff00) >> 8;
			/* span 4 */
			tor->tspans[3].dahdi_span.chans[n]->readchunk[i] = rxword & 0xff;
		}
	}

	i = tor->passno & 15;
	/* if an E1 card, do rx signalling for it */
	if ((i < 3) && (tor->cardtype == TYPE_E1)) { /* if an E1 card */
		for (j = (i * 5); j < (i * 5) + 5; j++) {
			for (k = 1; k <= SPANS_PER_CARD; k++) {
				c = t1in(tor,k,0x31 + j);
				rxc = c & 15;
				if (rxc != tor->tspans[k - 1].dahdi_span.chans[j + 16]->rxsig) {
					/* Check for changes in received bits */
					if (!(tor->tspans[k - 1].dahdi_span.chans[j + 16]->sig & DAHDI_SIG_CLEAR))
						dahdi_rbsbits(tor->tspans[k - 1].dahdi_span.chans[j + 16], rxc);
				}
				rxc = c >> 4;
				if (rxc != tor->tspans[k - 1].dahdi_span.chans[j]->rxsig) {
					/* Check for changes in received bits */
					if (!(tor->tspans[k - 1].dahdi_span.chans[j]->sig & DAHDI_SIG_CLEAR))
						dahdi_rbsbits(tor->tspans[k - 1].dahdi_span.chans[j], rxc);
				}
			}
		}
	}

	  /* if a T1, do the signalling */
	if ((i < 12) && (tor->cardtype == TYPE_T1)) {
		k = (i / 3);	/* get span */
		n = (i % 3);	/* get base */
		abits = t1in(tor,k + 1, 0x60 + n);
		bbits = t1in(tor,k + 1, 0x63 + n);
		for (j=0; j< 8; j++) {
			/* Get channel number */
			i = (n * 8) + j;
			rxc = 0;
			if (abits & (1 << j)) rxc |= DAHDI_ABIT;
			if (bbits & (1 << j)) rxc |= DAHDI_BBIT;
			if (tor->tspans[k].dahdi_span.chans[i]->rxsig != rxc) {
				/* Check for changes in received bits */
				if (!(tor->tspans[k].dahdi_span.chans[i]->sig & DAHDI_SIG_CLEAR))
					dahdi_rbsbits(tor->tspans[k].dahdi_span.chans[i], rxc);
			}
		}
	}

	for (i = 0; i < SPANS_PER_CARD; i++) { /* Go thru all the spans */
		  /* if alarm timer, and it's timed out */
		if (tor->alarmtimer[i]) {
			if (!--tor->alarmtimer[i]) {
				  /* clear recover status */
				tor->tspans[i].dahdi_span.alarms &= ~DAHDI_ALARM_RECOVER;
				if (tor->cardtype == TYPE_E1)
					t1out(tor,i + 1,0x21,0x5f); /* turn off yel */
				else 
					t1out(tor,i + 1,0x35,0x10); /* turn off yel */
				dahdi_alarm_notify(&tor->tspans[i].dahdi_span);  /* let them know */
			   }
		}
	}

	i = tor->passno & 15;
	if ((i >= 10) && (i <= 13) && !(tor->passno & 0x30))
	   {
		j = 0;  /* clear this alarm status */
		i -= 10;
		if (tor->cardtype == TYPE_T1) {
			c = t1in(tor,i + 1,0x31); /* get RIR2 */
			tor->tspans[i].dahdi_span.rxlevel = c >> 6;  /* get rx level */
			t1out(tor,i + 1,0x20,0xff); 
			c = t1in(tor,i + 1,0x20);  /* get the status */
			  /* detect the code, only if we are not sending one */
			if ((!tor->tspans[i].dahdi_span.mainttimer) && (c & 0x80))  /* if loop-up code detected */
			   {
				  /* set into remote loop, if not there already */
				if ((tor->loopupcnt[i]++ > 80) && 
					(tor->tspans[i].dahdi_span.maintstat != DAHDI_MAINT_REMOTELOOP))
				   {
					t1out(tor,i + 1,0x1e,(japan ? 0x80 : 0x00)); /* no local loop */
					t1out(tor,i + 1,0x0a,0x40); /* remote loop */
					tor->tspans[i].dahdi_span.maintstat = DAHDI_MAINT_REMOTELOOP;
				   }
			   } else tor->loopupcnt[i] = 0;
			  /* detect the code, only if we are not sending one */
			if ((!tor->tspans[i].dahdi_span.mainttimer) && (c & 0x40))  /* if loop-down code detected */
			   {
				  /* if in remote loop, get out of it */
				if ((tor->loopdowncnt[i]++ > 80) &&
					(tor->tspans[i].dahdi_span.maintstat == DAHDI_MAINT_REMOTELOOP))
				   {
					t1out(tor,i + 1,0x1e,(japan ? 0x80 : 0x00)); /* no local loop */
					t1out(tor,i + 1,0x0a,0); /* no remote loop */
					tor->tspans[i].dahdi_span.maintstat = DAHDI_MAINT_NONE;
				   }
			   } else tor->loopdowncnt[i] = 0;
			if (c & 3) /* if red alarm */
			   {
				j |= DAHDI_ALARM_RED;
			   }
			if (c & 8) /* if blue alarm */
			   {
				j |= DAHDI_ALARM_BLUE;
			   }
		} else { /* its an E1 card */
			t1out(tor,i + 1,6,0xff); 
			c = t1in(tor,i + 1,6);  /* get the status */
			if (c & 9) /* if red alarm */
			   {
				j |= DAHDI_ALARM_RED;
			   }
			if (c & 2) /* if blue alarm */
			   {
				j |= DAHDI_ALARM_BLUE;
			   }
		}
		  /* only consider previous carrier alarm state */
		tor->tspans[i].dahdi_span.alarms &= (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_NOTOPEN);
		n = 1; /* set to 1 so will not be in yellow alarm if we dont
			care about open channels */
		/* if to have yellow alarm if nothing open */
		if (tor->tspans[i].dahdi_span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
			/* go thru all chans, and count # open */
			for (n = 0, k = 0; k < tor->tspans[i].dahdi_span.channels; k++) {
				if (((tor->chans[i][k])->flags & DAHDI_FLAG_OPEN) ||
				    dahdi_have_netdev(tor->chans[i][k]))
					n++;
			}
			/* if none open, set alarm condition */
			if (!n)
				j |= DAHDI_ALARM_NOTOPEN;
		}
		  /* if no more alarms, and we had some */
		if ((!j) && tor->tspans[i].dahdi_span.alarms)
			tor->alarmtimer[i] = DAHDI_ALARMSETTLE_TIME; 
		if (tor->alarmtimer[i]) j |= DAHDI_ALARM_RECOVER;
		  /* if going into alarm state, set yellow alarm */
		if ((j) && (!tor->tspans[i].dahdi_span.alarms)) {
			if (tor->cardtype == TYPE_E1)
				t1out(tor,i + 1,0x21,0x7f);
			else
				t1out(tor,i + 1,0x35,0x11);
		}
		if (c & 4) /* if yellow alarm */
			j |= DAHDI_ALARM_YELLOW;
		if (tor->tspans[i].dahdi_span.maintstat || tor->tspans[i].dahdi_span.mainttimer)
			j |= DAHDI_ALARM_LOOPBACK;
		tor->tspans[i].dahdi_span.alarms = j;
		c = (LEDRED | LEDGREEN) << (2 * i);
		tor->leds &= ~c;  /* mask out bits for this span */
		/* light LED's if span configured and running */
		if (tor->tspans[i].dahdi_span.flags & DAHDI_FLAG_RUNNING) {
			if (j & DAHDI_ALARM_RED) tor->leds |= LEDRED << (2 * i);
			else if (j & DAHDI_ALARM_YELLOW) tor->leds |= (LEDRED | LEDGREEN) << (2 * i);
			else tor->leds |= LEDGREEN << (2 * i);
		}
		writeb(tor->leds, &tor->mem8[LEDREG]);
		dahdi_alarm_notify(&tor->tspans[i].dahdi_span);
	   }
	if (!(tor->passno % 1000)) /* even second boundary */
	   {
		  /* do all spans */
		for (i = 1; i <= SPANS_PER_CARD; i++)
		   {
			if (tor->cardtype == TYPE_E1)
			{
				   /* add this second's BPV count to total one */
				tor->tspans[i - 1].dahdi_span.count.bpv +=
					t1in(tor, i, 1) + (t1in(tor, i, 0)<<8);

				if (tor->tspans[i - 1].dahdi_span.lineconfig & DAHDI_CONFIG_CRC4)
				{
					tor->tspans[i - 1].dahdi_span.count.crc4 +=
						t1in(tor, i, 3) +
						((t1in(tor, i, 2) & 3) << 8);
					tor->tspans[i - 1].dahdi_span.count.ebit +=
						t1in(tor, i, 5) +
						((t1in(tor, i, 4) & 3) << 8);
				}
				tor->tspans[i - 1].dahdi_span.count.fas +=
					(t1in(tor, i, 4) >> 2) +
					((t1in(tor, i, 2) & 0x3F) << 6);
			}
			else
			{
				   /* add this second's BPV count to total one */
				tor->tspans[i - 1].dahdi_span.count.bpv +=
					t1in(tor, i, 0x24) +
					(t1in(tor, i, 0x23) << 8);
			}
		   }
	   }
	if (!timingcable) {
		/* re-evaluate active sync src (no cable version) */
		tor->syncsrc = 0;
		newsyncsrc = 0;
		  /* if primary sync specified, see if we can use it */
		if (tor->psyncs[0])
		   {
			  /* if no alarms, use it */
			if (!(tor->tspans[tor->psyncs[0] - 1].dahdi_span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE |
				DAHDI_ALARM_LOOPBACK))) {
					tor->syncsrc = tor->psyncs[0];
					newsyncsrc = tor->syncs[0];
					}
		   }
		  /* if any others specified, see if we can use them */
		for (i = 1; i < SPANS_PER_CARD; i++) {
			   /* if we dont have one yet, and there is one specified at this level, see if we can use it */
			if ((!tor->syncsrc) && (tor->psyncs[i])) {
				  /* if no alarms, use it */
				if (!(tor->tspans[tor->psyncs[i] - 1].dahdi_span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE |
					DAHDI_ALARM_LOOPBACK))) {
						tor->syncsrc = tor->psyncs[i];
						newsyncsrc = tor->syncs[i];
						}
			}
		}
		/* update sync src info */
		for (i = 0; i < SPANS_PER_CARD; i++)
			tor->tspans[i].dahdi_span.syncsrc = newsyncsrc;

		/* actually set the sync register */
		writeb(tor->syncsrc, &tor->mem8[SYNCREG]);
	} else	/* Timing cable version */
		tor2_findsync(tor);

	tor->passno++;

#ifdef ENABLE_TASKLETS
	if (!tor->taskletpending) {
		tor->taskletpending = 1;
		tor->taskletsched++;
		tasklet_hi_schedule(&tor->tor2_tlet);
	} else {
		tor->txerrors++;
	}
#else
	tor2_run(tor);
#endif
	/* We are not the timing bus master */
	if (tor->cardtype == TYPE_E1)
		/* clear OUTBIT and enable interrupts */
		writeb(INTENA | E1DIV | tor->master, &tor->mem8[CTLREG]);
	else
		/* clear OUTBIT and enable interrupts */
		writeb(INTENA | tor->master, &tor->mem8[CTLREG]);
	return IRQ_RETVAL(1);
}


static int tor2_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	switch(cmd) {
	default:
		return -ENOTTY;
	}
	return 0;
}

MODULE_AUTHOR("Mark Spencer");
MODULE_DESCRIPTION("Tormenta 2 PCI Quad T1 or E1 DAHDI Driver");
MODULE_LICENSE("GPL v2");

module_param(debug, int, 0600);
module_param(loopback, int, 0600);
module_param(timingcable, int, 0600);
module_param(japan, int, 0600);

MODULE_DEVICE_TABLE(pci, tor2_pci_ids);

module_init(tor2_init);
module_exit(tor2_cleanup);
