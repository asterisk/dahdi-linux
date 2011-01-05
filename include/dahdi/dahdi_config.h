/*
 * DAHDI configuration options 
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

#ifndef _DAHDI_CONFIG_H
#define _DAHDI_CONFIG_H

#ifdef __KERNEL__
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#endif

/* DAHDI compile time options */

/* These default tone lengths are in units of milliseconds. */
#define DAHDI_CONFIG_DEFAULT_DTMF_LENGTH	100
#define DAHDI_CONFIG_DEFAULT_MFR1_LENGTH	68
#define DAHDI_CONFIG_DEFAULT_MFR2_LENGTH	100
#define DAHDI_CONFIG_PAUSE_LENGTH		500

/*
 * Uncomment if you have a European phone, or any other phone with a 
 *  short flash time.
 * This will stop the flash being mis-detected as a pulse dial "1" on
 *  phones with short flashes
 */
/* #define SHORT_FLASH_TIME */

/*
 * Uncomment to disable calibration and/or DC/DC converter tests
 * (not generally recommended)
 */
/* #define NO_CALIBRATION */
/* #define NO_DCDC */

/*
 * Boost ring voltage (Higher ring voltage, takes more power)
 * Note: this only affects the wcfxsusb and wcusb drivers; all other
 *       drivers have a 'boostringer' module parameter.
 */
/* #define BOOST_RINGER */

/*
 * Define CONFIG_CALC_XLAW if you have a small number of channels and/or
 * a small level 2 cache, to optimize for few channels
 *
 */
/* #define CONFIG_CALC_XLAW */

/*
 * Define if you want MMX optimizations in DAHDI
 *
 * Note: CONFIG_DAHDI_MMX is generally incompatible with AMD 
 * processors and can cause system instability!
 * 
 */
/* #define CONFIG_DAHDI_MMX */

/* We now use the linux kernel config to detect which options to use */
/* You can still override them below */
#if defined(CONFIG_HDLC) || defined(CONFIG_HDLC_MODULE)
#define DAHDI_HDLC_TYPE_TRANS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3)
#define HDLC_MAINTAINERS_ARE_MORE_STUPID_THAN_I_THOUGHT
#endif
#endif

/*
 * Uncomment CONFIG_DAHDI_NET to enable SyncPPP, CiscoHDLC, and Frame Relay
 * support.
 */
/* #define CONFIG_DAHDI_NET */

/*
 * Uncomment for Generic PPP support (i.e. DAHDIRAS)
 */

#if defined(CONFIG_PPP) || defined(CONFIG_PPP_MODULE)
/* #define CONFIG_DAHDI_PPP */
#endif

/*
 * Uncomment to enable "watchdog" to monitor if interfaces
 * stop taking interrupts or otherwise misbehave
 */
/* #define CONFIG_DAHDI_WATCHDOG */

/*
 * Uncomment the following to include extra debugging output.
 */
/* #define CONFIG_DAHDI_DEBUG */

/*
 * Uncomment for Non-standard FXS groundstart start state (A=Low, B=Low)
 * particularly for CAC channel bank groundstart FXO ports.
 */
/* #define CONFIG_CAC_GROUNDSTART */

/*
 * Define CONFIG_DAHDI_CORE_TIMER if you would like dahdi to always provide a
 * timing source regardless of which spans / drivers are configured.
 */
#define CONFIG_DAHDI_CORE_TIMER

/*
 * Define CONFIG_DAHDI_NO_ECHOCAN_DISABLE to prevent the 2100Hz tone detector
 * from disabling any installed software echocan.
 *
 */
/* #define CONFIG_DAHDI_NO_ECHOCAN_DISABLE */

/*
 * Define if you would like to allow software echocans to process the tx audio
 * in addition to the rx audio.  Used for things like DC removal.
 *
 */
/* #define CONFIG_DAHDI_ECHOCAN_PROCESS_TX */

/* 
 * Uncomment if you happen have an early TDM400P Rev H which 
 * sometimes forgets its PCI ID to have wcfxs match essentially all
 * subvendor ID's
 */
/* #define TDM_REVH_MATCHALL */

/* 
 * Uncomment the following if you want to support E&M trunks being
 * able to "flash" after going off-hook (dont ask why, just nod :-) ).
 *
 * NOTE: *DO NOT* Enable "EMFLASH" and "EMPULSE" at the same time!!
 *
 */
/* #define EMFLASH */

/* 
 * Uncomment the following if you want to support E&M trunks being
 * able to recognize Dial Pulse digits. This can validly be enabled
 * so that either Dial Pulse or DTMF/MF tones will be recognized, but
 * the drawback is that the ONHOOK will take an extra {rxwinktime}
 * to be recognized.
 *
 * NOTE: *DO NOT* Enable "EMFLASH" and "EMPULSE" at the same time!!
 *
 */
/* #define EMPULSE */

/* 
 * Comment out the following if you dont want events to indicate the
 * beginning of an incoming ring. Most non-Asterisk applications will
 * want this commented out.
 */
#define RINGBEGIN

/* 
 * Uncomment the following if you need to support FXS Flash events.
 * Most applications will want this commented out.
 */
/* #define FXSFLASH */

/*
 * Enable sync_tick() calls. Allows low-level drivers to synchronize
 * their internal clocks to the DAHDI master clock.
 */
#define DAHDI_SYNC_TICK

/*
 * Skip processing PCM if low-level driver won't use it anyway
 */
/* #define	OPTIMIZE_CHANMUTE */


/*
 * Pass DAHDI_AUDIOMODE to channel driver as well
 */
/* #define	DAHDI_AUDIO_NOTIFY */

/*
 * Creates an interface for mirroring the raw channel data out to a pseudo-chan
 */
/* #define CONFIG_DAHDI_MIRROR */

#endif
