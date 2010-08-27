/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_conf_bridge_pub.h

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

$Octasic_Revision: 22 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CONF_BRIDGE_PUB_H__
#define __OCT6100_CONF_BRIDGE_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_CONF_BRIDGE_OPEN_
{
	PUINT32	pulConfBridgeHndl;			/* Handle returned when the bridge is opened. */
	BOOL	fFlexibleConferencing;

} tOCT6100_CONF_BRIDGE_OPEN, *tPOCT6100_CONF_BRIDGE_OPEN;

typedef struct _OCT6100_CONF_BRIDGE_CLOSE_
{
	UINT32	ulConfBridgeHndl;
	
} tOCT6100_CONF_BRIDGE_CLOSE, *tPOCT6100_CONF_BRIDGE_CLOSE;

typedef struct _OCT6100_CONF_BRIDGE_CHAN_ADD_
{
	UINT32	ulConfBridgeHndl;
	UINT32	ulChannelHndl;
	UINT32	ulInputPort;
	UINT32	ulListenerMaskIndex;
	UINT32	ulListenerMask;
	BOOL	fMute;
	UINT32	ulTappedChannelHndl;

} tOCT6100_CONF_BRIDGE_CHAN_ADD, *tPOCT6100_CONF_BRIDGE_CHAN_ADD;

typedef struct _OCT6100_CONF_BRIDGE_CHAN_REMOVE_
{
	UINT32	ulConfBridgeHndl;
	UINT32	ulChannelHndl;
	BOOL	fRemoveAll;

} tOCT6100_CONF_BRIDGE_CHAN_REMOVE, *tPOCT6100_CONF_BRIDGE_CHAN_REMOVE;

typedef struct _OCT6100_CONF_BRIDGE_CHAN_MUTE_
{
	UINT32	ulChannelHndl;

} tOCT6100_CONF_BRIDGE_CHAN_MUTE, *tPOCT6100_CONF_BRIDGE_CHAN_MUTE;

typedef struct _OCT6100_CONF_BRIDGE_CHAN_UNMUTE_
{
	UINT32	ulChannelHndl;

} tOCT6100_CONF_BRIDGE_CHAN_UNMUTE, *tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE;

typedef struct _OCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET_
{
	UINT32	ulConfBridgeHndl;
	UINT32	ulChannelHndl;
	
} tOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET, *tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET;

typedef struct _OCT6100_CONF_BRIDGE_MASK_CHANGE_
{
	UINT32	ulChannelHndl;
	UINT32	ulNewListenerMask;
	
} tOCT6100_CONF_BRIDGE_MASK_CHANGE, *tPOCT6100_CONF_BRIDGE_MASK_CHANGE;

typedef struct _OCT6100_CONF_BRIDGE_STATS_
{
	UINT32	ulConfBridgeHndl;
	UINT32	ulNumChannels;
	UINT32	ulNumTappedChannels;
	BOOL	fFlexibleConferencing;

} tOCT6100_CONF_BRIDGE_STATS, *tPOCT6100_CONF_BRIDGE_STATS;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ConfBridgeOpenDef(
			OUT		tPOCT6100_CONF_BRIDGE_OPEN					f_pConfBridgeOpen );
UINT32 Oct6100ConfBridgeOpen(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_OPEN					f_pConfBridgeOpen );

UINT32 Oct6100ConfBridgeCloseDef(
			OUT		tPOCT6100_CONF_BRIDGE_CLOSE					f_pConfBridgeClose );
UINT32 Oct6100ConfBridgeClose(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_CLOSE					f_pConfBridgeClose );

UINT32 Oct6100ConfBridgeChanAddDef(
			OUT		tPOCT6100_CONF_BRIDGE_CHAN_ADD				f_pConfBridgeAdd );
UINT32 Oct6100ConfBridgeChanAdd(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_CHAN_ADD				f_pConfBridgeAdd );

UINT32 Oct6100ConfBridgeChanRemoveDef(
			OUT		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove );
UINT32 Oct6100ConfBridgeChanRemove(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove );

UINT32 Oct6100ConfBridgeChanMuteDef(
			OUT		tPOCT6100_CONF_BRIDGE_CHAN_MUTE				f_pConfBridgeMute );
UINT32 Oct6100ConfBridgeChanMute(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_CHAN_MUTE				f_pConfBridgeMute );

UINT32 Oct6100ConfBridgeChanUnMuteDef(
			OUT		tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE			f_pConfBridgeUnMute );
UINT32 Oct6100ConfBridgeChanUnMute(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE			f_pConfBridgeUnMute );

UINT32 Oct6100ConfBridgeDominantSpeakerSetDef(
			OUT		tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker );
UINT32 Oct6100ConfBridgeDominantSpeakerSet(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker );

UINT32 Oct6100ConfBridgeMaskChangeDef(
			OUT		tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange );
UINT32 Oct6100ConfBridgeMaskChange(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange );

UINT32 Oct6100ConfBridgeGetStatsDef(
			OUT		tPOCT6100_CONF_BRIDGE_STATS					f_pConfBridgeStats );
UINT32 Oct6100ConfBridgeGetStats(
			IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
			IN OUT	tPOCT6100_CONF_BRIDGE_STATS					f_pConfBridgeStats );

#endif /* __OCT6100_CONF_BRIDGE_PUB_H__ */
