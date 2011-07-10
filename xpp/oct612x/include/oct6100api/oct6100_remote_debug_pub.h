/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_remote_debug_pub.h

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

#ifndef __OCT6100_REMOTE_DEBUG_PUB_H__
#define __OCT6100_REMOTE_DEBUG_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_REMOTE_DEBUG_
{
	PUINT32	pulReceivedPktPayload;
	UINT32	ulReceivedPktLength;

	PUINT32	pulResponsePktPayload;
	UINT32	ulMaxResponsePktLength;
	UINT32	ulResponsePktLength;

} tOCT6100_REMOTE_DEBUG, *tPOCT6100_REMOTE_DEBUG;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100RemoteDebugDef(
				OUT		tPOCT6100_REMOTE_DEBUG		f_pRemoteDebug );
UINT32 Oct6100RemoteDebug(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInst,
				IN OUT	tPOCT6100_REMOTE_DEBUG		f_pRemoteDebug );

#endif /* __OCT6100_REMOTE_DEBUG_PUB_H__ */
