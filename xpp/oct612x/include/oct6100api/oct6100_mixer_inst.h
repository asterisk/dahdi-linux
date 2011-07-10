/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_mixer_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_mixer.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_mixer_priv.h file.

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

#ifndef __OCT6100_MIXER_INST_H__
#define __OCT6100_MIXER_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_MIXER_EVENT_
{
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Type of the event.*/
	UINT16	usEventType;

	/* Source channel index */
	UINT16	usSourceChanIndex;

	/* Destination channel index */
	UINT16	usDestinationChanIndex;
	
	/* Pointer to the next entry.*/
	UINT16	usNextEventPtr;

} tOCT6100_API_MIXER_EVENT, *tPOCT6100_API_MIXER_EVENT;


typedef struct _OCT6100_API_COPY_EVENT_
{
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt;

	/* Source + destination ports. */
	UINT8	bySourcePort;
	UINT8	byDestinationPort;

	/* Index of the channels associated to this event.*/
	UINT16	usSourceChanIndex;
	UINT16	usDestinationChanIndex;

	UINT16	usMixerEventIndex;
	
} tOCT6100_API_COPY_EVENT, *tPOCT6100_API_COPY_EVENT;


#endif /* __OCT6100_MIXER_INST_H__ */
