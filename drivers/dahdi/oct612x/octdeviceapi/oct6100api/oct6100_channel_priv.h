/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_channel_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_channel.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_channel_pub.h file.

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

$Octasic_Revision: 62 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHANNEL_PRIV_H__
#define __OCT6100_CHANNEL_PRIV_H__


/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

/* ECHO channel list pointer macros. */
#define mOCT6100_GET_CHANNEL_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_CHANNEL )(( PUINT8 )pSharedInfo + pSharedInfo->ulChannelListOfst );

#define mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_CHANNEL )(( PUINT8 )pSharedInfo + pSharedInfo->ulChannelListOfst)) + ulIndex;

#define mOCT6100_GET_CHANNEL_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulChannelAllocOfst);

#define mOCT6100_GET_BIDIR_CHANNEL_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_BIDIR_CHANNEL )(( PUINT8 )pSharedInfo + pSharedInfo->ulBiDirChannelListOfst );

#define mOCT6100_GET_BIDIR_CHANNEL_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_BIDIR_CHANNEL )(( PUINT8 )pSharedInfo + pSharedInfo->ulBiDirChannelListOfst)) + ulIndex;

#define mOCT6100_GET_BIDIR_CHANNEL_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulBiDirChannelAllocOfst );


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_ECHO_CHAN_INDEX_
{
	/* Index of the channel in the API echo channel list.*/
	UINT16	usEchoChanIndex;
	
	/* TSI chariot memory entry for the Rin/Rout stream. */
	UINT16	usRinRoutTsiMemIndex;

	/* TSI chariot memory entry for the Sin/Sout stream. */
	UINT16	usSinSoutTsiMemIndex;

	/* SSPX memory entry. */
	UINT16	usEchoMemIndex;
	
	/* TDM sample conversion control memory entry. */
	UINT16	usRinRoutConversionMemIndex;
	UINT16	usSinSoutConversionMemIndex;

	/* Internal info for quick access to structures associated to this TSI cnct. */
	UINT16	usRinTsstIndex;
	UINT16	usSinTsstIndex;
	UINT16	usRoutTsstIndex;
	UINT16	usSoutTsstIndex;

	/* Index of the phasing TSST */
	UINT16	usPhasingTsstIndex;

	UINT8	fSinSoutCodecActive;
	UINT8	fRinRoutCodecActive;


	/* Extended Tone Detection resources.*/
	UINT16	usExtToneChanIndex;
	UINT16	usExtToneMixerIndex;
	UINT16	usExtToneTsiIndex;
} tOCT6100_API_ECHO_CHAN_INDEX, *tPOCT6100_API_ECHO_CHAN_INDEX;


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetChannelsEchoSwSizes(
				IN		tPOCT6100_CHIP_OPEN						f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES			f_pInstSizes );

UINT32 Oct6100ApiChannelsEchoSwInit(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance );

UINT32 Oct6100ChannelOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pChannelOpen );

UINT32 Oct6100ApiCheckChannelParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN OUT	tPOCT6100_API_ECHO_CHAN_INDEX			f_pChanIndexConf );

UINT32 Oct6100ApiReserveChannelResources(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN OUT	tPOCT6100_API_ECHO_CHAN_INDEX			f_pChanIndexConf );

UINT32 Oct6100ApiWriteChannelStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		tPOCT6100_API_ECHO_CHAN_INDEX			f_pChanIndexConf );

UINT32 Oct6100ApiUpdateChannelEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		tPOCT6100_API_ECHO_CHAN_INDEX			f_pChanIndexConf );

UINT32 Oct6100ChannelCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_CLOSE					f_pChannelClose );

UINT32 Oct6100ApiAssertChannelParams( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_CLOSE					f_pChannelClose,

				IN OUT	PUINT16									f_pusChanIndex );

UINT32 Oct6100ApiInvalidateChannelStructs( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,

				IN		UINT16									f_usChanIndex );

UINT32 Oct6100ApiReleaseChannelResources( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usChannelIndex );

UINT32 Oct6100ChannelModifySer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY				f_pChannelModify );

UINT32 Oct6100ApiCheckChannelModify(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY				f_pChannelModify,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pTempChanOpen,
				OUT		PUINT16									f_pusNewPhasingTsstIndex,
				OUT		PUINT16									f_pusChanIndex );

UINT32 Oct6100ApiModifyChannelResources(	
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY				f_pChannelModify,
				IN		UINT16									f_usChanIndex,
				OUT		PUINT16									f_pusNewRinTsstIndex,
				OUT		PUINT16									f_pusNewSinTsstIndex,
				OUT		PUINT16									f_pusNewRoutTsstIndex,
				OUT		PUINT16									f_pusNewSoutTsstIndex );

UINT32 Oct6100ApiModifyChannelStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY				f_pChannelModify, 
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen, 
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usNewPhasingTsstIndex,
				OUT		PUINT8									f_pfSinSoutCodecActive,
				OUT		PUINT8									f_pfRinRoutCodecActive,
				IN		UINT16									f_usNewRinTsstIndex,
				IN		UINT16									f_uslNewSinTsstIndex,
				IN		UINT16									f_usNewRoutTsstIndex,
				IN		UINT16									f_usNewSoutTsstIndex );

UINT32 Oct6100ApiModifyChannelEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_MODIFY				f_pChannelModify,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usNewPhasingTsstIndex,
				IN		UINT8									f_fSinSoutCodecActive,
				IN		UINT8									f_fRinRoutCodecActive,
				IN		UINT16									f_usNewRinTsstIndex,
				IN		UINT16									f_usNewSinTsstIndex,
				IN		UINT16									f_usNewRoutTsstIndex,
				IN		UINT16									f_usNewSoutTsstIndex );

UINT32 Oct6100ChannelBroadcastTsstAddSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd );

UINT32 Oct6100ApiCheckChanTsstAddParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstRemove, 
				OUT		PUINT16									f_pusChanIndex );

UINT32 Oct6100ApiReserveTsstAddResources(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstRemove, 
				IN		UINT16									f_usChanIndex,
				OUT		PUINT16									f_pusNewTsstIndex,
				OUT		PUINT16									f_pusNewTsstEntry );

UINT32 Oct6100ApiWriteTsstAddStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstRemove, 
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usNewTsstIndex );

UINT32 Oct6100ApiUpdateTsstAddChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstRemove, 
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usNewTsstIndex,
				IN		UINT16									f_usNewTsstEntry );

UINT32 Oct6100ChannelBroadcastTsstRemoveSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove);

UINT32 Oct6100ApiAssertChanTsstRemoveParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove, 
				OUT		PUINT16									f_pusChanIndex,
				OUT		PUINT16									f_pusTsstIndex,
				OUT		PUINT16									f_pusTsstEntry,
				OUT		PUINT16									f_pusPrevTsstEntry );

UINT32 Oct6100ApiInvalidateTsstRemoveStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usTsstIndex,
				IN		UINT32									f_ulPort,
				IN		BOOL									f_fRemoveAll );

UINT32 Oct6100ApiReleaseTsstRemoveResources(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove, 
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usTsstIndex,
				IN		UINT16									f_usTsstEntry,
				IN		UINT16									f_usPrevTsstEntry );

UINT32 Oct6100ApiChannelGetStatsSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_STATS					f_pChannelStats );

UINT32 Oct6100ApiReserveEchoEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				OUT		PUINT16									f_pusEchoIndex );

UINT32 Oct6100ApiReleaseEchoEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usEchoChanIndex );

UINT32 Oct6100ApiCheckTdmConfig( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_TDM				f_pTdmConfig );

UINT32 Oct6100ApiCheckVqeConfig( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		BOOL									f_fEnableToneDisabler );

UINT32 Oct6100ApiCheckCodecConfig( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_CODEC			f_pCodecConfig,
				IN		UINT32									f_ulDecoderNumTssts,
				OUT		PUINT16									f_pusPhasingTsstIndex );

UINT32 Oct6100ApiWriteInputTsstControlMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usTsstIndex,
				IN		UINT16									f_usTsiMemIndex,
				IN		UINT32									f_ulTsstInputLaw );

UINT32 Oct6100ApiWriteOutputTsstControlMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usTsstIndex,
				IN		UINT32									f_ulAdpcmNibblePosition,
				IN		UINT32									f_ulNumTssts,
				IN		UINT16									f_usTsiMemIndex );

UINT32 Oct6100ApiWriteEncoderMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulEncoderIndex,
				IN		UINT32									f_ulCompType,
				IN		UINT16									f_usTsiMemIndex,
				IN		UINT32									f_ulEnableSilenceSuppression,
				IN		UINT32									f_ulAdpcmNibblePosition,
				IN		UINT16									f_usPhasingTsstIndex,
				IN		UINT32									f_ulPhasingType,
				IN		UINT32									f_ulPhase );

UINT32 Oct6100ApiWriteDecoderMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usDecoderIndex,
				IN		UINT32									f_ulCompType,
				IN		UINT16									f_usTsiMemIndex,
				IN		UINT32									f_ulPcmLaw,
				IN		UINT32									f_ulAdpcmNibblePosition );


UINT32 Oct6100ApiClearConversionMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usConversionMemIndex );

UINT32 Oct6100ApiWriteVqeMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		BOOL									f_fClearPlayoutPointers,
				IN		BOOL									f_fModifyOnly );

UINT32 Oct6100ApiWriteVqeNlpMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		BOOL									f_fClearPlayoutPointers,
				IN		BOOL									f_fModifyOnly );

UINT32 Oct6100ApiWriteVqeAfMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		BOOL									f_fClearPlayoutPointers,
				IN		BOOL									f_fModifyOnly );

UINT32 Oct6100ApiWriteEchoMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_TDM				f_pTdmConfig,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usEchoIndex,
				IN		UINT16									f_usRinRoutTsiIndex,
				IN		UINT16									f_usSinSoutTsiIndex );

UINT32 Oct6100ApiUpdateOpenStruct( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY				f_pChanModify,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pChanOpen,
				IN		tPOCT6100_API_CHANNEL					f_pChanEntry );





UINT32 Oct6100ApiRetrieveNlpConfDword( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_API_CHANNEL					f_pChanEntry,
				IN		UINT32									f_ulAddress,
				OUT		PUINT32									f_pulConfigDword );

UINT32 Oct6100ApiSaveNlpConfDword( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_API_CHANNEL					f_pChanEntry,
				IN		UINT32									f_ulAddress,
				IN		UINT32									f_ulConfigDword );

UINT32 Oct6100ChannelCreateBiDirSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	IN OUT tPOCT6100_CHANNEL_CREATE_BIDIR	f_pChannelCreateBiDir );

UINT32 Oct6100ApiCheckChannelCreateBiDirParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_CREATE_BIDIR			f_pChannelCreateBiDir, 
				OUT		PUINT16									f_pusFirstChanIndex, 
				OUT		PUINT16									f_pusFirstChanExtraTsiIndex, 
				OUT		PUINT16									f_pusFirstChanSinCopyEventIndex,
				OUT		PUINT16									f_pusSecondChanIndex, 
				OUT		PUINT16									f_pusSecondChanExtraTsiIndex,
				OUT		PUINT16									f_pusSecondChanSinCopyEventIndex

				);

UINT32 Oct6100ApiReserveChannelCreateBiDirResources(	
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,

				OUT		PUINT16									f_pusBiDirChanIndex, 
				IN OUT	PUINT16									f_pusFirstChanExtraTsiIndex, 
				IN OUT	PUINT16									f_pusFirstChanSinCopyEventIndex, 
				OUT		PUINT16									f_pusFirstChanSoutCopyEventIndex,
				IN OUT	PUINT16									f_pusSecondChanExtraTsiIndex, 
				IN OUT	PUINT16									f_pusSecondChanSinCopyEventIndex,
				OUT		PUINT16									f_pusSecondChanSoutCopyEventIndex );

UINT32 Oct6100ApiWriteChannelCreateBiDirStructs(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,

				IN		UINT16									f_usFirstChanIndex,
				IN		UINT16									f_usFirstChanExtraTsiIndex, 
				IN		UINT16									f_usFirstChanSinCopyEventIndex, 
				IN		UINT16									f_usFirstChanSoutCopyEventIndex,
				IN		UINT16									f_usSecondChanIndex,
				IN		UINT16									f_usSecondChanExtraTsiIndex, 
				IN		UINT16									f_usSecondChanSinCopyEventIndex,
				IN		UINT16									f_usSecondChanSoutCopyEventIndex );

UINT32 Oct6100ApiUpdateBiDirChannelEntry(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				OUT		tPOCT6100_CHANNEL_CREATE_BIDIR			f_pChannelCreateBiDir,
				IN		UINT16									f_usBiDirChanIndex,
				IN		UINT16									f_usFirstChanIndex,
				IN		UINT16									f_usFirstChanExtraTsiIndex, 
				IN		UINT16									f_usFirstChanSinCopyEventIndex, 
				IN		UINT16									f_usFirstChanSoutCopyEventIndex,
				IN		UINT16									f_usSecondChanIndex,
				IN		UINT16									f_usSecondChanExtraTsiIndex, 
				IN		UINT16									f_usSecondChanSinCopyEventIndex,
				IN		UINT16									f_usSecondChanSoutCopyEventIndex );

UINT32 Oct6100ChannelDestroyBiDirSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_DESTROY_BIDIR			f_pChannelDestroyBiDir );

UINT32 Oct6100ApiAssertDestroyBiDirChanParams( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_DESTROY_BIDIR			f_pChannelDestroyBiDir,
				IN OUT	PUINT16									f_pusBiDirChanIndex,

				IN OUT	PUINT16									f_pusFirstChanIndex,
				IN OUT	PUINT16									f_pusSecondChanIndex );

UINT32 Oct6100ApiInvalidateBiDirChannelStructs( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,

				IN		UINT16									f_usFirstChanIndex,
				IN		UINT16									f_usSecondChanIndex );

UINT32 Oct6100ApiReleaseBiDirChannelResources( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usBiDirChanIndex,

				IN		UINT16									f_usFirstChanIndex,
				IN		UINT16									f_usSecondChanIndex );

UINT32 Oct6100ApiWriteDebugChanMemory( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_TDM				f_pTdmConfig,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		UINT16									f_usRinRoutTsiIndex,
				IN		UINT16									f_usSinSoutTsiIndex );

UINT32 Oct6100ApiDebugChannelOpen( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance );

UINT32 Oct6100ApiMutePorts( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usEchoIndex,
				IN		UINT16									f_usRinTsstIndex,
				IN		UINT16									f_usSinTsstIndex,
				IN		BOOL									f_fCheckBridgeIndex );

UINT32 Oct6100ApiSetChannelLevelControl(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		BOOL									f_fClearAlcHlcStatusBit );

UINT32 Oct6100ApiSetChannelTailConfiguration(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE				f_pVqeConfig,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usEchoMemIndex,
				IN		BOOL									f_fModifyOnly );

UINT32 Oct6100ChannelMuteSer( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MUTE					f_pChannelMute );

UINT32 Oct6100ApiAssertChannelMuteParams(	
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance, 
				IN		tPOCT6100_CHANNEL_MUTE					f_pChannelMute, 
				OUT		PUINT16									f_pusChanIndex,
				OUT		PUINT16									f_pusPorts );

UINT32 Oct6100ChannelUnMuteSer( 
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CHANNEL_UNMUTE				f_pChannelUnMute );

UINT32 Oct6100ApiAssertChannelUnMuteParams(	
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance, 
				IN		tPOCT6100_CHANNEL_UNMUTE				f_pChannelUnMute, 
				OUT		PUINT16									f_pusChanIndex,
				OUT		PUINT16									f_pusPorts );

UINT32 Oct6100ApiMuteSinWithFeatures(
				IN		tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usChanIndex,
				IN		BOOL									f_fEnableSinWithFeatures );

UINT32 Oct6100ApiMuteChannelPorts(	
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT16									f_usChanIndex,
				IN		UINT16									f_usPortMask,
				IN		BOOL									f_fMute );

INT32 Oct6100ApiOctFloatToDbEnergyByte(
				IN	UINT8 x );

INT32 Oct6100ApiOctFloatToDbEnergyHalf(
				IN	UINT16 x );

UINT16 Oct6100ApiDbAmpHalfToOctFloat(
				IN	INT32 x );

#endif /* __OCT6100_CHANNEL_PRIV_H__ */
