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

typedef struct _OCT6100_API_CHANNEL_TDM_
{
	/* Laws. */
	UINT8	byRinPcmLaw;
	UINT8	bySinPcmLaw;
	UINT8	byRoutPcmLaw;
	UINT8	bySoutPcmLaw;

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

	UINT8	byRinNumTssts;
	UINT8	bySinNumTssts;
	UINT8	byRoutNumTssts;
	UINT8	bySoutNumTssts;

} tOCT6100_API_CHANNEL_TDM, *tPOCT6100_API_CHANNEL_TDM;

typedef struct _OCT6100_API_CHANNEL_VQE_
{
	UINT8	fEnableNlp;
	UINT8	fEnableTailDisplacement;
	UINT16	usTailDisplacement;
	UINT16	usTailLength;

	UINT8	fSinDcOffsetRemoval;
	UINT8	fRinDcOffsetRemoval;
	UINT8	fRinLevelControl;
	UINT8	fSoutLevelControl;
	
	UINT8	fRinAutomaticLevelControl;
	UINT8	fSoutAutomaticLevelControl;
	OCT_INT8	chRinAutomaticLevelControlTargetDb;
	OCT_INT8	chSoutAutomaticLevelControlTargetDb;
	
	UINT8	fRinHighLevelCompensation;
	OCT_INT8	chRinHighLevelCompensationThresholdDb;
	
	UINT8	bySoutAutomaticListenerEnhancementGainDb;
	UINT8	fSoutNaturalListenerEnhancement;

	UINT8	fSoutAdaptiveNoiseReduction;
	UINT8	fDtmfToneRemoval;
	UINT8	fAcousticEcho;
	UINT8	byComfortNoiseMode;

	UINT8	byNonLinearityBehaviorA;
	UINT8	byNonLinearityBehaviorB;
	OCT_INT8	chRinLevelControlGainDb;
	OCT_INT8	chSoutLevelControlGainDb;

	OCT_INT8	chDefaultErlDb;
	OCT_INT8	chAecDefaultErlDb;

	UINT8	fRoutNoiseReduction;
	OCT_INT8	chRoutNoiseReductionLevelGainDb;
	OCT_INT8	chAnrSnrEnhancementDb;

	UINT8	fEnableMusicProtection;
	UINT8	fIdleCodeDetection;

	UINT8	byAnrVoiceNoiseSegregation;
	UINT8	bySoutNaturalListenerEnhancementGainDb;
	
	UINT16	usToneDisablerVqeActivationDelay;
	UINT16	usAecTailLength;

	UINT8	byDoubleTalkBehavior;
	UINT8	fSoutNoiseBleaching;
	


	UINT8	fSoutConferencingNoiseReduction;



} tOCT6100_API_CHANNEL_VQE, *tPOCT6100_API_CHANNEL_VQE;

typedef struct _OCT6100_API_CHANNEL_CODEC_
{
	UINT8	byAdpcmNibblePosition;
	UINT8	fEnableSilenceSuppression;

	UINT8	byEncoderPort;
	UINT8	byEncodingRate;

	UINT8	byDecoderPort;
	UINT8	byDecodingRate;
	
	UINT8	byPhase;
	UINT8	byPhasingType;

} tOCT6100_API_CHANNEL_CODEC, *tPOCT6100_API_CHANNEL_CODEC;

typedef struct _OCT6100_API_CHANNEL_
{
	/*=======================================================================*/
	/* Channel configuration. */

	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt;

	/* Is this a bidirectionnal channel? */
	UINT8	fBiDirChannel;

	/* Enable tone disabler? */
	UINT8	fEnableToneDisabler;

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

	/* User channel ID, transparently passed to the user. */
	UINT32	ulUserChanId;

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

	/* Current echo operation mode. */
	UINT8	byEchoOperationMode;

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

	UINT8	byToneDisablerStatus;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Bridge information. */

	UINT16	usBridgeIndex;

	UINT8	fMute;
	UINT8	fTap;
	UINT8	fBeingTapped;
	UINT8	fCopyEventCreated;

	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	
	UINT16	usFlexConfParticipantIndex;
	UINT16	usTapBridgeIndex;
	UINT16	usTapChanIndex;
	
	/*=======================================================================*/


	/*=======================================================================*/
	/* Buffer playout information. */

	UINT32	ulRinBufWritePtr;
	UINT32	ulRinBufSkipPtr;
	
	UINT8	fSoutBufPlaying;
	UINT8	fRinBufPlaying;
	
	UINT8	fRinBufPlayoutNotifyOnStop;
	UINT8	fRinBufPlayoutRepeatUsed;

	UINT32	ulSoutBufWritePtr;
	UINT32	ulSoutBufSkipPtr;

	UINT8	fSoutBufPlayoutNotifyOnStop;
	UINT8	fSoutBufPlayoutRepeatUsed;

	UINT8	fRinHardStop;
	UINT8	fSoutHardStop;

	UINT32	ulRinUserBufPlayoutEventId;
	UINT32	ulSoutUserBufPlayoutEventId;
	
	UINT8	byRinPlayoutStopEventType;
	UINT8	bySoutPlayoutStopEventType;
	
	UINT8	fRinBufAdded;
	UINT8	fSoutBufAdded;
	
	UINT8	fBufPlayoutActive;
	
	/*=======================================================================*/


	/*=======================================================================*/
	/* Copy events information. */

	/* Number of copy events created. */
	UINT16	usCopyEventCnt;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Extended tone detection info. */

	/* Enable extended tone detection. */
	UINT8	fEnableExtToneDetection;
	
	UINT16	usExtToneChanIndex;
	UINT16	usExtToneMixerIndex;
	UINT16	usExtToneTsiIndex;

	/* Mode of operation of the channel based on the extended tone detection configuration. */
	UINT32	ulExtToneChanMode;

	/*=======================================================================*/

	/* Tone detection state. */
	/* This array is configured as follow. */
	/* Index 0 contain event 0 to 31 and Index 1 contains event 32 - 55 */
	UINT32	aulToneConf[ 2 ];
	UINT32	ulLastSSToneDetected;
	UINT32	ulLastSSToneTimestamp;

	/*=======================================================================*/


	/*=======================================================================*/

	/* Index of the phasing TSST */
	UINT16	usPhasingTsstIndex;
	
	/* State of the codec structure associated to this channel. */
	UINT8	fSinSoutCodecActive;
	UINT8	fRinRoutCodecActive;

	/* Codec configuration. */
	tOCT6100_API_CHANNEL_CODEC	CodecConfig;

	/*=======================================================================*/
	






	/* Nlp Conf Dword, index 0 contains the dword where the dword is located. and
	   index 1 is the actual value of the dword.*/
	UINT32	aulNlpConfDword[ cOCT6100_MAX_NLP_CONF_DWORD ][ 2 ];

} tOCT6100_API_CHANNEL, *tPOCT6100_API_CHANNEL;

typedef struct _OCT6100_API_BIDIR_CHANNEL_
{
	UINT16	usFirstChanIndex;
	UINT16	usSecondChanIndex;
	
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;

	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt;



} tOCT6100_API_BIDIR_CHANNEL, *tPOCT6100_API_BIDIR_CHANNEL;

#endif /* __OCT6100_CHANNEL_INST_H__ */
