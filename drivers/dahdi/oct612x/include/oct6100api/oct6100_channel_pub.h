/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_channel_pub.h

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

$Octasic_Revision: 84 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHANNEL_PUB_H__
#define __OCT6100_CHANNEL_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

/* Channel open structures. */
typedef struct _OCT6100_CHANNEL_OPEN_TDM_
{
	UINT32	ulRinNumTssts;
	UINT32	ulSinNumTssts;
	UINT32	ulRoutNumTssts;
	UINT32	ulSoutNumTssts;

	UINT32	ulSinTimeslot;
	UINT32	ulSinStream;
	UINT32	ulSinPcmLaw;

	UINT32	ulSoutTimeslot;
	UINT32	ulSoutStream;
	UINT32	ulSoutPcmLaw;

	UINT32	ulRinTimeslot;
	UINT32	ulRinStream;
	UINT32	ulRinPcmLaw;

	UINT32	ulRoutTimeslot;
	UINT32	ulRoutStream;
	UINT32	ulRoutPcmLaw;

} tOCT6100_CHANNEL_OPEN_TDM, *tPOCT6100_CHANNEL_OPEN_TDM;

typedef struct _OCT6100_CHANNEL_OPEN_VQE_
{
	BOOL	fEnableNlp;
	BOOL	fEnableTailDisplacement;
	UINT32	ulTailDisplacement;
	UINT32	ulTailLength;

	BOOL	fSinDcOffsetRemoval;
	BOOL	fRinDcOffsetRemoval;
	BOOL	fRinLevelControl;
	BOOL	fSoutLevelControl;
	BOOL	fRinAutomaticLevelControl;
	BOOL	fSoutAutomaticLevelControl;
	BOOL	fRinHighLevelCompensation;
	BOOL	fAcousticEcho;
	BOOL	fSoutAdaptiveNoiseReduction;
	BOOL	fDtmfToneRemoval;

	BOOL	fSoutNoiseBleaching;
	BOOL	fSoutConferencingNoiseReduction;

	UINT32	ulComfortNoiseMode;
	UINT32	ulNonLinearityBehaviorA;
	UINT32	ulNonLinearityBehaviorB;

	INT32	lRinLevelControlGainDb;
	INT32	lSoutLevelControlGainDb;
	INT32	lRinAutomaticLevelControlTargetDb;
	INT32	lSoutAutomaticLevelControlTargetDb;
	INT32	lRinHighLevelCompensationThresholdDb;
	INT32	lDefaultErlDb;
	INT32	lAecDefaultErlDb;
	UINT32	ulAecTailLength;
	UINT32	ulSoutAutomaticListenerEnhancementGainDb;
	UINT32	ulSoutNaturalListenerEnhancementGainDb;
	BOOL	fSoutNaturalListenerEnhancement;
	BOOL	fRoutNoiseReduction;
	INT32	lRoutNoiseReductionLevelGainDb;
	INT32	lAnrSnrEnhancementDb;
	UINT32	ulAnrVoiceNoiseSegregation;
	UINT32	ulDoubleTalkBehavior;
	
	UINT32	ulToneDisablerVqeActivationDelay;

	BOOL	fEnableMusicProtection;
	BOOL	fIdleCodeDetection;
	


} tOCT6100_CHANNEL_OPEN_VQE, *tPOCT6100_CHANNEL_OPEN_VQE;

typedef struct _OCT6100_CHANNEL_OPEN_CODEC_
{
	UINT32	ulAdpcmNibblePosition;

	UINT32	ulEncoderPort;
	UINT32	ulEncodingRate;

	UINT32	ulDecoderPort;
	UINT32	ulDecodingRate;

	BOOL	fEnableSilenceSuppression;
	UINT32	ulPhase;
	UINT32	ulPhasingType;
	UINT32	ulPhasingTsstHndl;

} tOCT6100_CHANNEL_OPEN_CODEC, *tPOCT6100_CHANNEL_OPEN_CODEC;

typedef struct _OCT6100_CHANNEL_OPEN_
{
	PUINT32	pulChannelHndl;
	UINT32	ulUserChanId;

	UINT32	ulEchoOperationMode;

	BOOL	fEnableToneDisabler;

	BOOL	fEnableExtToneDetection;

	tOCT6100_CHANNEL_OPEN_TDM	TdmConfig;
	tOCT6100_CHANNEL_OPEN_VQE	VqeConfig;
	tOCT6100_CHANNEL_OPEN_CODEC	CodecConfig;



} tOCT6100_CHANNEL_OPEN, *tPOCT6100_CHANNEL_OPEN;

/* Channel close structure. */
typedef struct _OCT6100_CHANNEL_CLOSE_
{
	UINT32	ulChannelHndl;

} tOCT6100_CHANNEL_CLOSE, *tPOCT6100_CHANNEL_CLOSE;

/* Channel modify structures. */
typedef struct _OCT6100_CHANNEL_MODIFY_TDM_
{
	UINT32	ulRinNumTssts;
	UINT32	ulSinNumTssts;
	UINT32	ulRoutNumTssts;
	UINT32	ulSoutNumTssts;
	
	UINT32	ulSinTimeslot;
	UINT32	ulSinStream;
	UINT32	ulSinPcmLaw;

	UINT32	ulSoutTimeslot;
	UINT32	ulSoutStream;
	UINT32	ulSoutPcmLaw;

	UINT32	ulRinTimeslot;
	UINT32	ulRinStream;
	UINT32	ulRinPcmLaw;

	UINT32	ulRoutTimeslot;
	UINT32	ulRoutStream;
	UINT32	ulRoutPcmLaw;

} tOCT6100_CHANNEL_MODIFY_TDM, *tPOCT6100_CHANNEL_MODIFY_TDM;

typedef struct _OCT6100_CHANNEL_MODIFY_VQE_
{
	BOOL	fEnableNlp;
	BOOL	fEnableTailDisplacement;
	UINT32	ulTailDisplacement;

	BOOL	fSinDcOffsetRemoval;
	BOOL	fRinDcOffsetRemoval;
	BOOL	fRinLevelControl;
	BOOL	fSoutLevelControl;
	BOOL	fRinAutomaticLevelControl;
	BOOL	fSoutAutomaticLevelControl;
	BOOL	fRinHighLevelCompensation;
	BOOL	fAcousticEcho;
	BOOL	fSoutAdaptiveNoiseReduction;
	BOOL	fDtmfToneRemoval;

	BOOL	fSoutConferencingNoiseReduction;
	BOOL	fSoutNoiseBleaching;

	UINT32	ulNonLinearityBehaviorA;
	UINT32	ulNonLinearityBehaviorB;
	UINT32	ulComfortNoiseMode;

	INT32	lRinLevelControlGainDb;
	INT32	lSoutLevelControlGainDb;
	INT32	lRinAutomaticLevelControlTargetDb;
	INT32	lSoutAutomaticLevelControlTargetDb;
	INT32	lRinHighLevelCompensationThresholdDb;
	INT32	lDefaultErlDb;
	INT32	lAecDefaultErlDb;
	UINT32	ulAecTailLength;
	UINT32	ulSoutAutomaticListenerEnhancementGainDb;
	UINT32	ulSoutNaturalListenerEnhancementGainDb;
	BOOL	fSoutNaturalListenerEnhancement;
	BOOL	fRoutNoiseReduction;
	INT32	lRoutNoiseReductionLevelGainDb;
	INT32	lAnrSnrEnhancementDb;
	UINT32	ulAnrVoiceNoiseSegregation;
	UINT32	ulDoubleTalkBehavior;

	UINT32	ulToneDisablerVqeActivationDelay;

	BOOL	fEnableMusicProtection;
	BOOL	fIdleCodeDetection;



} tOCT6100_CHANNEL_MODIFY_VQE, *tPOCT6100_CHANNEL_MODIFY_VQE;

typedef struct _OCT6100_CHANNEL_MODIFY_CODEC_
{
	UINT32	ulEncoderPort;
	UINT32	ulEncodingRate;

	UINT32	ulDecoderPort;
	UINT32	ulDecodingRate;

	BOOL	fEnableSilenceSuppression;
	UINT32	ulPhase;
	UINT32	ulPhasingType;
	UINT32	ulPhasingTsstHndl;

} tOCT6100_CHANNEL_MODIFY_CODEC, *tPOCT6100_CHANNEL_MODIFY_CODEC;

typedef struct _OCT6100_CHANNEL_MODIFY_
{
	UINT32	ulChannelHndl;
	UINT32	ulUserChanId;
	UINT32	ulEchoOperationMode;

	BOOL	fEnableToneDisabler;
	
	BOOL	fApplyToAllChannels;
	
	BOOL	fDisableToneDetection;
	BOOL	fStopBufferPlayout;
	BOOL	fRemoveConfBridgeParticipant;
	BOOL	fRemoveBroadcastTssts;

	BOOL	fTdmConfigModified;		/* TRUE/FALSE */
	BOOL	fVqeConfigModified;		/* TRUE/FALSE */
	BOOL	fCodecConfigModified;	/* TRUE/FALSE */


	tOCT6100_CHANNEL_MODIFY_TDM		TdmConfig;
	tOCT6100_CHANNEL_MODIFY_VQE		VqeConfig;
	tOCT6100_CHANNEL_MODIFY_CODEC	CodecConfig;

} tOCT6100_CHANNEL_MODIFY, *tPOCT6100_CHANNEL_MODIFY;

typedef struct _OCT6100_CHANNEL_BROADCAST_TSST_ADD_
{
	UINT32	ulChannelHndl;

	UINT32	ulPort;
	UINT32	ulTimeslot;
	UINT32	ulStream;

} tOCT6100_CHANNEL_BROADCAST_TSST_ADD, *tPOCT6100_CHANNEL_BROADCAST_TSST_ADD;

typedef struct _OCT6100_CHANNEL_BROADCAST_TSST_REMOVE_
{
	UINT32	ulChannelHndl;

	UINT32	ulPort;
	UINT32	ulTimeslot;
	UINT32	ulStream;

	BOOL	fRemoveAll;

} tOCT6100_CHANNEL_BROADCAST_TSST_REMOVE, *tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE;

/* Channel open structures.*/
typedef struct _OCT6100_CHANNEL_STATS_TDM_
{
	UINT32	ulMaxBroadcastTssts;
	UINT32	ulNumRoutBroadcastTssts;
	BOOL	fMoreRoutBroadcastTssts;
	UINT32	ulNumSoutBroadcastTssts;
	BOOL	fMoreSoutBroadcastTssts;

	UINT32	ulSinNumTssts;
	UINT32	ulSoutNumTssts;
	UINT32	ulRinNumTssts;
	UINT32	ulRoutNumTssts;

	UINT32	ulSinTimeslot;
	UINT32	ulSinStream;
	UINT32	ulSinPcmLaw;

	UINT32	ulSoutTimeslot;
	UINT32	ulSoutStream;
	UINT32	ulSoutPcmLaw;

	PUINT32	pulSoutBroadcastTimeslot;
	PUINT32	pulSoutBroadcastStream;
	
	UINT32	ulRinTimeslot;
	UINT32	ulRinStream;
	UINT32	ulRinPcmLaw;

	UINT32	ulRoutTimeslot;
	UINT32	ulRoutStream;
	UINT32	ulRoutPcmLaw;

	PUINT32	pulRoutBroadcastTimeslot;
	PUINT32	pulRoutBroadcastStream;

} tOCT6100_CHANNEL_STATS_TDM, *tPOCT6100_CHANNEL_STATS_TDM;

typedef struct _OCT6100_CHANNEL_STATS_VQE_
{
	BOOL	fEnableNlp;
	BOOL	fEnableTailDisplacement;
	UINT32	ulTailDisplacement;
	UINT32	ulTailLength;

	BOOL	fSinDcOffsetRemoval;
	BOOL	fRinDcOffsetRemoval;
	BOOL	fRinLevelControl;
	BOOL	fSoutLevelControl;
	BOOL	fRinAutomaticLevelControl;
	BOOL	fSoutAutomaticLevelControl;
	BOOL	fRinHighLevelCompensation;
	BOOL	fAcousticEcho;
	BOOL	fSoutAdaptiveNoiseReduction;
	BOOL	fDtmfToneRemoval;

	BOOL	fSoutConferencingNoiseReduction;
	BOOL	fSoutNoiseBleaching;
	
	UINT32	ulComfortNoiseMode;
	UINT32	ulNonLinearityBehaviorA;
	UINT32	ulNonLinearityBehaviorB;

	INT32	lRinLevelControlGainDb;
	INT32	lSoutLevelControlGainDb;
	INT32	lRinAutomaticLevelControlTargetDb;
	INT32	lSoutAutomaticLevelControlTargetDb;
	INT32	lRinHighLevelCompensationThresholdDb;
	INT32	lDefaultErlDb;
	INT32	lAecDefaultErlDb;
	UINT32	ulAecTailLength;
	UINT32	ulSoutAutomaticListenerEnhancementGainDb;
	UINT32	ulSoutNaturalListenerEnhancementGainDb;
	BOOL	fSoutNaturalListenerEnhancement;
	BOOL	fRoutNoiseReduction;
	INT32	lRoutNoiseReductionLevelGainDb;
	INT32	lAnrSnrEnhancementDb;
	UINT32	ulAnrVoiceNoiseSegregation;
	UINT32	ulDoubleTalkBehavior;

	UINT32	ulToneDisablerVqeActivationDelay;

	BOOL	fEnableMusicProtection;
	BOOL	fIdleCodeDetection;



} tOCT6100_CHANNEL_STATS_VQE, *tPOCT6100_CHANNEL_STATS_VQE;

typedef struct _OCT6100_CHANNEL_STATS_CODEC_
{
	UINT32	ulAdpcmNibblePosition;

	UINT32	ulEncoderPort;
	UINT32	ulEncodingRate;

	UINT32	ulDecoderPort;
	UINT32	ulDecodingRate;

	BOOL	fEnableSilenceSuppression;
	UINT32	ulPhase;
	UINT32	ulPhasingType;
	UINT32	ulPhasingTsstHndl;

} tOCT6100_CHANNEL_STATS_CODEC, *tPOCT6100_CHANNEL_STATS_CODEC;

typedef struct _OCT6100_CHANNEL_STATS_
{
	BOOL	fResetStats;
	
	UINT32	ulChannelHndl;
	UINT32	ulUserChanId;

	UINT32	ulEchoOperationMode;
	BOOL	fEnableToneDisabler;

	UINT32	ulMutePortsMask;
	BOOL	fEnableExtToneDetection;

	tOCT6100_CHANNEL_STATS_TDM		TdmConfig;
	tOCT6100_CHANNEL_STATS_VQE		VqeConfig;
	tOCT6100_CHANNEL_STATS_CODEC	CodecConfig;

	/* Real stats. */
	UINT32	ulNumEchoPathChanges;
	UINT32	ulToneDisablerStatus;

	INT32	lCurrentERL;
	INT32	lCurrentERLE;
	UINT32	ulCurrentEchoDelay;

	INT32	lMaxERL;
	INT32	lMaxERLE;
	UINT32	ulMaxEchoDelay;

	INT32	lRinLevel;
	INT32	lSinLevel;
	INT32	lRinAppliedGain;
	INT32	lSoutAppliedGain;
	INT32	lComfortNoiseLevel;
	
	BOOL	fEchoCancellerConverged;
	BOOL	fSinVoiceDetected;



} tOCT6100_CHANNEL_STATS, *tPOCT6100_CHANNEL_STATS;

typedef struct _OCT6100_CHANNEL_CREATE_BIDIR_
{
	PUINT32	pulBiDirChannelHndl;

	UINT32	ulFirstChannelHndl;
	UINT32	ulSecondChannelHndl;
	


} tOCT6100_CHANNEL_CREATE_BIDIR, *tPOCT6100_CHANNEL_CREATE_BIDIR;

typedef struct _OCT6100_CHANNEL_DESTROY_BIDIR_
{
	UINT32	ulBiDirChannelHndl;

} tOCT6100_CHANNEL_DESTROY_BIDIR, *tPOCT6100_CHANNEL_DESTROY_BIDIR;

typedef struct _OCT6100_CHANNEL_MUTE_
{
	UINT32	ulChannelHndl;
	UINT32	ulPortMask;

} tOCT6100_CHANNEL_MUTE, *tPOCT6100_CHANNEL_MUTE;

typedef struct _OCT6100_CHANNEL_UNMUTE_
{
	UINT32	ulChannelHndl;
	UINT32	ulPortMask;

} tOCT6100_CHANNEL_UNMUTE, *tPOCT6100_CHANNEL_UNMUTE;


/************************** FUNCTION PROTOTYPES  *****************************/


UINT32 Oct6100ChannelOpenDef(
				OUT		tPOCT6100_CHANNEL_OPEN					f_pChannelOpen );
UINT32 Oct6100ChannelOpen(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_OPEN					f_pChannelOpen );

UINT32 Oct6100ChannelCloseDef(
				OUT		tPOCT6100_CHANNEL_CLOSE					f_pChannelClose );
UINT32 Oct6100ChannelClose(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_CLOSE					f_pChannelClose );

UINT32 Oct6100ChannelModifyDef(
				OUT		tPOCT6100_CHANNEL_MODIFY				f_pChannelModify );
UINT32 Oct6100ChannelModify(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_MODIFY				f_pChannelModify );

UINT32 Oct6100ChannelBroadcastTsstAddDef(
				OUT		tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd );
UINT32 Oct6100ChannelBroadcastTsstAdd(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd );

UINT32 Oct6100ChannelBroadcastTsstRemoveDef(
				OUT		tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove );
UINT32 Oct6100ChannelBroadcastTsstRemove(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove );

UINT32 Oct6100ChannelGetStatsDef(
				OUT		tPOCT6100_CHANNEL_STATS					f_pChannelStats );
UINT32 Oct6100ChannelGetStats(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_STATS					f_pChannelStats );

UINT32 Oct6100ChannelCreateBiDirDef(
				OUT		tPOCT6100_CHANNEL_CREATE_BIDIR			f_pChannelCreateBiDir );
UINT32 Oct6100ChannelCreateBiDir(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_CREATE_BIDIR			f_pChannelCreateBiDir );

UINT32 Oct6100ChannelDestroyBiDirDef(
				OUT		tPOCT6100_CHANNEL_DESTROY_BIDIR			f_pChannelDestroyBiDir );
UINT32 Oct6100ChannelDestroyBiDir(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_DESTROY_BIDIR			f_pChannelDestroyBiDir );

UINT32 Oct6100ChannelMuteDef(
				OUT		tPOCT6100_CHANNEL_MUTE					f_pChannelMute );
UINT32 Oct6100ChannelMute(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_MUTE					f_pChannelMute );

UINT32 Oct6100ChannelUnMuteDef(
				OUT		tPOCT6100_CHANNEL_UNMUTE				f_pChannelUnMute );
UINT32 Oct6100ChannelUnMute(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_UNMUTE				f_pChannelUnMute );

#endif /* __OCT6100_CHANNEL_PUB_H__ */
