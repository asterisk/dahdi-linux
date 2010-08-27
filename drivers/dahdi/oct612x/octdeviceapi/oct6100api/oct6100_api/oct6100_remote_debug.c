/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_remote_debug.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the routines used for remote debugging.

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

$Octasic_Revision: 35 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "apilib/octapi_bt0.h"
#include "apilib/octapi_largmath.h"

#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_channel_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_debug_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_remote_debug_pub.h"

#include "octrpc/rpc_protocol.h"
#include "octrpc/oct6100_rpc_protocol.h"

#include "oct6100_miscellaneous_priv.h"
#include "oct6100_chip_open_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_debug_priv.h"
#include "oct6100_remote_debug_priv.h"

/****************************  PUBLIC FUNCTIONS  *****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100RemoteDebug

Description:    This function interprets the remote debugging packets received 
				by the user’s software. Commands contained in the packet are 
				executed by the API. In addition, a response packet is 
				constructed and returned by the function. It is the responsibility 
				of the user’s software to transmit the response packet back to 
				the source of the debugging packet.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pRemoteDebug			Pointer to a remote debug structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100RemoteDebugDef
UINT32 Oct6100RemoteDebugDef(
				tPOCT6100_REMOTE_DEBUG		f_pRemoteDebug )
{
	f_pRemoteDebug->pulReceivedPktPayload = NULL;
	f_pRemoteDebug->ulReceivedPktLength = 0;
	f_pRemoteDebug->pulResponsePktPayload = NULL;
	f_pRemoteDebug->ulMaxResponsePktLength = 0;
	f_pRemoteDebug->ulResponsePktLength = 0;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100RemoteDebug
UINT32 Oct6100RemoteDebug(
				tPOCT6100_INSTANCE_API		f_pApiInstance,
				tPOCT6100_REMOTE_DEBUG		f_pRemoteDebug )
{
	tPOCTRPC_OGRDTP_HEADER		pOgrdtpHeader;
	tPOCTRPC_INTERFACE_HEADER	pInterfaceHeader;
	tPOCTRPC_COMMAND_HEADER		pRspCmndHeader;
	PUINT32	pulRcvPktPayload;
	PUINT32	pulRspPktPayload;
	UINT32	ulPktLengthDword;
	UINT32	ulSessionIndex;
	UINT32	ulChecksum;
	UINT32	ulResult;

	/* Check for errors. */
	if ( f_pRemoteDebug->pulReceivedPktPayload == NULL )
		return cOCT6100_ERR_REMOTEDEBUG_RECEIVED_PKT_PAYLOAD;
	if ( f_pRemoteDebug->pulResponsePktPayload == NULL )
		return cOCT6100_ERR_REMOTEDEBUG_RESPONSE_PKT_PAYLOAD;
	if ( f_pRemoteDebug->ulReceivedPktLength < cOCTRPC_MIN_PACKET_BYTE_LENGTH )
		return cOCT6100_ERR_REMOTEDEBUG_RECEIVED_PKT_LENGTH;
	if ( f_pRemoteDebug->ulReceivedPktLength > cOCTRPC_MAX_PACKET_BYTE_LENGTH )
		return cOCT6100_ERR_REMOTEDEBUG_RECEIVED_PKT_LENGTH;
	if ( f_pRemoteDebug->ulMaxResponsePktLength < f_pRemoteDebug->ulReceivedPktLength )
		return cOCT6100_ERR_REMOTEDEBUG_RESPONSE_PKT_LENGTH;
	if ( (f_pRemoteDebug->ulReceivedPktLength % 4) != 0 )
		return cOCT6100_ERR_REMOTEDEBUG_RECEIVED_PKT_LENGTH;
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxRemoteDebugSessions == 0 )
		return cOCT6100_ERR_REMOTEDEBUG_DISABLED;

	/* Set response length as received length. */
	f_pRemoteDebug->ulResponsePktLength = f_pRemoteDebug->ulReceivedPktLength;

	/* Typecast the packet payload to local pointers. */
	pOgrdtpHeader = ( tPOCTRPC_OGRDTP_HEADER )f_pRemoteDebug->pulReceivedPktPayload;
	pInterfaceHeader = ( tPOCTRPC_INTERFACE_HEADER )(f_pRemoteDebug->pulReceivedPktPayload + (sizeof( tOCTRPC_OGRDTP_HEADER ) / 4));

	/* Get local pointer to received and response packet payloads. */
	pulRcvPktPayload = f_pRemoteDebug->pulReceivedPktPayload;
	pulRspPktPayload = f_pRemoteDebug->pulResponsePktPayload;

	/* Get the length of the packet in UINT32s. */
	ulPktLengthDword = f_pRemoteDebug->ulReceivedPktLength / 4;

	/* Check the endian detection field to determine if the payload must be */
	/* swapped to account for different endian formats. */
	ulResult = Oct6100ApiCheckEndianDetectField( pOgrdtpHeader, ulPktLengthDword );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check the packet's length. */
	if ( pOgrdtpHeader->ulPktByteSize != f_pRemoteDebug->ulReceivedPktLength )
		return cOCT6100_ERR_REMOTEDEBUG_RECEIVED_PKT_LENGTH;

	/* Perform the sum of each word in the packet and compare to checksum. */
	Oct6100ApiCalculateChecksum( pulRcvPktPayload, ulPktLengthDword, &ulChecksum );
	if ( ulChecksum != pOgrdtpHeader->ulChecksum )
		return cOCT6100_ERR_REMOTEDEBUG_CHECKSUM;

	/* Check if the packet's session number has a corresponding entry in the API table.
		If not then close an entry which has timed out, and allocate the entry to the
		new session number. */
	ulResult = Oct6100ApiCheckSessionNum( f_pApiInstance, pOgrdtpHeader, &ulSessionIndex );
	if ( ulResult == cOCT6100_ERR_REMOTEDEBUG_ALL_SESSIONS_OPEN )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, cOCT6100_INVALID_VALUE, cOCTRPC_RDBGERR_ALL_SESSIONS_OPEN, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}
	else if ( ulResult == cOCT6100_ERR_REMOTEDEBUG_TRANSACTION_ANSWERED )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, TRUE, FALSE, FALSE, FALSE, ulSessionIndex, cOCT6100_INVALID_VALUE, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}
	else if ( ulResult != cOCT6100_ERR_OK )
	{
		return ulResult;
	}

	/* Check if an echo packet.  If so then there's no need to check the rest of
		the packet.  Simply copy the packet back to the output buffer, enter the
		protocol number supported by this API compilation, and recalculate the
		checksum.  If the packet is not an echo packet and the protocol version
		does not correspond to this compiled version then return the supported
		protocol version. */
	if ( pOgrdtpHeader->ulRpcProtocolNum == cOCTRPC_ECHO_PROTOCOL )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, TRUE, FALSE, FALSE, ulSessionIndex, cOCT6100_INVALID_VALUE, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}
	else if ( pOgrdtpHeader->ulRpcProtocolNum != cOCTRPC_PROTOCOL_V1_1 )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, TRUE, FALSE, FALSE, ulSessionIndex, cOCTRPC_RDBGERR_PROTOCOL_NUMBER, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}
	else if ( f_pRemoteDebug->ulReceivedPktLength <= cOCTRPC_FIRST_COMMAND_BYTE_OFFSET )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, ulSessionIndex, cOCTRPC_RDBGERR_NO_COMMAND_HEADER, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}


	/* Check the packet's RPC interface type and version.  If either does not match then
		return the packet with the supported interface type and version of this compilation. */
	if ( pInterfaceHeader->ulInterfaceVersion != cOCTRPC_INTERFACE_VERSION )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, FALSE, TRUE, TRUE, ulSessionIndex, cOCTRPC_RDBGERR_INTERFACE_VERSION, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}
	if ( pInterfaceHeader->ulInterfaceType != cOCTRPC_OCT6100_INTERFACE )
	{
		Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, FALSE, TRUE, TRUE, ulSessionIndex, cOCTRPC_RDBGERR_INTERFACE_TYPE, cOCT6100_INVALID_VALUE, ulChecksum );
		return cOCT6100_ERR_OK;
	}

	/* Check each command header to make sure the indicated command and length agree.  If
		there is an error in the packet's commands then the response packet will be
		constructed by the function. */
	ulResult = Oct6100ApiCheckPktCommands( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulSessionIndex, ulPktLengthDword, ulChecksum );
	if ( ulResult == cOCT6100_ERR_REMOTE_DEBUG_PARSING_ERROR )
		return cOCT6100_ERR_OK;

	/* The packet's fields are valid.  Each command must now be extracted and executed. */
	Oct6100ApiExecutePktCommands( f_pApiInstance, pulRcvPktPayload, ulPktLengthDword );

	pRspCmndHeader = ( tPOCTRPC_COMMAND_HEADER )(( PUINT32 )pulRspPktPayload + ((sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER )) / 4));

	/* Verify if the new method of using the protocol is the selected case. */
	/* All commands have been executed.  Calculate the packet's new checksum
	   and copy the packet to user provided buffer for response packet. */
	Oct6100ApiCalculateChecksum( pulRcvPktPayload, ulPktLengthDword, &ulChecksum );
	
	/* Send response packet. */
	Oct6100ApiFormResponsePkt( f_pApiInstance, pulRcvPktPayload, pulRspPktPayload, ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, ulSessionIndex, cOCTRPC_RDBGERR_OK, cOCT6100_INVALID_VALUE, ulChecksum );

	return cOCT6100_ERR_OK;
}
#endif


/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetRemoteDebugSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of remote debugging.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pChipOpen				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetRemoteDebugSwSizes
UINT32 Oct6100ApiGetRemoteDebugSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pChipOpen,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Memory needed for remote debugging sessions. */
	if ( f_pChipOpen->ulMaxRemoteDebugSessions > 0 )
	{
		f_pInstSizes->ulRemoteDebugList = f_pChipOpen->ulMaxRemoteDebugSessions * sizeof( tOCT6100_API_REMOTE_DEBUG_SESSION );

		ulResult = octapi_bt0_get_size( f_pChipOpen->ulMaxRemoteDebugSessions, 4, 4, &f_pInstSizes->ulRemoteDebugTree );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_41;

		f_pInstSizes->ulRemoteDebugPktCache = cOCTRPC_MAX_PACKET_BYTE_LENGTH * f_pChipOpen->ulMaxRemoteDebugSessions;
		f_pInstSizes->ulRemoteDebugDataBuf = cOCTRPC_MAX_PACKET_BYTE_LENGTH * 4;
	}
	else
	{
		f_pInstSizes->ulRemoteDebugList = 0;
		f_pInstSizes->ulRemoteDebugTree = 0;
		f_pInstSizes->ulRemoteDebugPktCache = 0;
		f_pInstSizes->ulRemoteDebugDataBuf = 0;
	}

	/* Round off the size. */
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulRemoteDebugList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulRemoteDebugTree, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulRemoteDebugPktCache, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulRemoteDebugDataBuf, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRemoteDebuggingSwInit

Description:    Initializes all portions of the API instance associated to
				remote debugging.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRemoteDebuggingSwInit
UINT32 Oct6100ApiRemoteDebuggingSwInit(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	PVOID	pSessionTree;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	pSharedInfo->RemoteDebugInfo.ulNumSessionsOpen = 0;
	pSharedInfo->RemoteDebugInfo.ulMaxSessionsOpen = pSharedInfo->ChipConfig.usMaxRemoteDebugSessions;
	pSharedInfo->RemoteDebugInfo.ulSessionListHead = cOCT6100_INVALID_VALUE;
	pSharedInfo->RemoteDebugInfo.ulSessionListTail = cOCT6100_INVALID_VALUE;

	if ( pSharedInfo->ChipConfig.usMaxRemoteDebugSessions > 0 )
	{
		mOCT6100_GET_REMOTE_DEBUG_TREE_PNT( pSharedInfo, pSessionTree )
	
		ulResult = octapi_bt0_init( ( ( PVOID* )&pSessionTree ), pSharedInfo->ChipConfig.usMaxRemoteDebugSessions, 4, 4 );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_42;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckEndianDetectField

Description:    Checks the endian field of a packet and performs a swap of
				the packet data if deemed necessary.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulPktLengthDword		Length of the packet in dwords.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckEndianDetectField
UINT32 Oct6100ApiCheckEndianDetectField(
				IN OUT	tPOCTRPC_OGRDTP_HEADER	f_pOgrdtpHeader,
				IN		UINT32					f_ulPktLengthDword )
{
	PUINT32	pulPktPayload;
	UINT32	ulBytePositionW = cOCT6100_INVALID_VALUE;
	UINT32	ulBytePositionX = cOCT6100_INVALID_VALUE;
	UINT32	ulBytePositionY = cOCT6100_INVALID_VALUE;
	UINT32	ulBytePositionZ = cOCT6100_INVALID_VALUE;
	UINT32	ulTempVar;
	UINT32	i;

	/* Bytes in dword are labeled as Z Y X W. */

	/* Only swap if necessary. */
	if ( f_pOgrdtpHeader->ulEndianDetect != cOCTRPC_ENDIAN_DETECT )
	{
		/* Find the position of each byte. */
		for ( i = 0; i < 4; i++ )
		{
			ulTempVar = (f_pOgrdtpHeader->ulEndianDetect >> (8 * i)) & 0xFF;
			switch ( ulTempVar )
			{
			case cOCTRPC_ENDIAN_DETECT_BYTE_W:	
				ulBytePositionW = i * 8;	
				break;
			case cOCTRPC_ENDIAN_DETECT_BYTE_X:	
				ulBytePositionX = i * 8;	
				break;
			case cOCTRPC_ENDIAN_DETECT_BYTE_Y:	
				ulBytePositionY = i * 8;	
				break;
			case cOCTRPC_ENDIAN_DETECT_BYTE_Z:	
				ulBytePositionZ = i * 8;	
				break;
			default:	
				return cOCT6100_ERR_REMOTEDEBUG_INVALID_PACKET;
			}
		}

		/* Make sure all bytes of the endian detect field were found. */
		if ( ulBytePositionW == cOCT6100_INVALID_VALUE ||
			 ulBytePositionX == cOCT6100_INVALID_VALUE ||
			 ulBytePositionY == cOCT6100_INVALID_VALUE ||
			 ulBytePositionZ == cOCT6100_INVALID_VALUE )
			 return cOCT6100_ERR_REMOTEDEBUG_INVALID_PACKET;

		/* Swap the bytes of each dword of the packet. */
		pulPktPayload = ( PUINT32 )f_pOgrdtpHeader;
		for ( i = 0; i < f_ulPktLengthDword; i++ )
		{
			ulTempVar = pulPktPayload[ i ];
			pulPktPayload[ i ] = ((ulTempVar >> ulBytePositionZ) & 0xFF) << 24;
			pulPktPayload[ i ] |= ((ulTempVar >> ulBytePositionY) & 0xFF) << 16;
			pulPktPayload[ i ] |= ((ulTempVar >> ulBytePositionX) & 0xFF) << 8;
			pulPktPayload[ i ] |= ((ulTempVar >> ulBytePositionW) & 0xFF) << 0;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCalculateChecksum

Description:    Calculates the checksum of the given packet payload.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pulPktPayload			Pointer to the payload of the packet.
f_ulPktLengthDword		Length of the packet in dwords.
f_pulChecksum			Pointer to the checksum of the packet.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCalculateChecksum
VOID Oct6100ApiCalculateChecksum(
				IN		PUINT32		f_pulPktPayload,
				IN		UINT32		f_ulPktLengthDword,
				OUT		PUINT32		f_pulChecksum )
{
	tPOCTRPC_OGRDTP_HEADER	pOgrdtpHeader;
	UINT32	i;

	for ( i = 0, *f_pulChecksum = 0; i < f_ulPktLengthDword; i++ )
	{
		*f_pulChecksum += (f_pulPktPayload[ i ] >> 16) & 0xFFFF;
		*f_pulChecksum += (f_pulPktPayload[ i ] >> 0) & 0xFFFF;
	}

	pOgrdtpHeader = ( tPOCTRPC_OGRDTP_HEADER )f_pulPktPayload;
	*f_pulChecksum -= (pOgrdtpHeader->ulChecksum >> 16) & 0xFFFF;
	*f_pulChecksum -= (pOgrdtpHeader->ulChecksum >> 0) & 0xFFFF;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiFormResponsePkt

Description:    Modifies the values of the indicated receive packet, update
				the checksum field, and copy the receive packet to the
				response packet.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pulRcvPktPayload			Pointer to the payload of the received packet.
f_pulRspPktPayload			Pointer to the payload of the response packet.
f_ulPktLengthDword			Length of the packet in dwords.
f_fRetryPktResponse			Flag indicating if the received packet was a retry packet.
f_fReplaceProtocolNum		Flag indicating if the protocol number must be replaced.
f_fReplaceInterfaceType		Flag indicating if the interface type must be replaced.
f_fReplaceInterfaceVersion	Flag indicating if the interface version must be replaced.
f_ulSessionIndex			Index of the remote debug session within the API' session list.
f_ulParsingErrorValue		Parsing error value.
f_ulPayloadDwordIndex		Index in the packet where the payload starts.
f_ulChecksum				Checksum of the packet.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiFormResponsePkt
VOID Oct6100ApiFormResponsePkt(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		PUINT32						f_pulRcvPktPayload,
				OUT		PUINT32						f_pulRspPktPayload,
				IN		UINT32						f_ulPktLengthDword,
				IN		BOOL						f_fRetryPktResponse,
				IN		BOOL						f_fReplaceProtocolNum,
				IN		BOOL						f_fReplaceInterfaceType,
				IN		BOOL						f_fReplaceInterfaceVersion,
				IN		UINT32						f_ulSessionIndex,
				IN		UINT32						f_ulParsingErrorValue,
				IN		UINT32						f_ulPayloadDwordIndex,
				IN		UINT32						f_ulChecksum )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCTRPC_OGRDTP_HEADER		pOgrdtpHeader;
	tPOCTRPC_INTERFACE_HEADER	pInterfaceHeader;
	PUINT32		pulPktCache;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Typecast pointer to OGRDTP packet header. */
	pOgrdtpHeader = ( tPOCTRPC_OGRDTP_HEADER )f_pulRcvPktPayload;

	/* Check if a response to a retry packet. */
	if ( f_fRetryPktResponse == TRUE )
	{
		mOCT6100_GET_REMOTE_DEBUG_SESSION_PKT_CACHE_PNT( pSharedInfo, pulPktCache, f_ulSessionIndex )

		Oct6100UserMemCopy( f_pulRspPktPayload, pulPktCache, f_ulPktLengthDword * 4 );
		return;
	}

	/* Replace all packet header fields which must be changed. */
	if ( f_ulParsingErrorValue != cOCT6100_INVALID_VALUE )
	{
		f_ulChecksum -= (pOgrdtpHeader->ulParsingError >> 16) & 0xFFFF;
		f_ulChecksum -= (pOgrdtpHeader->ulParsingError >> 0) & 0xFFFF;

		pOgrdtpHeader->ulParsingError = f_ulParsingErrorValue;
		
		f_ulChecksum += (pOgrdtpHeader->ulParsingError >> 16) & 0xFFFF;
		f_ulChecksum += (pOgrdtpHeader->ulParsingError >> 0) & 0xFFFF;
	}

	if ( f_fReplaceProtocolNum == TRUE )
	{
		f_ulChecksum -= (pOgrdtpHeader->ulRpcProtocolNum >> 16) & 0xFFFF;
		f_ulChecksum -= (pOgrdtpHeader->ulRpcProtocolNum >> 0) & 0xFFFF;

		pOgrdtpHeader->ulRpcProtocolNum = cOCTRPC_PROTOCOL_V1_1;
		
		f_ulChecksum += (pOgrdtpHeader->ulRpcProtocolNum >> 16) & 0xFFFF;
		f_ulChecksum += (pOgrdtpHeader->ulRpcProtocolNum >> 0) & 0xFFFF;
	}

	if ( f_fReplaceInterfaceType == TRUE )
	{
		pInterfaceHeader = ( tPOCTRPC_INTERFACE_HEADER )(f_pulRcvPktPayload + (sizeof( tOCTRPC_OGRDTP_HEADER ) / 4));

		f_ulChecksum -= (pInterfaceHeader->ulInterfaceType >> 16) & 0xFFFF;
		f_ulChecksum -= (pInterfaceHeader->ulInterfaceType >> 0) & 0xFFFF;

		pInterfaceHeader->ulInterfaceType = cOCTRPC_OCT6100_INTERFACE;
		
		f_ulChecksum += (pInterfaceHeader->ulInterfaceType >> 16) & 0xFFFF;
		f_ulChecksum += (pInterfaceHeader->ulInterfaceType >> 0) & 0xFFFF;
	}

	if ( f_fReplaceInterfaceVersion == TRUE )
	{
		pInterfaceHeader = ( tPOCTRPC_INTERFACE_HEADER )(f_pulRcvPktPayload + (sizeof( tOCTRPC_OGRDTP_HEADER ) / 4));

		f_ulChecksum -= (pInterfaceHeader->ulInterfaceVersion >> 16) & 0xFFFF;
		f_ulChecksum -= (pInterfaceHeader->ulInterfaceVersion >> 0) & 0xFFFF;

		pInterfaceHeader->ulInterfaceVersion = cOCTRPC_INTERFACE_VERSION;
		
		f_ulChecksum += (pInterfaceHeader->ulInterfaceVersion >> 16) & 0xFFFF;
		f_ulChecksum += (pInterfaceHeader->ulInterfaceVersion >> 0) & 0xFFFF;
	}

	if ( f_ulPayloadDwordIndex != cOCT6100_INVALID_VALUE )
	{
		f_pulRcvPktPayload += f_ulPayloadDwordIndex;

		f_ulChecksum -= (*f_pulRcvPktPayload >> 16) & 0xFFFF;
		f_ulChecksum -= (*f_pulRcvPktPayload >> 0) & 0xFFFF;

		*f_pulRcvPktPayload = cOCTRPC_UNKNOWN_COMMAND_NUM;
		
		f_ulChecksum += (*f_pulRcvPktPayload >> 16) & 0xFFFF;
		f_ulChecksum += (*f_pulRcvPktPayload >> 0) & 0xFFFF;

		f_pulRcvPktPayload -= f_ulPayloadDwordIndex;
	}

	/* Replace checksum. */
	pOgrdtpHeader->ulChecksum = f_ulChecksum;

	/* Copy the modified receive packet payload to the response packet. */
	Oct6100UserMemCopy( f_pulRspPktPayload, f_pulRcvPktPayload, f_ulPktLengthDword * 4 );

	/* Copy the response packet to the session's packet cache. */
	if ( f_ulSessionIndex != cOCT6100_INVALID_VALUE )
	{
		mOCT6100_GET_REMOTE_DEBUG_SESSION_PKT_CACHE_PNT( pSharedInfo, pulPktCache, f_ulSessionIndex )

		Oct6100UserMemCopy( pulPktCache, f_pulRspPktPayload, f_ulPktLengthDword * 4 );
	}
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckPktCommands

Description:    Checks the commands contained in the packet for errors in size.
				Also checks for unknown commands.  If an error is encountered
				then the function will construct the response packet.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pulRcvPktPayload			Pointer to the payload of the received packet.
f_pulRspPktPayload			Pointer to the payload of the response packet.
f_ulPktLengthDword			Length of the packet in dwords.
f_ulSessionIndex			Index of the remote debug session within the API' session list.
f_ulChecksum				Checksum of the packet.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckPktCommands
UINT32 Oct6100ApiCheckPktCommands(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		PUINT32	f_pulRcvPktPayload,
				IN OUT	PUINT32	f_pulRspPktPayload,
				IN		UINT32	f_ulSessionIndex,
				IN		UINT32	f_ulPktLengthDword,
				IN		UINT32	f_ulChecksum )
{
	tPOCTRPC_COMMAND_HEADER	pCmndHeader;
	UINT32	ulNumDwordsLeft;
	UINT32	ulNumDwordsNeeded = 0;
	UINT32	ulRpcCmndSizeDword;
	BOOL	fCmndIdentified;
	BOOL	fCmndHeaderPresent;

	pCmndHeader = ( tPOCTRPC_COMMAND_HEADER )(f_pulRcvPktPayload + ((sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER )) / 4));
	ulNumDwordsLeft = f_ulPktLengthDword - ((sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER )) / 4);
	ulRpcCmndSizeDword = sizeof( tOCTRPC_COMMAND_HEADER ) / 4;
	fCmndIdentified = TRUE;

	while ( ulNumDwordsLeft != 0 )
	{
		if ( ulNumDwordsLeft < ulRpcCmndSizeDword )
		{
			fCmndHeaderPresent = FALSE;
		}
		else
		{
			fCmndHeaderPresent = TRUE;

			switch ( pCmndHeader->ulRpcCommandNum )
			{
			case cOCT6100_RPC_READ_WORD:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_READ_WORD ) / 4;
				}
			break;
			case cOCT6100_RPC_READ_BURST:
				{
					tPOCT6100_RPC_READ_BURST	pBurstHeader;

					ulNumDwordsNeeded = (sizeof( tOCT6100_RPC_READ_BURST ) - sizeof( UINT32 )) / 4;
					pBurstHeader = ( tPOCT6100_RPC_READ_BURST )(( PUINT32 )pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));
					ulNumDwordsNeeded += (pBurstHeader->ulBurstLength + 1) / 2;
				}
			break;
			case cOCT6100_RPC_WRITE_WORD:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_WRITE_WORD ) / 4;
				}
			break;
			case cOCT6100_RPC_WRITE_SMEAR:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_WRITE_SMEAR ) / 4;
				}
			break;
			case cOCT6100_RPC_WRITE_INC:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_WRITE_INC ) / 4;
				}
			break;
			case cOCT6100_RPC_READ_ARRAY:
				{
					tPOCT6100_RPC_READ_ARRAY	pArrayHeader;

					ulNumDwordsNeeded = (sizeof( tOCT6100_RPC_READ_ARRAY ) - sizeof( UINT32 )) / 4;
					pArrayHeader = ( tPOCT6100_RPC_READ_ARRAY )(( PUINT32 )pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));
					ulNumDwordsNeeded += pArrayHeader->ulArrayLength;
					ulNumDwordsNeeded += (pArrayHeader->ulArrayLength + 1) / 2;
				}
			break;
			case cOCT6100_RPC_WRITE_BURST:
				{
					tPOCT6100_RPC_WRITE_BURST	pBurstHeader;

					ulNumDwordsNeeded = (sizeof( tOCT6100_RPC_WRITE_BURST ) - sizeof( UINT32 )) / 4;
					pBurstHeader = ( tPOCT6100_RPC_WRITE_BURST )(( PUINT32 )pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));
					ulNumDwordsNeeded += (pBurstHeader->ulBurstLength + 1) / 2;
				}
			break;
			case cOCT6100_RPC_SET_HOT_CHANNEL:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_SET_HOT_CHANNEL ) / 4;
				}
			break;
			case cOCT6100_RPC_GET_DEBUG_CHAN_INDEX:
				{
					ulNumDwordsNeeded = sizeof( tOCT6100_RPC_GET_DEBUG_CHAN_INDEX ) / 4;
				}
			break;
			case cOCT6100_RPC_API_DISCONNECT:
				{
					/* There is no parameter to the disconnect command. */
					ulNumDwordsNeeded = 0;
				}
			break;
			default:
				fCmndIdentified = FALSE;
			}

			ulNumDwordsNeeded += sizeof( tOCTRPC_COMMAND_HEADER ) / 4;
		}

		if ( fCmndHeaderPresent != TRUE )
		{
			Oct6100ApiFormResponsePkt( f_pApiInstance, f_pulRcvPktPayload, f_pulRspPktPayload, f_ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, f_ulSessionIndex, cOCTRPC_RDBGERR_INVALID_PACKET_LENGTH, cOCT6100_INVALID_VALUE, f_ulChecksum );
			return cOCT6100_ERR_REMOTE_DEBUG_PARSING_ERROR;
		}
		if ( fCmndIdentified != TRUE )
		{
			Oct6100ApiFormResponsePkt( f_pApiInstance, f_pulRcvPktPayload, f_pulRspPktPayload, f_ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, f_ulSessionIndex, cOCTRPC_RDBGERR_INVALID_COMMAND_NUMBER, f_ulPktLengthDword - ulNumDwordsLeft, f_ulChecksum );
			return cOCT6100_ERR_REMOTE_DEBUG_PARSING_ERROR;
		}

		if ( ulNumDwordsNeeded != (pCmndHeader->ulCommandByteSize / 4) ||
			 ulNumDwordsNeeded > ulNumDwordsLeft )
		{
			Oct6100ApiFormResponsePkt( f_pApiInstance, f_pulRcvPktPayload, f_pulRspPktPayload, f_ulPktLengthDword, FALSE, FALSE, FALSE, FALSE, f_ulSessionIndex, cOCTRPC_RDBGERR_INVALID_COMMAND_LENGTH, cOCT6100_INVALID_VALUE, f_ulChecksum );
			return cOCT6100_ERR_REMOTE_DEBUG_PARSING_ERROR;
		}

		pCmndHeader = ( tPOCTRPC_COMMAND_HEADER )(( PUINT32 )pCmndHeader + ulNumDwordsNeeded);
		ulNumDwordsLeft -= ulNumDwordsNeeded;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiExecutePktCommands

Description:    Executes the commands contained in the received packet.  The
				received packet payload buffer is modified but NOT copied to
				the response packet buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pulRcvPktPayload		Pointer to the payload of the received packet.
f_ulPktLengthDword		Length of the packet in dwords.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiExecutePktCommands
VOID Oct6100ApiExecutePktCommands(
				IN		tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		PUINT32					f_pulRcvPktPayload,
				IN		UINT32					f_ulPktLengthDword )
{
	tPOCTRPC_COMMAND_HEADER	pReqCmndHeader;
	tPOCTRPC_OGRDTP_HEADER	pReqPktHeader;
	UINT32	ulNumDwordsLeft;
	UINT32	ulRpcCmndSizeDword;

	pReqPktHeader = ( tPOCTRPC_OGRDTP_HEADER )(f_pulRcvPktPayload);
	pReqCmndHeader = ( tPOCTRPC_COMMAND_HEADER )(( PUINT32 )f_pulRcvPktPayload + ((sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER )) / 4));
	ulNumDwordsLeft = f_ulPktLengthDword - ((sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER )) / 4);
	ulRpcCmndSizeDword = sizeof( tOCTRPC_COMMAND_HEADER ) / 4;

	while ( ulNumDwordsLeft != 0 )
	{
		/* Switch on command number. */
		switch ( pReqCmndHeader->ulRpcCommandNum )
		{
		case cOCT6100_RPC_READ_WORD:
			Oct6100ApiRpcReadWord( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_READ_BURST:
			Oct6100ApiRpcReadBurst( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_READ_ARRAY:
			Oct6100ApiRpcReadArray( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_WRITE_WORD:
			Oct6100ApiRpcWriteWord( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_WRITE_SMEAR:
			Oct6100ApiRpcWriteSmear( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_WRITE_BURST:
			Oct6100ApiRpcWriteBurst( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_SET_HOT_CHANNEL:
			Oct6100ApiRpcSetHotChannel( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_GET_DEBUG_CHAN_INDEX:
			Oct6100ApiRpcGetDebugChanIndex( f_pApiInstance, pReqCmndHeader );
		break;
		case cOCT6100_RPC_API_DISCONNECT:
			Oct6100ApiRpcDisconnect( f_pApiInstance, pReqCmndHeader, pReqPktHeader->ulDebugSessionNum );
		break;
		default:
			pReqCmndHeader->ulFunctionResult = cOCT6100_ERR_REMOTEDEBUG_INVALID_RPC_COMMAND_NUM;
		break;
		}

		/* Insert the result of the operation in the command header. */
		if ( pReqCmndHeader->ulFunctionResult != cOCT6100_ERR_OK )
			break;

		/* Decrement the number of DWORDs left in the packet. */
		ulNumDwordsLeft -= pReqCmndHeader->ulCommandByteSize / 4;

		/* Point to the next command in the packet. */
		pReqCmndHeader = ( tPOCTRPC_COMMAND_HEADER )(( PUINT32 )pReqCmndHeader + (pReqCmndHeader->ulCommandByteSize / 4));
	}
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckSessionNum

Description:    Checks if there is a session list entry open for the session
				number received.  If not, a free one is reserved if one is
				available.  If none are free, one which has timed-out is
				released.  If none are timed out then an error is returned.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pOgrdtpHeader			Pointer to the header of the packet.
f_pulSessionIndex		Pointer to the remote debugging session within the 
						API's session list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckSessionNum
UINT32 Oct6100ApiCheckSessionNum(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		tPOCTRPC_OGRDTP_HEADER		f_pOgrdtpHeader,
				OUT		PUINT32						f_pulSessionIndex )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_REMOTE_DEBUG_INFO		pRemoteDebugInfo;
	tPOCT6100_API_REMOTE_DEBUG_SESSION	pSessionEntry;
	tPOCT6100_API_REMOTE_DEBUG_SESSION	pSessionLink;
	tOCT6100_GET_TIME					GetTime;
	PVOID	pSessionTree;
	PUINT32	pulTreeData;
	UINT32	ulNewSessionIndex;
	UINT32	aulTimeDiff[ 2 ];
	UINT32	ulResult;
	UINT16	usNegative;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context of GetTime. */
	GetTime.pProcessContext = f_pApiInstance->pProcessContext;

	/* Get the current system time. */
	ulResult = Oct6100UserGetTime( &GetTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Get a local pointer to the remote debugging info. */
	pRemoteDebugInfo = &pSharedInfo->RemoteDebugInfo;

	/* Check if the session number has an associated session list entry. */
	mOCT6100_GET_REMOTE_DEBUG_TREE_PNT( pSharedInfo, pSessionTree )

	ulResult = octapi_bt0_query_node( pSessionTree, ( ( PVOID )(&f_pOgrdtpHeader->ulDebugSessionNum) ), ( ( PVOID* )&pulTreeData ) );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		/* Return session index. */
		*f_pulSessionIndex = *pulTreeData;

		/* A session list entry is associated, so update the entries last packet time,
			transaction number and packet retry number, and position in the linked list. */
		mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, *pulTreeData, pSessionEntry )

		pSessionEntry->aulLastPktTime[ 0 ] = GetTime.aulWallTimeUs[ 0 ];
		pSessionEntry->aulLastPktTime[ 1 ] = GetTime.aulWallTimeUs[ 1 ];
		pSessionEntry->ulPktRetryNum = f_pOgrdtpHeader->ulPktRetryNum;

		/* Remove the node from its current place in the linked-list and add it to the end. */
		if ( pRemoteDebugInfo->ulSessionListTail != *pulTreeData )
		{
			/* Obtain local pointer to the session list entry to be moved. */
			mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, *pulTreeData, pSessionEntry )
			
			/* Update link of previous session in list. */
			if ( pSessionEntry->ulBackwardLink != cOCT6100_INVALID_VALUE )
			{
				mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulBackwardLink, pSessionLink )
				pSessionLink->ulForwardLink = pSessionEntry->ulForwardLink;
			}
			else
			{
				pRemoteDebugInfo->ulSessionListHead = pSessionEntry->ulForwardLink;
			}

			/* Update link of next session in list. */
			if ( pSessionEntry->ulForwardLink != cOCT6100_INVALID_VALUE )
			{
				mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulForwardLink, pSessionLink )
				pSessionLink->ulBackwardLink = pSessionEntry->ulBackwardLink;
			}
			else
			{
				pRemoteDebugInfo->ulSessionListTail = pSessionEntry->ulBackwardLink;
			}

			/* Place session at the end of the list. */
			pSessionEntry->ulBackwardLink = pRemoteDebugInfo->ulSessionListTail;
			pSessionEntry->ulForwardLink = cOCT6100_INVALID_VALUE;

			pRemoteDebugInfo->ulSessionListTail = *pulTreeData;

			if ( pRemoteDebugInfo->ulSessionListHead == cOCT6100_INVALID_VALUE )
			{
				pRemoteDebugInfo->ulSessionListHead = *pulTreeData;
			}
			else
			{
				mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulBackwardLink, pSessionLink )
				pSessionLink->ulForwardLink = *pulTreeData;
			}
		}

		/* Check if packet should be interpreted based on transaction number. */
		if ( f_pOgrdtpHeader->ulPktRetryNum != 0 &&
			 pSessionEntry->ulTransactionNum == f_pOgrdtpHeader->ulTransactionNum &&
			 pSessionEntry->ulPktByteSize == f_pOgrdtpHeader->ulPktByteSize )
			return cOCT6100_ERR_REMOTEDEBUG_TRANSACTION_ANSWERED;

		/* Update transaction number since packet will be interpreted. */
		pSessionEntry->ulTransactionNum = f_pOgrdtpHeader->ulTransactionNum;
		pSessionEntry->ulPktByteSize = f_pOgrdtpHeader->ulPktByteSize;

		return cOCT6100_ERR_OK;
	}
	else if ( ulResult == OCTAPI_BT0_KEY_NOT_IN_TREE )
	{
		/* If there is a free entry in the session list then seize it.  Else, try to
			find an entry which has timed out.  If there are none then return an error. */
		if ( pRemoteDebugInfo->ulNumSessionsOpen < pRemoteDebugInfo->ulMaxSessionsOpen )
		{
			ulNewSessionIndex = pRemoteDebugInfo->ulNumSessionsOpen;
		}
		else /* ( pRemoteDebugInfo->ulNumSessionsOpen == pRemoteDebugInfo->ulMaxSessionsOpen ) */
		{
			mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pRemoteDebugInfo->ulSessionListHead, pSessionEntry )
			
			ulResult = octapi_lm_subtract( GetTime.aulWallTimeUs, 1, pSessionEntry->aulLastPktTime, 1, aulTimeDiff, 1, &usNegative );
			if ( ulResult != cOCT6100_ERR_OK || usNegative != FALSE )
				return cOCT6100_ERR_FATAL_43;

			/* If there are no session list entries available then return the packet with
				a parsing error. */
			if ( aulTimeDiff[ 1 ] == 0 && aulTimeDiff[ 0 ] < (cOCTRPC_SESSION_TIMEOUT * 1000000) )
				return cOCT6100_ERR_REMOTEDEBUG_ALL_SESSIONS_OPEN;

			ulNewSessionIndex = pRemoteDebugInfo->ulSessionListHead;

			/* Remove old session index. */
			ulResult = octapi_bt0_remove_node( pSessionTree, ( ( PVOID )&pSessionEntry->ulSessionNum ) );
			if ( ulResult != cOCT6100_ERR_OK )
				return cOCT6100_ERR_FATAL_44;

			if ( pSessionEntry->ulBackwardLink != cOCT6100_INVALID_VALUE )
			{
				mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulBackwardLink, pSessionLink )
				pSessionLink->ulForwardLink = pSessionEntry->ulForwardLink;
			}
			else
			{
				pRemoteDebugInfo->ulSessionListHead = pSessionEntry->ulForwardLink;
			}

			if ( pSessionEntry->ulForwardLink != cOCT6100_INVALID_VALUE )
			{
				mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulForwardLink, pSessionLink )
				pSessionLink->ulBackwardLink = pSessionEntry->ulBackwardLink;
			}
			else
			{
				pRemoteDebugInfo->ulSessionListTail = pSessionEntry->ulBackwardLink;
			}

			/* Decrement number of open sessions. */
			pRemoteDebugInfo->ulNumSessionsOpen--;
		}

		/* Add new session. */
		ulResult = octapi_bt0_add_node( pSessionTree, ( ( PVOID )&f_pOgrdtpHeader->ulDebugSessionNum ), ( ( PVOID* )&pulTreeData ) );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_45;
		*pulTreeData = ulNewSessionIndex;

		mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, ulNewSessionIndex, pSessionEntry )

		pSessionEntry->aulLastPktTime[ 0 ] = GetTime.aulWallTimeUs[ 0 ];
		pSessionEntry->aulLastPktTime[ 1 ] = GetTime.aulWallTimeUs[ 1 ];
		pSessionEntry->ulSessionNum = f_pOgrdtpHeader->ulDebugSessionNum;
		pSessionEntry->ulTransactionNum = f_pOgrdtpHeader->ulTransactionNum;
		pSessionEntry->ulPktRetryNum = f_pOgrdtpHeader->ulPktRetryNum;

		pSessionEntry->ulBackwardLink = pRemoteDebugInfo->ulSessionListTail;
		pSessionEntry->ulForwardLink = cOCT6100_INVALID_VALUE;

		pRemoteDebugInfo->ulSessionListTail = ulNewSessionIndex;
		if ( pRemoteDebugInfo->ulSessionListHead == cOCT6100_INVALID_VALUE )
			pRemoteDebugInfo->ulSessionListHead = ulNewSessionIndex;

		if ( pSessionEntry->ulBackwardLink != cOCT6100_INVALID_VALUE )
		{
			mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulBackwardLink, pSessionLink )
			pSessionLink->ulForwardLink = ulNewSessionIndex;
		}

		*f_pulSessionIndex = ulNewSessionIndex;

		/* Increment number of open sessions. */
		pRemoteDebugInfo->ulNumSessionsOpen++;

		return cOCT6100_ERR_OK;
	}
	else
	{
		return cOCT6100_ERR_FATAL_46;
	}
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcReadWord

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_READ_WORD command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcReadWord
VOID Oct6100ApiRpcReadWord(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_READ_WORD	pReadCommand;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32					ulResult;
	UINT16					usReadData;

	/* Get pointer to command arguments. */
	pReadCommand = ( tPOCT6100_RPC_READ_WORD )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set some read structure parameters. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy parameters from packet payload to local read structure. */
	ReadParams.ulReadAddress = pReadCommand->ulAddress;

	/* Supply memory for read data. */
	ReadParams.pusReadData = &usReadData;

	/* Perform read access. */
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		
	usReadData &= 0xFFFF;
	
	/* Return read data and result. */
	pReadCommand->ulReadData = (usReadData << 16) | usReadData;
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcReadBurst

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_READ_BURST command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcReadBurst
VOID Oct6100ApiRpcReadBurst(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_READ_BURST	pBurstCommand;
	tOCT6100_READ_BURST_PARAMS	BurstParams;
	UINT32						ulResult = cOCT6100_ERR_OK;
	UINT32						ulTempVar;
	UINT32						i;
	PUINT16						pusReadData;
	UINT32						ulNumWordsToRead;

	/* Get local pointer to remote debugging read data buffer. */
	mOCT6100_GET_REMOTE_DEBUG_DATA_BUF_PNT( f_pApiInstance->pSharedInfo, pusReadData )

	/* Get pointer to command arguments. */
	pBurstCommand = ( tPOCT6100_RPC_READ_BURST )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set some read structure parameters. */
	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy parameters from packet payload to local read structure. */
	BurstParams.ulReadAddress = pBurstCommand->ulAddress;
	

	/* Supply memory for read data. */
	BurstParams.pusReadData = pusReadData;

	ulNumWordsToRead = pBurstCommand->ulBurstLength; 
	while( ulNumWordsToRead > 0)
	{
		if ( ulNumWordsToRead <= f_pApiInstance->pSharedInfo->ChipConfig.usMaxRwAccesses )
			BurstParams.ulReadLength = ulNumWordsToRead;
		else
			BurstParams.ulReadLength = f_pApiInstance->pSharedInfo->ChipConfig.usMaxRwAccesses;

		/* Perform read access. */
		mOCT6100_DRIVER_READ_BURST_API( BurstParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
		{
			f_pCmndHeader->ulFunctionResult = ulResult;
			return;
		}

		BurstParams.ulReadAddress	+= BurstParams.ulReadLength * 2;
		BurstParams.pusReadData		+= BurstParams.ulReadLength;

		/* Update the number of dword to read. */
		ulNumWordsToRead	-= BurstParams.ulReadLength;
	}

	/* Return read data. */
	ulTempVar = (pBurstCommand->ulBurstLength + 1) / 2;
	for ( i = 0; i < ulTempVar; i++ )
	{
		pBurstCommand->aulReadData[ i ] = (*pusReadData & 0xFFFF) << 16;
		pusReadData++;
		pBurstCommand->aulReadData[ i ] |= (*pusReadData & 0xFFFF) << 0;
		pusReadData++;
	}

	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcReadArray

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_READ_ARRAY command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcReadArray
VOID Oct6100ApiRpcReadArray(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_READ_ARRAY	pArrayCommand;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32						ulResult = cOCT6100_ERR_OK;
	UINT32						i;
	PUINT32						pulAddressArray;
	PUINT32						pulDataArray;
	UINT16						usReadData;


	/* Get pointer to command arguments. */
	pArrayCommand = ( tPOCT6100_RPC_READ_ARRAY )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set some read structure parameters. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Supply memory for read data. */
	ReadParams.pusReadData = &usReadData;

	/* Get pointers to array of addresses and data. */
	pulAddressArray = pArrayCommand->aulArrayData;
	pulDataArray = pArrayCommand->aulArrayData + pArrayCommand->ulArrayLength;

	for ( i = 0; i < pArrayCommand->ulArrayLength; i++ )
	{
		/* Copy parameters from packet payload to local read structure. */
		ReadParams.ulReadAddress = pulAddressArray[ i ];

		/* Perform read access. */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			break;

		/* Return read data. */
		if ( (i % 2) == 0 )
			pulDataArray[ i / 2 ] = (usReadData & 0xFFFF) << 16;
		else /* ( (i % 2) == 1 ) */
			pulDataArray[ i / 2 ] |= (usReadData & 0xFFFF) << 0;
	}

	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcWriteWord

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_WRITE_WORD command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcWriteWord
VOID Oct6100ApiRpcWriteWord(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_WRITE_WORD	pWriteCommand;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						ulResult;

	/* Get pointer to command arguments. */
	pWriteCommand = ( tPOCT6100_RPC_WRITE_WORD )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set some read structure parameters. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy parameters from packet payload to local read structure. */
	WriteParams.ulWriteAddress = pWriteCommand->ulAddress;
	WriteParams.usWriteData = (UINT16)pWriteCommand->ulWriteData;

	/* Perform write access. */
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	
	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcWriteSmear

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_WRITE_SMEAR command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcWriteSmear
VOID Oct6100ApiRpcWriteSmear(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_WRITE_SMEAR	pSmearCommand;
	tOCT6100_WRITE_SMEAR_PARAMS	SmearParams;
	UINT32						ulResult;

	/* Get pointer to command arguments. */
	pSmearCommand = ( tPOCT6100_RPC_WRITE_SMEAR )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set the smear structure parameters. */
	SmearParams.pProcessContext = f_pApiInstance->pProcessContext;

	SmearParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy parameters from packet payload to local read structure. */
	SmearParams.ulWriteAddress = pSmearCommand->ulAddress;
	SmearParams.usWriteData = (UINT16)pSmearCommand->ulWriteData;
	SmearParams.ulWriteLength = pSmearCommand->ulSmearLength;

	/* Perform write access. */
	mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult )

	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcWriteBurst

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_WRITE_BURST command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcWriteBurst
VOID Oct6100ApiRpcWriteBurst(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_WRITE_BURST	pBurstCommand;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	UINT32						ulResult;
	UINT32						ulTempVar;
	UINT32						i, j;
	PUINT16						pusWriteData;

	/* Get local pointer to remote debugging write data buffer. */
	mOCT6100_GET_REMOTE_DEBUG_DATA_BUF_PNT( f_pApiInstance->pSharedInfo, pusWriteData )

	/* Get pointer to command arguments. */
	pBurstCommand = ( tPOCT6100_RPC_WRITE_BURST )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	ulTempVar = (pBurstCommand->ulBurstLength + 1) / 2;
	for ( i = 0, j = 0; i < ulTempVar; i++ )
	{
		pusWriteData[ j++ ] = (UINT16)((pBurstCommand->aulWriteData[ i ] >> 16) & 0xFFFF);
		pusWriteData[ j++ ] = (UINT16)((pBurstCommand->aulWriteData[ i ] >> 0) & 0xFFFF);
	}

	/* Set some structure parameters. */
	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy parameters from packet payload to local read structure. */
	BurstParams.ulWriteAddress = pBurstCommand->ulAddress;
	BurstParams.ulWriteLength = pBurstCommand->ulBurstLength;
	BurstParams.pusWriteData = pusWriteData;

	/* Perform write access. */
	mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult )

	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcSetHotChannel

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_SET_HOT_CHANNEL command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcSetHotChannel
VOID Oct6100ApiRpcSetHotChannel(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_SET_HOT_CHANNEL	pHotChanCommand;
	tOCT6100_DEBUG_SELECT_CHANNEL	DebugSelectChannel;
	tPOCT6100_API_CHANNEL			pChanEntry;
	UINT32							ulResult;

	pHotChanCommand = ( tPOCT6100_RPC_SET_HOT_CHANNEL )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Verify if the hot channel index is valid. */
	if ( pHotChanCommand->ulHotChannel >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
	{
		f_pCmndHeader->ulFunctionResult = cOCT6100_ERR_REMOTEDEBUG_INVALID_HOT_CHAN_INDEX;
		return;
	}

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, pHotChanCommand->ulHotChannel );

	DebugSelectChannel.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | (pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | pHotChanCommand->ulHotChannel;

	/* The PCM law parameter is now obsolete. */
	/* The instance knows the law of the channel being recorded! */

	/* Call the function. */
	ulResult = Oct6100DebugSelectChannelSer( f_pApiInstance, &DebugSelectChannel, FALSE );
	
	/* Return result. */
	f_pCmndHeader->ulFunctionResult = ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcGetDebugChanIndex

Description:    Checks the provided portion of an OCTRPC packet and interprets
				it as an cOCT6100_RPC_GET_DEBUG_CHAN_INDEX command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcGetDebugChanIndex
VOID Oct6100ApiRpcGetDebugChanIndex(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader )
{
	tPOCT6100_RPC_GET_DEBUG_CHAN_INDEX	pDebugChanCommand;

	pDebugChanCommand = ( tPOCT6100_RPC_GET_DEBUG_CHAN_INDEX )(( PUINT32 )f_pCmndHeader + (sizeof( tOCTRPC_COMMAND_HEADER ) / 4));

	/* Set the debug channel index of the structure. */
	pDebugChanCommand->ulDebugChanIndex = f_pApiInstance->pSharedInfo->DebugInfo.usRecordMemIndex;

	/* Return result. */
	f_pCmndHeader->ulFunctionResult = cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRpcDisconnect

Description:    Destroy the current session.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCmndHeader			Pointer to RPC command structure.
f_ulSessionNumber		Session number of the current remote debugging session.
	
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRpcDisconnect
VOID Oct6100ApiRpcDisconnect(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCTRPC_COMMAND_HEADER		f_pCmndHeader,
				IN		UINT32						f_ulSessionNumber )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_REMOTE_DEBUG_INFO		pRemoteDebugInfo;
	tPOCT6100_API_REMOTE_DEBUG_SESSION	pSessionEntry;
	tPOCT6100_API_REMOTE_DEBUG_SESSION	pSessionTempEntry;
	PVOID						pSessionTree;
	UINT32						ulResult;
	PUINT32						pulTreeData;
	UINT32						ulSessionIndex;
	
	f_pCmndHeader->ulFunctionResult = cOCT6100_ERR_OK;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get a local pointer to the remote debugging info. */
	pRemoteDebugInfo = &pSharedInfo->RemoteDebugInfo;

	/* Check if the session number has an associated session list entry. */
	mOCT6100_GET_REMOTE_DEBUG_TREE_PNT( pSharedInfo, pSessionTree )

	ulResult = octapi_bt0_query_node( pSessionTree, ( ( PVOID )(&f_ulSessionNumber) ), ( ( PVOID* )&pulTreeData ) );
	if ( ulResult != cOCT6100_ERR_OK )
		f_pCmndHeader->ulFunctionResult = cOCT6100_ERR_REMOTEDEBUG_INAVLID_SESSION_NUMBER;

	/* Return session index. */
	ulSessionIndex= *pulTreeData;

	mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, ulSessionIndex, pSessionEntry );

	/* Clear the entry of the session. */
	pSessionEntry->aulLastPktTime[ 0 ] = 0;
	pSessionEntry->aulLastPktTime[ 1 ] = 0;
	pSessionEntry->ulSessionNum = cOCT6100_INVALID_VALUE;
	pSessionEntry->ulTransactionNum = cOCT6100_INVALID_VALUE;
	pSessionEntry->ulPktRetryNum = cOCT6100_INVALID_VALUE;

	/* Update the other entry before removing the node. */
	pSessionEntry->ulBackwardLink = pRemoteDebugInfo->ulSessionListTail;
	pSessionEntry->ulForwardLink = cOCT6100_INVALID_VALUE;

	if ( pSessionEntry->ulBackwardLink != cOCT6100_INVALID_VALUE )
	{
		mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulBackwardLink, pSessionTempEntry );
		pSessionTempEntry->ulForwardLink = pSessionEntry->ulForwardLink;
	}
	else /* pSessionEntry->ulBackwardLink == cOCT6100_INVALID_VALUE */
	{
		pRemoteDebugInfo->ulSessionListHead = pSessionEntry->ulForwardLink;
	}
		
	if ( pSessionEntry->ulForwardLink != cOCT6100_INVALID_VALUE )
	{
		mOCT6100_GET_REMOTE_DEBUG_LIST_ENTRY_PNT( pSharedInfo, pSessionEntry->ulForwardLink, pSessionTempEntry );
		pSessionTempEntry->ulBackwardLink = pSessionEntry->ulBackwardLink;
	}
	else /* pSessionEntry->ulForwardLink == cOCT6100_INVALID_VALUE */
	{
		pRemoteDebugInfo->ulSessionListTail = pSessionEntry->ulBackwardLink;
	}

	/* Invalidate the pointer. */
	pSessionEntry->ulBackwardLink = cOCT6100_INVALID_VALUE;
	pSessionEntry->ulForwardLink = cOCT6100_INVALID_VALUE;

	/* Remove the session. */
	ulResult = octapi_bt0_remove_node( pSessionTree, ( ( PVOID )&f_ulSessionNumber ) );
	if ( ulResult != cOCT6100_ERR_OK )
		f_pCmndHeader->ulFunctionResult = cOCT6100_ERR_FATAL_47;

	/* Increment number of open sessions. */
	pRemoteDebugInfo->ulNumSessionsOpen--;
}
#endif
