/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_debug_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_debug.c.  All elements defined in this file are for public
	usage of the API.

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

#ifndef __OCT6100_DEBUG_PUB_H__
#define __OCT6100_DEBUG_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_DEBUG_SELECT_CHANNEL_
{
	UINT32	ulChannelHndl;

} tOCT6100_DEBUG_SELECT_CHANNEL, *tPOCT6100_DEBUG_SELECT_CHANNEL;

typedef struct _OCT6100_DEBUG_GET_DATA_
{
	UINT32	ulGetDataMode;
	UINT32	ulGetDataContent;
	UINT32	ulRemainingNumBytes;
	UINT32	ulTotalNumBytes;
	UINT32	ulMaxBytes;
	UINT32	ulValidNumBytes;
	PUINT8	pbyData;

} tOCT6100_DEBUG_GET_DATA, *tPOCT6100_DEBUG_GET_DATA;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100DebugSelectChannelDef(
				OUT		tPOCT6100_DEBUG_SELECT_CHANNEL		f_pSelectDebugChan );
UINT32 Oct6100DebugSelectChannel(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInst,
				IN OUT	tPOCT6100_DEBUG_SELECT_CHANNEL		f_pSelectDebugChan );

UINT32 Oct6100DebugGetDataDef(
				OUT		tPOCT6100_DEBUG_GET_DATA			f_pGetData );
UINT32 Oct6100DebugGetData(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInst,
				IN OUT	tPOCT6100_DEBUG_GET_DATA			f_pGetData );

#endif /* __OCT6100_DEBUG_PUB_H__ */
