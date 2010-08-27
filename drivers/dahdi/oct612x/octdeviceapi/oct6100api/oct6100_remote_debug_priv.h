/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_remote_debug_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_remote_debug.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_remote_debug_pub.h file.

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

$Octasic_Revision: 13 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_REMOTE_DEBUG_PRIV_H__
#define __OCT6100_REMOTE_DEBUG_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, ulIndex, pEntry )	\
			pEntry = ( tPOCT6100_API_REMOTE_DEBUG_SESSION )(( PUINT8 )pSharedInfo + pSharedInfo->RemoteDebugInfo.ulSessionListOfst) + ulIndex;

#define mOCT6100_GET_REMOTE_DEBUG_TREE_PNT( pSharedInfo, pList )	\
			pList = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->RemoteDebugInfo.ulSessionTreeOfst);

#define mOCT6100_GET_REMOTE_DEBUG_DATA_BUF_PNT( pSharedInfo, pulDataBuf )	\
			pulDataBuf = ( PUINT16 )(( PUINT8 )pSharedInfo + pSharedInfo->RemoteDebugInfo.ulDataBufOfst);

#define mOCT6100_GET_REMOTE_DEBUG_SESSION_PKT_CACHE_PNT( pSharedInfo, pulPktCache, ulSessionIndex )	\
			pulPktCache = ( PUINT32 )(( PUINT8 )pSharedInfo + pSharedInfo->RemoteDebugInfo.ulPktCacheOfst) + (ulSessionIndex * (cOCTRPC_MAX_PACKET_BYTE_LENGTH / 4));

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetRemoteDebugSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiRemoteDebuggingSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiCheckEndianDetectField(
				IN		tPOCTRPC_OGRDTP_HEADER			f_pOgrdtpHeader,
				IN		UINT32							f_ulPktLengthDword );

VOID Oct6100ApiCalculateChecksum(
				IN		PUINT32							f_pulPktPayload,
				IN		UINT32							f_ulPktLengthDword,
				OUT		PUINT32							f_pulChecksum );

VOID Oct6100ApiFormResponsePkt(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		PUINT32							f_pulRcvPktPayload,
				IN OUT	PUINT32							f_pulRspPktPayload,
				IN		UINT32							f_ulPktLengthDword,
				IN		BOOL							f_fRetryPktResponse,
				IN		BOOL							f_fReplaceProtocolNum,
				IN		BOOL							f_fReplaceInterfaceType,
				IN		BOOL							f_fReplaceInterfaceVersion,
				IN		UINT32							f_ulSessionIndex,
				IN		UINT32							f_ulParsingErrorValue,
				IN		UINT32							f_ulPayloadDwordIndex,
				IN		UINT32							f_ulChecksum );

UINT32 Oct6100ApiCheckPktCommands(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		PUINT32							f_pulRcvPktPayload,
				IN OUT	PUINT32							f_pulRspPktPayload,
				IN		UINT32							f_ulSessionIndex,
				IN		UINT32							f_ulPktLengthDword,
				IN		UINT32							f_ulChecksum );

VOID Oct6100ApiExecutePktCommands(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		PUINT32							f_pulRcvPktPayload,
				IN		UINT32							f_ulPktLengthDword );

UINT32 Oct6100ApiCheckSessionNum(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCTRPC_OGRDTP_HEADER			f_pOgrdtpHeader,
				OUT		PUINT32							f_pulSessionIndex );

VOID Oct6100ApiRpcReadWord(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcReadBurst(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcReadArray(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcWriteWord(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcWriteSmear(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcWriteBurst(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcSetHotChannel(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcGetDebugChanIndex(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader );

VOID Oct6100ApiRpcDisconnect(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER			f_pCmndHeader,
				IN OUT	UINT32							f_ulSessionNumber );

#endif /* __OCT6100_REMOTE_DEBUG_PRIV_H__ */
