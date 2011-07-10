/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:	rpc_protocol.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

This file contains RPC related definitions and prototypes.

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

$Octasic_Revision: 23 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __RPC_PROTOCOL_H__
#define __RPC_PROTOCOL_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define cOCTRPC_ENDIAN_DETECT				0x27182819
#define cOCTRPC_ENDIAN_DETECT_BYTE_W		0x19
#define cOCTRPC_ENDIAN_DETECT_BYTE_X		0x28
#define cOCTRPC_ENDIAN_DETECT_BYTE_Y		0x18
#define cOCTRPC_ENDIAN_DETECT_BYTE_Z		0x27
#define cOCTRPC_ECHO_PROTOCOL				0x00000000

#define cOCTRPC_MIN_PACKET_BYTE_LENGTH		(sizeof( tOCTRPC_OGRDTP_HEADER ))
#define cOCTRPC_FIRST_COMMAND_BYTE_OFFSET	(sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER ))
#define cOCTRPC_GENERIC_HEADERS_BYTE_SIZE	(sizeof( tOCTRPC_OGRDTP_HEADER ) + sizeof( tOCTRPC_INTERFACE_HEADER ) + sizeof( tOCTRPC_COMMAND_HEADER ))
#define cOCTRPC_MAX_PACKET_BYTE_LENGTH		32768

/* Protocol versions */
#define cOCTRPC_PROTOCOL_V1_0				0x00010000
#define cOCTRPC_PROTOCOL_V1_1				0x00010001
#define cOCTRPC_PROTOCOL_V1_2				0x00010002
#define cOCTRPC_PROTOCOL_V1_3				0x00010003
#define cOCTRPC_OCTASIC_PROTOCOL_V1_0		0xFF010000
#define cOCTRPC_OCTASIC_PROTOCOL_V1_1		0xFF010001
#define cOCTRPC_OCTASIC_PROTOCOL_V1_2		0xFF010002
#define cOCTRPC_OCTASIC_PROTOCOL_V1_3		0xFF010003

/* Chips */
#define cOCTRPC_OCT8304_INTERFACE			0x00000000
#define cOCTRPC_OCT6100_INTERFACE			0x00000001

/* Timeout values. */
#define cOCTRPC_SESSION_TIMEOUT				30

/* Generic errors */
#define cOCTRPC_RDBGERR_OK							0x00000000
#define cOCTRPC_RDBGERR_NO_ANSWER					0xFFFF0000
#define cOCTRPC_RDBGERR_ALL_SESSIONS_OPEN			0xFFFF0001
#define cOCTRPC_RDBGERR_PROTOCOL_NUMBER				0xFFFF0002
#define cOCTRPC_RDBGERR_NO_COMMAND_HEADER			0xFFFF0003
#define cOCTRPC_RDBGERR_INTERFACE_TYPE				0xFFFF0004
#define cOCTRPC_RDBGERR_INTERFACE_VERSION			0xFFFF0005
#define cOCTRPC_RDBGERR_INVALID_PACKET_LENGTH		0xFFFF0006
#define cOCTRPC_RDBGERR_INVALID_COMMAND_LENGTH		0xFFFF0007
#define cOCTRPC_RDBGERR_INVALID_COMMAND_NUMBER		0xFFFF0008
#define cOCTRPC_RDBGERR_PACKET_TOO_LARGE			0xFFFF0009
#define cOCTRPC_RDBGERR_LIST_EMPTY					0xFFFF000A

#define cOCTRPC_RDBGERR_FATAL						0xFFFFFFFF


/*****************************  TYPES  ***************************************/

typedef struct _OCTRPC_OGRDTP_HEADER_
{
	UINT32	IN		ulEndianDetect;
	UINT32	IN		ulDebugSessionNum;
	UINT32	IN		ulTransactionNum;
	UINT32	IN		ulPktRetryNum;
	UINT32	IN		ulPktByteSize;
	UINT32	IN		ulChecksum;
	UINT32	OUT		ulParsingError;
	UINT32	IN		ulRpcProtocolNum;

} tOCTRPC_OGRDTP_HEADER, *tPOCTRPC_OGRDTP_HEADER;

typedef struct _OCTRPC_INTERFACE_HEADER_
{
	UINT32	IN		ulInterfaceType;
	UINT32	IN		ulInterfaceVersion;

} tOCTRPC_INTERFACE_HEADER, *tPOCTRPC_INTERFACE_HEADER;

typedef struct _OCTRPC_COMMAND_HEADER_
{
	UINT32	IN		ulCommandByteSize;
	UINT32	IN OUT	ulRpcCommandNum;
	UINT32	OUT		ulFunctionResult;

} tOCTRPC_COMMAND_HEADER, *tPOCTRPC_COMMAND_HEADER;

#endif /* __RPC_PROTOCOL_H__ */
