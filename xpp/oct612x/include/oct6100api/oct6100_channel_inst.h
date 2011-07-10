/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_channel_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_channel.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_channel_priv.h file.

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

$Octasic_Revision: 90 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHANNEL_INST_H__
#define __OCT6100_CHANNEL_INST_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

/*****************************  TYPES  ***************************************/

#ifndef	__KERNEL__
#include	<stdint.h>
#endif

#ifndef	PTR_TYPE
#define	PTR_TYPE	UINT16
#endif

typedef struct _OCT6100_API_CHANNEL_TDM_
{
	/* Laws. */
	UINT8	byRinPcmLaw : 1;
	UINT8	bySinPcmLaw : 1;
	UINT8	byRoutPcmLaw : 1;
	UINT8	bySoutPcmLaw : 1;

	UINT8	byRinNumTssts : 1;
	UINT8	bySinNumTssts : 1;
	UINT8	byRoutNumTssts : 1;
	UINT8	bySoutNumTssts : 1;

	/* RIN port. */
	UINT16	usRinTimeslot;
	UINT16	usRinStream;

	/* SIN port. */
	UINT16	usSinTimeslot;
	UINT16	usSinStream;

	/* ROUT port. */
	UINT16	usRoutTimeslot;
	UINT16	usRoutStream;
	
	/* SOUT port. */
	UINT16	usSoutTimeslot;
	UINT16	usSoutStream;

	/* ROUT broadcast info. */
	UINT16	usRoutBrdcastTsstFirstEntry;
	UINT16	usRoutBrdcastTsstNumEntry;

	/* SOUT broadcast info. */
	UINT16	usSoutBrdcastTsstFirstEntry;
	UINT16	usSoutBrdcastTsstNumEntry;

} tOCT6100_API_CHANNEL_TDM, *tPOCT6100_API_CHANNEL_TDM;

typedef struct _OCT6100_API_CHANNEL_VQE_
{
	UINT8	fEnableNlp : 1;
	UINT8	fEnableTailDisplacement : 1;
	UINT8	fSinDcOffsetRemoval : 1;
	UINT8	fRinDcOffsetRemoval : 1;
	UINT8	fRinLevelControl : 1;
	UINT8	fSoutLevelControl : 1;
	UINT8	fRinAutomaticLevelControl : 1;
	UINT8	fSoutAutomaticLevelControl : 1;
	UINT8	fRinHighLevelCompensation : 1;
	UINT8	fSoutAdaptiveNoiseReduction : 1;
	UINT8	fDtmfToneRemoval : 1;
	UINT8	fAcousticEcho : 1;
	UINT8	byComfortNoiseMode : 1;
	UINT8	fSoutNaturalListenerEnhancement : 1;
	UINT8	fRoutNoiseReduction : 1;
	UINT8	fEnableMusicProtection : 1;
	UINT8	fIdleCodeDetection : 1;
	UINT8	byAnrVoiceNoiseSegregation : 1;
	UINT8	byDoubleTalkBehavior : 1;
	UINT8	fSoutNoiseBleaching : 1;
	UINT8	fSoutConferencingNoiseReduction : 1;
	UINT8	bySoutAutomaticListenerEnhancementGainDb : 1;
	UINT8	byNonLinearityBehaviorA : 1;
	UINT8	byNonLinearityBehaviorB : 1;
	UINT8	bySoutNaturalListenerEnhancementGainDb : 1;

	OCT_INT8	chRinAutomaticLevelControlTargetDb;
	OCT_INT8	chSoutAutomaticLevelControlTargetDb;
	
	OCT_INT8	chRinHighLevelCompensationThresholdDb;
	
	OCT_INT8	chRinLevelControlGainDb;
	OCT_INT8	chSoutLevelControlGainDb;

	OCT_INT8	chDefaultErlDb;
	OCT_INT8	chAecDefaultErlDb;

	OCT_INT8	chRoutNoiseReductionLevelGainDb;
	OCT_INT8	chAnrSnrEnhancementDb;

	UINT16	usToneDisablerVqeActivationDelay;
	UINT16	usAecTailLength;

	UINT16	usTailDisplacement;
	UINT16	usTailLength;

} tOCT6100_API_CHANNEL_VQE, *tPOCT6100_API_CHANNEL_VQE;

typedef struct _OCT6100_API_CHANNEL_CODEC_
{
	UINT8	byAdpcmNibblePosition : 1;
	UINT8	fEnableSilenceSuppression : 1;

	UINT8	byEncoderPort : 1;
	UINT8	byEncodingRate : 1;

	UINT8	byDecoderPort : 1;
	UINT8	byDecodingRate : 1;
	
	UINT8	byPhase : 1;
	UINT8	byPhasingType : 1;

} tOCT6100_API_CHANNEL_CODEC, *tPOCT6100_API_CHANNEL_CODEC;

typedef struct _OCT6100_API_CHANNEL_
{
	/*=======================================================================*/
	/* Channel configuration. */

	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved : 1;

	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt : 1;

	/* Is this a bidirectionnal channel? */
	UINT8	fBiDirChannel : 1;

	/* Enable tone disabler? */
	UINT8	fEnableToneDisabler : 1;

	/* Current echo operation mode. */
	UINT8	byEchoOperationMode : 1;

	UINT8	byToneDisablerStatus : 1;

	UINT8	fMute : 1;
	UINT8	fTap : 1;
	UINT8	fBeingTapped : 1;
	UINT8	fCopyEventCreated : 1;

	UINT8	fSoutBufPlaying : 1;
	UINT8	fRinBufPlaying : 1;

	UINT8	fRinBufPlayoutNotifyOnStop : 1;
	UINT8	fRinBufPlayoutRepeatUsed : 1;


	UINT8	fSoutBufPlayoutNotifyOnStop : 1;
	UINT8	fSoutBufPlayoutRepeatUsed : 1;

	UINT8	fRinHardStop : 1;
	UINT8	fSoutHardStop : 1;

	UINT8	byRinPlayoutStopEventType : 1;
	UINT8	bySoutPlayoutStopEventType : 1;

	UINT8	fRinBufAdded : 1;
	UINT8	fSoutBufAdded : 1;

	UINT8	fBufPlayoutActive : 1;

	/* Enable extended tone detection. */
	UINT8	fEnableExtToneDetection : 1;

	/* State of the codec structure associated to this channel. */
	UINT8	fSinSoutCodecActive : 1;
	UINT8	fRinRoutCodecActive : 1;

	/* TSI chariot memory entry for the Rin/Rout stream. */
	UINT16	usRinRoutTsiMemIndex;

	/* TSI chariot memory entry for the Sin/Sout stream. */
	UINT16	usSinSoutTsiMemIndex;

	/* Additional TSI entry used to temporarily store the SIN signal. */
	UINT16	usExtraSinTsiMemIndex;
	UINT16	usExtraSinTsiDependencyCnt;

	/* Additional TSI entry used to temporarily store the RIN signal. */
	UINT16	usExtraRinTsiMemIndex;
	UINT16	usExtraRinTsiDependencyCnt;
	
	/* Conversion chariot memory entry. */
	UINT16	usRinRoutConversionMemIndex;
	UINT16	usSinSoutConversionMemIndex;
	
	/* TSST control memory entry. */
	UINT16	usRinTsstIndex;
	UINT16	usSinTsstIndex;
	UINT16	usRoutTsstIndex;
	UINT16	usSoutTsstIndex;

	/* SSPX memory entry. */
	UINT16	usEchoMemIndex;

	/* Active mixer events count to test for last event. */
	UINT16	usMixerEventCnt;

	/* Copy events. */
	UINT16	usSinCopyEventIndex;
	UINT16	usSoutCopyEventIndex;
	
	/* Silence events. */
	UINT16	usRinSilenceEventIndex;
	UINT16	usSinSilenceEventIndex;

	/* TDM configuration. */
	tOCT6100_API_CHANNEL_TDM	TdmConfig;

	/* VQE configuration. */
	tOCT6100_API_CHANNEL_VQE	VqeConfig;

	/* Currently muted ports. */
	UINT16	usMutedPorts;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Statistics section. */

	INT16	sComfortNoiseLevel;

	UINT16	usCurrentEchoDelay;
	UINT16	usMaxEchoDelay;

	UINT16	usNumEchoPathChanges;
	UINT16	usNumEchoPathChangesOfst;
	
	INT16	sCurrentERL;
	INT16	sCurrentERLE;
	
	INT16	sMaxERL;
	INT16	sMaxERLE;
	
	INT16	sRinLevel;
	INT16	sSinLevel;
	
	INT16	sRinAppliedGain;
	INT16	sSoutAppliedGain;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Bridge information. */

	UINT16	usBridgeIndex;

	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	
	UINT16	usFlexConfParticipantIndex;
	UINT16	usTapBridgeIndex;
	UINT16	usTapChanIndex;
	
	/*=======================================================================*/


	/*=======================================================================*/
	/* Buffer playout information. */

	PTR_TYPE	ulRinBufWritePtr;
	PTR_TYPE	ulRinBufSkipPtr;
	PTR_TYPE	ulSoutBufWritePtr;
	PTR_TYPE	ulSoutBufSkipPtr;

	/* User channel ID, transparently passed to the user. */
	
	/*=======================================================================*/


	/*=======================================================================*/
	/* Copy events information. */

	/* Number of copy events created. */
	UINT16	usCopyEventCnt;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Extended tone detection info. */

	
	UINT16	usExtToneChanIndex;
	UINT16	usExtToneMixerIndex;
	UINT16	usExtToneTsiIndex;

	/* Index of the phasing TSST */
	UINT16	usPhasingTsstIndex;

	/* Mode of operation of the channel based on the extended tone detection configuration. */
	PTR_TYPE	ulExtToneChanMode;

	/*=======================================================================*/

	/* Tone detection state. */
	/* This array is configured as follow. */
	/* Index 0 contain event 0 to 31 and Index 1 contains event 32 - 55 */
	PTR_TYPE	ulLastSSToneDetected;
	PTR_TYPE	ulLastSSToneTimestamp;


	PTR_TYPE	ulRinUserBufPlayoutEventId;
	PTR_TYPE	ulSoutUserBufPlayoutEventId;

	UINT32	aulToneConf[2];
	UINT32	ulUserChanId;
	/*=======================================================================*/


	/*=======================================================================*/


	/* Codec configuration. */
	tOCT6100_API_CHANNEL_CODEC	CodecConfig;

} tOCT6100_API_CHANNEL, *tPOCT6100_API_CHANNEL;

typedef struct _OCT6100_API_BIDIR_CHANNEL_
{
	UINT16	usFirstChanIndex;
	UINT16	usSecondChanIndex;
	
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved : 1;
	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt : 1;

} tOCT6100_API_BIDIR_CHANNEL, *tPOCT6100_API_BIDIR_CHANNEL;

#endif /* __OCT6100_CHANNEL_INST_H__ */
