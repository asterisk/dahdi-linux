/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_remote_debug_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_remote_debug.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_remote_debug_priv.h file.

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

#ifndef __OCT6100_REMOTE_DEBUG_INST_H__
#define __OCT6100_REMOTE_DEBUG_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_REMOTE_DEBUG_INFO_
{
	UINT32	ulSessionTreeOfst;
	UINT32	ulSessionListOfst;
	UINT32	ulSessionListHead;
	UINT32	ulSessionListTail;

	UINT32	ulPktCacheOfst;
	UINT32	ulDataBufOfst;
	
	UINT32	ulNumSessionsOpen;
	UINT32	ulMaxSessionsOpen;

} tOCT6100_API_REMOTE_DEBUG_INFO, *tPOCT6100_API_REMOTE_DEBUG_INFO;

typedef struct _OCT6100_API_REMOTE_DEBUG_SESSION_
{
	UINT32	ulSessionNum;
	UINT32	ulTransactionNum;
	UINT32	ulPktRetryNum;
	UINT32	ulPktByteSize;

	UINT32	aulLastPktTime[ 2 ];
	UINT32	ulForwardLink;
	UINT32	ulBackwardLink;

} tOCT6100_API_REMOTE_DEBUG_SESSION, *tPOCT6100_API_REMOTE_DEBUG_SESSION;

#endif /* __OCT6100_REMOTE_DEBUG_INST_H__ */
