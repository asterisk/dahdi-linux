/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_conf_bridge_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_conf_bridge.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_conf_bridge_priv.h file.

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

$Octasic_Revision: 19 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CONF_BRIDGE_INST_H__
#define __OCT6100_CONF_BRIDGE_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_CONF_BRIDGE_
{
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Entry counter for the resources. */
	UINT8	byEntryOpenCnt;

	/* Next bridge pointer. */
	UINT16	usNextBridgePtr;

	/* Previous bridge pointer. */
	UINT16	usPrevBridgePtr;

	/* Number of clients connected to the bridge. */
	UINT16	usNumClients;

	/* Store the index of the load event, to diffentiate him form the accumulate. */
	UINT16	usLoadIndex;

	/* Pointer to the first bridge events.*/
	UINT16	usFirstLoadEventPtr;
	UINT16	usFirstSubStoreEventPtr;
	UINT16	usLastSubStoreEventPtr;

	/* Pointer to the silence load event, if it exists. */
	UINT16	usSilenceLoadEventPtr;

	/* Flag specifying whether the dominant speaker is set or not. */
	UINT16	usDominantSpeakerChanIndex;
	UINT8	fDominantSpeakerSet;

	/* Flag specifying if this is flexible conferencing bridge. */
	UINT8	fFlexibleConferencing;
	
	/* Number of clients being tapped. */
	UINT16	usNumTappedClients;

} tOCT6100_API_CONF_BRIDGE, *tPOCT6100_API_CONF_BRIDGE;

typedef struct _OCT6100_API_FLEX_CONF_PARTICIPANT_
{
	/* Input port of the conferencing for this participant. */
	UINT32	ulInputPort;

	/* Whether the flexible mixer has been created. */
	UINT8	fFlexibleMixerCreated;

	/* Listener mask ( who can hear us ). */
	UINT32	ulListenerMask;

	/* Our index in the listener mask. */
	UINT32	ulListenerMaskIndex;

	/* Mixer event indexes for this participant's mixer. */
	UINT16	ausLoadOrAccumulateEventIndex[ cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE ];

} tOCT6100_API_FLEX_CONF_PARTICIPANT, *tPOCT6100_API_FLEX_CONF_PARTICIPANT;

#endif /* __OCT6100_CONF_BRIDGE_INST_H__ */
