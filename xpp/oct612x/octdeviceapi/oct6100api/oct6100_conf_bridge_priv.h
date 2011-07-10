/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_conf_bridge_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_conf_bridge.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_conf_bridge_pub.h file.

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

$Octasic_Revision: 30 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CONF_BRIDGE_PRIV_H__
#define __OCT6100_CONF_BRIDGE_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_CONF_BRIDGE_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_CONF_BRIDGE )(( PUINT8 )pSharedInfo + pSharedInfo->ulConfBridgeListOfst);

#define mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_CONF_BRIDGE )(( PUINT8 )pSharedInfo + pSharedInfo->ulConfBridgeListOfst)) + ulIndex;

#define mOCT6100_GET_CONF_BRIDGE_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulConfBridgeAllocOfst);

#define mOCT6100_GET_FLEX_CONF_PARTICIPANT_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_FLEX_CONF_PARTICIPANT )(( PUINT8 )pSharedInfo + pSharedInfo->ulFlexConfParticipantListOfst);

#define mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_FLEX_CONF_PARTICIPANT )(( PUINT8 )pSharedInfo + pSharedInfo->ulFlexConfParticipantListOfst)) + ulIndex;

#define mOCT6100_GET_FLEX_CONF_PARTICIPANT_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulFlexConfParticipantAllocOfst);


/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetConfBridgeSwSizes(
				IN OUT	tPOCT6100_CHIP_OPEN							f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES				f_pInstSizes );

UINT32 Oct6100ApiConfBridgeSwInit(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst );

UINT32 Oct6100ConfBridgeOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN OUT	tPOCT6100_CONF_BRIDGE_OPEN					f_pConfBridgeOpen );

UINT32 Oct6100ApiCheckBridgeParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN OUT	tPOCT6100_CONF_BRIDGE_OPEN					f_pConfBridgeOpen );

UINT32 Oct6100ApiReserveBridgeResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				OUT		PUINT16										f_pusBridgeIndex );

UINT32 Oct6100ApiUpdateBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_OPEN					f_pConfBridgeOpen,
				IN		UINT16										f_usBridgeIndex );

UINT32 Oct6100ConfBridgeCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CLOSE					f_pConfBridgeClose );

UINT32 Oct6100ApiAssertBridgeParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CLOSE					f_pConfBridgeClose,
				OUT		PUINT16										f_pusBridgeIndex );

UINT32 Oct6100ApiReleaseBridgeResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex );

UINT32 Oct6100ConfBridgeChanAddSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_ADD				f_pConfBridgeAdd );

UINT32 Oct6100ApiCheckBridgeAddParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_ADD				f_pConfBridgeAdd,
				OUT		PUINT16										f_pusBridgeIndex, 
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT8										f_pfMute,
				OUT		PUINT32										f_pulInputPort, 
				OUT		PUINT8										f_pfFlexibleConfBridge,
				OUT		PUINT32										f_pulListenerMaskIndex,
				OUT		PUINT32										f_pulListenerMask,
				OUT		PUINT8										f_pfTap,
				OUT		PUINT16										f_pusTapChannelIndex );

UINT32 Oct6100ApiReserveBridgeAddResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex,
				IN		UINT16										f_usChanIndex,
				IN		UINT32										f_ulInputPort,
				IN		UINT8										f_fFlexibleConfBridge,
				IN		UINT32										f_ulListenerMaskIndex,
				IN		UINT32										f_ulListenerMask,
				IN		UINT8										f_fTap,
				OUT		PUINT16										f_pusLoadEventIndex,
				OUT		PUINT16										f_pusSubStoreEventIndex,
				OUT		PUINT16										f_pusCopyEventIndex,
				OUT		PUINT16										f_pusTapBridgeIndex );

UINT32 Oct6100ApiBridgeEventAdd( 
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex, 
				IN		UINT16										f_usChannelIndex,
				IN		UINT8										f_fFlexibleConfBridge,
				IN		UINT16										f_usLoadEventIndex,
				IN		UINT16										f_usSubStoreEventIndex,
				IN		UINT16										f_usCopyEventIndex,
				IN		UINT32										f_ulInputPort,
				IN		UINT8										f_fMute, 
				IN		UINT32										f_ulListenerMaskIndex,
				IN		UINT32										f_ulListenerMask,
				IN		UINT8										f_fTap,
				IN		UINT16										f_usTapBridgeIndex,
				IN		UINT16										f_usTapChanIndex );

UINT32 Oct6100ApiBridgeAddParticipantToChannel(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex, 
				IN		UINT16										f_usSourceChannelIndex,
				IN		UINT16										f_usDestinationChannelIndex,
				IN		UINT16										f_usLoadOrAccumulateEventIndex,
				IN		UINT16										f_usStoreEventIndex,
				IN		UINT16										f_usCopyEventIndex,
				IN		UINT32										f_ulSourceInputPort,
				IN		UINT32										f_ulDestinationInputPort );

UINT32 Oct6100ConfBridgeChanRemoveSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove );

UINT32 Oct6100ApiCheckChanRemoveParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove,
				OUT		PUINT16										f_pusBridgeIndex, 
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT8										f_pfFlexibleConfBridge,
				OUT		PUINT8										f_pfTap,
				OUT		PUINT16										f_pusLoadEventIndex,
				OUT		PUINT16										f_pusSubStoreEventIndex,
				OUT		PUINT16										f_pusCopyEventIndex );

UINT32 Oct6100ApiReleaseChanEventResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove,
				IN		UINT16										f_usBridgeIndex, 
				IN		UINT16										f_usChanIndex, 
				IN		UINT8										f_fFlexibleConfBridge,
				IN		UINT16										f_usLoadEventIndex,
				IN		UINT16										f_usSubStoreEventIndex,
				IN		UINT16										f_usCopyEventIndex );

UINT32 Oct6100ApiBridgeEventRemove (
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE			f_pConfBridgeRemove,
				IN		UINT16										f_usBridgeIndex, 
				IN		UINT16										f_usChannelIndex,
				IN		UINT8										f_fFlexibleConfBridge,
				IN		UINT16										f_usLoadEventIndex,
				IN		UINT16										f_usSubStoreEventIndex,
				IN		UINT16										f_usCopyEventIndex,
				IN		UINT8										f_fTap );

UINT32 Oct6100ApiBridgeRemoveParticipantFromChannel(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst, 
				IN		UINT16										f_usBridgeIndex,
				IN		UINT16										f_usSourceChannelIndex,
				IN		UINT16										f_usDestinationChannelIndex,
				IN		UINT8										f_fRemovePermanently );

UINT32 Oct6100ConfBridgeChanMuteSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_MUTE				f_pConfBridgeMute );

UINT32 Oct6100ApiUpdateBridgeMuteResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usChanIndex,
				IN		UINT16										f_usLoadEventIndex,
				IN		UINT16										f_usSubStoreEventIndex, 
				IN		UINT8										f_fFlexibleConfBridge );
				
UINT32 Oct6100ApiCheckBridgeMuteParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_MUTE				f_pConfBridgeMute,
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT16										f_pusLoadEventIndex,
				OUT		PUINT16										f_pusSubStoreEventIndex, 
				OUT		PUINT8										f_pfFlexibleConfBridge );

UINT32 Oct6100ConfBridgeChanUnMuteSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE			f_pConfBridgeUnMute );

UINT32 Oct6100ApiCheckBridgeUnMuteParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE			f_pConfBridgeUnMute,
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT16										f_pusLoadEventIndex,
				OUT		PUINT16										f_pusSubStoreEventIndex, 
				OUT		PUINT8										f_pfFlexibleConfBridge );

UINT32 Oct6100ApiUpdateBridgeUnMuteResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usChanIndex,
				IN		UINT16										f_usLoadEventIndex,
				IN		UINT16										f_usSubStoreEventIndex, 
				IN		UINT8										f_fFlexibleConfBridge );

UINT32 Oct6100ConfBridgeDominantSpeakerSetSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker );

UINT32 Oct6100ApiCheckBridgeDominantSpeakerParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker,
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT16										f_pusBridgeIndex );

UINT32 Oct6100ApiUpdateBridgeDominantSpeakerResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usChanIndex,
				IN		UINT16										f_usBridgeIndex );

UINT32 Oct6100ConfBridgeMaskChangeSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange );

UINT32 Oct6100ApiCheckBridgeMaskChangeParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange,
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT16										f_pusBridgeIndex,
				OUT		PUINT32										f_pulNewParticipantMask );

UINT32 Oct6100ApiUpdateMaskModifyResources(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex,
				IN		UINT16										f_usChanIndex,
				IN		UINT32										f_ulNewListenerMask );

UINT32 Oct6100ApiBridgeUpdateMask( 
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst, 
				IN		UINT16										f_usBridgeIndex, 
				IN		UINT16										f_usChanIndex, 
				IN		UINT32										f_ulNewListenerMask );

UINT32 Oct6100ConfBridgeGetStatsSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN OUT	tPOCT6100_CONF_BRIDGE_STATS					f_pConfBridgeStats );

UINT32 Oct6100ApiReserveBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				OUT		PUINT16										f_pusConfBridgeIndex );

UINT32 Oct6100ApiReleaseBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usConfBridgeIndex );

UINT32 Oct6100ApiGetPrevLastSubStoreEvent(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usBridgeIndex,
				IN		UINT16										f_usBridgeFirstLoadEventPtr,
				OUT		PUINT16										f_pusLastSubStoreEventIndex );

UINT32 Oct6100ApiGetPreviousEvent(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usStartIndex,
				IN		UINT16										f_usSearchedIndex,
				IN		UINT16										f_usLoopCnt,
				OUT		PUINT16										f_pusPreviousIndex );

UINT32 Oct6100ApiBridgeSetDominantSpeaker(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usChannelIndex,
				IN		UINT16										f_usDominantSpeakerIndex );

UINT32 Oct6100ApiReserveFlexConfParticipantEntry(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				OUT		PUINT16										f_pusParticipantIndex );

UINT32 Oct6100ApiReleaseFlexConfParticipantEntry(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInst,
				IN		UINT16										f_usParticipantIndex );

#endif /* __OCT6100_CONF_BRIDGE_PRIV_H__ */
