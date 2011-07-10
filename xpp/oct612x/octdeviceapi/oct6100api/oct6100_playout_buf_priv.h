/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_playout_buf_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_playout_buf.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_playout_buf_pub.h file.

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

$Octasic_Revision: 22 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_PLAYOUT_BUF_PRIV_H__
#define __OCT6100_PLAYOUT_BUF_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

/* Playout buffer list pointer macros. */
#define mOCT6100_GET_BUFFER_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_BUFFER )(( PUINT8 )pSharedInfo + pSharedInfo->ulPlayoutBufListOfst );

#define mOCT6100_GET_BUFFER_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_BUFFER )(( PUINT8 )pSharedInfo + pSharedInfo->ulPlayoutBufListOfst)) + ulIndex;

#define mOCT6100_GET_BUFFER_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulPlayoutBufAllocOfst);

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetPlayoutBufferSwSizes(
				IN		tPOCT6100_CHIP_OPEN						f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES			f_pInstSizes );

UINT32 Oct6100ApiPlayoutBufferSwInit(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance );

UINT32 Oct6100BufferLoadSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD					f_pBufferLoad,
				IN		BOOL									f_fReserveListStruct,
				IN		UINT32									f_ulBufIndex );

UINT32 Oct6100BufferLoadBlockInitSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK_INIT		f_pBufferLoadBlockInit );

UINT32 Oct6100BufferLoadBlockSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK				f_pBufferLoadBlock );

UINT32 Oct6100ApiCheckBufferParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_LOAD					f_pBufferLoad,
				IN		BOOL									f_fCheckBufferPtr );

UINT32 Oct6100ApiCheckBufferLoadBlockParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_LOAD_BLOCK				f_pBufferLoadBlock,
				OUT		PUINT32									f_pulBufferBase );

UINT32 Oct6100ApiReserveBufferResources(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_LOAD					f_pBufferLoad,
				IN		BOOL									f_fReserveListStruct,
				IN		UINT32									f_ulBufIndex,
				OUT		PUINT32									f_pulBufIndex,
				OUT		PUINT32									f_pulBufBase );

UINT32 Oct6100ApiWriteBufferInMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulBufferBase,
				IN		UINT32									f_ulBufferLength,
				IN		PUINT8									f_pbyBuffer );

UINT32 Oct6100ApiUpdateBufferEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD					f_pBufferLoad,
				IN		UINT32									f_ulBufIndex,
				IN		UINT32									f_ulBufBase );

UINT32 Oct6100BufferUnloadSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_UNLOAD					f_pBufferUnload,
				IN		BOOL									f_fReleaseListStruct );

UINT32 Oct6100ApiAssertBufferParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_UNLOAD					f_pBufferUnload,
				OUT		PUINT32									f_pulBufIndex,
				OUT		PUINT32									f_pulBufBase );

UINT32 Oct6100ApiReleaseBufferResources(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulBufIndex,
				IN		UINT32									f_ulBufBase,
				IN		BOOL									f_fReleaseListStruct );

UINT32 Oct6100BufferPlayoutAddSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_ADD			f_pBufferPlayoutAdd );

UINT32 Oct6100ApiCheckPlayoutAddParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_ADD			f_pBufferPlayoutAdd,
				OUT		PUINT32									f_pulChannelIndex, 
				OUT		PUINT32									f_pulBufferIndex );

UINT32 Oct6100ApiWriteBufferAddStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_ADD			f_pBufferPlayoutAdd,
				IN		UINT32									f_ulChannelIndex, 
				IN		UINT32									f_ulBufferIndex );

UINT32 Oct6100BufferPlayoutStartSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_START			f_pBufferPlayoutStart,
				IN		UINT32									f_ulPlayoutStopEventType );

UINT32 Oct6100ApiCheckPlayoutStartParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_START			f_pBufferPlayoutStart,
				OUT		PUINT32									f_pulChannelIndex, 
				OUT		PUINT32									f_pulBufferIndex,
				OUT		PBOOL									f_pfNotifyOnPlayoutStop,
				OUT		PUINT32									f_pulUserEventId,
				OUT		PBOOL									f_pfAllowStartIfActive );

UINT32 Oct6100ApiWriteChanPlayoutStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_START			f_pBufferPlayoutStart,
				IN		UINT32									f_ulChannelIndex, 
				IN		UINT32									f_ulBufferIndex,
				IN		BOOL									f_fNotifyOnPlayoutStop,
				IN		UINT32									f_ulUserEventId,
				IN		BOOL									f_fAllowStartIfActive,
				IN		UINT32									f_ulPlayoutStopEventType );

UINT32 Oct6100ApiUpdateChanPlayoutEntry (
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_START			f_pBufferPlayoutStart,
				IN		UINT32									f_ulChannelIndex, 
				IN		UINT32									f_ulBufferIndex );

UINT32 Oct6100BufferPlayoutStopSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_STOP			f_pBufferPlayoutStop );

UINT32 Oct6100ApiAssertPlayoutStopParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_STOP			f_pBufferPlayoutStop,
				OUT		PUINT32									f_pulChannelIndex,
				OUT		PUINT16									f_pusEchoMemIndex );

UINT32 Oct6100ApiInvalidateChanPlayoutStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_STOP			f_pBufferPlayoutStop,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT16									f_usEchoMemIndex

				);

UINT32 Oct6100ApiReleaseChanPlayoutResources (
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_STOP			f_pBufferPlayoutStop,
				IN		UINT32									f_ulChannelIndex );

UINT32 Oct6100ApiReserveBufPlayoutListEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				OUT		PUINT32									f_pulBufIndex );

UINT32 Oct6100ApiReleaseBufPlayoutListEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulBufIndex );

#endif /* __OCT6100_PLAYOUT_BUF_PRIV_H__ */
