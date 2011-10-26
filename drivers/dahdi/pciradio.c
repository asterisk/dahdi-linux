/*
 * PCI RADIO Card DAHDI Telephony PCI Quad Radio Interface driver
 *
 * Written by Jim Dixon <jim@lambdatel.com>
 * Based on previous work by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and archetectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001-2007 Jim Dixon / Zapata Telephony.
 *
 * All rights reserved.
 *
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

/*
  The PCI Radio Interface card interfaces up to 4 two-way radios (either
  a base/mobile radio or repeater system) to DAHDI channels. The driver
  may work either independent of an application, or with it, through
  the driver;s ioctl() interface. This file gives you access to specify
  load-time parameters for Radio channels, so that the driver may run
  by itself, and just act like a generic DAHDI radio interface.
*/

/* Latency tests:

Without driver:	308496
With driver:	303826  (1.5 %)

*/


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/delay.h> 

#include <dahdi/kernel.h>

#define RAD_MAX_IFACES 128

#define	NUM_CODES 15

#define	SERIAL_BUFLEN 128

#define	SRX_TIMEOUT 300

#define RAD_CNTL    	0x00
#define RAD_OPER	0x01
#define RAD_AUXC    	0x02
#define RAD_AUXD    	0x03
	#define	XPGM 4
	#define	XCS 2

#define RAD_MASK0   	0x04
#define RAD_MASK1   	0x05
#define RAD_INTSTAT 	0x06
#define RAD_AUXR	0x07
	#define	XINIT 8
	#define	XDONE 0x10

#define RAD_DMAWS	0x08
#define RAD_DMAWI	0x0c
#define RAD_DMAWE	0x10
#define RAD_DMARS	0x18
#define RAD_DMARI	0x1c
#define RAD_DMARE	0x20

#define RAD_AUXFUNC	0x2b
#define RAD_SERCTL	0x2d
#define RAD_FSCDELAY	0x2f

#define RAD_REGBASE	0xc0

#define	RAD_CTCSSMASK	0xf
#define	RAD_CTCSSOTHER	0xf
#define	RAD_CTCSSVALID	0x10

#define NUM_CHANS 4

#define	RAD_GOTRX_DEBOUNCE_TIME 75
#define RAD_CTCSS_ACQUIRE_TIME 10
#define RAD_CTCSS_TALKOFF_TIME 1000

#define DAHDI_RADPAR_CTCSSACQUIRETIME 18 /* DEBUG only, this belongs in dahdi.h */
#define DAHDI_RADPAR_CTCSSTALKOFFTIME 19 /* DEBUG only, this belongs in dahdi.h */

/*
* MX828 Commands
*/
                                                                                
#define MX828_GEN_RESET         0x01            /* W */
#define MX828_SAUDIO_CTRL       0x80            /* W */
#define MX828_SAUDIO_STATUS     0x81            /* R */
#define MX828_SAUDIO_SETUP      0x82            /* W */
#define MX828_TX_TONE           0x83            /* W16 */
#define MX828_RX_TONE           0x84            /* W16 */
#define MX828_DCS3              0x85            /* W */
#define MX828_DCS2              0x86            /* W */
#define MX828_DCS1              0x87            /* W */
#define MX828_GEN_CTRL          0x88            /* W */
#define MX828_GPT               0x8B            /* W */
#define MX828_IRQ_MASK          0x8E            /* W */
#define MX828_SELCALL           0x8D            /* W16 */
#define MX828_AUD_CTRL          0x8A            /* W16 */
#define MX828_IRQ_FLAG          0x8F            /* R */
                                                                                

struct encdec
{
	unsigned char state;  /* 0 = idle */
	int chan;
	unsigned char req[NUM_CHANS];
	unsigned char dcsrx[NUM_CHANS];
	unsigned char ctrx[NUM_CHANS];
	unsigned char dcstx[NUM_CHANS];
	unsigned char cttx[NUM_CHANS];
	unsigned char saudio_ctrl[NUM_CHANS];
	unsigned char saudio_setup[NUM_CHANS];
	unsigned char txcode[NUM_CHANS];
	unsigned long lastcmd;
	int myindex[NUM_CHANS];
	unsigned long waittime;
	unsigned char retstate;
} ;


struct pciradio {
	struct pci_dev *dev;
	struct dahdi_device *ddev;
	struct dahdi_span span;
	unsigned char ios;
	int usecount;
	unsigned int intcount;
	int dead;
	int pos;
	int freeregion;
	int nchans;
	spinlock_t lock;
	int remote_locked;
	unsigned char rxbuf[SERIAL_BUFLEN];
	unsigned short rxindex;
	unsigned long srxtimer;
	unsigned char txbuf[SERIAL_BUFLEN];
	unsigned short txindex;
	unsigned short txlen;
	unsigned char pasave;
	unsigned char pfsave;
	volatile unsigned long ioaddr;
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile unsigned int *writechunk;	/* Double-word aligned write memory */
	volatile unsigned int *readchunk;	/* Double-word aligned read memory */
	unsigned char saudio_status[NUM_CHANS];
	char gotcor[NUM_CHANS];
	char gotct[NUM_CHANS];
	char newctcssstate[NUM_CHANS];
	char ctcssstate[NUM_CHANS];
	char gotrx[NUM_CHANS];
	char gotrx1[NUM_CHANS];
	char gottx[NUM_CHANS];
	char lasttx[NUM_CHANS];
	int gotrxtimer[NUM_CHANS];
	int ctcsstimer[NUM_CHANS];
	int debouncetime[NUM_CHANS];
	int ctcssacquiretime[NUM_CHANS];
	int ctcsstalkofftime[NUM_CHANS];
	int bursttime[NUM_CHANS];
	int bursttimer[NUM_CHANS];
	unsigned char remmode[NUM_CHANS];
	unsigned short present_code[NUM_CHANS];
	unsigned short last_code[NUM_CHANS];
	unsigned short rxcode[NUM_CHANS][NUM_CODES + 1];
	unsigned short rxclass[NUM_CHANS][NUM_CODES + 1];
	unsigned short txcode[NUM_CHANS][NUM_CODES + 1];;
	unsigned char radmode[NUM_CHANS]; 
#define	RADMODE_INVERTCOR 1
#define	RADMODE_IGNORECOR 2
#define	RADMODE_EXTTONE	4
#define	RADMODE_EXTINVERT 8
#define	RADMODE_IGNORECT 16
#define	RADMODE_NOENCODE 32
	unsigned char corthresh[NUM_CHANS];
	struct dahdi_chan _chans[NUM_CHANS];
	struct dahdi_chan *chans;
	unsigned char mx828_addr;
	struct encdec encdec;
	unsigned long lastremcmd;
};


static struct pciradio *ifaces[RAD_MAX_IFACES];

static void pciradio_release(struct pciradio *rad);

static int debug = 0;

struct tonedef {
	int code;
	unsigned char b1;
	unsigned char b2;
} ;

#include "radfw.h"

static struct tonedef cttable_tx [] = {
{0,0,0},
{670,0xE,0xB1},
{693,0xE,0x34},
{719,0xD,0xB1},
{744,0xD,0x3B},
{770,0xC,0xC9},
{797,0xC,0x5A},
{825,0xB,0xEF},
{854,0xB,0x87},
{885,0xB,0x1F},
{915,0xA,0xC2},
{948,0xA,0x62},
{974,0xA,0x1B},
{1000,0x9,0xD8},
{1035,0x9,0x83},
{1072,0x9,0x2F},
{1109,0x8,0xE0},
{1148,0x8,0x93},
{1188,0x8,0x49},
{1230,0x8,0x1},
{1273,0x7,0xBC},
{1318,0x7,0x78},
{1365,0x7,0x36},
{1413,0x6,0xF7},
{1462,0x6,0xBC},
{1514,0x6,0x80},
{1567,0x6,0x48},
{1598,0x6,0x29},
{1622,0x6,0x12},
{1679,0x5,0xDD},
{1738,0x5,0xAA},
{1799,0x5,0x79},
{1835,0x5,0x5D},
{1862,0x5,0x49},
{1899,0x5,0x2F},
{1928,0x5,0x1B},
{1966,0x5,0x2},
{1995,0x4,0xEF},
{2035,0x4,0xD6},
{2065,0x4,0xC4},
{2107,0x4,0xAC},
{2181,0x4,0x83},
{2257,0x4,0x5D},
{2291,0x4,0x4C},
{2336,0x4,0x37},
{2418,0x4,0x12},
{2503,0x3,0xEF},
{2541,0x3,0xE0},
{0,0,0}
} ;

static struct tonedef cttable_rx [] = {
{0,0,0},
{670,0x3,0xD8},
{693,0x4,0x9},
{719,0x4,0x1B},
{744,0x4,0x4E},
{770,0x4,0x83},
{797,0x4,0x94},
{825,0x4,0xCB},
{854,0x5,0x2},
{885,0x5,0x14},
{915,0x5,0x4C},
{948,0x5,0x87},
{974,0x5,0x94},
{1000,0x5,0xCB},
{1035,0x6,0x7},
{1072,0x6,0x45},
{1109,0x6,0x82},
{1148,0x6,0xC0},
{1188,0x6,0xD1},
{1230,0x7,0x10},
{1273,0x7,0x50},
{1318,0x7,0xC0},
{1365,0x8,0x2},
{1413,0x8,0x44},
{1462,0x8,0x86},
{1514,0x8,0xC9},
{1567,0x9,0xC},
{1598,0x9,0x48},
{1622,0x9,0x82},
{1679,0x9,0xC6},
{1738,0xA,0xB},
{1799,0xA,0x84},
{1835,0xA,0xC2},
{1862,0xA,0xC9},
{1899,0xB,0x8},
{1928,0xB,0x44},
{1966,0xB,0x83},
{1995,0xB,0x8A},
{2035,0xB,0xC9},
{2065,0xC,0x6},
{2107,0xC,0x46},
{2181,0xC,0xC3},
{2257,0xD,0x41},
{2291,0xD,0x48},
{2336,0xD,0x89},
{2418,0xE,0x8},
{2503,0xE,0x88},
{2541,0xE,0xC7},
{0,0,0}
};

static struct {
	int code;
	char b3;
	char b2;
	char b1;
} dcstable[] = {
{0,0,0,0},
{23,0x76,0x38,0x13},
{25,0x6B,0x78,0x15},
{26,0x65,0xD8,0x16},
{31,0x51,0xF8,0x19},
{32,0x5F,0x58,0x1A},
{43,0x5B,0x68,0x23},
{47,0x0F,0xD8,0x27},
{51,0x7C,0xA8,0x29},
{54,0x6F,0x48,0x2C},
{65,0x5D,0x18,0x35},
{71,0x67,0x98,0x39},
{72,0x69,0x38,0x3A},
{73,0x2E,0x68,0x3B},
{74,0x74,0x78,0x3C},
{114,0x35,0xE8,0x4C},
{115,0x72,0xB8,0x4D},
{116,0x7C,0x18,0x4E},
{125,0x07,0xB8,0x55},
{131,0x3D,0x38,0x59},
{132,0x33,0x98,0x5A},
{134,0x2E,0xD8,0x5C},
{143,0x37,0xA8,0x63},
{152,0x1E,0xC8,0x6A},
{155,0x44,0xD8,0x6D},
{156,0x4A,0x78,0x6E},
{162,0x6B,0xC8,0x72},
{165,0x31,0xD8,0x75},
{172,0x05,0xF8,0x7A},
{174,0x18,0xB8,0x7C},
{205,0x6E,0x98,0x85},
{223,0x68,0xE8,0x93},
{226,0x7B,0x08,0x96},
{243,0x45,0xB8,0xA3},
{244,0x1F,0xA8,0xA4},
{245,0x58,0xF8,0xA5},
{251,0x62,0x78,0xA9},
{261,0x17,0x78,0xB1},
{263,0x5E,0x88,0xB3},
{265,0x43,0xC8,0xB5},
{271,0x79,0x48,0xB9},
{306,0x0C,0xF8,0xC6},
{311,0x38,0xD8,0xC9},
{315,0x6C,0x68,0xCD},
{331,0x23,0xE8,0xD9},
{343,0x29,0x78,0xE3},
{346,0x3A,0x98,0xE6},
{351,0x0E,0xB8,0xE9},
{364,0x68,0x58,0xF4},
{365,0x2F,0x08,0xF5},
{371,0x15,0x88,0xF9},
{411,0x77,0x69,0x09},
{412,0x79,0xC9,0x0A},
{413,0x3E,0x99,0x0B},
{423,0x4B,0x99,0x13},
{431,0x6C,0x59,0x19},
{432,0x62,0xF9,0x1A},
{445,0x7B,0x89,0x25},
{464,0x27,0xE9,0x34},
{465,0x60,0xB9,0x35},
{466,0x6E,0x19,0x36},
{503,0x3C,0x69,0x43},
{506,0x2F,0x89,0x46},
{516,0x41,0xB9,0x4E},
{532,0x0E,0x39,0x5A},
{546,0x19,0xE9,0x66},
{565,0x0C,0x79,0x75},
{606,0x5D,0x99,0x86},
{612,0x67,0x19,0x8A},
{624,0x0F,0x59,0x94},
{627,0x01,0xF9,0x97},
{631,0x72,0x89,0x99},
{632,0x7C,0x29,0x9A},
{654,0x4C,0x39,0xAC},
{662,0x24,0x79,0xB2},
{664,0x39,0x39,0xB4},
{703,0x22,0xB9,0xC3},
{712,0x0B,0xD9,0xCA},
{723,0x39,0x89,0xD3},
{731,0x1E,0x49,0xD9},
{732,0x10,0xE9,0xDA},
{734,0x0D,0xA9,0xDC},
{743,0x14,0xD9,0xE3},
{754,0x20,0xF9,0xEC},
{0,0,0,0}
};

static int gettxtone(int code)
{
int	i;

	if (!code) return(0);
	for(i = 0; cttable_tx[i].code || (!i); i++)
	{
		if (cttable_tx[i].code == code)
		{
			return (i);
		}
	}
	return(-1);
}

static int getrxtone(int code)
{
int	i;

	if (!code) return(0);
	for(i = 0; cttable_rx[i].code || (!i); i++)
	{
		if (cttable_rx[i].code == code)
		{
			return (i);
		}
	}
	return(-1);
}


static int getdcstone(int code)
{
int	i;

	if (!code) return(0);
	for(i = 0; dcstable[i].code || (!i); i++)
	{
		if (dcstable[i].code == code)
		{
			return (i);
		}
	}
	return(-1);
}


static void __pciradio_setcreg(struct pciradio *rad, unsigned char reg, unsigned char val)
{
	outb(val, rad->ioaddr + RAD_REGBASE + ((reg & 0xf) << 2));
}

static unsigned char __pciradio_getcreg(struct pciradio *rad, unsigned char reg)
{
	return inb(rad->ioaddr + RAD_REGBASE + ((reg & 0xf) << 2));
}

static void rbi_out(struct pciradio *rad, int n, unsigned char *rbicmd)
{
unsigned long flags;
int	x;
DECLARE_WAIT_QUEUE_HEAD(mywait);


	for(;;)
	{
		spin_lock_irqsave(&rad->lock,flags);
		x = rad->remote_locked || (__pciradio_getcreg(rad,0xc) & 2);
		if (!x) rad->remote_locked = 1;
		spin_unlock_irqrestore(&rad->lock,flags);
		if (x) interruptible_sleep_on_timeout(&mywait,2);
		else break;
	}	
	spin_lock_irqsave(&rad->lock,flags);
	/* enable and address RBI serializer */
	__pciradio_setcreg(rad,0xf,rad->pfsave | (n << 4) | 0x40);
	/* output commands */
	for(x = 0; x < 5; x++) __pciradio_setcreg(rad,0xc,rbicmd[x]);
	/* output it */
	__pciradio_setcreg(rad,0xb,1);
	rad->remote_locked = 0;
	spin_unlock_irqrestore(&rad->lock,flags);
	return;
}


/*
* Output a command to the MX828 over the serial bus
*/


static void mx828_command(struct pciradio *rad,int channel, unsigned char command, unsigned char *byte1, unsigned char *byte2)
{

	if(channel > 3)
		return;

	rad->mx828_addr = channel;
	__pciradio_setcreg(rad,0,channel);
	if (byte1) __pciradio_setcreg(rad,1,*byte1);
	if (byte2) __pciradio_setcreg(rad,2,*byte2);
	__pciradio_setcreg(rad,3,command);
	
}

static void mx828_command_wait(struct pciradio *rad,int channel, unsigned char command, unsigned char *byte1, unsigned char *byte2)
{
DECLARE_WAIT_QUEUE_HEAD(mywait);
unsigned long flags;


	spin_lock_irqsave(&rad->lock,flags);  
	while(rad->encdec.state)
	{
		spin_unlock_irqrestore(&rad->lock,flags);  
		interruptible_sleep_on_timeout(&mywait,2);   
		spin_lock_irqsave(&rad->lock,flags);  
	}
	rad->encdec.lastcmd = jiffies + 1000;
	spin_unlock_irqrestore(&rad->lock,flags);  
	while(__pciradio_getcreg(rad,0xc) & 1);
	rad->encdec.lastcmd = jiffies + 1000;
	spin_lock_irqsave(&rad->lock,flags);  
	rad->encdec.lastcmd = jiffies + 1000;
	mx828_command(rad,channel,command,byte1,byte2);
	spin_unlock_irqrestore(&rad->lock,flags);  
	rad->encdec.lastcmd = jiffies + 1000;
	while(__pciradio_getcreg(rad,0xc) & 1);
	rad->encdec.lastcmd = jiffies;
}

static void _do_encdec(struct pciradio *rad)
{
int	i,n;
unsigned char byte1 = 0,byte2 = 0;

	/* return doing nothing if busy */
	if ((rad->encdec.lastcmd + 2) > jiffies) return;
	if (__pciradio_getcreg(rad,0xc) & 1) return;
	n = 0;
	byte2 = 0;
	switch(rad->encdec.state)
	{
	    case 0:
		for(i = 0; i < rad->nchans; i++)
		{
			n = (unsigned)(i - rad->intcount) % rad->nchans;
			if (rad->encdec.req[n]) break;
		}
		if (i >= rad->nchans) return;
		rad->encdec.req[n] = 0;
		rad->encdec.dcsrx[n] = 0;
		rad->encdec.ctrx[n] = 0;
		rad->encdec.dcstx[n] = 0;
		rad->encdec.cttx[n] = 0;
		rad->encdec.myindex[n] = 0;
		rad->encdec.req[n] = 0;
		rad->encdec.chan = n;

		/* if something in code 0 for rx, is DCS */
		if (rad->rxcode[n][0]) rad->encdec.dcsrx[n] = 1;
		else { /* otherwise, if something in other codes, is CT rx */
			for(i = 1; i <= NUM_CODES; i++)
			{
				if (rad->rxcode[n][1]) rad->encdec.ctrx[n] = 1;
			}
		}		
		/* get index for tx code. Will be 0 if not receiving a CT */
		rad->encdec.myindex[n] = 0;
		if (rad->gotrx[n] && rad->encdec.ctrx[n] && (rad->present_code[n])) 
			rad->encdec.myindex[n] = rad->present_code[n];
		/* get actual tx code from array */
		rad->encdec.txcode[n] = rad->txcode[n][rad->encdec.myindex[n]];
		if (rad->encdec.txcode[n] & 0x8000) rad->encdec.dcstx[n] = 1;
		     else if (rad->encdec.txcode[n]) rad->encdec.cttx[n] = 1;
		if (rad->radmode[n] & RADMODE_NOENCODE) 
			rad->encdec.dcstx[n] = rad->encdec.cttx[n] = 0;
		if ((!rad->gottx[n]) || rad->bursttimer[n]) 
			rad->encdec.dcstx[n] = rad->encdec.cttx[n] = 0;
		rad->encdec.saudio_ctrl[n] = 0;
		rad->encdec.saudio_setup[n] = 0;
		rad->encdec.state = 1;
		break;
	    case 1:
		if (rad->encdec.dcstx[rad->encdec.chan] && (!rad->encdec.dcsrx[rad->encdec.chan])) /* if to transmit DCS */
		{
			rad->encdec.saudio_setup[rad->encdec.chan] |= 3;
			rad->encdec.saudio_ctrl[rad->encdec.chan] |= 0x80;
			byte1 = dcstable[rad->encdec.txcode[rad->encdec.chan] & 0x7fff].b1;
			mx828_command(rad,rad->encdec.chan, MX828_DCS1, &byte1, &byte2 );
			rad->encdec.state = 2;
			break;
		} 
		rad->encdec.state = 4;
		break;
	    case 2:	
		byte1 = dcstable[rad->encdec.txcode[rad->encdec.chan] & 0x7fff].b2;
		mx828_command(rad,rad->encdec.chan, MX828_DCS2, &byte1, &byte2 );
		rad->encdec.state = 3;
		break;
	    case 3:
		byte1 = dcstable[rad->encdec.txcode[rad->encdec.chan] & 0x7fff].b3;
		mx828_command(rad,rad->encdec.chan, MX828_DCS3, &byte1, &byte2 );
		rad->encdec.state = 4;
		break;
	    case 4:
		if (rad->encdec.cttx[rad->encdec.chan])
		{
			rad->encdec.saudio_ctrl[rad->encdec.chan] |= 0x80;
			byte1 = cttable_tx[rad->encdec.txcode[rad->encdec.chan]].b1;
			byte2 = cttable_tx[rad->encdec.txcode[rad->encdec.chan]].b2;
			mx828_command(rad,rad->encdec.chan, MX828_TX_TONE, &byte1, &byte2 );
		} 
		rad->encdec.state = 5;
		break;
	    case 5:
		if (rad->encdec.dcsrx[rad->encdec.chan])
		{
			rad->encdec.saudio_setup[rad->encdec.chan] |= 1;
			rad->encdec.saudio_ctrl[rad->encdec.chan] |= 0x41;
			byte1 = dcstable[rad->rxcode[rad->encdec.chan][0]].b1;
			mx828_command(rad,rad->encdec.chan, MX828_DCS1, &byte1, &byte2 );
			rad->encdec.state = 6;
			break;
		}
		rad->encdec.state = 8;
		break;
	    case 6:
		byte1 = dcstable[rad->rxcode[rad->encdec.chan][0]].b2;
		mx828_command(rad,rad->encdec.chan, MX828_DCS2, &byte1, &byte2 );
		rad->encdec.state = 7;
		break;
	    case 7:
		byte1 = dcstable[rad->rxcode[rad->encdec.chan][0]].b3;
		mx828_command(rad,rad->encdec.chan, MX828_DCS3, &byte1, &byte2 );
		rad->encdec.state = 8;
		break;
	    case 8:
		if (rad->encdec.ctrx[rad->encdec.chan])
		{
			rad->encdec.saudio_setup[rad->encdec.chan] |= 0x80;
			rad->encdec.saudio_ctrl[rad->encdec.chan] |= 0x60;
		}
		byte1 = rad->encdec.saudio_setup[rad->encdec.chan];
		mx828_command(rad,rad->encdec.chan, MX828_SAUDIO_SETUP, &byte1, &byte2 );
		rad->encdec.state = 9;
		break;
	    case 9:
		byte1 = rad->encdec.saudio_ctrl[rad->encdec.chan];
		mx828_command(rad,rad->encdec.chan, MX828_SAUDIO_CTRL, &byte1, &byte2 );
		rad->encdec.state = 10;
		break;
	    case 10:
		rad->encdec.chan = 0;
		rad->encdec.state = 0;
		break;
	}
}

static inline void pciradio_transmitprep(struct pciradio *rad, unsigned char ints)
{
	volatile unsigned int *writechunk;
	int x;
	if (ints & 0x01) 
		/* Write is at interrupt address.  Start writing from normal offset */
		writechunk = rad->writechunk;
	else 
		writechunk = rad->writechunk + DAHDI_CHUNKSIZE;

	/* Calculate Transmission */
	dahdi_transmit(&rad->span);

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Send a sample, as a 32-bit word */
		writechunk[x] = 0;
		writechunk[x] |= (rad->chans[0].writechunk[x] << 24);
		writechunk[x] |= (rad->chans[1].writechunk[x] << 16);
		writechunk[x] |= (rad->chans[2].writechunk[x] << 8);
		writechunk[x] |= (rad->chans[3].writechunk[x]);
	}
}

static inline void pciradio_receiveprep(struct pciradio *rad, unsigned char ints)
{
	volatile unsigned int *readchunk;
	int x;

	if (ints & 0x08)
		readchunk = rad->readchunk + DAHDI_CHUNKSIZE;
	else
		/* Read is at interrupt address.  Valid data is available at normal offset */
		readchunk = rad->readchunk;
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		rad->chans[0].readchunk[x] = (readchunk[x] >> 24) & 0xff;
		rad->chans[1].readchunk[x] = (readchunk[x] >> 16) & 0xff;
		rad->chans[2].readchunk[x] = (readchunk[x] >> 8) & 0xff;
		rad->chans[3].readchunk[x] = (readchunk[x]) & 0xff;
	}
	for (x=0;x<rad->nchans;x++) {
		dahdi_ec_chunk(&rad->chans[x], rad->chans[x].readchunk, rad->chans[x].writechunk);
	}
	dahdi_receive(&rad->span);
}

static void pciradio_stop_dma(struct pciradio *rad);
static void pciradio_reset_serial(struct pciradio *rad);
static void pciradio_restart_dma(struct pciradio *rad);

#ifdef	LEAVE_THIS_COMMENTED_OUT
static irqreturn_t pciradio_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#endif

DAHDI_IRQ_HANDLER(pciradio_interrupt)
{
	struct pciradio *rad = dev_id;
	unsigned char ints,byte1,byte2,gotcor,gotctcss,gotslowctcss,ctcss;
	int i,x,gotrx;

	ints = inb(rad->ioaddr + RAD_INTSTAT);
	outb(ints, rad->ioaddr + RAD_INTSTAT);

	if (!ints)
		return IRQ_NONE;

	if (ints & 0x10) {
		/* Stop DMA, wait for watchdog */
		printk(KERN_INFO "RADIO PCI Master abort\n");
		pciradio_stop_dma(rad);
		return IRQ_RETVAL(1);
	}
	
	if (ints & 0x20) {
		printk(KERN_INFO "RADIO PCI Target abort\n");
		return IRQ_RETVAL(1);
	}

	if (ints & 0x0f) {

		rad->intcount++;
		x = rad->intcount % rad->nchans;
		/* freeze */
		__pciradio_setcreg(rad,0,rad->mx828_addr | 4);
		/* read SAUDIO_STATUS for the proper channel */
		byte1 = rad->saudio_status[x] = __pciradio_getcreg(rad,x);
		/* thaw */
		__pciradio_setcreg(rad,0,rad->mx828_addr);
		/* get COR input */
		byte2 = __pciradio_getcreg(rad,9);
		/* get bit for this channel */
		gotcor = byte2 & (1 << x);
		if (rad->radmode[x] & RADMODE_INVERTCOR) gotcor = !gotcor;
		rad->gotcor[x] = gotcor;
		if (rad->radmode[x] & RADMODE_IGNORECOR) gotcor = 1;
		gotslowctcss = 0;
		if ((byte1 & RAD_CTCSSVALID) && 
			((byte1 & RAD_CTCSSMASK) != RAD_CTCSSOTHER)) gotslowctcss = 1;
		gotctcss = 1;
		ctcss = 0;
		/* set ctcss to 1 if decoding ctcss */
		if (!rad->rxcode[x][0])
		{
			for(i = 1; i <= NUM_CODES; i++)
			{
				if (rad->rxcode[x][i])
				{
					ctcss = 1;
					break;
				}
			}
		}
		if (ctcss)
		{
			if ((!(byte1 & 0x40)) || 
				((!rad->gotrx[x]) && (!gotslowctcss))) gotctcss = 0; 
		}
		rad->present_code[x] = 0;
		if (rad->rxcode[x][0]) 
		{
			if (byte1 & 0x80) gotctcss = gotslowctcss = 1; else gotctcss = 0;
		} else if (gotslowctcss) rad->present_code[x] = (byte1 & RAD_CTCSSMASK) + 1;
		if (rad->radmode[x] & RADMODE_EXTTONE)
		{
			unsigned mask = 1 << (x + 4); /* they're on the UIOB's */
			unsigned char byteuio;

			/* set UIOB as input */
			byteuio = __pciradio_getcreg(rad,0xe);
			byteuio |= mask;
			__pciradio_setcreg(rad,0xe,byteuio);
			/* get UIO input */
			byteuio = __pciradio_getcreg(rad,8);
			if (rad->radmode[x] & RADMODE_EXTINVERT)
				gotctcss = gotslowctcss = ((byteuio & mask) == 0);
			else
				gotctcss = gotslowctcss = ((byteuio & mask) != 0);
		}
		rad->gotct[x] = gotslowctcss;
		if ((rad->radmode[x] & RADMODE_IGNORECT) || 
		   ((!(rad->radmode[x] & RADMODE_EXTTONE)) && (!ctcss))) 
		{
			gotctcss = 1;
			gotslowctcss = 1;
			rad->present_code[x] = 0;
		}
		if(rad->newctcssstate[x] != gotctcss){
			rad->newctcssstate[x] = gotctcss;
			if(rad->newctcssstate[x])
				rad->ctcsstimer[x]=rad->ctcssacquiretime[x];
			else
				rad->ctcsstimer[x]=rad->ctcsstalkofftime[x];
		}
		else{
			 if(!rad->ctcsstimer[x])
				rad->ctcssstate[x] = rad->newctcssstate[x];
			else
				rad->ctcsstimer[x]--;
		}
		gotrx = gotcor && rad->ctcssstate[x];
		if (gotrx != rad->gotrx[x])
		{
			rad->gotrxtimer[x] = rad->debouncetime[x];
		}
		rad->gotrx[x] = gotrx;
		if (rad->present_code[x] != rad->last_code[x])
		{
			rad->encdec.req[x] = 1;
			rad->last_code[x] = rad->present_code[x];
		}
		_do_encdec(rad);
		for(x = 0; x < rad->nchans; x++)
		{
			unsigned char mask = 1 << x;

			if (rad->gottx[x] != rad->lasttx[x])
			{
				if (rad->gottx[x])
				{
					rad->bursttimer[x] = 0;
					rad->pasave |= mask;
					__pciradio_setcreg(rad, 0xa, rad->pasave);
				}
				else
				{
					if (!rad->bursttime[x])
					{
						rad->pasave &= ~mask;
						__pciradio_setcreg(rad, 0xa, rad->pasave);
					}
					else
					{
						rad->bursttimer[x] = rad->bursttime[x];
					}
				}
				rad->encdec.req[x] = 1;
				rad->lasttx[x] = rad->gottx[x];
			}
			if (rad->bursttimer[x])
			{
				/* if just getting to zero */
				if (!(--rad->bursttimer[x]))
				{
					rad->pasave &= ~mask;
					__pciradio_setcreg(rad, 0xa, rad->pasave);
				}
			}

			/* if timer active */
			if (rad->gotrxtimer[x])
			{
				/* if just getting to zero */
				if (!(--rad->gotrxtimer[x]))
				{
					mask = 1 << (x + 4);
					rad->pasave &= ~mask;
					if (gotctcss) rad->pasave |= mask;
					__pciradio_setcreg(rad, 0xa, rad->pasave);

					if (rad->gotrx[x] != rad->gotrx1[x])
					{
						if (rad->gotrx[x]) {
						    if (debug)
							{
								if (rad->present_code[x])
								    printk(KERN_DEBUG "Chan %d got rx (ctcss code %d)\n",x + 1,
									cttable_rx[rad->rxcode[x][rad->present_code[x]]].code);
								else
								    printk(KERN_DEBUG "Chan %d got rx\n",x + 1);
							}
						    dahdi_hooksig(&rad->chans[x],DAHDI_RXSIG_OFFHOOK);
						} else {
						    if (debug) printk(KERN_DEBUG "Chan %d lost rx\n",x + 1);
						    dahdi_hooksig(&rad->chans[x],DAHDI_RXSIG_ONHOOK);
						}
						rad->encdec.req[x] = 1; 
					}
					rad->gotrx1[x] = rad->gotrx[x];
				}
			}
		}
		/* process serial if any */
		/* send byte if there is one in buffer to send */
		if (rad->txlen && (rad->txlen != rad->txindex))
		{
			/* if tx not busy */
			if (!(__pciradio_getcreg(rad,9) & 0x80))
			{
				__pciradio_setcreg(rad, 4, rad->txbuf[rad->txindex++]);
			}				
		}
		rad->srxtimer++;
		/* if something in rx to read */
		while(__pciradio_getcreg(rad,9) & 0x10)
		{
			unsigned char c = __pciradio_getcreg(rad,4);
			rad->srxtimer = 0;
			if (rad->rxindex < RAD_SERIAL_BUFLEN)
			{
				rad->rxbuf[rad->rxindex++] = c;
			}
			udelay(1);
		}
		pciradio_receiveprep(rad, ints);
		pciradio_transmitprep(rad, ints);
		i = 0;
		for(x = 0; x < 4; x++)
		{
			if (rad->gottx[x]) i |= (1 << (x * 2));
			if (rad->gotrx[x]) i |= (2 << (x * 2));
		}  
		/* output LED's */
		__pciradio_setcreg(rad, 9, i);
	}

	return IRQ_RETVAL(1);
}

static int pciradio_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	int i,mycode;
	unsigned long flags;
	unsigned char byte1,byte2,mask;
	union {
		struct dahdi_radio_stat s;
		struct dahdi_radio_param p;
	} stack;

	struct pciradio *rad = chan->pvt;
	DECLARE_WAIT_QUEUE_HEAD(mywait);

	switch (cmd) {
	case DAHDI_RADIO_GETPARAM:
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p))) return -EFAULT;
		spin_lock_irqsave(&rad->lock,flags);
		stack.p.data = 0; /* start with 0 value in output */
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (rad->radmode[chan->chanpos - 1] & RADMODE_INVERTCOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECOR:
			if (rad->radmode[chan->chanpos - 1] & RADMODE_IGNORECOR)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_IGNORECT:
			if (rad->radmode[chan->chanpos - 1] & RADMODE_IGNORECT)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_NOENCODE:
			if (rad->radmode[chan->chanpos - 1] & RADMODE_NOENCODE)
				stack.p.data = 1;
			break;
		case DAHDI_RADPAR_CORTHRESH:
			stack.p.data = rad->corthresh[chan->chanpos - 1] & 7;
			break;
		case DAHDI_RADPAR_EXTRXTONE:
			if (rad->radmode[chan->chanpos - 1] & RADMODE_EXTTONE)
			{
				stack.p.data = 1;
				if (rad->radmode[chan->chanpos - 1] & RADMODE_EXTINVERT)
				{
					stack.p.data = 2;
				}
			}
			break;
		case DAHDI_RADPAR_NUMTONES:
			stack.p.data = NUM_CODES;
			break;
		case DAHDI_RADPAR_RXTONE:
			if ((stack.p.index < 1) || (stack.p.index > NUM_CODES)) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			stack.p.data = 
				cttable_rx[rad->rxcode[chan->chanpos - 1][stack.p.index] & 0x7fff].code;
			break;
		case DAHDI_RADPAR_RXTONECLASS:
			if ((stack.p.index < 1) || (stack.p.index > NUM_CODES)) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			stack.p.data = rad->rxclass[chan->chanpos - 1][stack.p.index] & 0xffff;
			break;
		case DAHDI_RADPAR_TXTONE:
			if (stack.p.index > NUM_CODES) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			stack.p.data = cttable_tx[rad->txcode[chan->chanpos - 1][stack.p.index] & 0x7fff].code;
			/* if a DCS tone, return as such */
			if (rad->txcode[chan->chanpos - 1][stack.p.index] & 0x8000)
				stack.p.data |= 0x8000;
			break;
		case DAHDI_RADPAR_DEBOUNCETIME:
			stack.p.data = rad->debouncetime[chan->chanpos - 1];
			break;

		case DAHDI_RADPAR_CTCSSACQUIRETIME:
			stack.p.data = rad->ctcssacquiretime[chan->chanpos - 1];
			break;

		case DAHDI_RADPAR_CTCSSTALKOFFTIME:
			stack.p.data = rad->ctcsstalkofftime[chan->chanpos - 1];
			break;

		case DAHDI_RADPAR_BURSTTIME:
			stack.p.data = rad->bursttime[chan->chanpos - 1];
			break;
		case DAHDI_RADPAR_UIODATA:
			stack.p.data = 0;
			byte1 = __pciradio_getcreg(rad,8);
			if (byte1 & (1 << (chan->chanpos - 1))) stack.p.data |= 1;
			if (byte1 & (1 << (chan->chanpos + 3))) stack.p.data |= 2;
			break;
		case DAHDI_RADPAR_UIOMODE:
			stack.p.data = 0;
			byte1 = __pciradio_getcreg(rad,0xe);
			if (byte1 & (1 << (chan->chanpos - 1))) stack.p.data |= 1;
			if (byte1 & (1 << (chan->chanpos + 3))) stack.p.data |= 2;
			break;
		case DAHDI_RADPAR_REMMODE:
			stack.p.data = rad->remmode[chan->chanpos - 1];
			break;
		default:
			spin_unlock_irqrestore(&rad->lock,flags);
			return -EINVAL;
		}
		spin_unlock_irqrestore(&rad->lock,flags);
		if (copy_to_user((__user void *) data, &stack.p, sizeof(stack.p))) return -EFAULT;
		break;
	case DAHDI_RADIO_SETPARAM:
		if (copy_from_user(&stack.p, (__user void *) data, sizeof(stack.p))) return -EFAULT;
		spin_lock_irqsave(&rad->lock,flags);
		switch(stack.p.radpar) {
		case DAHDI_RADPAR_INVERTCOR:
			if (stack.p.data)
				rad->radmode[chan->chanpos - 1] |= RADMODE_INVERTCOR;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_INVERTCOR;
			break;
		case DAHDI_RADPAR_IGNORECOR:
			if (stack.p.data)
				rad->radmode[chan->chanpos - 1] |= RADMODE_IGNORECOR;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_IGNORECOR;
			break;
		case DAHDI_RADPAR_IGNORECT:
			if (stack.p.data)
				rad->radmode[chan->chanpos - 1] |= RADMODE_IGNORECT;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_IGNORECT;
			break;
		case DAHDI_RADPAR_NOENCODE:
			if (stack.p.data)
				rad->radmode[chan->chanpos - 1] |= RADMODE_NOENCODE;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_NOENCODE;
			break;
		case DAHDI_RADPAR_CORTHRESH:
			if ((stack.p.data < 0) || (stack.p.data > 7)) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			rad->corthresh[chan->chanpos - 1] = stack.p.data;
			byte1 = 0xc0 | (rad->corthresh[chan->chanpos - 1] << 2);
			spin_unlock_irqrestore(&rad->lock,flags);
			mx828_command_wait(rad,chan->chanpos - 1, MX828_GEN_CTRL, &byte1, &byte2);
			spin_lock_irqsave(&rad->lock,flags);
			break;
		case DAHDI_RADPAR_EXTRXTONE:
			if (stack.p.data)
				rad->radmode[chan->chanpos - 1] |= RADMODE_EXTTONE;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_EXTTONE;
			if (stack.p.data > 1)
				rad->radmode[chan->chanpos - 1] |= RADMODE_EXTINVERT;
			else
				rad->radmode[chan->chanpos - 1] &= ~RADMODE_EXTINVERT;
			break;
		case DAHDI_RADPAR_INITTONE:
			for(i = 0; i <= NUM_CODES; i++)
			{
				rad->rxcode[chan->chanpos - 1][i] = 0;
				rad->rxclass[chan->chanpos - 1][i] = 0;
				rad->txcode[chan->chanpos - 1][i] = 0;
			}
			spin_unlock_irqrestore(&rad->lock,flags);
			for(i = 0; i < NUM_CODES; i++)
			{
				/* set to no encode/decode */
				byte1 = 0;
				mx828_command_wait(rad,chan->chanpos - 1, MX828_SAUDIO_CTRL, &byte1, &byte2 );
				/* set rx tone to none */
				byte1 = i << 4;
				byte2 = 0;
				mx828_command_wait(rad,chan->chanpos - 1, MX828_RX_TONE, &byte1, &byte2 );
			}
			spin_lock_irqsave(&rad->lock,flags);
			break;
		case DAHDI_RADPAR_RXTONE:
			if (!stack.p.index) /* if RX DCS mode */
			{
				if ((stack.p.data < 0) || (stack.p.data > 777)) {
					spin_unlock_irqrestore(&rad->lock,flags);
					return -EINVAL;
				}
				mycode = getdcstone(stack.p.data);
				if (mycode < 0) {
					spin_unlock_irqrestore(&rad->lock,flags);
					return -EINVAL;
				}
				rad->rxcode[chan->chanpos - 1][0] = mycode;
				rad->encdec.req[chan->chanpos - 1] = 1;
				break;
			}
			if ((stack.p.index < 1) || (stack.p.index > NUM_CODES)) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			mycode = getrxtone(stack.p.data);
			if (mycode < 0) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			rad->rxcode[chan->chanpos - 1][stack.p.index] = mycode;
			byte1 = cttable_rx[mycode].b1 | ((stack.p.index - 1) << 4);
			byte2 = cttable_rx[mycode].b2;
			spin_unlock_irqrestore(&rad->lock,flags);
			mx828_command_wait(rad,chan->chanpos - 1, MX828_RX_TONE, &byte1, &byte2 );
			spin_lock_irqsave(&rad->lock,flags);
			/* zot out DCS one if there */
			rad->rxcode[chan->chanpos - 1][0] = 0;
			rad->encdec.req[chan->chanpos - 1] = 1;
			break;
		case DAHDI_RADPAR_RXTONECLASS:
			if ((stack.p.index < 1) || (stack.p.index > NUM_CODES)) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			rad->rxclass[chan->chanpos - 1][stack.p.index] = stack.p.data & 0xffff;
			break;
		case DAHDI_RADPAR_TXTONE:
			if (stack.p.index > NUM_CODES) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			if (stack.p.data & 0x8000) /* if dcs */
				mycode = getdcstone(stack.p.data & 0x7fff);
			else
				mycode = gettxtone(stack.p.data);
			if (mycode < 0) {
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			if (stack.p.data & 0x8000) mycode |= 0x8000;
			rad->txcode[chan->chanpos - 1][stack.p.index] = mycode;
			rad->encdec.req[chan->chanpos - 1] = 1;
			break;
		case DAHDI_RADPAR_DEBOUNCETIME:
			rad->debouncetime[chan->chanpos - 1] = stack.p.data;
			break;

		case DAHDI_RADPAR_CTCSSACQUIRETIME:
			rad->ctcssacquiretime[chan->chanpos - 1] = stack.p.data;
			break;

		case DAHDI_RADPAR_CTCSSTALKOFFTIME:
			rad->ctcsstalkofftime[chan->chanpos - 1] = stack.p.data;
			break;

		case DAHDI_RADPAR_BURSTTIME:
			rad->bursttime[chan->chanpos - 1] = stack.p.data;
			break;
		case DAHDI_RADPAR_UIODATA:
			byte1 = __pciradio_getcreg(rad,8);
			byte1 &= ~(1 << (chan->chanpos - 1));
			byte1 &= ~(1 << (chan->chanpos + 3));
			if (stack.p.data & 1) byte1 |= (1 << (chan->chanpos - 1));
			if (stack.p.data & 2) byte1 |= (1 << (chan->chanpos + 3));
			__pciradio_setcreg(rad,8,byte1);
			break;
		case DAHDI_RADPAR_UIOMODE:
			byte1 = __pciradio_getcreg(rad,0xe);
			byte1 &= ~(1 << (chan->chanpos - 1));
			byte1 &= ~(1 << (chan->chanpos + 3));
			if (stack.p.data & 1) byte1 |= (1 << (chan->chanpos - 1));
			if (stack.p.data & 2) byte1 |= (1 << (chan->chanpos + 3));
			__pciradio_setcreg(rad,0xe,byte1);
			break;
		case DAHDI_RADPAR_REMMODE:
			rad->remmode[chan->chanpos - 1] = stack.p.data;
			break;
		case DAHDI_RADPAR_REMCOMMAND:
			/* if no remote mode, return an error */
			if (rad->remmode[chan->chanpos - 1] == DAHDI_RADPAR_REM_NONE)
			{
				spin_unlock_irqrestore(&rad->lock,flags);
				return -EINVAL;
			}
			i = 0;
			if (rad->remmode[chan->chanpos - 1] == DAHDI_RADPAR_REM_RBI1)
			{
				/* set UIOA and UIOB for output */
				byte1 = __pciradio_getcreg(rad,0xe);
				mask = (1 << (chan->chanpos - 1)) | 
					(1 << (chan->chanpos + 3));
				byte2 = byte1 & (~mask);
				i = (byte2 != byte1);
				__pciradio_setcreg(rad,0xe,byte2);
				byte1 = __pciradio_getcreg(rad,8);
				mask = 1 << (chan->chanpos - 1);
				byte2 = byte1 | mask;
				i = (byte2 != byte1);
				__pciradio_setcreg(rad,8,byte2);
				spin_unlock_irqrestore(&rad->lock,flags);
				if (i || (jiffies < rad->lastremcmd + 10))
					interruptible_sleep_on_timeout(&mywait,10);
				rad->lastremcmd = jiffies;
				rbi_out(rad,chan->chanpos - 1,(unsigned char *)&stack.p.data);
				spin_lock_irqsave(&rad->lock,flags);
				break;
			}
			spin_unlock_irqrestore(&rad->lock,flags);
			for(;;)
			{
				int x;

				spin_lock_irqsave(&rad->lock,flags);
				x = rad->remote_locked || (__pciradio_getcreg(rad,0xc) & 2);
				if (!x) rad->remote_locked = 1;
				spin_unlock_irqrestore(&rad->lock,flags);
				if (x) interruptible_sleep_on_timeout(&mywait,2);
				else break;
			}	
			spin_lock_irqsave(&rad->lock,flags);
			/* set UIOA for input and UIOB for output */
			byte1 = __pciradio_getcreg(rad,0xe);
			mask = 1 << (chan->chanpos + 3); /* B an output */
			byte2 = byte1 & (~mask);
			byte2 |= 1 << (chan->chanpos - 1); /* A in input */
			__pciradio_setcreg(rad,0xe,byte2);
			byte1 = __pciradio_getcreg(rad,8);
			byte2 = byte1 | mask;
			byte2 |= 1 << (chan->chanpos - 1);
			byte2 |= 1 << (chan->chanpos + 3);
			__pciradio_setcreg(rad,8,byte2);
			spin_unlock_irqrestore(&rad->lock,flags);
			if (byte1 != byte2) 
				interruptible_sleep_on_timeout(&mywait,3);
			while (jiffies < rad->lastremcmd + 10)
				interruptible_sleep_on_timeout(&mywait,10);
			rad->lastremcmd = jiffies;
			for(;;)
			{
				if (!(__pciradio_getcreg(rad,0xc) & 2)) break;
 				interruptible_sleep_on_timeout(&mywait,2);
			}
			spin_lock_irqsave(&rad->lock,flags);
			/* enable and address async serializer */
			__pciradio_setcreg(rad,0xf,rad->pfsave | ((chan->chanpos - 1) << 4) | 0x80);
			/* copy tx buffer */
			memcpy(rad->txbuf,stack.p.buf,stack.p.index);
			rad->txlen = stack.p.index;
			rad->txindex = 0;
			rad->rxindex = 0;
			rad->srxtimer = 0;
			memset(stack.p.buf,0,SERIAL_BUFLEN);
			stack.p.index = 0;
			if (stack.p.data) for(;;)
			{			
				rad->rxbuf[rad->rxindex] = 0;
				if ((rad->rxindex < stack.p.data) &&
				  (rad->srxtimer < SRX_TIMEOUT) &&
				    ((rad->remmode[chan->chanpos - 1] == DAHDI_RADPAR_REM_SERIAL) ||
					(!strchr((char *)rad->rxbuf,'\r'))))
				{
					spin_unlock_irqrestore(&rad->lock,flags);
					interruptible_sleep_on_timeout(&mywait,2);
					spin_lock_irqsave(&rad->lock,flags);
					continue;
				}
				memset(stack.p.buf,0,SERIAL_BUFLEN);
				if (stack.p.data && (rad->rxindex > stack.p.data))
					rad->rxindex = stack.p.data;
				if (rad->rxindex)
					memcpy(stack.p.buf,rad->rxbuf,rad->rxindex);
				stack.p.index = rad->rxindex;
				break;
			}
			/* wait for done if in SERIAL_ASCII mode, or if no Rx aftwards */
			if ((rad->remmode[chan->chanpos - 1] == DAHDI_RADPAR_REM_SERIAL_ASCII) ||
				(!stack.p.data))
			{			
				/* wait for TX to be done if not already */
				while(rad->txlen && (rad->txindex < rad->txlen))
				{
					spin_unlock_irqrestore(&rad->lock,flags);
					interruptible_sleep_on_timeout(&mywait,2);
					spin_lock_irqsave(&rad->lock,flags);
				}
				/* disable and un-address async serializer */
				__pciradio_setcreg(rad,0xf,rad->pfsave); 
			}
			rad->remote_locked = 0;
			spin_unlock_irqrestore(&rad->lock,flags);
			if (rad->remmode[chan->chanpos - 1] == DAHDI_RADPAR_REM_SERIAL_ASCII)
				interruptible_sleep_on_timeout(&mywait,100);
			if (copy_to_user((__user void *) data, &stack.p, sizeof(stack.p))) return -EFAULT;
			return 0;
		default:
			spin_unlock_irqrestore(&rad->lock,flags);
			return -EINVAL;
		}
		spin_unlock_irqrestore(&rad->lock,flags);
		break;
	case DAHDI_RADIO_GETSTAT:
		spin_lock_irqsave(&rad->lock,flags);
		/* start with clean object */
		memset(&stack.s, 0, sizeof(stack.s));
		/* if we have rx */
		if (rad->gotrx[chan->chanpos - 1])
		{
			stack.s.radstat |= DAHDI_RADSTAT_RX;
			if (rad->rxcode[chan->chanpos - 1][0])
			    stack.s.ctcode_rx = 
				dcstable[rad->rxcode[chan->chanpos - 1][0]].code | 0x8000;
			else {
				stack.s.ctcode_rx = 
				    cttable_rx[rad->rxcode[chan->chanpos - 1][rad->present_code[chan->chanpos - 1]]].code;
				stack.s.ctclass = 
				    rad->rxclass[chan->chanpos - 1][rad->present_code[chan->chanpos - 1]];
			}
		}
		/* if we have tx */
		if (rad->gottx[chan->chanpos - 1])
		{
			unsigned short x,myindex;

			stack.s.radstat |= DAHDI_RADSTAT_TX;
			stack.s.radstat |= DAHDI_RADSTAT_TX;

			myindex = 0;
			if ((!rad->rxcode[chan->chanpos - 1][0]) 
				&& (rad->present_code[chan->chanpos - 1])) 
					myindex = rad->present_code[chan->chanpos - 1];
			x = rad->txcode[chan->chanpos - 1][myindex];
			if (x & 0x8000)
				stack.s.ctcode_tx = dcstable[x & 0x7fff].code | 0x8000;
			else
				stack.s.ctcode_tx = cttable_tx[x].code;
			
		}

		if (rad->radmode[chan->chanpos - 1] & RADMODE_IGNORECOR)
			stack.s.radstat |= DAHDI_RADSTAT_IGNCOR;
		if (rad->radmode[chan->chanpos - 1] & RADMODE_IGNORECT)
			stack.s.radstat |= DAHDI_RADSTAT_IGNCT;
		if (rad->radmode[chan->chanpos - 1] & RADMODE_NOENCODE)
			stack.s.radstat |= DAHDI_RADSTAT_NOENCODE;
		if (rad->gotcor[chan->chanpos - 1])
			stack.s.radstat |= DAHDI_RADSTAT_RXCOR;
		if (rad->gotct[chan->chanpos - 1])
			stack.s.radstat |= DAHDI_RADSTAT_RXCT;
		spin_unlock_irqrestore(&rad->lock,flags);
		if (copy_to_user((__user void *) data, &stack.s, sizeof(stack.s))) return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}
	return 0;

}

static int pciradio_open(struct dahdi_chan *chan)
{
	struct pciradio *rad = chan->pvt;
	if (rad->dead)
		return -ENODEV;
	rad->usecount++;
	return 0;
}

static int pciradio_watchdog(struct dahdi_span *span, int event)
{
	printk(KERN_INFO "PCI RADIO: Restarting DMA\n");
	pciradio_restart_dma(container_of(span, struct pciradio, span));
	return 0;
}

static int pciradio_close(struct dahdi_chan *chan)
{
	struct pciradio *rad = chan->pvt;
	rad->usecount--;
	/* If we're dead, release us now */
	if (!rad->usecount && rad->dead) 
		pciradio_release(rad);
	return 0;
}

static int pciradio_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct pciradio *rad = chan->pvt;

	switch(txsig) {
	case DAHDI_TXSIG_START:
	case DAHDI_TXSIG_OFFHOOK:
		rad->gottx[chan->chanpos - 1] = 1;
		break;
	case DAHDI_TXSIG_ONHOOK:
		rad->gottx[chan->chanpos - 1] = 0;
		break;
	default:
		printk(KERN_DEBUG "pciradio: Can't set tx state to %d\n", txsig);
		break;
	}
	if (debug) 
		printk(KERN_DEBUG "pciradio: Setting Radio hook state to %d on chan %d\n", txsig, chan->chanpos);
	return 0;
}

static const struct dahdi_span_ops pciradio_span_ops = {
	.owner = THIS_MODULE,
	.hooksig = pciradio_hooksig,
	.open = pciradio_open,
	.close = pciradio_close,
	.ioctl = pciradio_ioctl,
	.watchdog = pciradio_watchdog,
};

static int pciradio_initialize(struct pciradio *rad)
{
	int x;

	/* DAHDI stuff */
	sprintf(rad->span.name, "PCIRADIO/%d", rad->pos);
	sprintf(rad->span.desc, "Board %d", rad->pos + 1);
	rad->span.deflaw = DAHDI_LAW_MULAW;
	for (x=0;x<rad->nchans;x++) {
		sprintf(rad->chans[x].name, "PCIRADIO/%d/%d", rad->pos, x);
		rad->chans[x].sigcap = DAHDI_SIG_SF | DAHDI_SIG_EM;
		rad->chans[x].chanpos = x+1;
		rad->chans[x].pvt = rad;
		rad->debouncetime[x] = RAD_GOTRX_DEBOUNCE_TIME;
		rad->ctcssacquiretime[x] = RAD_CTCSS_ACQUIRE_TIME;
		rad->ctcsstalkofftime[x] = RAD_CTCSS_TALKOFF_TIME;
	}
	rad->span.chans = &rad->chans;
	rad->span.channels = rad->nchans;
	rad->span.flags = DAHDI_FLAG_RBS;
	rad->span.ops = &pciradio_span_ops;

	if (dahdi_register_device(rad->ddev, &rad->dev->dev)) {
		printk(KERN_NOTICE "Unable to register span with DAHDI\n");
		return -1;
	}
	return 0;
}

static void wait_just_a_bit(int foo)
{
	long newjiffies;
	newjiffies = jiffies + foo;
	while(jiffies < newjiffies);
}

static int pciradio_hardware_init(struct pciradio *rad)
{
unsigned char byte1,byte2;
int	x;
unsigned long endjif;

	/* Signal Reset */
	outb(0x01, rad->ioaddr + RAD_CNTL);

	/* Reset PCI Interface chip and registers (and serial) */
	outb(0x06, rad->ioaddr + RAD_CNTL);
	/* Setup our proper outputs */
	rad->ios = 0xfe;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);

	/* Set all to outputs except AUX 3 & 4, which are inputs */
	outb(0x67, rad->ioaddr + RAD_AUXC);

	/* Select alternate function for AUX0 */
	outb(0x4, rad->ioaddr + RAD_AUXFUNC);
	
	/* Wait 1/4 of a sec */
	wait_just_a_bit(HZ/4);

	/* attempt to load the Xilinx Chip */
	/* De-assert CS+Write */
	rad->ios |= XCS;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);
	/* Assert PGM */
	rad->ios &= ~XPGM;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);
	/* wait for INIT and DONE to go low */
	endjif = jiffies + 10;
	while (inb(rad->ioaddr + RAD_AUXR) & (XINIT | XDONE) && (jiffies <= endjif));
	if (endjif < jiffies) {
		printk(KERN_DEBUG "Timeout waiting for INIT and DONE to go low\n");
		return -1;
	}
	if (debug) printk(KERN_DEBUG "fwload: Init and done gone to low\n");
	/* De-assert PGM */
	rad->ios |= XPGM;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);
	/* wait for INIT to go high (clearing done */
	endjif = jiffies + 10;
	while (!(inb(rad->ioaddr + RAD_AUXR) & XINIT) && (jiffies <= endjif));
	if (endjif < jiffies) {
		printk(KERN_DEBUG "Timeout waiting for INIT to go high\n");
		return -1;
	}
	if (debug) printk(KERN_DEBUG "fwload: Init went high (clearing done)\nNow loading...\n");
	/* Assert CS+Write */
	rad->ios &= ~XCS;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);
	for (x = 0; x < sizeof(radfw); x++)
	   {
		  /* write the byte */
		outb(radfw[x],rad->ioaddr + RAD_REGBASE);
		  /* if DONE signal, we're done, exit */
		if (inb(rad->ioaddr + RAD_AUXR) & XDONE) break;
		  /* if INIT drops, we're screwed, exit */
		if (!(inb(rad->ioaddr + RAD_AUXR) & XINIT)) break;
	   }
	if (debug) printk(KERN_DEBUG "fwload: Transferred %d bytes into chip\n",x);
	/* Wait for FIFO to clear */
	endjif = jiffies + 2;
	while (jiffies < endjif); /* wait */
	printk(KERN_DEBUG "Transfered %d bytes into chip\n",x);
	/* De-assert CS+Write */
	rad->ios |= XCS;
	outb(rad->ios, rad->ioaddr + RAD_AUXD);
	if (debug) printk(KERN_INFO "fwload: Loading done!\n");	
	/* Wait for FIFO to clear */
	endjif = jiffies + 2;
	while (jiffies < endjif); /* wait */
	if (!(inb(rad->ioaddr + RAD_AUXR) & XINIT))
	   {
		printk(KERN_NOTICE "Drove Init low!! CRC Error!!!\n");
		return -1;
	   }
	if (!(inb(rad->ioaddr + RAD_AUXR) & XDONE))
	   {
		printk(KERN_INFO "Did not get DONE signal. Short file maybe??\n");
		return -1;
	   }
	wait_just_a_bit(2);
	/* get the thingy started */
	outb(0,rad->ioaddr + RAD_REGBASE);
	outb(0,rad->ioaddr + RAD_REGBASE);
	printk(KERN_INFO "Xilinx Chip successfully loaded, configured and started!!\n");

	wait_just_a_bit(HZ/4);


	/* Back to normal, with automatic DMA wrap around */
	outb(0x30 | 0x01, rad->ioaddr + RAD_CNTL);
	
	/* Configure serial port for MSB->LSB operation */
	outb(0xc1, rad->ioaddr + RAD_SERCTL); /* DEBUG set double dlck to 0 SR */

	rad->pasave = 0;
	__pciradio_setcreg(rad,0xa,rad->pasave);

	__pciradio_setcreg(rad,0xf,rad->pfsave);
	__pciradio_setcreg(rad,8,0xff);
	__pciradio_setcreg(rad,0xe,0xff);
	__pciradio_setcreg(rad,9,0);
	rad->pfsave = 0;

	/* Delay FSC by 0 so it's properly aligned */
	outb(/* 1 */ 0, rad->ioaddr + RAD_FSCDELAY);

	/* Setup DMA Addresses */
	outl(rad->writedma,                    rad->ioaddr + RAD_DMAWS);		/* Write start */
	outl(rad->writedma + DAHDI_CHUNKSIZE * 4 - 4, rad->ioaddr + RAD_DMAWI);		/* Middle (interrupt) */
	outl(rad->writedma + DAHDI_CHUNKSIZE * 8 - 4, rad->ioaddr + RAD_DMAWE);			/* End */
	
	outl(rad->readdma,                    	 rad->ioaddr + RAD_DMARS);	/* Read start */
	outl(rad->readdma + DAHDI_CHUNKSIZE * 4 - 4, 	 rad->ioaddr + RAD_DMARI);	/* Middle (interrupt) */
	outl(rad->readdma + DAHDI_CHUNKSIZE * 8 - 4, rad->ioaddr + RAD_DMARE);	/* End */
	
	/* Clear interrupts */
	outb(0xff, rad->ioaddr + RAD_INTSTAT);

	/* Wait 1/4 of a second more */
	wait_just_a_bit(HZ/4);

	for(x = 0; x < rad->nchans; x++)
	{
		mx828_command_wait(rad,x, MX828_GEN_RESET, &byte1, &byte2 );
		byte1 = 0x3f;
		byte2 = 0x3f;
		mx828_command_wait(rad,x, MX828_AUD_CTRL, &byte1, &byte2 );
		byte1 = 0;
		mx828_command_wait(rad,x, MX828_SAUDIO_SETUP, &byte1, &byte2 );
		byte1 = 0;
		mx828_command_wait(rad,x, MX828_SAUDIO_CTRL, &byte1, &byte2 );
		byte1 = 0xc8;  /* default COR thresh is 2 */
		mx828_command_wait(rad,x, MX828_GEN_CTRL, &byte1, &byte2);
		rad->corthresh[x] = 2;
	}
	/* Wait 1/4 of a sec */
	wait_just_a_bit(HZ/4);

	return 0;
}

static void pciradio_enable_interrupts(struct pciradio *rad)
{
	/* Enable interrupts (we care about all of them) */
	outb(0x3f, rad->ioaddr + RAD_MASK0);
	/* No external interrupts */
	outb(0x00, rad->ioaddr + RAD_MASK1);
}

static void pciradio_restart_dma(struct pciradio *rad)
{
	/* Reset Master and serial */
	outb(0x31, rad->ioaddr + RAD_CNTL);
	outb(0x01, rad->ioaddr + RAD_OPER);
}

static void pciradio_start_dma(struct pciradio *rad)
{
	/* Reset Master and serial */
	outb(0x3f, rad->ioaddr + RAD_CNTL);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	outb(0x31, rad->ioaddr + RAD_CNTL);
	outb(0x01, rad->ioaddr + RAD_OPER);
}

static void pciradio_stop_dma(struct pciradio *rad)
{
	outb(0x00, rad->ioaddr + RAD_OPER);
}

static void pciradio_reset_serial(struct pciradio *rad)
{
	/* Reset serial */
	outb(0x3f, rad->ioaddr + RAD_CNTL);
}

static void pciradio_disable_interrupts(struct pciradio *rad)	
{
	outb(0x00, rad->ioaddr + RAD_MASK0);
	outb(0x00, rad->ioaddr + RAD_MASK1);
}

static int __devinit pciradio_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int res;
	struct pciradio *rad;
	int x;
	static int initd_ifaces=0;
	
	if(initd_ifaces){
		memset((void *)ifaces,0,(sizeof(struct pciradio *))*RAD_MAX_IFACES);
		initd_ifaces=1;
	}
	for (x=0;x<RAD_MAX_IFACES;x++)
		if (!ifaces[x]) break;
	if (x >= RAD_MAX_IFACES) {
		printk(KERN_NOTICE "Too many interfaces\n");
		return -EIO;
	}
	
	if (pci_enable_device(pdev)) {
		res = -EIO;
	} else {
		rad = kmalloc(sizeof(struct pciradio), GFP_KERNEL);
		if (rad) {
			int i;

			ifaces[x] = rad;
			rad->chans = rad->_chans;
			memset(rad, 0, sizeof(struct pciradio));
			spin_lock_init(&rad->lock);
			rad->nchans = 4;
			rad->ioaddr = pci_resource_start(pdev, 0);
			rad->dev = pdev;
			rad->pos = x;
			for(i = 0; i < rad->nchans; i++) rad->lasttx[x] = rad->gotrx1[i] = -1;
			/* Keep track of whether we need to free the region */
			if (request_region(rad->ioaddr, 0xff, "pciradio")) 
				rad->freeregion = 1;

			/* Allocate enough memory for two zt chunks, receive and transmit.  Each sample uses
			   32 bits.  Allocate an extra set just for control too */
			rad->writechunk = pci_alloc_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, &rad->writedma);
			if (!rad->writechunk) {
				printk(KERN_NOTICE "pciradio: Unable to allocate DMA-able memory\n");
				if (rad->freeregion)
					release_region(rad->ioaddr, 0xff);
				return -ENOMEM;
			}

			rad->readchunk = rad->writechunk + DAHDI_MAX_CHUNKSIZE * 2;	/* in doublewords */
			rad->readdma = rad->writedma + DAHDI_MAX_CHUNKSIZE * 8;		/* in bytes */

			if (pciradio_initialize(rad)) {
				printk(KERN_INFO "pciradio: Unable to intialize\n");
				/* Set Reset Low */
				x=inb(rad->ioaddr + RAD_CNTL);
				outb((~0x1)&x, rad->ioaddr + RAD_CNTL);
				outb(x, rad->ioaddr + RAD_CNTL);
				__pciradio_setcreg(rad,8,0xff);
				__pciradio_setcreg(rad,0xe,0xff);
				/* Free Resources */
				free_irq(pdev->irq, rad);
				if (rad->freeregion)
					release_region(rad->ioaddr, 0xff);
				pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)rad->writechunk, rad->writedma);
				kfree(rad);
				return -EIO;
			}

			/* Enable bus mastering */
			pci_set_master(pdev);

			/* Keep track of which device we are */
			pci_set_drvdata(pdev, rad);

			if (pciradio_hardware_init(rad)) {
				/* Set Reset Low */
				x=inb(rad->ioaddr + RAD_CNTL);
				outb((~0x1)&x, rad->ioaddr + RAD_CNTL);
				outb(x, rad->ioaddr + RAD_CNTL);
				__pciradio_setcreg(rad,8,0xff);
				__pciradio_setcreg(rad,0xe,0xff);
				/* Free Resources */
				free_irq(pdev->irq, rad);
				if (rad->freeregion)
					release_region(rad->ioaddr, 0xff);
				pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)rad->writechunk, rad->writedma);
				pci_set_drvdata(pdev, NULL);
				dahdi_free_device(rad->ddev);
				kfree(rad);
				return -EIO;

			}

			if (request_irq(pdev->irq, pciradio_interrupt, DAHDI_IRQ_SHARED, "pciradio", rad)) {
				printk(KERN_NOTICE "pciradio: Unable to request IRQ %d\n", pdev->irq);
				if (rad->freeregion)
					release_region(rad->ioaddr, 0xff);
				pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)rad->writechunk, rad->writedma);
				pci_set_drvdata(pdev, NULL);
				kfree(rad);
				return -EIO;
			}

			/* Enable interrupts */
			pciradio_enable_interrupts(rad);
			/* Initialize Write/Buffers to all blank data */
			memset((void *)rad->writechunk,0,DAHDI_MAX_CHUNKSIZE * 2 * 2 * 4);

			/* Start DMA */
			pciradio_start_dma(rad);
			printk(KERN_INFO "Found a PCI Radio Card\n");
			res = 0;
		} else
			res = -ENOMEM;
	}
	return res;
}

static void pciradio_release(struct pciradio *rad)
{
	dahdi_unregister_device(rad->ddev);
	if (rad->freeregion)
		release_region(rad->ioaddr, 0xff);
	kfree(rad);
	printk(KERN_INFO "Freed a PCI RADIO card\n");
}

static void __devexit pciradio_remove_one(struct pci_dev *pdev)
{
	struct pciradio *rad = pci_get_drvdata(pdev);
	if (rad) {

		/* Stop any DMA */
		pciradio_stop_dma(rad);
		pciradio_reset_serial(rad);

		/* In case hardware is still there */
		pciradio_disable_interrupts(rad);
		
		/* Immediately free resources */
		pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)rad->writechunk, rad->writedma);
		free_irq(pdev->irq, rad);

		/* Reset PCI chip and registers */
		outb(0x3e, rad->ioaddr + RAD_CNTL); 

		/* Clear Reset Line */
		outb(0x3f, rad->ioaddr + RAD_CNTL); 

		__pciradio_setcreg(rad,8,0xff);
		__pciradio_setcreg(rad,0xe,0xff);

		/* Release span, possibly delayed */
		if (!rad->usecount)
			pciradio_release(rad);
		else
			rad->dead = 1;
	}
}

static DEFINE_PCI_DEVICE_TABLE(pciradio_pci_tbl) = {
	{ 0xe159, 0x0001, 0xe16b, PCI_ANY_ID, 0, 0, (unsigned long)"PCIRADIO" },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pciradio_pci_tbl);

static struct pci_driver pciradio_driver = {
	.name = "pciradio",
	.probe = pciradio_init_one,
	.remove = __devexit_p(pciradio_remove_one),
	.id_table = pciradio_pci_tbl,
};

static int __init pciradio_init(void)
{
	int res;

	res = dahdi_pci_module(&pciradio_driver);
	if (res)
		return -ENODEV;
	return 0;
}

static void __exit pciradio_cleanup(void)
{
	pci_unregister_driver(&pciradio_driver);
}

module_param(debug, int, 0600);

MODULE_DESCRIPTION("DAHDI Telephony PCI Radio Card Driver");
MODULE_AUTHOR("Jim Dixon <jim@lambdatel.com>");
MODULE_LICENSE("GPL v2");

module_init(pciradio_init);
module_exit(pciradio_cleanup);
