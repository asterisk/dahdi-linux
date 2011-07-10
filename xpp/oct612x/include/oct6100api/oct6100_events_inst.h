
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_events_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_events.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_events_priv.h file.

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

$Octasic_Revision: 12 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_EVENTS_INST_H__
#define __OCT6100_EVENTS_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_TONE_EVENT_
{
	UINT32	ulChannelHandle;
	UINT32	ulUserChanId;
	UINT32	ulToneDetected;		/* Tone number of the tone detected. */
	UINT32	ulTimestamp;
	UINT32	ulEventType;
	UINT32	ulExtToneDetectionPort;

} tOCT6100_API_TONE_EVENT, *tPOCT6100_API_TONE_EVENT;

typedef struct _OCT6100_API_BUFFER_PLAYOUT_EVENT_
{
	UINT32	ulChannelHandle;
	UINT32	ulUserChanId;
	UINT32	ulChannelPort;
	UINT32	ulTimestamp;
	UINT32	ulUserEventId;
	UINT32	ulEventType;

} tOCT6100_API_BUFFER_PLAYOUT_EVENT, *tPOCT6100_API_BUFFER_PLAYOUT_EVENT;

#endif /* __OCT6100_EVENTS_INST_H__ */

