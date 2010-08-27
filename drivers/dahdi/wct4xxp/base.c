/*
 * TE410P  Quad-T1/E1 PCI Driver version 0.1, 12/16/02
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and archetectures conceived and
 *   written by Jim Dixon <jim@lambdatel.com>.
 * Further modified, optimized, and maintained by 
 *   Matthew Fredrickson <creslin@digium.com> and
 *   Russ Meyerriecks <rmeyerriecks@digium.com>
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

#include "wct4xxp.h"
#include "vpm450m.h"

/* Work queues are a way to better distribute load on SMP systems */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
/*
 * Work queues can significantly improve performance and scalability
 * on multi-processor machines, but requires bypassing some kernel
 * API's, so it's not guaranteed to be compatible with all kernels.
 */
/* #define ENABLE_WORKQUEUES */
#endif

/* Enable prefetching may help performance */
#define ENABLE_PREFETCH

/* Support first generation cards? */
#define SUPPORT_GEN1 

/* Define to get more attention-grabbing but slightly more I/O using
   alarm status */
#define FANCY_ALARM

/* Define to support Digium Voice Processing Module expansion card */
#define VPM_SUPPORT

#define DEBUG_MAIN 		(1 << 0)
#define DEBUG_DTMF 		(1 << 1)
#define DEBUG_REGS 		(1 << 2)
#define DEBUG_TSI  		(1 << 3)
#define DEBUG_ECHOCAN 	(1 << 4)
#define DEBUG_RBS 		(1 << 5)
#define DEBUG_FRAMER		(1 << 6)

/* Maximum latency to be used with Gen 5 */
#define GEN5_MAX_LATENCY	127

#define T4_BASE_SIZE (DAHDI_MAX_CHUNKSIZE * 32 * 4) 

#ifdef ENABLE_WORKQUEUES
#include <linux/cpu.h>

/* XXX UGLY!!!! XXX  We have to access the direct structures of the workqueue which
  are only defined within workqueue.c because they don't give us a routine to allow us
  to nail a work to a particular thread of the CPU.  Nailing to threads gives us substantially
  higher scalability in multi-CPU environments though! */

/*
 * The per-CPU workqueue (if single thread, we always use cpu 0's).
 *
 * The sequence counters are for flush_scheduled_work().  It wants to wait
 * until until all currently-scheduled works are completed, but it doesn't
 * want to be livelocked by new, incoming ones.  So it waits until
 * remove_sequence is >= the insert_sequence which pertained when
 * flush_scheduled_work() was called.
 */
 
struct cpu_workqueue_struct {

	spinlock_t lock;

	long remove_sequence;	/* Least-recently added (next to run) */
	long insert_sequence;	/* Next to add */

	struct list_head worklist;
	wait_queue_head_t more_work;
	wait_queue_head_t work_done;

	struct workqueue_struct *wq;
	task_t *thread;

	int run_depth;		/* Detect run_workqueue() recursion depth */
} ____cacheline_aligned;

/*
 * The externally visible workqueue abstraction is an array of
 * per-CPU workqueues:
 */
struct workqueue_struct {
	/* TODO: Find out exactly where the API changed */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
	struct cpu_workqueue_struct *cpu_wq;
#else
	struct cpu_workqueue_struct cpu_wq[NR_CPUS];
#endif
	const char *name;
	struct list_head list; 	/* Empty if single thread */
};

/* Preempt must be disabled. */
static void __t4_queue_work(struct cpu_workqueue_struct *cwq,
			 struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&cwq->lock, flags);
	work->wq_data = cwq;
	list_add_tail(&work->entry, &cwq->worklist);
	cwq->insert_sequence++;
	wake_up(&cwq->more_work);
	spin_unlock_irqrestore(&cwq->lock, flags);
}

/*
 * Queue work on a workqueue. Return non-zero if it was successfully
 * added.
 *
 * We queue the work to the CPU it was submitted, but there is no
 * guarantee that it will be processed by that CPU.
 */
static inline int t4_queue_work(struct workqueue_struct *wq, struct work_struct *work, int cpu)
{
	int ret = 0;
	get_cpu();
	if (!test_and_set_bit(0, &work->pending)) {
		BUG_ON(!list_empty(&work->entry));
		__t4_queue_work(wq->cpu_wq + cpu, work);
		ret = 1;
	}
	put_cpu();
	return ret;
}

#endif

/*
 * Define CONFIG_EXTENDED_RESET to allow the qfalc framer extra time
 * to reset itself upon hardware initialization. This exits for rare
 * cases for customers who are seeing the qfalc returning unexpected
 * information at initialization
 */
#undef CONFIG_EXTENDED_RESET

static int pedanticpci = 1;
static int debug=0;
static int timingcable = 0;
static int t1e1override = -1;  /* 0xff for E1, 0x00 for T1 */
static int j1mode = 0;
static int sigmode = FRMR_MODE_NO_ADDR_CMP;
static int alarmdebounce = 2500; /* LOF/LFA def to 2.5s AT&T TR54016*/
static int losalarmdebounce = 2500;/* LOS def to 2.5s AT&T TR54016*/
static int aisalarmdebounce = 2500;/* AIS(blue) def to 2.5s AT&T TR54016*/
static int yelalarmdebounce = 500;/* RAI(yellow) def to 0.5s AT&T devguide */
static int max_latency = GEN5_MAX_LATENCY;  /* Used to set a maximum latency (if you don't wish it to hard cap it at a certain value) in milliseconds */
#ifdef VPM_SUPPORT
static int vpmsupport = 1;
/* If set to auto, vpmdtmfsupport is enabled for VPM400M and disabled for VPM450M */
static int vpmdtmfsupport = -1; /* -1=auto, 0=disabled, 1=enabled*/
static int vpmspans = 4;
#define VPM_DEFAULT_DTMFTHRESHOLD 1000
static int dtmfthreshold = VPM_DEFAULT_DTMFTHRESHOLD;
static int lastdtmfthreshold = VPM_DEFAULT_DTMFTHRESHOLD;
#endif
/* Enabling bursting can more efficiently utilize PCI bus bandwidth, but
   can also cause PCI bus starvation, especially in combination with other
   aggressive cards.  Please note that burst mode has no effect on CPU
   utilization / max number of calls / etc. */
static int noburst;
/* For 56kbps links, set this module parameter to 0x7f */
static int hardhdlcmode = 0xff;

static int latency = 1;

static int ms_per_irq = 1;

#ifdef FANCY_ALARM
static int altab[] = {
0, 0, 0, 1, 2, 3, 4, 6, 8, 9, 11, 13, 16, 18, 20, 22, 24, 25, 27, 28, 29, 30, 31, 31, 32, 31, 31, 30, 29, 28, 27, 25, 23, 22, 20, 18, 16, 13, 11, 9, 8, 6, 4, 3, 2, 1, 0, 0, 
};
#endif

#define MAX_SPANS 16

#define FLAG_STARTED (1 << 0)
#define FLAG_NMF (1 << 1)
#define FLAG_SENDINGYELLOW (1 << 2)


#define	TYPE_T1	1		/* is a T1 card */
#define	TYPE_E1	2		/* is an E1 card */
#define TYPE_J1 3		/* is a running J1 */

#define FLAG_2NDGEN  (1 << 3)
#define FLAG_2PORT   (1 << 4)
#define FLAG_VPM2GEN (1 << 5)
#define FLAG_OCTOPT  (1 << 6)
#define FLAG_3RDGEN  (1 << 7)
#define FLAG_BURST   (1 << 8)
#define FLAG_EXPRESS (1 << 9)
#define FLAG_5THGEN  (1 << 10)

#define CANARY 0xc0de


#define PORTS_PER_FRAMER 4

struct devtype {
	char *desc;
	unsigned int flags;
};

static struct devtype wct420p5 = { "Wildcard TE420 (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_EXPRESS };
static struct devtype wct410p5 = { "Wildcard TE410P (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct405p5 = { "Wildcard TE405P (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct220p5 = { "Wildcard TE220 (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT | FLAG_EXPRESS };
static struct devtype wct210p5 = { "Wildcard TE210P (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };
static struct devtype wct205p5 = { "Wildcard TE205P (5th Gen)", FLAG_5THGEN | FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };

static struct devtype wct4xxp = { "Wildcard TE410P/TE405P (1st Gen)", 0 };
static struct devtype wct420p4 = { "Wildcard TE420 (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_EXPRESS };
static struct devtype wct410p4 = { "Wildcard TE410P (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct410p3 = { "Wildcard TE410P (3rd Gen)", FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct405p4 = { "Wildcard TE405P (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct405p3 = { "Wildcard TE405P (3rd Gen)", FLAG_2NDGEN | FLAG_3RDGEN };
static struct devtype wct410p2 = { "Wildcard TE410P (2nd Gen)", FLAG_2NDGEN };
static struct devtype wct405p2 = { "Wildcard TE405P (2nd Gen)", FLAG_2NDGEN };
static struct devtype wct220p4 = { "Wildcard TE220 (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT | FLAG_EXPRESS };
static struct devtype wct205p4 = { "Wildcard TE205P (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };
static struct devtype wct205p3 = { "Wildcard TE205P (3rd Gen)", FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };
static struct devtype wct210p4 = { "Wildcard TE210P (4th Gen)", FLAG_BURST | FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };
static struct devtype wct210p3 = { "Wildcard TE210P (3rd Gen)", FLAG_2NDGEN | FLAG_3RDGEN | FLAG_2PORT };
static struct devtype wct205 = { "Wildcard TE205P ", FLAG_2NDGEN | FLAG_2PORT };
static struct devtype wct210 = { "Wildcard TE210P ", FLAG_2NDGEN | FLAG_2PORT };
	

struct t4;

struct t4_span {
	struct t4 *owner;
	unsigned int *writechunk;					/* Double-word aligned write memory */
	unsigned int *readchunk;					/* Double-word aligned read memory */
	int spantype;		/* card type, T1 or E1 or J1 */
	int sync;
	int psync;
	int alarmtimer;
	int redalarms;
	int notclear;
	int alarmcount;
	int losalarmcount;
	int aisalarmcount;
	int yelalarmcount;
	int spanflags;
	int syncpos;
#ifdef SUPPORT_GEN1
	int e1check;			/* E1 check */
#endif
	struct dahdi_span span;
	unsigned char txsigs[16];	/* Transmit sigs */
	int loopupcnt;
	int loopdowncnt;
#ifdef SUPPORT_GEN1
	unsigned char ec_chunk1[31][DAHDI_CHUNKSIZE]; /* first EC chunk buffer */
	unsigned char ec_chunk2[31][DAHDI_CHUNKSIZE]; /* second EC chunk buffer */
#endif
	int irqmisses;
	
	/* HDLC controller fields */
	struct dahdi_chan *sigchan;
	unsigned char sigmode;
	int sigactive;
	int frames_out;
	int frames_in;

#ifdef VPM_SUPPORT
	unsigned long dtmfactive;
	unsigned long dtmfmask;
	unsigned long dtmfmutemask;
	short dtmfenergy[31];
	short dtmfdigit[31];
#endif
#ifdef ENABLE_WORKQUEUES
	struct work_struct swork;
#endif	
	struct dahdi_chan *chans[32];		/* Individual channels */
	struct dahdi_echocan_state *ec[32];	/* Echocan state for each channel */
};

struct t4 {
	/* This structure exists one per card */
	struct pci_dev *dev;		/* Pointer to PCI device */
	unsigned int intcount;
	int num;			/* Which card we are */
	int t1e1;			/* T1/E1 select pins */
	int globalconfig;	/* Whether global setup has been done */
	int syncsrc;			/* active sync source */
	struct t4_span *tspans[4];	/* Individual spans */
	int numspans;			/* Number of spans on the card */
	int blinktimer;
#ifdef FANCY_ALARM
	int alarmpos;
#endif
	int irq;			/* IRQ used by device */
	int order;			/* Order */
	int flags;                      /* Device flags */
	unsigned int falc31 : 1;	/* are we falc v3.1 (atomic not necessary) */
	int master;				/* Are we master */
	int ledreg;				/* LED Register */
	unsigned int gpio;
	unsigned int gpioctl;
	int e1recover;			/* E1 recovery timer */
	spinlock_t reglock;		/* lock register access */
	int spansstarted;		/* number of spans started */
	volatile unsigned int *writechunk;					/* Double-word aligned write memory */
	volatile unsigned int *readchunk;					/* Double-word aligned read memory */
	unsigned short canary;
#ifdef ENABLE_WORKQUEUES
	atomic_t worklist;
	struct workqueue_struct *workq;
#endif
	unsigned int passno;	/* number of interrupt passes */
	char *variety;
	int last0;		/* for detecting double-missed IRQ */

	/* DMA related fields */
	unsigned int dmactrl;
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	unsigned long memaddr;		/* Base address of card */
	unsigned long memlen;
	__iomem volatile unsigned int *membase;	/* Base address of card */

	/* Add this for our softlockup protector */
	unsigned int oct_rw_count;

	/* Flags for our bottom half */
	unsigned long checkflag;
	struct tasklet_struct t4_tlet;
	unsigned int vpm400checkstatus;
	/* Latency related additions */
	unsigned char rxident;
	unsigned char lastindex;
	int numbufs;
	int needed_latency;
	
#ifdef VPM_SUPPORT
	struct vpm450m *vpm450m;
	int vpm;
#endif	

};

#define T4_VPM_PRESENT (1 << 28)

#ifdef VPM_SUPPORT
static void t4_vpm400_init(struct t4 *wc);
static void t4_vpm450_init(struct t4 *wc);
static void t4_vpm_set_dtmf_threshold(struct t4 *wc, unsigned int threshold);

static void echocan_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);

static const struct dahdi_echocan_features vpm400m_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_features vpm450m_ec_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_ops vpm400m_ec_ops = {
	.name = "VPM400M",
	.echocan_free = echocan_free,
};

static const struct dahdi_echocan_ops vpm450m_ec_ops = {
	.name = "VPM450M",
	.echocan_free = echocan_free,
};
#endif

static void __set_clear(struct t4 *wc, int span);
static int t4_startup(struct dahdi_span *span);
static int t4_shutdown(struct dahdi_span *span);
static int t4_rbsbits(struct dahdi_chan *chan, int bits);
static int t4_maint(struct dahdi_span *span, int cmd);
static int t4_clear_maint(struct dahdi_span *span);
static int t4_reset_counters(struct dahdi_span *span);
#ifdef SUPPORT_GEN1
static int t4_reset_dma(struct t4 *wc);
#endif
static void t4_hdlc_hard_xmit(struct dahdi_chan *chan);
static int t4_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data);
static void t4_tsi_assign(struct t4 *wc, int fromspan, int fromchan, int tospan, int tochan);
static void t4_tsi_unassign(struct t4 *wc, int tospan, int tochan);
static void __t4_set_rclk_src(struct t4 *wc, int span);
static void __t4_set_sclk_src(struct t4 *wc, int mode, int master, int slave);
static void t4_check_alarms(struct t4 *wc, int span);
static void t4_check_sigbits(struct t4 *wc, int span);

#define WC_RDADDR	0
#define WC_WRADDR	1
#define WC_COUNT	2
#define WC_DMACTRL	3	
#define WC_INTR		4
/* #define WC_GPIO		5 */
#define WC_VERSION	6
#define WC_LEDS		7
#define WC_GPIOCTL	8
#define WC_GPIO		9
#define WC_LADDR	10
#define WC_LDATA		11
#define WC_LCS		(1 << 11)
#define WC_LCS2		(1 << 12)
#define WC_LALE			(1 << 13)
#define WC_LFRMR_CS	(1 << 10)	/* Framer's ChipSelect signal */
#define WC_ACTIVATE	(1 << 12)
#define WC_LREAD			(1 << 15)
#define WC_LWRITE		(1 << 16)

#define WC_OFF    (0)
#define WC_RED    (1)
#define WC_GREEN  (2)
#define WC_YELLOW (3)

#define WC_RECOVER 	0
#define WC_SELF 	1

#define LIM0_T 0x36 		/* Line interface mode 0 register */
#define LIM0_LL (1 << 1)	/* Local Loop */
#define LIM1_T 0x37		/* Line interface mode 1 register */
#define LIM1_RL (1 << 1)	/* Remote Loop */

#define FMR0 0x1C		/* Framer Mode Register 0 */
#define FMR0_SIM (1 << 0)	/* Alarm Simulation */
#define FMR1_T 0x1D		/* Framer Mode Register 1 */
#define FMR1_ECM (1 << 2)	/* Error Counter 1sec Interrupt Enable */
#define DEC_T 0x60		/* Diable Error Counter */
#define IERR_T 0x1B		/* Single Bit Defect Insertion Register */
#define IBV	0	 /* Bipolar violation */
#define IPE	(1 << 1) /* PRBS defect */
#define ICASE	(1 << 2) /* CAS defect */
#define ICRCE	(1 << 3) /* CRC defect */
#define IMFE	(1 << 4) /* Multiframe defect */
#define IFASE	(1 << 5) /* FAS defect */
#define ISR3_SEC (1 << 6)	/* Internal one-second interrupt bit mask */
#define ISR3_ES (1 << 7)	/* Errored Second interrupt bit mask */
#define ESM 0x47		/* Errored Second mask register */

#define FMR2_T 0x1E		/* Framer Mode Register 2 */
#define FMR2_PLB (1 << 2)	/* Framer Mode Register 2 */

#define FECL_T 0x50		/* Framing Error Counter Lower Byte */
#define FECH_T 0x51		/* Framing Error Counter Higher Byte */
#define CVCL_T 0x52		/* Code Violation Counter Lower Byte */
#define CVCH_T 0x53		/* Code Violation Counter Higher Byte */
#define CEC1L_T 0x54		/* CRC Error Counter 1 Lower Byte */
#define CEC1H_T 0x55		/* CRC Error Counter 1 Higher Byte */
#define EBCL_T 0x56		/* E-Bit Error Counter Lower Byte */
#define EBCH_T 0x57		/* E-Bit Error Counter Higher Byte */
#define BECL_T 0x58		/* Bit Error Counter Lower Byte */
#define BECH_T 0x59		/* Bit Error Counter Higher Byte */
#define COEC_T 0x5A		/* COFA Event Counter */
#define PRBSSTA_T 0xDA		/* PRBS Status Register */

#define LCR1_T 0x3B		/* Loop Code Register 1 */
#define EPRM (1 << 7)		/* Enable PRBS rx */
#define XPRBS (1 << 6)		/* Enable PRBS tx */
#define FLLB (1 << 1)		/* Framed line loop/Invert */
#define LLBP (1 << 0)		/* Line Loopback Pattern */
#define TPC0_T 0xA8		/* Test Pattern Control Register */
#define FRA (1 << 6)		/* Framed/Unframed Selection */
#define PRBS23 (3 << 4)		/* Pattern selection (23 poly) */
#define PRM (1 << 2)		/* Non framed mode */
#define FRS1_T 0x4D		/* Framer Receive Status Reg 1 */
#define LLBDD (1 << 4)
#define LLBAD (1 << 3)

#define MAX_T4_CARDS 64

static void t4_isr_bh(unsigned long data);

static struct t4 *cards[MAX_T4_CARDS];


#define MAX_TDM_CHAN 32
#define MAX_DTMF_DET 16

#define HDLC_IMR0_MASK (FRMR_IMR0_RME | FRMR_IMR0_RPF)
#if 0
#define HDLC_IMR1_MASK (FRMR_IMR1_ALLS | FRMR_IMR1_XDU | FRMR_IMR1_XPR)
#else
#define HDLC_IMR1_MASK	(FRMR_IMR1_XDU | FRMR_IMR1_XPR)
#endif

static inline unsigned int __t4_pci_in(struct t4 *wc, const unsigned int addr)
{
	unsigned int res = readl(&wc->membase[addr]);
	return res;
}

static inline void __t4_pci_out(struct t4 *wc, const unsigned int addr, const unsigned int value)
{
	unsigned int tmp;
	writel(value, &wc->membase[addr]);
	if (pedanticpci) {
		tmp = __t4_pci_in(wc, WC_VERSION);
		if ((tmp & 0xffff0000) != 0xc01a0000)
			dev_notice(&wc->dev->dev,
					"Version Synchronization Error!\n");
	}
#if 0
	tmp = __t4_pci_in(wc, addr);
	if ((value != tmp) && (addr != WC_LEDS) && (addr != WC_LDATA) &&
		(addr != WC_GPIO) && (addr != WC_INTR))
		dev_info(&wc->dev->dev, "Tried to load %08x into %08x, "
				"but got %08x instead\n", value, addr, tmp);
#endif		
}

static inline void __t4_gpio_set(struct t4 *wc, unsigned bits, unsigned int val)
{
	unsigned int newgpio;
	newgpio = wc->gpio & (~bits);
	newgpio |= val;
	if (newgpio != wc->gpio) {
		wc->gpio = newgpio;
		__t4_pci_out(wc, WC_GPIO, wc->gpio);
	}	
}

static inline void __t4_gpio_setdir(struct t4 *wc, unsigned int bits, unsigned int val)
{
	unsigned int newgpioctl;
	newgpioctl = wc->gpioctl & (~bits);
	newgpioctl |= val;
	if (newgpioctl != wc->gpioctl) {
		wc->gpioctl = newgpioctl;
		__t4_pci_out(wc, WC_GPIOCTL, wc->gpioctl);
	}
}

static inline void t4_gpio_setdir(struct t4 *wc, unsigned int bits, unsigned int val)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_gpio_setdir(wc, bits, val);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void t4_gpio_set(struct t4 *wc, unsigned int bits, unsigned int val)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_gpio_set(wc, bits, val);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void t4_pci_out(struct t4 *wc, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_pci_out(wc, addr, value);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void __t4_set_led(struct t4 *wc, int span, int color)
{
	int oldreg = wc->ledreg;
	wc->ledreg &= ~(0x3 << (span << 1));
	wc->ledreg |= (color << (span << 1));
	if (oldreg != wc->ledreg)
		__t4_pci_out(wc, WC_LEDS, wc->ledreg);
}

static inline void t4_activate(struct t4 *wc)
{
	wc->ledreg |= WC_ACTIVATE;
	t4_pci_out(wc, WC_LEDS, wc->ledreg);
}

static inline unsigned int t4_pci_in(struct t4 *wc, const unsigned int addr)
{
	unsigned int ret;
	unsigned long flags;
	
	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t4_pci_in(wc, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline unsigned int __t4_framer_in(struct t4 *wc, int unit, const unsigned int addr)
{
	unsigned int ret;
	unit &= 0x3;
	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | WC_LFRMR_CS | WC_LREAD);
	if (!pedanticpci) {
		__t4_pci_in(wc, WC_VERSION);
	} else {
		__t4_pci_out(wc, WC_VERSION, 0);
	}
	ret = __t4_pci_in(wc, WC_LDATA);
 	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));

	if (unlikely(debug & DEBUG_REGS))
		dev_info(&wc->dev->dev, "Reading unit %d address %02x is "
				"%02x\n", unit, addr, ret & 0xff);

	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);

	return ret & 0xff;
}

static inline unsigned int t4_framer_in(struct t4 *wc, int unit, const unsigned int addr)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t4_framer_in(wc, unit, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;

}

static inline void __t4_framer_out(struct t4 *wc, int unit, const unsigned int addr, const unsigned int value)
{
	unit &= 0x3;
	if (unlikely(debug & DEBUG_REGS))
		dev_info(&wc->dev->dev, "Writing %02x to address %02x of "
				"unit %d\n", value, addr, unit);
	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));
	__t4_pci_out(wc, WC_LDATA, value);
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff) | WC_LFRMR_CS | WC_LWRITE);
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	__t4_pci_out(wc, WC_LADDR, (unit << 8) | (addr & 0xff));	
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	if (unlikely(debug & DEBUG_REGS))
		dev_info(&wc->dev->dev, "Write complete\n");
#if 0
	if ((addr != FRMR_TXFIFO) && (addr != FRMR_CMDR) && (addr != 0xbc))
	{ unsigned int tmp;
	tmp = __t4_framer_in(wc, unit, addr);
	if (tmp != value) {
		dev_notice(&wc->dev->dev, "Expected %d from unit %d "
				"register %d but got %d instead\n",
				value, unit, addr, tmp);
	} }
#endif	
}

static inline void t4_framer_out(struct t4 *wc, int unit, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_framer_out(wc, unit, addr, value);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

#ifdef VPM_SUPPORT

static inline void wait_a_little(void)
{
	unsigned long newjiffies=jiffies+2;
	while(jiffies < newjiffies);
}

static inline unsigned int __t4_vpm_in(struct t4 *wc, int unit, const unsigned int addr)
{
	unsigned int ret;
	unit &= 0x7;
	__t4_pci_out(wc, WC_LADDR, (addr & 0x1ff) | ( unit << 12));
	__t4_pci_out(wc, WC_LADDR, (addr & 0x1ff) | ( unit << 12) | (1 << 11) | WC_LREAD);
	ret = __t4_pci_in(wc, WC_LDATA);
	__t4_pci_out(wc, WC_LADDR, 0);
	return ret & 0xff;
}

static inline void __t4_raw_oct_out(struct t4 *wc, const unsigned int addr, const unsigned int value)
{
	int octopt = wc->tspans[0]->spanflags & FLAG_OCTOPT;
	if (!octopt) 
		__t4_gpio_set(wc, 0xff, (addr >> 8));
	__t4_pci_out(wc, WC_LDATA, 0x10000 | (addr & 0xffff));
	if (!octopt)
		__t4_pci_out(wc, WC_LADDR, (WC_LWRITE));
	__t4_pci_out(wc, WC_LADDR, (WC_LWRITE | WC_LALE));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	if (!octopt)
		__t4_gpio_set(wc, 0xff, (value >> 8));
	__t4_pci_out(wc, WC_LDATA, (value & 0xffff));
	__t4_pci_out(wc, WC_LADDR, (WC_LWRITE | WC_LALE | WC_LCS));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	__t4_pci_out(wc, WC_LADDR, (0));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
}

static inline unsigned int __t4_raw_oct_in(struct t4 *wc, const unsigned int addr)
{
	unsigned int ret;
	int octopt = wc->tspans[0]->spanflags & FLAG_OCTOPT;
	if (!octopt)
		__t4_gpio_set(wc, 0xff, (addr >> 8));
	__t4_pci_out(wc, WC_LDATA, 0x10000 | (addr & 0xffff));
	if (!octopt)
		__t4_pci_out(wc, WC_LADDR, (WC_LWRITE));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	__t4_pci_out(wc, WC_LADDR, (WC_LWRITE | WC_LALE));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
#ifdef PEDANTIC_OCTASIC_CHECKING 
	__t4_pci_out(wc, WC_LADDR, (WC_LALE));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
#endif
	if (!octopt) {
		__t4_gpio_setdir(wc, 0xff, 0x00);
		__t4_gpio_set(wc, 0xff, 0x00);
	}
	__t4_pci_out(wc, WC_LADDR, (WC_LREAD | WC_LALE | WC_LCS));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	if (octopt) {
		ret = __t4_pci_in(wc, WC_LDATA) & 0xffff;
	} else {
		ret = __t4_pci_in(wc, WC_LDATA) & 0xff;
		ret |= (__t4_pci_in(wc, WC_GPIO) & 0xff) << 8;
	}
	__t4_pci_out(wc, WC_LADDR, (0));
	if (!pedanticpci)
		__t4_pci_in(wc, WC_VERSION);
	if (!octopt)
		__t4_gpio_setdir(wc, 0xff, 0xff);
	return ret & 0xffff;
}

static inline unsigned int __t4_oct_in(struct t4 *wc, unsigned int addr)
{
#ifdef PEDANTIC_OCTASIC_CHECKING
	int count = 1000;
#endif
	__t4_raw_oct_out(wc, 0x0008, (addr >> 20));
	__t4_raw_oct_out(wc, 0x000a, (addr >> 4) & ((1 << 16) - 1));
	__t4_raw_oct_out(wc, 0x0000, (((addr >> 1) & 0x7) << 9) | (1 << 8) | (1));
#ifdef PEDANTIC_OCTASIC_CHECKING
	while((__t4_raw_oct_in(wc, 0x0000) & (1 << 8)) && --count);
	if (count != 1000)
		dev_notice(&wc->dev->dev, "Yah, read can be slow...\n");
	if (!count)
		dev_notice(&wc->dev->dev, "Read timed out!\n");
#endif
	return __t4_raw_oct_in(wc, 0x0004);
}

static inline unsigned int t4_oct_in(struct t4 *wc, const unsigned int addr)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t4_oct_in(wc, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline unsigned int t4_vpm_in(struct t4 *wc, int unit, const unsigned int addr)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&wc->reglock, flags);
	ret = __t4_vpm_in(wc, unit, addr);
	spin_unlock_irqrestore(&wc->reglock, flags);
	return ret;
}

static inline void __t4_vpm_out(struct t4 *wc, int unit, const unsigned int addr, const unsigned int value)
{
	unit &= 0x7;
	if (debug & DEBUG_REGS)
		dev_notice(&wc->dev->dev, "Writing %02x to address %02x of "
				"ec unit %d\n", value, addr, unit);
	__t4_pci_out(wc, WC_LADDR, (addr & 0xff));
	__t4_pci_out(wc, WC_LDATA, value);
	__t4_pci_out(wc, WC_LADDR, (unit << 12) | (addr & 0x1ff) | (1 << 11));
	__t4_pci_out(wc, WC_LADDR, (unit << 12) | (addr & 0x1ff) | (1 << 11) | WC_LWRITE);
	__t4_pci_out(wc, WC_LADDR, (unit << 12) | (addr & 0x1ff) | (1 << 11));
	__t4_pci_out(wc, WC_LADDR, (unit << 12) | (addr & 0x1ff));	
	__t4_pci_out(wc, WC_LADDR, 0);
	if (debug & DEBUG_REGS)
		dev_notice(&wc->dev->dev, "Write complete\n");

      
#if 0
	{ unsigned int tmp;
	tmp = t4_vpm_in(wc, unit, addr);
	if (tmp != value) {
		dev_notice(&wc->dev->dev, "Expected %d from unit %d echo "
				"register %d but got %d instead\n",
				value, unit, addr, tmp);
	} }
#endif
}

static inline void __t4_oct_out(struct t4 *wc, unsigned int addr, unsigned int value)
{
#ifdef PEDANTIC_OCTASIC_CHECKING
	int count = 1000;
#endif
	__t4_raw_oct_out(wc, 0x0008, (addr >> 20));
	__t4_raw_oct_out(wc, 0x000a, (addr >> 4) & ((1 << 16) - 1));
	__t4_raw_oct_out(wc, 0x0004, value);
	__t4_raw_oct_out(wc, 0x0000, (((addr >> 1) & 0x7) << 9) | (1 << 8) | (3 << 12) | 1);
#ifdef PEDANTIC_OCTASIC_CHECKING
	while((__t4_raw_oct_in(wc, 0x0000) & (1 << 8)) && --count);
	if (count != 1000)
		dev_notice(&wc->dev->dev, "Yah, write can be slow\n");
	if (!count)
		dev_notice(&wc->dev->dev, "Write timed out!\n");
#endif
}

static inline void t4_oct_out(struct t4 *wc, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;

	spin_lock_irqsave(&wc->reglock, flags);
	__t4_oct_out(wc, addr, value);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static inline void t4_vpm_out(struct t4 *wc, int unit, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_vpm_out(wc, unit, addr, value);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static const char vpm_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', '*', '#'};

static void t4_check_vpm450(struct t4 *wc)
{
	int channel, tone, start, span;

	if (vpm450m_checkirq(wc->vpm450m)) {
		while(vpm450m_getdtmf(wc->vpm450m, &channel, &tone, &start)) {
			span = channel & 0x3;
			channel >>= 2;
			if (!wc->t1e1)
				channel -= 5;
			else
				channel -= 1;
			if (unlikely(debug))
				dev_info(&wc->dev->dev, "Got tone %s of '%c' "
					"on channel %d of span %d\n",
					(start ? "START" : "STOP"),
					tone, channel, span + 1);
			if (test_bit(channel, &wc->tspans[span]->dtmfmask) && (tone != 'u')) {
				if (start) {
					/* The octasic is supposed to mute us, but...  Yah, you
					   guessed it.  */
					if (test_bit(channel, &wc->tspans[span]->dtmfmutemask)) {
						unsigned long flags;
						struct dahdi_chan *chan = wc->tspans[span]->span.chans[channel];
						int y;
						spin_lock_irqsave(&chan->lock, flags);
						for (y=0;y<chan->numbufs;y++) {
							if ((chan->inreadbuf > -1) && (chan->readidx[y]))
								memset(chan->readbuf[chan->inreadbuf], DAHDI_XLAW(0, chan), chan->readidx[y]);
						}
						spin_unlock_irqrestore(&chan->lock, flags);
					}
					set_bit(channel, &wc->tspans[span]->dtmfactive);
					dahdi_qevent_lock(wc->tspans[span]->span.chans[channel], (DAHDI_EVENT_DTMFDOWN | tone));
				} else {
					clear_bit(channel, &wc->tspans[span]->dtmfactive);
					dahdi_qevent_lock(wc->tspans[span]->span.chans[channel], (DAHDI_EVENT_DTMFUP | tone));
				}
			}
		}
	}
}

static void t4_check_vpm400(struct t4 *wc, unsigned int newio)
{
	unsigned int digit, regval = 0;
	unsigned int regbyte;
	int x, i;
	short energy=0;
	static unsigned int lastio = 0;
	struct t4_span *ts;

	if (debug && (newio != lastio)) 
		dev_notice(&wc->dev->dev, "Last was %08x, new is %08x\n",
				lastio, newio);

	lastio = newio;
 
	for(x = 0; x < 8; x++) {
		if (newio & (1 << (7 - x)))
			continue;
		ts = wc->tspans[x%4];
		/* Start of DTMF detection process */	
		regbyte = t4_vpm_in(wc, x, 0xb8);
		t4_vpm_out(wc, x, 0xb8, regbyte); /* Write 1 to clear */
		regval = regbyte << 8;
		regbyte = t4_vpm_in(wc, x, 0xb9);
		t4_vpm_out(wc, x, 0xb9, regbyte);
		regval |= regbyte;

		for(i = 0; (i < MAX_DTMF_DET) && regval; i++) {
			if(regval & 0x0001) {
				int channel = (i << 1) + (x >> 2);
				int base = channel - 1;

				if (!wc->t1e1)
					base -= 4;
				regbyte = t4_vpm_in(wc, x, 0xa8 + i);
				digit = vpm_digits[regbyte];
				if (!(wc->tspans[0]->spanflags & FLAG_VPM2GEN)) {
					energy = t4_vpm_in(wc, x, 0x58 + channel);
					energy = DAHDI_XLAW(energy, ts->chans[0]);
					ts->dtmfenergy[base] = energy;
				}
				set_bit(base, &ts->dtmfactive);
				if (ts->dtmfdigit[base]) {
					if (ts->dtmfmask & (1 << base))
						dahdi_qevent_lock(ts->span.chans[base], (DAHDI_EVENT_DTMFUP | ts->dtmfdigit[base]));
				}
				ts->dtmfdigit[base] = digit;
				if (test_bit(base, &ts->dtmfmask))
					dahdi_qevent_lock(ts->span.chans[base], (DAHDI_EVENT_DTMFDOWN | digit));
				if (test_bit(base, &ts->dtmfmutemask)) {
					/* Mute active receive buffer*/
					unsigned long flags;
					struct dahdi_chan *chan = ts->span.chans[base];
					int y;
					spin_lock_irqsave(&chan->lock, flags);
					for (y=0;y<chan->numbufs;y++) {
						if ((chan->inreadbuf > -1) && (chan->readidx[y]))
							memset(chan->readbuf[chan->inreadbuf], DAHDI_XLAW(0, chan), chan->readidx[y]);
					}
					spin_unlock_irqrestore(&chan->lock, flags);
				}
				if (debug)
					dev_notice(&wc->dev->dev, "Digit "
						"Seen: %d, Span: %d, channel:"
						" %d, energy: %02x, 'channel "
						"%d' chip %d\n", digit, x % 4,
						base + 1, energy, channel, x);
				
			}
			regval = regval >> 1;
		}
		if (!(wc->tspans[0]->spanflags & FLAG_VPM2GEN))
			continue;

		/* Start of DTMF off detection process */	
		regbyte = t4_vpm_in(wc, x, 0xbc);
		t4_vpm_out(wc, x, 0xbc, regbyte); /* Write 1 to clear */
		regval = regbyte << 8;
		regbyte = t4_vpm_in(wc, x, 0xbd);
		t4_vpm_out(wc, x, 0xbd, regbyte);
		regval |= regbyte;

		for(i = 0; (i < MAX_DTMF_DET) && regval; i++) {
			if(regval & 0x0001) {
				int channel = (i << 1) + (x >> 2);
				int base = channel - 1;

				if (!wc->t1e1)
					base -= 4;
				clear_bit(base, &ts->dtmfactive);
				if (ts->dtmfdigit[base]) {
					if (test_bit(base, &ts->dtmfmask))
						dahdi_qevent_lock(ts->span.chans[base], (DAHDI_EVENT_DTMFUP | ts->dtmfdigit[base]));
				}
				digit = ts->dtmfdigit[base];
				ts->dtmfdigit[base] = 0;
				if (debug)
					dev_notice(&wc->dev->dev, "Digit "
						"Gone: %d, Span: %d, channel:"
						" %d, energy: %02x, 'channel "
						"%d' chip %d\n", digit, x % 4,
						base + 1, energy, channel, x);
				
			}
			regval = regval >> 1;
		}

	}
}
#endif

static void hdlc_stop(struct t4 *wc, unsigned int span)
{
	struct t4_span *t = wc->tspans[span];
	unsigned char imr0, imr1, mode;
	int i = 0;

	if (debug & DEBUG_FRAMER)
		dev_notice(&wc->dev->dev, "Stopping HDLC controller on span "
				"%d\n", span+1);
	
	/* Clear receive and transmit timeslots */
	for (i = 0; i < 4; i++) {
		t4_framer_out(wc, span, FRMR_RTR_BASE + i, 0x00);
		t4_framer_out(wc, span, FRMR_TTR_BASE + i, 0x00);
	}

	imr0 = t4_framer_in(wc, span, FRMR_IMR0);
	imr1 = t4_framer_in(wc, span, FRMR_IMR1);

	/* Disable HDLC interrupts */
	imr0 |= HDLC_IMR0_MASK;
	t4_framer_out(wc, span, FRMR_IMR0, imr0);

	imr1 |= HDLC_IMR1_MASK;
	t4_framer_out(wc, span, FRMR_IMR1, imr1);

	mode = t4_framer_in(wc, span, FRMR_MODE);
	mode &= ~FRMR_MODE_HRAC;
	t4_framer_out(wc, span, FRMR_MODE, mode);

	t->sigactive = 0;
}

static inline void __t4_framer_cmd(struct t4 *wc, unsigned int span, int cmd)
{
	__t4_framer_out(wc, span, FRMR_CMDR, cmd);
}

static inline void t4_framer_cmd_wait(struct t4 *wc, unsigned int span, int cmd)
{
	int sis;
	int loops = 0;

	/* XXX could be time consuming XXX */
	for (;;) {
		sis = t4_framer_in(wc, span, FRMR_SIS);
		if (!(sis & 0x04))
			break;
		if (!loops++ && (debug & DEBUG_FRAMER)) {
			dev_notice(&wc->dev->dev, "!!!SIS Waiting before cmd "
					"%02x\n", cmd);
		}
	}
	if (loops && (debug & DEBUG_FRAMER))
		dev_notice(&wc->dev->dev, "!!!SIS waited %d loops\n", loops);

	t4_framer_out(wc, span, FRMR_CMDR, cmd);
}

static int hdlc_start(struct t4 *wc, unsigned int span, struct dahdi_chan *chan, unsigned char mode)
{
	struct t4_span *t = wc->tspans[span];
	unsigned char imr0, imr1;
	int offset = chan->chanpos;
	unsigned long flags;

	if (debug & DEBUG_FRAMER)
		dev_info(&wc->dev->dev, "Starting HDLC controller for channel "
				"%d span %d\n", offset, span+1);

	if (mode != FRMR_MODE_NO_ADDR_CMP)
		return -1;

	mode |= FRMR_MODE_HRAC;

	/* Make sure we're in the right mode */
	t4_framer_out(wc, span, FRMR_MODE, mode);
	t4_framer_out(wc, span, FRMR_TSEO, 0x00);
	t4_framer_out(wc, span, FRMR_TSBS1, hardhdlcmode);

	/* Set the interframe gaps, etc */
	t4_framer_out(wc, span, FRMR_CCR1, FRMR_CCR1_ITF|FRMR_CCR1_EITS);

	t4_framer_out(wc, span, FRMR_CCR2, FRMR_CCR2_RCRC);
	
	/* Set up the time slot that we want to tx/rx on */
	t4_framer_out(wc, span, FRMR_TTR_BASE + (offset / 8), (0x80 >> (offset % 8)));
	t4_framer_out(wc, span, FRMR_RTR_BASE + (offset / 8), (0x80 >> (offset % 8)));

	imr0 = t4_framer_in(wc, span, FRMR_IMR0);
	imr1 = t4_framer_in(wc, span, FRMR_IMR1);

	/* Enable our interrupts again */
	imr0 &= ~HDLC_IMR0_MASK;
	t4_framer_out(wc, span, FRMR_IMR0, imr0);

	imr1 &= ~HDLC_IMR1_MASK;
	t4_framer_out(wc, span, FRMR_IMR1, imr1);

	/* Reset the signaling controller */
	t4_framer_cmd_wait(wc, span, FRMR_CMDR_SRES);

	spin_lock_irqsave(&wc->reglock, flags);
	t->sigchan = chan;
	spin_unlock_irqrestore(&wc->reglock, flags);

	t->sigactive = 0;

	return 0;
}

static void __set_clear(struct t4 *wc, int span)
{
	int i,j;
	int oldnotclear;
	unsigned short val=0;
	struct t4_span *ts = wc->tspans[span];

	oldnotclear = ts->notclear;
	if ((ts->spantype == TYPE_T1) || (ts->spantype == TYPE_J1)) {
		for (i=0;i<24;i++) {
			j = (i/8);
			if (ts->span.chans[i]->flags & DAHDI_FLAG_CLEAR) {
				val |= 1 << (7 - (i % 8));
				ts->notclear &= ~(1 << i);
			} else
				ts->notclear |= (1 << i);
			if ((i % 8)==7) {
				if (debug)
					dev_notice(&wc->dev->dev, "Putting %d "
						"in register %02x on span %d"
						"\n", val, 0x2f + j, span + 1);
				__t4_framer_out(wc, span, 0x2f + j, val);
				val = 0;
			}
		}
	} else {
		for (i=0;i<31;i++) {
			if (ts->span.chans[i]->flags & DAHDI_FLAG_CLEAR)
				ts->notclear &= ~(1 << i);
			else 
				ts->notclear |= (1 << i);
		}
	}
	if (ts->notclear != oldnotclear) {
		unsigned char reg;
		reg = __t4_framer_in(wc, span, FRMR_IMR0);
		if (ts->notclear)
			reg &= ~0x08;
		else
			reg |= 0x08;
		__t4_framer_out(wc, span, FRMR_IMR0, reg);
	}
}

#if 0
static void set_clear(struct t4 *wc, int span)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->reglock, flags);
	__set_clear(wc, span);
	spin_unlock_irqrestore(&wc->reglock, flags);
}
#endif

static int t4_dacs(struct dahdi_chan *dst, struct dahdi_chan *src)
{
	struct t4 *wc;
	struct t4_span *ts;
	wc = dst->pvt;
	ts = wc->tspans[dst->span->offset];
	if (src && (src->pvt != dst->pvt)) {
		if (ts->spanflags & FLAG_2NDGEN)
			t4_tsi_unassign(wc, dst->span->offset, dst->chanpos);
		wc = src->pvt;
		if (ts->spanflags & FLAG_2NDGEN)
			t4_tsi_unassign(wc, src->span->offset, src->chanpos);
		if (debug)
			dev_notice(&wc->dev->dev, "Unassigning %d/%d by "
				"default and...\n", src->span->offset,
				src->chanpos);
		if (debug)
			dev_notice(&wc->dev->dev, "Unassigning %d/%d by "
				"default\n", dst->span->offset, dst->chanpos);
		return -1;
	}
	if (src) {
		t4_tsi_assign(wc, src->span->offset, src->chanpos, dst->span->offset, dst->chanpos);
		if (debug)
			dev_notice(&wc->dev->dev, "Assigning channel %d/%d -> "
				"%d/%d!\n", src->span->offset, src->chanpos,
				dst->span->offset, dst->chanpos);
	} else {
		t4_tsi_unassign(wc, dst->span->offset, dst->chanpos);
		if (debug)
			dev_notice(&wc->dev->dev, "Unassigning channel %d/%d!"
				"\n", dst->span->offset, dst->chanpos);
	}
	return 0;
}

#ifdef VPM_SUPPORT

void oct_set_reg(void *data, unsigned int reg, unsigned int val)
{
	struct t4 *wc = data;
	t4_oct_out(wc, reg, val);
}

unsigned int oct_get_reg(void *data, unsigned int reg)
{
	struct t4 *wc = data;
	unsigned int ret;
	ret = t4_oct_in(wc, reg);
	return ret;
}

static int t4_vpm_unit(int span, int channel)
{
	int unit = 0;
	switch(vpmspans) {
	case 4:
		unit = span;
		unit += (channel & 1) << 2;
		break;
	case 2:
		unit = span;
		unit += (channel & 0x3) << 1;
		break;
	case 1:
		unit = span;
		unit += (channel & 0x7);
	}
	return unit;
}

static int t4_echocan_create(struct dahdi_chan *chan,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p,
			     struct dahdi_echocan_state **ec)
{
	struct t4 *wc = chan->pvt;
	struct t4_span *tspan = container_of(chan->span, struct t4_span, span);
	int channel;
	const struct dahdi_echocan_ops *ops;
	const struct dahdi_echocan_features *features;

	if (!vpmsupport || !wc->vpm)
		return -ENODEV;

	if (chan->span->offset >= vpmspans)
		return -ENODEV;

	if (wc->vpm450m) {
		ops = &vpm450m_ec_ops;
		features = &vpm450m_ec_features;
	} else {
		ops = &vpm400m_ec_ops;
		features = &vpm400m_ec_features;
	}

	if (ecp->param_count > 0) {
		dev_warn(&wc->dev->dev, "%s echo canceller does not support "
				"parameters; failing request\n", ops->name);
		return -EINVAL;
	}

	*ec = tspan->ec[chan->chanpos - 1];
	(*ec)->ops = ops;
	(*ec)->features = *features;

	channel = wc->t1e1 ? chan->chanpos : chan->chanpos + 4;

	if (wc->vpm450m) {
		channel = channel << 2;
		channel |= chan->span->offset;
		if (debug & DEBUG_ECHOCAN)
			dev_notice(&wc->dev->dev, "echocan: Card is %d, "
				"Channel is %d, Span is %d, offset is %d "
				"length %d\n", wc->num, chan->chanpos,
				chan->span->offset, channel, ecp->tap_length);
		vpm450m_setec(wc->vpm450m, channel, ecp->tap_length);
	} else {
		int unit = t4_vpm_unit(chan->span->offset, channel);

		if (debug & DEBUG_ECHOCAN)
			dev_notice(&wc->dev->dev, "echocan: Card is %d, "
				"Channel is %d, Span is %d, unit is %d, "
				"unit offset is %d length %d\n", wc->num,
				chan->chanpos, chan->span->offset, unit,
				channel, ecp->tap_length);
		t4_vpm_out(wc, unit, channel, 0x3e);
	}

	return 0;
}

static void echocan_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct t4 *wc = chan->pvt;
	int channel;

	memset(ec, 0, sizeof(*ec));

	channel = wc->t1e1 ? chan->chanpos : chan->chanpos + 4;

	if (wc->vpm450m) {
		channel = channel << 2;
		channel |= chan->span->offset;
		if (debug & DEBUG_ECHOCAN)
			dev_notice(&wc->dev->dev, "echocan: Card is %d, "
				"Channel is %d, Span is %d, offset is %d "
				"length 0\n", wc->num, chan->chanpos,
				chan->span->offset, channel);
		vpm450m_setec(wc->vpm450m, channel, 0);
	} else {
		int unit = t4_vpm_unit(chan->span->offset, channel);

		if (debug & DEBUG_ECHOCAN)
			dev_notice(&wc->dev->dev, "echocan: Card is %d, "
				"Channel is %d, Span is %d, unit is %d, "
				"unit offset is %d length 0\n", wc->num,
				chan->chanpos, chan->span->offset, unit,
				channel);
		t4_vpm_out(wc, unit, channel, 0x01);
	}
}
#endif

static int t4_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct t4_regs regs;
	int x;
	struct t4 *wc = chan->pvt;
#ifdef VPM_SUPPORT
	int j;
	int channel;
	struct t4_span *ts = wc->tspans[chan->span->offset];
#endif

#ifdef VPM_SUPPORT
	if (dtmfthreshold == 0)
		dtmfthreshold = VPM_DEFAULT_DTMFTHRESHOLD;
	if (lastdtmfthreshold != dtmfthreshold) {
		lastdtmfthreshold = dtmfthreshold;
		t4_vpm_set_dtmf_threshold(wc, dtmfthreshold);
	}
#endif

	switch(cmd) {
	case WCT4_GET_REGS:
		for (x=0;x<NUM_PCI;x++)
			regs.pci[x] = t4_pci_in(wc, x);
		for (x=0;x<NUM_REGS;x++)
			regs.regs[x] = t4_framer_in(wc, chan->span->offset, x);
		if (copy_to_user((__user void *) data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
#ifdef VPM_SUPPORT
	case DAHDI_TONEDETECT:
		if (get_user(j, (__user int *) data))
			return -EFAULT;
		if (!wc->vpm)
			return -ENOSYS;
		if (j && (vpmdtmfsupport == 0))
			return -ENOSYS;
		if (j & DAHDI_TONEDETECT_ON)
			set_bit(chan->chanpos - 1, &ts->dtmfmask);
		else
			clear_bit(chan->chanpos - 1, &ts->dtmfmask);
		if (j & DAHDI_TONEDETECT_MUTE)
			set_bit(chan->chanpos - 1, &ts->dtmfmutemask);
		else
			clear_bit(chan->chanpos - 1, &ts->dtmfmutemask);
		if (wc->vpm450m) {
			channel = (chan->chanpos) << 2;
			if (!wc->t1e1)
				channel += (4 << 2);
			channel |= chan->span->offset;
			vpm450m_setdtmf(wc->vpm450m, channel, j & DAHDI_TONEDETECT_ON, j & DAHDI_TONEDETECT_MUTE);
		}
		return 0;
#endif
	default:
		return -ENOTTY;
	}
	return 0;
}

static void inline t4_hdlc_xmit_fifo(struct t4 *wc, unsigned int span, struct t4_span *ts)
{
	int res, i;
	unsigned int size = 32;
	unsigned char buf[32];

	res = dahdi_hdlc_getbuf(ts->sigchan, buf, &size);
	if (debug & DEBUG_FRAMER)
		dev_notice(&wc->dev->dev, "Got buffer sized %d and res %d "
				"for %d\n", size, res, span);
	if (size > 0) {
		ts->sigactive = 1;

		if (debug & DEBUG_FRAMER) {
			dev_notice(&wc->dev->dev, "TX(");
			for (i = 0; i < size; i++)
				dev_notice(&wc->dev->dev, "%s%02x",
						(i ? " " : ""), buf[i]);
			dev_notice(&wc->dev->dev, ")\n");
		}

		for (i = 0; i < size; i++)
			t4_framer_out(wc, span, FRMR_TXFIFO, buf[i]);

		if (res) /* End of message */ {
			if (debug & DEBUG_FRAMER)
				dev_notice(&wc->dev->dev,
					"transmiting XHF|XME\n");
			t4_framer_cmd_wait(wc, span, FRMR_CMDR_XHF | FRMR_CMDR_XME);
#if 0
			ts->sigactive = (__t4_framer_in(wc, span, FRMR_SIS) & FRMR_SIS_XFW) ? 0 : 1;
#endif
			++ts->frames_out;
			if ((debug & DEBUG_FRAMER) && !(ts->frames_out & 0x0f))
				dev_notice(&wc->dev->dev, "Transmitted %d "
					"frames on span %d\n", ts->frames_out,
					span);
		} else { /* Still more to transmit */
			if (debug & DEBUG_FRAMER)
				dev_notice(&wc->dev->dev, "transmiting XHF\n");
			t4_framer_cmd_wait(wc, span, FRMR_CMDR_XHF);
		}
	}
	else if (res < 0)
		ts->sigactive = 0;
}

static void t4_hdlc_hard_xmit(struct dahdi_chan *chan)
{
	struct t4 *wc = chan->pvt;
	int span = chan->span->offset;
	struct t4_span *ts = wc->tspans[span];
	unsigned long flags; 

	spin_lock_irqsave(&wc->reglock, flags);
	if (!ts->sigchan) {
		dev_notice(&wc->dev->dev, "t4_hdlc_hard_xmit: Invalid (NULL) "
				"signalling channel\n");
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (debug & DEBUG_FRAMER)
		dev_notice(&wc->dev->dev, "t4_hdlc_hard_xmit on channel %s "
				"(sigchan %s), sigactive=%d\n", chan->name,
				ts->sigchan->name, ts->sigactive);

	if ((ts->sigchan == chan) && !ts->sigactive)
		t4_hdlc_xmit_fifo(wc, span, ts);
}

static int t4_maint(struct dahdi_span *span, int cmd)
{
	struct t4_span *ts = container_of(span, struct t4_span, span);
	struct t4 *wc = ts->owner;
	unsigned int reg;
	unsigned long flags;

	if (ts->spantype == TYPE_E1) {
		switch(cmd) {
		case DAHDI_MAINT_NONE:
			dev_info(&wc->dev->dev, "Clearing all maint modes\n");
			t4_clear_maint(span);
			break;
		case DAHDI_MAINT_LOCALLOOP:
			dev_info(&wc->dev->dev,
				 "Turning on local loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, LIM0_T);
			t4_framer_out(wc, span->offset, LIM0_T, (reg|LIM0_LL));
			break;
		case DAHDI_MAINT_NETWORKLINELOOP:
			dev_info(&wc->dev->dev,
				 "Turning on network line loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, LIM1_T);
			t4_framer_out(wc, span->offset, LIM1_T, (reg|LIM1_RL));
			break;
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
			dev_info(&wc->dev->dev,
				 "Turning on network payload loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, FMR2_T);
			t4_framer_out(wc, span->offset, FMR2_T, (reg|FMR2_PLB));
			break;
		case DAHDI_MAINT_LOOPUP:
		case DAHDI_MAINT_LOOPDOWN:
		case DAHDI_MAINT_LOOPSTOP:
			dev_info(&wc->dev->dev,
				"Loopup & loopdown supported in E1 mode\n");
			return -ENOSYS;
		case DAHDI_MAINT_FAS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IFASE);
			break;
		case DAHDI_MAINT_MULTI_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IMFE);
			break;
		case DAHDI_MAINT_CRC_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, ICRCE);
			break;
		case DAHDI_MAINT_CAS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, ICASE);
			break;
		case DAHDI_MAINT_PRBS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IPE);
			break;
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IBV);
			break;
		case DAHDI_RESET_COUNTERS:
			t4_reset_counters(span);
			break;
		case DAHDI_MAINT_ALARM_SIM:
			dev_info(&wc->dev->dev, "Invoking alarm state");
			reg = t4_framer_in(wc, span->offset, FMR0);
			t4_framer_out(wc, span->offset, FMR0, (reg|FMR0_SIM));
			break;
		default:
			dev_info(&wc->dev->dev,
					"Unknown E1 maint command: %d\n", cmd);
			return -ENOSYS;
		}
	} else {
		switch(cmd) {
		case DAHDI_MAINT_NONE:
			dev_info(&wc->dev->dev, "Clearing all maint modes\n");
			t4_clear_maint(span);
			break;
		case DAHDI_MAINT_LOCALLOOP:
			dev_info(&wc->dev->dev,
				 "Turning on local loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, LIM0_T);
			t4_framer_out(wc, span->offset, LIM0_T, (reg|LIM0_LL));
			break;
		case DAHDI_MAINT_NETWORKLINELOOP:
			dev_info(&wc->dev->dev,
				 "Turning on network line loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, LIM1_T);
			t4_framer_out(wc, span->offset, LIM1_T, (reg|LIM1_RL));
			break;
		case DAHDI_MAINT_NETWORKPAYLOADLOOP:
			dev_info(&wc->dev->dev,
				 "Turning on network payload loopback\n");
			t4_clear_maint(span);
			reg = t4_framer_in(wc, span->offset, FMR2_T);
			t4_framer_out(wc, span->offset, FMR2_T, (reg|FMR2_PLB));
			break;
		case DAHDI_MAINT_LOOPUP:
			dev_info(&wc->dev->dev, "Transmitting loopup code\n");
			t4_clear_maint(span);
			t4_framer_out(wc, span->offset, 0x21, 0x50);
			break;
		case DAHDI_MAINT_LOOPDOWN:
			dev_info(&wc->dev->dev, "Transmitting loopdown code\n");
			t4_clear_maint(span);
			t4_framer_out(wc, span->offset, 0x21, 0x60);
			break;
		case DAHDI_MAINT_LOOPSTOP:
			dev_info(&wc->dev->dev, "Transmitting loopstop code\n");
			t4_clear_maint(span);
			t4_framer_out(wc, span->offset, 0x21, 0x40);
			break;
		case DAHDI_MAINT_FAS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IFASE);
			break;
		case DAHDI_MAINT_MULTI_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IMFE);
			break;
		case DAHDI_MAINT_CRC_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, ICRCE);
			break;
		case DAHDI_MAINT_CAS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, ICASE);
			break;
		case DAHDI_MAINT_PRBS_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IPE);
			break;
		case DAHDI_MAINT_BIPOLAR_DEFECT:
			t4_framer_out(wc, span->offset, IERR_T, IBV);
			break;
		case DAHDI_MAINT_PRBS:
			dev_info(&wc->dev->dev, "PRBS not supported\n");
#if 0
			dev_notice(&wc->dev->dev, "Enabling PRBS!\n");
			span->mainttimer = 1;
			/* Enable PRBS monitor */
			reg = t4_framer_in(wc, span->offset, LCR1_T);
			reg |= EPRM;

			/* Setup PRBS xmit */
			t4_framer_out(wc, span->offset, TPC0_T, 0);

			/* Enable PRBS transmit */
			reg |= XPRBS;
			reg &= ~LLBP;
			reg &= ~FLLB;
			t4_framer_out(wc, span->offset, LCR1_T, reg);
#endif
			return -ENOSYS;
		case DAHDI_RESET_COUNTERS:
			t4_reset_counters(span);
			break;
		case DAHDI_MAINT_ALARM_SIM:
			reg = t4_framer_in(wc, span->offset, FMR0);

			/*
			 * The alarm simulation state machine requires us to
			 * bring this bit up and down for at least 1 clock cycle
			 */
			spin_lock_irqsave(&wc->reglock, flags);
			__t4_framer_out(wc, span->offset,
					FMR0, (reg | FMR0_SIM));
			udelay(1);
			__t4_framer_out(wc, span->offset,
					FMR0, (reg & ~FMR0_SIM));
			udelay(1);
			spin_unlock_irqrestore(&wc->reglock, flags);

			reg = t4_framer_in(wc, span->offset, 0x4e);
			if (debug & DEBUG_MAIN) {
				dev_info(&wc->dev->dev,
					"FRS2(alarm state): %d\n",
					((reg & 0xe0) >> 5));
			}
			break;
		default:
			dev_info(&wc->dev->dev, "Unknown T1 maint command:%d\n",
									cmd);
			break;
	   }
    }
	return 0;
}

static int t4_clear_maint(struct dahdi_span *span)
{
	struct t4_span *ts = container_of(span, struct t4_span, span);
	struct t4 *wc = ts->owner;
	unsigned int reg;

	/* Clear local loop */
	reg = t4_framer_in(wc, span->offset, LIM0_T);
	t4_framer_out(wc, span->offset, LIM0_T, (reg & ~LIM0_LL));

	/* Clear Remote Loop */
	reg = t4_framer_in(wc, span->offset, LIM1_T);
	t4_framer_out(wc, span->offset, LIM1_T, (reg & ~LIM1_RL));

	/* Clear Remote Payload Loop */
	reg = t4_framer_in(wc, span->offset, FMR2_T);
	t4_framer_out(wc, span->offset, FMR2_T, (reg & ~FMR2_PLB));

	/* Clear PRBS */
	reg = t4_framer_in(wc, span->offset, LCR1_T);
	t4_framer_out(wc, span->offset, LCR1_T, (reg & ~(XPRBS | EPRM)));

	span->mainttimer = 0;

	return 0;
}

static int t4_reset_counters(struct dahdi_span *span)
{
	struct t4_span *ts = container_of(span, struct t4_span, span);
	memset(&ts->span.count, 0, sizeof(ts->span.count));
	return 0;
}

static int t4_rbsbits(struct dahdi_chan *chan, int bits)
{
	u_char m,c;
	int k,n,b;
	struct t4 *wc = chan->pvt;
	struct t4_span *ts = wc->tspans[chan->span->offset];
	unsigned long flags;
	
	if (debug & DEBUG_RBS)
		dev_notice(&wc->dev->dev, "Setting bits to %d on channel %s\n",
				bits, chan->name);
	spin_lock_irqsave(&wc->reglock, flags);	
	k = chan->span->offset;
	if (ts->spantype == TYPE_E1) { /* do it E1 way */
		if (chan->chanpos == 16) {
			spin_unlock_irqrestore(&wc->reglock, flags);
			return 0;
		}
		n = chan->chanpos - 1;
		if (chan->chanpos > 15) n--;
		b = (n % 15);
		c = ts->txsigs[b];
		m = (n / 15) << 2; /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		ts->txsigs[b] = c;
		  /* output them to the chip */
		__t4_framer_out(wc,k,0x71 + b,c); 
	} else if (ts->span.lineconfig & DAHDI_CONFIG_D4) {
		n = chan->chanpos - 1;
		b = (n/4);
		c = ts->txsigs[b];
		m = ((3 - (n % 4)) << 1); /* nibble selector */
		c &= ~(0x3 << m); /* keep the other nibble */
		c |= ((bits >> 2) & 0x3) << m; /* put our new nibble here */
		ts->txsigs[b] = c;
		  /* output them to the chip */
		__t4_framer_out(wc,k,0x70 + b,c); 
		__t4_framer_out(wc,k,0x70 + b + 6,c); 
	} else if (ts->span.lineconfig & DAHDI_CONFIG_ESF) {
		n = chan->chanpos - 1;
		b = (n/2);
		c = ts->txsigs[b];
		m = ((n % 2) << 2); /* nibble selector */
		c &= (0xf << m); /* keep the other nibble */
		c |= (bits & 0xf) << (4 - m); /* put our new nibble here */
		ts->txsigs[b] = c;
		  /* output them to the chip */
		__t4_framer_out(wc,k,0x70 + b,c); 
	} 
	spin_unlock_irqrestore(&wc->reglock, flags);
	if (debug & DEBUG_RBS)
		dev_notice(&wc->dev->dev, "Finished setting RBS bits\n");
	return 0;
}

static int t4_shutdown(struct dahdi_span *span)
{
	int tspan;
	int wasrunning;
	unsigned long flags;
	struct t4_span *ts = container_of(span, struct t4_span, span);
	struct t4 *wc = ts->owner;

	tspan = span->offset + 1;
	if (tspan < 0) {
		dev_notice(&wc->dev->dev, "T%dXXP: Span '%d' isn't us?\n",
				wc->numspans, span->spanno);
		return -1;
	}

	if (debug & DEBUG_MAIN)
		dev_notice(&wc->dev->dev, "Shutting down span %d (%s)\n",
				span->spanno, span->name);

	/* Stop HDLC controller if runned */
	if (ts->sigchan)
		hdlc_stop(wc, span->offset);
	
	spin_lock_irqsave(&wc->reglock, flags);
	wasrunning = span->flags & DAHDI_FLAG_RUNNING;

	span->flags &= ~DAHDI_FLAG_RUNNING;
	__t4_set_led(wc, span->offset, WC_OFF);
	if (((wc->numspans == 4) && 
	    (!(wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[2]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[3]->span.flags & DAHDI_FLAG_RUNNING)))
	    			|| 
	    ((wc->numspans == 2) && 
	    (!(wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)))) {
		/* No longer in use, disable interrupts */
		dev_info(&wc->dev->dev, "TE%dXXP: Disabling interrupts since "
				"there are no active spans\n", wc->numspans);
		set_bit(T4_STOP_DMA, &wc->checkflag);
	} else
		set_bit(T4_CHECK_TIMING, &wc->checkflag);

	spin_unlock_irqrestore(&wc->reglock, flags);

	/* Wait for interrupt routine to shut itself down */
	msleep(10);
	if (wasrunning)
		wc->spansstarted--;

	if (debug & DEBUG_MAIN)
		dev_notice(&wc->dev->dev, "Span %d (%s) shutdown\n",
				span->spanno, span->name);
	return 0;
}

static void t4_chan_set_sigcap(struct dahdi_span *span, int x)
{
	struct t4_span *wc = container_of(span, struct t4_span, span);
	struct dahdi_chan *chan = wc->chans[x];
	chan->sigcap = DAHDI_SIG_CLEAR;
	/* E&M variant supported depends on span type */
	if (wc->spantype == TYPE_E1) {
		/* E1 sigcap setup */
		if (span->lineconfig & DAHDI_CONFIG_CCS) {
			/* CCS setup */
			chan->sigcap |= DAHDI_SIG_MTP2 | DAHDI_SIG_SF |
				DAHDI_SIG_HARDHDLC;
			return;
		}
		/* clear out sig and sigcap for channel 16 on E1 CAS
		 * lines, otherwise, set it correctly */
		if (x == 15) {
			/* CAS signaling channel setup */
			wc->chans[15]->sigcap = 0;
			wc->chans[15]->sig = 0;
			return;
		}
		/* normal CAS setup */
		chan->sigcap |= DAHDI_SIG_EM_E1 | DAHDI_SIG_FXSLS |
			DAHDI_SIG_FXSGS | DAHDI_SIG_FXSKS | DAHDI_SIG_SF |
			DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS | DAHDI_SIG_FXOKS |
			DAHDI_SIG_CAS | DAHDI_SIG_DACS_RBS;
	} else {
		/* T1 sigcap setup */
		chan->sigcap |= DAHDI_SIG_EM | DAHDI_SIG_FXSLS |
			DAHDI_SIG_FXSGS | DAHDI_SIG_FXSKS | DAHDI_SIG_MTP2 |
			DAHDI_SIG_SF | DAHDI_SIG_FXOLS | DAHDI_SIG_FXOGS |
			DAHDI_SIG_FXOKS | DAHDI_SIG_CAS | DAHDI_SIG_DACS_RBS |
			DAHDI_SIG_HARDHDLC;
	}
}

static int t4_spanconfig(struct dahdi_span *span, struct dahdi_lineconfig *lc)
{
	int i;
	struct t4_span *ts = container_of(span, struct t4_span, span);
	struct t4 *wc = ts->owner;

	if (debug)
		dev_info(&wc->dev->dev, "About to enter spanconfig!\n");
	if (debug & DEBUG_MAIN)
		dev_notice(&wc->dev->dev, "TE%dXXP: Configuring span %d\n",
				wc->numspans, span->spanno);

	if (lc->sync < 0)
		lc->sync = 0;
	if (lc->sync > 4)
		lc->sync = 0;
	
	/* remove this span number from the current sync sources, if there */
	for(i = 0; i < wc->numspans; i++) {
		if (wc->tspans[i]->sync == span->spanno) {
			wc->tspans[i]->sync = 0;
			wc->tspans[i]->psync = 0;
		}
	}
	wc->tspans[span->offset]->syncpos = lc->sync;
	/* if a sync src, put it in proper place */
	if (lc->sync) {
		wc->tspans[lc->sync - 1]->sync = span->spanno;
		wc->tspans[lc->sync - 1]->psync = span->offset + 1;
	}
	set_bit(T4_CHECK_TIMING, &wc->checkflag);

	/* Make sure this is clear in case of multiple startup and shutdown
	 * iterations */
	clear_bit(T4_STOP_DMA, &wc->checkflag);
	
	/* make sure that sigcaps gets updated if necessary */
	for (i = 0; i < span->channels; i++)
		t4_chan_set_sigcap(span, i);

	/* If we're already running, then go ahead and apply the changes */
	if (span->flags & DAHDI_FLAG_RUNNING)
		return t4_startup(span);

	if (debug)
		dev_info(&wc->dev->dev, "Done with spanconfig!\n");
	return 0;
}

static int t4_chanconfig(struct dahdi_chan *chan, int sigtype)
{
	int alreadyrunning;
	unsigned long flags;
	struct t4 *wc = chan->pvt;
	struct t4_span *ts = wc->tspans[chan->span->offset];

	alreadyrunning = ts->span.flags & DAHDI_FLAG_RUNNING;
	if (debug & DEBUG_MAIN) {
		if (alreadyrunning)
			dev_notice(&wc->dev->dev, "TE%dXXP: Reconfigured "
				"channel %d (%s) sigtype %d\n", wc->numspans,
				chan->channo, chan->name, sigtype);
		else
			dev_notice(&wc->dev->dev, "TE%dXXP: Configured channel"
				" %d (%s) sigtype %d\n", wc->numspans,
				chan->channo, chan->name, sigtype);
	}

	spin_lock_irqsave(&wc->reglock, flags);	

	if (alreadyrunning)
		__set_clear(wc, chan->span->offset);

	spin_unlock_irqrestore(&wc->reglock, flags);	

	/* (re)configure signalling channel */
	if ((sigtype == DAHDI_SIG_HARDHDLC) || (ts->sigchan == chan)) {
		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "%sonfiguring hardware HDLC "
				"on %s\n",
				((sigtype == DAHDI_SIG_HARDHDLC) ? "C" : "Unc"),
				chan->name);
		if (alreadyrunning) {
			if (ts->sigchan)
				hdlc_stop(wc, ts->sigchan->span->offset);
			if (sigtype == DAHDI_SIG_HARDHDLC) {
				if (hdlc_start(wc, chan->span->offset, chan, ts->sigmode)) {
					dev_notice(&wc->dev->dev, "Error "
						"initializing signalling "
						"controller\n");
					return -1;
				}
			} else {
				spin_lock_irqsave(&wc->reglock, flags);
				ts->sigchan = NULL;
				spin_unlock_irqrestore(&wc->reglock, flags);
			}
		
		}
		else {
			spin_lock_irqsave(&wc->reglock, flags);
			ts->sigchan = (sigtype == DAHDI_SIG_HARDHDLC) ? chan : NULL;
			spin_unlock_irqrestore(&wc->reglock, flags);
			ts->sigactive = 0;
		}
	}
	return 0;
}

static int t4_open(struct dahdi_chan *chan)
{
	return 0;
}

static int t4_close(struct dahdi_chan *chan)
{
	return 0;
}

static void set_span_devicetype(struct t4 *wc)
{
	int x;
	struct t4_span *ts;

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		dahdi_copy_string(ts->span.devicetype, wc->variety, sizeof(ts->span.devicetype));
		if (wc->vpm == T4_VPM_PRESENT) {
			if (!wc->vpm450m)
				strncat(ts->span.devicetype, " (VPM400M)", sizeof(ts->span.devicetype) - 1);
			else
				strncat(ts->span.devicetype, (wc->numspans > 2) ? " (VPMOCT128)" : " (VPMOCT064)",
					sizeof(ts->span.devicetype) - 1);
		}
	}
}

/* The number of cards we have seen with each
   possible 'order' switch setting.
*/
static unsigned int order_index[16];

static void setup_chunks(struct t4 *wc, int which)
{
	struct t4_span *ts;
	int offset = 1;
	int x, y;
	int gen2;

	if (!wc->t1e1)
		offset += 4;

	gen2 = (wc->tspans[0]->spanflags & FLAG_2NDGEN);

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		ts->writechunk = (void *)(wc->writechunk + (x * 32 * 2) + (which * (1024 >> 2)));
		ts->readchunk = (void *)(wc->readchunk + (x * 32 * 2) + (which * (1024 >> 2)));
		for (y=0;y<wc->tspans[x]->span.channels;y++) {
			struct dahdi_chan *mychans = ts->chans[y];
			if (gen2) {
				mychans->writechunk = (void *)(wc->writechunk + ((x * 32 + y + offset) * 2) + (which * (1024 >> 2)));
				mychans->readchunk = (void *)(wc->readchunk + ((x * 32 + y + offset) * 2) + (which * (1024 >> 2)));
			}
		}
	}
}

static const struct dahdi_span_ops t4_gen1_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = t4_spanconfig,
	.chanconfig = t4_chanconfig,
	.startup = t4_startup,
	.shutdown = t4_shutdown,
	.rbsbits = t4_rbsbits,
	.maint = t4_maint,
	.open = t4_open,
	.close  = t4_close,
	.ioctl = t4_ioctl,
	.hdlc_hard_xmit = t4_hdlc_hard_xmit,
};

static const struct dahdi_span_ops t4_gen2_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = t4_spanconfig,
	.chanconfig = t4_chanconfig,
	.startup = t4_startup,
	.shutdown = t4_shutdown,
	.rbsbits = t4_rbsbits,
	.maint = t4_maint,
	.open = t4_open,
	.close  = t4_close,
	.ioctl = t4_ioctl,
	.hdlc_hard_xmit = t4_hdlc_hard_xmit,
	.dacs = t4_dacs,
#ifdef VPM_SUPPORT
	.echocan_create = t4_echocan_create,
#endif
};

static void init_spans(struct t4 *wc)
{
	int x,y;
	int gen2;
	struct t4_span *ts;
	unsigned int reg;
	
	gen2 = (wc->tspans[0]->spanflags & FLAG_2NDGEN);
	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		sprintf(ts->span.name, "TE%d/%d/%d", wc->numspans, wc->num, x + 1);
		snprintf(ts->span.desc, sizeof(ts->span.desc) - 1,
			 "T%dXXP (PCI) Card %d Span %d", wc->numspans, wc->num, x+1);
		ts->span.manufacturer = "Digium";
		if (order_index[wc->order] == 1)
			snprintf(ts->span.location, sizeof(ts->span.location) - 1, "Board ID Switch %d", wc->order);
		else
			snprintf(ts->span.location, sizeof(ts->span.location) - 1,
				 "PCI%s Bus %02d Slot %02d", (ts->spanflags & FLAG_EXPRESS) ? " Express" : " ",
				 wc->dev->bus->number, PCI_SLOT(wc->dev->devfn) + 1);
		switch (ts->spantype) {
		case TYPE_T1:
			ts->span.spantype = "T1";
			break;
		case TYPE_E1:
			ts->span.spantype = "E1";
			break;
		case TYPE_J1:
			ts->span.spantype = "J1";
			break;
		}
		ts->span.irq = wc->dev->irq;

		/* HDLC Specific init */
		ts->sigchan = NULL;
		ts->sigmode = sigmode;
		ts->sigactive = 0;
		
		if (ts->spantype == TYPE_T1 || ts->spantype == TYPE_J1) {
			ts->span.channels = 24;
			ts->span.deflaw = DAHDI_LAW_MULAW;
			ts->span.linecompat = DAHDI_CONFIG_AMI |
				DAHDI_CONFIG_B8ZS | DAHDI_CONFIG_D4 |
				DAHDI_CONFIG_ESF;
		} else {
			ts->span.channels = 31;
			ts->span.deflaw = DAHDI_LAW_ALAW;
			ts->span.linecompat = DAHDI_CONFIG_AMI |
				DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS |
				DAHDI_CONFIG_CRC4;
		}
		ts->span.chans = ts->chans;
		ts->span.flags = DAHDI_FLAG_RBS;

		ts->owner = wc;
		ts->span.offset = x;
		ts->writechunk = (void *)(wc->writechunk + x * 32 * 2);
		ts->readchunk = (void *)(wc->readchunk + x * 32 * 2);
		init_waitqueue_head(&ts->span.maintq);

		if (gen2) {
			ts->span.ops = &t4_gen2_span_ops;
		} else {
			ts->span.ops = &t4_gen1_span_ops;
		}

		for (y=0;y<wc->tspans[x]->span.channels;y++) {
			struct dahdi_chan *mychans = ts->chans[y];
			sprintf(mychans->name, "TE%d/%d/%d/%d", wc->numspans, wc->num, x + 1, y + 1);
			t4_chan_set_sigcap(&ts->span, x);
			mychans->pvt = wc;
			mychans->chanpos = y + 1;
		}

		/* Enable 1sec timer interrupt */
		reg = t4_framer_in(wc, x, FMR1_T);
		t4_framer_out(wc, x, FMR1_T, (reg | FMR1_ECM));

		/* Enable Errored Second interrupt */
		t4_framer_out(wc, x, ESM, 0);

		t4_reset_counters(&ts->span);

	}

	set_span_devicetype(wc);
	setup_chunks(wc, 0);
	wc->lastindex = 0;
}

static void t4_serial_setup(struct t4 *wc, int unit)
{
	if (!wc->globalconfig) {
		wc->globalconfig = 1;
		if (debug)
			dev_info(&wc->dev->dev, "TE%dXXP: Setting up global "
					"serial parameters\n", wc->numspans);
		t4_framer_out(wc, 0, 0x85, 0xe0);	/* GPC1: Multiplex mode enabled, FSC is output, active low, RCLK from channel 0 */
		t4_framer_out(wc, 0, 0x08, 0x01);	/* IPC: Interrupt push/pull active low */
	
		/* Global clocks (8.192 Mhz CLK) */
		t4_framer_out(wc, 0, 0x92, 0x00);	
		t4_framer_out(wc, 0, 0x93, 0x18);
		t4_framer_out(wc, 0, 0x94, 0xfb);
		t4_framer_out(wc, 0, 0x95, 0x0b);
		t4_framer_out(wc, 0, 0x96, 0x00);
		t4_framer_out(wc, 0, 0x97, 0x0b);
		t4_framer_out(wc, 0, 0x98, 0xdb);
		t4_framer_out(wc, 0, 0x99, 0xdf);
	}

	/* Configure interrupts */	
	t4_framer_out(wc, unit, FRMR_GCR, 0x00);	/* GCR: Interrupt on Activation/Deactivation of each */

	/* Configure system interface */
	t4_framer_out(wc, unit, FRMR_SIC1, 0xc2);	/* SIC1: 8.192 Mhz clock/bus, double buffer receive / transmit, byte interleaved */
	t4_framer_out(wc, unit, FRMR_SIC2, 0x20 | (unit << 1)); /* SIC2: No FFS, no center receive eliastic buffer, phase */
	t4_framer_out(wc, unit, FRMR_SIC3, 0x04);	/* SIC3: Edges for capture */
	t4_framer_out(wc, unit, FRMR_CMR2, 0x00);	/* CMR2: We provide sync and clock for tx and rx. */
	if (!wc->t1e1) { /* T1 mode */
		t4_framer_out(wc, unit, FRMR_XC0, 0x03);	/* XC0: Normal operation of Sa-bits */
		t4_framer_out(wc, unit, FRMR_XC1, 0x84);	/* XC1: 0 offset */
		if (wc->tspans[unit]->spantype == TYPE_J1)
			t4_framer_out(wc, unit, FRMR_RC0, 0x83);	/* RC0: Just shy of 1023 */
		else
			t4_framer_out(wc, unit, FRMR_RC0, 0x03);	/* RC0: Just shy of 1023 */
		t4_framer_out(wc, unit, FRMR_RC1, 0x84);	/* RC1: The rest of RC0 */
	} else { /* E1 mode */
		t4_framer_out(wc, unit, FRMR_XC0, 0x00);	/* XC0: Normal operation of Sa-bits */
		t4_framer_out(wc, unit, FRMR_XC1, 0x04);	/* XC1: 0 offset */
		t4_framer_out(wc, unit, FRMR_RC0, 0x04);	/* RC0: Just shy of 1023 */
		t4_framer_out(wc, unit, FRMR_RC1, 0x04);	/* RC1: The rest of RC0 */
	}
	
	/* Configure ports */
	t4_framer_out(wc, unit, 0x80, 0x00);	/* PC1: SPYR/SPYX input on RPA/XPA */
	if (wc->falc31) {
			  t4_framer_out(wc, unit, 0x81, 0xBB);	/* PC2: RMFB/XSIG output/input on RPB/XPB */
			  t4_framer_out(wc, unit, 0x82, 0xBB);	/* PC3: Some unused stuff */
			  t4_framer_out(wc, unit, 0x83, 0xBB);	/* PC4: Some more unused stuff */
	} else {
			  t4_framer_out(wc, unit, 0x81, 0x22);	/* PC2: RMFB/XSIG output/input on RPB/XPB */
			  t4_framer_out(wc, unit, 0x82, 0x65);	/* PC3: Some unused stuff */
			  t4_framer_out(wc, unit, 0x83, 0x35);	/* PC4: Some more unused stuff */
	}
	t4_framer_out(wc, unit, 0x84, 0x01);	/* PC5: XMFS active low, SCLKR is input, RCLK is output */
	if (debug & DEBUG_MAIN)
		dev_notice(&wc->dev->dev, "Successfully initialized serial "
				"bus for unit %d\n", unit);
}

static int syncsrc = 0;
static int syncnum = 0 /* -1 */;
static int syncspan = 0;
#ifdef DEFINE_SPINLOCK
static DEFINE_SPINLOCK(synclock);
#else
static spinlock_t synclock = SPIN_LOCK_UNLOCKED;
#endif

static void __t4_set_rclk_src(struct t4 *wc, int span)
{
	int cmr1 = 0x38;	/* Clock Mode: RCLK sourced by DCO-R1
				   by default, Disable Clock-Switching */

	cmr1 |= (span << 6);
	__t4_framer_out(wc, 0, 0x44, cmr1);

	dev_info(&wc->dev->dev, "RCLK source set to span %d\n", span+1);
}

static void __t4_set_sclk_src(struct t4 *wc, int mode, int master, int slave)
{
	if (slave) {
		wc->dmactrl |= (1 << 25);
		dev_info(&wc->dev->dev, "SCLK is slaved to timing cable\n");
	} else {
		wc->dmactrl &= ~(1 << 25);
	}

	if (master) {
		wc->dmactrl |= (1 << 24);
		dev_info(&wc->dev->dev, "SCLK is master to timing cable\n");
	} else {
		wc->dmactrl &= ~(1 << 24);
	}

	if (mode == WC_RECOVER)
		wc->dmactrl |= (1 << 29); /* Recover timing from RCLK */

	if (mode == WC_SELF)
		wc->dmactrl &= ~(1 << 29);/* Provide timing from MCLK */

	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18))
static ssize_t t4_timing_master_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct t4 *wc = dev_get_drvdata(dev);
	if (wc->dmactrl & (1 << 29))
		return sprintf(buf, "%d\n", wc->syncsrc);
	else
		return sprintf(buf, "%d\n", -1);
}

static DEVICE_ATTR(timing_master, 0400, t4_timing_master_show, NULL);

static void create_sysfs_files(struct t4 *wc)
{
	int ret;
	ret = device_create_file(&wc->dev->dev,
				 &dev_attr_timing_master);
	if (ret) {
		dev_info(&wc->dev->dev,
			"Failed to create device attributes.\n");
	}
}

static void remove_sysfs_files(struct t4 *wc)
{
	device_remove_file(&wc->dev->dev,
			   &dev_attr_timing_master);
}

#else

static inline void create_sysfs_files(struct t4 *wc) { return; }
static inline void remove_sysfs_files(struct t4 *wc) { return; }

#endif /* LINUX_KERNEL > 2.6.18 */

static inline void __t4_update_timing(struct t4 *wc)
{
	int i;
	/* update sync src info */
	if (wc->syncsrc != syncsrc) {
		dev_info(&wc->dev->dev, "Swapping card %d from %d to %d\n",
				wc->num, wc->syncsrc, syncsrc);
		wc->syncsrc = syncsrc;
		/* Update sync sources */
		for (i = 0; i < wc->numspans; i++) {
			wc->tspans[i]->span.syncsrc = wc->syncsrc;
		}
		if (syncnum == wc->num) {
			__t4_set_rclk_src(wc, syncspan-1);
			__t4_set_sclk_src(wc, WC_RECOVER, 1, 0);
			if (debug)
				dev_notice(&wc->dev->dev, "Card %d, using sync "
					"span %d, master\n", wc->num, syncspan);
		} else {
			__t4_set_sclk_src(wc, WC_RECOVER, 0, 1);
			if (debug)
				dev_notice(&wc->dev->dev, "Card %d, using "
					"Timing Bus, NOT master\n", wc->num);
		}
	}
}

static int __t4_findsync(struct t4 *wc)
{
	int i;
	int x;
	unsigned long flags;
	int p;
	int nonzero;
	int newsyncsrc = 0;			/* DAHDI span number */
	int newsyncnum = 0;			/* wct4xxp card number */
	int newsyncspan = 0;		/* span on given wct4xxp card */
	spin_lock_irqsave(&synclock, flags);
#if 1
	if (!wc->num) {
		/* If we're the first card, go through all the motions, up to 8 levels
		   of sync source */
		p = 1;
		while (p < 8) {
			nonzero = 0;
			for (x=0;cards[x];x++) {
				for (i = 0; i < wc->numspans; i++) {
					if (cards[x]->tspans[i]->syncpos) {
						nonzero = 1;
						if ((cards[x]->tspans[i]->syncpos == p) &&
						    !(cards[x]->tspans[i]->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_LOOPBACK)) &&
							(cards[x]->tspans[i]->span.flags & DAHDI_FLAG_RUNNING)) {
								/* This makes a good sync source */
								newsyncsrc = cards[x]->tspans[i]->span.spanno;
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
			if (debug)
				dev_notice(&wc->dev->dev, "New syncnum: %d "
					"(was %d), syncsrc: %d (was %d), "
					"syncspan: %d (was %d)\n", newsyncnum,
					syncnum, newsyncsrc, syncsrc,
					newsyncspan, syncspan);
			syncnum = newsyncnum;
			syncsrc = newsyncsrc;
			syncspan = newsyncspan;
			for (x=0;cards[x];x++) {
				__t4_update_timing(cards[x]);
			}
		}
	}
	__t4_update_timing(wc);
#endif	
	spin_unlock_irqrestore(&synclock, flags);
	return 0;
}

static void __t4_set_timing_source_auto(struct t4 *wc)
{
	int x;
	int firstprio, secondprio;
	firstprio = secondprio = 4;

	if (debug)
		dev_info(&wc->dev->dev, "timing source auto\n");
	clear_bit(T4_CHECK_TIMING, &wc->checkflag);
	if (timingcable) {
		__t4_findsync(wc);
	} else {
		if (debug)
			dev_info(&wc->dev->dev, "Evaluating spans for timing "
					"source\n");
		for (x=0;x<wc->numspans;x++) {
			if ((wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) &&
			   !(wc->tspans[x]->span.alarms & (DAHDI_ALARM_RED |
							   DAHDI_ALARM_BLUE))) {
				if (debug)
					dev_info(&wc->dev->dev, "span %d is "
						"green : syncpos %d\n", x+1,
						wc->tspans[x]->syncpos);
				if (wc->tspans[x]->syncpos) {
					/* Valid rsync source in recovered
					   timing mode */
					if (firstprio == 4)
						firstprio = x;
					else if (wc->tspans[x]->syncpos <
						wc->tspans[firstprio]->syncpos)
						firstprio = x;
				} else {
					/* Valid rsync source in system timing
					   mode */
					if (secondprio == 4)
						secondprio = x;
				}
			}
		}
		if (firstprio != 4) {
			wc->syncsrc = firstprio;
			__t4_set_rclk_src(wc, firstprio);
			__t4_set_sclk_src(wc, WC_RECOVER, 0, 0);
			dev_info(&wc->dev->dev, "Recovered timing mode, "\
						"RCLK set to span %d\n",
						firstprio+1);
		} else if (secondprio != 4) {
			wc->syncsrc = -1;
			__t4_set_rclk_src(wc, secondprio);
			__t4_set_sclk_src(wc, WC_SELF, 0, 0);
			dev_info(&wc->dev->dev, "System timing mode, "\
						"RCLK set to span %d\n",
						secondprio+1);
		} else {
			wc->syncsrc = -1;
			dev_info(&wc->dev->dev, "All spans in alarm : No valid"\
						"span to source RCLK from\n");
			/* Default rclk to lock with span 1 */
			__t4_set_rclk_src(wc, 0);
			__t4_set_sclk_src(wc, WC_SELF, 0, 0);
		}
	}
}

static void __t4_configure_t1(struct t4 *wc, int unit, int lineconfig, int txlevel)
{
	unsigned int fmr4, fmr2, fmr1, fmr0, lim2;
	char *framing, *line;
	int mytxlevel;
	if ((txlevel > 7) || (txlevel < 4))
		mytxlevel = 0;
	else
		mytxlevel = txlevel - 4;
	fmr1 = 0x9c; /* FMR1: Mode 1, T1 mode, CRC on for ESF, 8.192 Mhz system data rate, no XAIS */
	fmr2 = 0x20; /* FMR2: no payload loopback, don't auto yellow */
	fmr4 = 0x0c; /* FMR4: Lose sync on 2 out of 5 framing bits, auto resync */
	lim2 = 0x21; /* LIM2: 50% peak is a "1", Advanced Loss recovery */
	lim2 |= (mytxlevel << 6);	/* LIM2: Add line buildout */
	__t4_framer_out(wc, unit, 0x1d, fmr1);
	__t4_framer_out(wc, unit, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		/* workaround for errata #2 in ES v3 09-10-16 */
		fmr0 = (wc->falc31) ? 0xb0 : 0xa0;
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
	__t4_framer_out(wc, unit, 0x1c, fmr0);
	__t4_framer_out(wc, unit, 0x20, fmr4);
	__t4_framer_out(wc, unit, 0x21, 0x40);	/* FMR5: Enable RBS mode */

	__t4_framer_out(wc, unit, 0x37, 0xf0 );	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No remote loop, no DRS */
	__t4_framer_out(wc, unit, 0x36, 0x08);	/* LIM0: Enable auto long haul mode, no local loop (must be after LIM1) */

	__t4_framer_out(wc, unit, 0x02, 0x50);	/* CMDR: Reset the receiver and transmitter line interface */
	__t4_framer_out(wc, unit, 0x02, 0x00);	/* CMDR: Reset the receiver and transmitter line interface */

	if (wc->falc31) {
		if (debug)
			dev_info(&wc->dev->dev, "card %d span %d: setting Rtx "
					"to 0ohm for T1\n", wc->num, unit);
		__t4_framer_out(wc, unit, 0x86, 0x00);	/* PC6: set Rtx to 0ohm for T1 */

		// Hitting the bugfix register to fix errata #3
		__t4_framer_out(wc, unit, 0xbd, 0x05);
	}

	__t4_framer_out(wc, unit, 0x3a, lim2);	/* LIM2: 50% peak amplitude is a "1" */
	__t4_framer_out(wc, unit, 0x38, 0x0a);	/* PCD: LOS after 176 consecutive "zeros" */
	__t4_framer_out(wc, unit, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */
	
	/* Generate pulse mask for T1 */
	switch(mytxlevel) {
	case 3:
		__t4_framer_out(wc, unit, 0x26, 0x07);	/* XPM0 */
		__t4_framer_out(wc, unit, 0x27, 0x01);	/* XPM1 */
		__t4_framer_out(wc, unit, 0x28, 0x00);	/* XPM2 */
		break;
	case 2:
		__t4_framer_out(wc, unit, 0x26, 0x8c);	/* XPM0 */
		__t4_framer_out(wc, unit, 0x27, 0x11);	/* XPM1 */
		__t4_framer_out(wc, unit, 0x28, 0x01);	/* XPM2 */
		break;
	case 1:
		__t4_framer_out(wc, unit, 0x26, 0x8c);	/* XPM0 */
		__t4_framer_out(wc, unit, 0x27, 0x01);	/* XPM1 */
		__t4_framer_out(wc, unit, 0x28, 0x00);	/* XPM2 */
		break;
	case 0:
	default:
		__t4_framer_out(wc, unit, 0x26, 0xd7);	/* XPM0 */
		__t4_framer_out(wc, unit, 0x27, 0x22);	/* XPM1 */
		__t4_framer_out(wc, unit, 0x28, 0x01);	/* XPM2 */
		break;
	}

	/* Don't mask framer interrupts if hardware HDLC is in use */
	__t4_framer_out(wc, unit, FRMR_IMR0, 0xff & ~((wc->tspans[unit]->sigchan) ? HDLC_IMR0_MASK : 0));	/* IMR0: We care about CAS changes, etc */
	__t4_framer_out(wc, unit, FRMR_IMR1, 0xff & ~((wc->tspans[unit]->sigchan) ? HDLC_IMR1_MASK : 0));	/* IMR1: We care about nothing */
	__t4_framer_out(wc, unit, 0x16, 0x00);	/* IMR2: All the alarm stuff! */
	__t4_framer_out(wc, unit, 0x17, 0x34);	/* IMR3: AIS and friends */
	__t4_framer_out(wc, unit, 0x18, 0x3f);  /* IMR4: Slips on transmit */

	dev_info(&wc->dev->dev, "Span %d configured for %s/%s\n", unit + 1,
			framing, line);
}

static void __t4_configure_e1(struct t4 *wc, int unit, int lineconfig)
{
	unsigned int fmr2, fmr1, fmr0;
	unsigned int cas = 0;
	unsigned int imr3extra=0;
	char *crc4 = "";
	char *framing, *line;
	fmr1 = 0x44; /* FMR1: E1 mode, Automatic force resync, PCM30 mode, 8.192 Mhz backplane, no XAIS */
	fmr2 = 0x03; /* FMR2: Auto transmit remote alarm, auto loss of multiframe recovery, no payload loopback */
	if (lineconfig & DAHDI_CONFIG_CRC4) {
		fmr1 |= 0x08;	/* CRC4 transmit */
		fmr2 |= 0xc0;	/* CRC4 receive */
		crc4 = "/CRC4";
	}
	__t4_framer_out(wc, unit, 0x1d, fmr1);
	__t4_framer_out(wc, unit, 0x1e, fmr2);

	/* Configure line interface */
	if (lineconfig & DAHDI_CONFIG_AMI) {
		line = "AMI";
		/* workaround for errata #2 in ES v3 09-10-16 */
		fmr0 = (wc->falc31) ? 0xb0 : 0xa0;
	} else {
		line = "HDB3";
		fmr0 = 0xf0;
	}
	if (lineconfig & DAHDI_CONFIG_CCS) {
		framing = "CCS";
		imr3extra = 0x28;
	} else {
		framing = "CAS";
		cas = 0x40;
	}
	__t4_framer_out(wc, unit, 0x1c, fmr0);

	__t4_framer_out(wc, unit, 0x37, 0xf0 /*| 0x6 */ );	/* LIM1: Clear data in case of LOS, Set receiver threshold (0.5V), No remote loop, no DRS */
	__t4_framer_out(wc, unit, 0x36, 0x08);	/* LIM0: Enable auto long haul mode, no local loop (must be after LIM1) */

	__t4_framer_out(wc, unit, 0x02, 0x50);	/* CMDR: Reset the receiver and transmitter line interface */
	__t4_framer_out(wc, unit, 0x02, 0x00);	/* CMDR: Reset the receiver and transmitter line interface */

	if (wc->falc31) {
		if (debug)
			dev_info(&wc->dev->dev,
					"setting Rtx to 7.5ohm for E1\n");
		__t4_framer_out(wc, unit, 0x86, 0x40);	/* PC6: turn on 7.5ohm Rtx for E1 */
	}

	/* Condition receive line interface for E1 after reset */
	__t4_framer_out(wc, unit, 0xbb, 0x17);
	__t4_framer_out(wc, unit, 0xbc, 0x55);
	__t4_framer_out(wc, unit, 0xbb, 0x97);
	__t4_framer_out(wc, unit, 0xbb, 0x11);
	__t4_framer_out(wc, unit, 0xbc, 0xaa);
	__t4_framer_out(wc, unit, 0xbb, 0x91);
	__t4_framer_out(wc, unit, 0xbb, 0x12);
	__t4_framer_out(wc, unit, 0xbc, 0x55);
	__t4_framer_out(wc, unit, 0xbb, 0x92);
	__t4_framer_out(wc, unit, 0xbb, 0x0c);
	__t4_framer_out(wc, unit, 0xbb, 0x00);
	__t4_framer_out(wc, unit, 0xbb, 0x8c);
	
	__t4_framer_out(wc, unit, 0x3a, 0x20);	/* LIM2: 50% peak amplitude is a "1" */
	__t4_framer_out(wc, unit, 0x38, 0x0a);	/* PCD: LOS after 176 consecutive "zeros" */
	__t4_framer_out(wc, unit, 0x39, 0x15);	/* PCR: 22 "ones" clear LOS */
	
	__t4_framer_out(wc, unit, 0x20, 0x9f);	/* XSW: Spare bits all to 1 */
	__t4_framer_out(wc, unit, 0x21, 0x1c|cas);	/* XSP: E-bit set when async. AXS auto, XSIF to 1 */
	
	
	/* Generate pulse mask for E1 */
	__t4_framer_out(wc, unit, 0x26, 0x54);	/* XPM0 */
	__t4_framer_out(wc, unit, 0x27, 0x02);	/* XPM1 */
	__t4_framer_out(wc, unit, 0x28, 0x00);	/* XPM2 */

	/* Don't mask framer interrupts if hardware HDLC is in use */
	__t4_framer_out(wc, unit, FRMR_IMR0, 0xff & ~((wc->tspans[unit]->sigchan) ? HDLC_IMR0_MASK : 0));	/* IMR0: We care about CRC errors, CAS changes, etc */
	__t4_framer_out(wc, unit, FRMR_IMR1, 0x3f & ~((wc->tspans[unit]->sigchan) ? HDLC_IMR1_MASK : 0));	/* IMR1: We care about loopup / loopdown */
	__t4_framer_out(wc, unit, 0x16, 0x00);	/* IMR2: We care about all the alarm stuff! */
	__t4_framer_out(wc, unit, 0x17, 0x04 | imr3extra); /* IMR3: AIS */
	__t4_framer_out(wc, unit, 0x18, 0x3f);  /* IMR4: We care about slips on transmit */

	dev_info(&wc->dev->dev, "TE%dXXP: Span %d configured for %s/%s%s\n",
			wc->numspans, unit + 1, framing, line, crc4);
}

static int t4_startup(struct dahdi_span *span)
{
#ifdef SUPPORT_GEN1
	int i;
#endif
	int tspan;
	unsigned long flags;
	int alreadyrunning;
	struct t4_span *ts = container_of(span, struct t4_span, span);
	struct t4 *wc = ts->owner;

	set_bit(T4_IGNORE_LATENCY, &wc->checkflag);
	if (debug)
		dev_info(&wc->dev->dev, "About to enter startup!\n");
	tspan = span->offset + 1;
	if (tspan < 0) {
		dev_info(&wc->dev->dev, "TE%dXXP: Span '%d' isn't us?\n",
				wc->numspans, span->spanno);
		return -1;
	}

	spin_lock_irqsave(&wc->reglock, flags);

	alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

#ifdef SUPPORT_GEN1
	/* initialize the start value for the entire chunk of last ec buffer */
	for(i = 0; i < span->channels; i++)
	{
		memset(ts->ec_chunk1[i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
		memset(ts->ec_chunk2[i],
			DAHDI_LIN2X(0,span->chans[i]),DAHDI_CHUNKSIZE);
	}
#endif
	/* Force re-evaluation of timing source */
	wc->syncsrc = -1;
	set_bit(T4_CHECK_TIMING, &wc->checkflag);

	if (ts->spantype == TYPE_E1) { /* if this is an E1 card */
		__t4_configure_e1(wc, span->offset, span->lineconfig);
	} else { /* is a T1 card */
		__t4_configure_t1(wc, span->offset, span->lineconfig, span->txlevel);
	}

	/* Note clear channel status */
	wc->tspans[span->offset]->notclear = 0;
	__set_clear(wc, span->offset);
	
	if (!alreadyrunning) {
		span->flags |= DAHDI_FLAG_RUNNING;
		wc->spansstarted++;

		if (wc->flags & FLAG_5THGEN)
			__t4_pci_out(wc, 5, (ms_per_irq << 16) | wc->numbufs);
		/* enable interrupts */
		/* Start DMA, enabling DMA interrupts on read only */
#if 0
		/* Enable framer only interrupts */
		wc->dmactrl |= 1 << 27;
#endif
		wc->dmactrl |= (ts->spanflags & FLAG_2NDGEN) ? 0xc0000000 : 0xc0000003;
#ifdef VPM_SUPPORT
		wc->dmactrl |= wc->vpm;
#endif
		/* Seed interrupt register */
		__t4_pci_out(wc, WC_INTR, 0x0c);
		if (noburst || !(ts->spanflags & FLAG_BURST))
			wc->dmactrl |= (1 << 26);
		__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);

		/* Startup HDLC controller too */
	}

	if (ts->sigchan) {
		struct dahdi_chan *sigchan = ts->sigchan;

		spin_unlock_irqrestore(&wc->reglock, flags);
		if (hdlc_start(wc, span->offset, sigchan, ts->sigmode)) {
			dev_notice(&wc->dev->dev, "Error initializing "
					"signalling controller\n");
			return -1;
		}
		spin_lock_irqsave(&wc->reglock, flags);
	}

	spin_unlock_irqrestore(&wc->reglock, flags);

	t4_check_alarms(wc, span->offset);
	t4_check_sigbits(wc, span->offset);

	if (wc->tspans[0]->sync == span->spanno)
		dev_info(&wc->dev->dev, "SPAN %d: Primary Sync Source\n",
				span->spanno);
	if (wc->tspans[1]->sync == span->spanno)
		dev_info(&wc->dev->dev, "SPAN %d: Secondary Sync Source\n",
				span->spanno);
	if (wc->numspans == 4) {
		if (wc->tspans[2]->sync == span->spanno)
			dev_info(&wc->dev->dev, "SPAN %d: Tertiary Sync Source"
					"\n", span->spanno);
		if (wc->tspans[3]->sync == span->spanno)
			dev_info(&wc->dev->dev, "SPAN %d: Quaternary Sync "
					"Source\n", span->spanno);
	}
#ifdef VPM_SUPPORT
	if (!alreadyrunning && !wc->vpm) {
		wait_a_little();
		t4_vpm400_init(wc);
		if (!wc->vpm)
			t4_vpm450_init(wc);
		wc->dmactrl |= wc->vpm;
		t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
		if (wc->vpm)
			set_span_devicetype(wc);
	}
#endif
	if (debug)
		dev_info(&wc->dev->dev, "Completed startup!\n");
	clear_bit(T4_IGNORE_LATENCY, &wc->checkflag);
	return 0;
}

#ifdef SUPPORT_GEN1
static inline void e1_check(struct t4 *wc, int span, int val)
{
	struct t4_span *ts = wc->tspans[span];
	if ((ts->span.channels > 24) &&
	    (ts->span.flags & DAHDI_FLAG_RUNNING) &&
	    !(ts->span.alarms) &&
	    (!wc->e1recover))   {
		if (val != 0x1b) {
			ts->e1check++;
		} else
			ts->e1check = 0;
		if (ts->e1check > 100) {
			/* Wait 1000 ms */
			wc->e1recover = 1000 * 8;
			wc->tspans[0]->e1check = wc->tspans[1]->e1check = 0;
			if (wc->numspans == 4)
				wc->tspans[2]->e1check = wc->tspans[3]->e1check = 0;
			if (debug & DEBUG_MAIN)
				dev_notice(&wc->dev->dev, "Detected loss of "
					"E1 alignment on span %d!\n", span);
			t4_reset_dma(wc);
		}
	}
}

static void t4_receiveprep(struct t4 *wc, int irq)
{
	volatile unsigned int *readchunk;
	int dbl = 0;
	int x,y,z;
	unsigned int tmp;
	int offset=0;
	if (!wc->t1e1)
		offset = 4;
	if (irq & 1) {
		/* First part */
		readchunk = wc->readchunk;
		if (!wc->last0) 
			dbl = 1;
		wc->last0 = 0;
	} else {
		readchunk = wc->readchunk + DAHDI_CHUNKSIZE * 32;
		if (wc->last0) 
			dbl = 1;
		wc->last0 = 1;
	}
	if (dbl) {
		for (x=0;x<wc->numspans;x++)
			wc->tspans[x]->irqmisses++;
		if (debug & DEBUG_MAIN)
			dev_notice(&wc->dev->dev, "TE%dXXP: Double/missed "
				"interrupt detected\n", wc->numspans);
	}
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		for (z=0;z<24;z++) {
			/* All T1/E1 channels */
			tmp = readchunk[z+1+offset];
			if (wc->numspans == 4) {
				wc->tspans[3]->span.chans[z]->readchunk[x] = tmp & 0xff;
				wc->tspans[2]->span.chans[z]->readchunk[x] = (tmp & 0xff00) >> 8;
			}
			wc->tspans[1]->span.chans[z]->readchunk[x] = (tmp & 0xff0000) >> 16;
			wc->tspans[0]->span.chans[z]->readchunk[x] = tmp >> 24;
		}
		if (wc->t1e1) {
			if (wc->e1recover > 0)
				wc->e1recover--;
			tmp = readchunk[0];
			if (wc->numspans == 4) {
				e1_check(wc, 3, (tmp & 0x7f));
				e1_check(wc, 2, (tmp & 0x7f00) >> 8);
			}
			e1_check(wc, 1, (tmp & 0x7f0000) >> 16);
			e1_check(wc, 0, (tmp & 0x7f000000) >> 24);
			for (z=24;z<31;z++) {
				/* Only E1 channels now */
				tmp = readchunk[z+1];
				if (wc->numspans == 4) {
					if (wc->tspans[3]->span.channels > 24)
						wc->tspans[3]->span.chans[z]->readchunk[x] = tmp & 0xff;
					if (wc->tspans[2]->span.channels > 24)
						wc->tspans[2]->span.chans[z]->readchunk[x] = (tmp & 0xff00) >> 8;
				}
				if (wc->tspans[1]->span.channels > 24)
					wc->tspans[1]->span.chans[z]->readchunk[x] = (tmp & 0xff0000) >> 16;
				if (wc->tspans[0]->span.channels > 24)
					wc->tspans[0]->span.chans[z]->readchunk[x] = tmp >> 24;
			}
		}
		/* Advance pointer by 4 TDM frame lengths */
		readchunk += 32;
	}
	for (x=0;x<wc->numspans;x++) {
		if (wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) {
			for (y=0;y<wc->tspans[x]->span.channels;y++) {
				/* Echo cancel double buffered data */
				dahdi_ec_chunk(wc->tspans[x]->span.chans[y], 
				    wc->tspans[x]->span.chans[y]->readchunk, 
					wc->tspans[x]->ec_chunk2[y]);
				memcpy(wc->tspans[x]->ec_chunk2[y],wc->tspans[x]->ec_chunk1[y],
					DAHDI_CHUNKSIZE);
				memcpy(wc->tspans[x]->ec_chunk1[y],
					wc->tspans[x]->span.chans[y]->writechunk,
						DAHDI_CHUNKSIZE);
			}
			dahdi_receive(&wc->tspans[x]->span);
		}
	}
}
#endif

#if (DAHDI_CHUNKSIZE != 8)
#error Sorry, nextgen does not support chunksize != 8
#endif

static inline void __receive_span(struct t4_span *ts)
{
#ifdef VPM_SUPPORT
	int y;
	unsigned long merged;
	merged = ts->dtmfactive & ts->dtmfmutemask;
	if (merged) {
		for (y=0;y<ts->span.channels;y++) {
			/* Mute any DTMFs which are supposed to be muted */
			if (test_bit(y, &merged)) {
				memset(ts->span.chans[y]->readchunk, DAHDI_XLAW(0, ts->span.chans[y]), DAHDI_CHUNKSIZE);
			}
		}
	}
#endif	

#ifdef ENABLE_PREFETCH
	prefetch((void *)(ts->readchunk));
	prefetch((void *)(ts->writechunk));
	prefetch((void *)(ts->readchunk + 8));
	prefetch((void *)(ts->writechunk + 8));
	prefetch((void *)(ts->readchunk + 16));
	prefetch((void *)(ts->writechunk + 16));
	prefetch((void *)(ts->readchunk + 24));
	prefetch((void *)(ts->writechunk + 24));
	prefetch((void *)(ts->readchunk + 32));
	prefetch((void *)(ts->writechunk + 32));
	prefetch((void *)(ts->readchunk + 40));
	prefetch((void *)(ts->writechunk + 40));
	prefetch((void *)(ts->readchunk + 48));
	prefetch((void *)(ts->writechunk + 48));
	prefetch((void *)(ts->readchunk + 56));
	prefetch((void *)(ts->writechunk + 56));
#endif

	dahdi_ec_span(&ts->span);
	dahdi_receive(&ts->span);
}

static inline void __transmit_span(struct t4_span *ts)
{
	dahdi_transmit(&ts->span);
}

#ifdef ENABLE_WORKQUEUES
static void workq_handlespan(void *data)
{
	struct t4_span *ts = data;
	struct t4 *wc = ts->owner;
	
	__receive_span(ts);
	__transmit_span(ts);
	atomic_dec(&wc->worklist);
	if (!atomic_read(&wc->worklist))
		t4_pci_out(wc, WC_INTR, 0);
}
#else
static void t4_prep_gen2(struct t4 *wc)
{
	int x;
	for (x=0;x<wc->numspans;x++) {
		if (wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) {
			__receive_span(wc->tspans[x]);
			__transmit_span(wc->tspans[x]);
		}
	}
}

#endif
#ifdef SUPPORT_GEN1
static void t4_transmitprep(struct t4 *wc, int irq)
{
	volatile unsigned int *writechunk;
	int x,y,z;
	unsigned int tmp;
	int offset=0;
	if (!wc->t1e1)
		offset = 4;
	if (irq & 1) {
		/* First part */
		writechunk = wc->writechunk + 1;
	} else {
		writechunk = wc->writechunk + DAHDI_CHUNKSIZE * 32  + 1;
	}
	for (y=0;y<wc->numspans;y++) {
		if (wc->tspans[y]->span.flags & DAHDI_FLAG_RUNNING) 
			dahdi_transmit(&wc->tspans[y]->span);
	}

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Once per chunk */
		for (z=0;z<24;z++) {
			/* All T1/E1 channels */
			tmp = (wc->tspans[3]->span.chans[z]->writechunk[x]) | 
				  (wc->tspans[2]->span.chans[z]->writechunk[x] << 8) |
				  (wc->tspans[1]->span.chans[z]->writechunk[x] << 16) |
				  (wc->tspans[0]->span.chans[z]->writechunk[x] << 24);
			writechunk[z+offset] = tmp;
		}
		if (wc->t1e1) {
			for (z=24;z<31;z++) {
				/* Only E1 channels now */
				tmp = 0;
				if (wc->numspans == 4) {
					if (wc->tspans[3]->span.channels > 24)
						tmp |= wc->tspans[3]->span.chans[z]->writechunk[x];
					if (wc->tspans[2]->span.channels > 24)
						tmp |= (wc->tspans[2]->span.chans[z]->writechunk[x] << 8);
				}
				if (wc->tspans[1]->span.channels > 24)
					tmp |= (wc->tspans[1]->span.chans[z]->writechunk[x] << 16);
				if (wc->tspans[0]->span.channels > 24)
					tmp |= (wc->tspans[0]->span.chans[z]->writechunk[x] << 24);
				writechunk[z] = tmp;
			}
		}
		/* Advance pointer by 4 TDM frame lengths */
		writechunk += 32;
	}

}
#endif

static void t4_check_sigbits(struct t4 *wc, int span)
{
	int a,i,rxs;
	struct t4_span *ts = wc->tspans[span];

	if (debug & DEBUG_RBS)
		dev_notice(&wc->dev->dev, "Checking sigbits on span %d\n",
				span + 1);

	if (!(ts->span.flags & DAHDI_FLAG_RUNNING))
		return;
	if (ts->spantype == TYPE_E1) {
		for (i = 0; i < 15; i++) {
			a = t4_framer_in(wc, span, 0x71 + i);
			/* Get high channel in low bits */
			rxs = (a & 0xf);
			if (!(ts->span.chans[i+16]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i+16]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i+16], rxs);
			}
			rxs = (a >> 4) & 0xf;
			if (!(ts->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i], rxs);
			}
		}
	} else if (ts->span.lineconfig & DAHDI_CONFIG_D4) {
		for (i = 0; i < 24; i+=4) {
			a = t4_framer_in(wc, span, 0x70 + (i>>2));
			/* Get high channel in low bits */
			rxs = (a & 0x3) << 2;
			if (!(ts->span.chans[i+3]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i+3]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i+3], rxs);
			}
			rxs = (a & 0xc);
			if (!(ts->span.chans[i+2]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i+2]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i+2], rxs);
			}
			rxs = (a >> 2) & 0xc;
			if (!(ts->span.chans[i+1]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i+1]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i+1], rxs);
			}
			rxs = (a >> 4) & 0xc;
			if (!(ts->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				if (ts->span.chans[i]->rxsig != rxs)
					dahdi_rbsbits(ts->span.chans[i], rxs);
			}
		}
	} else {
		for (i = 0; i < 24; i+=2) {
			a = t4_framer_in(wc, span, 0x70 + (i>>1));
			/* Get high channel in low bits */
			rxs = (a & 0xf);
			if (!(ts->span.chans[i+1]->sig & DAHDI_SIG_CLEAR)) {
				/* XXX Not really reset on every trans! XXX */
				if (ts->span.chans[i+1]->rxsig != rxs) {
					dahdi_rbsbits(ts->span.chans[i+1], rxs);
				}
			}
			rxs = (a >> 4) & 0xf;
			if (!(ts->span.chans[i]->sig & DAHDI_SIG_CLEAR)) {
				/* XXX Not really reset on every trans! XXX */
				if (ts->span.chans[i]->rxsig != rxs) {
					dahdi_rbsbits(ts->span.chans[i], rxs);
				}
			}
		}
	}
}

static void t4_check_alarms(struct t4 *wc, int span)
{
	unsigned char c, d, e;
	int alarms;
	int x,j;
	struct t4_span *ts = wc->tspans[span];
	unsigned long flags;

	if (!(ts->span.flags & DAHDI_FLAG_RUNNING))
		return;

	spin_lock_irqsave(&wc->reglock, flags);

	c = __t4_framer_in(wc, span, 0x4c);
	d = __t4_framer_in(wc, span, 0x4d);

	/* Assume no alarms */
	alarms = 0;

	/* And consider only carrier alarms */
	ts->span.alarms &= (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE | DAHDI_ALARM_NOTOPEN);

	if (ts->spantype == TYPE_E1) {
		if (c & 0x04) {
			/* No multiframe found, force RAI high after 400ms only if
			   we haven't found a multiframe since last loss
			   of frame */
			if (!(ts->spanflags & FLAG_NMF)) {
				__t4_framer_out(wc, span, 0x20, 0x9f | 0x20);	/* LIM0: Force RAI High */
				ts->spanflags |= FLAG_NMF;
				dev_notice(&wc->dev->dev,
					"NMF workaround on!\n");
			}
			__t4_framer_out(wc, span, 0x1e, 0xc3);	/* Reset to CRC4 mode */
			__t4_framer_out(wc, span, 0x1c, 0xf2);	/* Force Resync */
			__t4_framer_out(wc, span, 0x1c, 0xf0);	/* Force Resync */
		} else if (!(c & 0x02)) {
			if ((ts->spanflags & FLAG_NMF)) {
				__t4_framer_out(wc, span, 0x20, 0x9f);	/* LIM0: Clear forced RAI */
				ts->spanflags &= ~FLAG_NMF;
				dev_notice(&wc->dev->dev,
					"NMF workaround off!\n");
			}
		}
	} else {
		/* Detect loopup code if we're not sending one */
		if ((!ts->span.mainttimer) && (d & 0x08)) {
			/* Loop-up code detected */
			if ((ts->loopupcnt++ > 80)  && (ts->span.maintstat != DAHDI_MAINT_REMOTELOOP)) {
				__t4_framer_out(wc, span, 0x36, 0x08);	/* LIM0: Disable any local loop */
				__t4_framer_out(wc, span, 0x37, 0xf6 );	/* LIM1: Enable remote loop */
				ts->span.maintstat = DAHDI_MAINT_REMOTELOOP;
			}
		} else
			ts->loopupcnt = 0;
		/* Same for loopdown code */
		if ((!ts->span.mainttimer) && (d & 0x10)) {
			/* Loop-down code detected */
			if ((ts->loopdowncnt++ > 80)  && (ts->span.maintstat == DAHDI_MAINT_REMOTELOOP)) {
				__t4_framer_out(wc, span, 0x36, 0x08);	/* LIM0: Disable any local loop */
				__t4_framer_out(wc, span, 0x37, 0xf0 );	/* LIM1: Disable remote loop */
				ts->span.maintstat = DAHDI_MAINT_NONE;
			}
		} else
			ts->loopdowncnt = 0;
	}

	if (ts->span.lineconfig & DAHDI_CONFIG_NOTOPEN) {
		for (x=0,j=0;x < ts->span.channels;x++)
			if ((ts->span.chans[x]->flags & DAHDI_FLAG_OPEN) ||
			    (ts->span.chans[x]->flags & DAHDI_FLAG_NETDEV))
				j++;
		if (!j)
			alarms |= DAHDI_ALARM_NOTOPEN;
	}

	/* Loss of Frame Alignment */
	if (c & 0x20) {
		if (ts->alarmcount >= alarmdebounce) {

			/* Disable Slip Interrupts */
			e = __t4_framer_in(wc, span, 0x17);
			__t4_framer_out(wc, span, 0x17, (e|0x03));

			alarms |= DAHDI_ALARM_RED;
		} else {
			if (unlikely(debug && !ts->alarmcount)) {
				/* starting to debounce LOF/LFA */
				dev_info(&wc->dev->dev, "wct%dxxp: LOF/LFA "
					"detected on span %d but debouncing "
					"for %d ms\n", wc->numspans, span + 1,
					alarmdebounce);
			}
			ts->alarmcount++;
		}
	} else
		ts->alarmcount = 0;

	/* Loss of Signal */
	if (c & 0x80) {
		if (ts->losalarmcount >= losalarmdebounce) {
			/* Disable Slip Interrupts */
			e = __t4_framer_in(wc, span, 0x17);
			__t4_framer_out(wc, span, 0x17, (e|0x03));

			alarms |= DAHDI_ALARM_RED;
		} else {
			if (unlikely(debug && !ts->losalarmcount)) {
				/* starting to debounce LOS */
				dev_info(&wc->dev->dev, "wct%dxxp: LOS "
					"detected on span %d but debouncing "
					"for %d ms\n", wc->numspans,
					span + 1, losalarmdebounce);
			}
			ts->losalarmcount++;
		}
	} else
		ts->losalarmcount = 0;

	/* Alarm Indication Signal */
	if (c & 0x40) {
		if (ts->aisalarmcount >= aisalarmdebounce)
			alarms |= DAHDI_ALARM_BLUE;
		else {
			if (unlikely(debug && !ts->aisalarmcount)) {
				/* starting to debounce AIS */
				dev_info(&wc->dev->dev, "wct%dxxp: AIS "
					"detected on span %d but debouncing "
					"for %d ms\n", wc->numspans,
					span + 1, aisalarmdebounce);
			}
			ts->aisalarmcount++;
		}
	} else
		ts->aisalarmcount = 0;

	/* Add detailed alarm status information to a red alarm state */
	if (alarms & DAHDI_ALARM_RED) {
		if (c & FRS0_LOS)
			alarms |= DAHDI_ALARM_LOS;
		if (c & FRS0_LFA)
			alarms |= DAHDI_ALARM_LFA;
		if (c & FRS0_LMFA)
			alarms |= DAHDI_ALARM_LMFA;
	}

	if (unlikely(debug)) {
		/* Check to ensure the xmit line isn't shorted */
		if (unlikely(d & FRS1_XLS)) {
			dev_info(&wc->dev->dev,
				"Detected a possible hardware malfunction"\
				" this card may need servicing\n");
		}
	}

	if (((!ts->span.alarms) && alarms) || 
	    (ts->span.alarms && (!alarms))) 
		set_bit(T4_CHECK_TIMING, &wc->checkflag);

	/* Keep track of recovering */
	if ((!alarms) && ts->span.alarms) 
		ts->alarmtimer = DAHDI_ALARMSETTLE_TIME;
	if (ts->alarmtimer)
		alarms |= DAHDI_ALARM_RECOVER;

	/* If receiving alarms, go into Yellow alarm state */
	if (alarms && !(ts->spanflags & FLAG_SENDINGYELLOW)) {
		/* We manually do yellow alarm to handle RECOVER and NOTOPEN, otherwise it's auto anyway */
		unsigned char fmr4;
		fmr4 = __t4_framer_in(wc, span, 0x20);
		__t4_framer_out(wc, span, 0x20, fmr4 | 0x20);
		dev_info(&wc->dev->dev, "Setting yellow alarm span %d\n",
								span+1);
		ts->spanflags |= FLAG_SENDINGYELLOW;
	} else if ((!alarms) && (ts->spanflags & FLAG_SENDINGYELLOW)) {
		unsigned char fmr4;
		/* We manually do yellow alarm to handle RECOVER  */
		fmr4 = __t4_framer_in(wc, span, 0x20);
		__t4_framer_out(wc, span, 0x20, fmr4 & ~0x20);
		dev_info(&wc->dev->dev, "Clearing yellow alarm span %d\n",
								span+1);

		/* Re-enable timing slip interrupts */
		e = __t4_framer_in(wc, span, 0x17);

		__t4_framer_out(wc, span, 0x17, (e & ~(0x03)));

		ts->spanflags &= ~FLAG_SENDINGYELLOW;
	}

	/* Re-check the timing source when we enter/leave alarm, not withstanding
	   yellow alarm */
	if (c & 0x10) { /* receiving yellow (RAI) */
		if (ts->yelalarmcount >= yelalarmdebounce)
			alarms |= DAHDI_ALARM_YELLOW;
		else {
			if (unlikely(debug && !ts->yelalarmcount)) {
				/* starting to debounce AIS */
				dev_info(&wc->dev->dev, "wct%dxxp: yellow "
					"(RAI) detected on span %d but "
					"debouncing for %d ms\n",
					wc->numspans, span + 1,
					yelalarmdebounce);
			}
			ts->yelalarmcount++;
		}
	} else
		ts->yelalarmcount = 0;

	if (ts->span.mainttimer || ts->span.maintstat) 
		alarms |= DAHDI_ALARM_LOOPBACK;
	ts->span.alarms = alarms;
	spin_unlock_irqrestore(&wc->reglock, flags);
	dahdi_alarm_notify(&ts->span);
}

static void t4_do_counters(struct t4 *wc)
{
	int span;
	for (span=0;span<wc->numspans;span++) {
		struct t4_span *ts = wc->tspans[span];
		int docheck=0;

		spin_lock(&wc->reglock);
		if (ts->loopupcnt || ts->loopdowncnt || ts->alarmcount
			|| ts->losalarmcount || ts->aisalarmcount
			|| ts->yelalarmcount)
			docheck++;

		if (ts->alarmtimer) {
			if (!--ts->alarmtimer) {
				docheck++;
				ts->span.alarms &= ~(DAHDI_ALARM_RECOVER);
			}
		}
		spin_unlock(&wc->reglock);
		if (docheck) {
			t4_check_alarms(wc, span);
			dahdi_alarm_notify(&ts->span);
		}
	}
}

static inline void __handle_leds(struct t4 *wc)
{
	int x;

	wc->blinktimer++;
	for (x=0;x<wc->numspans;x++) {
		struct t4_span *ts = wc->tspans[x];
		if (ts->span.flags & DAHDI_FLAG_RUNNING) {
			if ((ts->span.alarms & (DAHDI_ALARM_RED | DAHDI_ALARM_BLUE)) || ts->losalarmcount) {
#ifdef FANCY_ALARM
				if (wc->blinktimer == (altab[wc->alarmpos] >> 1)) {
					__t4_set_led(wc, x, WC_RED);
				}
				if (wc->blinktimer == 0xf) {
					__t4_set_led(wc, x, WC_OFF);
				}
#else
				if (wc->blinktimer == 160) {
					__t4_set_led(wc, x, WC_RED);
				} else if (wc->blinktimer == 480) {
					__t4_set_led(wc, x, WC_OFF);
				}
#endif
			} else if (ts->span.alarms & DAHDI_ALARM_YELLOW) {
				/* Yellow Alarm */
				__t4_set_led(wc, x, WC_YELLOW);
			} else if (ts->span.mainttimer || ts->span.maintstat) {
#ifdef FANCY_ALARM
				if (wc->blinktimer == (altab[wc->alarmpos] >> 1)) {
					__t4_set_led(wc, x, WC_GREEN);
				}
				if (wc->blinktimer == 0xf) {
					__t4_set_led(wc, x, WC_OFF);
				}
#else
				if (wc->blinktimer == 160) {
					__t4_set_led(wc, x, WC_GREEN);
				} else if (wc->blinktimer == 480) {
					__t4_set_led(wc, x, WC_OFF);
				}
#endif
			} else {
				/* No Alarm */
				__t4_set_led(wc, x, WC_GREEN);
			}
		}	else
				__t4_set_led(wc, x, WC_OFF);

	}
#ifdef FANCY_ALARM
	if (wc->blinktimer == 0xf) {
		wc->blinktimer = -1;
		wc->alarmpos++;
		if (wc->alarmpos >= (sizeof(altab) / sizeof(altab[0])))
			wc->alarmpos = 0;
	}
#else
	if (wc->blinktimer == 480)
		wc->blinktimer = 0;
#endif
}

static inline void t4_framer_interrupt(struct t4 *wc, int span)
{
	/* Check interrupts for a given span */
	unsigned char gis, isr0, isr1, isr2, isr3, isr4, reg;
	int readsize = -1;
	struct t4_span *ts = wc->tspans[span];
	struct dahdi_chan *sigchan;
	unsigned long flags;


	/* 1st gen cards isn't used interrupts */
	gis = t4_framer_in(wc, span, FRMR_GIS);
	isr0 = (gis & FRMR_GIS_ISR0) ? t4_framer_in(wc, span, FRMR_ISR0) : 0;
	isr1 = (gis & FRMR_GIS_ISR1) ? t4_framer_in(wc, span, FRMR_ISR1) : 0;
	isr2 = (gis & FRMR_GIS_ISR2) ? t4_framer_in(wc, span, FRMR_ISR2) : 0;
	isr3 = (gis & FRMR_GIS_ISR3) ? t4_framer_in(wc, span, FRMR_ISR3) : 0;
	isr4 = (gis & FRMR_GIS_ISR4) ? t4_framer_in(wc, span, FRMR_ISR4) : 0;

 	if ((debug & DEBUG_FRAMER) && !(isr3 & ISR3_SEC)) {
 		dev_info(&wc->dev->dev, "gis: %02x, isr0: %02x, isr1: %02x, "\
 			"isr2: %02x, isr3: %08x, isr4: %02x, intcount=%u\n",
 			gis, isr0, isr1, isr2, isr3, isr4, wc->intcount);
 	}
 
	/* Collect performance counters once per second */
 	if (isr3 & ISR3_SEC) {
 		ts->span.count.fe += t4_framer_in(wc, span, FECL_T);
 		ts->span.count.crc4 += t4_framer_in(wc, span, CEC1L_T);
 		ts->span.count.cv += t4_framer_in(wc, span, CVCL_T);
 		ts->span.count.ebit += t4_framer_in(wc, span, EBCL_T);
 		ts->span.count.be += t4_framer_in(wc, span, BECL_T);
 		ts->span.count.prbs = t4_framer_in(wc, span, FRS1_T);
 	}
 
	/* Collect errored second counter once per second */
 	if (isr3 & ISR3_ES) {
 		ts->span.count.errsec += 1;
 	}
 
 	if (isr3 & 0x08) {
 		reg = t4_framer_in(wc, span, FRS1_T);
		dev_info(&wc->dev->dev, "FRS1: %d\n", reg);
 		if (reg & LLBDD) {
 			dev_info(&wc->dev->dev, "Line loop-back activation "\
 					"signal detected with status: %01d "\
 					"for span %d\n", reg & LLBAD, span+1);
 		}
 	}

	if (isr0)
		t4_check_sigbits(wc, span);

	if (ts->spantype == TYPE_E1) {
		/* E1 checks */
		if ((isr3 & 0x38) || isr2 || isr1)
			t4_check_alarms(wc, span);
	} else {
		/* T1 checks */
		if (isr2 || (isr3 & 0x08))
			t4_check_alarms(wc, span);
	}
	if (!ts->span.alarms) {
		if ((isr3 & 0x3) || (isr4 & 0xc0))
			ts->span.timingslips++;

		if (debug & DEBUG_MAIN) {
			if (isr3 & 0x02)
				dev_notice(&wc->dev->dev, "TE%d10P: RECEIVE "
					"slip NEGATIVE on span %d\n",
					wc->numspans, span + 1);
			if (isr3 & 0x01)
				dev_notice(&wc->dev->dev, "TE%d10P: RECEIVE "
					"slip POSITIVE on span %d\n",
					wc->numspans, span + 1);
			if (isr4 & 0x80)
				dev_notice(&wc->dev->dev, "TE%dXXP: TRANSMIT "
					"slip POSITIVE on span %d\n",
					wc->numspans, span + 1);
			if (isr4 & 0x40)
				dev_notice(&wc->dev->dev, "TE%d10P: TRANSMIT "
					"slip NEGATIVE on span %d\n",
					wc->numspans, span + 1);
		}
	} else
		ts->span.timingslips = 0;

	spin_lock_irqsave(&wc->reglock, flags);
	/* HDLC controller checks - receive side */
	if (!ts->sigchan) {
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}

	sigchan = ts->sigchan;
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (isr0 & FRMR_ISR0_RME) {
		readsize = (t4_framer_in(wc, span, FRMR_RBCH) << 8) | t4_framer_in(wc, span, FRMR_RBCL);
		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "Received data length is %d "
				"(%d)\n", readsize,
				readsize & FRMR_RBCL_MAX_SIZE);
		/* RPF isn't set on last part of frame */
		if ((readsize > 0) && ((readsize &= FRMR_RBCL_MAX_SIZE) == 0))
			readsize = FRMR_RBCL_MAX_SIZE + 1;
	} else if (isr0 & FRMR_ISR0_RPF)
		readsize = FRMR_RBCL_MAX_SIZE + 1;

	if (readsize > 0) {
		int i;
		unsigned char readbuf[FRMR_RBCL_MAX_SIZE + 1];

		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "Framer %d: Got RPF/RME! "
				"readsize is %d\n", sigchan->span->offset,
				readsize);

		for (i = 0; i < readsize; i++)
			readbuf[i] = t4_framer_in(wc, span, FRMR_RXFIFO);

		/* Tell the framer to clear the RFIFO */
		t4_framer_cmd_wait(wc, span, FRMR_CMDR_RMC);

		if (debug & DEBUG_FRAMER) {
			dev_notice(&wc->dev->dev, "RX(");
			for (i = 0; i < readsize; i++)
				dev_notice(&wc->dev->dev, "%s%02x",
					(i ? " " : ""), readbuf[i]);
			dev_notice(&wc->dev->dev, ")\n");
		}

		if (isr0 & FRMR_ISR0_RME) {
			/* Do checks for HDLC problems */
			unsigned char rsis = readbuf[readsize-1];
#if 0
			unsigned int olddebug = debug;
#endif
			unsigned char rsis_reg = t4_framer_in(wc, span, FRMR_RSIS);

#if 0
			if ((rsis != 0xA2) || (rsis != rsis_reg))
				debug |= DEBUG_FRAMER;
#endif

			++ts->frames_in;
			if ((debug & DEBUG_FRAMER) && !(ts->frames_in & 0x0f))
				dev_notice(&wc->dev->dev, "Received %d frames "
					"on span %d\n", ts->frames_in, span);
			if (debug & DEBUG_FRAMER)
				dev_notice(&wc->dev->dev, "Received HDLC frame"
					" %d.  RSIS = 0x%x (%x)\n",
					ts->frames_in, rsis, rsis_reg);
			if (!(rsis & FRMR_RSIS_CRC16)) {
				if (debug & DEBUG_FRAMER)
					dev_notice(&wc->dev->dev, "CRC check "
							"failed %d\n", span);
				dahdi_hdlc_abort(sigchan, DAHDI_EVENT_BADFCS);
			} else if (rsis & FRMR_RSIS_RAB) {
				if (debug & DEBUG_FRAMER)
					dev_notice(&wc->dev->dev, "ABORT of "
						"current frame due to "
						"overflow %d\n", span);
				dahdi_hdlc_abort(sigchan, DAHDI_EVENT_ABORT);
			} else if (rsis & FRMR_RSIS_RDO) {
				if (debug & DEBUG_FRAMER)
					dev_notice(&wc->dev->dev, "HDLC "
						"overflow occured %d\n",
						span);
				dahdi_hdlc_abort(sigchan, DAHDI_EVENT_OVERRUN);
			} else if (!(rsis & FRMR_RSIS_VFR)) {
				if (debug & DEBUG_FRAMER)
					dev_notice(&wc->dev->dev, "Valid Frame"
						" check failed on span %d\n",
						span);
				dahdi_hdlc_abort(sigchan, DAHDI_EVENT_ABORT);
			} else {
				dahdi_hdlc_putbuf(sigchan, readbuf, readsize - 1);
				dahdi_hdlc_finish(sigchan);
				if (debug & DEBUG_FRAMER)
					dev_notice(&wc->dev->dev, "Received "
						"valid HDLC frame on span %d"
						"\n", span);
			}
#if 0
			debug = olddebug;
#endif
		} else if (isr0 & FRMR_ISR0_RPF)
			dahdi_hdlc_putbuf(sigchan, readbuf, readsize);
	}

	/* Transmit side */
	if (isr1 & FRMR_ISR1_XDU) {
		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "XDU: Resetting signal "
					"controller!\n");
		t4_framer_cmd_wait(wc, span, FRMR_CMDR_SRES);
	} else if (isr1 & FRMR_ISR1_XPR) {
		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "Sigchan %d is %p\n",
					sigchan->chanpos, sigchan);

		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "Framer %d: Got XPR!\n",
					sigchan->span->offset);
		t4_hdlc_xmit_fifo(wc, span, ts);
	}

	if (isr1 & FRMR_ISR1_ALLS) {
		if (debug & DEBUG_FRAMER)
			dev_notice(&wc->dev->dev, "ALLS received\n");
	}
}

#ifdef SUPPORT_GEN1
DAHDI_IRQ_HANDLER(t4_interrupt)
{
	struct t4 *wc = dev_id;
	unsigned long flags;
	int x;
	
	unsigned int status;
	unsigned int status2;

#if 0
	if (wc->intcount < 20)
		dev_notice(&wc->dev->dev, "Pre-interrupt\n");
#endif
	
	/* Make sure it's really for us */
	status = __t4_pci_in(wc, WC_INTR);

	/* Process framer interrupts */
	status2 = t4_framer_in(wc, 0, FRMR_CIS);
	if (status2 & 0x0f) {
		for (x = 0; x < wc->numspans; ++x) {
			if (status2 & (1 << x))
				t4_framer_interrupt(wc, x);
		}
	}

	/* Ignore if it's not for us */
	if (!status)
		return IRQ_NONE;

	__t4_pci_out(wc, WC_INTR, 0);

	if (!wc->spansstarted) {
		dev_notice(&wc->dev->dev, "Not prepped yet!\n");
		return IRQ_NONE;
	}

	wc->intcount++;
#if 0
	if (wc->intcount < 20)
		dev_notice(&wc->dev->dev, "Got interrupt, status = %08x\n",
				status);
#endif		

	if (status & 0x3) {
		t4_receiveprep(wc, status);
		t4_transmitprep(wc, status);
	}
	
#if 0
	if ((wc->intcount < 10) || !(wc->intcount % 1000)) {
		status2 = t4_framer_in(wc, 0, FRMR_CIS);
		dev_notice(&wc->dev->dev, "Status2: %04x\n", status2);
		for (x = 0;x<wc->numspans;x++) {
			status2 = t4_framer_in(wc, x, FRMR_FRS0);
			dev_notice(&wc->dev->dev, "FRS0/%d: %04x\n", x,
					status2);
		}
	}
#endif
	t4_do_counters(wc);

	x = wc->intcount & 15 /* 63 */;
	switch(x) {
	case 0:
	case 1:
	case 2:
	case 3:
		t4_check_sigbits(wc, x);
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		t4_check_alarms(wc, x - 4);
		break;
	}

	spin_lock_irqsave(&wc->reglock, flags);

	__handle_leds(wc);

	if (test_bit(T4_CHECK_TIMING, &wc->checkflag))
		__t4_set_timing_source_auto(wc);

	spin_unlock_irqrestore(&wc->reglock, flags);

	return IRQ_RETVAL(1);
}
#endif

static int t4_allocate_buffers(struct t4 *wc, int numbufs, volatile unsigned int **oldalloc, dma_addr_t *oldwritedma)
{
	volatile unsigned int *alloc;
	dma_addr_t writedma;

	alloc =
		/* 32 channels, Double-buffer, Read/Write, 4 spans */
		(unsigned int *)pci_alloc_consistent(wc->dev, numbufs * T4_BASE_SIZE * 2, &writedma);

	if (!alloc) {
		dev_notice(&wc->dev->dev, "wct%dxxp: Unable to allocate "
				"DMA-able memory\n", wc->numspans);
		return -ENOMEM;
	}

	if (oldwritedma)
		*oldwritedma = wc->writedma;
	if (oldalloc)
		*oldalloc = wc->writechunk;

	wc->writechunk = alloc;
	wc->writedma = writedma;

	/* Read is after the whole write piece (in words) */
	wc->readchunk = wc->writechunk + (T4_BASE_SIZE * numbufs) / 4;
	
	/* Same thing but in bytes...  */
	wc->readdma = wc->writedma + (T4_BASE_SIZE * numbufs);

	wc->numbufs = numbufs;
	
	/* Initialize Write/Buffers to all blank data */
	memset((void *)wc->writechunk,0x00, T4_BASE_SIZE * numbufs);
	memset((void *)wc->readchunk,0xff, T4_BASE_SIZE * numbufs);

	dev_notice(&wc->dev->dev, "DMA memory base of size %d at %p.  Read: "
		"%p and Write %p\n", numbufs * T4_BASE_SIZE * 2,
		wc->writechunk, wc->readchunk, wc->writechunk);

	return 0;
}

static void t4_increase_latency(struct t4 *wc, int newlatency)
{
	unsigned long flags;
	volatile unsigned int *oldalloc;
	dma_addr_t oldaddr;
	int oldbufs;

	spin_lock_irqsave(&wc->reglock, flags);

	__t4_pci_out(wc, WC_DMACTRL, 0x00000000);
	/* Acknowledge any pending interrupts */
	__t4_pci_out(wc, WC_INTR, 0x00000000);

	__t4_pci_in(wc, WC_VERSION);

	oldbufs = wc->numbufs;

	if (t4_allocate_buffers(wc, newlatency, &oldalloc, &oldaddr)) {
		dev_info(&wc->dev->dev, "Error allocating latency buffers for "
				"latency of %d\n", newlatency);
		__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}

	__t4_pci_out(wc, WC_RDADDR, wc->readdma);
	__t4_pci_out(wc, WC_WRADDR, wc->writedma);

	__t4_pci_in(wc, WC_VERSION);

	__t4_pci_out(wc, 5, (ms_per_irq << 16) | newlatency);
	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);

	__t4_pci_in(wc, WC_VERSION);

	wc->rxident = 0;
	wc->lastindex = 0;

	spin_unlock_irqrestore(&wc->reglock, flags);

	pci_free_consistent(wc->dev, T4_BASE_SIZE * oldbufs * 2, (void *)oldalloc, oldaddr);

	dev_info(&wc->dev->dev, "Increased latency to %d\n", newlatency);

}

static void t4_isr_bh(unsigned long data)
{
	struct t4 *wc = (struct t4 *)data;

	if (test_bit(T4_CHANGE_LATENCY, &wc->checkflag)) {
		if (wc->needed_latency != wc->numbufs) {
			t4_increase_latency(wc, wc->needed_latency);
			clear_bit(T4_CHANGE_LATENCY, &wc->checkflag);
		}
	}
#ifdef VPM_SUPPORT
	if (wc->vpm) {
		if (test_and_clear_bit(T4_CHECK_VPM, &wc->checkflag)) {
			if (wc->vpm450m) {
				/* How stupid is it that the octasic can't generate an
				   interrupt when there's a tone, in spite of what their
				   documentation says? */
				t4_check_vpm450(wc);
			} else
				t4_check_vpm400(wc, wc->vpm400checkstatus);
		}
	}
#endif
}

DAHDI_IRQ_HANDLER(t4_interrupt_gen2)
{
	struct t4 *wc = dev_id;
	unsigned int status;
	unsigned char rxident, expected;
	
#if 0
	if (unlikely(test_bit(T4_CHANGE_LATENCY, &wc->checkflag))) {
		goto out;
	}
#endif

	/* Check this first in case we get a spurious interrupt */
	if (unlikely(test_bit(T4_STOP_DMA, &wc->checkflag))) {
		/* Stop DMA cleanly if requested */
		wc->dmactrl = 0x0;
		t4_pci_out(wc, WC_DMACTRL, 0x00000000);
		/* Acknowledge any pending interrupts */
		t4_pci_out(wc, WC_INTR, 0x00000000);
		spin_lock(&wc->reglock);
		__t4_set_sclk_src(wc, WC_SELF, 0, 0);
		spin_unlock(&wc->reglock);
		return IRQ_RETVAL(1);
	}

	/* Make sure it's really for us */
	status = __t4_pci_in(wc, WC_INTR);

	/* Ignore if it's not for us */
	if (!(status & 0x7)) {
		return IRQ_NONE;
	}

#ifdef ENABLE_WORKQUEUES
	__t4_pci_out(wc, WC_INTR, status & 0x00000008);
#endif

	if (unlikely(!wc->spansstarted)) {
		dev_info(&wc->dev->dev, "Not prepped yet!\n");
		return IRQ_NONE;
	}

	wc->intcount++;
	if ((wc->flags & FLAG_5THGEN) && (status & 0x2)) {
		rxident = (status >> 16) & 0x7f;
		expected = (wc->rxident + ms_per_irq) % 128;
	
		if ((rxident != expected) && !test_bit(T4_IGNORE_LATENCY, &wc->checkflag)) {
			int needed_latency;
			int smallest_max;

			if (debug & DEBUG_MAIN)
				dev_warn(&wc->dev->dev, "Missed interrupt.  "
					"Expected ident of %d and got ident "
					"of %d\n", expected, rxident);

			if (test_bit(T4_IGNORE_LATENCY, &wc->checkflag)) {
				dev_info(&wc->dev->dev,
					"Should have ignored latency\n");
			}
			if (rxident > wc->rxident) {
				needed_latency = rxident - wc->rxident;
			} else {
				needed_latency = (128 - wc->rxident) + rxident;
			}

			needed_latency += 1;

			smallest_max = (max_latency >= GEN5_MAX_LATENCY) ? GEN5_MAX_LATENCY : max_latency;

			if (needed_latency > smallest_max) {
				dev_info(&wc->dev->dev, "Truncating latency "
					"request to %d instead of %d\n",
					smallest_max, needed_latency);
				needed_latency = smallest_max;
			}

			if (needed_latency > wc->numbufs) {
				int x;

				dev_info(&wc->dev->dev, "Need to increase "
					"latency.  Estimated latency should "
					"be %d\n", needed_latency);
				for (x = 0; x < wc->numspans; x++)
					wc->tspans[x]->span.irqmisses++;
				wc->needed_latency = needed_latency;
				__t4_pci_out(wc, WC_DMACTRL, 0x00000000);
				set_bit(T4_CHANGE_LATENCY, &wc->checkflag);
				goto out;
			}
		}
	
		wc->rxident = rxident;
	}

	if (unlikely((wc->intcount < 20)))

		dev_info(&wc->dev->dev, "2G: Got interrupt, status = %08x, "
			"CIS = %04x\n", status, t4_framer_in(wc, 0, FRMR_CIS));

	if (likely(status & 0x2)) {
#ifdef ENABLE_WORKQUEUES
		int cpus = num_online_cpus();
		atomic_set(&wc->worklist, wc->numspans);
		if (wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)
			t4_queue_work(wc->workq, &wc->tspans[0]->swork, 0);
		else
			atomic_dec(&wc->worklist);
		if (wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)
			t4_queue_work(wc->workq, &wc->tspans[1]->swork, 1 % cpus);
		else
			atomic_dec(&wc->worklist);
		if (wc->numspans == 4) {
			if (wc->tspans[2]->span.flags & DAHDI_FLAG_RUNNING)
				t4_queue_work(wc->workq, &wc->tspans[2]->swork, 2 % cpus);
			else
				atomic_dec(&wc->worklist);
			if (wc->tspans[3]->span.flags & DAHDI_FLAG_RUNNING)
				t4_queue_work(wc->workq, &wc->tspans[3]->swork, 3 % cpus);
			else
				atomic_dec(&wc->worklist);
		}
#else
#if 1
		unsigned int reg5 = __t4_pci_in(wc, 5);
		if (wc->intcount < 20) {

			dev_info(&wc->dev->dev, "Reg 5 is %08x\n", reg5);
		}
#endif

		if (wc->flags & FLAG_5THGEN) {
			unsigned int current_index = (reg5 >> 8) & 0x7f;
#if 0
			int catchup = 0;
#endif

			while (((wc->lastindex + 1) % wc->numbufs) != current_index) {
#if 0
				catchup++;
#endif
				wc->lastindex = (wc->lastindex + 1) % wc->numbufs;
				setup_chunks(wc, wc->lastindex);
				t4_prep_gen2(wc);
			}
#if 0
			if (catchup > 1) {
				dev_info(&wc->dev->dev, "Caught up %d "
						"chunks\n", catchup);
			}
#endif
		} else {
			t4_prep_gen2(wc);
		}

#endif
		t4_do_counters(wc);
		spin_lock(&wc->reglock);
		__handle_leds(wc);
		spin_unlock(&wc->reglock);

	}

	if (unlikely(status & 0x1)) {
		unsigned char cis;

		cis = t4_framer_in(wc, 0, FRMR_CIS);
		if (cis & FRMR_CIS_GIS1)
			t4_framer_interrupt(wc, 0);
		if (cis & FRMR_CIS_GIS2)
			t4_framer_interrupt(wc, 1);
		if (cis & FRMR_CIS_GIS3)
			t4_framer_interrupt(wc, 2);
		if (cis & FRMR_CIS_GIS4)
			t4_framer_interrupt(wc, 3);
	}

	if (wc->vpm && vpmdtmfsupport) {
		if (wc->vpm450m) {
			/* How stupid is it that the octasic can't generate an
			   interrupt when there's a tone, in spite of what their
			   documentation says? */
			if (!(wc->intcount & 0xf)) {
				set_bit(T4_CHECK_VPM, &wc->checkflag);
			}
		} else if ((status & 0xff00) != 0xff00) {
			wc->vpm400checkstatus = (status & 0xff00) >> 8;
			set_bit(T4_CHECK_VPM, &wc->checkflag);
		}
	}

	spin_lock(&wc->reglock);

	if (unlikely(test_bit(T4_CHECK_TIMING, &wc->checkflag))) {
		__t4_set_timing_source_auto(wc);
	}

	spin_unlock(&wc->reglock);

out:
	if (unlikely(test_bit(T4_CHANGE_LATENCY, &wc->checkflag) || test_bit(T4_CHECK_VPM, &wc->checkflag)))
		tasklet_schedule(&wc->t4_tlet);

#ifndef ENABLE_WORKQUEUES
	__t4_pci_out(wc, WC_INTR, 0);
#endif	

	return IRQ_RETVAL(1);
}

#ifdef SUPPORT_GEN1
static int t4_reset_dma(struct t4 *wc)
{
	/* Turn off DMA and such */
	wc->dmactrl = 0x0;
	t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	t4_pci_out(wc, WC_COUNT, 0);
	t4_pci_out(wc, WC_RDADDR, 0);
	t4_pci_out(wc, WC_WRADDR, 0);
	t4_pci_out(wc, WC_INTR, 0);
	/* Turn it all back on */
	t4_pci_out(wc, WC_RDADDR, wc->readdma);
	t4_pci_out(wc, WC_WRADDR, wc->writedma);
	t4_pci_out(wc, WC_COUNT, ((DAHDI_MAX_CHUNKSIZE * 2 * 32 - 1) << 18) | ((DAHDI_MAX_CHUNKSIZE * 2 * 32 - 1) << 2));
	t4_pci_out(wc, WC_INTR, 0);
#ifdef VPM_SUPPORT
	wc->dmactrl = 0xc0000000 | (1 << 29) | wc->vpm;
#else	
	wc->dmactrl = 0xc0000000 | (1 << 29);
#endif
	if (noburst)
		wc->dmactrl |= (1 << 26);
	t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	return 0;
}
#endif

#ifdef VPM_SUPPORT
static void t4_vpm_set_dtmf_threshold(struct t4 *wc, unsigned int threshold)
{
	unsigned int x;

	for (x = 0; x < 8; x++) {
		t4_vpm_out(wc, x, 0xC4, (threshold >> 8) & 0xFF);
		t4_vpm_out(wc, x, 0xC5, (threshold & 0xFF));
	}
	dev_info(&wc->dev->dev, "VPM: DTMF threshold set to %d\n", threshold);
}

static unsigned int t4_vpm_mask(int chip)
{
	unsigned int mask=0;
	switch(vpmspans) {
	case 4:
		mask = 0x55555555 << (chip >> 2);
		break;
	case 2:
		mask = 0x11111111 << (chip >> 1);
		break;
	case 1:
		mask = 0x01010101 << chip;
		break;
	}
	return mask;
}

static int t4_vpm_spanno(int chip)
{
	int spanno = 0;
	switch(vpmspans) {
	case 4:
		spanno = chip & 0x3;
		break;
	case 2:
		spanno = chip & 0x1;
		break;
	/* Case 1 is implicit */
	}
	return spanno;
}

static int t4_vpm_echotail(void)
{
	int echotail = 0x01ff;
	switch(vpmspans) {
	case 4:
		echotail = 0x007f;
		break;
	case 2:
		echotail = 0x00ff;
		break;
	/* Case 1 is implicit */
	}
	return echotail;
}

static void t4_vpm450_init(struct t4 *wc)
{
	unsigned int check1, check2;
	int laws[4] = { 0, };
	int x;
	unsigned int vpm_capacity;
	struct firmware embedded_firmware;
	const struct firmware *firmware = &embedded_firmware;
#if !defined(HOTPLUG_FIRMWARE)
	extern void _binary_dahdi_fw_oct6114_064_bin_size;
	extern void _binary_dahdi_fw_oct6114_128_bin_size;
	extern u8 _binary_dahdi_fw_oct6114_064_bin_start[];
	extern u8 _binary_dahdi_fw_oct6114_128_bin_start[];
#else
	static const char oct064_firmware[] = "dahdi-fw-oct6114-064.bin";
	static const char oct128_firmware[] = "dahdi-fw-oct6114-128.bin";
#endif

	if (!vpmsupport) {
		dev_info(&wc->dev->dev, "VPM450: Support Disabled\n");
		return;
	}

	/* Turn on GPIO/DATA mux if supported */
	t4_gpio_setdir(wc, (1 << 24), (1 << 24));
	__t4_raw_oct_out(wc, 0x000a, 0x5678);
	__t4_raw_oct_out(wc, 0x0004, 0x1234);
	check1 = __t4_raw_oct_in(wc, 0x0004);
	check2 = __t4_raw_oct_in(wc, 0x000a);
	if (debug)
		dev_notice(&wc->dev->dev, "OCT Result: %04x/%04x\n",
			__t4_raw_oct_in(wc, 0x0004),
			__t4_raw_oct_in(wc, 0x000a));
	if (__t4_raw_oct_in(wc, 0x0004) != 0x1234) {
		dev_notice(&wc->dev->dev, "VPM450: Not Present\n");
		return;
	}

	/* Setup alaw vs ulaw rules */
	for (x = 0;x < wc->numspans; x++) {
		if (wc->tspans[x]->span.channels > 24)
			laws[x] = 1;
	}

	switch ((vpm_capacity = get_vpm450m_capacity(wc))) {
	case 64:
#if defined(HOTPLUG_FIRMWARE)
		if ((request_firmware(&firmware, oct064_firmware, &wc->dev->dev) != 0) ||
		    !firmware) {
			dev_notice(&wc->dev->dev, "VPM450: firmware %s not "
				"available from userspace\n", oct064_firmware);
			return;
		}
#else
		embedded_firmware.data = _binary_dahdi_fw_oct6114_064_bin_start;
		/* Yes... this is weird. objcopy gives us a symbol containing
		   the size of the firmware, not a pointer a variable containing
		   the size. The only way we can get the value of the symbol
		   is to take its address, so we define it as a pointer and
		   then cast that value to the proper type.
	      */
		embedded_firmware.size = (size_t) &_binary_dahdi_fw_oct6114_064_bin_size;
#endif
		break;
	case 128:
#if defined(HOTPLUG_FIRMWARE)
		if ((request_firmware(&firmware, oct128_firmware, &wc->dev->dev) != 0) ||
		    !firmware) {
			dev_notice(&wc->dev->dev, "VPM450: firmware %s not "
				"available from userspace\n", oct128_firmware);
			return;
		}
#else
		embedded_firmware.data = _binary_dahdi_fw_oct6114_128_bin_start;
		/* Yes... this is weird. objcopy gives us a symbol containing
		   the size of the firmware, not a pointer a variable containing
		   the size. The only way we can get the value of the symbol
		   is to take its address, so we define it as a pointer and
		   then cast that value to the proper type.
		*/
		embedded_firmware.size = (size_t) &_binary_dahdi_fw_oct6114_128_bin_size;
#endif
		break;
	default:
		dev_notice(&wc->dev->dev, "Unsupported channel capacity found "
				"on VPM module (%d).\n", vpm_capacity);
		return;
	}

	if (!(wc->vpm450m = init_vpm450m(wc, laws, wc->numspans, firmware))) {
		dev_notice(&wc->dev->dev, "VPM450: Failed to initialize\n");
		if (firmware != &embedded_firmware)
			release_firmware(firmware);
		return;
	}

	if (firmware != &embedded_firmware)
		release_firmware(firmware);

	if (vpmdtmfsupport == -1) {
		dev_notice(&wc->dev->dev, "VPM450: hardware DTMF disabled.\n");
		vpmdtmfsupport = 0;
	}

	wc->vpm = T4_VPM_PRESENT;
	dev_info(&wc->dev->dev, "VPM450: Present and operational servicing %d "
			"span(s)\n", wc->numspans);
		
}

static void t4_vpm400_init(struct t4 *wc)
{
	unsigned char reg;
	unsigned int mask;
	unsigned int ver;
	unsigned int i, x, y, gen2vpm=0;

	if (!vpmsupport) {
		dev_info(&wc->dev->dev, "VPM400: Support Disabled\n");
		return;
	}

	switch(vpmspans) {
	case 4:
	case 2:
	case 1:
		break;
	default:
		dev_notice(&wc->dev->dev, "VPM400: %d is not a valid vpmspans "
				"value, using 4\n", vpmspans);
		vpmspans = 4;
	}

	for (x=0;x<8;x++) {
		int spanno = t4_vpm_spanno(x);
		struct t4_span *ts = wc->tspans[spanno];
		int echotail = t4_vpm_echotail();

		ver = t4_vpm_in(wc, x, 0x1a0); /* revision */
		if ((ver != 0x26) && (ver != 0x33)) {
			if (x)
				dev_notice(&wc->dev->dev,
					"VPM400: Inoperable\n");
			return;
		}
		if (ver == 0x33) {
			if (x && !gen2vpm) {
				dev_notice(&wc->dev->dev,
					"VPM400: Inconsistent\n");
				return;
			}
			ts->spanflags |= FLAG_VPM2GEN;
			gen2vpm++;
		} else if (gen2vpm) {
			dev_notice(&wc->dev->dev,
				"VPM400: Inconsistent\n");
			return;
		}


		/* Setup GPIO's */
		for (y=0;y<4;y++) {
			t4_vpm_out(wc, x, 0x1a8 + y, 0x00); /* GPIO out */
			t4_vpm_out(wc, x, 0x1ac + y, 0x00); /* GPIO dir */
			t4_vpm_out(wc, x, 0x1b0 + y, 0x00); /* GPIO sel */
		}

		/* Setup TDM path - sets fsync and tdm_clk as inputs */
		reg = t4_vpm_in(wc, x, 0x1a3); /* misc_con */
		t4_vpm_out(wc, x, 0x1a3, reg & ~2);

		/* Setup timeslots */
		t4_vpm_out(wc, x, 0x02f, 0x20 | (spanno << 3)); 

		/* Setup Echo length (128 taps) */
		t4_vpm_out(wc, x, 0x022, (echotail >> 8));
		t4_vpm_out(wc, x, 0x023, (echotail & 0xff));
		
		/* Setup the tdm channel masks for all chips*/
		mask = t4_vpm_mask(x);
		for (i = 0; i < 4; i++)
			t4_vpm_out(wc, x, 0x30 + i, (mask >> (i << 3)) & 0xff);

		/* Setup convergence rate */
		reg = t4_vpm_in(wc,x,0x20);
		reg &= 0xE0;
		if (ts->spantype == TYPE_E1) {
			if (x < vpmspans)
				dev_info(&wc->dev->dev, "VPM400: Span %d "
						"A-law mode\n", spanno);
			reg |= 0x01;
		} else {
			if (x < vpmspans)
				dev_info(&wc->dev->dev, "VPM400: Span %d "
						"U-law mode\n", spanno);
			reg &= ~0x01;
		}
		t4_vpm_out(wc,x,0x20,(reg | 0x20));
		
		/* Initialize echo cans */
		for (i = 0 ; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				t4_vpm_out(wc,x,i,0x00);
		}

		wait_a_little();

		/* Put in bypass mode */
		for (i = 0 ; i < MAX_TDM_CHAN ; i++) {
			if (mask & (0x00000001 << i)) {
				t4_vpm_out(wc,x,i,0x01);
			}
		}

		/* Enable bypass */
		for (i = 0 ; i < MAX_TDM_CHAN ; i++) {
			if (mask & (0x00000001 << i))
				t4_vpm_out(wc,x,0x78 + i,0x01);
		}
      
		/* set DTMF detection threshold */
		t4_vpm_set_dtmf_threshold(wc, dtmfthreshold);

		/* Enable DTMF detectors (always DTMF detect all spans) */
		for (i = 0; i < MAX_DTMF_DET; i++) {
			t4_vpm_out(wc, x, 0x98 + i, 0x40 | (i * 2) | ((x < 4) ? 0 : 1));
		}
		for (i = 0x34; i < 0x38; i++)
			t4_vpm_out(wc, x, i, 0x00);
		for (i = 0x3C; i < 0x40; i++)
			t4_vpm_out(wc, x, i, 0x00);

		for (i = 0x48; i < 0x4B; i++)
			t4_vpm_out(wc, x, i, 0x00);
		for (i = 0x50; i < 0x53; i++)
			t4_vpm_out(wc, x, i, 0x00);
		for (i = 0xB8; i < 0xBE; i++)
			t4_vpm_out(wc, x, i, 0xFF);
		if (gen2vpm) {
			for (i = 0xBE; i < 0xC0; i++)
				t4_vpm_out(wc, x, i, 0xFF);
		} else {
			for (i = 0xBE; i < 0xC0; i++)
				t4_vpm_out(wc, x, i, 0x00);
		}
		for (i = 0xC0; i < 0xC4; i++)
			t4_vpm_out(wc, x, i, (x < 4) ? 0x55 : 0xAA);

	} 
	if (vpmdtmfsupport == -1) {
		dev_info(&wc->dev->dev, "VPM400: hardware DTMF enabled.\n");
		vpmdtmfsupport = 0;
	}
	dev_info(&wc->dev->dev, "VPM400%s: Present and operational servicing "
		"%d span(s)\n", (gen2vpm ? " (2nd Gen)" : ""), wc->numspans);
	wc->vpm = T4_VPM_PRESENT;
}

#endif

static void t4_tsi_reset(struct t4 *wc) 
{
	int x;
	for (x=0;x<128;x++) {
		wc->dmactrl &= ~0x00007fff;
		wc->dmactrl |= (0x00004000 | (x << 7));
		t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	}
	wc->dmactrl &= ~0x00007fff;
	t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
}

/* Note that channels here start from 1 */
static void t4_tsi_assign(struct t4 *wc, int fromspan, int fromchan, int tospan, int tochan)
{
	unsigned long flags;
	int fromts, tots;

	fromts = (fromspan << 5) |(fromchan);
	tots = (tospan << 5) | (tochan);

	if (!wc->t1e1) {
		fromts += 4;
		tots += 4;
	}
	spin_lock_irqsave(&wc->reglock, flags);
	wc->dmactrl &= ~0x00007fff;
	wc->dmactrl |= (0x00004000 | (tots << 7) | (fromts));
	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	wc->dmactrl &= ~0x00007fff;
	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	spin_unlock_irqrestore(&wc->reglock, flags);
}

static void t4_tsi_unassign(struct t4 *wc, int tospan, int tochan)
{
	unsigned long flags;
	int tots;

	tots = (tospan << 5) | (tochan);

	if (!wc->t1e1) 
		tots += 4;
	spin_lock_irqsave(&wc->reglock, flags);
	wc->dmactrl &= ~0x00007fff;
	wc->dmactrl |= (0x00004000 | (tots << 7));
	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	if (debug & DEBUG_TSI)
		dev_notice(&wc->dev->dev, "Sending '%08x\n", wc->dmactrl);
	wc->dmactrl &= ~0x00007fff;
	__t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	spin_unlock_irqrestore(&wc->reglock, flags);
}
#ifdef CONFIG_EXTENDED_RESET
static void t4_extended_reset(struct t4 *wc)
{
	unsigned int oldreg = t4_pci_in(wc, 0x4);

	udelay(1000);

	t4_pci_out(wc, 0x4, 0x42000000);
	t4_pci_out(wc, 0xa, 0x42000000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0xa, 0x00080000);
	t4_pci_out(wc, 0xa, 0x00180000);
	t4_pci_out(wc, 0x4, oldreg);

	udelay(1000);
}
#endif

static int t4_hardware_init_1(struct t4 *wc, unsigned int cardflags)
{
	unsigned int version;

	version = t4_pci_in(wc, WC_VERSION);
	dev_info(&wc->dev->dev, "Firmware Version: %08x\n", version);
	dev_info(&wc->dev->dev, "Burst Mode: %s\n",
		(!(cardflags & FLAG_BURST) && noburst) ? "Off" : "On");
#ifdef ENABLE_WORKQUEUES
	dev_info(&wc->dev->dev, "Work Queues: Enabled\n");
#endif

#ifdef CONFIG_EXTENDED_RESET
	t4_extended_reset(wc);
#endif

	/* Make sure DMA engine is not running and interrupts are acknowledged */
	wc->dmactrl = 0x0;
	t4_pci_out(wc, WC_DMACTRL, wc->dmactrl);
	/* Reset Framer and friends */
	t4_pci_out(wc, WC_LEDS, 0x00000000);

	/* Set DMA addresses */
	t4_pci_out(wc, WC_RDADDR, wc->readdma);
	t4_pci_out(wc, WC_WRADDR, wc->writedma);

	/* Setup counters, interrupt flags (ignored in Gen2) */
	if (cardflags & FLAG_2NDGEN) {
		t4_tsi_reset(wc);
	} else {
		t4_pci_out(wc, WC_COUNT, ((DAHDI_MAX_CHUNKSIZE * 2 * 32 - 1) << 18) | ((DAHDI_MAX_CHUNKSIZE * 2 * 32 - 1) << 2));
	}
	
	/* Reset pending interrupts */
	t4_pci_out(wc, WC_INTR, 0x00000000);

	/* Read T1/E1 status */
	if (t1e1override > -1)
		wc->t1e1 = t1e1override;
	else
		wc->t1e1 = ((t4_pci_in(wc, WC_LEDS)) & 0x0f00) >> 8;
	wc->order = ((t4_pci_in(wc, WC_LEDS)) & 0xf0000000) >> 28;
	order_index[wc->order]++;
	return 0;
}

static int t4_hardware_init_2(struct t4 *wc)
{
	int x;
	unsigned int regval;

	if (t4_pci_in(wc, WC_VERSION) >= 0xc01a0165) {
		wc->tspans[0]->spanflags |= FLAG_OCTOPT;
		dev_info(&wc->dev->dev, "Octasic Optimizations: Enabled\n");
	}
	/* Setup LEDS, take out of reset */
	t4_pci_out(wc, WC_LEDS, 0x000000ff);
	t4_activate(wc);

	/* 
	 * In order to find out the QFALC framer version, we have to temporarily term off compat
	 * mode and take a peak at VSTR.  We turn compat back on when we are done.
	 */
	if (t4_framer_in(wc, 0, 0x4a) != 0x05)
		dev_info(&wc->dev->dev, "WARNING: FALC framer not intialized "
				"in compatibility mode.\n");
	regval = t4_framer_in(wc, 0 ,0xd6);
	regval |= (1 << 5); /* set COMP_DIS*/
	t4_framer_out(wc, 0, 0xd6, regval);
	regval = t4_framer_in(wc, 0, 0x4a);
	if (regval == 0x05)
		dev_info(&wc->dev->dev, "FALC Framer Version: 2.1 or "
				"earlier\n");
	else if (regval == 0x20) {
		dev_info(&wc->dev->dev, "FALC Framer Version: 3.1\n");
		wc->falc31 = 1;
	} else
		dev_info(&wc->dev->dev, "FALC Framer Version: Unknown "
				"(VSTR = 0x%02x)\n", regval);
	regval = t4_framer_in(wc, 0 ,0xd6);
	regval &= ~(1 << 5); /* clear COMP_DIS*/
	t4_framer_out(wc, 0, 0xd6, regval);
	
	t4_framer_out(wc, 0, 0x4a, 0xaa);
	dev_info(&wc->dev->dev, "Board ID: %02x\n", wc->order);

	for (x=0;x< 11;x++)
		dev_info(&wc->dev->dev, "Reg %d: 0x%08x\n", x,
				t4_pci_in(wc, x));
	return 0;
}

static int __devinit t4_launch(struct t4 *wc)
{
	int x;
	unsigned long flags;
	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &wc->tspans[0]->span.flags))
		return 0;
	dev_info(&wc->dev->dev, "TE%dXXP: Launching card: %d\n", wc->numspans,
			wc->order);

	/* Setup serial parameters and system interface */
	for (x=0;x<PORTS_PER_FRAMER;x++)
		t4_serial_setup(wc, x);

	if (dahdi_register(&wc->tspans[0]->span, 0)) {
		dev_err(&wc->dev->dev, "Unable to register span %s\n",
				wc->tspans[0]->span.name);
		return -1;
	}
	if (dahdi_register(&wc->tspans[1]->span, 0)) {
		dev_err(&wc->dev->dev, "Unable to register span %s\n",
				wc->tspans[1]->span.name);
		dahdi_unregister(&wc->tspans[0]->span);
		return -1;
	}

	if (wc->numspans == 4) {
		if (dahdi_register(&wc->tspans[2]->span, 0)) {
			dev_err(&wc->dev->dev, "Unable to register span %s\n",
					wc->tspans[2]->span.name);
			dahdi_unregister(&wc->tspans[0]->span);
			dahdi_unregister(&wc->tspans[1]->span);
			return -1;
		}
		if (dahdi_register(&wc->tspans[3]->span, 0)) {
			dev_err(&wc->dev->dev, "Unable to register span %s\n",
					wc->tspans[3]->span.name);
			dahdi_unregister(&wc->tspans[0]->span);
			dahdi_unregister(&wc->tspans[1]->span);
			dahdi_unregister(&wc->tspans[2]->span);
			return -1;
		}
	}
	set_bit(T4_CHECK_TIMING, &wc->checkflag);
	spin_lock_irqsave(&wc->reglock, flags);
	__t4_set_sclk_src(wc, WC_SELF, 0, 0);
	spin_unlock_irqrestore(&wc->reglock, flags);
	tasklet_init(&wc->t4_tlet, t4_isr_bh, (unsigned long)wc);
	return 0;
}

static void free_wc(struct t4 *wc)
{
	unsigned int x, y;

	for (x = 0; x < sizeof(wc->tspans)/sizeof(wc->tspans[0]); x++) {
		if (!wc->tspans[x]) {
			continue;
		}

		for (y = 0; y < sizeof(wc->tspans[x]->chans)/sizeof(wc->tspans[x]->chans[0]); y++) {
			if (wc->tspans[x]->chans[y]) {
				kfree(wc->tspans[x]->chans[y]);
			}
			if (wc->tspans[x]->ec[y])
				kfree(wc->tspans[x]->ec[y]);
		}
		kfree(wc->tspans[x]);
	}
	kfree(wc);
}

static int __devinit t4_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct t4 *wc;
	struct devtype *dt;
	unsigned int x, f;
	int init_latency;
	
	if (pci_enable_device(pdev)) {
		return -EIO;
	}

	if (!(wc = kmalloc(sizeof(*wc), GFP_KERNEL))) {
		return -ENOMEM;
	}

	memset(wc, 0x0, sizeof(*wc));
	spin_lock_init(&wc->reglock);
	dt = (struct devtype *) (ent->driver_data);

	wc->flags = dt->flags;

	if (wc->flags & FLAG_2PORT) 
		wc->numspans = 2;
	else
		wc->numspans = 4;
	
	wc->variety = dt->desc;
	
	wc->memaddr = pci_resource_start(pdev, 0);
	wc->memlen = pci_resource_len(pdev, 0);
	wc->membase = ioremap(wc->memaddr, wc->memlen);
	/* This rids of the Double missed interrupt message after loading */
	wc->last0 = 1;
#if 0
	if (!request_mem_region(wc->memaddr, wc->memlen, wc->variety))
		dev_info(&wc->dev->dev, "wct4: Unable to request memory "
				"region :(, using anyway...\n");
#endif
	if (pci_request_regions(pdev, wc->variety))
		dev_info(&pdev->dev, "wct%dxxp: Unable to request regions\n",
				wc->numspans);
	
	dev_info(&pdev->dev, "Found TE%dXXP at base address %08lx, remapped "
			"to %p\n", wc->numspans, wc->memaddr, wc->membase);
	
	wc->dev = pdev;
	
	/* Enable bus mastering */
	pci_set_master(pdev);

	/* Keep track of which device we are */
	pci_set_drvdata(pdev, wc);

	if (wc->flags & FLAG_5THGEN) {
		if ((ms_per_irq > 1) && (latency <= ((ms_per_irq) << 1))) {
			init_latency = ms_per_irq << 1;
		} else {
			if (latency > 2)
				init_latency = latency;
			else
				init_latency = 2;
		}
		dev_info(&wc->dev->dev, "5th gen card with initial latency of "
			"%d and %d ms per IRQ\n", init_latency, ms_per_irq);
	} else {
		if (wc->flags & FLAG_2NDGEN)
			init_latency = 1;
		else
			init_latency = 2;
	}

	if (max_latency < init_latency) {
		printk(KERN_INFO "maxlatency must be set to something greater than %d ms, increasing it to %d\n", init_latency, init_latency);
		max_latency = init_latency;
	}
	
	if (t4_allocate_buffers(wc, init_latency, NULL, NULL)) {
		return -ENOMEM;
	}

	/* Initialize hardware */
	t4_hardware_init_1(wc, wc->flags);
	
	for(x = 0; x < MAX_T4_CARDS; x++) {
		if (!cards[x])
			break;
	}
	
	if (x >= MAX_T4_CARDS) {
		dev_notice(&wc->dev->dev, "No cards[] slot available!!\n");
		kfree(wc);
		return -ENOMEM;
	}
	
	wc->num = x;
	cards[x] = wc;
	
#ifdef ENABLE_WORKQUEUES
	if (wc->flags & FLAG_2NDGEN) {
		char tmp[20];

		sprintf(tmp, "te%dxxp[%d]", wc->numspans, wc->num);
		wc->workq = create_workqueue(tmp);
	}
#endif			

	/* Allocate pieces we need here */
	for (x = 0; x < PORTS_PER_FRAMER; x++) {
		if (!(wc->tspans[x] = kmalloc(sizeof(*wc->tspans[x]), GFP_KERNEL))) {
			free_wc(wc);
			return -ENOMEM;
		}

		memset(wc->tspans[x], 0, sizeof(*wc->tspans[x]));

		if (wc->t1e1 & (1 << x)) {
			wc->tspans[x]->spantype = TYPE_E1;
		} else {
			if (j1mode)
				wc->tspans[x]->spantype = TYPE_J1;
			else
				wc->tspans[x]->spantype = TYPE_T1;
		}

		for (f = 0; f < (wc->tspans[x]->spantype == TYPE_E1 ? 31 : 24); f++) {
			if (!(wc->tspans[x]->chans[f] = kmalloc(sizeof(*wc->tspans[x]->chans[f]), GFP_KERNEL))) {
				free_wc(wc);
				return -ENOMEM;
			}
			memset(wc->tspans[x]->chans[f], 0, sizeof(*wc->tspans[x]->chans[f]));
			if (!(wc->tspans[x]->ec[f] = kmalloc(sizeof(*wc->tspans[x]->ec[f]), GFP_KERNEL))) {
				free_wc(wc);
				return -ENOMEM;
			}
			memset(wc->tspans[x]->ec[f], 0, sizeof(*wc->tspans[x]->ec[f]));
		}

#ifdef ENABLE_WORKQUEUES
		INIT_WORK(&wc->tspans[x]->swork, workq_handlespan, wc->tspans[x]);
#endif				
		wc->tspans[x]->spanflags |= wc->flags;
	}
	
	/* Continue hardware intiialization */
	t4_hardware_init_2(wc);
	
#ifdef SUPPORT_GEN1
	if (request_irq(pdev->irq, (wc->flags & FLAG_2NDGEN) ? t4_interrupt_gen2 : t4_interrupt, DAHDI_IRQ_SHARED_DISABLED, (wc->numspans == 2) ? "wct2xxp" : "wct4xxp", wc)) 
#else
		if (!(wc->tspans[0]->spanflags & FLAG_2NDGEN)) {
			dev_notice(&wc->dev->dev, "This driver does not "
					"support 1st gen modules\n");
			free_wc(wc);
			return -ENODEV;
		}	
	if (request_irq(pdev->irq, t4_interrupt_gen2, DAHDI_IRQ_SHARED_DISABLED, "t4xxp", wc)) 
#endif
	{
		dev_notice(&wc->dev->dev, "t4xxp: Unable to request IRQ %d\n",
				pdev->irq);
		free_wc(wc);
		return -EIO;
	}
	
	init_spans(wc);
	/* get the current number of probed cards and run a slice of a tail
	 * insertion sort */
	for (x = 0; x < MAX_T4_CARDS; x++) {
		if (!cards[x+1])
			break;
	}
	for ( ; x > 0; x--) {
		if (cards[x]->order < cards[x-1]->order) {
			struct t4 *tmp = cards[x];
			cards[x] = cards[x-1];
			cards[x-1] = tmp;
		} else {
			/* if we're not moving it, we won't move any more
			 * since all cards are sorted on addition */
			break;
		}
	}
	
	dev_info(&wc->dev->dev, "Found a Wildcard: %s\n", wc->variety);
	wc->gpio = 0x00000000;
	t4_pci_out(wc, WC_GPIO, wc->gpio);
	t4_gpio_setdir(wc, (1 << 17), (1 << 17));
	t4_gpio_setdir(wc, (0xff), (0xff));

	create_sysfs_files(wc);
	
#if 0
	for (x=0;x<0x10000;x++) {
		__t4_raw_oct_out(wc, 0x0004, x);
		__t4_raw_oct_out(wc, 0x000a, x ^ 0xffff);
		if (__t4_raw_oct_in(wc, 0x0004) != x) 
			dev_notice(&wc->dev->dev, "Register 4 failed %04x\n",
					x);
		if (__t4_raw_oct_in(wc, 0x000a) != (x ^ 0xffff))
			dev_notice(&wc->dev->dev, "Register 10 failed %04x\n",
					x);
	}
#endif

	return 0;
}

static int t4_hardware_stop(struct t4 *wc)
{

	/* Turn off DMA, leave interrupts enabled */
	set_bit(T4_STOP_DMA, &wc->checkflag);

	/* Wait for interrupts to stop */
	msleep(25);

	/* Turn off counter, address, etc */
	if (wc->tspans[0]->spanflags & FLAG_2NDGEN) {
		t4_tsi_reset(wc);
	} else {
		t4_pci_out(wc, WC_COUNT, 0x000000);
	}
	t4_pci_out(wc, WC_RDADDR, 0x0000000);
	t4_pci_out(wc, WC_WRADDR, 0x0000000);
	wc->gpio = 0x00000000;
	t4_pci_out(wc, WC_GPIO, wc->gpio);
	t4_pci_out(wc, WC_LEDS, 0x00000000);

	dev_notice(&wc->dev->dev, "\nStopped TE%dXXP, Turned off DMA\n",
			wc->numspans);
	return 0;
}

static void __devexit t4_remove_one(struct pci_dev *pdev)
{
	struct t4 *wc = pci_get_drvdata(pdev);
	struct dahdi_span *span;
	int basesize;
	int i;

	if (!wc) {
		return;
	}

	remove_sysfs_files(wc);

	/* Stop hardware */
	t4_hardware_stop(wc);
	
	/* Release vpm450m */
	if (wc->vpm450m)
		release_vpm450m(wc->vpm450m);
	wc->vpm450m = NULL;
	/* Unregister spans */

	basesize = DAHDI_MAX_CHUNKSIZE * 32 * 4;
	if (!(wc->tspans[0]->spanflags & FLAG_2NDGEN))
		basesize = basesize * 2;

	for (i = 0; i < wc->numspans; ++i) {
		span = &wc->tspans[i]->span;
		if (test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags))
			dahdi_unregister(span);
	}
#ifdef ENABLE_WORKQUEUES
	if (wc->workq) {
		flush_workqueue(wc->workq);
		destroy_workqueue(wc->workq);
	}
#endif			
	
	free_irq(pdev->irq, wc);
	
	if (wc->membase)
		iounmap(wc->membase);
	
	pci_release_regions(pdev);		
	
	/* Immediately free resources */
	pci_free_consistent(pdev, T4_BASE_SIZE * wc->numbufs * 2, (void *)wc->writechunk, wc->writedma);
	
	order_index[wc->order]--;
	
	cards[wc->num] = NULL;
	pci_set_drvdata(pdev, NULL);
	free_wc(wc);
}


static struct pci_device_id t4_pci_tbl[] __devinitdata =
{
	{ 0x10ee, 0x0314, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&wct4xxp },

 	{ 0xd161, 0x1420, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct420p5 },
	{ 0xd161, 0x1410, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct410p5 },
	{ 0xd161, 0x1405, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct405p5 },
 	{ 0xd161, 0x0420, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct420p4 },
	{ 0xd161, 0x0410, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct410p4 },
	{ 0xd161, 0x0405, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct405p4 },
	{ 0xd161, 0x0410, 0x0003,     PCI_ANY_ID, 0, 0, (unsigned long)&wct410p3 },
	{ 0xd161, 0x0405, 0x0003,     PCI_ANY_ID, 0, 0, (unsigned long)&wct405p3 },
	{ 0xd161, 0x0410, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&wct410p2 },
	{ 0xd161, 0x0405, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&wct405p2 },

 	{ 0xd161, 0x1220, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct220p5 },
	{ 0xd161, 0x1205, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct205p5 },
	{ 0xd161, 0x1210, 0x0005,     PCI_ANY_ID, 0, 0, (unsigned long)&wct210p5 },
 	{ 0xd161, 0x0220, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct220p4 },
	{ 0xd161, 0x0205, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct205p4 },
	{ 0xd161, 0x0210, 0x0004,     PCI_ANY_ID, 0, 0, (unsigned long)&wct210p4 },
	{ 0xd161, 0x0205, 0x0003,     PCI_ANY_ID, 0, 0, (unsigned long)&wct205p3 },
	{ 0xd161, 0x0210, 0x0003,     PCI_ANY_ID, 0, 0, (unsigned long)&wct210p3 },
	{ 0xd161, 0x0205, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&wct205 },
	{ 0xd161, 0x0210, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&wct210 },
	{ 0, }
};

static struct pci_driver t4_driver = {
	.name = "wct4xxp",
	.probe = t4_init_one,
	.remove = __devexit_p(t4_remove_one),
	.id_table = t4_pci_tbl,
};

static int __init t4_init(void)
{
	int res;
	res = dahdi_pci_module(&t4_driver);
	if (res)
		return -ENODEV;
	/* initialize cards since we have all of them */
	/* warn for missing zero and duplicate numbers */
	if (cards[0] && cards[0]->order != 0) {
		printk(KERN_NOTICE "wct4xxp: Ident of first card is not zero (%d)\n",
			cards[0]->order);
	}
	for (res = 0; cards[res]; res++) {
		/* warn the user of duplicate ident values it is probably
		 * unintended */
		if (debug && res < 15 && cards[res+1] &&
		    cards[res]->order == cards[res+1]->order) {
			printk(KERN_NOTICE "wct4xxp: Duplicate ident value found (%d)\n",
				cards[res]->order);
		}
		t4_launch(cards[res]);
	}
	return 0;
}

static void __exit t4_cleanup(void)
{
	pci_unregister_driver(&t4_driver);
}


MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_DESCRIPTION("Wildcard Dual-/Quad-port Digital Card Driver");
MODULE_ALIAS("wct2xxp");
MODULE_LICENSE("GPL v2");

module_param(pedanticpci, int, 0600);
module_param(debug, int, 0600);
module_param(noburst, int, 0600);
module_param(timingcable, int, 0600);
module_param(t1e1override, int, 0600);
module_param(alarmdebounce, int, 0600);
module_param(losalarmdebounce, int, 0600);
module_param(aisalarmdebounce, int, 0600);
module_param(yelalarmdebounce, int, 0600);
module_param(max_latency, int, 0600);
module_param(j1mode, int, 0600);
module_param(sigmode, int, 0600);
module_param(latency, int, 0600);
module_param(ms_per_irq, int, 0600);
#ifdef VPM_SUPPORT
module_param(vpmsupport, int, 0600);
module_param(vpmdtmfsupport, int, 0600);
module_param(vpmspans, int, 0600);
module_param(dtmfthreshold, int, 0600);
#endif

MODULE_DEVICE_TABLE(pci, t4_pci_tbl);

module_init(t4_init);
module_exit(t4_cleanup);
