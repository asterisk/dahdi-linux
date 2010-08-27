/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_events_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

  	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_events.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_events_pub.h file.

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

#ifndef __OCT6100_EVENTS_PRIV_H__
#define __OCT6100_EVENTS_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftBuf )	\
			pSoftBuf = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->SoftBufs.ulToneEventBufferMemOfst );

#define mOCT6100_GET_BUFFER_PLAYOUT_EVENT_BUF_PNT( pSharedInfo, pSoftBuf )	\
			pSoftBuf = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->SoftBufs.ulBufPlayoutEventBufferMemOfst );

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetEventsSwSizes(
				IN		tPOCT6100_CHIP_OPEN					f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES		f_pInstSizes );

UINT32 Oct6100EventGetToneSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_EVENT_GET_TONE			f_pEventGetTone );

UINT32 Oct6100ApiTransferToneEvents(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT32								f_ulResetBuf );



UINT32 Oct6100BufferPlayoutGetEventSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_GET_EVENT	f_pBufPlayoutGetEvent );

UINT32 Oct6100BufferPlayoutTransferEvents(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT32								f_ulResetBuf );

UINT32 Oct6100BufferPlayoutCheckForSpecificEvent(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT32								f_ulChannelIndex,
				IN		UINT32								f_ulChannelPort,
				IN		BOOL								f_fSaveToSoftBuffer,
				OUT		PBOOL								f_pfEventDetected );

#endif /* __OCT6100_EVENTS_PRIV_H__ */
