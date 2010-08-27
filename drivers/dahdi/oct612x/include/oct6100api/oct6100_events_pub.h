/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_events_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_events.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_events_priv.h file.

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

$Octasic_Revision: 14 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_EVENTS_PUB_H__
#define __OCT6100_EVENTS_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_TONE_EVENT_
{
	UINT32	ulChannelHndl;
	UINT32	ulUserChanId;

	UINT32	ulToneDetected;

	UINT32	ulTimestamp;
	UINT32	ulEventType;
	
	UINT32	ulExtToneDetectionPort;

} tOCT6100_TONE_EVENT, *tPOCT6100_TONE_EVENT;

typedef struct _OCT6100_EVENT_GET_TONE_
{
	BOOL	fMoreEvents;
	BOOL	fResetBufs;
	
	UINT32	ulMaxToneEvent;
	UINT32	ulNumValidToneEvent;

	tPOCT6100_TONE_EVENT	pToneEvent;

} tOCT6100_EVENT_GET_TONE, *tPOCT6100_EVENT_GET_TONE;

typedef struct _OCT6100_BUFFER_PLAYOUT_EVENT_
{
	UINT32	ulChannelHndl;
	UINT32	ulUserChanId;
	UINT32	ulChannelPort;

	UINT32	ulTimestamp;

	UINT32	ulUserEventId;
	UINT32	ulEventType;

} tOCT6100_BUFFER_PLAYOUT_EVENT, *tPOCT6100_BUFFER_PLAYOUT_EVENT;

typedef struct _OCT6100_BUFFER_PLAYOUT_GET_EVENT_
{
	BOOL	fMoreEvents;
	BOOL	fResetBufs;

	UINT32	ulMaxEvent;
	UINT32	ulNumValidEvent;

	tPOCT6100_BUFFER_PLAYOUT_EVENT	pBufferPlayoutEvent;

} tOCT6100_BUFFER_PLAYOUT_GET_EVENT, *tPOCT6100_BUFFER_PLAYOUT_GET_EVENT;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100EventGetToneDef(
					OUT		tPOCT6100_EVENT_GET_TONE			f_pEventGetTone );
UINT32 Oct6100EventGetTone(
					IN OUT	tPOCT6100_INSTANCE_API				f_pApiInst,
					IN OUT	tPOCT6100_EVENT_GET_TONE			f_pEventGetTone );

UINT32 Oct6100BufferPlayoutGetEventDef(
					OUT		tPOCT6100_BUFFER_PLAYOUT_GET_EVENT	f_pBufPlayoutGetEvent );
UINT32 Oct6100BufferPlayoutGetEvent(
					IN OUT	tPOCT6100_INSTANCE_API				f_pApiInst,
					IN OUT	tPOCT6100_BUFFER_PLAYOUT_GET_EVENT	f_pBufPlayoutGetEvent );

#endif /* __OCT6100_EVENTS_PUB_H__ */

