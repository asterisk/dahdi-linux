/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_playout_buf_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_playout_buf.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_playout_buf_priv.h file.

This file is part of the Octasic OCT6100 GPL API . The OCT6100 GPL API  is 
free software; you can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation; 
either version 2 of the License, or (at your option) any later version.

The OCT6100 GPL API is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details. 

You should have received a copy of the GNU General Public License 
along with the OCT6100 GPL API; if not, write to the Free Software 
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

$Octasic_Release: OCT612xAPI-01.00-PR49 $

$Octasic_Revision: 21 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_PLAYOUT_BUF_PUB_H__
#define __OCT6100_PLAYOUT_BUF_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_BUFFER_LOAD_
{
	PUINT32	pulBufferIndex;			/* Index identifying the buffer. */
	PUINT32	pulPlayoutFreeMemSize;	/* Amount of free memory available for other buffers. */
	
	PUINT8	pbyBufferPattern;		/* A byte pointer pointing to a valid buffer to be loaded into the chip's external memory. */
	UINT32	ulBufferSize;			/* Size of the buffer loaded into external memory. */

	UINT32	ulBufferPcmLaw;			/* Buffer PCM law. */

} tOCT6100_BUFFER_LOAD, *tPOCT6100_BUFFER_LOAD;

typedef struct _OCT6100_BUFFER_LOAD_BLOCK_INIT_
{
	PUINT32	pulBufferIndex;			/* Index identifying the buffer. */
	PUINT32	pulPlayoutFreeMemSize;	/* Amount of free memory available for other buffers. */
	
	UINT32	ulBufferSize;			/* Size of the buffer to be loaded in memory.  This space will be reserved. */

	UINT32	ulBufferPcmLaw;			/* Buffer PCM law. */

} tOCT6100_BUFFER_LOAD_BLOCK_INIT, *tPOCT6100_BUFFER_LOAD_BLOCK_INIT;

typedef struct _OCT6100_BUFFER_LOAD_BLOCK_
{
	UINT32	ulBufferIndex;			/* Index identifying the buffer. */
	
	/* Offset, in bytes, of the first byte in the block to be loaded. */
	/* This offset is with respect to the beginning of the buffer. */
	/* This value must be modulo 2 */
	UINT32	ulBlockOffset;			
									
	/* Size of the block to be loaded into external memory. */
	/* This value must be modulo 2. */
	UINT32	ulBlockLength;			

	/* A pointer pointing to a valid buffer block to be loaded */
	/* into the chip's external memory. This is a pointer to the entire */
	/* buffer. The API uses the ulBlockOffset and ulBlockLength to index */
	/* within this buffer and obtain the block to be loaded. */
	PUINT8	pbyBufferPattern;

} tOCT6100_BUFFER_LOAD_BLOCK, *tPOCT6100_BUFFER_LOAD_BLOCK;

typedef struct _OCT6100_BUFFER_UNLOAD_
{
	UINT32	ulBufferIndex;			/* Index identifying the buffer. */

} tOCT6100_BUFFER_UNLOAD, *tPOCT6100_BUFFER_UNLOAD;

typedef struct _OCT6100_BUFFER_PLAYOUT_ADD_
{
	UINT32	ulChannelHndl;			/* Echo cancelling channel on which to play the buffer. */

	UINT32	ulBufferIndex;			/* Index identifying the buffer. */
	
	UINT32	ulPlayoutPort;			/* Selected channel port where to play to tone. */
	UINT32	ulMixingMode;			/* Weither or not the voice stream will be muted while playing the buffer. */

	INT32	lGainDb;				/* Gain applied to the buffer that will be played on the specified port. */

	BOOL	fRepeat;				/* Use ulRepeatCount variable. */
	UINT32	ulRepeatCount;			/* Number of times to repeat playing the selected buffer. */

	UINT32	ulDuration;				/* Duration in millisecond that this buffer should play.  Setting this overrides fRepeat. */
	
	UINT32	ulBufferLength;			/* Length of the buffer to play (starting at the beginning), AUTO_SELECT for all. */

} tOCT6100_BUFFER_PLAYOUT_ADD, *tPOCT6100_BUFFER_PLAYOUT_ADD;

typedef struct _OCT6100_BUFFER_PLAYOUT_START_
{
	UINT32	ulChannelHndl;			/* Echo cancelling channel on which to play the buffer. */
	UINT32	ulPlayoutPort;			/* Selected channel port where to play to tone. */

	BOOL	fNotifyOnPlayoutStop;	/* Check if the buffers have finished playing on this channel/port. */
									/* The events are queued in a soft buffer that the user must empty regularly. */
	UINT32	ulUserEventId;			/* Returned to the user when the playout is finished and the user has set the fNotifyOnPlayoutStop flag. */

	BOOL	fAllowStartWhileActive;	/* Use this to add buffers to something that is already playing on the channel/port. */

} tOCT6100_BUFFER_PLAYOUT_START, *tPOCT6100_BUFFER_PLAYOUT_START;

typedef struct _OCT6100_BUFFER_PLAYOUT_STOP_
{
	UINT32	ulChannelHndl;			/* Echo cancelling channel on which to play the buffer. */
	UINT32	ulPlayoutPort;			/* Selected channel port where to play to tone. */
	BOOL	fStopCleanly;			/* Whether or not the skip will be clean. */

	PBOOL	pfAlreadyStopped;		/* Whether playout was already stopped or not. */
	PBOOL	pfNotifyOnPlayoutStop;	/* Whether the user chosed to receive an event on playout stop. */
	
} tOCT6100_BUFFER_PLAYOUT_STOP, *tPOCT6100_BUFFER_PLAYOUT_STOP;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100BufferPlayoutLoadDef(
				OUT		tPOCT6100_BUFFER_LOAD				f_pBufferLoad );
UINT32 Oct6100BufferPlayoutLoad(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD				f_pBufferLoad );

UINT32 Oct6100BufferPlayoutLoadBlockInitDef(
				OUT		tPOCT6100_BUFFER_LOAD_BLOCK_INIT	f_pBufferLoadBlockInit );
UINT32 Oct6100BufferPlayoutLoadBlockInit(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK_INIT	f_pBufferLoadBlockInit );

UINT32 Oct6100BufferPlayoutLoadBlockDef(
				OUT		tPOCT6100_BUFFER_LOAD_BLOCK			f_pBufferLoadBlock );
UINT32 Oct6100BufferPlayoutLoadBlock(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK			f_pBufferLoadBlock );

UINT32 Oct6100BufferPlayoutUnloadDef(
				OUT		tPOCT6100_BUFFER_UNLOAD				f_pBufferUnload );
UINT32 Oct6100BufferPlayoutUnload(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_UNLOAD				f_pBufferUnload );

UINT32 Oct6100BufferPlayoutAddDef(
				OUT		tPOCT6100_BUFFER_PLAYOUT_ADD		f_pBufferPlayoutAdd );
UINT32 Oct6100BufferPlayoutAdd(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_ADD		f_pBufferPlayoutAdd );

UINT32 Oct6100BufferPlayoutStartDef(
				OUT		tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart );
UINT32 Oct6100BufferPlayoutStart(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart );

UINT32 Oct6100BufferPlayoutStopDef(
				OUT		tPOCT6100_BUFFER_PLAYOUT_STOP		f_pBufferPlayoutStop );
UINT32 Oct6100BufferPlayoutStop(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_STOP		f_pBufferPlayoutStop );

#endif /* __OCT6100_PLAYOUT_BUF_PUB_H__ */
