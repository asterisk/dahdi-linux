/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_rpc_protocol.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines and prototypes related to the OCT6100 RPC 
	protocol for exchanging debug commands.

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

$Octasic_Revision: 6 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_RPC_PROTOCOL_H__
#define __OCT6100_RPC_PROTOCOL_H__

/*****************************  DEFINES  *************************************/

#define cOCTRPC_INTERFACE_VERSION				0x00010002

/* Octasic commands. */
#define cOCT6100_RPC_CHIP_LIST					0xFF000000
#define cOCT6100_RPC_CHIP_CHOICE				0xFF000001
#define cOCT6100_RPC_ENV_DISCONNECT				0xFF000002

/* Commands */
/* Read commands */
#define cOCT6100_RPC_READ_WORD					0x00000000
#define cOCT6100_RPC_READ_BURST					0x00000001
#define cOCT6100_RPC_READ_DEBUG					0x00000002
#define cOCT6100_RPC_READ_ARRAY					0x00000003
#define cOCT6100_RPC_API_DISCONNECT				0x00000004

/* Write commands */
#define cOCT6100_RPC_WRITE_WORD					0x00000010
#define cOCT6100_RPC_WRITE_BURST				0x00000011
#define cOCT6100_RPC_WRITE_SMEAR				0x00000012
#define cOCT6100_RPC_WRITE_INC					0x00000013

/* Debug commands.*/
#define cOCT6100_RPC_SET_HOT_CHANNEL			0x00000014
#define cOCT6100_RPC_GET_DEBUG_CHAN_INDEX		0x00000015

#define cOCTRPC_UNKNOWN_COMMAND_NUM				0xFFFFFFFF

/* Errors */
#define cOCT6100_RPCERR_OK						0x00000000
#define cOCT6100_RPCERR_INVALID_COMMAND_NUMBER	0x00000001
#define cOCT6100_RPCERR_INVALID_COMMAND_PAYLOAD	0x00000002
#define cOCT6100_RPCERR_INVALID_COMMAND_LENGTH	0x00000003


/*****************************  TYPES  ***************************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_READ_WORD

Description:    Command structure for the read of one word.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			Address at which to read.
OUT	ulReadData			The word read, returned.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_READ_WORD_
{
	UINT32	IN	ulAddress;
	UINT32	OUT	ulReadData;

} tOCT6100_RPC_READ_WORD, *tPOCT6100_RPC_READ_WORD;


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_READ_BURST

Description:    Command structure for a read burst.  The burst starts at the
				given address and reads the specified number of consecutive
				words.

				Whereas every command structure uses a complete dword for every
				member, irrespective of the size of data unit needed, this
				structure does not do so for the read data.  To save bandwidth
				the read data words are returned two per dword.

Example packet:	31                 16 15                  0
				-------------------------------------------
				|             ulAddress = 0x100           |
				-------------------------------------------
				|            ulBurstLength = 0x3          |
				-------------------------------------------
 aulReadData ->	|         D0         |         D1         |
				-------------------------------------------
				|         D2         |         xx         |
				-------------------------------------------

				Dy is the read data at ulAddress + 2 * y.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			Address at which to read.
IN	ulBurstLength		The number of consecutive words to be read.
OUT	aulReadData			The read data returned.  The dwords of the structure
						starting at this address are arranged as indicated in
						the example packet above.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_READ_BURST_
{
	UINT32	IN	ulAddress;
	UINT32	IN	ulBurstLength;
	UINT32	OUT	aulReadData[ 1 ];

} tOCT6100_RPC_READ_BURST, *tPOCT6100_RPC_READ_BURST;



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_READ_ARRAY

Description:    Command structure for a variable number of reads.  The reads do
				not have to be at contiguous addresses.

				Whereas every command structure uses a complete dword for every
				member, irrespective of the size of data unit needed, this
				structure does not do so for the read data. To save bandwidth
				the read data words are returned two per dword, and the
				parity bits are returned 16 per dword (two parity bits per read
				access).

Example packet:	31                 16 15                  0
				-------------------------------------------
				|            ulArrayLength = 0x3          |
				-------------------------------------------
 aulArrayData ->|                   A0                    |
				-------------------------------------------
				|                   A1                    |
				-------------------------------------------
				|                   A2                    |
				-------------------------------------------
				|         D0         |         D1         |
				-------------------------------------------
				|         D2         |         xx         |
				-------------------------------------------

				Ay is the address for access y.
				Dy is the read data at Ay.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN		ulArrayLength	Number of reads to do.
IN OUT	aulArrayData	The addresses at which to read (IN) and the read data
						returned (OUT).  The dwords of the command structure
						starting at this address are arranged as indicated in
						the example packet above.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_READ_ARRAY
{
	UINT32	IN		ulArrayLength;
	UINT32	IN OUT	aulArrayData[ 1 ];
	
} tOCT6100_RPC_READ_ARRAY, *tPOCT6100_RPC_READ_ARRAY;


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_WRITE_WORD

Description:    Command structure for the write of one word.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			Address at which to write.
IN	ulWriteData			The word to write.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_WRITE_WORD_
{
	UINT32	IN	ulAddress;
	UINT32	IN	ulParity;
	UINT32	IN	ulWriteData;

} tOCT6100_RPC_WRITE_WORD, *tPOCT6100_RPC_WRITE_WORD;


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_WRITE_SMEAR

Description:    Command structure for the write of one word at one or many
				consecutive addresses.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			Address of first write.
IN	ulSmearLength		Number of consecutive addresses to write.
IN	ulWriteData			The word to write at each address.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_WRITE_SMEAR_
{
	UINT32	IN	ulAddress;
	UINT32	IN	ulSmearLength;
	UINT32	IN	ulParity;
	UINT32	IN	ulWriteData;

} tOCT6100_RPC_WRITE_SMEAR, *tPOCT6100_RPC_WRITE_SMEAR;


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_WRITE_INC

Description:    Command structure for the write of an incremental pattern at
				one or many consecutive addresses.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			Address of first write.
IN	ulIncLength			Number of consecutive addresses to write.
IN	ulWriteData			The first word of the incremental pattern.  For each
						consecutive write the word will be incremented by 1.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_WRITE_INC_
{
	UINT32	IN	ulAddress;
	UINT32	IN	ulIncLength;
	UINT32	IN	ulParity;
	UINT32	IN	ulWriteData;

} tOCT6100_RPC_WRITE_INC, *tPOCT6100_RPC_WRITE_INC;


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_WRITE_BURST

Description:    Command structure for a write burst.  The burst starts at the
				given address and writes a given word for each address.

				Whereas every command structure uses a complete dword for every
				member, irrespective of the size of data unit needed, this
				structure does not do so for the write data.  To save bandwidth
				the write data words are sent two per dword.

Example packet:	31                 16 15                  0
				-------------------------------------------
				|             ulAddress = 0x100           |
				-------------------------------------------
				|            ulBurstLength = 0x3          |
				-------------------------------------------
 aulWriteData ->|         D0         |         D1         |
				-------------------------------------------
				|         D2         |         xx         |
				-------------------------------------------

				Dy is the write data for ulAddress + 2 * y.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulAddress			First address at which to write.
IN	ulBurstLength		The number of consecutive addresses to be write.
IN	aulWriteData		The write data words.  The dwords of the structure
						starting at this address are arranged as indicated in
						the example packet above.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_WRITE_BURST_
{
	UINT32	IN	ulAddress;
	UINT32	IN	ulBurstLength;
	UINT32	IN	ulParity;
	UINT32	IN	aulWriteData[ 1 ];

} tOCT6100_RPC_WRITE_BURST, *tPOCT6100_RPC_WRITE_BURST;



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_SET_HOT_CHANNEL

Description:    Command structure to set the hot channel.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulDebugChannel			Index of the channel to debug.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_SET_HOT_CHANNEL_
{
	UINT32	IN	ulHotChannel;
	UINT32	IN	ulPcmLaw;

} tOCT6100_RPC_SET_HOT_CHANNEL, *tPOCT6100_RPC_SET_HOT_CHANNEL;



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Structure:		OCT6100_RPC_GET_DEBUG_CHAN_INDEX

Description:    Command structure to get the debug channel index used by the API.

-------------------------------------------------------------------------------
|	Member			|	Description
-------------------------------------------------------------------------------
IN	ulDebugChannel			Index of the channel to debug.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
typedef struct _OCT6100_RPC_GET_DEBUG_CHAN_INDEX_
{
	UINT32	OUT	ulDebugChanIndex;

} tOCT6100_RPC_GET_DEBUG_CHAN_INDEX, *tPOCT6100_RPC_GET_DEBUG_CHAN_INDEX;

#endif /* __OCT6100_RPC_PROTOCOL_H__ */
