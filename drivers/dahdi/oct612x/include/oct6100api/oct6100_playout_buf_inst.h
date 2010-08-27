/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_playout_buf_inst.h

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

$Octasic_Revision: 10 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_PLAYOUT_BUF_INST_H__
#define __OCT6100_PLAYOUT_BUF_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_BUFFER_MEMORY_NODE_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE )(( PUINT8 )pSharedInfo + pSharedInfo->ulPlayoutBufMemoryNodeListOfst );

#define mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE )(( PUINT8 )pSharedInfo + pSharedInfo->ulPlayoutBufMemoryNodeListOfst)) + ulIndex;

/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE_
{
	/* Next node. */
	UINT32	ulNext;

	/* Previous node. */
	UINT32	ulPrevious;

	/* Start address of this node. */
	UINT32	ulStartAddress;

	/* Size of this node. */
	UINT32	ulSize;

	/* Allocated node?  Free node? */
	UINT8	fAllocated;

} tOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE, *tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE;

typedef struct _OCT6100_API_BUFFER_
{
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Pcm law of the buffer. */
	UINT8	byBufferPcmLaw;

	/* Number of channels currently playing this buffer.*/
	UINT16	usDependencyCnt;

	/* Length of the buffer ( in bytes ).*/
	UINT32	ulBufferSize;
	
	/* Address in external memory of the buffer. */
	UINT32	ulBufferBase;

} tOCT6100_API_BUFFER, *tPOCT6100_API_BUFFER;

#endif /* __OCT6100_PLAYOUT_BUF_INST_H__ */
