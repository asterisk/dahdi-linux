#ifndef	DAHDI_DEBUG_H
#define	DAHDI_DEBUG_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <dahdi/kernel.h>	/* for dahdi_* defs */

/* Debugging Macros */

#define	PRINTK(level, category, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: " fmt, #level, category, THIS_MODULE->name, ## __VA_ARGS__)

#define	XBUS_PRINTK(level, category, xbus, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: %s: " fmt, #level,	\
		category, THIS_MODULE->name, (xbus)->busname, ## __VA_ARGS__)

#define	XPD_PRINTK(level, category, xpd, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: %s/%s: " fmt, #level,	\
		category, THIS_MODULE->name, (xpd)->xbus->busname, (xpd)->xpdname, ## __VA_ARGS__)

#define	LINE_PRINTK(level, category, xpd, pos, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: %s/%s/%d: " fmt, #level,	\
		category, THIS_MODULE->name, (xpd)->xbus->busname, (xpd)->xpdname, (pos), ## __VA_ARGS__)

#define	PORT_PRINTK(level, category, xbus, unit, port, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: %s UNIT=%d PORT=%d: " fmt, #level,	\
		category, THIS_MODULE->name, (xbus)->busname, (unit), (port), ## __VA_ARGS__)

#define	DBG(bits, fmt, ...)	\
	((void)((debug & (DBG_ ## bits)) && PRINTK(DEBUG, "-" #bits, "%s: " fmt, __FUNCTION__, ## __VA_ARGS__)))
#define	INFO(fmt, ...)		PRINTK(INFO, "", fmt, ## __VA_ARGS__)
#define	NOTICE(fmt, ...)	PRINTK(NOTICE, "", fmt, ## __VA_ARGS__)
#define	WARNING(fmt, ...)	PRINTK(WARNING, "", fmt, ## __VA_ARGS__)
#define	ERR(fmt, ...)		PRINTK(ERR, "", fmt, ## __VA_ARGS__)

#define	XBUS_DBG(bits, xbus, fmt, ...)	\
			((void)((debug & (DBG_ ## bits)) && XBUS_PRINTK(DEBUG, "-" #bits, xbus, "%s: " fmt, __FUNCTION__, ## __VA_ARGS__)))
#define	XBUS_INFO(xbus, fmt, ...)		XBUS_PRINTK(INFO, "", xbus, fmt, ## __VA_ARGS__)
#define	XBUS_NOTICE(xbus, fmt, ...)		XBUS_PRINTK(NOTICE, "", xbus, fmt, ## __VA_ARGS__)
#define	XBUS_ERR(xbus, fmt, ...)		XBUS_PRINTK(ERR, "", xbus, fmt, ## __VA_ARGS__)

#define	XPD_DBG(bits, xpd, fmt, ...)	\
		((void)((debug & (DBG_ ## bits)) && XPD_PRINTK(DEBUG, "-" #bits, xpd, "%s: " fmt, __FUNCTION__, ## __VA_ARGS__)))
#define	XPD_INFO(xpd, fmt, ...)		XPD_PRINTK(INFO, "", xpd, fmt, ## __VA_ARGS__)
#define	XPD_NOTICE(xpd, fmt, ...)	XPD_PRINTK(NOTICE, "", xpd, fmt, ## __VA_ARGS__)
#define	XPD_WARNING(xpd, fmt, ...)	XPD_PRINTK(WARNING, "", xpd, fmt, ## __VA_ARGS__)
#define	XPD_ERR(xpd, fmt, ...)		XPD_PRINTK(ERR, "", xpd, fmt, ## __VA_ARGS__)

#define	LINE_DBG(bits, xpd, pos, fmt, ...)	\
			((void)((debug & (DBG_ ## bits)) && LINE_PRINTK(DEBUG, "-" #bits, xpd, pos, "%s: " fmt, __FUNCTION__, ## __VA_ARGS__)))
#define	LINE_NOTICE(xpd, pos, fmt, ...)		LINE_PRINTK(NOTICE, "", xpd, pos, fmt, ## __VA_ARGS__)
#define	LINE_ERR(xpd, pos, fmt, ...)		LINE_PRINTK(ERR, "", xpd, pos, fmt, ## __VA_ARGS__)

#define	PORT_DBG(bits, xbus, unit, port, fmt, ...)	\
			((void)((debug & (DBG_ ## bits)) && PORT_PRINTK(DEBUG, "-" #bits,	\
					xbus, unit, port, "%s: " fmt, __FUNCTION__, ## __VA_ARGS__)))
#define	PORT_NOTICE(xbus, unit, port, fmt, ...)	PORT_PRINTK(NOTICE, "", xbus, unit, port, fmt, ## __VA_ARGS__)
#define	PORT_ERR(xbus, unit, port, fmt, ...)		PORT_PRINTK(ERR, "", xbus, unit, port, fmt, ## __VA_ARGS__)

/*
 * Bits for debug
 */
#define	DBG_GENERAL	BIT(0)
#define	DBG_PCM		BIT(1)
#define	DBG_LEDS	BIT(2)
#define	DBG_SYNC	BIT(3)
#define	DBG_SIGNAL	BIT(4)
#define	DBG_PROC	BIT(5)
#define	DBG_REGS	BIT(6)
#define	DBG_DEVICES	BIT(7)	/* instantiation/destruction etc. */
#define	DBG_COMMANDS	BIT(8)	/* All commands */
#define	DBG_ANY		(~0)

void dump_poll(int debug, const char *msg, int poll);

static inline char *rxsig2str(enum dahdi_rxsig sig)
{
	switch(sig) {
		case DAHDI_RXSIG_ONHOOK:	return "ONHOOK";
		case DAHDI_RXSIG_OFFHOOK:	return "OFFHOOK";
		case DAHDI_RXSIG_START:	return "START";
		case DAHDI_RXSIG_RING:	return "RING";
		case DAHDI_RXSIG_INITIAL:	return "INITIAL";
	}
	return "Unknown rxsig";
}

static inline char *txsig2str(enum dahdi_txsig sig)
{
	switch(sig) {
		case DAHDI_TXSIG_ONHOOK:	return "TXSIG_ONHOOK";
		case DAHDI_TXSIG_OFFHOOK:	return "TXSIG_OFFHOOK";
		case DAHDI_TXSIG_START:	return "TXSIG_START";
		case DAHDI_TXSIG_KEWL:	return "TXSIG_KEWL";				/* Drop battery if possible */
		case DAHDI_TXSIG_TOTAL: break;
	}
	return "Unknown txsig";
}

static inline char *event2str(int event)
{
	switch(event) {
		case DAHDI_EVENT_NONE:		return "NONE";
		case DAHDI_EVENT_ONHOOK:		return "ONHOOK";
		case DAHDI_EVENT_RINGOFFHOOK:	return "RINGOFFHOOK";
		case DAHDI_EVENT_WINKFLASH:	return "WINKFLASH";
		case DAHDI_EVENT_ALARM:		return "ALARM";
		case DAHDI_EVENT_NOALARM:		return "NOALARM";
		case DAHDI_EVENT_ABORT:		return "ABORT";
		case DAHDI_EVENT_OVERRUN:		return "OVERRUN";
		case DAHDI_EVENT_BADFCS:		return "BADFCS";
		case DAHDI_EVENT_DIALCOMPLETE:	return "DIALCOMPLETE";
		case DAHDI_EVENT_RINGERON:		return "RINGERON";
		case DAHDI_EVENT_RINGEROFF:	return "RINGEROFF";
		case DAHDI_EVENT_HOOKCOMPLETE:	return "HOOKCOMPLETE";
		case DAHDI_EVENT_BITSCHANGED:	return "BITSCHANGED";
		case DAHDI_EVENT_PULSE_START:	return "PULSE_START";
		case DAHDI_EVENT_TIMER_EXPIRED:	return "TIMER_EXPIRED";
		case DAHDI_EVENT_TIMER_PING:	return "TIMER_PING";
		case DAHDI_EVENT_POLARITY:		return "POLARITY";
	}
	return "Unknown event";
}

static inline char *hookstate2str(int hookstate)
{
	switch(hookstate) {
		case DAHDI_ONHOOK:		return "DAHDI_ONHOOK";
		case DAHDI_START:		return "DAHDI_START";
		case DAHDI_OFFHOOK:	return "DAHDI_OFFHOOK";
		case DAHDI_WINK:		return "DAHDI_WINK";
		case DAHDI_FLASH:		return "DAHDI_FLASH";
		case DAHDI_RING:		return "DAHDI_RING";
		case DAHDI_RINGOFF:	return "DAHDI_RINGOFF";
	}
	return "Unknown hookstate";
}

/* From dahdi-base.c */
static inline char *sig2str(int sig)
{
	switch (sig) {
		case DAHDI_SIG_FXSLS:	return "FXSLS";
		case DAHDI_SIG_FXSKS:	return "FXSKS";
		case DAHDI_SIG_FXSGS:	return "FXSGS";
		case DAHDI_SIG_FXOLS:	return "FXOLS";
		case DAHDI_SIG_FXOKS:	return "FXOKS";
		case DAHDI_SIG_FXOGS:	return "FXOGS";
		case DAHDI_SIG_EM:		return "E&M";
		case DAHDI_SIG_EM_E1:	return "E&M-E1";
		case DAHDI_SIG_CLEAR:	return "Clear";
		case DAHDI_SIG_HDLCRAW:	return "HDLCRAW";
		case DAHDI_SIG_HDLCFCS:	return "HDLCFCS";
		case DAHDI_SIG_HDLCNET:	return "HDLCNET";
		case DAHDI_SIG_SLAVE:	return "Slave";
		case DAHDI_SIG_CAS:	return "CAS";
		case DAHDI_SIG_DACS:	return "DACS";
		case DAHDI_SIG_DACS_RBS:	return "DACS+RBS";
		case DAHDI_SIG_SF:		return "SF (ToneOnly)";
		case DAHDI_SIG_NONE:
					break;
	}
	return "Unconfigured";
}

static inline char *alarmbit2str(int alarmbit)
{
	/* from dahdi/kernel.h */
	switch(1 << alarmbit) {
		case DAHDI_ALARM_NONE:	return "NONE";
		case DAHDI_ALARM_RECOVER:	return "RECOVER";
		case DAHDI_ALARM_LOOPBACK:	return "LOOPBACK";
		case DAHDI_ALARM_YELLOW:	return "YELLOW";
		case DAHDI_ALARM_RED:	return "RED";
		case DAHDI_ALARM_BLUE:	return "BLUE";
		case DAHDI_ALARM_NOTOPEN:	return "NOTOPEN";
	}
	return "UNKNOWN";
}

void alarm2str(int alarm, char *buf, int buflen);

#endif	/* DAHDI_DEBUG_H */
