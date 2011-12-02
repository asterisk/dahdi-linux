/*
 * Copyright (c) 2005, Adaptive Digital Technologies, Inc.
 *
 * File Name: GpakCust.h
 *
 * Description:
 *   This file contains host system dependent definitions and prototypes of
 *   functions to support generic G.PAK API functions. The file is used when
 *   integrating G.PAK API functions in a specific host processor environment.
 *
 *   Note: This file may need to be modified by the G.PAK system integrator.
 *
 * Version: 1.0
 *
 * Revision History:
 *   06/15/05 - Initial release.
 *
 * This program has been released under the terms of the GPL version 2 by
 * permission of Adaptive Digital Technologies, Inc.
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

#ifndef _GPAKCUST_H  /* prevent multiple inclusion */
#define _GPAKCUST_H

#include <linux/module.h>
#include <linux/device.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
#include <linux/semaphore.h>
#endif

#include "gpakenum.h"
#include "adt_lec.h"

#define DEBUG_VPMADT032_ECHOCAN (1 << 4)

/* Host and DSP system dependent related definitions. */
#define MAX_DSP_CORES		128	/* maximum number of DSP cores */
#define MAX_CHANNELS		32	/* maximum number of channels */
#define MAX_WAIT_LOOPS		50	/* max number of wait delay loops */
#define DSP_IFBLK_ADDRESS	0x0100	/* DSP address of I/F block pointer */
#define DOWNLOAD_BLOCK_SIZE	512	/* download block size (DSP words) */

#define VPM150M_MAX_COMMANDS		8

#define __VPM150M_RWPAGE	(1 << 4)
#define __VPM150M_RD		(1 << 3)
#define __VPM150M_WR		(1 << 2)
#define __VPM150M_FIN		(1 << 1)
#define __VPM150M_TX		(1 << 0)
#define __VPM150M_RWPAGE	(1 << 4)
#define __VPM150M_RD		(1 << 3)
#define __VPM150M_WR		(1 << 2)
#define __VPM150M_FIN		(1 << 1)
#define __VPM150M_TX		(1 << 0)

/* Some Bit ops for different operations */
#define VPM150M_HPIRESET		1
#define VPM150M_SWRESET			2
#define VPM150M_ACTIVE			4

#define NLPTYPE_NONE		0
#define NLPTYPE_MUTE		1
#define NLPTYPE_RANDOM_NOISE 	2
#define HOTH_NOISE_NLPTYPE 	3
#define NLPTYPE_SUPPRESS	4
#define NLPTYPE_RESERVED	5
#define NLPTYPE_AUTOSUPPRESS	6
#define DEFAULT_NLPTYPE 	NLPTYPE_AUTOSUPPRESS

/* This is the threshold (in dB) for enabling and disabling of the NLP */
#define DEFAULT_NLPTHRESH		22
#define DEFAULT_NLPMAXSUPP		10

struct vpmadt032_cmd {
	struct list_head node;
	__le32  address;
	__le16	data;
	u8	desc;
	u8	txident;
	struct completion complete;
};

/* Contains the options used when initializing the vpmadt032 module */
struct vpmadt032_options {
	int vpmnlptype;
	int vpmnlpthresh;
	int vpmnlpmaxsupp;
	u32 debug;
	u32 channels;
};

struct GpakChannelConfig;

struct vpmadt032 {
	struct voicebus *vb;
	struct work_struct work;
	struct workqueue_struct *wq;
	int dspid;
	struct semaphore sem;
	unsigned long control;
	unsigned char curpage;
	unsigned short version;
	struct adt_lec_params curecstate[MAX_CHANNELS];
	spinlock_t change_list_lock;
	struct list_head change_list;
	spinlock_t list_lock;
	/* Commands that are waiting to be processed. */
	struct list_head pending_cmds;
	/* Commands that are currently in progress by the VPM module */
	struct list_head active_cmds;
	struct vpmadt032_options options;
	void (*setchanconfig_from_state)(struct vpmadt032 *vpm, int channel, struct GpakChannelConfig *chanconfig);
};

struct voicebus;
struct dahdi_chan;
struct dahdi_echocanparams;
struct dahdi_echocanparam;
struct dahdi_echocan_state;

char vpmadt032tone_to_zaptone(GpakToneCodes_t tone);
int vpmadt032_test(struct vpmadt032 *vpm, struct voicebus *vb);
int vpmadt032_init(struct vpmadt032 *vpm);
int vpmadt032_reset(struct vpmadt032 *vpm);
struct vpmadt032 *vpmadt032_alloc(struct vpmadt032_options *options);
void vpmadt032_free(struct vpmadt032 *vpm);
int vpmadt032_echocan_create(struct vpmadt032 *vpm, int channo,
			     enum adt_companding companding,
			     struct dahdi_echocanparams *ecp,
			     struct dahdi_echocanparam *p);
void vpmadt032_echocan_free(struct vpmadt032 *vpm, int channo,
	struct dahdi_echocan_state *ec);

struct GpakEcanParms;
void vpmadt032_get_default_parameters(struct GpakEcanParms *p);

/* If there is a command ready to go to the VPMADT032, return it, otherwise
 * NULL. Call with local interrupts disabled.  */
static inline struct vpmadt032_cmd *vpmadt032_get_ready_cmd(struct vpmadt032 *vpm)
{
	struct vpmadt032_cmd *cmd;

	spin_lock(&vpm->list_lock);
	if (list_empty(&vpm->pending_cmds)) {
		spin_unlock(&vpm->list_lock);
		return NULL;
	}
	cmd = list_entry(vpm->pending_cmds.next, struct vpmadt032_cmd, node);
	list_move_tail(&cmd->node, &vpm->active_cmds);
	spin_unlock(&vpm->list_lock);
	return cmd;
}

/**
 * call with local interrupts disabled.
 */
static inline void vpmadt032_resend(struct vpmadt032 *vpm)
{
	struct vpmadt032_cmd *cmd, *temp;

	/* By moving the commands back to the pending list, they will be
	 * transmitted when room is available */
	spin_lock(&vpm->list_lock);
	list_for_each_entry_safe(cmd, temp, &vpm->active_cmds, node) {
		cmd->desc &= ~(__VPM150M_TX);
		list_move_tail(&cmd->node, &vpm->pending_cmds);
	}
	spin_unlock(&vpm->list_lock);
}


typedef __u16 DSP_WORD;			/* 16 bit DSP word */
typedef __u32 DSP_ADDRESS;		/* 32 bit DSP address */
typedef __u32 GPAK_FILE_ID;		/* G.PAK Download file identifier */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadDspMemory - Read DSP memory.
 *
 * FUNCTION
 *  This function reads a contiguous block of words from DSP memory starting at
 *  the specified address.
 *
 * RETURNS
 *  nothing
 *
 */
extern void gpakReadDspMemory(
    unsigned short int  DspId,  /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    DSP_ADDRESS DspAddress,     /* DSP's memory address of first word */
    unsigned int NumWords,      /* number of contiguous words to read */
    DSP_WORD *pWordValues       /* pointer to array of word values variable */
    );


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakWriteDspMemory - Write DSP memory.
 *
 * FUNCTION
 *  This function writes a contiguous block of words to DSP memory starting at
 *  the specified address.
 *
 * RETURNS
 *  nothing
 *
 */
extern void gpakWriteDspMemory(
    unsigned short int DspId,   /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    DSP_ADDRESS DspAddress,     /* DSP's memory address of first word */
    unsigned int NumWords,      /* number of contiguous words to write */
    DSP_WORD *pWordValues       /* pointer to array of word values to write */
    );


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakHostDelay - Delay for a fixed time interval.
 *
 * FUNCTION
 *  This function delays for a fixed time interval before returning. The time
 *  interval is the Host Port Interface sampling period when polling a DSP for
 *  replies to command messages.
 *
 * RETURNS
 *  nothing
 *
 */
extern void gpakHostDelay(void);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakLockAccess - Lock access to the specified DSP.
 *
 * FUNCTION
 *  This function aquires exclusive access to the specified DSP.
 *
 * RETURNS
 *  nothing
 *
 */
extern void gpakLockAccess(
    unsigned short int DspId      /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    );

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakUnlockAccess - Unlock access to the specified DSP.
 *
 * FUNCTION
 *  This function releases exclusive access to the specified DSP.
 *
 * RETURNS
 *  nothing
 *
 */
extern void gpakUnlockAccess(
    unsigned short int DspId       /* DSP Identifier (0 to MAX_DSP_CORES-1) */
    );

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadFile - Read a block of bytes from a G.PAK Download file.
 *
 * FUNCTION
 *  This function reads a contiguous block of bytes from a G.PAK Download file
 *  starting at the current file position.
 *
 * RETURNS
 *  The number of bytes read from the file.
 *   -1 indicates an error occurred.
 *    0 indicates all bytes have been read (end of file)
 *
 */
extern int gpakReadFile(
    GPAK_FILE_ID FileId,        /* G.PAK Download File Identifier */
    unsigned char *pBuffer,	    /* pointer to buffer for storing bytes */
    unsigned int NumBytes       /* number of bytes to read */
    );


#endif  /* prevent multiple inclusion */


