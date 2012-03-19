/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_channel.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to open, modify and close echo 
	cancellation channels.

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

$Octasic_Revision: 492 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <dahdi/compat/bsd.h>
#else
#ifndef __KERNEL__
#include <stdlib.h>
#include <stdio.h>
#define kmalloc(size, type)    malloc(size)
#define kfree(ptr)             free(ptr)
#define GFP_ATOMIC             0 /* Dummy */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#else
#include <linux/slab.h>
#include <linux/kernel.h>
#endif
#endif

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"
#include "oct6100api/oct6100_apiud.h"

#include "apilib/octapi_llman.h"

#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"
#include "oct6100api/oct6100_mixer_inst.h"
#include "oct6100api/oct6100_tsi_cnct_inst.h"
#include "oct6100api/oct6100_conf_bridge_inst.h"
#include "oct6100api/oct6100_tone_detection_inst.h"
#include "oct6100api/oct6100_phasing_tsst_inst.h"
#include "oct6100api/oct6100_tsst_inst.h"
#include "oct6100api/oct6100_channel_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_tsi_cnct_pub.h"
#include "oct6100api/oct6100_playout_buf_pub.h"
#include "oct6100api/oct6100_phasing_tsst_pub.h"
#include "oct6100api/oct6100_mixer_pub.h"
#include "oct6100api/oct6100_conf_bridge_pub.h"
#include "oct6100api/oct6100_tone_detection_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_debug_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_mixer_priv.h"
#include "oct6100_phasing_tsst_priv.h"
#include "oct6100_tsi_cnct_priv.h"
#include "oct6100_playout_buf_priv.h"
#include "oct6100_conf_bridge_priv.h"
#include "oct6100_tone_detection_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_debug_priv.h"


/****************************  PUBLIC FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelOpen

Description:    This function opens a echo cancellation channel. An echo cancellation
				channel is constituted of two voice stream (RIN/ROUT and SIN/SOUT), and
				an echo cancelling core.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelOpen			Pointer to echo channel open structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelOpenDef
UINT32 Oct6100ChannelOpenDef(
				IN OUT tPOCT6100_CHANNEL_OPEN			f_pChannelOpen )
{
	f_pChannelOpen->pulChannelHndl			= NULL;
	f_pChannelOpen->ulUserChanId			= cOCT6100_INVALID_VALUE;
	f_pChannelOpen->ulEchoOperationMode		= cOCT6100_ECHO_OP_MODE_POWER_DOWN;
	f_pChannelOpen->fEnableToneDisabler		= FALSE;
	f_pChannelOpen->fEnableExtToneDetection		= FALSE;

	/* VQE configuration.*/
	f_pChannelOpen->VqeConfig.fSinDcOffsetRemoval = TRUE;
	f_pChannelOpen->VqeConfig.fRinDcOffsetRemoval = TRUE;
	f_pChannelOpen->VqeConfig.fRinLevelControl	= FALSE;
	f_pChannelOpen->VqeConfig.lRinLevelControlGainDb = 0;
	f_pChannelOpen->VqeConfig.fSoutLevelControl = FALSE;
	f_pChannelOpen->VqeConfig.lSoutLevelControlGainDb = 0;
	f_pChannelOpen->VqeConfig.fRinAutomaticLevelControl	= FALSE;
	f_pChannelOpen->VqeConfig.lRinAutomaticLevelControlTargetDb = -20;
	f_pChannelOpen->VqeConfig.fSoutAutomaticLevelControl = FALSE;
	f_pChannelOpen->VqeConfig.lSoutAutomaticLevelControlTargetDb = -20;
	f_pChannelOpen->VqeConfig.fRinHighLevelCompensation = FALSE;
	f_pChannelOpen->VqeConfig.lRinHighLevelCompensationThresholdDb = -10;
	f_pChannelOpen->VqeConfig.fSoutAdaptiveNoiseReduction = FALSE;
	f_pChannelOpen->VqeConfig.fSoutNoiseBleaching = FALSE;
	f_pChannelOpen->VqeConfig.fSoutConferencingNoiseReduction = FALSE;
	f_pChannelOpen->VqeConfig.ulComfortNoiseMode = cOCT6100_COMFORT_NOISE_NORMAL;
	f_pChannelOpen->VqeConfig.fEnableNlp = TRUE;
	f_pChannelOpen->VqeConfig.fEnableTailDisplacement = FALSE;
	f_pChannelOpen->VqeConfig.ulTailDisplacement = cOCT6100_AUTO_SELECT_TAIL;
	f_pChannelOpen->VqeConfig.ulTailLength = cOCT6100_AUTO_SELECT_TAIL;

	f_pChannelOpen->VqeConfig.fDtmfToneRemoval = FALSE;

	f_pChannelOpen->VqeConfig.fAcousticEcho = FALSE;
	f_pChannelOpen->VqeConfig.lDefaultErlDb = -6;
	f_pChannelOpen->VqeConfig.ulAecTailLength = 128;
	f_pChannelOpen->VqeConfig.lAecDefaultErlDb = 0;
	f_pChannelOpen->VqeConfig.ulNonLinearityBehaviorA = 1;
	f_pChannelOpen->VqeConfig.ulNonLinearityBehaviorB = 0;	
	f_pChannelOpen->VqeConfig.ulDoubleTalkBehavior = cOCT6100_DOUBLE_TALK_BEH_NORMAL;
	f_pChannelOpen->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb = 0;
	f_pChannelOpen->VqeConfig.ulSoutNaturalListenerEnhancementGainDb = 0;
	f_pChannelOpen->VqeConfig.fSoutNaturalListenerEnhancement = FALSE;
	f_pChannelOpen->VqeConfig.fRoutNoiseReduction = FALSE;
	f_pChannelOpen->VqeConfig.lRoutNoiseReductionLevelGainDb = -18;
	f_pChannelOpen->VqeConfig.lAnrSnrEnhancementDb = -18;
	f_pChannelOpen->VqeConfig.ulAnrVoiceNoiseSegregation = 6;
	f_pChannelOpen->VqeConfig.ulToneDisablerVqeActivationDelay = 300;
	f_pChannelOpen->VqeConfig.fEnableMusicProtection = FALSE;
	/* Older images have idle code detection hard-coded to enabled. */
	f_pChannelOpen->VqeConfig.fIdleCodeDetection = TRUE;

	/* TDM configuration.*/
	f_pChannelOpen->TdmConfig.ulRinNumTssts = 1;
	f_pChannelOpen->TdmConfig.ulSinNumTssts = 1;
	f_pChannelOpen->TdmConfig.ulRoutNumTssts = 1;
	f_pChannelOpen->TdmConfig.ulSoutNumTssts = 1;

	f_pChannelOpen->TdmConfig.ulRinTimeslot = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulRinStream = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulRinPcmLaw = cOCT6100_PCM_U_LAW;

	f_pChannelOpen->TdmConfig.ulSinTimeslot = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulSinStream = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulSinPcmLaw = cOCT6100_PCM_U_LAW;

	f_pChannelOpen->TdmConfig.ulRoutTimeslot = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulRoutStream = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulRoutPcmLaw = cOCT6100_PCM_U_LAW;

	f_pChannelOpen->TdmConfig.ulSoutTimeslot = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulSoutStream = cOCT6100_UNASSIGNED;
	f_pChannelOpen->TdmConfig.ulSoutPcmLaw = cOCT6100_PCM_U_LAW;

	/* CODEC configuration.*/
	f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition = cOCT6100_ADPCM_IN_LOW_BITS;

	f_pChannelOpen->CodecConfig.ulEncoderPort = cOCT6100_CHANNEL_PORT_SOUT;
	f_pChannelOpen->CodecConfig.ulEncodingRate = cOCT6100_G711_64KBPS;
	f_pChannelOpen->CodecConfig.ulDecoderPort = cOCT6100_CHANNEL_PORT_RIN;
	f_pChannelOpen->CodecConfig.ulDecodingRate = cOCT6100_G711_64KBPS;

	f_pChannelOpen->CodecConfig.fEnableSilenceSuppression = FALSE;
	f_pChannelOpen->CodecConfig.ulPhasingTsstHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelOpen->CodecConfig.ulPhase = 1;
	f_pChannelOpen->CodecConfig.ulPhasingType = cOCT6100_NO_PHASING;


	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelOpen
UINT32 Oct6100ChannelOpen(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_OPEN			f_pChannelOpen )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelOpenSer( f_pApiInstance, f_pChannelOpen );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelClose

Description:    This function closes an echo canceller channel

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelClose			Pointer to channel close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelCloseDef
UINT32 Oct6100ChannelCloseDef(
				IN OUT tPOCT6100_CHANNEL_CLOSE		f_pChannelClose )
{
	f_pChannelClose->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelClose
UINT32 Oct6100ChannelClose(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN tPOCT6100_CHANNEL_CLOSE			f_pChannelClose )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;

	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelCloseSer( f_pApiInstance, f_pChannelClose );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelModify

Description:    This function will modify the parameter of an echo channel. If 
				the call to this channel allows the channel to go from power down
				to enable, the API will activate it.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pChannelModify			Pointer to echo channel change structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelModifyDef
UINT32 Oct6100ChannelModifyDef(
				IN OUT tPOCT6100_CHANNEL_MODIFY			f_pChannelModify )
{
	f_pChannelModify->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelModify->ulUserChanId = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->ulEchoOperationMode = cOCT6100_KEEP_PREVIOUS_SETTING;
	
	f_pChannelModify->fEnableToneDisabler = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->fApplyToAllChannels = FALSE;

	f_pChannelModify->fDisableToneDetection = FALSE;
	f_pChannelModify->fStopBufferPlayout = FALSE;
	f_pChannelModify->fRemoveConfBridgeParticipant = FALSE;
	f_pChannelModify->fRemoveBroadcastTssts = FALSE;

	f_pChannelModify->fTdmConfigModified = FALSE;
	f_pChannelModify->fVqeConfigModified = FALSE;
	f_pChannelModify->fCodecConfigModified = FALSE;

	/* VQE config. */
	f_pChannelModify->VqeConfig.fSinDcOffsetRemoval = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fRinDcOffsetRemoval = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fRinLevelControl = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lRinLevelControlGainDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutLevelControl = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lSoutLevelControlGainDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fRinAutomaticLevelControl = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lRinAutomaticLevelControlTargetDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutAutomaticLevelControl = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lSoutAutomaticLevelControlTargetDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fRinHighLevelCompensation = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lRinHighLevelCompensationThresholdDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutAdaptiveNoiseReduction = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutNoiseBleaching = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutConferencingNoiseReduction	= cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulComfortNoiseMode = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fEnableNlp = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fEnableTailDisplacement = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulTailDisplacement = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->VqeConfig.fDtmfToneRemoval = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->VqeConfig.fAcousticEcho = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lDefaultErlDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING; 
	f_pChannelModify->VqeConfig.ulAecTailLength = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lAecDefaultErlDb = (INT32)cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulNonLinearityBehaviorA = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulNonLinearityBehaviorB = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulDoubleTalkBehavior = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulSoutNaturalListenerEnhancementGainDb = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fSoutNaturalListenerEnhancement = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fRoutNoiseReduction = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lRoutNoiseReductionLevelGainDb = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.lAnrSnrEnhancementDb = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulAnrVoiceNoiseSegregation = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.ulToneDisablerVqeActivationDelay = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fEnableMusicProtection = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->VqeConfig.fIdleCodeDetection = cOCT6100_KEEP_PREVIOUS_SETTING;

	/* TDM config. */
	f_pChannelModify->TdmConfig.ulRinNumTssts = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSinNumTssts = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulRoutNumTssts = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSoutNumTssts = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->TdmConfig.ulRinTimeslot = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulRinStream = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulRinPcmLaw = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->TdmConfig.ulSinTimeslot = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSinStream = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSinPcmLaw = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->TdmConfig.ulRoutTimeslot = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulRoutStream = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulRoutPcmLaw = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->TdmConfig.ulSoutTimeslot = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSoutStream = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->TdmConfig.ulSoutPcmLaw = cOCT6100_KEEP_PREVIOUS_SETTING;

	/* CODEC config. */
	f_pChannelModify->CodecConfig.ulEncoderPort = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulEncodingRate = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulDecoderPort = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulDecodingRate = cOCT6100_KEEP_PREVIOUS_SETTING;

	f_pChannelModify->CodecConfig.fEnableSilenceSuppression = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulPhasingTsstHndl = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulPhase = cOCT6100_KEEP_PREVIOUS_SETTING;
	f_pChannelModify->CodecConfig.ulPhasingType = cOCT6100_KEEP_PREVIOUS_SETTING;


	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelModify
UINT32 Oct6100ChannelModify(
				IN tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_MODIFY	f_pChannelModify )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Check the apply to all channels flag first. */
		if ( f_pChannelModify->fApplyToAllChannels != TRUE &&
			f_pChannelModify->fApplyToAllChannels != FALSE )
			return cOCT6100_ERR_CHANNEL_APPLY_TO_ALL_CHANNELS;

		/* Check if must apply modification to all channels. */
		if ( f_pChannelModify->fApplyToAllChannels == TRUE )
		{
			tPOCT6100_API_CHANNEL	pChanEntry;
			UINT16					usChanIndex;

			/* Loop through all channels and look for the opened ones. */
			for ( usChanIndex = 0; usChanIndex < f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels; usChanIndex++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, usChanIndex );

				/* Check if this one is opened. */
				if ( pChanEntry->fReserved == TRUE )
				{
					/* Channel is opened.  Form handle and call actual modify function. */
					f_pChannelModify->ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | usChanIndex;

					/* Call the serialized function. */
					ulFncRes = Oct6100ChannelModifySer( f_pApiInstance, f_pChannelModify );
					if ( ulFncRes != cOCT6100_ERR_OK )
						break;
				}
			}
		}
		else /* if ( f_pChannelModify->fApplyToAllChannels == FALSE ) */
		{
			/* Call the serialized function. */
			ulFncRes = Oct6100ChannelModifySer( f_pApiInstance, f_pChannelModify );
		}
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelCreateBiDir

Description:    This function creates a bidirectional channel using two standard
				echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelCreateBiDir	Pointer to channel create BiDir structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelCreateBiDirDef
UINT32 Oct6100ChannelCreateBiDirDef(
			IN OUT	tPOCT6100_CHANNEL_CREATE_BIDIR		f_pChannelCreateBiDir )
{
	f_pChannelCreateBiDir->pulBiDirChannelHndl = NULL;

	f_pChannelCreateBiDir->ulFirstChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelCreateBiDir->ulSecondChannelHndl = cOCT6100_INVALID_HANDLE;


	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelCreateBiDir
UINT32 Oct6100ChannelCreateBiDir(	
			IN tPOCT6100_INSTANCE_API					f_pApiInstance,
			IN OUT tPOCT6100_CHANNEL_CREATE_BIDIR		f_pChannelCreateBiDir )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;
	
	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelCreateBiDirSer( f_pApiInstance, f_pChannelCreateBiDir );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif




/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelDestroyBiDir

Description:    This function destroys a bidirectional channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelDestroyBiDir	Pointer to channel destroy BiDir structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelDestroyBiDirDef
UINT32 Oct6100ChannelDestroyBiDirDef(
			IN OUT	tPOCT6100_CHANNEL_DESTROY_BIDIR		f_pChannelDestroyBiDir )
{
	f_pChannelDestroyBiDir->ulBiDirChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelDestroyBiDir
UINT32 Oct6100ChannelDestroyBiDir(	
			IN tPOCT6100_INSTANCE_API					f_pApiInstance,
			IN OUT tPOCT6100_CHANNEL_DESTROY_BIDIR		f_pChannelDestroyBiDir )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelDestroyBiDirSer( f_pApiInstance, f_pChannelDestroyBiDir );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelBroadcastTsstAdd

Description:    This function adds a TSST to one of the two output ports of a channel.
				This TSST can never be modified by a call to Oct6100ChannelModify.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pChannelBroadcastTsstAdd	Pointer to the an Add Broadcast TSST structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelBroadcastTsstAddDef
UINT32 Oct6100ChannelBroadcastTsstAddDef(
			tPOCT6100_CHANNEL_BROADCAST_TSST_ADD		f_pChannelBroadcastTsstAdd )
{
	f_pChannelBroadcastTsstAdd->ulChannelHndl = cOCT6100_INVALID_HANDLE;

	f_pChannelBroadcastTsstAdd->ulPort = cOCT6100_INVALID_PORT;
	f_pChannelBroadcastTsstAdd->ulTimeslot = cOCT6100_INVALID_TIMESLOT;
	f_pChannelBroadcastTsstAdd->ulStream = cOCT6100_INVALID_STREAM;

	return cOCT6100_ERR_OK;

}
#endif

#if !SKIP_Oct6100ChannelBroadcastTsstAdd
UINT32 Oct6100ChannelBroadcastTsstAdd(
			tPOCT6100_INSTANCE_API						f_pApiInstance,
			tPOCT6100_CHANNEL_BROADCAST_TSST_ADD		f_pChannelBroadcastTsstAdd )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelBroadcastTsstAddSer( f_pApiInstance, f_pChannelBroadcastTsstAdd );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;	
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelBroadcastTsstRemove

Description:    This function removes a TSST from one of the two output ports of a channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance					Pointer to API instance. This memory is used to keep
								the present state of the chip and all its resources.

f_pChannelBroadcastTsstRemove	Pointer to the a Remove Broadcast TSST structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelBroadcastTsstRemoveDef
UINT32 Oct6100ChannelBroadcastTsstRemoveDef(
			tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE		f_pChannelBroadcastTsstRemove )
{
	f_pChannelBroadcastTsstRemove->ulChannelHndl = cOCT6100_INVALID_HANDLE;

	f_pChannelBroadcastTsstRemove->ulPort = cOCT6100_INVALID_PORT;
	f_pChannelBroadcastTsstRemove->ulTimeslot = cOCT6100_INVALID_TIMESLOT;
	f_pChannelBroadcastTsstRemove->ulStream = cOCT6100_INVALID_STREAM;
	
	f_pChannelBroadcastTsstRemove->fRemoveAll = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ChannelBroadcastTsstRemove
UINT32 Oct6100ChannelBroadcastTsstRemove(
			tPOCT6100_INSTANCE_API						f_pApiInstance,
			tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE		f_pChannelBroadcastTsstRemove )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelBroadcastTsstRemoveSer( f_pApiInstance, f_pChannelBroadcastTsstRemove );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;	
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelGetStats

Description:    This function retrieves all the config and stats related to the channel
				designated by ulChannelHndl.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelStats			Pointer to a tOCT6100_CHANNEL_STATS structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelGetStatsDef
UINT32 Oct6100ChannelGetStatsDef(
				IN OUT tPOCT6100_CHANNEL_STATS			f_pChannelStats )
{
	f_pChannelStats->fResetStats = FALSE;

	f_pChannelStats->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelStats->ulUserChanId = cOCT6100_INVALID_STAT;
	f_pChannelStats->ulEchoOperationMode = cOCT6100_INVALID_STAT;
	f_pChannelStats->fEnableToneDisabler = FALSE;
	f_pChannelStats->ulMutePortsMask = cOCT6100_CHANNEL_MUTE_PORT_NONE;
	f_pChannelStats->fEnableExtToneDetection = FALSE;

	/* VQE configuration.*/
	f_pChannelStats->VqeConfig.fEnableNlp = FALSE;
	f_pChannelStats->VqeConfig.fEnableTailDisplacement = FALSE;
	f_pChannelStats->VqeConfig.ulTailDisplacement = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulTailLength = cOCT6100_INVALID_STAT;

	f_pChannelStats->VqeConfig.fSinDcOffsetRemoval = FALSE;
	f_pChannelStats->VqeConfig.fRinDcOffsetRemoval = FALSE;
	f_pChannelStats->VqeConfig.fRinLevelControl = FALSE;
	f_pChannelStats->VqeConfig.fSoutLevelControl = FALSE;
	f_pChannelStats->VqeConfig.fRinAutomaticLevelControl = FALSE;
	f_pChannelStats->VqeConfig.fSoutAutomaticLevelControl = FALSE;
	f_pChannelStats->VqeConfig.fRinHighLevelCompensation = FALSE;
	f_pChannelStats->VqeConfig.fAcousticEcho = FALSE;
	f_pChannelStats->VqeConfig.fSoutAdaptiveNoiseReduction = FALSE;
	f_pChannelStats->VqeConfig.fDtmfToneRemoval = FALSE;

	f_pChannelStats->VqeConfig.fSoutNoiseBleaching = FALSE;
	f_pChannelStats->VqeConfig.fSoutConferencingNoiseReduction = FALSE;
	
	f_pChannelStats->VqeConfig.ulComfortNoiseMode = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulNonLinearityBehaviorA = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulNonLinearityBehaviorB = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulDoubleTalkBehavior = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.lRinLevelControlGainDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lSoutLevelControlGainDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lRinAutomaticLevelControlTargetDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lSoutAutomaticLevelControlTargetDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lRinHighLevelCompensationThresholdDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lDefaultErlDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lAecDefaultErlDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.ulAecTailLength = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulSoutNaturalListenerEnhancementGainDb = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.fSoutNaturalListenerEnhancement = FALSE;
	f_pChannelStats->VqeConfig.fRoutNoiseReduction = FALSE;
	f_pChannelStats->VqeConfig.lRoutNoiseReductionLevelGainDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.lAnrSnrEnhancementDb = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->VqeConfig.ulAnrVoiceNoiseSegregation = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.ulToneDisablerVqeActivationDelay = cOCT6100_INVALID_STAT;
	f_pChannelStats->VqeConfig.fEnableMusicProtection = FALSE;
	f_pChannelStats->VqeConfig.fIdleCodeDetection = FALSE;



	/* TDM configuration.*/
	f_pChannelStats->TdmConfig.ulMaxBroadcastTssts = 0;
	f_pChannelStats->TdmConfig.fMoreRoutBroadcastTssts = FALSE;
	f_pChannelStats->TdmConfig.fMoreSoutBroadcastTssts = FALSE;

	f_pChannelStats->TdmConfig.ulNumRoutBroadcastTssts = 0;
	f_pChannelStats->TdmConfig.ulNumSoutBroadcastTssts = 0;
	
	f_pChannelStats->TdmConfig.ulRinNumTssts = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSinNumTssts = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulRoutNumTssts = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSoutNumTssts = cOCT6100_INVALID_STAT;

	f_pChannelStats->TdmConfig.ulRinTimeslot = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulRinStream = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulRinPcmLaw = cOCT6100_INVALID_STAT;

	f_pChannelStats->TdmConfig.ulSinTimeslot = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSinStream = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSinPcmLaw = cOCT6100_INVALID_STAT;

	f_pChannelStats->TdmConfig.ulRoutTimeslot = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulRoutStream = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulRoutPcmLaw = cOCT6100_INVALID_STAT;
	
	f_pChannelStats->TdmConfig.pulRoutBroadcastTimeslot = NULL;
	f_pChannelStats->TdmConfig.pulRoutBroadcastStream = NULL;
	
	f_pChannelStats->TdmConfig.ulSoutTimeslot = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSoutStream = cOCT6100_INVALID_STAT;
	f_pChannelStats->TdmConfig.ulSoutPcmLaw = cOCT6100_INVALID_STAT;

	f_pChannelStats->TdmConfig.pulSoutBroadcastTimeslot = NULL;
	f_pChannelStats->TdmConfig.pulSoutBroadcastStream = NULL;
	

	/* CODEC configuration.*/
	f_pChannelStats->CodecConfig.ulAdpcmNibblePosition = cOCT6100_INVALID_STAT;

	f_pChannelStats->CodecConfig.ulEncoderPort = cOCT6100_INVALID_STAT;
	f_pChannelStats->CodecConfig.ulEncodingRate = cOCT6100_INVALID_STAT;
	f_pChannelStats->CodecConfig.ulDecoderPort = cOCT6100_INVALID_STAT;
	f_pChannelStats->CodecConfig.ulDecodingRate = cOCT6100_INVALID_STAT;

	f_pChannelStats->CodecConfig.fEnableSilenceSuppression = FALSE;
	f_pChannelStats->CodecConfig.ulPhasingTsstHndl = cOCT6100_INVALID_STAT;
	f_pChannelStats->CodecConfig.ulPhase = cOCT6100_INVALID_STAT;
	f_pChannelStats->CodecConfig.ulPhasingType = cOCT6100_INVALID_STAT;

	f_pChannelStats->ulNumEchoPathChanges = cOCT6100_INVALID_STAT;
	f_pChannelStats->ulToneDisablerStatus = cOCT6100_INVALID_STAT;
	f_pChannelStats->fEchoCancellerConverged = FALSE;
	f_pChannelStats->fSinVoiceDetected = FALSE;
	f_pChannelStats->lCurrentERL  = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lCurrentERLE = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->ulCurrentEchoDelay = cOCT6100_INVALID_STAT;
	
	f_pChannelStats->lMaxERL  = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lMaxERLE = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->ulMaxEchoDelay = cOCT6100_INVALID_STAT;
	
	f_pChannelStats->lRinLevel = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lSinLevel = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lRinAppliedGain = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lSoutAppliedGain = cOCT6100_INVALID_SIGNED_STAT;
	f_pChannelStats->lComfortNoiseLevel = cOCT6100_INVALID_SIGNED_STAT;
	


	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelGetStats
UINT32 Oct6100ChannelGetStats(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_STATS			f_pChannelStats )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ApiChannelGetStatsSer( f_pApiInstance, f_pChannelStats );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelMute

Description:    This function mutes some or all of the ports designated by 
				ulChannelHndl.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelMute			Pointer to a tPOCT6100_CHANNEL_MUTE structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelMuteDef
UINT32 Oct6100ChannelMuteDef(
				IN OUT tPOCT6100_CHANNEL_MUTE			f_pChannelMute )
{
	f_pChannelMute->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelMute->ulPortMask = cOCT6100_CHANNEL_MUTE_PORT_NONE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelMute
UINT32 Oct6100ChannelMute(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_MUTE			f_pChannelMute )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelMuteSer( f_pApiInstance, f_pChannelMute );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelUnMute

Description:    This function unmutes some or all of the ports designated by 
				ulChannelHndl.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelUnMute		Pointer to a tPOCT6100_CHANNEL_UNMUTE structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelUnMuteDef
UINT32 Oct6100ChannelUnMuteDef(
				IN OUT tPOCT6100_CHANNEL_UNMUTE			f_pChannelUnMute )
{
	f_pChannelUnMute->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pChannelUnMute->ulPortMask = cOCT6100_CHANNEL_MUTE_PORT_NONE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChannelUnMute
UINT32 Oct6100ChannelUnMute(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_UNMUTE			f_pChannelUnMute )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChannelUnMuteSer( f_pApiInstance, f_pChannelUnMute );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif

/****************************  PRIVATE FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetChannelsEchoSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of the ECHO memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetChannelsEchoSwSizes
UINT32 Oct6100ApiGetChannelsEchoSwSizes(
				IN	tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT	tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;
	UINT32	ulMaxChannels;

	ulMaxChannels = f_pOpenChip->ulMaxChannels;

	if ( f_pOpenChip->fEnableChannelRecording == TRUE && ulMaxChannels != 672 )
		ulMaxChannels++;

	/* Determine the amount of memory required for the API echo channel list.*/
	f_pInstSizes->ulChannelList			= ulMaxChannels * sizeof( tOCT6100_API_CHANNEL );	/* Add one for the record channel.*/
	f_pInstSizes->ulBiDirChannelList	= f_pOpenChip->ulMaxBiDirChannels * sizeof( tOCT6100_API_BIDIR_CHANNEL );
	if ( ulMaxChannels > 0 )
	{
		/* Calculate memory needed for ECHO memory allocation */
		ulResult = OctapiLlmAllocGetSize( ulMaxChannels, &f_pInstSizes->ulChannelAlloc );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_0;
	}
	else
	{
		f_pInstSizes->ulChannelAlloc = 0;
	}
	if ( f_pOpenChip->ulMaxBiDirChannels > 0 )
	{
		/* Calculate memory needed for ECHO memory allocation */
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxBiDirChannels, &f_pInstSizes->ulBiDirChannelAlloc );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_0;
	}
	else
	{
		f_pInstSizes->ulBiDirChannelAlloc = 0;
	}

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulChannelList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulChannelAlloc, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulBiDirChannelList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulBiDirChannelAlloc, ulTempVar )
	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiChannelsEchoSwInit

Description:    Initializes all elements of the instance structure associated
				to the ECHO memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiChannelsEchoSwInit
UINT32 Oct6100ApiChannelsEchoSwInit(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_CHANNEL			pChannelsEchoList;
	tPOCT6100_API_BIDIR_CHANNEL		pBiDirChannelsList;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	UINT16	usMaxChannels;
	PVOID	pEchoChanAlloc;
	PVOID	pBiDirChanAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of the API instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize the ECHO channel API list.*/
	usMaxChannels = pSharedInfo->ChipConfig.usMaxChannels;

	/* add a channel to initialize if the recording is activated. */
	if ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
		usMaxChannels++;

	/* Set all entries in the ADCPM channel list to unused. */
	mOCT6100_GET_CHANNEL_LIST_PNT( pSharedInfo, pChannelsEchoList );
	
	/* Initialize the API ECHO channels allocation software to "all free". */
	if ( usMaxChannels > 0 )
	{
		/* Clear the memory */
		Oct6100UserMemSet( pChannelsEchoList, 0x00, sizeof(tOCT6100_API_CHANNEL) * usMaxChannels );

		mOCT6100_GET_CHANNEL_ALLOC_PNT( pSharedInfo, pEchoChanAlloc )
		
		ulResult = OctapiLlmAllocInit( &pEchoChanAlloc, usMaxChannels );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_1;
	}

	mOCT6100_GET_BIDIR_CHANNEL_LIST_PNT( pSharedInfo, pBiDirChannelsList );	

	if ( pSharedInfo->ChipConfig.usMaxBiDirChannels > 0 )
	{
		/* Clear the memory */
		Oct6100UserMemSet( pBiDirChannelsList, 0x00, sizeof(tOCT6100_API_BIDIR_CHANNEL) * pSharedInfo->ChipConfig.usMaxBiDirChannels );
		
		mOCT6100_GET_BIDIR_CHANNEL_ALLOC_PNT( pSharedInfo, pBiDirChanAlloc )
		
		ulResult = OctapiLlmAllocInit( &pBiDirChanAlloc, pSharedInfo->ChipConfig.usMaxBiDirChannels );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_A9;
		
	}

	return cOCT6100_ERR_OK;
}
#endif









/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelOpenSer

Description:    Opens a echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelOpen			Pointer to channel configuration structure.  Then handle
						identifying the buffer in all future function calls is
						returned in this structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelOpenSer
UINT32 Oct6100ChannelOpenSer(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_OPEN		f_pChannelOpen )
{
	tOCT6100_API_ECHO_CHAN_INDEX		*ChannelIndexConf;
	UINT32	ulResult;

	ChannelIndexConf = kmalloc(sizeof(*ChannelIndexConf), GFP_ATOMIC);

	if (!ChannelIndexConf)
		return cOCT6100_ERR_FATAL_0;

	/* Check the user's configuration of the echo cancellation channel for errors. */
	ulResult = Oct6100ApiCheckChannelParams( f_pApiInstance, f_pChannelOpen, ChannelIndexConf );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Reserve all resources needed by the echo cancellation channel. */
	ulResult = Oct6100ApiReserveChannelResources( f_pApiInstance, f_pChannelOpen, ChannelIndexConf );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Write all necessary structures to activate the echo cancellation channel. */
	ulResult = Oct6100ApiWriteChannelStructs( f_pApiInstance, f_pChannelOpen, ChannelIndexConf );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Update the new echo cancellation channels's entry in the ECHO channel list. */
	ulResult = Oct6100ApiUpdateChannelEntry( f_pApiInstance, f_pChannelOpen, ChannelIndexConf );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	kfree(ChannelIndexConf);
	return cOCT6100_ERR_OK;

out:
	kfree(ChannelIndexConf);
	return ulResult;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChannelParams

Description:    Checks the user's echo cancellation channel open configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelOpen			Pointer to echo cancellation channel open configuration structure.
f_pChanIndexConf		Pointer to a structure used to store the multiple resources indexes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChannelParams
UINT32 Oct6100ApiCheckChannelParams(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN tPOCT6100_CHANNEL_OPEN				f_pChannelOpen,
				OUT tPOCT6100_API_ECHO_CHAN_INDEX		f_pChanIndexConf )
{
	tPOCT6100_CHANNEL_OPEN_TDM		pTdmConfig;
	tPOCT6100_CHANNEL_OPEN_VQE		pVqeConfig;
	tPOCT6100_CHANNEL_OPEN_CODEC	pCodecConfig;
	UINT32	ulDecoderNumTssts;
	UINT32	ulResult;

	/* Dereference the configuration structure for clearer code and faster access.*/
	pTdmConfig	 = &f_pChannelOpen->TdmConfig;
	pVqeConfig	 = &f_pChannelOpen->VqeConfig;
	pCodecConfig = &f_pChannelOpen->CodecConfig;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels == 0 )
		return cOCT6100_ERR_CHANNEL_DISABLED;

	if ( f_pChannelOpen->pulChannelHndl == NULL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	if ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NORMAL &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_HT_FREEZE &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_HT_RESET &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_POWER_DOWN &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_EXTERNAL &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION &&
		 f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NO_ECHO )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE;

	/* Check the 2100Hz echo disabling configuration.*/
	if ( f_pChannelOpen->fEnableToneDisabler != TRUE && 
		 f_pChannelOpen->fEnableToneDisabler != FALSE  )
		return cOCT6100_ERR_CHANNEL_TONE_DISABLER_ENABLE;
	
	/* Check the extended Tone Detection flag value.*/
	if ( f_pChannelOpen->fEnableExtToneDetection != TRUE &&
		 f_pChannelOpen->fEnableExtToneDetection != FALSE )
		return cOCT6100_ERR_CHANNEL_ENABLE_EXT_TONE_DETECTION;

	/* Check that extented tone detection is actually enabled by the user. */
	if ( ( f_pChannelOpen->fEnableExtToneDetection == TRUE ) &&
		( f_pApiInstance->pSharedInfo->ChipConfig.fEnableExtToneDetection == FALSE ) )
		return cOCT6100_ERR_CHANNEL_EXT_TONE_DETECTION_DISABLED;



	/*==============================================================================*/
	/* Check the TDM configuration parameters.*/

	ulResult = Oct6100ApiCheckTdmConfig( f_pApiInstance, pTdmConfig );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/


	/*==============================================================================*/
	/* Now validate the VQE parameters */

	ulResult = Oct6100ApiCheckVqeConfig( f_pApiInstance, pVqeConfig, f_pChannelOpen->fEnableToneDisabler );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Verify if the echo operation mode selected can be applied. */
	if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
		&& ( pVqeConfig->fEnableNlp == FALSE ) )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE_NLP_REQUIRED;
	
	if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION )
		&& ( pVqeConfig->fEnableNlp == FALSE ) )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE_NLP_REQUIRED;

	/* Comfort noise must be activated for speech recognition mode to work. */
	if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION )
		&& ( pVqeConfig->ulComfortNoiseMode == cOCT6100_COMFORT_NOISE_OFF ) )
		return cOCT6100_ERR_CHANNEL_COMFORT_NOISE_REQUIRED;

	/*==============================================================================*/

	/*==============================================================================*/
	/* Finally, validate the CODEC configuration.*/

	if ( pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
		ulDecoderNumTssts = pTdmConfig->ulRinNumTssts;
	else /* pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
		ulDecoderNumTssts  = pTdmConfig->ulSinNumTssts;
	
	ulResult = Oct6100ApiCheckCodecConfig( f_pApiInstance, pCodecConfig, ulDecoderNumTssts, &f_pChanIndexConf->usPhasingTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;



	/* make sure that if silence suppression is activated, the NLP is enabled.*/
	if ( pCodecConfig->fEnableSilenceSuppression == TRUE && pVqeConfig->fEnableNlp == FALSE )
		return cOCT6100_ERR_CHANNEL_SIL_SUP_NLP_MUST_BE_ENABLED;
	
	/* Verify if law conversion is allowed. */
	if ( pCodecConfig->ulEncoderPort == cOCT6100_NO_ENCODING ||
		 pCodecConfig->ulDecoderPort == cOCT6100_NO_DECODING )
	{
		/* No law conversion can occurs if one ADPCM memory is not reserved.*/
		if ( pTdmConfig->ulRinPcmLaw != pTdmConfig->ulRoutPcmLaw )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_LAW_TRANSLATION;

		if ( pTdmConfig->ulSinPcmLaw != pTdmConfig->ulSoutPcmLaw )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_LAW_TRANSLATION;
	}

	/* Verify if the config supports extended tone detection.*/
	if ( f_pChannelOpen->fEnableExtToneDetection == TRUE )
	{
		if ( pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
			return cOCT6100_ERR_CHANNEL_EXT_TONE_DETECTION_DECODER_PORT;
	}
	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveChannelResources

Description:    Reserves all resources needed for the new channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pChannelOpen			Pointer to echo cancellation channel configuration structure.
f_pulChannelIndex		Allocated entry in ECHO channel list.
f_pChanIndexConf		Pointer to a structure used to store the multiple resources indexes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveChannelResources
UINT32 Oct6100ApiReserveChannelResources(	
				IN  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN  tPOCT6100_CHANNEL_OPEN				f_pChannelOpen,
				OUT tPOCT6100_API_ECHO_CHAN_INDEX		f_pChanIndexConf )
{
	tPOCT6100_CHANNEL_OPEN_TDM		pTdmConfig;
	tPOCT6100_CHANNEL_OPEN_CODEC	pCodecConfig;

	UINT32	ulResult;
	UINT32	ulTempVar;
	UINT32	ulFreeMixerEventCnt;

	BOOL	fRinTsstEntry = FALSE;
	BOOL	fSinTsstEntry = FALSE;
	BOOL	fRoutTsstEntry = FALSE;
	BOOL	fSoutTsstEntry = FALSE;

	BOOL	fRinRoutTsiMemEntry = FALSE;
	BOOL	fSinSoutTsiMemEntry = FALSE;

	BOOL	fEchoChanEntry = FALSE;

	PUINT16	pusRinRoutConversionMemIndex = NULL;
	PUINT16	pusSinSoutConversionMemIndex = NULL;
	BOOL	fRinRoutConversionMemEntry = FALSE;
	BOOL	fSinSoutConversionMemEntry = FALSE;

	BOOL	fExtToneChanEntry	= FALSE;
	BOOL	fExtToneTsiEntry	= FALSE;
	BOOL	fExtToneMixerEntry	= FALSE;

	/* Obtain a local pointer to the configuration structures.*/
	pTdmConfig		= &f_pChannelOpen->TdmConfig;
	pCodecConfig	= &f_pChannelOpen->CodecConfig;

	/*===============================================================================*/
	/* Reserve Echo and TSI entries. */

	ulResult = Oct6100ApiReserveEchoEntry( f_pApiInstance, 
										   &f_pChanIndexConf->usEchoChanIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		fEchoChanEntry = TRUE;

		/* Set the echo, encoder and decoder memory indexes.*/
		f_pChanIndexConf->usEchoMemIndex = f_pChanIndexConf->usEchoChanIndex;
		
		/* Reserve an entry for the RIN/ROUT tsi chariot memory. */
		ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, 
												 &f_pChanIndexConf->usRinRoutTsiMemIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			fRinRoutTsiMemEntry = TRUE;

			/* Reserve an entry for the SIN/SOUT tsi chariot memory. */
			ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, 
													 &f_pChanIndexConf->usSinSoutTsiMemIndex );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				fSinSoutTsiMemEntry = TRUE;

				/* Reserve an ADPCM memory block for compression if required.*/
				if ( pCodecConfig->ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
				{
					pusRinRoutConversionMemIndex = &f_pChanIndexConf->usRinRoutConversionMemIndex;
				}
				else if ( pCodecConfig->ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT )
				{
					pusSinSoutConversionMemIndex = &f_pChanIndexConf->usSinSoutConversionMemIndex;
				}

				/* Reserve an ADPCM memory block for decompression if required.*/
				if ( pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
				{
					pusRinRoutConversionMemIndex = &f_pChanIndexConf->usRinRoutConversionMemIndex;
				}
				else if ( pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN )
				{
					pusSinSoutConversionMemIndex = &f_pChanIndexConf->usSinSoutConversionMemIndex;
				}


				/* Reserve the conversion memories. */
				if ( pusRinRoutConversionMemIndex != NULL )
				{
					/* Reserve a conversion memory for the Rin/Rout stream. */
					ulResult = Oct6100ApiReserveConversionMemEntry( f_pApiInstance, 
																	pusRinRoutConversionMemIndex );
					if ( ulResult == cOCT6100_ERR_OK )
					{
						fRinRoutConversionMemEntry = TRUE;
					}
				}
				else
				{
					/* No conversion memory reserved.*/
					f_pChanIndexConf->usRinRoutConversionMemIndex = cOCT6100_INVALID_INDEX;
				}

				if ( ( pusSinSoutConversionMemIndex != NULL ) && 
					 ( ulResult == cOCT6100_ERR_OK ) )
				{
					/* Reserve a conversion memory for the Sin/Sout stream. */
					ulResult = Oct6100ApiReserveConversionMemEntry( f_pApiInstance, 
																	pusSinSoutConversionMemIndex );
					if ( ulResult == cOCT6100_ERR_OK )
					{
						fSinSoutConversionMemEntry = TRUE;
					}
				}
				else
				{
					/* No conversion memory reserved.*/
					f_pChanIndexConf->usSinSoutConversionMemIndex = cOCT6100_INVALID_INDEX;
				}

				/* Reserve any resources required if the extended Tone detection is enabled.*/
				if ( f_pChannelOpen->fEnableExtToneDetection == TRUE )
				{
					ulResult = Oct6100ApiReserveEchoEntry( f_pApiInstance, 
														   &f_pChanIndexConf->usExtToneChanIndex );
					if ( ulResult == cOCT6100_ERR_OK )
					{
						fExtToneChanEntry = TRUE;
						
						/* Reserve an entry for the TSI chariot memory for the additionnal channel. */
						ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, 
																 &f_pChanIndexConf->usExtToneTsiIndex );
						if ( ulResult == cOCT6100_ERR_OK )
						{
							fExtToneTsiEntry = TRUE;

							/* Reserve an entry for the TSI chariot memory for the additionnal channel. */
							ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, 
																		 &f_pChanIndexConf->usExtToneMixerIndex );
							if ( ulResult == cOCT6100_ERR_OK )
								fExtToneMixerEntry = TRUE;
						}
					}
				}
				else
				{
					f_pChanIndexConf->usExtToneChanIndex	= cOCT6100_INVALID_INDEX;
					f_pChanIndexConf->usExtToneMixerIndex	= cOCT6100_INVALID_INDEX;
					f_pChanIndexConf->usExtToneTsiIndex		= cOCT6100_INVALID_INDEX;
				}
			}
			else
			{
				/* Return an error other then a Fatal.*/
				ulResult = cOCT6100_ERR_CHANNEL_OUT_OF_TSI_MEMORY;
			}
		}
		else
		{
			/* Return an error other then a Fatal.*/
			ulResult = cOCT6100_ERR_CHANNEL_OUT_OF_TSI_MEMORY;
		}
	}

	/*===============================================================================*/

	/*===============================================================================*/
	/* Now reserve the TSST entries if required.*/

	/* Reserve the Rin TSST entry */	
	if ( (ulResult == cOCT6100_ERR_OK ) &&
		 (pTdmConfig->ulRinTimeslot != cOCT6100_UNASSIGNED && 
		  pTdmConfig->ulRinStream != cOCT6100_UNASSIGNED) )
	{
		ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
										  pTdmConfig->ulRinTimeslot, 
										  pTdmConfig->ulRinStream, 
										  pTdmConfig->ulRinNumTssts, 
										  cOCT6100_INPUT_TSST,
										  &f_pChanIndexConf->usRinTsstIndex, 
										  NULL );
		if ( ulResult == cOCT6100_ERR_OK )
			fRinTsstEntry = TRUE;
	}
	else
	{
		f_pChanIndexConf->usRinTsstIndex = cOCT6100_INVALID_INDEX;
	}

		
	if ( (ulResult == cOCT6100_ERR_OK ) &&
		 (pTdmConfig->ulSinTimeslot != cOCT6100_UNASSIGNED && 
		  pTdmConfig->ulSinStream != cOCT6100_UNASSIGNED) )
	{
		/* Reserve the Sin TSST entry.*/
		ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
										  pTdmConfig->ulSinTimeslot, 
										  pTdmConfig->ulSinStream, 
										  pTdmConfig->ulSinNumTssts, 
										  cOCT6100_INPUT_TSST,
										  &f_pChanIndexConf->usSinTsstIndex, 
										  NULL );
		if ( ulResult == cOCT6100_ERR_OK )
			fSinTsstEntry = TRUE;
	}
	else 
	{
		f_pChanIndexConf->usSinTsstIndex = cOCT6100_INVALID_INDEX;
	}

	if ( (ulResult == cOCT6100_ERR_OK ) &&
		 (pTdmConfig->ulRoutTimeslot != cOCT6100_UNASSIGNED && 
		  pTdmConfig->ulRoutStream != cOCT6100_UNASSIGNED) )
	{
		/* Reserve the Rout TSST entry.*/
		ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
										  pTdmConfig->ulRoutTimeslot, 
										  pTdmConfig->ulRoutStream, 
										  pTdmConfig->ulRoutNumTssts, 
										  cOCT6100_OUTPUT_TSST,
										  &f_pChanIndexConf->usRoutTsstIndex, 
										  NULL );
		if ( ulResult == cOCT6100_ERR_OK )
			fRoutTsstEntry = TRUE;
	}
	else
	{
		f_pChanIndexConf->usRoutTsstIndex = cOCT6100_INVALID_INDEX;
	}

				
	if ( (ulResult == cOCT6100_ERR_OK ) &&
		 (pTdmConfig->ulSoutTimeslot != cOCT6100_UNASSIGNED && 
		  pTdmConfig->ulSoutStream != cOCT6100_UNASSIGNED) )
	{
		/* Reserve the Sout TSST entry.*/
		ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
										  pTdmConfig->ulSoutTimeslot, 
										  pTdmConfig->ulSoutStream, 
										  pTdmConfig->ulSoutNumTssts, 
										  cOCT6100_OUTPUT_TSST,
										  &f_pChanIndexConf->usSoutTsstIndex, 
										  NULL );
		if ( ulResult == cOCT6100_ERR_OK )
			fSoutTsstEntry = TRUE;
	}
	else 
	{
		f_pChanIndexConf->usSoutTsstIndex = cOCT6100_INVALID_INDEX;
	}

	/*===============================================================================*/
	

	/*===============================================================================*/
	/* Check if there are a couple of mixer events available for us. */

	if ( ulResult == cOCT6100_ERR_OK )
	{
		UINT32 ulMixerEventCntNeeded = 0;

		/* Calculate how many mixer events are needed. */
		if ( f_pChanIndexConf->usRinTsstIndex == cOCT6100_INVALID_INDEX )
			ulMixerEventCntNeeded++;

		if ( f_pChanIndexConf->usSinTsstIndex == cOCT6100_INVALID_INDEX )
			ulMixerEventCntNeeded++;

		/* If at least 1 mixer event is needed, check if those are available. */
		if ( ulMixerEventCntNeeded != 0 )
		{
			ulResult = Oct6100ApiGetFreeMixerEventCnt( f_pApiInstance, &ulFreeMixerEventCnt );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				/* The API might need more mixer events if the ports have to be muted. */
				/* Check if these are available. */
				if ( ulFreeMixerEventCnt < ulMixerEventCntNeeded )
				{
					ulResult = cOCT6100_ERR_CHANNEL_OUT_OF_MIXER_EVENTS;
				}
			}
		}
	}

	/*===============================================================================*/


	/*===============================================================================*/
	/* Release the resources if something went wrong */		
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		/*===============================================================================*/
		/* Release the previously reserved resources .*/
		if( fRinTsstEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance,  
											   pTdmConfig->ulRinTimeslot,
											   pTdmConfig->ulRinStream,
											   pTdmConfig->ulRinNumTssts, 
											   cOCT6100_INPUT_TSST,
											   cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fSinTsstEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance,  
											   pTdmConfig->ulSinTimeslot,
											   pTdmConfig->ulSinStream,
											   pTdmConfig->ulSinNumTssts, 
											   cOCT6100_INPUT_TSST,
											   cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fRoutTsstEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance,  
											   pTdmConfig->ulRoutTimeslot,
											   pTdmConfig->ulRoutStream,
											   pTdmConfig->ulRoutNumTssts, 
											   cOCT6100_OUTPUT_TSST,
											   cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fSoutTsstEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											   pTdmConfig->ulSoutTimeslot,
											   pTdmConfig->ulSoutStream,
											   pTdmConfig->ulSoutNumTssts, 
											   cOCT6100_OUTPUT_TSST,
											   cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fRinRoutTsiMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, 
													  f_pChanIndexConf->usRinRoutTsiMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fSinSoutTsiMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, 
													  f_pChanIndexConf->usSinSoutTsiMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/*===============================================================================*/

		/*===============================================================================*/
		/* Release the previously reserved echo resources .*/
		if( fEchoChanEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseEchoEntry( f_pApiInstance, 
													f_pChanIndexConf->usEchoChanIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/*===============================================================================*/
	
		/*===============================================================================*/
		/* Release the previously reserved resources for the extended tone detection.*/
		if( fExtToneChanEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseEchoEntry( f_pApiInstance, 
													f_pChanIndexConf->usExtToneChanIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fExtToneTsiEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, 
													  f_pChanIndexConf->usExtToneTsiIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fExtToneMixerEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, 
														 f_pChanIndexConf->usExtToneMixerIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}
		/*===============================================================================*/

		/*===============================================================================*/
		/* Release the conversion resources. */
		if( fRinRoutConversionMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, 
															f_pChanIndexConf->usRinRoutConversionMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fSinSoutConversionMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, 
															f_pChanIndexConf->usSinSoutConversionMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/*===============================================================================*/
		
		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteChannelStructs

Description:    Performs all the required structure writes to configure the
				new echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pChannelOpen			Pointer to echo cancellation channel configuration structure.
f_pChanIndexConf		Pointer to a structure used to store the multiple resources indexes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteChannelStructs
UINT32 Oct6100ApiWriteChannelStructs(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				OUT tPOCT6100_API_ECHO_CHAN_INDEX	f_pChanIndexConf )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_CHANNEL_OPEN_TDM		pTdmConfig;
	tOCT6100_WRITE_PARAMS			WriteParams;
	tPOCT6100_API_CHANNEL			pChanEntry;
	UINT32	ulResult;
	UINT32	ulDwordAddress;
	UINT32	ulDwordData;
	BOOL	fProgramAdpcmMem;
	UINT32	ulCompType = 0;
	UINT32	ulPcmLaw;
	UINT16	usTempTsiMemIndex;
	UINT16	usConversionMemIndex;
	UINT32	ulToneEventNumber;
	BOOL	fSSTone;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Obtain a local pointer to the TDM configuration structure.*/
	pTdmConfig = &f_pChannelOpen->TdmConfig;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_pChanIndexConf->usEchoChanIndex );

	/*==============================================================================*/
	/* Configure the Input Tsst control memory.*/
	
	/* Set the RIN Tsst control entry.*/
	if ( f_pChanIndexConf->usRinTsstIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
														  f_pChanIndexConf->usRinTsstIndex,
														  f_pChanIndexConf->usRinRoutTsiMemIndex,
														  pTdmConfig->ulRinPcmLaw );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Set the SIN Tsst control entry.*/
	if ( f_pChanIndexConf->usSinTsstIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
														  f_pChanIndexConf->usSinTsstIndex,
														  f_pChanIndexConf->usSinSoutTsiMemIndex,
														  pTdmConfig->ulSinPcmLaw );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*==============================================================================*/

	/*==============================================================================*/
	/* Configure the ADPCM control memory for the Decoder.*/

	/* Set the codec state flags.*/
	f_pChanIndexConf->fRinRoutCodecActive = FALSE;
	f_pChanIndexConf->fSinSoutCodecActive = FALSE;

	if ( f_pChannelOpen->CodecConfig.ulDecoderPort != cOCT6100_NO_DECODING )
	{
		fProgramAdpcmMem = TRUE;

		switch( f_pChannelOpen->CodecConfig.ulDecodingRate )
		{
		case cOCT6100_G711_64KBPS:				
			ulCompType = 0x8;		
			if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
			{
				if ( pTdmConfig->ulRinPcmLaw == pTdmConfig->ulRoutPcmLaw )
					fProgramAdpcmMem = FALSE;

				/* Check if both ports are assigned.  If not, no law conversion needed here.. */
				if ( ( pTdmConfig->ulRinStream == cOCT6100_UNASSIGNED ) 
					|| ( pTdmConfig->ulRoutStream == cOCT6100_UNASSIGNED ) )
					fProgramAdpcmMem = FALSE;
			}
			else /*  f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
			{
				if ( pTdmConfig->ulSinPcmLaw == pTdmConfig->ulSoutPcmLaw )
					fProgramAdpcmMem = FALSE;

				if ( ( pTdmConfig->ulSinStream == cOCT6100_UNASSIGNED ) 
					|| ( pTdmConfig->ulSoutStream == cOCT6100_UNASSIGNED ) )
					fProgramAdpcmMem = FALSE;
			}
			break;
		case cOCT6100_G726_40KBPS:				
			ulCompType = 0x3;		
			break;

		case cOCT6100_G726_32KBPS:				
			ulCompType = 0x2;		
			break;

		case cOCT6100_G726_24KBPS:				
			ulCompType = 0x1;		
			break;

		case cOCT6100_G726_16KBPS:				
			ulCompType = 0x0;		
			break;		

		case cOCT6100_G727_2C_ENCODED:			
			ulCompType = 0x4;		
			break;

		case cOCT6100_G727_3C_ENCODED:			
			ulCompType = 0x5;		
			break;

		case cOCT6100_G727_4C_ENCODED:			
			ulCompType = 0x6;		
			break;

		case cOCT6100_G726_ENCODED:				
			ulCompType = 0x9;		
			break;

		case cOCT6100_G711_G726_ENCODED:		
			ulCompType = 0xA;		
			break;

		case cOCT6100_G711_G727_2C_ENCODED:		
			ulCompType = 0xC;		
			break;

		case cOCT6100_G711_G727_3C_ENCODED:		
			ulCompType = 0xD;		
			break;

		case cOCT6100_G711_G727_4C_ENCODED:		
			ulCompType = 0xE;		
			break;
		default:
			return cOCT6100_ERR_FATAL_D4;
		}

		if ( fProgramAdpcmMem == TRUE )
		{
			/* Set the chariot memory based on the selected port.*/
			if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
			{
				usTempTsiMemIndex = f_pChanIndexConf->usRinRoutTsiMemIndex;
				ulPcmLaw = pTdmConfig->ulRoutPcmLaw;		/* Set the law for later use */

				/* Set the codec state flags.*/
				f_pChanIndexConf->fRinRoutCodecActive = TRUE;

				/* Set the conversion memory index to use for decompression */
				usConversionMemIndex = f_pChanIndexConf->usRinRoutConversionMemIndex;
			}
			else /* f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
			{
				usTempTsiMemIndex = f_pChanIndexConf->usSinSoutTsiMemIndex;
				ulPcmLaw = pTdmConfig->ulSoutPcmLaw;		/* Set the law for later use */

				/* Set the codec state flags.*/
				f_pChanIndexConf->fSinSoutCodecActive = TRUE;

				/* Set the conversion memory index to use for decompression */
				usConversionMemIndex = f_pChanIndexConf->usSinSoutConversionMemIndex;
			}

			ulResult = Oct6100ApiWriteDecoderMemory( f_pApiInstance,
													 usConversionMemIndex,
													 ulCompType,
													 usTempTsiMemIndex,
													 ulPcmLaw,
													 f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Configure the ADPCM control memory for the Encoder */

	if ( f_pChannelOpen->CodecConfig.ulEncoderPort != cOCT6100_NO_ENCODING )
	{
		fProgramAdpcmMem = TRUE;

		switch( f_pChannelOpen->CodecConfig.ulEncodingRate )
		{
		case cOCT6100_G711_64KBPS:
			if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
			{
				if ( pTdmConfig->ulRoutPcmLaw == cOCT6100_PCM_U_LAW )
					ulCompType = 0x4;
				else
					ulCompType = 0x5;

				/* Check for law conversion.*/
				if ( pTdmConfig->ulRinPcmLaw == pTdmConfig->ulRoutPcmLaw )
					fProgramAdpcmMem = FALSE;

				/* Check if both ports are assigned.  If not, no law conversion needed here.. */
				if ( ( pTdmConfig->ulRinStream == cOCT6100_UNASSIGNED ) 
					|| ( pTdmConfig->ulRoutStream == cOCT6100_UNASSIGNED ) )
					fProgramAdpcmMem = FALSE;
			}	
			else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
			{
				if ( pTdmConfig->ulSoutPcmLaw == cOCT6100_PCM_U_LAW )
					ulCompType = 0x4;
				else
					ulCompType = 0x5;

				/* Check for law conversion.*/
				if ( pTdmConfig->ulSinPcmLaw == pTdmConfig->ulSoutPcmLaw )
					fProgramAdpcmMem = FALSE;

				/* Check if both ports are assigned.  If not, no law conversion needed here.. */
				if ( ( pTdmConfig->ulSinStream == cOCT6100_UNASSIGNED ) 
					|| ( pTdmConfig->ulSoutStream == cOCT6100_UNASSIGNED ) )
					fProgramAdpcmMem = FALSE;
			}
			
			break;
		case cOCT6100_G726_40KBPS:				
			ulCompType = 0x3;		
			break;

		case cOCT6100_G726_32KBPS:				
			ulCompType = 0x2;		
			break;

		case cOCT6100_G726_24KBPS:				
			ulCompType = 0x1;		
			break;

		case cOCT6100_G726_16KBPS:				
			ulCompType = 0x0;		
			break;		

		case cOCT6100_G727_40KBPS_4_1:			
			ulCompType = 0xD;		
			break;

		case cOCT6100_G727_40KBPS_3_2:			
			ulCompType = 0xA;		
			break;

		case cOCT6100_G727_40KBPS_2_3:			
			ulCompType = 0x6;		
			break;

		case cOCT6100_G727_32KBPS_4_0:			
			ulCompType = 0xE;		
			break;

		case cOCT6100_G727_32KBPS_3_1:			
			ulCompType = 0xB;		
			break;

		case cOCT6100_G727_32KBPS_2_2:			
			ulCompType = 0x7;		
			break;

		case cOCT6100_G727_24KBPS_3_0:			
			ulCompType = 0xC;		
			break;

		case cOCT6100_G727_24KBPS_2_1:			
			ulCompType = 0x8;		
			break;

		case cOCT6100_G727_16KBPS_2_0:			
			ulCompType = 0x9;		
			break;

		default:
			return cOCT6100_ERR_FATAL_D5;
		}

		/* Program the APDCM memory only if ADPCM is requried.*/
		if ( fProgramAdpcmMem == TRUE || f_pChanIndexConf->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
		{
			/* Set the chariot memory based on the selected port.*/
			if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
			{
				usTempTsiMemIndex = f_pChanIndexConf->usRinRoutTsiMemIndex;

				/* Set the codec state flags.*/
				f_pChanIndexConf->fRinRoutCodecActive = TRUE;

				/* Set the conversion memory index to use for compression */
				usConversionMemIndex = f_pChanIndexConf->usRinRoutConversionMemIndex;
			}

			else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
			{
				usTempTsiMemIndex = f_pChanIndexConf->usSinSoutTsiMemIndex;

				/* Set the codec state flags.*/
				f_pChanIndexConf->fSinSoutCodecActive = TRUE;

				/* Set the conversion memory index to use for compression */
				usConversionMemIndex = f_pChanIndexConf->usSinSoutConversionMemIndex;
			}

			ulResult = Oct6100ApiWriteEncoderMemory( f_pApiInstance,
													 usConversionMemIndex,
													 ulCompType,
													 usTempTsiMemIndex,
													 f_pChannelOpen->CodecConfig.fEnableSilenceSuppression,
													 f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition,
													 f_pChanIndexConf->usPhasingTsstIndex,
													 f_pChannelOpen->CodecConfig.ulPhasingType,
													 f_pChannelOpen->CodecConfig.ulPhase );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	/*==============================================================================*/

	
	/*==============================================================================*/
	/* Clearing the tone events bit vector */

	ulDwordAddress  = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_pChanIndexConf->usEchoChanIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
	ulDwordAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;
	ulDwordData = 0x00000000;

	ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulDwordAddress, ulDwordData );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulDwordAddress += 4;

	ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulDwordAddress, ulDwordData );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/

	
	/*==============================================================================*/
	/*	Write the VQE memory */
	
	ulResult = Oct6100ApiWriteVqeMemory( f_pApiInstance,
										  &f_pChannelOpen->VqeConfig,
										  f_pChannelOpen,
										  f_pChanIndexConf->usEchoChanIndex,	
										  f_pChanIndexConf->usEchoMemIndex,
										  TRUE,
										  FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/

	/*==============================================================================*/
	/*	Write the echo memory */

	ulResult = Oct6100ApiWriteEchoMemory( f_pApiInstance,
										  pTdmConfig,
										  f_pChannelOpen,
										  f_pChanIndexConf->usEchoMemIndex,
										  f_pChanIndexConf->usRinRoutTsiMemIndex,
										  f_pChanIndexConf->usSinSoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/



	/*==============================================================================*/
	/*	Mute channel if required, this is done on a port basis */

	/* Initialize the silence indexes to invalid for now. */
	pChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	pChanEntry->usSinSilenceEventIndex = cOCT6100_INVALID_INDEX;

	/* Set the TSI memory indexes. */
	pChanEntry->usRinRoutTsiMemIndex  = f_pChanIndexConf->usRinRoutTsiMemIndex;
	pChanEntry->usSinSoutTsiMemIndex  = f_pChanIndexConf->usSinSoutTsiMemIndex;

	ulResult = Oct6100ApiMutePorts( f_pApiInstance,
									f_pChanIndexConf->usEchoChanIndex,
									f_pChanIndexConf->usRinTsstIndex,
									f_pChanIndexConf->usSinTsstIndex,
									FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*==============================================================================*/

	
	/*==============================================================================*/
	/* Set the dominant speaker to unassigned, if required. */

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fDominantSpeakerEnabled == TRUE )
	{
		ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, f_pChanIndexConf->usEchoChanIndex, cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	
	/*==============================================================================*/


	/*==============================================================================*/
	/* If necessary, configure the extended tone detection channel.*/

	if ( f_pChannelOpen->fEnableExtToneDetection == TRUE )
	{
		UINT32	ulTempSinLaw;
		UINT32	ulTempSoutLaw;
		UINT32	ulTempEchoOpMode;

		/* save the original law.*/
		ulTempSinLaw		= pTdmConfig->ulSinPcmLaw;
		ulTempSoutLaw		= pTdmConfig->ulSoutPcmLaw;
		ulTempEchoOpMode	= f_pChannelOpen->ulEchoOperationMode;

		/* Now, make sure the Sin and Sout law are the same as the Rin law.*/

		pTdmConfig->ulSinPcmLaw		= pTdmConfig->ulRinPcmLaw;
		pTdmConfig->ulSoutPcmLaw	= pTdmConfig->ulRinPcmLaw;
		
		f_pChannelOpen->ulEchoOperationMode = cOCT6100_ECHO_OP_MODE_NORMAL;

		/* Write the Echo and VQE memory of the extended channel.*/

		ulResult = Oct6100ApiWriteDebugChanMemory( f_pApiInstance,
												   pTdmConfig,
												   &f_pChannelOpen->VqeConfig,
												   f_pChannelOpen,
												   f_pChanIndexConf->usExtToneChanIndex,
												   f_pChanIndexConf->usExtToneChanIndex,
												   cOCT6100_API_EXT_TONE_EXTRA_TSI,
												   f_pChanIndexConf->usExtToneTsiIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Now, write the mixer event used to copy the RIN signal of the original channel
		   into the SIN signal of the exteded channel. */

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_pChanIndexConf->usExtToneMixerIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
		WriteParams.usWriteData |= f_pChanIndexConf->usRinRoutTsiMemIndex;
		WriteParams.usWriteData |= pTdmConfig->ulRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = f_pChanIndexConf->usExtToneTsiIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Now insert the Sin copy event into the list.*/

		ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
											f_pChanIndexConf->usExtToneMixerIndex,
											cOCT6100_EVENT_TYPE_SIN_COPY,
											f_pChanIndexConf->usEchoChanIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		/*=======================================================================*/

		/*==============================================================================*/
		/* Clearing the tone events bit vector */

		ulDwordAddress  = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_pChanIndexConf->usExtToneChanIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
		ulDwordAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;
		ulDwordData = 0x00000000;

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulDwordAddress, ulDwordData );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		ulDwordAddress += 4;

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulDwordAddress, ulDwordData );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*==============================================================================*/

		/* Write back the original values in the channel open structure.*/

		pTdmConfig->ulSinPcmLaw		= ulTempSinLaw;
		pTdmConfig->ulSoutPcmLaw	= ulTempSoutLaw;
		
		f_pChannelOpen->ulEchoOperationMode = ulTempEchoOpMode;
	}

	/*==============================================================================*/


	/*==============================================================================*/
	/* If necessary, configure the SS tone detection. */

	for ( ulToneEventNumber = 0; ulToneEventNumber < cOCT6100_MAX_TONE_EVENT; ulToneEventNumber++ )
	{
		/* Check if the current tone is a SS tone. */
		ulResult = Oct6100ApiIsSSTone( 
									f_pApiInstance, 
									f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulToneID, 
									&fSSTone );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( fSSTone == TRUE )
		{
			/* Write to all resources needed to activate tone detection on this SS tone. */
			ulResult = Oct6100ApiWriteToneDetectEvent( 
													f_pApiInstance, 
													f_pChanIndexConf->usEchoChanIndex, 
													ulToneEventNumber,

													cOCT6100_INVALID_INDEX );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;
		}
	}

	/*==============================================================================*/


	/*==============================================================================*/
	/* Configure the Output Tsst control memory.*/
	
	/* Set the ROUT Tsst control entry.*/
	if ( f_pChanIndexConf->usRoutTsstIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
														   f_pChanIndexConf->usRoutTsstIndex,
														   f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition,
														   pTdmConfig->ulRoutNumTssts,
														   f_pChanIndexConf->usRinRoutTsiMemIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Set the SOUT Tsst control entry.*/
	if ( f_pChanIndexConf->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
														   f_pChanIndexConf->usSoutTsstIndex,
														   f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition,
														   pTdmConfig->ulSoutNumTssts,
														   f_pChanIndexConf->usSinSoutTsiMemIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateChannelEntry

Description:    Updates the new channel in the ECHO channel list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChannelOpen			Pointer to echo cancellation channel configuration structure.
f_pChanIndexConf		Pointer to a structure used to store the multiple resources indexes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateChannelEntry
UINT32 Oct6100ApiUpdateChannelEntry(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				OUT tPOCT6100_API_ECHO_CHAN_INDEX	f_pChanIndexConf )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_CHANNEL_OPEN_TDM		pTdmConfig;
	tPOCT6100_CHANNEL_OPEN_VQE		pVqeConfig;
	tPOCT6100_CHANNEL_OPEN_CODEC	pCodecConfig;

	/* Obtain a pointer to the config structures of the tPOCT6100_CHANNEL_OPEN structure. */
	pTdmConfig   = &f_pChannelOpen->TdmConfig;
	pVqeConfig   = &f_pChannelOpen->VqeConfig;
	pCodecConfig = &f_pChannelOpen->CodecConfig;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_pChanIndexConf->usEchoChanIndex )
	
	/*=======================================================================*/
	/* Copy the channel's configuration and allocated resources. */
	pChanEntry->ulUserChanId = f_pChannelOpen->ulUserChanId;
	pChanEntry->byEchoOperationMode = (UINT8)( f_pChannelOpen->ulEchoOperationMode & 0xFF );
	pChanEntry->fEnableToneDisabler = (UINT8)( f_pChannelOpen->fEnableToneDisabler & 0xFF );
	pChanEntry->fEnableExtToneDetection = (UINT8)( f_pChannelOpen->fEnableExtToneDetection & 0xFF );

	/* Save the VQE configuration.*/
	pChanEntry->VqeConfig.byComfortNoiseMode = (UINT8)( pVqeConfig->ulComfortNoiseMode & 0xFF );
	pChanEntry->VqeConfig.fEnableNlp = (UINT8)( pVqeConfig->fEnableNlp & 0xFF );
	pChanEntry->VqeConfig.fEnableTailDisplacement = (UINT8)( pVqeConfig->fEnableTailDisplacement );
	pChanEntry->VqeConfig.usTailDisplacement = (UINT16)( pVqeConfig->ulTailDisplacement & 0xFFFF );
	pChanEntry->VqeConfig.usTailLength = (UINT16)( pVqeConfig->ulTailLength & 0xFFFF );

	pChanEntry->VqeConfig.fSinDcOffsetRemoval = (UINT8)( pVqeConfig->fSinDcOffsetRemoval & 0xFF );
	pChanEntry->VqeConfig.fRinDcOffsetRemoval = (UINT8)( pVqeConfig->fRinDcOffsetRemoval & 0xFF );
	pChanEntry->VqeConfig.fRinLevelControl = (UINT8)( pVqeConfig->fRinLevelControl & 0xFF );
	pChanEntry->VqeConfig.chRinLevelControlGainDb = (OCT_INT8)( pVqeConfig->lRinLevelControlGainDb & 0xFF );
	pChanEntry->VqeConfig.fSoutLevelControl = (UINT8)( pVqeConfig->fSoutLevelControl & 0xFF );
	pChanEntry->VqeConfig.chSoutLevelControlGainDb = (OCT_INT8)( pVqeConfig->lSoutLevelControlGainDb & 0xFF );
	pChanEntry->VqeConfig.fRinAutomaticLevelControl = (UINT8)( pVqeConfig->fRinAutomaticLevelControl & 0xFF );
	pChanEntry->VqeConfig.chRinAutomaticLevelControlTargetDb = (OCT_INT8)( pVqeConfig->lRinAutomaticLevelControlTargetDb & 0xFF );
	pChanEntry->VqeConfig.fSoutAutomaticLevelControl = (UINT8)( pVqeConfig->fSoutAutomaticLevelControl & 0xFF );
	pChanEntry->VqeConfig.chSoutAutomaticLevelControlTargetDb = (OCT_INT8)( pVqeConfig->lSoutAutomaticLevelControlTargetDb & 0xFF );
	pChanEntry->VqeConfig.fRinHighLevelCompensation = (UINT8)( pVqeConfig->fRinHighLevelCompensation & 0xFF );
	pChanEntry->VqeConfig.chRinHighLevelCompensationThresholdDb = (OCT_INT8)( pVqeConfig->lRinHighLevelCompensationThresholdDb & 0xFF );
	pChanEntry->VqeConfig.fSoutAdaptiveNoiseReduction = (UINT8)( pVqeConfig->fSoutAdaptiveNoiseReduction & 0xFF );
	pChanEntry->VqeConfig.fSoutNoiseBleaching = (UINT8)( pVqeConfig->fSoutNoiseBleaching & 0xFF );
	pChanEntry->VqeConfig.fSoutConferencingNoiseReduction = (UINT8)( pVqeConfig->fSoutConferencingNoiseReduction & 0xFF );

	pChanEntry->VqeConfig.fAcousticEcho		= (UINT8)( pVqeConfig->fAcousticEcho & 0xFF );

	pChanEntry->VqeConfig.fDtmfToneRemoval	= (UINT8)( pVqeConfig->fDtmfToneRemoval & 0xFF );

	pChanEntry->VqeConfig.chDefaultErlDb	= (OCT_INT8)( pVqeConfig->lDefaultErlDb & 0xFF );
	pChanEntry->VqeConfig.chAecDefaultErlDb	= (OCT_INT8)( pVqeConfig->lAecDefaultErlDb & 0xFF );
	pChanEntry->VqeConfig.usAecTailLength = (UINT16)( pVqeConfig->ulAecTailLength & 0xFFFF );
	pChanEntry->VqeConfig.byNonLinearityBehaviorA = (UINT8)( pVqeConfig->ulNonLinearityBehaviorA & 0xFF );
	pChanEntry->VqeConfig.byNonLinearityBehaviorB = (UINT8)( pVqeConfig->ulNonLinearityBehaviorB & 0xFF );
	pChanEntry->VqeConfig.byDoubleTalkBehavior = (UINT8)( pVqeConfig->ulDoubleTalkBehavior & 0xFF );
	pChanEntry->VqeConfig.chAnrSnrEnhancementDb	= (OCT_INT8)( pVqeConfig->lAnrSnrEnhancementDb & 0xFF );
	pChanEntry->VqeConfig.byAnrVoiceNoiseSegregation = (UINT8)( pVqeConfig->ulAnrVoiceNoiseSegregation & 0xFF );
	pChanEntry->VqeConfig.usToneDisablerVqeActivationDelay = (UINT16)( pVqeConfig->ulToneDisablerVqeActivationDelay & 0xFFFF );

	pChanEntry->VqeConfig.bySoutAutomaticListenerEnhancementGainDb = (UINT8)( pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb & 0xFF );
	pChanEntry->VqeConfig.bySoutNaturalListenerEnhancementGainDb = (UINT8)( pVqeConfig->ulSoutNaturalListenerEnhancementGainDb & 0xFF );
	pChanEntry->VqeConfig.fSoutNaturalListenerEnhancement = (UINT8)( pVqeConfig->fSoutNaturalListenerEnhancement & 0xFF );
	pChanEntry->VqeConfig.fRoutNoiseReduction = (UINT8)( pVqeConfig->fRoutNoiseReduction & 0xFF );
	pChanEntry->VqeConfig.chRoutNoiseReductionLevelGainDb = (OCT_INT8) (pVqeConfig->lRoutNoiseReductionLevelGainDb & 0xFF);
	pChanEntry->VqeConfig.fEnableMusicProtection = (UINT8)( pVqeConfig->fEnableMusicProtection & 0xFF );
	pChanEntry->VqeConfig.fIdleCodeDetection = (UINT8)( pVqeConfig->fIdleCodeDetection & 0xFF );

	/* Save the codec information.*/
	pChanEntry->CodecConfig.byAdpcmNibblePosition = (UINT8)( pCodecConfig->ulAdpcmNibblePosition & 0xFF );

	pChanEntry->CodecConfig.byDecoderPort = (UINT8)( pCodecConfig->ulDecoderPort & 0xFF );
	pChanEntry->CodecConfig.byDecodingRate = (UINT8)( pCodecConfig->ulDecodingRate & 0xFF );
	pChanEntry->CodecConfig.byEncoderPort = (UINT8)( pCodecConfig->ulEncoderPort & 0xFF );
	pChanEntry->CodecConfig.byEncodingRate = (UINT8)( pCodecConfig->ulEncodingRate & 0xFF );
	
	pChanEntry->CodecConfig.fEnableSilenceSuppression = (UINT8)( pCodecConfig->fEnableSilenceSuppression & 0xFF );
	pChanEntry->CodecConfig.byPhase = (UINT8)( pCodecConfig->ulPhase & 0xFF );
	pChanEntry->CodecConfig.byPhasingType = (UINT8)( pCodecConfig->ulPhasingType & 0xFF );
	
	/* Save the RIN settings.*/
	pChanEntry->TdmConfig.byRinPcmLaw = (UINT8)( pTdmConfig->ulRinPcmLaw & 0xFF );
	pChanEntry->TdmConfig.usRinTimeslot = (UINT16)( pTdmConfig->ulRinTimeslot & 0xFFFF );
	pChanEntry->TdmConfig.usRinStream = (UINT16)( pTdmConfig->ulRinStream & 0xFFFF );
	
	/* Save the SIN settings.*/
	pChanEntry->TdmConfig.bySinPcmLaw = (UINT8)( pTdmConfig->ulSinPcmLaw & 0xFF );
	pChanEntry->TdmConfig.usSinTimeslot = (UINT16)( pTdmConfig->ulSinTimeslot & 0xFFFF );
	pChanEntry->TdmConfig.usSinStream = (UINT16)( pTdmConfig->ulSinStream & 0xFFFF );

	/* Save the ROUT settings.*/
	pChanEntry->TdmConfig.byRoutPcmLaw = (UINT8)( pTdmConfig->ulRoutPcmLaw & 0xFF );
	pChanEntry->TdmConfig.usRoutTimeslot = (UINT16)( pTdmConfig->ulRoutTimeslot & 0xFFFF );
	pChanEntry->TdmConfig.usRoutStream = (UINT16)( pTdmConfig->ulRoutStream & 0xFFFF );

	pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry = cOCT6100_INVALID_INDEX;
	pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry = 0;

	/* Save the SOUT settings.*/
	pChanEntry->TdmConfig.bySoutPcmLaw = (UINT8)( pTdmConfig->ulSoutPcmLaw & 0xFF );
	pChanEntry->TdmConfig.usSoutTimeslot = (UINT16)( pTdmConfig->ulSoutTimeslot & 0xFFFF );
	pChanEntry->TdmConfig.usSoutStream = (UINT16)( pTdmConfig->ulSoutStream & 0xFFFF );

	pChanEntry->TdmConfig.byRinNumTssts = (UINT8)( pTdmConfig->ulRinNumTssts & 0xFF );
	pChanEntry->TdmConfig.bySinNumTssts = (UINT8)( pTdmConfig->ulSinNumTssts & 0xFF );
	pChanEntry->TdmConfig.byRoutNumTssts = (UINT8)( pTdmConfig->ulRoutNumTssts & 0xFF );
	pChanEntry->TdmConfig.bySoutNumTssts = (UINT8)( pTdmConfig->ulSoutNumTssts & 0xFF );
	pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry = cOCT6100_INVALID_INDEX;
	pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry = 0;

	/* Save the extended Tone detection information.*/
	pChanEntry->usExtToneChanIndex		= f_pChanIndexConf->usExtToneChanIndex;
	pChanEntry->usExtToneMixerIndex		= f_pChanIndexConf->usExtToneMixerIndex;
	pChanEntry->usExtToneTsiIndex		= f_pChanIndexConf->usExtToneTsiIndex;

	if ( f_pChannelOpen->fEnableExtToneDetection == TRUE )
	{
		tPOCT6100_API_CHANNEL			pExtToneChanEntry;

		/* Set the mode of the original channel. He is the channel performing detection on the
		   SIN port.  The extended channel will perform detection on the RIN port.*/
		pChanEntry->ulExtToneChanMode = cOCT6100_API_EXT_TONE_SIN_PORT_MODE;

		/* Now, program the associated channel.*/
		
		/* Obtain a pointer to the extended tone detection channel entry. */
		mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pExtToneChanEntry, f_pChanIndexConf->usExtToneChanIndex );

		pExtToneChanEntry->fReserved			= TRUE;
		pExtToneChanEntry->ulExtToneChanMode	= cOCT6100_API_EXT_TONE_RIN_PORT_MODE;	/* Detect on RIN port.*/
		pExtToneChanEntry->usExtToneChanIndex	= f_pChanIndexConf->usEchoChanIndex;

		pExtToneChanEntry->aulToneConf[ 0 ] = 0;
		pExtToneChanEntry->aulToneConf[ 1 ] = 0;

	}
	else
	{
		/* No extended tone detection supported.*/
		pChanEntry->ulExtToneChanMode = cOCT6100_API_EXT_TONE_DISABLED;
	}

	/*=======================================================================*/

	/*=======================================================================*/
	/* Store hardware related information.*/
	pChanEntry->usRinRoutTsiMemIndex  = f_pChanIndexConf->usRinRoutTsiMemIndex;
	pChanEntry->usSinSoutTsiMemIndex  = f_pChanIndexConf->usSinSoutTsiMemIndex;
	pChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
	pChanEntry->usExtraRinTsiMemIndex = cOCT6100_INVALID_INDEX;

	/* We are not being tapped for now. */
	pChanEntry->fBeingTapped = FALSE;

	pChanEntry->usTapChanIndex = cOCT6100_INVALID_INDEX;
	pChanEntry->usTapBridgeIndex = cOCT6100_INVALID_INDEX;

	/* The copy event has not yet been created. */
	pChanEntry->fCopyEventCreated = FALSE;

	pChanEntry->usRinRoutConversionMemIndex = f_pChanIndexConf->usRinRoutConversionMemIndex;
	pChanEntry->usSinSoutConversionMemIndex = f_pChanIndexConf->usSinSoutConversionMemIndex;

	pChanEntry->usPhasingTsstIndex = f_pChanIndexConf->usPhasingTsstIndex;

	pChanEntry->fSinSoutCodecActive = f_pChanIndexConf->fSinSoutCodecActive;
	pChanEntry->fRinRoutCodecActive = f_pChanIndexConf->fRinRoutCodecActive;
	


	pChanEntry->usEchoMemIndex = f_pChanIndexConf->usEchoMemIndex;

	pChanEntry->usRinTsstIndex = f_pChanIndexConf->usRinTsstIndex;
	pChanEntry->usSinTsstIndex = f_pChanIndexConf->usSinTsstIndex;
	pChanEntry->usRoutTsstIndex = f_pChanIndexConf->usRoutTsstIndex;
	pChanEntry->usSoutTsstIndex = f_pChanIndexConf->usSoutTsstIndex;

	pChanEntry->usSinCopyEventIndex		= cOCT6100_INVALID_INDEX;
	pChanEntry->usSoutCopyEventIndex	= cOCT6100_INVALID_INDEX;

	/* Nothing muted for now. */
	pChanEntry->usMutedPorts			= cOCT6100_CHANNEL_MUTE_PORT_NONE;

	/* Set all the GW feature initial value.*/
	/* Bridge info */
	pChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;
	pChanEntry->fMute = FALSE;

	pChanEntry->usLoadEventIndex		= cOCT6100_INVALID_INDEX;
	pChanEntry->usSubStoreEventIndex	= cOCT6100_INVALID_INDEX;

	/* Buffer playout info.*/
	pChanEntry->fRinBufPlaying = FALSE;
	pChanEntry->fSoutBufPlaying = FALSE;

	/* Tone detection state. */
	/* This array is configured as follow.*/
	/* Index 0 contain event 0 to 31 (msb = event 31) and Index 1 contain index 32 - 55  */
	pChanEntry->aulToneConf[ 0 ] = 0;
	pChanEntry->aulToneConf[ 1 ] = 0;
	pChanEntry->ulLastSSToneDetected = (PTR_TYPE)cOCT6100_INVALID_VALUE;
	pChanEntry->ulLastSSToneTimestamp = (PTR_TYPE)cOCT6100_INVALID_VALUE;

	/* Initialize the bidirectional flag.*/
	pChanEntry->fBiDirChannel = FALSE;

	/*=======================================================================*/
	/* Init some of the stats.*/

	pChanEntry->sMaxERL						= cOCT6100_INVALID_SIGNED_STAT_W;
	pChanEntry->sMaxERLE					= cOCT6100_INVALID_SIGNED_STAT_W;
	pChanEntry->usMaxEchoDelay				= cOCT6100_INVALID_STAT_W;
	pChanEntry->usNumEchoPathChangesOfst	= 0;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Update the dependency of the phasing TSST if one is associated to the chanel.*/

	if ( f_pChanIndexConf->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
	{
		tPOCT6100_API_PHASING_TSST	pPhasingEntry;

		mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingEntry, f_pChanIndexConf->usPhasingTsstIndex );

		pPhasingEntry->usDependencyCnt++;
	}
	/*=======================================================================*/

	/*=======================================================================*/
	
	/* Form handle returned to user. */
	*f_pChannelOpen->pulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | (pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_pChanIndexConf->usEchoChanIndex;

	/* Finally, mark the channel as open. */
	pChanEntry->fReserved = TRUE;
	pChanEntry->usExtraSinTsiDependencyCnt = 0;
	
	/* Increment the number of channel open.*/
	f_pApiInstance->pSharedInfo->ChipStats.usNumberChannels++;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif






/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelCloseSer

Description:    Closes a echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelClose			Pointer to echo cancellation channel close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelCloseSer
UINT32 Oct6100ChannelCloseSer(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN tPOCT6100_CHANNEL_CLOSE				f_pChannelClose )
{
	UINT16	usChannelIndex;


	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertChannelParams( f_pApiInstance, 
											  f_pChannelClose, 

											  &usChannelIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiInvalidateChannelStructs( f_pApiInstance, 

												   usChannelIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiReleaseChannelResources( f_pApiInstance, usChannelIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Invalidate the handle.*/
	f_pChannelClose->ulChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertChannelParams

Description:    Validate the handle given by the user and verify the state of 
				the channel about to be closed.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelClose			Pointer to echo cancellation channel close structure.
f_pulFpgaChanIndex		Pointer to the FPGA channel index associated to this channel.
f_pusChanIndex			Pointer to the index of the channel within the API instance.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertChannelParams
UINT32 Oct6100ApiAssertChannelParams( 
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CHANNEL_CLOSE				f_pChannelClose,

				IN OUT	PUINT16								f_pusChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pChanEntry;
	UINT32							ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pChannelClose->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelClose->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, *f_pusChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelClose->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;
	if ( pChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CHANNEL_PART_OF_BIDIR_CHANNEL;

	/*=======================================================================*/
	
	/* Check if the channel is bound to a bridge. */
	if ( pChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CHANNEL_ACTIVE_DEPENDENCIES;



	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateChannelStructs

Description:    Closes a echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulFpgaChanIndex		Index of the channel within the SCN_PLC FPGA.
f_usChanIndex			Index of the channel within the API instance.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateChannelStructs
UINT32 Oct6100ApiInvalidateChannelStructs( 
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,

				IN		UINT16								f_usChanIndex )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_API_CHANNEL_TDM		pTdmConfig;
	tPOCT6100_API_TSST_ENTRY		pTsstEntry;
	tOCT6100_BUFFER_PLAYOUT_STOP	BufferPlayoutStop;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_WRITE_SMEAR_PARAMS		SmearParams;
	UINT32							ulResult;
	UINT16							usCurrentEntry;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex );
	
	/* Obtain local pointer to the TDM configuration of the channel */
	pTdmConfig = &pChanEntry->TdmConfig;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	SmearParams.pProcessContext = f_pApiInstance->pProcessContext;

	SmearParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	
	/* If this channel is currently debugged, automatically close the debug channel. */
	if ( ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
		&& ( pSharedInfo->DebugInfo.usCurrentDebugChanIndex == f_usChanIndex ) )
	{
		tOCT6100_DEBUG_SELECT_CHANNEL	SelectDebugChan;

		/* Ensure forward compatibility. */
		Oct6100DebugSelectChannelDef( &SelectDebugChan );

		/* Set the hot channel to an invalid handle to disable recording. */
		SelectDebugChan.ulChannelHndl = cOCT6100_INVALID_HANDLE;

		/* Call the serialized fonction. */
		ulResult = Oct6100DebugSelectChannelSer( f_pApiInstance, &SelectDebugChan, FALSE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Deactivate the TSST control memory if used. */
	
	/* RIN port.*/
	if ( pTdmConfig->usRinTimeslot != cOCT6100_UNASSIGNED )
	{
		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( pChanEntry->usRinTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}

	/* SIN port.*/
	if ( pTdmConfig->usSinTimeslot != cOCT6100_UNASSIGNED )
	{
		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( pChanEntry->usSinTsstIndex  * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}
	
	/*=======================================================================*/
	/* ROUT port.*/
	
	if ( pTdmConfig->usRoutTimeslot != cOCT6100_UNASSIGNED )
	{
		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( pChanEntry->usRoutTsstIndex  * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}

	/* Now the broadcast TSST.*/
	usCurrentEntry = pTdmConfig->usRoutBrdcastTsstFirstEntry;
	while( usCurrentEntry != cOCT6100_INVALID_INDEX )
	{
		mOCT6100_GET_TSST_LIST_ENTRY_PNT( pSharedInfo, pTsstEntry, usCurrentEntry );

		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pTsstEntry->usTsstMemoryIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
		
		/* Move to the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;
	}
	/*=======================================================================*/
	
	/*=======================================================================*/
	/* SOUT port.*/

	if ( pTdmConfig->usSoutTimeslot != cOCT6100_UNASSIGNED )
	{
		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( pChanEntry->usSoutTsstIndex  * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}

	/* Now the broadcast TSST.*/
	usCurrentEntry = pTdmConfig->usSoutBrdcastTsstFirstEntry;
	while( usCurrentEntry != cOCT6100_INVALID_INDEX )
	{
		mOCT6100_GET_TSST_LIST_ENTRY_PNT( pSharedInfo, pTsstEntry, usCurrentEntry );

		/* Deactivate the TSST entry.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pTsstEntry->usTsstMemoryIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
		
		/* Move to the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;
	}
		/*=======================================================================*/

	
	/*------------------------------------------------------------------------------*/
	/* Deactivate the ECHO control memory entry.*/
	
	/* Set the input Echo control entry to unused.*/
	WriteParams.ulWriteAddress  = cOCT6100_ECHO_CONTROL_MEM_BASE + ( pChanEntry->usEchoMemIndex * cOCT6100_ECHO_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.usWriteData = 0x85FF;	/* TSI index 1535 reserved for power-down mode */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = 0xC5FF;	/* TSI index 1535 reserved for power-down mode */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	/*------------------------------------------------------------------------------*/

	/*------------------------------------------------------------------------------*/
	/* Deactivate the conversion control memories if used. */
	
	if ( pChanEntry->usRinRoutConversionMemIndex != cOCT6100_INVALID_INDEX )
	{
		/* Rin/Rout stream conversion memory was used */
		ulResult = Oct6100ApiClearConversionMemory( f_pApiInstance, pChanEntry->usRinRoutConversionMemIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}
	
	if ( pChanEntry->usSinSoutConversionMemIndex != cOCT6100_INVALID_INDEX )
	{
		/* Sin/Sout stream conversion memory was used */
		ulResult = Oct6100ApiClearConversionMemory( f_pApiInstance, pChanEntry->usSinSoutConversionMemIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}

	/*------------------------------------------------------------------------------*/


	/*------------------------------------------------------------------------------*/
	/* Clear the silence copy events if they were created. */

	/* Unmute the Rin port if it was muted. */
	if ( pChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX )
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pChanEntry->usRinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usRinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_DF;

		pChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	/* Unmute the Sin port if it was muted. */
	if ( pChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pChanEntry->usSinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usSinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_E0;

		pChanEntry->usSinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	/*------------------------------------------------------------------------------*/

	/* Synch all the buffer playout field.*/
	if ( pSharedInfo->ImageInfo.fBufferPlayout == TRUE )
	{	
		Oct6100BufferPlayoutStopDef( &BufferPlayoutStop );

		BufferPlayoutStop.ulChannelHndl = cOCT6100_INVALID_HANDLE;
		BufferPlayoutStop.fStopCleanly = FALSE;
		
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;
		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
													f_pApiInstance, 
													&BufferPlayoutStop, 
													f_usChanIndex, 
													pChanEntry->usEchoMemIndex

													);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_SOUT;
		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
													f_pApiInstance, 
													&BufferPlayoutStop, 
													f_usChanIndex, 
													pChanEntry->usEchoMemIndex 

													);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}





	/* Free all resources reserved for extended tone detection.*/
	if ( pChanEntry->fEnableExtToneDetection == TRUE )
	{
		/*------------------------------------------------------------------------------*/
		/* Deactivate the ECHO control memory entry of the extended channel.*/
		
		/* Set the input Echo control entry to unused.*/
		WriteParams.ulWriteAddress  = cOCT6100_ECHO_CONTROL_MEM_BASE + ( pChanEntry->usExtToneChanIndex * cOCT6100_ECHO_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData = 0x85FF;	/* TSI index 1535 reserved for power-down mode */

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = 0xC5FF;	/* TSI index 1535 reserved for power-down mode */

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
		/*------------------------------------------------------------------------------*/

		/*------------------------------------------------------------------------------*/
		/* Remove the mixer event used to copy the RIN signal to the SIN port of the extended
		   channel.*/

		/* Clear the Copy event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usExtToneMixerIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pChanEntry->usExtToneMixerIndex,
												cOCT6100_EVENT_TYPE_SIN_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/*------------------------------------------------------------------------------*/

	}

	/*------------------------------------------------------------------------------*/
	/* Reset PGSP */

	WriteParams.ulWriteAddress = cOCT6100_CHANNEL_ROOT_BASE + ( pChanEntry->usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;
	WriteParams.usWriteData = 0x0800;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult; 

	/*------------------------------------------------------------------------------*/

	/*------------------------------------------------------------------------------*/
	/* Clear the mute with feature bit. */

	if ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 )
	{
		ulResult = Oct6100ApiMuteSinWithFeatures( f_pApiInstance, f_usChanIndex, FALSE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*------------------------------------------------------------------------------*/

	/*------------------------------------------------------------------------------*/
	/* Clear the VQE memory. */

	SmearParams.ulWriteAddress = cOCT6100_CHANNEL_ROOT_BASE + ( pChanEntry->usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst + 0x20;
	SmearParams.usWriteData = 0x0000;
	SmearParams.ulWriteLength = 2;

	mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*------------------------------------------------------------------------------*/
	/*------------------------------------------------------------------------------*/
	/* Clear the NLP memory. */

	SmearParams.ulWriteAddress = cOCT6100_CHANNEL_ROOT_BASE + ( pChanEntry->usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst + 0x28;
	SmearParams.usWriteData = 0x0000;
	SmearParams.ulWriteLength = 2;

	mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*------------------------------------------------------------------------------*/
	/* Clear the AF information memory. */

	SmearParams.ulWriteAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( pChanEntry->usEchoMemIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst;
	SmearParams.usWriteData = 0x0000;
	SmearParams.ulWriteLength = 12;

	mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/*Reset ALC status*/
	WriteParams.ulWriteAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( pChanEntry->usEchoMemIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst + 0x3A;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult; 
	
	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseChannelResources

Description:	Release and clear the API entry associated to the echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usChannelIndex		Index of the echo cancellation channel in the API list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseChannelResources
UINT32 Oct6100ApiReleaseChannelResources( 
				IN  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	UINT16								f_usChannelIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_API_CHANNEL_TDM	pTdmConfig;
	tPOCT6100_API_TSST_ENTRY	pTsstEntry;
	UINT32	ulResult;
	UINT16	usCurrentEntry;
	UINT32	ulTimeslot;
	UINT32	ulStream;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChannelIndex );

	/* Obtain local pointer to the TDM configurationof the channel */
	pTdmConfig = &pChanEntry->TdmConfig;

	/* Release the two TSI chariot memory entries.*/
	ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usRinRoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return cOCT6100_ERR_FATAL_2;

	ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usSinSoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return cOCT6100_ERR_FATAL_3;

	/* Now release the ECHO channel and control memory entries.*/
	ulResult = Oct6100ApiReleaseEchoEntry( f_pApiInstance, f_usChannelIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return cOCT6100_ERR_FATAL_4;

	/* Release the conversion resources.*/
	if ( pChanEntry->usRinRoutConversionMemIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, pChanEntry->usRinRoutConversionMemIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_B9;

		pChanEntry->usRinRoutConversionMemIndex = cOCT6100_INVALID_INDEX;
	}

	if ( pChanEntry->usSinSoutConversionMemIndex != cOCT6100_INVALID_INDEX )
	{
		ulResult = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, pChanEntry->usSinSoutConversionMemIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_BA;

		pChanEntry->usSinSoutConversionMemIndex = cOCT6100_INVALID_INDEX;
	}

	/*=========================================================================*/
	/* Release the TSST control memory entries if any were reserved.*/
	if ( pTdmConfig->usRinTimeslot != cOCT6100_UNASSIGNED)
	{
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  pTdmConfig->usRinTimeslot,
										  pTdmConfig->usRinStream,
 									      pTdmConfig->byRinNumTssts, 
										  cOCT6100_INPUT_TSST,
										  cOCT6100_INVALID_INDEX );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_5;
	}

	if ( pTdmConfig->usSinTimeslot != cOCT6100_UNASSIGNED)
	{
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  pTdmConfig->usSinTimeslot,
										  pTdmConfig->usSinStream,
 									      pTdmConfig->bySinNumTssts, 
										  cOCT6100_INPUT_TSST,
										  cOCT6100_INVALID_INDEX );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_6;
	}

		/*=======================================================================*/
	/* Release all the TSSTs associated to the ROUT port of this channel. */
	if ( pTdmConfig->usRoutTimeslot != cOCT6100_UNASSIGNED)
	{
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  pTdmConfig->usRoutTimeslot,
										  pTdmConfig->usRoutStream,
 									      pTdmConfig->byRoutNumTssts, 
										  cOCT6100_OUTPUT_TSST,
										  cOCT6100_INVALID_INDEX );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_7;
	}

	/* Now release the Broadcast TSSTs. */
	usCurrentEntry = pTdmConfig->usRoutBrdcastTsstFirstEntry;
	while( usCurrentEntry != cOCT6100_INVALID_INDEX )
	{
		mOCT6100_GET_TSST_LIST_ENTRY_PNT( pSharedInfo, pTsstEntry, usCurrentEntry );

		ulTimeslot = pTsstEntry->usTsstValue >> 5;
		ulStream = pTsstEntry->usTsstValue & 0x1F;

		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  ulTimeslot,
										  ulStream,
 									      cOCT6100_NUMBER_TSSTS_1, 
										  cOCT6100_OUTPUT_TSST,
										  usCurrentEntry );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_8;

		/* Move to the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;

		/* Invalidate the current entry.*/
		pTsstEntry->usTsstMemoryIndex = 0xFFFF;
		pTsstEntry->usTsstValue = 0xFFFF;
		pTsstEntry->usNextEntry = cOCT6100_INVALID_INDEX;
	}

		/*=======================================================================*/

	
		/*=======================================================================*/
	/* Release all the TSSTs associated to the SOUT port of this channel. */
	if ( pTdmConfig->usSoutTimeslot != cOCT6100_UNASSIGNED)
	{
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  pTdmConfig->usSoutTimeslot,
										  pTdmConfig->usSoutStream,
 									      pTdmConfig->bySoutNumTssts, 
										  cOCT6100_OUTPUT_TSST,
										  cOCT6100_INVALID_INDEX );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_9;
	}

	/* Now release the Broadcast TSSTs. */
	usCurrentEntry = pTdmConfig->usSoutBrdcastTsstFirstEntry;
	while( usCurrentEntry != cOCT6100_INVALID_INDEX )
	{
		mOCT6100_GET_TSST_LIST_ENTRY_PNT( pSharedInfo, pTsstEntry, usCurrentEntry );

		ulTimeslot = pTsstEntry->usTsstValue >> 5;
		ulStream = pTsstEntry->usTsstValue & 0x1F;

		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  ulTimeslot,
										  ulStream,
 									      cOCT6100_NUMBER_TSSTS_1,
										  cOCT6100_OUTPUT_TSST,
										  usCurrentEntry );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_A;

		/* Move to the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;

		/* Invalidate the current entry.*/
		pTsstEntry->usTsstMemoryIndex = 0xFFFF;
		pTsstEntry->usTsstValue = 0xFFFF;
		pTsstEntry->usNextEntry = cOCT6100_INVALID_INDEX;
	}
	/*=======================================================================*/

	/*=======================================================================*/
	/* Update the dependency of the phasing TSST if one is associated to the chanel.*/

	if ( pChanEntry->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
	{
		tPOCT6100_API_PHASING_TSST	pPhasingEntry;

		mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingEntry, pChanEntry->usPhasingTsstIndex );

		pPhasingEntry->usDependencyCnt--;
	}
	/*=======================================================================*/


	/*=======================================================================*/
	/* Release any resources reserved for the extended tone detection.*/

	if ( pChanEntry->fEnableExtToneDetection == TRUE )
	{
		tPOCT6100_API_CHANNEL		pExtToneChanEntry;

		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pExtToneChanEntry, pChanEntry->usExtToneChanIndex );

		/* Release the ECHO channel and control memory entries.*/
		ulResult = Oct6100ApiReleaseEchoEntry( f_pApiInstance, pChanEntry->usExtToneChanIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_C1;

		ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usExtToneTsiIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_C2;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usExtToneMixerIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_C3;

		/* Now release the channel entry */
		pExtToneChanEntry->ulExtToneChanMode	= cOCT6100_API_EXT_TONE_DISABLED;
		pExtToneChanEntry->fReserved			= FALSE;

		/* Set the current entry to disable, just in case.*/
		pChanEntry->ulExtToneChanMode			= cOCT6100_API_EXT_TONE_DISABLED;
	}
	/*=======================================================================*/


	/*=======================================================================*/
	/* Update the channel's list entry. */

	/* Clear the NLP dword array. */
	Oct6100UserMemSet( pChanEntry->aulNlpConfDword, 0, sizeof( pChanEntry->aulNlpConfDword ) );

	/* Clear the echo operation mode. */
	pChanEntry->byEchoOperationMode = cOCT6100_ECHO_OP_MODE_POWER_DOWN;

	/* Mark the channel as closed. */
	pChanEntry->fReserved = FALSE;
	pChanEntry->byEntryOpenCnt++;

	/* Reset the port, the bridge and BidirInfo */
	pChanEntry->usMutedPorts = cOCT6100_CHANNEL_MUTE_PORT_NONE;
	pChanEntry->fBiDirChannel = FALSE;
	pChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;
	
	/* Decrement the number of channel open.*/
	f_pApiInstance->pSharedInfo->ChipStats.usNumberChannels--;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;

}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelModifySer

Description:    Modify an echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelModify		Pointer to channel configuration structure.  The handle
						identifying the buffer in all future function calls is
						returned in this structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelModifySer
UINT32 Oct6100ChannelModifySer(
				IN		tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_MODIFY				f_pChannelModify )
{
	UINT16	usChanIndex;
	UINT32	ulResult;
	UINT16	usNewRinTsstIndex;
	UINT16	usNewSinTsstIndex;
	UINT16	usNewRoutTsstIndex;
	UINT16	usNewSoutTsstIndex;
	UINT8	fSinSoutCodecActive = FALSE;
	UINT8	fRinRoutCodecActive = FALSE;
	UINT16	usNewPhasingTsstIndex;
	tOCT6100_CHANNEL_OPEN	*pTempChanOpen;

	/* We don't want this 290 byte structure on the stack */
	pTempChanOpen = kmalloc(sizeof(*pTempChanOpen), GFP_ATOMIC);
	if (!pTempChanOpen)
		return cOCT6100_ERR_FATAL_0;

	/* Check the user's configuration of the echo cancellation channel for errors. */
	ulResult = Oct6100ApiCheckChannelModify( f_pApiInstance, 
											 f_pChannelModify, 
											 pTempChanOpen, 
											 &usNewPhasingTsstIndex,
											 &usChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Reserve all resources needed by the echo cancellation channel. */
	ulResult = Oct6100ApiModifyChannelResources( f_pApiInstance, 
												 f_pChannelModify, 
												 usChanIndex, 
												 &usNewRinTsstIndex, 
												 &usNewSinTsstIndex, 
												 &usNewRoutTsstIndex, 
												 &usNewSoutTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Write all necessary structures to activate the echo cancellation channel. */
	ulResult = Oct6100ApiModifyChannelStructs( f_pApiInstance, 
											   f_pChannelModify, 
											   pTempChanOpen, 
											   usChanIndex, 
											   usNewPhasingTsstIndex,
											   &fSinSoutCodecActive,
											   &fRinRoutCodecActive,
											   usNewRinTsstIndex, 
											   usNewSinTsstIndex, 
											   usNewRoutTsstIndex, 
											   usNewSoutTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		goto out;

	/* Update the new echo cancellation channels's entry in the ECHO channel list. */
	ulResult = Oct6100ApiModifyChannelEntry( f_pApiInstance, 
											 f_pChannelModify, 
											 pTempChanOpen, 
											 usChanIndex,  
											 usNewPhasingTsstIndex,
											 fSinSoutCodecActive,
											 fRinRoutCodecActive,
											 usNewRinTsstIndex, 
											 usNewSinTsstIndex, 
											 usNewRoutTsstIndex, 
											 usNewSoutTsstIndex  );
out:
	kfree(pTempChanOpen);

	return ulResult;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChannelModify

Description:    Checks the user's echo cancellation channel modify structure for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelModify		Pointer to echo cancellation channel modify structure.
f_pTempChanOpen			Pointer to a channel open structure.
f_pusNewPhasingTsstIndex	Pointer to a new phasing TSST index within the API instance.
f_pusChanIndex			Pointer to the channel index within the API instance channel list

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChannelModify
UINT32 Oct6100ApiCheckChannelModify(
				IN		tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_CHANNEL_MODIFY				f_pChannelModify,
				IN		tPOCT6100_CHANNEL_OPEN					f_pTempChanOpen,
				OUT		PUINT16									f_pusNewPhasingTsstIndex,
				OUT		PUINT16									f_pusChanIndex )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	UINT32	ulResult;
	UINT32	ulEntryOpenCnt;
	UINT32	ulDecoderNumTssts;

	/* Check the provided handle. */
	if ( (f_pChannelModify->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelModify->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, *f_pusChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelModify->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Check the general modify parameters. */
	
	if ( f_pChannelModify->ulEchoOperationMode != cOCT6100_KEEP_PREVIOUS_SETTING &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NORMAL &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_HT_FREEZE &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_HT_RESET &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_POWER_DOWN &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_EXTERNAL &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION &&
		 f_pChannelModify->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NO_ECHO )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE;

	/* Check the 2100Hz echo disabling configuration.*/
	if ( f_pChannelModify->fEnableToneDisabler != cOCT6100_KEEP_PREVIOUS_SETTING &&
		 f_pChannelModify->fEnableToneDisabler != TRUE && 
		 f_pChannelModify->fEnableToneDisabler != FALSE )
		return cOCT6100_ERR_CHANNEL_TONE_DISABLER_ENABLE;

	/* Check the disable tone detection flag. */
	if ( f_pChannelModify->fDisableToneDetection != TRUE &&
		f_pChannelModify->fDisableToneDetection != FALSE )
		return cOCT6100_ERR_CHANNEL_DISABLE_TONE_DETECTION;

	/* Check the stop buffer playout flag. */
	if ( f_pChannelModify->fStopBufferPlayout != TRUE &&
		f_pChannelModify->fStopBufferPlayout != FALSE )
		return cOCT6100_ERR_CHANNEL_STOP_BUFFER_PLAYOUT;

	/* Check the remove conference bridge participant flag. */
	if ( f_pChannelModify->fRemoveConfBridgeParticipant != TRUE &&
		f_pChannelModify->fRemoveConfBridgeParticipant != FALSE )
		return cOCT6100_ERR_CHANNEL_REMOVE_CONF_BRIDGE_PARTICIPANT;

	/* Check the remove broadcast timeslots flag. */
	if ( f_pChannelModify->fRemoveBroadcastTssts != TRUE &&
		f_pChannelModify->fRemoveBroadcastTssts != FALSE )
		return cOCT6100_ERR_CHANNEL_REMOVE_BROADCAST_TSSTS;

	if ( f_pChannelModify->fCodecConfigModified != TRUE && 
		 f_pChannelModify->fCodecConfigModified != FALSE )
		return cOCT6100_ERR_CHANNEL_MODIFY_CODEC_CONFIG;

	if ( f_pChannelModify->fVqeConfigModified != TRUE && 
		 f_pChannelModify->fVqeConfigModified != FALSE )
		return cOCT6100_ERR_CHANNEL_MODIFY_VQE_CONFIG;

	if ( f_pChannelModify->fTdmConfigModified != TRUE && 
		 f_pChannelModify->fTdmConfigModified != FALSE )
		return cOCT6100_ERR_CHANNEL_MODIFY_TDM_CONFIG;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Verify if any law change was requested. If so reprogram all structures.*/

	if (( f_pChannelModify->fTdmConfigModified == TRUE ) &&
		( f_pChannelModify->TdmConfig.ulRinPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING ||
		  f_pChannelModify->TdmConfig.ulSinPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING ||
		  f_pChannelModify->TdmConfig.ulRoutPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING ||
		  f_pChannelModify->TdmConfig.ulSoutPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING ))
	{
		f_pChannelModify->fVqeConfigModified = TRUE;
		f_pChannelModify->fCodecConfigModified = TRUE;
	}
	/*=======================================================================*/
	
	ulResult = Oct6100ApiUpdateOpenStruct( f_pApiInstance, f_pChannelModify, f_pTempChanOpen, pChanEntry );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* All further check will now be performed using the TempOpenChan structure in order
	   to reuse the checks written for the open channel structure.*/
	


	/* Check the TDM config.*/
	if ( f_pChannelModify->fTdmConfigModified == TRUE )
	{
		tPOCT6100_CHANNEL_OPEN_TDM			pOpenTdm;

		pOpenTdm = &f_pTempChanOpen->TdmConfig;

		ulResult = Oct6100ApiCheckTdmConfig( f_pApiInstance,
											 pOpenTdm );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if that Stream and Timeslot values are valid.*/
		
		/* Check the RIN port.*/
		if ( f_pChannelModify->TdmConfig.ulRinStream == cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_RIN_TIMESLOT;

		if ( f_pChannelModify->TdmConfig.ulRinStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulRinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_RIN_STREAM;
		 
		if ( pChanEntry->fBeingTapped == TRUE )
		{
			/* Check that the Rin stream + timeslot are not being assigned. */
			if ( f_pChannelModify->TdmConfig.ulRinStream != cOCT6100_KEEP_PREVIOUS_SETTING )
			{
				if ( f_pChannelModify->TdmConfig.ulRinStream != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_RIN_STREAM;

				if ( f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_RIN_TIMESLOT;
			}
		}

		/* Check the SIN port.*/
		if ( f_pChannelModify->TdmConfig.ulSinStream == cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_SIN_TIMESLOT;

		if ( f_pChannelModify->TdmConfig.ulSinStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulSinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_SIN_STREAM;

		/* Check the ROUT port.*/
		if ( f_pChannelModify->TdmConfig.ulRoutStream == cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_ROUT_TIMESLOT;

		if ( f_pChannelModify->TdmConfig.ulRoutStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulRoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_ROUT_STREAM;

		/* Check the SOUT port.*/
		if ( f_pChannelModify->TdmConfig.ulSoutStream == cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_SOUT_TIMESLOT;

		if ( f_pChannelModify->TdmConfig.ulSoutStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
			f_pChannelModify->TdmConfig.ulSoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
			return cOCT6100_ERR_CHANNEL_SOUT_STREAM;

		/* Verify if the channel is currently part of a bidirectional channel, and if */
		/* so perform the required checks. */
		if ( pChanEntry->fBiDirChannel == TRUE )
		{
			/* Check the ports that must remain unassigned.*/
			if ( f_pTempChanOpen->TdmConfig.ulRinTimeslot != cOCT6100_UNASSIGNED )
				return cOCT6100_ERR_CHANNEL_RIN_TIMESLOT;
			if ( f_pTempChanOpen->TdmConfig.ulSoutTimeslot != cOCT6100_UNASSIGNED )
				return cOCT6100_ERR_CHANNEL_SOUT_TIMESLOT;
			
			/* Check that no PCM law change is requested.*/
			if ( f_pTempChanOpen->TdmConfig.ulRinPcmLaw != f_pTempChanOpen->TdmConfig.ulRoutPcmLaw )
				return cOCT6100_ERR_CHANNEL_RIN_ROUT_LAW_CONVERSION;
			if ( f_pTempChanOpen->TdmConfig.ulSinPcmLaw != f_pTempChanOpen->TdmConfig.ulSoutPcmLaw )
				return cOCT6100_ERR_CHANNEL_SIN_SOUT_LAW_CONVERSION;
		}

		/* If this channel is on a conference bridge, a few more things must be checked. */
		if ( pChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		{
			/* If conferencing, law conversion cannot be applied. */
			/* This check is done only if both input and output ports are assigned. */
			if ( ( f_pTempChanOpen->TdmConfig.ulRinTimeslot != cOCT6100_UNASSIGNED )
				&& ( f_pTempChanOpen->TdmConfig.ulRoutTimeslot != cOCT6100_UNASSIGNED ) )
			{
				/* Laws must be the same! */
				if ( f_pTempChanOpen->TdmConfig.ulRinPcmLaw != f_pTempChanOpen->TdmConfig.ulRoutPcmLaw )
					return cOCT6100_ERR_CHANNEL_RIN_ROUT_LAW_CONVERSION;
			}

			/* Check for Sin. */
			if ( ( f_pTempChanOpen->TdmConfig.ulSinTimeslot != cOCT6100_UNASSIGNED )
				&& ( f_pTempChanOpen->TdmConfig.ulSoutTimeslot != cOCT6100_UNASSIGNED ) )
			{
				/* Laws must be the same! */
				if ( f_pTempChanOpen->TdmConfig.ulSinPcmLaw != f_pTempChanOpen->TdmConfig.ulSoutPcmLaw )
					return cOCT6100_ERR_CHANNEL_SIN_SOUT_LAW_CONVERSION;
			}

			/* Check if ADPCM is requested. */
			if ( f_pTempChanOpen->CodecConfig.ulEncoderPort != cOCT6100_NO_ENCODING &&
				 f_pTempChanOpen->CodecConfig.ulEncodingRate != cOCT6100_G711_64KBPS )
			{
				/* No ADPCM in a conference bridge! */
				return cOCT6100_ERR_CHANNEL_ENCODING_RATE;
			}

			if ( f_pTempChanOpen->CodecConfig.ulDecoderPort != cOCT6100_NO_DECODING &&
				 f_pTempChanOpen->CodecConfig.ulDecodingRate != cOCT6100_G711_64KBPS )
			{
				/* No ADPCM in a conference bridge! */
				return cOCT6100_ERR_CHANNEL_DECODING_RATE;
			}
		}

		if ( f_pTempChanOpen->CodecConfig.ulEncoderPort == cOCT6100_NO_ENCODING || 
			f_pTempChanOpen->CodecConfig.ulDecoderPort == cOCT6100_NO_DECODING )
		{
			/* Make sure no law conversion is attempted since it is not supported by the device.*/
			if ( f_pTempChanOpen->TdmConfig.ulRinPcmLaw != f_pTempChanOpen->TdmConfig.ulRoutPcmLaw )
				return cOCT6100_ERR_CHANNEL_RIN_ROUT_LAW_CONVERSION;
			if ( f_pTempChanOpen->TdmConfig.ulSinPcmLaw != f_pTempChanOpen->TdmConfig.ulSoutPcmLaw )
				return cOCT6100_ERR_CHANNEL_SIN_SOUT_LAW_CONVERSION;
		}

		if ( pChanEntry->fEnableExtToneDetection == TRUE && 
			 f_pTempChanOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
			return cOCT6100_ERR_CHANNEL_EXT_TONE_DETECTION_DECODER_PORT;

		/* A few special checks must be done if the configuration is to be applied */
		/* to all opened channels. */
		if ( f_pChannelModify->fApplyToAllChannels == TRUE )
		{
			/* When the configuration to be applied is for all channels, */
			/* check that the stream and timeslot parameters are not being assigned. */

			/* Check the Rout port. */
			if ( f_pChannelModify->TdmConfig.ulRoutStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
				f_pChannelModify->TdmConfig.ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			{
				/* Check that the Rout ports is being unassigned. */
				if ( f_pTempChanOpen->TdmConfig.ulRoutStream != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_ROUT_STREAM_UNASSIGN;
				if ( f_pTempChanOpen->TdmConfig.ulRoutTimeslot != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_ROUT_TIMESLOT_UNASSIGN;
			}

			/* Check the Rin port. */
			if ( f_pChannelModify->TdmConfig.ulRinStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
				f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			{
				/* Check that the Rin ports is being unassigned. */
				if ( f_pTempChanOpen->TdmConfig.ulRinStream != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_RIN_STREAM_UNASSIGN;
				if ( f_pTempChanOpen->TdmConfig.ulRinTimeslot != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_RIN_TIMESLOT_UNASSIGN;
			}

			/* Check the Sout port. */
			if ( f_pChannelModify->TdmConfig.ulSoutStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
				f_pChannelModify->TdmConfig.ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			{
				/* Check that the Sout ports is being unassigned. */
				if ( f_pTempChanOpen->TdmConfig.ulSoutStream != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_SOUT_STREAM_UNASSIGN;
				if ( f_pTempChanOpen->TdmConfig.ulSoutTimeslot != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_SOUT_TIMESLOT_UNASSIGN;
			}

			/* Check the Sin port. */
			if ( f_pChannelModify->TdmConfig.ulSinStream != cOCT6100_KEEP_PREVIOUS_SETTING &&
				f_pChannelModify->TdmConfig.ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			{
				/* Check that the Sin ports is being unassigned. */
				if ( f_pTempChanOpen->TdmConfig.ulSinStream != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_SIN_STREAM_UNASSIGN;
				if ( f_pTempChanOpen->TdmConfig.ulSinTimeslot != cOCT6100_UNASSIGNED )
					return cOCT6100_ERR_CHANNEL_SIN_TIMESLOT_UNASSIGN;
			}
		}
	}

	/* Check the VQE config.*/
	if ( f_pChannelModify->fVqeConfigModified == TRUE )
	{
		ulResult = Oct6100ApiCheckVqeConfig( f_pApiInstance,
											 &f_pTempChanOpen->VqeConfig,
											 f_pTempChanOpen->fEnableToneDisabler );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Verify if the echo operation mode selected can be applied. */
	if ( ( f_pTempChanOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
		&& ( f_pTempChanOpen->VqeConfig.fEnableNlp == FALSE ) )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE_NLP_REQUIRED;
	
	if ( ( f_pTempChanOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION )
		&& ( f_pTempChanOpen->VqeConfig.fEnableNlp == FALSE ) )
		return cOCT6100_ERR_CHANNEL_ECHO_OP_MODE_NLP_REQUIRED;

	/* Comfort noise must be activated for speech recognition mode to work. */
	if ( ( f_pTempChanOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION )
		&& ( f_pTempChanOpen->VqeConfig.ulComfortNoiseMode == cOCT6100_COMFORT_NOISE_OFF ) )
		return cOCT6100_ERR_CHANNEL_COMFORT_NOISE_REQUIRED;

	/* Check the Codec config.*/
	if ( f_pChannelModify->fCodecConfigModified == TRUE )
	{
		if ( f_pTempChanOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
			ulDecoderNumTssts = f_pTempChanOpen->TdmConfig.ulRinNumTssts;
		else /* f_pTempChanOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
			ulDecoderNumTssts  = f_pTempChanOpen->TdmConfig.ulSinNumTssts;

		ulResult = Oct6100ApiCheckCodecConfig( f_pApiInstance,
											   &f_pTempChanOpen->CodecConfig,
											   ulDecoderNumTssts,
											   f_pusNewPhasingTsstIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;



		/* make sure that if silence suppression is activated, the NLP is enabled.*/
		if ( f_pTempChanOpen->CodecConfig.fEnableSilenceSuppression == TRUE && f_pTempChanOpen->VqeConfig.fEnableNlp == FALSE )
			return cOCT6100_ERR_CHANNEL_SIL_SUP_NLP_MUST_BE_ENABLED;

		/* Verify if the channel is currently part of a bidirectional channel, and if so perform
		    the required checks.*/
		if ( pChanEntry->fBiDirChannel == TRUE )
		{
			/* Check the ports that must remain unassigned.*/
			if ( f_pTempChanOpen->CodecConfig.ulEncoderPort != cOCT6100_NO_ENCODING && 
				 f_pTempChanOpen->CodecConfig.ulEncodingRate != cOCT6100_G711_64KBPS )
				return cOCT6100_ERR_CHANNEL_ENCODING_RATE;

			if ( f_pTempChanOpen->CodecConfig.ulDecoderPort != cOCT6100_NO_DECODING && 
				 f_pTempChanOpen->CodecConfig.ulDecodingRate != cOCT6100_G711_64KBPS )
				return cOCT6100_ERR_CHANNEL_DECODING_RATE;
		}

	}
	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiModifyChannelResources

Description:    Reserves any new resources needed for the channel
-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pChannelModify		Pointer to echo cancellation channel configuration structure.
f_usChanIndex			Allocated entry in ECHO channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiModifyChannelResources
UINT32 Oct6100ApiModifyChannelResources(	
				IN  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN  tPOCT6100_CHANNEL_MODIFY			f_pChannelModify,
				IN	UINT16								f_usChanIndex,
				OUT	PUINT16								f_pusNewRinTsstIndex,
				OUT	PUINT16								f_pusNewSinTsstIndex,
				OUT	PUINT16								f_pusNewRoutTsstIndex,
				OUT	PUINT16								f_pusNewSoutTsstIndex )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;

	tPOCT6100_API_CHANNEL_TDM		pApiTdmConf;
	tPOCT6100_CHANNEL_MODIFY_TDM	pModifyTdmConf;

	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulTempVar = cOCT6100_ERR_OK;
	UINT32	ulFreeMixerEventCnt;
	
	BOOL	fRinReleased = FALSE;
	BOOL	fSinReleased = FALSE;
	BOOL	fRoutReleased = FALSE;
	BOOL	fSoutReleased = FALSE;

	BOOL	fRinReserved = FALSE;
	BOOL	fSinReserved = FALSE;
	BOOL	fRoutReserved = FALSE;
	BOOL	fSoutReserved = FALSE;

	BOOL	fReserveRin = FALSE;
	BOOL	fReserveSin = FALSE;
	BOOL	fReserveRout = FALSE;
	BOOL	fReserveSout = FALSE;

	BOOL	fRinRoutConversionMemReserved = FALSE;
	BOOL	fSinSoutConversionMemReserved = FALSE;


	UINT32	ulRinNumTssts = 1;
	UINT32	ulSinNumTssts = 1;
	UINT32	ulRoutNumTssts = 1;
	UINT32	ulSoutNumTssts = 1;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain local pointer to the TDM configuration structure of the tPOCT6100_CHANNEL_MODIFY structure. */
	pModifyTdmConf   = &f_pChannelModify->TdmConfig;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	/* Obtain local pointer to the TDM configuration structure of the tPOCT6100_API_CHANNEL structure. */
	pApiTdmConf   = &pChanEntry->TdmConfig;

	/*===============================================================================*/
	/* Modify TSST resources if required.*/
	if ( f_pChannelModify->fTdmConfigModified == TRUE )
	{
		/* First release any entry that need to be released.*/
		if ( ( pModifyTdmConf->ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			|| ( pModifyTdmConf->ulRinNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) 
			)
		{
			if ( pChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Release the previously reserved entry.*/
				ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
												  pChanEntry->TdmConfig.usRinTimeslot,
												  pChanEntry->TdmConfig.usRinStream, 
												  pChanEntry->TdmConfig.byRinNumTssts, 
												  cOCT6100_INPUT_TSST,
												  cOCT6100_INVALID_INDEX );
				if ( ulResult == cOCT6100_ERR_OK  )
				{
					fRinReleased = TRUE;
				}
			}

			fReserveRin = TRUE;
		}

		/* Release SIN port.*/
		if ( ( ulResult == cOCT6100_ERR_OK ) 
			&& ( ( pModifyTdmConf->ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
				|| ( pModifyTdmConf->ulSinNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING )
				) )
		{
			if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Release the previously reserved entry.*/
				ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
												  pChanEntry->TdmConfig.usSinTimeslot,
												  pChanEntry->TdmConfig.usSinStream, 
												  pChanEntry->TdmConfig.bySinNumTssts, 
												  cOCT6100_INPUT_TSST,
												  cOCT6100_INVALID_INDEX );
				if ( ulResult == cOCT6100_ERR_OK  )
				{
					fSinReleased = TRUE;
				}
			}

			fReserveSin = TRUE;
		}

		/* Release ROUT port.*/
		if ( ( ulResult == cOCT6100_ERR_OK ) 
			&& ( ( pModifyTdmConf->ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
				|| ( pModifyTdmConf->ulRoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) 
				) )
		{
			if ( pChanEntry->usRoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Release the previously reserved entry.*/
				ulResult = Oct6100ApiReleaseTsst( f_pApiInstance,
												  pChanEntry->TdmConfig.usRoutTimeslot,
												  pChanEntry->TdmConfig.usRoutStream, 
												  pChanEntry->TdmConfig.byRoutNumTssts, 
												  cOCT6100_OUTPUT_TSST,
												  cOCT6100_INVALID_INDEX );
				if ( ulResult == cOCT6100_ERR_OK  )
				{
					fRoutReleased = TRUE;
				}
			}

			fReserveRout = TRUE;
		}

		/* Release the SOUT port.*/
		if ( ( ulResult == cOCT6100_ERR_OK ) 
			&& ( ( pModifyTdmConf->ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
				|| ( pModifyTdmConf->ulSoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) 
				) )
		{
			if ( pChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Release the previously reserved entry.*/
				ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
												  pChanEntry->TdmConfig.usSoutTimeslot,
												  pChanEntry->TdmConfig.usSoutStream, 
												  pChanEntry->TdmConfig.bySoutNumTssts, 
												  cOCT6100_OUTPUT_TSST,
												  cOCT6100_INVALID_INDEX );
				if ( ulResult == cOCT6100_ERR_OK  )
				{
					fSoutReleased = TRUE;
				}
			}

			fReserveSout = TRUE;
		}

		/* Now reserve any new entry required.*/
		
		/* Modify RIN port.*/
		if ( ( fReserveRin == TRUE ) && ( ulResult == cOCT6100_ERR_OK ) )
		{
			if ( pModifyTdmConf->ulRinTimeslot != cOCT6100_UNASSIGNED )
			{
				/* Check what number of TSSTs should be reserved this time. */
				if ( pModifyTdmConf->ulRinNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					ulRinNumTssts = pApiTdmConf->byRinNumTssts;
				}
				else /* if ( pModifyTdmConf->ulRinNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* New number of TSSTs. */
					ulRinNumTssts = pModifyTdmConf->ulRinNumTssts;
				}

				if ( pModifyTdmConf->ulRinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* Reserve the new number of TSSTs. */
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pApiTdmConf->usRinTimeslot, 
												  pApiTdmConf->usRinStream, 
												  ulRinNumTssts, 
	  											  cOCT6100_INPUT_TSST,
												  f_pusNewRinTsstIndex, 
												  NULL );
				}
				else /* if ( pModifyTdmConf->ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* Reserve the new TSST.*/
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pModifyTdmConf->ulRinTimeslot, 
												  pModifyTdmConf->ulRinStream, 
												  ulRinNumTssts, 
	  											  cOCT6100_INPUT_TSST,
												  f_pusNewRinTsstIndex, 
												  NULL );
					if ( ulResult == cOCT6100_ERR_OK  )
					{
						fRinReserved = TRUE;
					}
				}
			}
			else
			{
				*f_pusNewRinTsstIndex = cOCT6100_INVALID_INDEX;
			}
		}
		else
		{
			*f_pusNewRinTsstIndex = cOCT6100_INVALID_INDEX;
		}

		/* Modify SIN port.*/
		if ( ( fReserveSin == TRUE ) && ( ulResult == cOCT6100_ERR_OK ) )
		{
			if ( pModifyTdmConf->ulSinTimeslot != cOCT6100_UNASSIGNED )
			{
				/* Check what number of TSSTs should be reserved this time. */
				if ( pModifyTdmConf->ulSinNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					ulSinNumTssts = pApiTdmConf->bySinNumTssts;
				}
				else /* if ( pModifyTdmConf->ulSinNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* New number of TSSTs. */
					ulSinNumTssts = pModifyTdmConf->ulSinNumTssts;
				}

				if ( pModifyTdmConf->ulSinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* Reserve the new number of TSSTs. */
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pApiTdmConf->usSinTimeslot, 
												  pApiTdmConf->usSinStream, 
												  ulSinNumTssts, 
	  											  cOCT6100_INPUT_TSST,
												  f_pusNewSinTsstIndex, 
												  NULL );
				}
				else /* if ( pModifyTdmConf->ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* Reserve the new TSST.*/
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pModifyTdmConf->ulSinTimeslot, 
												  pModifyTdmConf->ulSinStream, 
												  ulSinNumTssts, 
	  											  cOCT6100_INPUT_TSST,
												  f_pusNewSinTsstIndex, 
												  NULL );
					if ( ulResult == cOCT6100_ERR_OK )
					{
						fSinReserved = TRUE;
					}
				}
			}
			else
			{
				*f_pusNewSinTsstIndex = cOCT6100_INVALID_INDEX;
			}
		}
		else
		{
			*f_pusNewSinTsstIndex = cOCT6100_INVALID_INDEX;
		}

		/* Modify ROUT port.*/
		if ( ( fReserveRout == TRUE ) && ( ulResult == cOCT6100_ERR_OK ) )
		{
			if ( pModifyTdmConf->ulRoutTimeslot != cOCT6100_UNASSIGNED )
			{
				/* Check what number of TSSTs should be reserved this time. */
				if ( pModifyTdmConf->ulRoutNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					ulRoutNumTssts = pApiTdmConf->byRoutNumTssts;
				}
				else /* if ( pModifyTdmConf->ulRoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* New number of TSSTs. */
					ulRoutNumTssts = pModifyTdmConf->ulRoutNumTssts;
				}

				if ( pModifyTdmConf->ulRoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* Reserve the new number of TSSTs. */
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pApiTdmConf->usRoutTimeslot, 
												  pApiTdmConf->usRoutStream, 
												  ulRoutNumTssts, 
	  											  cOCT6100_OUTPUT_TSST,
												  f_pusNewRoutTsstIndex, 
												  NULL );
				}
				else /* if ( pModifyTdmConf->ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* Reserve the new TSST.*/
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pModifyTdmConf->ulRoutTimeslot, 
												  pModifyTdmConf->ulRoutStream, 
												  ulRoutNumTssts, 
	  											  cOCT6100_OUTPUT_TSST,
												  f_pusNewRoutTsstIndex, 
												  NULL );
					if ( ulResult == cOCT6100_ERR_OK  )
					{
						fRoutReserved = TRUE;
					}
				}
			}
			else
			{
				*f_pusNewRoutTsstIndex = cOCT6100_INVALID_INDEX;
			}
		}
		else
		{
			*f_pusNewRoutTsstIndex = cOCT6100_INVALID_INDEX;
		}

		/* Modify SOUT port.*/
		if ( ( fReserveSout == TRUE ) && ( ulResult == cOCT6100_ERR_OK ) )
		{
			if ( pModifyTdmConf->ulSoutTimeslot != cOCT6100_UNASSIGNED )
			{
				/* Check what number of TSSTs should be reserved this time. */
				if ( pModifyTdmConf->ulSoutNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					ulSoutNumTssts = pApiTdmConf->bySoutNumTssts;
				}
				else /* if ( pModifyTdmConf->ulSoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* New number of TSSTs. */
					ulSoutNumTssts = pModifyTdmConf->ulSoutNumTssts;
				}

				if ( pModifyTdmConf->ulSoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* Reserve the new number of TSSTs. */
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pApiTdmConf->usSoutTimeslot, 
												  pApiTdmConf->usSoutStream, 
												  ulSoutNumTssts, 
	  											  cOCT6100_OUTPUT_TSST,
												  f_pusNewSoutTsstIndex, 
												  NULL );
				}
				else /* if ( pModifyTdmConf->ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) */
				{
					/* Reserve the new TSST.*/
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  pModifyTdmConf->ulSoutTimeslot, 
												  pModifyTdmConf->ulSoutStream, 
												  ulSoutNumTssts, 
	  											  cOCT6100_OUTPUT_TSST,
												  f_pusNewSoutTsstIndex, 
												  NULL );
					if ( ulResult == cOCT6100_ERR_OK  )
					{
						fSoutReserved = TRUE;
					}
				}
			}
			else
			{
				*f_pusNewSoutTsstIndex = cOCT6100_INVALID_INDEX;
			}
		}
		else
		{
			*f_pusNewSoutTsstIndex = cOCT6100_INVALID_INDEX;
		}


	}

	if ( f_pChannelModify->fCodecConfigModified == TRUE )
	{
		if (   ulResult == cOCT6100_ERR_OK &&
			   pChanEntry->usRinRoutConversionMemIndex == cOCT6100_INVALID_INDEX &&
			 ( f_pChannelModify->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT ||
			   f_pChannelModify->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN ) )
		{
			/* Reserve an ADPCM memory block.*/
			ulResult = Oct6100ApiReserveConversionMemEntry( f_pApiInstance, &pChanEntry->usRinRoutConversionMemIndex );
			if ( ulResult == cOCT6100_ERR_OK  )
			{
				fRinRoutConversionMemReserved = TRUE;
			}
		}

		if (   ulResult == cOCT6100_ERR_OK && 
			   pChanEntry->usSinSoutConversionMemIndex == cOCT6100_INVALID_INDEX &&
			 ( f_pChannelModify->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT ||
			   f_pChannelModify->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN ) )
		{
			/* Reserve an ADPCM memory block.*/
			ulResult = Oct6100ApiReserveConversionMemEntry( f_pApiInstance, &pChanEntry->usSinSoutConversionMemIndex );
			if ( ulResult == cOCT6100_ERR_OK  )
			{
				fSinSoutConversionMemReserved = TRUE;
			}
		}
	}


	/*===============================================================================*/
	/* Check if there are a couple of mixer events available for us. */

	if ( ulResult == cOCT6100_ERR_OK )
	{
		UINT32 ulMixerEventCntNeeded = 0;

		/* Calculate how many mixer events are needed. */
		if ( pChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
		{
			/* If the channel is in bidir mode, do not create the Rin silence event!!! */
			if ( pChanEntry->fBiDirChannel == FALSE )
			{
				if ( ( *f_pusNewRinTsstIndex == cOCT6100_INVALID_INDEX )
					&& ( pChanEntry->usRinSilenceEventIndex == cOCT6100_INVALID_INDEX ) )
					ulMixerEventCntNeeded++;
			}
		}

		if ( ( *f_pusNewSinTsstIndex == cOCT6100_INVALID_INDEX )
			&& ( pChanEntry->usSinSilenceEventIndex == cOCT6100_INVALID_INDEX ) )
		{
			ulMixerEventCntNeeded++;
		}

		/* If at least 1 mixer event is needed, check if those are available. */
		if ( ulMixerEventCntNeeded != 0 )
		{
			ulResult = Oct6100ApiGetFreeMixerEventCnt( f_pApiInstance, &ulFreeMixerEventCnt );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				/* The API might need more mixer events if the ports have to be muted. */
				/* Check if these are available. */
				if ( ulFreeMixerEventCnt < ulMixerEventCntNeeded )
				{
					ulResult = cOCT6100_ERR_CHANNEL_OUT_OF_MIXER_EVENTS;
				}
			}
		}
	}

	/*===============================================================================*/

	/* Verify if an error occured. */
	if ( ulResult != cOCT6100_ERR_OK )
	{
		/* Release any resources newly reserved.*/
		if ( fRinReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  pModifyTdmConf->ulRinTimeslot,
											  pModifyTdmConf->ulRinStream, 
											  ulRinNumTssts, 
											  cOCT6100_INPUT_TSST,
											  cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the SIN port.*/
		if ( fSinReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  pModifyTdmConf->ulSinTimeslot,
											  pModifyTdmConf->ulSinStream, 
											  ulSinNumTssts, 
											  cOCT6100_INPUT_TSST,
											  cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the ROUT port.*/
		if ( fRoutReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  pModifyTdmConf->ulRoutTimeslot,
											  pModifyTdmConf->ulRoutStream, 
											  ulRoutNumTssts, 
											  cOCT6100_OUTPUT_TSST,
											  cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the SOUT port.*/
		if ( fSoutReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  pModifyTdmConf->ulSoutTimeslot,
											  pModifyTdmConf->ulSoutStream, 
											  ulSoutNumTssts, 
											  cOCT6100_OUTPUT_TSST,
											  cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* Now make sure any resources released gets reserved back again.*/

		/* For the RIN port.*/
		if ( fRinReleased == TRUE )
		{
			/* Reserve the new TSST.*/
			ulTempVar = Oct6100ApiReserveTsst( f_pApiInstance, 
											  pChanEntry->TdmConfig.usRinTimeslot, 
											  pChanEntry->TdmConfig.usRinStream, 
											  pChanEntry->TdmConfig.byRinNumTssts, 
	  										  cOCT6100_INPUT_TSST,
											  &pChanEntry->usRinTsstIndex, 
											  NULL );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the SIN port.*/
		if ( fSinReleased == TRUE )
		{
			/* Reserve the new TSST.*/
			ulTempVar = Oct6100ApiReserveTsst( f_pApiInstance, 
											  pChanEntry->TdmConfig.usSinTimeslot, 
											  pChanEntry->TdmConfig.usSinStream, 
											  pChanEntry->TdmConfig.bySinNumTssts, 
	  										  cOCT6100_INPUT_TSST,
											  &pChanEntry->usSinTsstIndex, 
											  NULL );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the ROUT port.*/
		if ( fRoutReleased == TRUE )
		{
			/* Reserve the new TSST.*/
			ulTempVar = Oct6100ApiReserveTsst( f_pApiInstance, 
											  pChanEntry->TdmConfig.usRoutTimeslot, 
											  pChanEntry->TdmConfig.usRoutStream, 
											  pChanEntry->TdmConfig.byRoutNumTssts, 
	  										  cOCT6100_OUTPUT_TSST,
											  &pChanEntry->usRoutTsstIndex, 
											  NULL );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* For the SOUT port.*/
		if ( fSoutReleased == TRUE )
		{
			/* Reserve the new TSST.*/
			ulTempVar = Oct6100ApiReserveTsst( f_pApiInstance, 
											  pChanEntry->TdmConfig.usSoutTimeslot, 
											  pChanEntry->TdmConfig.usSoutStream, 
											  pChanEntry->TdmConfig.bySoutNumTssts, 
	  										  cOCT6100_OUTPUT_TSST,
											  &pChanEntry->usSoutTsstIndex, 
											  NULL );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/* Release the conversion memories if they were reserved.*/
		if ( fRinRoutConversionMemReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, 
													    pChanEntry->usRinRoutConversionMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( fSinSoutConversionMemReserved == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, 
													    pChanEntry->usSinSoutConversionMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}
		
		/* Now return the error.*/
		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiModifyChannelStructs

Description:    Performs all the required structure writes to configure the
				echo cancellation channel based on the new modifications.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
					
f_pChannelModify			Pointer to echo cancellation channel configuration structure.
f_pChannelOpen				Pointer to a structure used to store the multiple resources indexes.
f_usChanIndex				Index of the channel within the API's channel list.
f_usNewPhasingTsstIndex		Index of the new phasing TSST.
f_pfSinSoutCodecActive		Pointer to the state of the SIN/SOUT codec.
f_pfRinRoutCodecActive		Pointer to the state of the RIN/ROUT codec.
f_usNewRinTsstIndex			New RIN TSST memory index.
f_usNewSinTsstIndex			New SIN TSST memory index.
f_usNewRoutTsstIndex		New ROUT TSST memory index.
f_usNewSoutTsstIndex		New SOUT TSST memory index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiModifyChannelStructs
UINT32 Oct6100ApiModifyChannelStructs(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_MODIFY		f_pChannelModify, 
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen, 
				IN	UINT16							f_usChanIndex,
				IN	UINT16							f_usNewPhasingTsstIndex,
				OUT	PUINT8							f_pfSinSoutCodecActive,
				OUT	PUINT8							f_pfRinRoutCodecActive,
				IN	UINT16							f_usNewRinTsstIndex,
				IN	UINT16							f_usNewSinTsstIndex,
				IN	UINT16							f_usNewRoutTsstIndex,
				IN	UINT16							f_usNewSoutTsstIndex )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_READ_PARAMS	ReadParams;
	tOCT6100_WRITE_PARAMS	WriteParams;
	tPOCT6100_API_CHANNEL_CODEC	pApiCodecConf;
	tPOCT6100_API_CHANNEL_TDM	pApiTdmConf;

	UINT32	ulResult;
	UINT16	usReadData;

	UINT16	usSinTsstIndex;
	UINT16	usRinTsstIndex;

	UINT32	ulToneConfIndex;
	BOOL	fClearPlayoutPointers = FALSE;



	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	/* Obtain local pointer to the configuration structures of the tPOCT6100_API_CHANNEL structure. */
	pApiCodecConf = &pChanEntry->CodecConfig;
	pApiTdmConf   = &pChanEntry->TdmConfig;

	/*=======================================================================*/
	/* Init the RIN and SIN TSST index */
	
	usRinTsstIndex = pChanEntry->usRinTsstIndex;
	usSinTsstIndex = pChanEntry->usSinTsstIndex;


	/*==============================================================================*/
	/* Clear the TSST that will be release.*/

	if ( f_pChannelModify->fTdmConfigModified == TRUE )	
	{
		/* Modify RIN port.*/
		if ( f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			if ( pChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Clear the previous entry  */
				WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pChanEntry->usRinTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData  = 0x0000;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK  )
					return ulResult;
			}
		}

		/* Modify SIN port.*/
		if ( f_pChannelModify->TdmConfig.ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Clear the previous entry  */
				WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pChanEntry->usSinTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData  = 0x0000;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK  )
					return ulResult;
			}
		}

		/* Modify ROUT port.*/
		if ( f_pChannelModify->TdmConfig.ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			if ( pChanEntry->usRoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Clear the previous entry  */
				WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pChanEntry->usRoutTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData  = 0x0000;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK  )
					return ulResult;
			}
		}

		/* Modify SOUT port.*/
		if ( f_pChannelModify->TdmConfig.ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING)
		{
			if ( pChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				/* Clear the previous entry  */
				WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (pChanEntry->usSoutTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData  = 0x0000;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK  )
					return ulResult;
			}
		}
	}
	/*==============================================================================*/

	
	/*==============================================================================*/
	/* Now, Configure the Tsst control memory.*/

	if ( f_pChannelModify->fTdmConfigModified == TRUE )	
	{
		/* Modify RIN port.*/
		if ( f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			usRinTsstIndex = f_usNewRinTsstIndex;

			if ( f_usNewRinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				if ( pChanEntry->usExtraRinTsiMemIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  f_usNewRinTsstIndex,
																	  pChanEntry->usExtraRinTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulRinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
				else
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  f_usNewRinTsstIndex,
																	  pChanEntry->usRinRoutTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulRinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
		}
		if ( f_pChannelModify->TdmConfig.ulRinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING &&
			 f_pChannelModify->TdmConfig.ulRinPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			if ( pChanEntry->usExtraRinTsiMemIndex != cOCT6100_INVALID_INDEX )
			{
				if ( pChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pChanEntry->usRinTsstIndex,
																	  pChanEntry->usExtraRinTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulRinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
			else
			{
				if ( pChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pChanEntry->usRinTsstIndex,
																	  pChanEntry->usRinRoutTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulRinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
		}

		/* Modify SIN port.*/
		if ( f_pChannelModify->TdmConfig.ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			usSinTsstIndex = f_usNewSinTsstIndex;

			if ( f_usNewSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				if ( pChanEntry->usExtraSinTsiMemIndex != cOCT6100_INVALID_INDEX )
				{
					/* Write the new entry now.*/
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  f_usNewSinTsstIndex,
																	  pChanEntry->usExtraSinTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulSinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
				else
				{
					/* Write the new entry now.*/
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  f_usNewSinTsstIndex,
																	  pChanEntry->usSinSoutTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulSinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
		}
		if ( f_pChannelModify->TdmConfig.ulSinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING &&
			 f_pChannelModify->TdmConfig.ulSinPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING )
		{
			if ( pChanEntry->usExtraSinTsiMemIndex != cOCT6100_INVALID_INDEX )
			{
				if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pChanEntry->usSinTsstIndex ,
																	  pChanEntry->usExtraSinTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulSinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
			else
			{
				if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pChanEntry->usSinTsstIndex ,
																	  pChanEntry->usSinSoutTsiMemIndex,
																	  f_pChannelOpen->TdmConfig.ulSinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK  )
						return ulResult;
				}
			}
		}

		/* Modify ROUT port.*/
		if ( ( f_pChannelModify->TdmConfig.ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			|| ( f_pChannelModify->TdmConfig.ulRoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) 
			)
		{
			if ( f_usNewRoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				if ( f_pChannelModify->TdmConfig.ulRoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* If this output port is not muted. */
					if ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) == 0x0 )
					{
						/* Write the new entry now.*/
						ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
																	   f_usNewRoutTsstIndex,
																	   pApiCodecConf->byAdpcmNibblePosition,
																	   f_pChannelModify->TdmConfig.ulRoutNumTssts,
																	   pChanEntry->usRinRoutTsiMemIndex );
						if ( ulResult != cOCT6100_ERR_OK  )
							return ulResult;
					}
				}
				else
				{
					/* If this output port is not muted. */
					if ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) == 0x0 )
					{
						/* Write the new entry now.*/
						ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
																   f_usNewRoutTsstIndex,
																   pApiCodecConf->byAdpcmNibblePosition,
																   pApiTdmConf->byRoutNumTssts,
																   pChanEntry->usRinRoutTsiMemIndex );
						if ( ulResult != cOCT6100_ERR_OK  )
							return ulResult;
					}
				}
			}
		}

		/* Modify SOUT port.*/
		if ( ( f_pChannelModify->TdmConfig.ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING )
			|| ( f_pChannelModify->TdmConfig.ulSoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING ) 
			)
		{
			if ( f_usNewSoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				if ( f_pChannelModify->TdmConfig.ulSoutNumTssts != cOCT6100_KEEP_PREVIOUS_SETTING )
				{
					/* If this output port is not muted. */
					if ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) == 0x0 )
					{
						/* Write the new entry now.*/
						ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
																	   f_usNewSoutTsstIndex,
																	   pApiCodecConf->byAdpcmNibblePosition,
																	   f_pChannelModify->TdmConfig.ulSoutNumTssts,
																	   pChanEntry->usSinSoutTsiMemIndex );

						if ( ulResult != cOCT6100_ERR_OK  )
							return ulResult;
					}
				}
				else
				{
					/* If this output port is not muted. */
					if ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) == 0x0 )
					{
						/* Write the new entry now.*/
						ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
																   f_usNewSoutTsstIndex,
																   pApiCodecConf->byAdpcmNibblePosition,
																   pApiTdmConf->bySoutNumTssts,
																   pChanEntry->usSinSoutTsiMemIndex );

						if ( ulResult != cOCT6100_ERR_OK  )
							return ulResult;
					}
				}
			}
		}



	}

	/*==============================================================================*/


	/*==============================================================================*/
	/* Modify the Encoder/Decoder memory if required.*/

	if ( f_pChannelModify->fCodecConfigModified == TRUE )
	{
		UINT32	ulCompType = 0;
		UINT16	usTempTsiMemIndex;
		UINT16	usDecoderMemIndex;
		UINT16	usEncoderMemIndex;
		UINT32	ulPcmLaw;
		UINT16	usPhasingIndex;
		BOOL	fModifyAdpcmMem = TRUE;

		/*==============================================================================*/
		/* Reprogram the Decoder memory.*/

		if ( pChanEntry->CodecConfig.byDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
		{
			usDecoderMemIndex = pChanEntry->usRinRoutConversionMemIndex;
		}
		else
		{
			usDecoderMemIndex = pChanEntry->usSinSoutConversionMemIndex;
		}

		if ( pChanEntry->CodecConfig.byEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
		{
			usEncoderMemIndex = pChanEntry->usRinRoutConversionMemIndex;
		}
		else
		{
			usEncoderMemIndex = pChanEntry->usSinSoutConversionMemIndex;
		}

		if ( usDecoderMemIndex != cOCT6100_INVALID_INDEX  )
		{
			switch( f_pChannelOpen->CodecConfig.ulDecodingRate )
			{
			case cOCT6100_G711_64KBPS:				
				ulCompType = 0x8;
				
				if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
				{
					if ( f_pChannelOpen->TdmConfig.ulRinPcmLaw == f_pChannelOpen->TdmConfig.ulRoutPcmLaw )
						fModifyAdpcmMem = FALSE;

					/* Check if both ports are assigned.  If not, no law conversion needed here.. */
					if ( ( f_pChannelOpen->TdmConfig.ulRinStream == cOCT6100_UNASSIGNED ) 
						|| ( f_pChannelOpen->TdmConfig.ulRoutStream == cOCT6100_UNASSIGNED ) )
						fModifyAdpcmMem = FALSE;
				}
				else /* f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
				{
					if ( f_pChannelOpen->TdmConfig.ulSinPcmLaw == f_pChannelOpen->TdmConfig.ulSoutPcmLaw )
						fModifyAdpcmMem = FALSE;

					/* Check if both ports are assigned.  If not, no law conversion needed here.. */
					if ( ( f_pChannelOpen->TdmConfig.ulSinStream == cOCT6100_UNASSIGNED ) 
						|| ( f_pChannelOpen->TdmConfig.ulSoutStream == cOCT6100_UNASSIGNED ) )
						fModifyAdpcmMem = FALSE;
				}

				break;
			case cOCT6100_G726_40KBPS:				
				ulCompType = 0x3;		
				break;

			case cOCT6100_G726_32KBPS:				
				ulCompType = 0x2;		
				break;

			case cOCT6100_G726_24KBPS:				
				ulCompType = 0x1;		
				break;

			case cOCT6100_G726_16KBPS:				
				ulCompType = 0x0;		
				break;		

			case cOCT6100_G727_2C_ENCODED:			
				ulCompType = 0x4;		
				break;

			case cOCT6100_G727_3C_ENCODED:			
				ulCompType = 0x5;		
				break;

			case cOCT6100_G727_4C_ENCODED:			
				ulCompType = 0x6;		
				break;

			case cOCT6100_G726_ENCODED:				
				ulCompType = 0x9;		
				break;

			case cOCT6100_G711_G726_ENCODED:		
				ulCompType = 0xA;		
				break;

			case cOCT6100_G711_G727_2C_ENCODED:		
				ulCompType = 0xC;		
				break;

			case cOCT6100_G711_G727_3C_ENCODED:		
				ulCompType = 0xD;		
				break;

			case cOCT6100_G711_G727_4C_ENCODED:		
				ulCompType = 0xE;		
				break;

			default:
				return cOCT6100_ERR_FATAL_D6;
			}

			if ( fModifyAdpcmMem == TRUE )
			{
				/* Set the chariot memory based on the selected port.*/
				if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
				{
					usTempTsiMemIndex = pChanEntry->usRinRoutTsiMemIndex;
					ulPcmLaw = f_pChannelOpen->TdmConfig.ulRoutPcmLaw;		/* Set the law for later use */
					
					/* Flag the entry as active.*/
					*f_pfRinRoutCodecActive = TRUE;
				}
				else /* f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
				{
					usTempTsiMemIndex = pChanEntry->usSinSoutTsiMemIndex;
					ulPcmLaw = f_pChannelOpen->TdmConfig.ulSoutPcmLaw;		/* Set the law for later use */

					/* Flag the entry as active.*/
					*f_pfSinSoutCodecActive = TRUE;
				}

				ulResult = Oct6100ApiWriteDecoderMemory( f_pApiInstance,
														 usDecoderMemIndex,
														 ulCompType,
														 usTempTsiMemIndex,
														 ulPcmLaw,
														 pApiCodecConf->byAdpcmNibblePosition );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
			else
			{
				ulResult = Oct6100ApiClearConversionMemory( f_pApiInstance,
														 usDecoderMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Flag the entry as deactivated.*/
				if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
				{
					*f_pfRinRoutCodecActive = FALSE;
				}
				else
				{
					*f_pfSinSoutCodecActive = FALSE;
				}
			}
		}

		/*==============================================================================*/




		/*==============================================================================*/
		/* Reprogram the Encoder memory.*/
	
		if ( usEncoderMemIndex != cOCT6100_INVALID_INDEX )
		{

			fModifyAdpcmMem = TRUE;
			
			/* Set the chariot memory based on the selected port.*/
			if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
			{
				usTempTsiMemIndex = pChanEntry->usRinRoutTsiMemIndex;
				ulPcmLaw = f_pChannelOpen->TdmConfig.ulRoutPcmLaw;		/* Set the law for later use */
			}
			else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
			{
				usTempTsiMemIndex = pChanEntry->usSinSoutTsiMemIndex;
				ulPcmLaw = f_pChannelOpen->TdmConfig.ulSoutPcmLaw;		/* Set the law for later use */
			}

			/* Set the phasing index .*/
			if ( f_usNewPhasingTsstIndex != cOCT6100_INVALID_INDEX )
				usPhasingIndex = f_usNewPhasingTsstIndex;
			else
				usPhasingIndex = pChanEntry->usPhasingTsstIndex;

			switch( f_pChannelOpen->CodecConfig.ulEncodingRate )
			{
				case cOCT6100_G711_64KBPS:
					if ( ulPcmLaw == cOCT6100_PCM_U_LAW )
						ulCompType = 0x4;
					else /* ulPcmLaw  == cOCT6100_PCM_A_LAW */
						ulCompType = 0x5;
					
					if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
					{
						if ( f_pChannelOpen->TdmConfig.ulRinPcmLaw == f_pChannelOpen->TdmConfig.ulRoutPcmLaw )
							fModifyAdpcmMem = FALSE;

						/* Check if both ports are assigned.  If not, no law conversion needed here.. */
						if ( ( f_pChannelOpen->TdmConfig.ulRinStream == cOCT6100_UNASSIGNED ) 
							|| ( f_pChannelOpen->TdmConfig.ulRoutStream == cOCT6100_UNASSIGNED ) )
							fModifyAdpcmMem = FALSE;
					}
					else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
					{
						if ( f_pChannelOpen->TdmConfig.ulSinPcmLaw == f_pChannelOpen->TdmConfig.ulSoutPcmLaw )
							fModifyAdpcmMem = FALSE;

						/* Check if both ports are assigned.  If not, no law conversion needed here.. */
						if ( ( f_pChannelOpen->TdmConfig.ulSinStream == cOCT6100_UNASSIGNED ) 
							|| ( f_pChannelOpen->TdmConfig.ulSoutStream == cOCT6100_UNASSIGNED ) )
							fModifyAdpcmMem = FALSE;
					}
					break;
				case cOCT6100_G726_40KBPS:				
					ulCompType = 0x3;		
					break;

				case cOCT6100_G726_32KBPS:				
					ulCompType = 0x2;		
					break;

				case cOCT6100_G726_24KBPS:				
					ulCompType = 0x1;		
					break;

				case cOCT6100_G726_16KBPS:				
					ulCompType = 0x0;		
					break;		

				case cOCT6100_G727_40KBPS_4_1:			
					ulCompType = 0xD;		
					break;

				case cOCT6100_G727_40KBPS_3_2:			
					ulCompType = 0xA;		
					break;

				case cOCT6100_G727_40KBPS_2_3:			
					ulCompType = 0x6;		
					break;

				case cOCT6100_G727_32KBPS_4_0:			
					ulCompType = 0xE;		
					break;

				case cOCT6100_G727_32KBPS_3_1:			
					ulCompType = 0xB;		
					break;

				case cOCT6100_G727_32KBPS_2_2:			
					ulCompType = 0x7;		
					break;

				case cOCT6100_G727_24KBPS_3_0:			
					ulCompType = 0xC;		
					break;

				case cOCT6100_G727_24KBPS_2_1:			
					ulCompType = 0x8;		
					break;

				case cOCT6100_G727_16KBPS_2_0:			
					ulCompType = 0x9;		
					break;

				default:
					return cOCT6100_ERR_FATAL_D7;
			}

			if ( fModifyAdpcmMem == TRUE )
			{
				if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
				{
					*f_pfRinRoutCodecActive	= TRUE;
				}
				else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
				{
					*f_pfSinSoutCodecActive	= TRUE;
				}
				
				ulResult = Oct6100ApiWriteEncoderMemory( f_pApiInstance,
														 usEncoderMemIndex,
														 ulCompType,
														 usTempTsiMemIndex,
														 f_pChannelOpen->CodecConfig.fEnableSilenceSuppression,
														 pApiCodecConf->byAdpcmNibblePosition,
														 usPhasingIndex,
														 f_pChannelOpen->CodecConfig.ulPhasingType,
														 f_pChannelOpen->CodecConfig.ulPhase );
													 
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
			else
			{
				ulResult = Oct6100ApiClearConversionMemory( f_pApiInstance,
															usEncoderMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				if ( f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT )
				{
					*f_pfRinRoutCodecActive	= FALSE;
				}
				else /* f_pChannelOpen->CodecConfig.ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT */
				{
					*f_pfSinSoutCodecActive	= FALSE;
				}
			}
		}

		/*==============================================================================*/
	}



	
	/*==============================================================================*/
	/* Modify the VQE parameter if required.*/

	if ( ( f_pChannelModify->fVqeConfigModified == TRUE )
		|| ( (UINT8)f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode )
		|| ( f_pChannelOpen->fEnableToneDisabler != pChanEntry->fEnableToneDisabler ) )
	{
		ulResult = Oct6100ApiWriteVqeMemory( f_pApiInstance,
											  &f_pChannelOpen->VqeConfig,
											  f_pChannelOpen,
											  f_usChanIndex,
											  pChanEntry->usEchoMemIndex,
											  FALSE,
											  TRUE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*==============================================================================*/
	/* Modify the Echo memory if required.*/
	if ( f_pChannelModify->fEnableToneDisabler		 != cOCT6100_KEEP_PREVIOUS_SETTING ||
		 f_pChannelModify->ulEchoOperationMode       != cOCT6100_KEEP_PREVIOUS_SETTING ||
		 f_pChannelModify->TdmConfig.ulRinPcmLaw	 != cOCT6100_KEEP_PREVIOUS_SETTING ||
		 f_pChannelModify->TdmConfig.ulSinPcmLaw	 != cOCT6100_KEEP_PREVIOUS_SETTING ||
		 f_pChannelModify->TdmConfig.ulRoutPcmLaw	 != cOCT6100_KEEP_PREVIOUS_SETTING ||
		 f_pChannelModify->TdmConfig.ulSoutPcmLaw	 != cOCT6100_KEEP_PREVIOUS_SETTING )
	{
		ulResult = Oct6100ApiWriteEchoMemory( f_pApiInstance,
											  &f_pChannelOpen->TdmConfig,
											  f_pChannelOpen,
											  pChanEntry->usEchoMemIndex,
											  pChanEntry->usRinRoutTsiMemIndex,
											  pChanEntry->usSinSoutTsiMemIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Synch all the buffer playout field if needed by echo operation mode. */
		/* Note that Oct6100ApiWriteVqeMemory does not clear the playout pointers */
		/* since the flag is set to FALSE. */
		if ( ( pSharedInfo->ImageInfo.fBufferPlayout == TRUE ) 
			&& ( ( f_pChannelModify->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_HT_FREEZE )
				|| ( f_pChannelModify->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_POWER_DOWN ) ) )
		{	
			/* Buffer playout must be stopped. */
			fClearPlayoutPointers = TRUE;
		}
	}

	/*==============================================================================*/
	/* Modify the Mixer events if law changes are requested. */
	
	if ( pChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX && 
		 f_pChannelModify->TdmConfig.ulSinPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING )
	{
		ReadParams.ulReadAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Modify the value according to the new law.*/
		if ( f_pChannelModify->TdmConfig.ulSinPcmLaw == cOCT6100_PCM_A_LAW )
			WriteParams.usWriteData = (UINT16)( usReadData | ( f_pChannelModify->TdmConfig.ulSinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET ));
		else
			WriteParams.usWriteData = (UINT16)( usReadData & (~( f_pChannelModify->TdmConfig.ulSinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET )));

		/* Write back the new value.*/
		WriteParams.ulWriteAddress = ReadParams.ulReadAddress;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	if ( pChanEntry->usSoutCopyEventIndex != cOCT6100_INVALID_INDEX && 
		 f_pChannelModify->TdmConfig.ulSoutPcmLaw != cOCT6100_KEEP_PREVIOUS_SETTING )
	{
		ReadParams.ulReadAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usSoutCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Modify the value according to the new law.*/
		if ( f_pChannelModify->TdmConfig.ulSoutPcmLaw == cOCT6100_PCM_A_LAW )
			WriteParams.usWriteData = (UINT16)( usReadData | ( f_pChannelModify->TdmConfig.ulSoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET ));
		else
			WriteParams.usWriteData = (UINT16)( usReadData & (~( f_pChannelModify->TdmConfig.ulSoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET )));

		/* Write back the new value.*/
		WriteParams.ulWriteAddress = ReadParams.ulReadAddress;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*==============================================================================*/
	/*	Mute channel if required, this is done on a port basis */
	
	ulResult = Oct6100ApiMutePorts( f_pApiInstance,
									f_usChanIndex,
									usRinTsstIndex,
									usSinTsstIndex,
									TRUE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/

	/* Completely disable tone detection? */
	if ( f_pChannelModify->fDisableToneDetection == TRUE )
	{
		/* Check if tone detection has been enabled on this channel. */
		for (ulToneConfIndex = 0; ulToneConfIndex < ARRAY_SIZE(pChanEntry->aulToneConf); ulToneConfIndex++)
		{
			/* Check if some tone has been activated on this channel. */
			if ( pChanEntry->aulToneConf[ ulToneConfIndex ] != 0 )
			{
				tOCT6100_TONE_DETECTION_DISABLE		ToneDetectDisable;

				/* Call the default function to make sure all parameters are assigned default values. */
				ulResult = Oct6100ToneDetectionDisableDef( &ToneDetectDisable );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
				
				/* Form channel handle. */
				ToneDetectDisable.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

				/* Disable all tones activated on this channel. */
				ToneDetectDisable.fDisableAll = TRUE;

				/* Call tone detection serialized function. */
				ulResult = Oct6100ToneDetectionDisableSer( f_pApiInstance, &ToneDetectDisable );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Get out of the loop, tone detection has been disabled! */
				break;
			}
		}
	}

	/* Hard-stop buffer playout? */
	if ( f_pChannelModify->fStopBufferPlayout == TRUE )
	{
		/* Check if playout has been started on the Rout port. */
		if ( ( pChanEntry->fRinBufPlaying == TRUE ) || ( pChanEntry->fRinBufAdded == TRUE ) )
		{
			tOCT6100_BUFFER_PLAYOUT_STOP	PlayoutStop;

			/* Call the default function to make sure all parameters are assigned default values. */
			ulResult = Oct6100BufferPlayoutStopDef( &PlayoutStop );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Hard stop request. */
			PlayoutStop.fStopCleanly = FALSE;

			/* Form channel handle. */
			PlayoutStop.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

			/* For the Rout port. */
			PlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;

			/* Call buffer playout stop serialized function. */
			ulResult = Oct6100BufferPlayoutStopSer( f_pApiInstance, &PlayoutStop );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		else
		{
			/* The chip might still be playing a last buffer.  Make sure it hard-stops! */
			fClearPlayoutPointers = TRUE;
		}

		/* Check if playout has been started on the Sout port. */
		if ( ( pChanEntry->fSoutBufPlaying == TRUE ) || ( pChanEntry->fSoutBufAdded == TRUE ) )
		{
			tOCT6100_BUFFER_PLAYOUT_STOP	PlayoutStop;

			/* Call the default function to make sure all parameters are assigned default values. */
			ulResult = Oct6100BufferPlayoutStopDef( &PlayoutStop );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Hard stop request. */
			PlayoutStop.fStopCleanly = FALSE;

			/* Form channel handle. */
			PlayoutStop.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

			/* For the Rout port. */
			PlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_SOUT;

			/* Call buffer playout stop serialized function. */
			ulResult = Oct6100BufferPlayoutStopSer( f_pApiInstance, &PlayoutStop );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		else
		{
			/* The chip might still be playing a last buffer.  Make sure it hard-stops! */
			fClearPlayoutPointers = TRUE;
		}
	}

	/* Remove participant from bridge? */
	if ( f_pChannelModify->fRemoveConfBridgeParticipant == TRUE )
	{
		/* Check if this channel is on a bridge. */
		if ( pChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		{
			/* Channel is on a bridge, remove it. */
			tOCT6100_CONF_BRIDGE_CHAN_REMOVE	BridgeChanRemove;

			/* Call the default function to make sure all parameters are assigned default values. */
			ulResult = Oct6100ConfBridgeChanRemoveDef( &BridgeChanRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Form channel handle. */
			BridgeChanRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

			/* Do not remove all channels, only the one specified. */
			BridgeChanRemove.fRemoveAll = FALSE;

			/* No need to assign conference bridge handle, the remove function will figure it out. */

			/* Call conference bridge channel remove serialized function. */
			ulResult = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, &BridgeChanRemove );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				if ( ulResult == cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_DEPENDENCY )
				{
					tPOCT6100_API_CHANNEL		pTapChanEntry;

					/* Get a pointer to the tap channel's list entry. */
					mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTapChanEntry, pChanEntry->usTapChanIndex )

					/* Form tap channel handle. */
					BridgeChanRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pTapChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | pChanEntry->usTapChanIndex;

					ulResult = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, &BridgeChanRemove );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					/* Re-form original channel handle. */
					BridgeChanRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

					ulResult = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, &BridgeChanRemove );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
				else
				{
					return ulResult;
				}
			}
		}
	}

	/* Remove all broadcast TSSTs? */
	if ( f_pChannelModify->fRemoveBroadcastTssts == TRUE )
	{
		/* Check if broadcast TSSTs were used on the Rout port. */
		if ( pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry != cOCT6100_INVALID_INDEX )
		{
			tOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	BroadcastTsstRemove;

			ulResult = Oct6100ChannelBroadcastTsstRemoveDef( &BroadcastTsstRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Form channel handle. */
			BroadcastTsstRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

			/* Remove all broadcast TSSTs associated to the current channel. */
			BroadcastTsstRemove.fRemoveAll = TRUE;

			/* On the Rout port. */
			BroadcastTsstRemove.ulPort = cOCT6100_CHANNEL_PORT_ROUT; 

			ulResult = Oct6100ChannelBroadcastTsstRemoveSer( f_pApiInstance, &BroadcastTsstRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}


		/* Check if broadcast TSSTs were used on the Sout port. */
		if ( pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry != cOCT6100_INVALID_INDEX )
		{
			tOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	BroadcastTsstRemove;

			ulResult = Oct6100ChannelBroadcastTsstRemoveDef( &BroadcastTsstRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Form channel handle. */
			BroadcastTsstRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | ( pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT ) | f_usChanIndex;

			/* Remove all broadcast TSSTs associated to the current channel. */
			BroadcastTsstRemove.fRemoveAll = TRUE;

			/* On the Sout port. */
			BroadcastTsstRemove.ulPort = cOCT6100_CHANNEL_PORT_SOUT;

			ulResult = Oct6100ChannelBroadcastTsstRemoveSer( f_pApiInstance, &BroadcastTsstRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Check if have to make sure buffer playout is stopped. */
	if ( fClearPlayoutPointers == TRUE )
	{	
		tOCT6100_BUFFER_PLAYOUT_STOP	BufferPlayoutStop;

		Oct6100BufferPlayoutStopDef( &BufferPlayoutStop );

		BufferPlayoutStop.fStopCleanly = FALSE;
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;

		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
													f_pApiInstance, 
													&BufferPlayoutStop, 
													f_usChanIndex, 
													pChanEntry->usEchoMemIndex 

													);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_SOUT;
		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
													f_pApiInstance, 
													&BufferPlayoutStop, 
													f_usChanIndex, 
													pChanEntry->usEchoMemIndex 

													);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiModifyChannelEntry

Description:    Updates the channel structure in the ECHO channel list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pChannelModify			Pointer to echo cancellation channel modify structure.
f_pChannelOpen				Pointer to echo cancellation channel configuration structure.
f_usChanIndex				Index of the channel within the API's channel list.
f_usNewPhasingTsstIndex		Index of the new phasing TSST.
f_fSinSoutCodecActive		State of the SIN/SOUT codec.
f_fRinRoutCodecActive		State of the RIN/ROUT codec.
f_usNewRinTsstIndex			New RIN TSST memory index.
f_usNewSinTsstIndex			New SIN TSST memory index.
f_usNewRoutTsstIndex		New ROUT TSST memory index.
f_usNewSoutTsstIndex		New SOUT TSST memory index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiModifyChannelEntry
UINT32 Oct6100ApiModifyChannelEntry(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN tPOCT6100_CHANNEL_MODIFY			f_pChannelModify,
				IN tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN UINT16							f_usChanIndex,
				IN UINT16							f_usNewPhasingTsstIndex,
				IN UINT8							f_fSinSoutCodecActive,
				IN UINT8							f_fRinRoutCodecActive,
				IN UINT16							f_usNewRinTsstIndex,
				IN UINT16							f_usNewSinTsstIndex,
				IN UINT16							f_usNewRoutTsstIndex,
				IN UINT16							f_usNewSoutTsstIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_API_CHANNEL_CODEC	pApiCodecConf;
	tPOCT6100_API_CHANNEL_TDM	pApiTdmConf;
	tPOCT6100_API_CHANNEL_VQE	pApiVqeConf;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	/* Obtain local pointer to the configuration structures of the tPOCT6100_API_CHANNEL structure. */
	pApiCodecConf = &pChanEntry->CodecConfig;
	pApiTdmConf   = &pChanEntry->TdmConfig;
	pApiVqeConf   = &pChanEntry->VqeConfig;

	/*=======================================================================*/
	/* Copy the channel's general configuration. */

	pChanEntry->ulUserChanId = f_pChannelOpen->ulUserChanId;
	pChanEntry->byEchoOperationMode = (UINT8)( f_pChannelOpen->ulEchoOperationMode & 0xFF );
	pChanEntry->fEnableToneDisabler = (UINT8)( f_pChannelOpen->fEnableToneDisabler & 0xFF );

	/* Save the codec state.*/
	pChanEntry->fSinSoutCodecActive = (UINT8)( f_fSinSoutCodecActive & 0xFF );
	pChanEntry->fRinRoutCodecActive = (UINT8)( f_fRinRoutCodecActive & 0xFF );

	/*=======================================================================*/
	/* Copy the channel's TDM configuration of all the modified fields. */

	if ( f_pChannelModify->fTdmConfigModified == TRUE )
	{
		pApiTdmConf->byRinPcmLaw = (UINT8)( f_pChannelOpen->TdmConfig.ulRinPcmLaw & 0xFF );
		pApiTdmConf->bySinPcmLaw = (UINT8)( f_pChannelOpen->TdmConfig.ulSinPcmLaw & 0xFF );
		pApiTdmConf->byRoutPcmLaw = (UINT8)( f_pChannelOpen->TdmConfig.ulRoutPcmLaw & 0xFF );
		pApiTdmConf->bySoutPcmLaw = (UINT8)( f_pChannelOpen->TdmConfig.ulSoutPcmLaw & 0xFF );

		pApiTdmConf->byRinNumTssts = (UINT8)( f_pChannelOpen->TdmConfig.ulRinNumTssts & 0xFF );
		pApiTdmConf->bySinNumTssts = (UINT8)( f_pChannelOpen->TdmConfig.ulSinNumTssts & 0xFF );
		pApiTdmConf->byRoutNumTssts = (UINT8)( f_pChannelOpen->TdmConfig.ulRoutNumTssts & 0xFF );
		pApiTdmConf->bySoutNumTssts = (UINT8)( f_pChannelOpen->TdmConfig.ulSoutNumTssts & 0xFF );

		if ( f_pChannelModify->TdmConfig.ulRinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) 
		{
			if ( f_usNewRinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				pApiTdmConf->usRinStream	= (UINT16)( f_pChannelOpen->TdmConfig.ulRinStream & 0xFFFF );
				pApiTdmConf->usRinTimeslot	= (UINT16)( f_pChannelOpen->TdmConfig.ulRinTimeslot & 0xFFFF );
				pChanEntry->usRinTsstIndex	= f_usNewRinTsstIndex;
			}
			else /* f_ulNewRinTsstIndex != cOCT6100_INVALID_INDEX */
			{
				pApiTdmConf->usRinStream	= cOCT6100_UNASSIGNED;
				pApiTdmConf->usRinTimeslot	= cOCT6100_UNASSIGNED;
				pChanEntry->usRinTsstIndex	= cOCT6100_INVALID_INDEX;
			}
		}

		if ( f_pChannelModify->TdmConfig.ulSinTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) 
		{
			if ( f_usNewSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				pApiTdmConf->usSinStream	= (UINT16)( f_pChannelOpen->TdmConfig.ulSinStream & 0xFFFF );
				pApiTdmConf->usSinTimeslot	= (UINT16)( f_pChannelOpen->TdmConfig.ulSinTimeslot & 0xFFFF );
				pChanEntry->usSinTsstIndex	= f_usNewSinTsstIndex;
			}
			else /* f_ulNewSinTsstIndex != cOCT6100_INVALID_INDEX */
			{
				pApiTdmConf->usSinStream	= cOCT6100_UNASSIGNED;
				pApiTdmConf->usSinTimeslot	= cOCT6100_UNASSIGNED;
				pChanEntry->usSinTsstIndex	= cOCT6100_INVALID_INDEX;
			}
		}

		if ( f_pChannelModify->TdmConfig.ulRoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) 
		{
			if ( f_usNewRoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				pApiTdmConf->usRoutStream	= (UINT16)( f_pChannelOpen->TdmConfig.ulRoutStream & 0xFFFF );
				pApiTdmConf->usRoutTimeslot	= (UINT16)( f_pChannelOpen->TdmConfig.ulRoutTimeslot & 0xFFFF );
				pChanEntry->usRoutTsstIndex	= f_usNewRoutTsstIndex;
			}
			else /* f_ulNewRoutTsstIndex != cOCT6100_INVALID_INDEX */
			{
				pApiTdmConf->usRoutStream	= cOCT6100_UNASSIGNED;
				pApiTdmConf->usRoutTimeslot	= cOCT6100_UNASSIGNED;
				pChanEntry->usRoutTsstIndex	= cOCT6100_INVALID_INDEX;
			}
		}

		if ( f_pChannelModify->TdmConfig.ulSoutTimeslot != cOCT6100_KEEP_PREVIOUS_SETTING ) 
		{
			if ( f_usNewSoutTsstIndex != cOCT6100_INVALID_INDEX )
			{
				pApiTdmConf->usSoutStream	= (UINT16)( f_pChannelOpen->TdmConfig.ulSoutStream & 0xFFFF );
				pApiTdmConf->usSoutTimeslot	= (UINT16)( f_pChannelOpen->TdmConfig.ulSoutTimeslot & 0xFFFF );
				pChanEntry->usSoutTsstIndex	= f_usNewSoutTsstIndex;
			}
			else /* f_ulNewSoutTsstIndex != cOCT6100_INVALID_INDEX */
			{
				pApiTdmConf->usSoutStream	= cOCT6100_UNASSIGNED;
				pApiTdmConf->usSoutTimeslot	= cOCT6100_UNASSIGNED;
				pChanEntry->usSoutTsstIndex	= cOCT6100_INVALID_INDEX;
			}
		}
	}
	
	/*=======================================================================*/
	/* Copy the channel's VQE configuration of all the modified fields. */

	if ( f_pChannelModify->fVqeConfigModified == TRUE )
	{
		pApiVqeConf->fEnableNlp									= (UINT8)( f_pChannelOpen->VqeConfig.fEnableNlp & 0xFF );
		pApiVqeConf->byComfortNoiseMode							= (UINT8)( f_pChannelOpen->VqeConfig.ulComfortNoiseMode & 0xFF );
		pApiVqeConf->fSinDcOffsetRemoval						= (UINT8)( f_pChannelOpen->VqeConfig.fSinDcOffsetRemoval & 0xFF );
		pApiVqeConf->fRinDcOffsetRemoval						= (UINT8)( f_pChannelOpen->VqeConfig.fRinDcOffsetRemoval & 0xFF );
		pApiVqeConf->fRinLevelControl							= (UINT8)( f_pChannelOpen->VqeConfig.fRinLevelControl & 0xFF );
		pApiVqeConf->fSoutLevelControl							= (UINT8)( f_pChannelOpen->VqeConfig.fSoutLevelControl & 0xFF );
		pApiVqeConf->fRinAutomaticLevelControl					= (UINT8)( f_pChannelOpen->VqeConfig.fRinAutomaticLevelControl & 0xFF );
		pApiVqeConf->fSoutAutomaticLevelControl					= (UINT8)( f_pChannelOpen->VqeConfig.fSoutAutomaticLevelControl & 0xFF );
		pApiVqeConf->fRinHighLevelCompensation					= (UINT8)( f_pChannelOpen->VqeConfig.fRinHighLevelCompensation & 0xFF );

		pApiVqeConf->fSoutAdaptiveNoiseReduction				= (UINT8)( f_pChannelOpen->VqeConfig.fSoutAdaptiveNoiseReduction & 0xFF );
		pApiVqeConf->fSoutNoiseBleaching						= (UINT8)( f_pChannelOpen->VqeConfig.fSoutNoiseBleaching & 0xFF );
		pApiVqeConf->fSoutConferencingNoiseReduction			= (UINT8)( f_pChannelOpen->VqeConfig.fSoutConferencingNoiseReduction & 0xFF );
		pApiVqeConf->chRinLevelControlGainDb					= (OCT_INT8)( f_pChannelOpen->VqeConfig.lRinLevelControlGainDb & 0xFF );
		pApiVqeConf->chSoutLevelControlGainDb					= (OCT_INT8)( f_pChannelOpen->VqeConfig.lSoutLevelControlGainDb & 0xFF );
		pApiVqeConf->chRinAutomaticLevelControlTargetDb			= (OCT_INT8)( f_pChannelOpen->VqeConfig.lRinAutomaticLevelControlTargetDb & 0xFF );
		pApiVqeConf->chSoutAutomaticLevelControlTargetDb		= (OCT_INT8)( f_pChannelOpen->VqeConfig.lSoutAutomaticLevelControlTargetDb & 0xFF );
		pApiVqeConf->chRinHighLevelCompensationThresholdDb		= (OCT_INT8)( f_pChannelOpen->VqeConfig.lRinHighLevelCompensationThresholdDb & 0xFF );
		pApiVqeConf->fEnableTailDisplacement					= (UINT8)( f_pChannelOpen->VqeConfig.fEnableTailDisplacement & 0xFF );
		pApiVqeConf->usTailDisplacement							= (UINT16)( f_pChannelOpen->VqeConfig.ulTailDisplacement & 0xFFFF );
		pApiVqeConf->usTailLength								= (UINT16)( f_pChannelOpen->VqeConfig.ulTailLength & 0xFFFF );
		pApiVqeConf->fAcousticEcho								= (UINT8)( f_pChannelOpen->VqeConfig.fAcousticEcho & 0xFF );
		pApiVqeConf->fDtmfToneRemoval							= (UINT8)( f_pChannelOpen->VqeConfig.fDtmfToneRemoval & 0xFF );

		pApiVqeConf->chDefaultErlDb								= (OCT_INT8)( f_pChannelOpen->VqeConfig.lDefaultErlDb & 0xFF );
		pApiVqeConf->chAecDefaultErlDb							= (OCT_INT8)( f_pChannelOpen->VqeConfig.lAecDefaultErlDb & 0xFF );
		pApiVqeConf->usAecTailLength							= (UINT16)( f_pChannelOpen->VqeConfig.ulAecTailLength & 0xFFFF );
		pApiVqeConf->chAnrSnrEnhancementDb						= (OCT_INT8)( f_pChannelOpen->VqeConfig.lAnrSnrEnhancementDb & 0xFF );
		pApiVqeConf->byAnrVoiceNoiseSegregation					= (UINT8)( f_pChannelOpen->VqeConfig.ulAnrVoiceNoiseSegregation & 0xFF );
		pApiVqeConf->usToneDisablerVqeActivationDelay			= (UINT16)( f_pChannelOpen->VqeConfig.ulToneDisablerVqeActivationDelay & 0xFFFF );
		pApiVqeConf->byNonLinearityBehaviorA					= (UINT8)( f_pChannelOpen->VqeConfig.ulNonLinearityBehaviorA & 0xFF );
		pApiVqeConf->byNonLinearityBehaviorB					= (UINT8)( f_pChannelOpen->VqeConfig.ulNonLinearityBehaviorB & 0xFF );
		pApiVqeConf->byDoubleTalkBehavior						= (UINT8)( f_pChannelOpen->VqeConfig.ulDoubleTalkBehavior & 0xFF );
		pApiVqeConf->bySoutAutomaticListenerEnhancementGainDb	= (UINT8)( f_pChannelOpen->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb & 0xFF );
		pApiVqeConf->bySoutNaturalListenerEnhancementGainDb		= (UINT8)( f_pChannelOpen->VqeConfig.ulSoutNaturalListenerEnhancementGainDb & 0xFF );
		pApiVqeConf->fSoutNaturalListenerEnhancement			= (UINT8)( f_pChannelOpen->VqeConfig.fSoutNaturalListenerEnhancement & 0xFF );
		pApiVqeConf->fRoutNoiseReduction						= (UINT8)( f_pChannelOpen->VqeConfig.fRoutNoiseReduction & 0xFF );
		pApiVqeConf->chRoutNoiseReductionLevelGainDb			= (OCT_INT8)( f_pChannelOpen->VqeConfig.lRoutNoiseReductionLevelGainDb & 0xFF );
		pApiVqeConf->fEnableMusicProtection						= (UINT8)( f_pChannelOpen->VqeConfig.fEnableMusicProtection & 0xFF );
		pApiVqeConf->fIdleCodeDetection							= (UINT8)( f_pChannelOpen->VqeConfig.fIdleCodeDetection & 0xFF );
	}

	/*=======================================================================*/
	/* Copy the channel's CODEC configuration of all the modified fields. */
	if ( f_pChannelModify->fCodecConfigModified == TRUE )
	{
		pApiCodecConf->byAdpcmNibblePosition		= (UINT8)( f_pChannelOpen->CodecConfig.ulAdpcmNibblePosition & 0xFF );
		pApiCodecConf->byEncoderPort				= (UINT8)( f_pChannelOpen->CodecConfig.ulEncoderPort & 0xFF );
		pApiCodecConf->byEncodingRate				= (UINT8)( f_pChannelOpen->CodecConfig.ulEncodingRate & 0xFF );
		pApiCodecConf->byDecoderPort				= (UINT8)( f_pChannelOpen->CodecConfig.ulDecoderPort & 0xFF );
		pApiCodecConf->byDecodingRate				= (UINT8)( f_pChannelOpen->CodecConfig.ulDecodingRate & 0xFF );
		pApiCodecConf->fEnableSilenceSuppression	= (UINT8)( f_pChannelOpen->CodecConfig.fEnableSilenceSuppression & 0xFF );
		pApiCodecConf->byPhase						= (UINT8)( f_pChannelOpen->CodecConfig.ulPhase & 0xFF );
		pApiCodecConf->byPhasingType				= (UINT8)( f_pChannelOpen->CodecConfig.ulPhasingType & 0xFF );

		/* Update the API phasing TSST structure */
		if ( f_usNewPhasingTsstIndex != cOCT6100_INVALID_INDEX )
		{
			tPOCT6100_API_PHASING_TSST	pPhasingTsst;

			/* Release the previous phasing TSST if the channel was already bound to one.*/
			if ( pChanEntry->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
			{
				mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pPhasingTsst, pChanEntry->usPhasingTsstIndex );

				pPhasingTsst->usDependencyCnt--;
			}
			
			mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pPhasingTsst, f_usNewPhasingTsstIndex );

			pPhasingTsst->usDependencyCnt++;
			pChanEntry->usPhasingTsstIndex = f_usNewPhasingTsstIndex;

		}
	}



	return cOCT6100_ERR_OK;
}
#endif















/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelBroadcastTsstAddSer

Description:    Assign a TSST to one of the port of an echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstAdd		Pointer to TSST assign structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelBroadcastTsstAddSer
UINT32 Oct6100ChannelBroadcastTsstAddSer(
				IN tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_BROADCAST_TSST_ADD		f_pChannelTsstAdd )
{
	UINT16	usChanIndex;
	UINT16	usNewTsstIndex;
	UINT16	usNewTsstEntry;
	UINT32	ulResult;

	ulResult = Oct6100ApiCheckChanTsstAddParams( f_pApiInstance, f_pChannelTsstAdd, &usChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulResult = Oct6100ApiReserveTsstAddResources( f_pApiInstance, f_pChannelTsstAdd, usChanIndex, &usNewTsstIndex, &usNewTsstEntry );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulResult = Oct6100ApiWriteTsstAddStructs( f_pApiInstance, f_pChannelTsstAdd, usChanIndex, usNewTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulResult = Oct6100ApiUpdateTsstAddChanEntry( f_pApiInstance, f_pChannelTsstAdd, usChanIndex, usNewTsstIndex, usNewTsstEntry );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChanTsstAddParams

Description:    Verify the validity of the tPOCT6100_CHANNEL_BROADCAST_TSST_ADD
				structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstAdd		Pointer to echo cancellation channel open configuration structure.
f_pusChanIndex			Pointer to a structure used to store the multiple resources indexes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChanTsstAddParams
UINT32 Oct6100ApiCheckChanTsstAddParams(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd, 
				OUT PUINT16									f_pusChanIndex )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	UINT32	ulResult;
	UINT32	ulNumTssts = 1;
	UINT32	ulEntryOpenCnt;

	/* Check the provided handle. */
	if ( (f_pChannelTsstAdd->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelTsstAdd->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, *f_pusChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelTsstAdd->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/

	/* validate the port parameter.*/
	if ( f_pChannelTsstAdd->ulPort != cOCT6100_CHANNEL_PORT_ROUT &&
		 f_pChannelTsstAdd->ulPort != cOCT6100_CHANNEL_PORT_SOUT )
		return cOCT6100_ERR_CHANNEL_TSST_ADD_PORT;

	/* Get the required number of TSST based on the port.*/
	switch( f_pChannelTsstAdd->ulPort )
	{
	case cOCT6100_CHANNEL_PORT_ROUT:
		ulNumTssts = pChanEntry->TdmConfig.byRoutNumTssts;
		break;
	case cOCT6100_CHANNEL_PORT_SOUT:
		ulNumTssts = pChanEntry->TdmConfig.bySoutNumTssts;
		break;
	default:
		return cOCT6100_ERR_FATAL_B;
	}

	/* Check the validity of the timeslot and stream. */
	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   ulNumTssts,
									   f_pChannelTsstAdd->ulTimeslot, 
									   f_pChannelTsstAdd->ulStream,
									   cOCT6100_OUTPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_CHANNEL_TSST_ADD_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_CHANNEL_TSST_ADD_STREAM;
		}
		else
		{
			return ulResult;
		}
	}

	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveTsstAddResources

Description:    Reserve the entry for the new broadcast TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstAdd		Pointer to echo cancellation channel open configuration structure.
f_usChanIndex			Channel index within the API's channel list.
f_pusNewTsstIndex		Pointer to the new TSST index within the API's TSST memory.
f_pusNewTsstEntry		Pointer to the new TSST entry within the API's TSST list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveTsstAddResources
UINT32 Oct6100ApiReserveTsstAddResources(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd, 
				IN	UINT16									f_usChanIndex,
				OUT	PUINT16									f_pusNewTsstIndex,
				OUT	PUINT16									f_pusNewTsstEntry )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	UINT32	ulResult;
	UINT32	ulNumTssts = 1;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_usChanIndex );

	switch( f_pChannelTsstAdd->ulPort )
	{
	case cOCT6100_CHANNEL_PORT_ROUT:
		ulNumTssts = pChanEntry->TdmConfig.byRoutNumTssts;
		break;
	case cOCT6100_CHANNEL_PORT_SOUT:
		ulNumTssts = pChanEntry->TdmConfig.bySoutNumTssts;
		break;
	default:
		return cOCT6100_ERR_FATAL_C;
	}

	/* Reserve the new entry.*/
	ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
									  f_pChannelTsstAdd->ulTimeslot, 
									  f_pChannelTsstAdd->ulStream, 
									  ulNumTssts, 
	  								  cOCT6100_OUTPUT_TSST,
									  f_pusNewTsstIndex, 
									  f_pusNewTsstEntry );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteTsstAddStructs

Description:    Configure the TSST control memory for the new TSST entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstAdd		Pointer to echo cancellation channel open configuration structure.
f_usChanIndex			Channel index.
f_usNewTsstIndex		Tsst index in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteTsstAddStructs
UINT32 Oct6100ApiWriteTsstAddStructs(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd, 
				IN	UINT16									f_usChanIndex,
				IN	UINT16									f_usNewTsstIndex )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT16	usTsiMemIndex;
	UINT32	ulNumTssts = 1;

	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_usChanIndex );

	switch( f_pChannelTsstAdd->ulPort )
	{
	case cOCT6100_CHANNEL_PORT_ROUT:
		usTsiMemIndex = pChanEntry->usRinRoutTsiMemIndex;
		ulNumTssts = pChanEntry->TdmConfig.byRoutNumTssts;
		break;
	case cOCT6100_CHANNEL_PORT_SOUT:
		usTsiMemIndex = pChanEntry->usSinSoutTsiMemIndex;
		ulNumTssts = pChanEntry->TdmConfig.bySoutNumTssts;
		break;
	default:
		return cOCT6100_ERR_FATAL_D;
	}


	/* Write the new entry now.*/
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (f_usNewTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_TSST_CONTROL_MEM_OUTPUT_TSST;
	WriteParams.usWriteData |= (UINT16)( pChanEntry->CodecConfig.byAdpcmNibblePosition << cOCT6100_TSST_CONTROL_MEM_NIBBLE_POS_OFFSET );
	WriteParams.usWriteData |= (UINT16)( (ulNumTssts - 1) << cOCT6100_TSST_CONTROL_MEM_TSST_NUM_OFFSET );
	WriteParams.usWriteData |= (UINT16)( usTsiMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK );
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}	
#endif




/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateTsstAddChanEntry

Description:    Update the associated channel API entry to add the new broacast TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstAdd		Pointer to echo cancellation channel open configuration structure.
f_usChanIndex			Channel index.
f_usNewTsstIndex		TSST index within the TSST control memory.
f_usNewTsstEntry		TSST entry within the API TSST list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateTsstAddChanEntry
UINT32 Oct6100ApiUpdateTsstAddChanEntry(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_ADD	f_pChannelTsstAdd, 
				IN	UINT16									f_usChanIndex,
				IN	UINT16									f_usNewTsstIndex,
				IN	UINT16									f_usNewTsstEntry )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_API_TSST_ENTRY	pTsstEntry;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_usChanIndex );
	mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, f_usNewTsstEntry );

	/* Update the channel entry.*/
	if ( f_pChannelTsstAdd->ulPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		/* Add the new TSST entry to the broadcast list.*/
		pTsstEntry->usNextEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry;
		pTsstEntry->usTsstMemoryIndex = (UINT16)f_usNewTsstIndex;
		pTsstEntry->usTsstValue = (UINT16)( (f_pChannelTsstAdd->ulTimeslot << 5) | f_pChannelTsstAdd->ulStream );

		/* Modify the first entry pointer.*/
		pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry = f_usNewTsstEntry;

		/* Increment the number of broadcast TSST. */
		pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry++;
		
	}
	else /* f_pChannelTsstAdd->ulPort == cOCT6100_CHANNEL_PORT_SOUT  */
	{
		/* Add the new TSST entry to the broadcast list.*/
		pTsstEntry->usNextEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry;
		pTsstEntry->usTsstMemoryIndex = (UINT16)f_usNewTsstIndex;
		pTsstEntry->usTsstValue = (UINT16)( (f_pChannelTsstAdd->ulTimeslot << 5) | f_pChannelTsstAdd->ulStream );

		/* Modify the first entry pointer.*/
		pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry = f_usNewTsstEntry;

		/* Increment the number of broadcast TSST. */
		pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry++;
	}

	return cOCT6100_ERR_OK;
}	
#endif








/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelBroadcastTsstRemoveSer

Description:    Removes a broadcast TSST from one of the output port of an 
				echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstRemove	Pointer to TSST remove structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelBroadcastTsstRemoveSer
UINT32 Oct6100ChannelBroadcastTsstRemoveSer(
				IN tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove)
{
	UINT16	usChanIndex;
	UINT16	usTsstIndex;
	UINT16	usTsstEntry;
	UINT16	usPrevTsstEntry;
	UINT32	ulResult;

	ulResult = Oct6100ApiAssertChanTsstRemoveParams( f_pApiInstance, f_pChannelTsstRemove, &usChanIndex, &usTsstIndex, &usTsstEntry, &usPrevTsstEntry );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulResult = Oct6100ApiInvalidateTsstRemoveStructs( f_pApiInstance, usChanIndex, usTsstIndex, f_pChannelTsstRemove->ulPort, f_pChannelTsstRemove->fRemoveAll );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	ulResult = Oct6100ApiReleaseTsstRemoveResources( f_pApiInstance, f_pChannelTsstRemove, usChanIndex, usTsstIndex, usTsstEntry, usPrevTsstEntry );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertChanTsstRemoveParams

Description:    Verify the validity of the tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE
				structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstRemove	Pointer to echo cancellation channel open configuration structure.
f_pulChanIndex			Pointer to a channel index.
f_pulNewTsstIndex		Pointer to a TSST index within the TSST control memory.
f_pulNewTsstEntry		Pointer to a TSST entry within the API TSST list.
f_pulPrevTsstEntry		Pointer to the previous TSST entry.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertChanTsstRemoveParams
UINT32 Oct6100ApiAssertChanTsstRemoveParams(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove, 
				OUT PUINT16									f_pusChanIndex,
				OUT	PUINT16									f_pusTsstIndex,
				OUT	PUINT16									f_pusTsstEntry,
				OUT	PUINT16									f_pusPrevTsstEntry )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_API_TSST_ENTRY	pTsstEntry;
	UINT32	ulResult;
	UINT32	ulNumTssts = 1;
	UINT32	ulEntryOpenCnt;
	UINT16	usCurrentEntry;
	UINT16	usTsstValue;
	UINT16	usNumEntry;
	
	/* Check the provided handle. */
	if ( (f_pChannelTsstRemove->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelTsstRemove->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, *f_pusChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelTsstRemove->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/

	/* validate the port parameter.*/
	if ( f_pChannelTsstRemove->ulPort != cOCT6100_CHANNEL_PORT_ROUT &&
		 f_pChannelTsstRemove->ulPort != cOCT6100_CHANNEL_PORT_SOUT )
		return cOCT6100_ERR_CHANNEL_TSST_REMOVE_PORT;

	/* Verify that the requested entry is present in the channel's port broadcast TSST.*/
	if ( f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		usCurrentEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry;
		usNumEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry;
	}
	else /* f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		usCurrentEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry;
		usNumEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry;
	}

	/* Verify if at least one TSST is present on the channel port.*/
	if ( usNumEntry == 0 )
		return cOCT6100_ERR_CHANNEL_TSST_REMOVE_NO_BROADCAST_TSST;

	/* Get the required number of TSST based on the port.*/
	switch( f_pChannelTsstRemove->ulPort )
	{
	case cOCT6100_CHANNEL_PORT_ROUT:
		ulNumTssts = pChanEntry->TdmConfig.byRoutNumTssts;
		break;
	case cOCT6100_CHANNEL_PORT_SOUT:
		ulNumTssts = pChanEntry->TdmConfig.bySoutNumTssts;
		break;
	default:
		return cOCT6100_ERR_FATAL_E;
	}

	/* Initialize the TSST entry to invalid.*/
	*f_pusTsstEntry		= cOCT6100_INVALID_INDEX;
	*f_pusPrevTsstEntry	= cOCT6100_INVALID_INDEX;
	*f_pusTsstIndex		= cOCT6100_INVALID_INDEX;

	if ( f_pChannelTsstRemove->fRemoveAll != TRUE )
	{
		/* Check the validity of the timeslot and Stream.*/
		ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
										   ulNumTssts,
										   f_pChannelTsstRemove->ulTimeslot, 
										   f_pChannelTsstRemove->ulStream,
										   cOCT6100_OUTPUT_TSST );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
			{
				return cOCT6100_ERR_CHANNEL_TSST_REMOVE_TIMESLOT;
			}
			else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
			{
				return cOCT6100_ERR_CHANNEL_TSST_REMOVE_STREAM;
			}
			else
			{
				return ulResult;
			}
		}
	
		/* Set the TSST value based on the timeslot and stream value.*/
		usTsstValue = (UINT16)( (f_pChannelTsstRemove->ulTimeslot << 5) | f_pChannelTsstRemove->ulStream );

		while( usCurrentEntry != cOCT6100_INVALID_INDEX )
		{
			mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, usCurrentEntry );

			if ( usTsstValue == pTsstEntry->usTsstValue )
			{
				/* A match was found.*/
				*f_pusTsstEntry = usCurrentEntry;
				*f_pusTsstIndex = pTsstEntry->usTsstMemoryIndex;
				break;
			}

			/* Move on to the next entry.*/
			*f_pusPrevTsstEntry = usCurrentEntry;
			usCurrentEntry = pTsstEntry->usNextEntry;
		}

		if ( *f_pusTsstEntry == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CHANNEL_TSST_REMOVE_INVALID_TSST;
	}
	
	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateTsstRemoveStructs

Description:    Invalidate the entry of the broadcast TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usChanIndex			Channel index.
f_usTsstIndex			TSST index within the TSST control memory.
f_ulPort				Channel port where the TSST are removed from. (only used if remove all == TRUE)
f_fRemoveAll			Remove all flag.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateTsstRemoveStructs
UINT32 Oct6100ApiInvalidateTsstRemoveStructs(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN	UINT16									f_usChanIndex,
				IN	UINT16									f_usTsstIndex,
				IN	UINT32									f_ulPort,
				IN	BOOL									f_fRemoveAll )
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32	ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	if ( f_fRemoveAll == FALSE )
	{
		/* Deactivate the entry now.*/
		WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (f_usTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData  = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}
	else /* f_fRemoveAll == TRUE */
	{
		tPOCT6100_API_CHANNEL		pChanEntry;
		tPOCT6100_API_TSST_ENTRY	pTsstEntry;
		UINT16						usTsstEntry;

		/*=======================================================================*/
		/* Get a pointer to the channel's list entry. */

		mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_usChanIndex );

		/* Clear all entry associated to the selected port.*/
		if ( f_ulPort == cOCT6100_CHANNEL_PORT_ROUT )
			usTsstEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry;
		else
			usTsstEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry;

		do
		{
			mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, usTsstEntry );

			/* Deactivate the entry now.*/
			WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( ( pTsstEntry->usTsstMemoryIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData  = 0x0000;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;

			usTsstEntry = pTsstEntry->usNextEntry;
		
		} while ( usTsstEntry != cOCT6100_INVALID_INDEX );
	}

	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseTsstRemoveResources

Description:    Release all API resources associated to the Removed TSST and 
				update the channel entry accordingly.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelTsstRemove	Pointer to echo cancellation channel open configuration structure.
f_usChanIndex			Channel index.
f_usTsstIndex			TSST index within the TSST control memory.
f_usTsstEntry			TSST entry within the API's TSST list.
f_usPrevTsstEntry		Previous TSST entry within the API's TSST list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseTsstRemoveResources
UINT32 Oct6100ApiReleaseTsstRemoveResources(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_BROADCAST_TSST_REMOVE	f_pChannelTsstRemove, 
				IN	UINT16									f_usChanIndex,
				IN	UINT16									f_usTsstIndex,
				IN	UINT16									f_usTsstEntry,
				IN	UINT16									f_usPrevTsstEntry )
{
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_API_TSST_ENTRY	pTsstEntry;
	tPOCT6100_API_TSST_ENTRY	pPrevTsstEntry;
	UINT16	usCurrentEntry;
	UINT32	ulResult;
	UINT32	ulTimeslot;
	UINT32	ulStream;
	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, f_usChanIndex );

	if ( f_pChannelTsstRemove->fRemoveAll == FALSE )
	{
		mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, f_usTsstEntry );

		/* Update the channel entry.*/
		if ( f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_ROUT )
		{
			/* Check if the entry was the first in the list.*/
			if ( f_usPrevTsstEntry == cOCT6100_INVALID_INDEX )
			{
				pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry = pTsstEntry->usNextEntry;
			}
			else /* f_ulPrevTsstEntry != cOCT6100_INVALID_INDEX */
			{
				/* Get a pointer to the previous entry.*/
				mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPrevTsstEntry, f_usPrevTsstEntry );
				pPrevTsstEntry->usNextEntry = pTsstEntry->usNextEntry;
			}

			/* Decrement the number of entry.*/
			pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry--;
		}
		else /* f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_SOUT */
		{
			/* Check if the entry was the first in the list.*/
			if ( f_usPrevTsstEntry == cOCT6100_INVALID_INDEX )
			{
				pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry = pTsstEntry->usNextEntry;
			}
			else /* f_ulPrevTsstEntry != cOCT6100_INVALID_INDEX */
			{
				/* Get a pointer to the previous entry.*/
				mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPrevTsstEntry, f_usPrevTsstEntry );
				pPrevTsstEntry->usNextEntry = pTsstEntry->usNextEntry;
			}

			/* Decrement the number of entry.*/
			pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry--;
		}

		ulTimeslot = pTsstEntry->usTsstValue >> 5;
		ulStream = pTsstEntry->usTsstValue & 0x1F;

		/* Release the  entry.*/
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  ulTimeslot,
										  ulStream,
 									      cOCT6100_NUMBER_TSSTS_1,
										  cOCT6100_OUTPUT_TSST,
										  f_usTsstEntry );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}
	else /* f_pChannelTsstRemove->fRemoveAll == TRUE */
	{

		/* Update the channel entry.*/
		if ( f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_ROUT )
			usCurrentEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry;
		else
			usCurrentEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry;

		do
		{
			mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, usCurrentEntry );

			ulTimeslot = pTsstEntry->usTsstValue >> 5;
			ulStream = pTsstEntry->usTsstValue & 0x1F;

			/* Release the  entry.*/
			ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  ulTimeslot,
											  ulStream,
 											  cOCT6100_NUMBER_TSSTS_1,
											  cOCT6100_OUTPUT_TSST,
											  usCurrentEntry );			/* Release the  entry.*/
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;

			usCurrentEntry = pTsstEntry->usNextEntry;

			/* Clear the previous node.*/
			pTsstEntry->usTsstMemoryIndex = 0xFFFF;
			pTsstEntry->usTsstValue = 0xFFFF;
			pTsstEntry->usNextEntry = cOCT6100_INVALID_INDEX;

		} while ( usCurrentEntry != cOCT6100_INVALID_INDEX );
		
		/* Reset the channel status.*/
		if ( f_pChannelTsstRemove->ulPort == cOCT6100_CHANNEL_PORT_ROUT )
		{
			pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry = cOCT6100_INVALID_INDEX;
			pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry = 0;
		}
		else
		{
			pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry = cOCT6100_INVALID_INDEX;
			pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry = 0;
		}
	}
	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiChannelGetStatsSer

Description:    Serialized function that returns all the stats of the specified
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelStats			Pointer to a channel stats structure.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiChannelGetStatsSer
UINT32 Oct6100ApiChannelGetStatsSer(
				IN  tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN  tPOCT6100_CHANNEL_STATS					f_pChannelStats )
{
	tOCT6100_READ_PARAMS		ReadParams;
	tOCT6100_READ_BURST_PARAMS	BurstParams;
	tPOCT6100_API_CHANNEL		pChanEntry;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_TSST_ENTRY	pTsstEntry;
	UINT32	ulEntryOpenCnt;
	UINT16	usCurrentEntry;
	UINT16	usTsstCount;
	UINT32	ulBaseAddress;
	UINT32	ulFeatureBytesOffset;
	UINT32	ulFeatureBitOffset;
	UINT32	ulFeatureFieldLength;
	UINT32	ulTempData;
	UINT32	ulMask;
	UINT16	usChanIndex;
	UINT16	ausReadData[ 32 ];

	BYTE	byRinEnergyRaw;
	BYTE	bySinEnergyRaw;
	BYTE	bySoutEnergyRaw;
	INT32	lSoutEnergyIndB;
	BYTE	byCnEnergyRaw;
	UINT16	usEchoDelayInFrames;
	UINT16	usErlRaw;

	UINT32	ulResult;
	UINT16	usReadData;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	BurstParams.pusReadData = ausReadData;
		
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Check the reset stats flag.*/
	if ( f_pChannelStats->fResetStats != TRUE && f_pChannelStats->fResetStats != FALSE )
		return cOCT6100_ERR_CHANNEL_STATS_RESET;

	/* Check the provided handle. */
	if ( cOCT6100_HNDL_TAG_CHANNEL != (f_pChannelStats->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	usChanIndex = (UINT16)( f_pChannelStats->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( usChanIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pChanEntry, usChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelStats->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	
	/* Check the value of the max broadcast tsst.*/
	if ( f_pChannelStats->TdmConfig.ulMaxBroadcastTssts > cOCT6100_MAX_TSSTS )
		return cOCT6100_ERR_CHANNEL_GET_STATS_MAX_BROADCAST_TSST;

	if ( f_pChannelStats->TdmConfig.ulMaxBroadcastTssts != 0 )
	{
		/* Check if memory was allocated by the user for the stream and timeslot values.*/
		if ( f_pChannelStats->TdmConfig.pulRoutBroadcastTimeslot == NULL )
			return cOCT6100_ERR_CHANNEL_ROUT_BROADCAST_TIMESLOT;

		if ( f_pChannelStats->TdmConfig.pulRoutBroadcastStream == NULL )
			return cOCT6100_ERR_CHANNEL_ROUT_BROADCAST_STREAM;

		if ( f_pChannelStats->TdmConfig.pulSoutBroadcastTimeslot == NULL )
			return cOCT6100_ERR_CHANNEL_SOUT_BROADCAST_TIMESLOT;

		if ( f_pChannelStats->TdmConfig.pulSoutBroadcastStream == NULL )
			return cOCT6100_ERR_CHANNEL_SOUT_BROADCAST_STREAM;
	}

	/* Copy the general configuration.*/
	f_pChannelStats->ulUserChanId = pChanEntry->ulUserChanId;
	f_pChannelStats->ulEchoOperationMode = pChanEntry->byEchoOperationMode;
	f_pChannelStats->fEnableToneDisabler = pChanEntry->fEnableToneDisabler;
	f_pChannelStats->ulMutePortsMask = pChanEntry->usMutedPorts;
	f_pChannelStats->fEnableExtToneDetection = pChanEntry->fEnableExtToneDetection;


	
	/* Copy the TDM configuration.*/
	f_pChannelStats->TdmConfig.ulNumRoutBroadcastTssts = pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry;
	f_pChannelStats->TdmConfig.ulNumSoutBroadcastTssts = pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry;

	f_pChannelStats->TdmConfig.ulSinNumTssts = pChanEntry->TdmConfig.bySinNumTssts;
	f_pChannelStats->TdmConfig.ulSinTimeslot = pChanEntry->TdmConfig.usSinTimeslot;
	f_pChannelStats->TdmConfig.ulSinStream = pChanEntry->TdmConfig.usSinStream;
	f_pChannelStats->TdmConfig.ulSinPcmLaw = pChanEntry->TdmConfig.bySinPcmLaw;

	f_pChannelStats->TdmConfig.ulSoutNumTssts = pChanEntry->TdmConfig.bySoutNumTssts;
	f_pChannelStats->TdmConfig.ulSoutTimeslot = pChanEntry->TdmConfig.usSoutTimeslot;
	f_pChannelStats->TdmConfig.ulSoutStream = pChanEntry->TdmConfig.usSoutStream;
	f_pChannelStats->TdmConfig.ulSoutPcmLaw = pChanEntry->TdmConfig.bySoutPcmLaw;

	/* Copy the SOUT Broadcast TSST into the Stream and timeslot array.*/
	usCurrentEntry = pChanEntry->TdmConfig.usSoutBrdcastTsstFirstEntry;
	for( usTsstCount = 0; (usTsstCount < pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry) && (usTsstCount < f_pChannelStats->TdmConfig.ulMaxBroadcastTssts); usTsstCount++ )
	{
		if ( usCurrentEntry == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_FATAL_F;

		mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, usCurrentEntry );

		f_pChannelStats->TdmConfig.pulSoutBroadcastStream[ usTsstCount ] = pTsstEntry->usTsstValue & 0x1F;
		f_pChannelStats->TdmConfig.pulSoutBroadcastStream[ usTsstCount ] = pTsstEntry->usTsstValue >> 5;

		/* Obtain the index of the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;
	}

	/* Check if all Sout Broadcast TSST were returned.*/
	if ( usTsstCount < pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry )
	{
		f_pChannelStats->TdmConfig.fMoreSoutBroadcastTssts = TRUE;
	}
	else /* usTsstCount >= pChanEntry->TdmConfig.usSoutBrdcastTsstNumEntry */
	{
		f_pChannelStats->TdmConfig.fMoreSoutBroadcastTssts = FALSE;
	}
		
	f_pChannelStats->TdmConfig.ulRinNumTssts = pChanEntry->TdmConfig.byRinNumTssts;
	f_pChannelStats->TdmConfig.ulRinTimeslot = pChanEntry->TdmConfig.usRinTimeslot;
	f_pChannelStats->TdmConfig.ulRinStream = pChanEntry->TdmConfig.usRinStream;
	f_pChannelStats->TdmConfig.ulRinPcmLaw = pChanEntry->TdmConfig.byRinPcmLaw;

	f_pChannelStats->TdmConfig.ulRoutNumTssts = pChanEntry->TdmConfig.byRoutNumTssts;
	f_pChannelStats->TdmConfig.ulRoutTimeslot = pChanEntry->TdmConfig.usRoutTimeslot;
	f_pChannelStats->TdmConfig.ulRoutStream = pChanEntry->TdmConfig.usRoutStream;
	f_pChannelStats->TdmConfig.ulRoutPcmLaw = pChanEntry->TdmConfig.byRoutPcmLaw;


	/* Copy the ROUT Broadcast TSST into the Stream and timeslot array.*/
	usCurrentEntry = pChanEntry->TdmConfig.usRoutBrdcastTsstFirstEntry;
	for( usTsstCount = 0; (usTsstCount < pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry) && (usTsstCount < f_pChannelStats->TdmConfig.ulMaxBroadcastTssts); usTsstCount++ )
	{
		if ( usCurrentEntry == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_FATAL_10;

		mOCT6100_GET_TSST_LIST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsstEntry, usCurrentEntry );

		f_pChannelStats->TdmConfig.pulRoutBroadcastStream[ usTsstCount ] = pTsstEntry->usTsstValue & 0x1F;
		f_pChannelStats->TdmConfig.pulRoutBroadcastStream[ usTsstCount ] = pTsstEntry->usTsstValue >> 5;

		/* Obtain the index of the next entry.*/
		usCurrentEntry = pTsstEntry->usNextEntry;
	}

	/* Check if all Rout Broadcast TSST were returned.*/
	if ( usTsstCount < pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry )
	{
		f_pChannelStats->TdmConfig.fMoreRoutBroadcastTssts = TRUE;
	}
	else /* usTsstCount >= pChanEntry->TdmConfig.usRoutBrdcastTsstNumEntry */
	{
		f_pChannelStats->TdmConfig.fMoreRoutBroadcastTssts = FALSE;
	}

	/* Copy the VQE configuration.*/
	f_pChannelStats->VqeConfig.fEnableNlp = pChanEntry->VqeConfig.fEnableNlp;
	f_pChannelStats->VqeConfig.ulComfortNoiseMode = pChanEntry->VqeConfig.byComfortNoiseMode;
	f_pChannelStats->VqeConfig.fEnableTailDisplacement = pChanEntry->VqeConfig.fEnableTailDisplacement;
	if ( pChanEntry->VqeConfig.usTailDisplacement != cOCT6100_AUTO_SELECT_TAIL )
		f_pChannelStats->VqeConfig.ulTailDisplacement = pChanEntry->VqeConfig.usTailDisplacement;
	else
		f_pChannelStats->VqeConfig.ulTailDisplacement = f_pApiInstance->pSharedInfo->ChipConfig.usTailDisplacement;

	if ( pChanEntry->VqeConfig.usTailLength != cOCT6100_AUTO_SELECT_TAIL )
		f_pChannelStats->VqeConfig.ulTailLength = pChanEntry->VqeConfig.usTailLength;
	else
		f_pChannelStats->VqeConfig.ulTailLength = f_pApiInstance->pSharedInfo->ImageInfo.usMaxTailLength;
	


	f_pChannelStats->VqeConfig.fSinDcOffsetRemoval = pChanEntry->VqeConfig.fSinDcOffsetRemoval;
	f_pChannelStats->VqeConfig.fRinDcOffsetRemoval = pChanEntry->VqeConfig.fRinDcOffsetRemoval;
	f_pChannelStats->VqeConfig.fRinLevelControl = pChanEntry->VqeConfig.fRinLevelControl;
	f_pChannelStats->VqeConfig.fSoutLevelControl = pChanEntry->VqeConfig.fSoutLevelControl;
	f_pChannelStats->VqeConfig.fRinAutomaticLevelControl = pChanEntry->VqeConfig.fRinAutomaticLevelControl;
	f_pChannelStats->VqeConfig.fSoutAutomaticLevelControl = pChanEntry->VqeConfig.fSoutAutomaticLevelControl;
	f_pChannelStats->VqeConfig.fRinHighLevelCompensation = pChanEntry->VqeConfig.fRinHighLevelCompensation;
	f_pChannelStats->VqeConfig.fSoutAdaptiveNoiseReduction = pChanEntry->VqeConfig.fSoutAdaptiveNoiseReduction;
	f_pChannelStats->VqeConfig.fSoutNoiseBleaching = pChanEntry->VqeConfig.fSoutNoiseBleaching;
	f_pChannelStats->VqeConfig.fSoutConferencingNoiseReduction = pChanEntry->VqeConfig.fSoutConferencingNoiseReduction;
	f_pChannelStats->VqeConfig.lRinLevelControlGainDb	= pChanEntry->VqeConfig.chRinLevelControlGainDb;
	f_pChannelStats->VqeConfig.lSoutLevelControlGainDb	= pChanEntry->VqeConfig.chSoutLevelControlGainDb;
	f_pChannelStats->VqeConfig.lRinAutomaticLevelControlTargetDb	= pChanEntry->VqeConfig.chRinAutomaticLevelControlTargetDb;
	f_pChannelStats->VqeConfig.lSoutAutomaticLevelControlTargetDb	= pChanEntry->VqeConfig.chSoutAutomaticLevelControlTargetDb;
	f_pChannelStats->VqeConfig.lRinHighLevelCompensationThresholdDb	= pChanEntry->VqeConfig.chRinHighLevelCompensationThresholdDb;
	f_pChannelStats->VqeConfig.fAcousticEcho			= pChanEntry->VqeConfig.fAcousticEcho;
	f_pChannelStats->VqeConfig.fDtmfToneRemoval			= pChanEntry->VqeConfig.fDtmfToneRemoval;

	f_pChannelStats->VqeConfig.lDefaultErlDb							= pChanEntry->VqeConfig.chDefaultErlDb;
	f_pChannelStats->VqeConfig.lAecDefaultErlDb							= pChanEntry->VqeConfig.chAecDefaultErlDb;
	f_pChannelStats->VqeConfig.ulAecTailLength							= pChanEntry->VqeConfig.usAecTailLength;
	f_pChannelStats->VqeConfig.lAnrSnrEnhancementDb						= pChanEntry->VqeConfig.chAnrSnrEnhancementDb;
	f_pChannelStats->VqeConfig.ulAnrVoiceNoiseSegregation				= pChanEntry->VqeConfig.byAnrVoiceNoiseSegregation;
	f_pChannelStats->VqeConfig.ulToneDisablerVqeActivationDelay			= pChanEntry->VqeConfig.usToneDisablerVqeActivationDelay;
	f_pChannelStats->VqeConfig.ulNonLinearityBehaviorA					= pChanEntry->VqeConfig.byNonLinearityBehaviorA;
	f_pChannelStats->VqeConfig.ulNonLinearityBehaviorB					= pChanEntry->VqeConfig.byNonLinearityBehaviorB;
	f_pChannelStats->VqeConfig.ulDoubleTalkBehavior						= pChanEntry->VqeConfig.byDoubleTalkBehavior;
	f_pChannelStats->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb	= pChanEntry->VqeConfig.bySoutAutomaticListenerEnhancementGainDb;
	f_pChannelStats->VqeConfig.ulSoutNaturalListenerEnhancementGainDb	= pChanEntry->VqeConfig.bySoutNaturalListenerEnhancementGainDb;
	f_pChannelStats->VqeConfig.fSoutNaturalListenerEnhancement			= pChanEntry->VqeConfig.fSoutNaturalListenerEnhancement;
	f_pChannelStats->VqeConfig.fRoutNoiseReduction						= pChanEntry->VqeConfig.fRoutNoiseReduction;
	f_pChannelStats->VqeConfig.lRoutNoiseReductionLevelGainDb			= pChanEntry->VqeConfig.chRoutNoiseReductionLevelGainDb;
	f_pChannelStats->VqeConfig.fEnableMusicProtection					= pChanEntry->VqeConfig.fEnableMusicProtection;
	f_pChannelStats->VqeConfig.fIdleCodeDetection						= pChanEntry->VqeConfig.fIdleCodeDetection;
	
	/* Copy the CODEC configuration.*/
	f_pChannelStats->CodecConfig.ulAdpcmNibblePosition = pChanEntry->CodecConfig.byAdpcmNibblePosition;

	f_pChannelStats->CodecConfig.ulEncoderPort = pChanEntry->CodecConfig.byEncoderPort;
	f_pChannelStats->CodecConfig.ulEncodingRate = pChanEntry->CodecConfig.byEncodingRate;

	f_pChannelStats->CodecConfig.ulDecoderPort = pChanEntry->CodecConfig.byDecoderPort;
	f_pChannelStats->CodecConfig.ulDecodingRate = pChanEntry->CodecConfig.byDecodingRate;

	f_pChannelStats->CodecConfig.fEnableSilenceSuppression = pChanEntry->CodecConfig.fEnableSilenceSuppression;
	f_pChannelStats->CodecConfig.ulPhase = pChanEntry->CodecConfig.byPhase;
	f_pChannelStats->CodecConfig.ulPhasingType = pChanEntry->CodecConfig.byPhasingType;

	if ( pChanEntry->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
	{
		tPOCT6100_API_PHASING_TSST	pPhasingTsstEntry;

		mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingTsstEntry, pChanEntry->usPhasingTsstIndex );

		f_pChannelStats->CodecConfig.ulPhasingTsstHndl = cOCT6100_HNDL_TAG_PHASING_TSST | (pPhasingTsstEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | pChanEntry->usPhasingTsstIndex;
	}
	else
	{
		f_pChannelStats->CodecConfig.ulPhasingTsstHndl = cOCT6100_INVALID_HANDLE;
	}


	/* Reset the stats and exit if the reset flag is set.*/
	if ( f_pChannelStats->fResetStats == TRUE )
	{
		pChanEntry->sMaxERLE = cOCT6100_INVALID_SIGNED_STAT_W;
		pChanEntry->sMaxERL = cOCT6100_INVALID_SIGNED_STAT_W;
		pChanEntry->usMaxEchoDelay = cOCT6100_INVALID_STAT_W;
	}
	
	/*---------------------------------------------------------------------*/
	/* Update the API internal stats.*/

	BurstParams.ulReadAddress  = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase + (usChanIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize );
	BurstParams.ulReadAddress += f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoStatsOfst;
	BurstParams.ulReadLength = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoStatsSize / 2;	/* Length in words.*/

	mOCT6100_DRIVER_READ_BURST_API( BurstParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Check if the energy stat are found in the new memory location. */
	if ( ( pSharedInfo->ImageInfo.fRinEnergyStat == TRUE )
		&& ( pSharedInfo->ImageInfo.fSoutEnergyStat == TRUE ) )
	{
		ulFeatureBytesOffset = f_pApiInstance->pSharedInfo->MemoryMap.RinEnergyStatFieldOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = f_pApiInstance->pSharedInfo->MemoryMap.RinEnergyStatFieldOfst.byBitOffset;
		ulFeatureFieldLength = f_pApiInstance->pSharedInfo->MemoryMap.RinEnergyStatFieldOfst.byFieldSize;

		ReadParams.ulReadAddress = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase + (usChanIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize );
		ReadParams.ulReadAddress += f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst + ulFeatureBytesOffset;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulFeatureBitOffset < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulFeatureBitOffset < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;

		/* Clear previous value set in the feature field. */
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= ulMask;

		/* Shift to get value. */
		ulTempData = ulTempData >> ulFeatureBitOffset;

		/* Overwrite value read the old way. */
		ausReadData[ 0 ] &= 0x00FF;
		ausReadData[ 0 ] |= (UINT16)( ( ulTempData << 8 ) & 0xFF00 );

		ulFeatureBytesOffset = f_pApiInstance->pSharedInfo->MemoryMap.SoutEnergyStatFieldOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = f_pApiInstance->pSharedInfo->MemoryMap.SoutEnergyStatFieldOfst.byBitOffset;
		ulFeatureFieldLength = f_pApiInstance->pSharedInfo->MemoryMap.SoutEnergyStatFieldOfst.byFieldSize;

		ReadParams.ulReadAddress = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase + (usChanIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize );
		ReadParams.ulReadAddress += f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst + ulFeatureBytesOffset;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulFeatureBitOffset < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulFeatureBitOffset < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;

		/* Clear previous value set in the feature field. */
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= ulMask;

		/* Shift to get value. */
		ulTempData = ulTempData >> ulFeatureBitOffset;

		/* Overwrite value read the old way. */
		ausReadData[ 1 ] &= 0x00FF;
		ausReadData[ 1 ] |= (UINT16)( ( ulTempData << 8 ) & 0xFF00 );
	}

	byRinEnergyRaw  = (BYTE)(( ausReadData[ 0 ] >> 8 ) & 0xFF);
	bySinEnergyRaw  = (BYTE)(( ausReadData[ 0 ] >> 0 ) & 0xFF);
	bySoutEnergyRaw = (BYTE)(( ausReadData[ 1 ] >> 8 ) & 0xFF);
	byCnEnergyRaw   = (BYTE)(( ausReadData[ 5 ] >> 8 ) & 0xFF);

	usEchoDelayInFrames		= (UINT16)(ausReadData[ 4 ]);
	usErlRaw				= ausReadData[ 2 ];

	pChanEntry->byToneDisablerStatus = (UINT8)(( ausReadData[ 5 ] >> 0 ) & 0xFF);
	if ( f_pChannelStats->fResetStats == TRUE )
	{
		pChanEntry->usNumEchoPathChangesOfst = (UINT16)(ausReadData[ 3 ]);
		pChanEntry->usNumEchoPathChanges = 0;
	}
	else /* if ( f_pChannelStats->fResetStats == FALSE ) */
	{
		pChanEntry->usNumEchoPathChanges = (UINT16)( ausReadData[ 3 ] - pChanEntry->usNumEchoPathChangesOfst );
	}

	pChanEntry->sComfortNoiseLevel  = (INT16)( Oct6100ApiOctFloatToDbEnergyByte( byCnEnergyRaw ) & 0xFFFF );
	pChanEntry->sComfortNoiseLevel -= 12;
	pChanEntry->sRinLevel  = (INT16)( Oct6100ApiOctFloatToDbEnergyByte( byRinEnergyRaw ) & 0xFFFF );
	pChanEntry->sRinLevel -= 12;
	pChanEntry->sSinLevel = (INT16)( Oct6100ApiOctFloatToDbEnergyByte( bySinEnergyRaw ) & 0xFFFF );
	pChanEntry->sSinLevel -= 12;
	lSoutEnergyIndB        = Oct6100ApiOctFloatToDbEnergyByte( bySoutEnergyRaw );
	lSoutEnergyIndB		  -= 12;
	
	/* Process some stats only if the channel is converged.*/
	if ( ( usEchoDelayInFrames != cOCT6100_INVALID_ECHO_DELAY ) 
		&& ( pChanEntry->byEchoOperationMode != cOCT6100_ECHO_OP_MODE_POWER_DOWN ) 
		&& ( pChanEntry->byEchoOperationMode != cOCT6100_ECHO_OP_MODE_HT_RESET ) )
	{
		/* Update the current ERL. */
		pChanEntry->sCurrentERL				= (INT16)( Oct6100ApiOctFloatToDbEnergyHalf( usErlRaw ) & 0xFFFF );
		pChanEntry->sCurrentERLE			= (INT16)( ( lSoutEnergyIndB - pChanEntry->sSinLevel ) & 0xFFFF );
		pChanEntry->usCurrentEchoDelay		= (UINT16)( usEchoDelayInFrames / 8 );	/* To convert in msec.*/

		/* Update the max value if required.*/
		if ( pChanEntry->usCurrentEchoDelay > pChanEntry->usMaxEchoDelay || 
			 pChanEntry->usMaxEchoDelay == cOCT6100_INVALID_STAT_W )
		{
			pChanEntry->usMaxEchoDelay = pChanEntry->usCurrentEchoDelay;
		}

		if ( pChanEntry->sCurrentERL > pChanEntry->sMaxERL ||
			 pChanEntry->sMaxERL == cOCT6100_INVALID_SIGNED_STAT_W )
		{
			pChanEntry->sMaxERL = pChanEntry->sCurrentERL;
		}

		if ( pChanEntry->sCurrentERLE > pChanEntry->sMaxERLE ||
			 pChanEntry->sMaxERLE == cOCT6100_INVALID_SIGNED_STAT_W )
		{
			pChanEntry->sMaxERLE = pChanEntry->sCurrentERLE;
		}
	}
	else
	{
		pChanEntry->sCurrentERLE		= cOCT6100_INVALID_SIGNED_STAT_W;
		pChanEntry->sCurrentERL			= cOCT6100_INVALID_SIGNED_STAT_W;
		pChanEntry->usCurrentEchoDelay	= cOCT6100_INVALID_STAT_W;
	}

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fRinAppliedGainStat == TRUE )
	{
		/* Calculate base address for auto level control + high level compensation configuration. */
		ulBaseAddress = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase + ( usChanIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst;
			
		ulFeatureBytesOffset = f_pApiInstance->pSharedInfo->MemoryMap.RinAppliedGainStatOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = f_pApiInstance->pSharedInfo->MemoryMap.RinAppliedGainStatOfst.byBitOffset;
		ulFeatureFieldLength = f_pApiInstance->pSharedInfo->MemoryMap.RinAppliedGainStatOfst.byFieldSize;

		ReadParams.ulReadAddress = ulBaseAddress + ulFeatureBytesOffset;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulFeatureBitOffset < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulFeatureBitOffset < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;

		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= ulMask;

		/* Shift to get value. */
		ulTempData = ulTempData >> ulFeatureBitOffset;
		
		pChanEntry->sRinAppliedGain = (INT16)( 2 * (INT16)( Oct6100ApiOctFloatToDbEnergyHalf( (UINT16)( ulTempData & 0xFFFF ) ) & 0xFFFF ) );
	}

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fSoutAppliedGainStat == TRUE )
	{
		/* Calculate base address for auto level control + high level compensation configuration. */
		ulBaseAddress = f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase + ( usChanIndex * f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst;
			
		ulFeatureBytesOffset = f_pApiInstance->pSharedInfo->MemoryMap.SoutAppliedGainStatOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = f_pApiInstance->pSharedInfo->MemoryMap.SoutAppliedGainStatOfst.byBitOffset;
		ulFeatureFieldLength = f_pApiInstance->pSharedInfo->MemoryMap.SoutAppliedGainStatOfst.byFieldSize;

		ReadParams.ulReadAddress = ulBaseAddress + ulFeatureBytesOffset;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulFeatureBitOffset < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulFeatureBitOffset < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;

		/* Clear previous value set in the feature field. */
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= ulMask;

		/* Shift to get value. */
		ulTempData = ulTempData >> ulFeatureBitOffset;
		
		pChanEntry->sSoutAppliedGain = (INT16)( 2 * (INT16)( Oct6100ApiOctFloatToDbEnergyHalf( (UINT16)( ulTempData & 0xFFFF ) ) & 0xFFFF ) );
	}

	/*---------------------------------------------------------------------*/
	/* Return the real stats.*/

	f_pChannelStats->ulNumEchoPathChanges = pChanEntry->usNumEchoPathChanges;
	if ( usEchoDelayInFrames != cOCT6100_INVALID_ECHO_DELAY )
	{
		f_pChannelStats->fEchoCancellerConverged = TRUE;
	}
	else
	{
		f_pChannelStats->fEchoCancellerConverged = FALSE;
	}
	if ( pChanEntry->sCurrentERL == cOCT6100_INVALID_SIGNED_STAT_W )
		f_pChannelStats->lCurrentERL = cOCT6100_INVALID_SIGNED_STAT;
	else
		f_pChannelStats->lCurrentERL = pChanEntry->sCurrentERL;

	if ( pChanEntry->sMaxERL == cOCT6100_INVALID_SIGNED_STAT_W )
		f_pChannelStats->lMaxERL = cOCT6100_INVALID_SIGNED_STAT;
	else
		f_pChannelStats->lMaxERL = pChanEntry->sMaxERL;

	if ( pChanEntry->usMaxEchoDelay == cOCT6100_INVALID_STAT_W )
		f_pChannelStats->ulMaxEchoDelay = cOCT6100_INVALID_STAT;
	else
		f_pChannelStats->ulMaxEchoDelay = pChanEntry->usMaxEchoDelay;

	if ( pChanEntry->sRinLevel == cOCT6100_INVALID_SIGNED_STAT_W )
		f_pChannelStats->lRinLevel = cOCT6100_INVALID_SIGNED_STAT;
	else
		f_pChannelStats->lRinLevel = pChanEntry->sRinLevel;

	if ( pSharedInfo->ImageInfo.fSinLevel == TRUE )
	{
		if ( pChanEntry->sSinLevel == cOCT6100_INVALID_SIGNED_STAT_W )
			f_pChannelStats->lSinLevel = cOCT6100_INVALID_SIGNED_STAT;
		else
			f_pChannelStats->lSinLevel = pChanEntry->sSinLevel;
	}
	else /* if ( pSharedInfo->ImageInfo.fSinLevel != TRUE ) */
	{
		/* SIN level is not supported in this image. */
		f_pChannelStats->lSinLevel = cOCT6100_INVALID_SIGNED_STAT;
	}

	f_pChannelStats->lRinAppliedGain = pChanEntry->VqeConfig.chRinLevelControlGainDb;
	if ( ( f_pApiInstance->pSharedInfo->ImageInfo.fRinAppliedGainStat == TRUE )
		&& ( ( pChanEntry->VqeConfig.fRinAutomaticLevelControl == TRUE )
		|| ( pChanEntry->VqeConfig.fRinHighLevelCompensation == TRUE ) ) )
	{
		f_pChannelStats->lRinAppliedGain = pChanEntry->sRinAppliedGain;
	}

	f_pChannelStats->lSoutAppliedGain = pChanEntry->VqeConfig.chSoutLevelControlGainDb;
	if ( ( f_pApiInstance->pSharedInfo->ImageInfo.fSoutAppliedGainStat == TRUE )
		&& ( pChanEntry->VqeConfig.fSoutAutomaticLevelControl == TRUE ) )
	{
		f_pChannelStats->lSoutAppliedGain = pChanEntry->sSoutAppliedGain;
	}

	if ( pChanEntry->usCurrentEchoDelay == cOCT6100_INVALID_STAT_W )
		f_pChannelStats->ulCurrentEchoDelay	= cOCT6100_INVALID_STAT;
	else
		f_pChannelStats->ulCurrentEchoDelay	= pChanEntry->usCurrentEchoDelay;

	if ( pSharedInfo->ImageInfo.fSinLevel == TRUE )
	{
		if ( pChanEntry->sCurrentERLE == cOCT6100_INVALID_SIGNED_STAT_W )
			f_pChannelStats->lCurrentERLE = cOCT6100_INVALID_SIGNED_STAT;
		else
			f_pChannelStats->lCurrentERLE = pChanEntry->sCurrentERLE;
	}
	else /* if ( pSharedInfo->ImageInfo.fSinLevel != TRUE ) */
	{
		f_pChannelStats->lCurrentERLE = cOCT6100_INVALID_SIGNED_STAT;
	}

	if ( pSharedInfo->ImageInfo.fSinLevel == TRUE )
	{
		if ( pChanEntry->sMaxERLE == cOCT6100_INVALID_SIGNED_STAT_W )
			f_pChannelStats->lMaxERLE = cOCT6100_INVALID_SIGNED_STAT;
		else
			f_pChannelStats->lMaxERLE = pChanEntry->sMaxERLE;
	}
	else /* if ( pSharedInfo->ImageInfo.fSinLevel != TRUE ) */
	{
		f_pChannelStats->lMaxERLE = cOCT6100_INVALID_SIGNED_STAT;
	}

	f_pChannelStats->lComfortNoiseLevel		= pChanEntry->sComfortNoiseLevel;
	f_pChannelStats->ulToneDisablerStatus   = pChanEntry->byToneDisablerStatus;

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fSinVoiceDetectedStat == TRUE )
	{
		UINT32 ulVoiceDetectedBytesOfst	= f_pApiInstance->pSharedInfo->MemoryMap.SinVoiceDetectedStatOfst.usDwordOffset * 4;
		UINT32 ulVoiceDetectedBitOfst	= f_pApiInstance->pSharedInfo->MemoryMap.SinVoiceDetectedStatOfst.byBitOffset;
		UINT32 ulVoiceDetectedFieldSize	= f_pApiInstance->pSharedInfo->MemoryMap.SinVoiceDetectedStatOfst.byFieldSize;

		/* Set the channel root base address.*/
		UINT32 ulChannelRootBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( usChanIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanRootConfOfst;

		ReadParams.ulReadAddress = ulChannelRootBaseAddress + ulVoiceDetectedBytesOfst;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulVoiceDetectedBitOfst < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulVoiceDetectedBitOfst < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;

		mOCT6100_CREATE_FEATURE_MASK( ulVoiceDetectedFieldSize, ulVoiceDetectedBitOfst, &ulMask );
		
		if ( ( ulTempData & ulMask ) != 0x0 )
			f_pChannelStats->fSinVoiceDetected = TRUE;
		else
			f_pChannelStats->fSinVoiceDetected = FALSE;
	}
	
	/*---------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}	
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveEchoEntry

Description:    Reserves one of the echo channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusEchoIndex			Resulting index reserved in the echo channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveEchoEntry
UINT32 Oct6100ApiReserveEchoEntry(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT	PUINT16						f_pusEchoIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pEchoAlloc;
	UINT32	ulResult;
	UINT32	ulEchoIndex;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ALLOC_PNT( pSharedInfo, pEchoAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pEchoAlloc, &ulEchoIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_CHANNEL_ALL_CHANNELS_ARE_OPENED;
		else
			return cOCT6100_ERR_FATAL_11;
	}

	*f_pusEchoIndex = (UINT16)( ulEchoIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseEchoEntry

Description:    Releases the specified ECHO channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usEchoIndex			Index reserved in the echo channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseEchoEntry
UINT32 Oct6100ApiReleaseEchoEntry(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	UINT16						f_usEchoIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pEchoAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ALLOC_PNT( pSharedInfo, pEchoAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pEchoAlloc, f_usEchoIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		return cOCT6100_ERR_FATAL_12;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBiDirChanEntry

Description:    Reserves one of the bidirectional channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusBiDirChanIndex		Resulting index reserved in the bidir channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBiDirChanEntry
UINT32 Oct6100ApiReserveBiDirChanEntry(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT	PUINT16						f_pusBiDirChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pChanAlloc;
	UINT32	ulResult;
	UINT32	ulBiDirChanIndex;

	/* Get local pointer to shared portion of the API instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_BIDIR_CHANNEL_ALLOC_PNT( pSharedInfo, pChanAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pChanAlloc, &ulBiDirChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_CHANNEL_ALL_BIDIR_CHANNELS_ARE_OPENED;
		else
			return cOCT6100_ERR_FATAL_9F;
	}

	*f_pusBiDirChanIndex = (UINT16)( ulBiDirChanIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBiDirChanEntry

Description:    Releases the specified bidirectional channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulBiDirChanIndex		Bidirectional channel index within the API's Bidir channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBiDirChanEntry
UINT32 Oct6100ApiReleaseBiDirChanEntry(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	UINT32						f_ulBiDirChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pChanAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_BIDIR_CHANNEL_ALLOC_PNT( pSharedInfo, pChanAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pChanAlloc, f_ulBiDirChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		return cOCT6100_ERR_FATAL_A0;
	}

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckTdmConfig

Description:    This function will check the validity of the TDM config parameter
				of an Open TDM config structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pTdmConfig			TDM config of the channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckTdmConfig
UINT32 Oct6100ApiCheckTdmConfig( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_TDM		f_pTdmConfig )
{
	UINT32	ulResult;

	/*==============================================================================*/
	/* Check the TDM configuration parameters.*/

	/* Check the validity of the timeslot and Stream only if it is defined.*/
	if ( f_pTdmConfig->ulRinTimeslot != cOCT6100_UNASSIGNED || 
		 f_pTdmConfig->ulRinStream != cOCT6100_UNASSIGNED )
	{
		if ( f_pTdmConfig->ulRinNumTssts != 1 &&
			 f_pTdmConfig->ulRinNumTssts != 2 )
			return cOCT6100_ERR_CHANNEL_RIN_NUM_TSSTS;

		/* Check the RIN TDM streams, timeslots component for errors.*/
		ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
										   f_pTdmConfig->ulRinNumTssts,
										   f_pTdmConfig->ulRinTimeslot, 
										   f_pTdmConfig->ulRinStream,
										   cOCT6100_INPUT_TSST );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
			{
				return cOCT6100_ERR_CHANNEL_RIN_TIMESLOT;
			}
			else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
			{
				return cOCT6100_ERR_CHANNEL_RIN_STREAM;
			}
			else
			{
				return ulResult;
			}
		}
	}

	/* Check the validity of the timeslot and Stream only if it is defined.*/
	if ( f_pTdmConfig->ulRoutTimeslot != cOCT6100_UNASSIGNED || 
		 f_pTdmConfig->ulRoutStream != cOCT6100_UNASSIGNED )
	{
		if ( f_pTdmConfig->ulRoutNumTssts != 1 &&
			 f_pTdmConfig->ulRoutNumTssts != 2 )
			return cOCT6100_ERR_CHANNEL_ROUT_NUM_TSSTS;

		/* Check the ROUT TDM streams, timeslots component for errors.*/
		ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
										   f_pTdmConfig->ulRoutNumTssts,
										   f_pTdmConfig->ulRoutTimeslot, 
										   f_pTdmConfig->ulRoutStream,
										   cOCT6100_OUTPUT_TSST );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
			{
				return cOCT6100_ERR_CHANNEL_ROUT_TIMESLOT;
			}
			else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
			{
				return cOCT6100_ERR_CHANNEL_ROUT_STREAM;
			}
			else
			{
				return ulResult;
			}
		}
	}

	/* Check the validity of the timeslot and Stream only if it is defined.*/
	if ( f_pTdmConfig->ulSinTimeslot != cOCT6100_UNASSIGNED || 
		 f_pTdmConfig->ulSinStream != cOCT6100_UNASSIGNED )
	{
		if ( f_pTdmConfig->ulSinNumTssts != 1 &&
			 f_pTdmConfig->ulSinNumTssts != 2 )
			return cOCT6100_ERR_CHANNEL_SIN_NUM_TSSTS;

		/* Check the SIN TDM streams, timeslots component for errors.*/
		ulResult = Oct6100ApiValidateTsst( f_pApiInstance,
										   f_pTdmConfig->ulSinNumTssts, 
										   f_pTdmConfig->ulSinTimeslot, 
										   f_pTdmConfig->ulSinStream,
										   cOCT6100_INPUT_TSST );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
			{
				return cOCT6100_ERR_CHANNEL_SIN_TIMESLOT;
			}
			else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
			{
				return cOCT6100_ERR_CHANNEL_SIN_STREAM;
			}
			else
			{
				return ulResult;
			}
		}
	}

	/* Check the validity of the timeslot and Stream only if it is defined.*/
	if ( f_pTdmConfig->ulSoutTimeslot != cOCT6100_UNASSIGNED || 
		 f_pTdmConfig->ulSoutStream != cOCT6100_UNASSIGNED )
	{
		if ( f_pTdmConfig->ulSoutNumTssts != 1 &&
			 f_pTdmConfig->ulSoutNumTssts != 2 )
			return cOCT6100_ERR_CHANNEL_SOUT_NUM_TSSTS;

		/* Check the ROUT TDM streams, timeslots component for errors.*/
		ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
										   f_pTdmConfig->ulSoutNumTssts,
										   f_pTdmConfig->ulSoutTimeslot, 
										   f_pTdmConfig->ulSoutStream,
										   cOCT6100_OUTPUT_TSST );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
			{
				return cOCT6100_ERR_CHANNEL_SOUT_TIMESLOT;
			}
			else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
			{
				return cOCT6100_ERR_CHANNEL_SOUT_STREAM;
			}
			else
			{
				return ulResult;
			}
		}
	}	
	
	/* Check the PCM law parameters.*/
	if ( f_pTdmConfig->ulRinPcmLaw != cOCT6100_PCM_U_LAW && 
		 f_pTdmConfig->ulRinPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_CHANNEL_RIN_PCM_LAW;

	if ( f_pTdmConfig->ulSinPcmLaw != cOCT6100_PCM_U_LAW && 
		 f_pTdmConfig->ulSinPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_CHANNEL_SIN_PCM_LAW;

	if ( f_pTdmConfig->ulRoutPcmLaw != cOCT6100_PCM_U_LAW && 
		 f_pTdmConfig->ulRoutPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_CHANNEL_ROUT_PCM_LAW;

	if ( f_pTdmConfig->ulSoutPcmLaw != cOCT6100_PCM_U_LAW && 
		 f_pTdmConfig->ulSoutPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_CHANNEL_SOUT_PCM_LAW;
	
	/*==============================================================================*/



	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckVqeConfig

Description:    This function will check the validity of the VQE config parameter
				of an Open VQE config structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pVqeConfig			VQE config of the channel.
f_fEnableToneDisabler	Whether the tone disabler is active or not.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckVqeConfig
UINT32 Oct6100ApiCheckVqeConfig( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_CHANNEL_OPEN_VQE		f_pVqeConfig,
				IN		BOOL							f_fEnableToneDisabler )
{
	tPOCT6100_API_IMAGE_INFO		pImageInfo;

	pImageInfo = &f_pApiInstance->pSharedInfo->ImageInfo;

	if ( f_pVqeConfig->fEnableNlp != TRUE && f_pVqeConfig->fEnableNlp != FALSE )
		return cOCT6100_ERR_CHANNEL_ENABLE_NLP;

	if ( f_pVqeConfig->fEnableNlp == TRUE && pImageInfo->fNlpControl == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_NLP_CONTROL;
	


	/* Check the comfort noise mode.*/
	if ( f_pVqeConfig->ulComfortNoiseMode != cOCT6100_COMFORT_NOISE_OFF && pImageInfo->fComfortNoise == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_BKG_NOISE_FREEZE;

	if ( f_pVqeConfig->ulComfortNoiseMode != cOCT6100_COMFORT_NOISE_NORMAL && 
		 f_pVqeConfig->ulComfortNoiseMode != cOCT6100_COMFORT_NOISE_EXTENDED &&
		 f_pVqeConfig->ulComfortNoiseMode != cOCT6100_COMFORT_NOISE_FAST_LATCH &&
		 f_pVqeConfig->ulComfortNoiseMode != cOCT6100_COMFORT_NOISE_OFF )
		return cOCT6100_ERR_CHANNEL_COMFORT_NOISE_MODE;

	/* Check the DC offset removal.*/
	if ( f_pVqeConfig->fSinDcOffsetRemoval != TRUE && f_pVqeConfig->fSinDcOffsetRemoval != FALSE )
		return cOCT6100_ERR_CHANNEL_SIN_DC_OFFSET_REM;

	if ( f_pVqeConfig->fSinDcOffsetRemoval == TRUE && pImageInfo->fSinDcOffsetRemoval == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_SIN_DC_OFFSET_REM;

	if ( f_pVqeConfig->fRinDcOffsetRemoval != TRUE && f_pVqeConfig->fRinDcOffsetRemoval != FALSE )
		return cOCT6100_ERR_CHANNEL_RIN_DC_OFFSET_REM;

	if ( f_pVqeConfig->fRinDcOffsetRemoval == TRUE && pImageInfo->fRinDcOffsetRemoval == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_RIN_DC_OFFSET_REM;

	/* Check the Level control.*/
	if ( f_pVqeConfig->fRinLevelControl != TRUE && f_pVqeConfig->fRinLevelControl != FALSE )
		return cOCT6100_ERR_CHANNEL_RIN_LEVEL_CONTROL;

	if ( f_pVqeConfig->fSoutLevelControl != TRUE && f_pVqeConfig->fSoutLevelControl != FALSE )
		return cOCT6100_ERR_CHANNEL_SOUT_LEVEL_CONTROL;

	if ( ( f_pVqeConfig->lRinLevelControlGainDb < -24 ) || ( f_pVqeConfig->lRinLevelControlGainDb >  24 ) )
		return cOCT6100_ERR_CHANNEL_RIN_LEVEL_CONTROL_GAIN;

	if ( ( f_pVqeConfig->lSoutLevelControlGainDb < -24 ) || ( f_pVqeConfig->lSoutLevelControlGainDb >  24 ) )
		return cOCT6100_ERR_CHANNEL_SOUT_LEVEL_CONTROL_GAIN;

	if ( ( f_pVqeConfig->fRinAutomaticLevelControl != TRUE ) && ( f_pVqeConfig->fRinAutomaticLevelControl != FALSE ) )
		return cOCT6100_ERR_CHANNEL_RIN_AUTO_LEVEL_CONTROL;

	if ( ( f_pVqeConfig->fRinHighLevelCompensation != TRUE ) && ( f_pVqeConfig->fRinHighLevelCompensation != FALSE ) )
		return cOCT6100_ERR_CHANNEL_RIN_HIGH_LEVEL_COMP;

	if ( ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE ) && ( pImageInfo->fRinAutoLevelControl == FALSE ) ) 
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_RIN_AUTO_LC;

	if ( ( f_pVqeConfig->fRinHighLevelCompensation == TRUE ) && ( pImageInfo->fRinHighLevelCompensation == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_RIN_HIGH_LEVEL_COMP;

	if ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE )
	{
		if ( f_pVqeConfig->fRinLevelControl == TRUE )
			return cOCT6100_ERR_CHANNEL_RIN_AUTO_LEVEL_MANUAL;

		if ( f_pVqeConfig->fRinHighLevelCompensation == TRUE )
			return cOCT6100_ERR_CHANNEL_RIN_AUTO_LEVEL_HIGH_LEVEL_COMP;

		if ( ( f_pVqeConfig->lRinAutomaticLevelControlTargetDb < -40 || f_pVqeConfig->lRinAutomaticLevelControlTargetDb > 0 ) )
			return cOCT6100_ERR_CHANNEL_RIN_AUTO_LEVEL_CONTROL_TARGET;
	}

	if ( f_pVqeConfig->fRinHighLevelCompensation == TRUE )
	{
		if ( f_pVqeConfig->fRinLevelControl == TRUE )
			return cOCT6100_ERR_CHANNEL_RIN_HIGH_LEVEL_COMP_MANUAL;

		if ( ( f_pVqeConfig->lRinHighLevelCompensationThresholdDb < -40 || f_pVqeConfig->lRinHighLevelCompensationThresholdDb > 0 ) )
			return cOCT6100_ERR_CHANNEL_RIN_HIGH_LEVEL_COMP_THRESHOLD;
	}

	if ( f_pVqeConfig->fSoutAutomaticLevelControl != TRUE && f_pVqeConfig->fSoutAutomaticLevelControl != FALSE )
		return cOCT6100_ERR_CHANNEL_SOUT_AUTO_LEVEL_CONTROL;

	if ( ( f_pVqeConfig->fSoutAutomaticLevelControl == TRUE ) && ( pImageInfo->fSoutAutoLevelControl == FALSE ) ) 
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_SOUT_AUTO_LC;

	if ( f_pVqeConfig->fSoutAutomaticLevelControl == TRUE )
	{
		if ( f_pVqeConfig->fSoutLevelControl == TRUE )
			return cOCT6100_ERR_CHANNEL_SOUT_AUTO_LEVEL_MANUAL;

		if ( ( f_pVqeConfig->lSoutAutomaticLevelControlTargetDb < -40 || f_pVqeConfig->lSoutAutomaticLevelControlTargetDb > 0 ) )
			return cOCT6100_ERR_CHANNEL_SOUT_AUTO_LEVEL_CONTROL_TARGET;
	}

	if ( f_pVqeConfig->fSoutAdaptiveNoiseReduction != TRUE && 
		 f_pVqeConfig->fSoutAdaptiveNoiseReduction != FALSE )
		return cOCT6100_ERR_CHANNEL_SOUT_ADAPT_NOISE_REDUCTION;

	if ( f_pVqeConfig->fSoutAdaptiveNoiseReduction == TRUE && pImageInfo->fAdaptiveNoiseReduction == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ANR;

	if ( f_pVqeConfig->fSoutConferencingNoiseReduction != TRUE && 
		 f_pVqeConfig->fSoutConferencingNoiseReduction != FALSE )
		return cOCT6100_ERR_CHANNEL_SOUT_CONFERENCE_NOISE_REDUCTION;

	if ( f_pVqeConfig->fSoutConferencingNoiseReduction == TRUE && pImageInfo->fConferencingNoiseReduction == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_CNR;

	/* Validate Sout noise bleaching parameter. */
	if ( f_pVqeConfig->fSoutNoiseBleaching != TRUE && 
		 f_pVqeConfig->fSoutNoiseBleaching != FALSE )
		return cOCT6100_ERR_CHANNEL_SOUT_NOISE_BLEACHING;

	/* Check if firmware supports Sout noise bleaching. */
	if ( f_pVqeConfig->fSoutNoiseBleaching == TRUE && pImageInfo->fSoutNoiseBleaching == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_NOISE_BLEACHING;

	/* If Sout noise bleaching is requested, no ANR or CNR shall be activated. */
	if ( f_pVqeConfig->fSoutNoiseBleaching == TRUE )
	{
		/* No xNR! */
		if ( ( f_pVqeConfig->fSoutConferencingNoiseReduction == TRUE )
			|| ( f_pVqeConfig->fSoutAdaptiveNoiseReduction == TRUE ) )
			return cOCT6100_ERR_CHANNEL_SOUT_NOISE_BLEACHING_NR;
	}

	/* Cannot activate both ANR and CNR when noise bleaching is present */
	if ( pImageInfo->fSoutNoiseBleaching == TRUE )
	{
		if ( f_pVqeConfig->fSoutAdaptiveNoiseReduction == TRUE && 
			f_pVqeConfig->fSoutConferencingNoiseReduction == TRUE )
			return cOCT6100_ERR_CHANNEL_ANR_CNR_SIMULTANEOUSLY;
	}

	/* Validate the DTMF tone removal parameter.*/
	if ( f_pVqeConfig->fDtmfToneRemoval != TRUE && f_pVqeConfig->fDtmfToneRemoval != FALSE )
		return cOCT6100_ERR_CHANNEL_TONE_REMOVAL;

	if ( f_pVqeConfig->fDtmfToneRemoval == TRUE && pImageInfo->fToneRemoval == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_TONE_REMOVAL;



	/* Check the Tail displacement enable.*/
	if ( f_pVqeConfig->fEnableTailDisplacement != TRUE && f_pVqeConfig->fEnableTailDisplacement != FALSE )
		return cOCT6100_ERR_CHANNEL_ENABLE_TAIL_DISPLACEMENT;

	if ( f_pVqeConfig->fEnableTailDisplacement == TRUE && pImageInfo->fTailDisplacement == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_TAIL_DISPLACEMENT;

	/* Check the Tail displacement value.*/
	if ( f_pVqeConfig->fEnableTailDisplacement == TRUE )
	{
		if ( f_pVqeConfig->ulTailDisplacement != cOCT6100_AUTO_SELECT_TAIL )
		{
			/* Check if this feature is supported by the image. */
			if ( pImageInfo->fPerChannelTailDisplacement == FALSE )
				return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_PER_CHAN_TAIL;

			/* Check that this value is not greater then what the image supports. */
			if ( f_pVqeConfig->ulTailDisplacement > pImageInfo->usMaxTailDisplacement )
				return cOCT6100_ERR_CHANNEL_TAIL_DISPLACEMENT_INVALID;
		}
	}

	/* Check the tail length value. */
	if ( f_pVqeConfig->ulTailLength != cOCT6100_AUTO_SELECT_TAIL )
	{
		/* Check if this feature is supported by the image. */
		if ( ( pImageInfo->fPerChannelTailLength == FALSE )
			&& ( (UINT16)( f_pVqeConfig->ulTailLength & 0xFFFF ) != pImageInfo->usMaxTailLength ) )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_TAIL_LENGTH;

		if ( ( f_pVqeConfig->ulTailLength < 32 ) || ( f_pVqeConfig->ulTailLength > 128 ) 
			|| ( ( f_pVqeConfig->ulTailLength % 4 ) != 0x0 ) )
			return cOCT6100_ERR_CHANNEL_TAIL_LENGTH;

		/* Check if the requested tail length is supported by the chip. */
		if ( f_pVqeConfig->ulTailLength > pImageInfo->usMaxTailLength )
			return cOCT6100_ERR_CHANNEL_TAIL_LENGTH_INVALID;
	}

	/* Validate the acoustic echo cancellation parameter.*/
	if ( f_pVqeConfig->fAcousticEcho != TRUE && f_pVqeConfig->fAcousticEcho != FALSE )
		return cOCT6100_ERR_CHANNEL_ACOUSTIC_ECHO;

	if ( f_pVqeConfig->fAcousticEcho == TRUE && pImageInfo->fAcousticEcho == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ACOUSTIC_ECHO;

	if ( f_pVqeConfig->fAcousticEcho == TRUE )
	{
		/* Check if acoustic echo tail length configuration is supported in the image. */
		if ( ( f_pVqeConfig->ulAecTailLength != 128 ) && ( pImageInfo->fAecTailLength == FALSE ) )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ACOUSTIC_ECHO_TAIL_LENGTH;

		/* Check the requested acoustic echo tail length. */
		if ( ( f_pVqeConfig->ulAecTailLength != 128 )
			&& ( f_pVqeConfig->ulAecTailLength != 256 ) 
			&& ( f_pVqeConfig->ulAecTailLength != 512 )
			&& ( f_pVqeConfig->ulAecTailLength != 1024 ) )
			return cOCT6100_ERR_CHANNEL_ACOUSTIC_ECHO_TAIL_LENGTH;

		if ( f_pVqeConfig->fEnableTailDisplacement == TRUE )
		{
			UINT32 ulTailSum;

			/* Start with requested tail displacement. */
			if ( f_pVqeConfig->ulTailDisplacement == cOCT6100_AUTO_SELECT_TAIL )
			{
				ulTailSum = f_pApiInstance->pSharedInfo->ChipConfig.usTailDisplacement;
			}
			else
			{
				ulTailSum = f_pVqeConfig->ulTailDisplacement;
			}

			/* Add requested tail length. */
			if ( f_pVqeConfig->ulTailLength == cOCT6100_AUTO_SELECT_TAIL )
			{
				ulTailSum += f_pApiInstance->pSharedInfo->ImageInfo.usMaxTailLength;
			}
			else
			{
				ulTailSum += f_pVqeConfig->ulTailLength;
			}

			/* The tail sum must be smaller then the requested AEC tail length. */
			if ( ulTailSum > f_pVqeConfig->ulAecTailLength )
				return cOCT6100_ERR_CHANNEL_ACOUSTIC_ECHO_TAIL_SUM;
		}
	}
	
	/* Validate the Default ERL parameter.*/
	if ( f_pVqeConfig->lDefaultErlDb != -6 && pImageInfo->fDefaultErl == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_DEFAULT_ERL;

	if ( ( f_pVqeConfig->lDefaultErlDb != 0 ) && 
		( f_pVqeConfig->lDefaultErlDb != -3 ) && 
		( f_pVqeConfig->lDefaultErlDb != -6 ) &&
		( f_pVqeConfig->lDefaultErlDb != -9 ) &&
		( f_pVqeConfig->lDefaultErlDb != -12 ) )
		return cOCT6100_ERR_CHANNEL_DEFAULT_ERL;

	/* Validate the Default AEC ERL parameter.*/
	if ( f_pVqeConfig->lAecDefaultErlDb != 0 && pImageInfo->fAecDefaultErl == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_AEC_DEFAULT_ERL;

	if ( f_pVqeConfig->lAecDefaultErlDb != 0 && f_pVqeConfig->lAecDefaultErlDb != -3 && f_pVqeConfig->lAecDefaultErlDb != -6 )
		return cOCT6100_ERR_CHANNEL_AEC_DEFAULT_ERL;

	/* Validate the non-linearity A parameter.*/
	if ( f_pVqeConfig->ulNonLinearityBehaviorA != 1 && pImageInfo->fNonLinearityBehaviorA == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_DOUBLE_TALK;

	if ( f_pVqeConfig->ulNonLinearityBehaviorA >= 14 )
		return cOCT6100_ERR_CHANNEL_DOUBLE_TALK;

	/* Validate the non-linearity B parameter.*/
	if ( f_pVqeConfig->ulNonLinearityBehaviorB != 0 && pImageInfo->fNonLinearityBehaviorB == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_NON_LINEARITY_B;

	if ( f_pVqeConfig->ulNonLinearityBehaviorB >= 9 )
		return cOCT6100_ERR_CHANNEL_NON_LINEARITY_B;

	/* Check if configuring the double talk behavior is supported in the firmware. */
	if ( f_pVqeConfig->ulDoubleTalkBehavior != cOCT6100_DOUBLE_TALK_BEH_NORMAL && pImageInfo->fDoubleTalkBehavior == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_DOUBLE_TALK_BEHAVIOR_MODE;
	
	/* Validate the double talk behavior mode parameter. */
	if ( f_pVqeConfig->ulDoubleTalkBehavior != cOCT6100_DOUBLE_TALK_BEH_NORMAL && f_pVqeConfig->ulDoubleTalkBehavior != cOCT6100_DOUBLE_TALK_BEH_LESS_AGGRESSIVE )
		return cOCT6100_ERR_CHANNEL_DOUBLE_TALK_MODE;

	/* Validate the Sout automatic listener enhancement ratio. */
	if ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != 0 && pImageInfo->fListenerEnhancement == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ALE;

	if ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb > 30 )
		return cOCT6100_ERR_CHANNEL_ALE_RATIO;

	/* Validate the Sout natural listener enhancement ratio. */
	if ( f_pVqeConfig->fSoutNaturalListenerEnhancement != TRUE && f_pVqeConfig->fSoutNaturalListenerEnhancement != FALSE )
		return cOCT6100_ERR_CHANNEL_NLE_FLAG;

	if ( f_pVqeConfig->fSoutNaturalListenerEnhancement == TRUE && pImageInfo->fListenerEnhancement == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_NLE;

	if ( f_pVqeConfig->fSoutNaturalListenerEnhancement == TRUE )
	{
		if ( f_pVqeConfig->ulSoutNaturalListenerEnhancementGainDb > 30 )
			return cOCT6100_ERR_CHANNEL_NLE_RATIO;
	}

	/* Both ALE and NLE cannot be activated simultaneously. */
	if ( ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != 0 )
		&& ( f_pVqeConfig->fSoutNaturalListenerEnhancement == TRUE ) )
		return cOCT6100_ERR_CHANNEL_ALE_NLE_SIMULTANEOUSLY;
	
	/* Validate Rout noise reduction. */
	if ( f_pVqeConfig->fRoutNoiseReduction != TRUE && f_pVqeConfig->fRoutNoiseReduction != FALSE )
		return cOCT6100_ERR_CHANNEL_ROUT_NOISE_REDUCTION;

	/* Check if Rout noise reduction is supported. */
	if ( f_pVqeConfig->fRoutNoiseReduction == TRUE && pImageInfo->fRoutNoiseReduction == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ROUT_NR;

	/*Check if noise reduction level gain is supported*/
	if ( ( pImageInfo->fRoutNoiseReductionLevel == FALSE ) && ( f_pVqeConfig->lRoutNoiseReductionLevelGainDb != -18 ) )
		 return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ROUT_NOISE_REDUCTION_GAIN;
	
	if ( ( f_pVqeConfig->lRoutNoiseReductionLevelGainDb != 0 ) && 
		( f_pVqeConfig->lRoutNoiseReductionLevelGainDb != -6 ) && 
		( f_pVqeConfig->lRoutNoiseReductionLevelGainDb != -12 ) &&
		( f_pVqeConfig->lRoutNoiseReductionLevelGainDb != -18 ) )
	
		return cOCT6100_ERR_CHANNEL_ROUT_NOISE_REDUCTION_GAIN;

	/* Check if ANR SNRE is supported. */
	if ( ( f_pVqeConfig->lAnrSnrEnhancementDb != -18 ) && ( pImageInfo->fAnrSnrEnhancement == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ANR_SNR_ENHANCEMENT;

	/* Validate Sout ANR SNR enhancement. */
	if ( ( f_pVqeConfig->lAnrSnrEnhancementDb != -9 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -12 ) 
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -15 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -18 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -21 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -24 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -27 )
		&& ( f_pVqeConfig->lAnrSnrEnhancementDb != -30 ) )
		return cOCT6100_ERR_CHANNEL_ANR_SNR_ENHANCEMENT;
	
	/* Validate ANR voice-noise segregation. */
	if ( f_pVqeConfig->ulAnrVoiceNoiseSegregation > 15 )
		return cOCT6100_ERR_CHANNEL_ANR_SEGREGATION;

	/* Check if ANR VN segregation is supported. */
	if ( ( f_pVqeConfig->ulAnrVoiceNoiseSegregation != 6 ) && ( pImageInfo->fAnrVoiceNoiseSegregation == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ANR_SEGREGATION;

	/* Check if the loaded image supports tone disabler VQE activation delay. */
	if ( ( f_pVqeConfig->ulToneDisablerVqeActivationDelay != 300 )
		&& ( pImageInfo->fToneDisablerVqeActivationDelay == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_TONE_DISABLER_ACTIVATION_DELAY;

	/* Check if the specified tone disabler VQE activation delay is correct. */
	if ( ( f_pVqeConfig->ulToneDisablerVqeActivationDelay < 300 )
		|| ( ( ( f_pVqeConfig->ulToneDisablerVqeActivationDelay - 300 ) % 512 ) != 0 ) )
		return cOCT6100_ERR_CHANNEL_TONE_DISABLER_ACTIVATION_DELAY;

	/* Check the enable music protection flag. */
	if ( ( f_pVqeConfig->fEnableMusicProtection != TRUE ) && ( f_pVqeConfig->fEnableMusicProtection != FALSE ) )
		return cOCT6100_ERR_CHANNEL_ENABLE_MUSIC_PROTECTION;

	/* The music protection module can only be activated if the image supports it. */
	if ( ( f_pVqeConfig->fEnableMusicProtection == TRUE ) &&
		( pImageInfo->fMusicProtection == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_MUSIC_PROTECTION;

	/* Check the enable idle code detection flag. */
	if ( ( f_pVqeConfig->fIdleCodeDetection != TRUE ) && ( f_pVqeConfig->fIdleCodeDetection != FALSE ) )
		return cOCT6100_ERR_CHANNEL_IDLE_CODE_DETECTION;

	/* The idle code detection module can only be activated if the image supports it. */
	if ( ( f_pVqeConfig->fIdleCodeDetection == TRUE ) && ( pImageInfo->fIdleCodeDetection == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_IDLE_CODE_DETECTION;

	/* The idle code detection module can be disabled only if idle code detection configuration */
	/* is supported in the image. */
	if ( pImageInfo->fIdleCodeDetection == TRUE )
	{
		if ( ( f_pVqeConfig->fIdleCodeDetection == FALSE ) && ( pImageInfo->fIdleCodeDetectionConfiguration == FALSE ) )
			return cOCT6100_ERR_NOT_SUPPORTED_IDLE_CODE_DETECTION_CONFIG;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckCodecConfig

Description:    This function will check the validity of the Codec config parameter
				of an Open Codec config structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pCodecConfig			Codec config of the channel.
f_ulDecoderNumTssts		Number of TSST for the decoder.
f_pusPhasingTsstIndex	Pointer to the Phasing TSST index within the API's phasing TSST list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckCodecConfig
UINT32 Oct6100ApiCheckCodecConfig( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_CODEC	f_pCodecConfig,
				IN  UINT32							f_ulDecoderNumTssts,
				OUT PUINT16							f_pusPhasingTsstIndex )
{
	
	/* Verify the ADPCM nibble value.*/
	if ( f_pCodecConfig->ulAdpcmNibblePosition != cOCT6100_ADPCM_IN_LOW_BITS && 
		 f_pCodecConfig->ulAdpcmNibblePosition != cOCT6100_ADPCM_IN_HIGH_BITS )
		return cOCT6100_ERR_CHANNEL_ADPCM_NIBBLE;

	/* Verify the Encoder port.*/
	if ( f_pCodecConfig->ulEncoderPort != cOCT6100_CHANNEL_PORT_ROUT && 
		 f_pCodecConfig->ulEncoderPort != cOCT6100_CHANNEL_PORT_SOUT &&
		 f_pCodecConfig->ulEncoderPort != cOCT6100_NO_ENCODING )
		return cOCT6100_ERR_CHANNEL_ENCODER_PORT;
	
	/* Verify the Decoder port.*/
	if ( f_pCodecConfig->ulDecoderPort != cOCT6100_CHANNEL_PORT_RIN && 
		 f_pCodecConfig->ulDecoderPort != cOCT6100_CHANNEL_PORT_SIN &&
		 f_pCodecConfig->ulDecoderPort != cOCT6100_NO_DECODING )
		return cOCT6100_ERR_CHANNEL_DECODER_PORT;

	/* The codec cannot be on the same stream.*/
	if ( f_pCodecConfig->ulEncoderPort == cOCT6100_CHANNEL_PORT_ROUT && 
		 f_pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
		return cOCT6100_ERR_CHANNEL_INVALID_CODEC_POSITION;

	if ( f_pCodecConfig->ulEncoderPort == cOCT6100_CHANNEL_PORT_SOUT && 
		 f_pCodecConfig->ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN )
		return cOCT6100_ERR_CHANNEL_INVALID_CODEC_POSITION;

	/* Verify if the requested functions are supported by the chip.*/
	if ( f_pApiInstance->pSharedInfo->ImageInfo.fAdpcm == FALSE &&
		 f_pCodecConfig->ulEncoderPort != cOCT6100_NO_ENCODING )
	{
		if ( f_pCodecConfig->ulEncodingRate != cOCT6100_G711_64KBPS )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_ENCODING;
	}

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fAdpcm == FALSE &&
		 f_pCodecConfig->ulDecoderPort != cOCT6100_NO_DECODING )
	{
		if ( f_pCodecConfig->ulDecodingRate != cOCT6100_G711_64KBPS )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_DECODING;
	}
	
	/* Check if encoder port has been specified when a rate has been set. */
	if ( f_pCodecConfig->ulEncoderPort == cOCT6100_NO_ENCODING && 
		f_pCodecConfig->ulEncodingRate != cOCT6100_G711_64KBPS )
		return cOCT6100_ERR_CHANNEL_ENCODER_PORT;

	/* Check if decoder port has been specified when a rate has been set. */
	if ( f_pCodecConfig->ulDecoderPort == cOCT6100_NO_DECODING && 
		f_pCodecConfig->ulDecodingRate != cOCT6100_G711_64KBPS )
		return cOCT6100_ERR_CHANNEL_DECODER_PORT;

	/* Check Encoder related parameter if one is used.*/
	if ( f_pCodecConfig->ulEncoderPort != cOCT6100_NO_ENCODING )
	{
		/* Check the Encoder compression rate.*/
		if ( ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G711_64KBPS ) && 
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G726_40KBPS ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G726_32KBPS ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G726_24KBPS ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G726_16KBPS ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_40KBPS_4_1 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_40KBPS_3_2 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_40KBPS_2_3 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_32KBPS_4_0 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_32KBPS_3_1 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_32KBPS_2_2 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_24KBPS_3_0 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_24KBPS_2_1 ) &&
			 ( f_pCodecConfig->ulEncodingRate  != cOCT6100_G727_16KBPS_2_0 ) )
			return cOCT6100_ERR_CHANNEL_ENCODING_RATE;

		/* Verify phasing information.*/
		if ( f_pCodecConfig->ulPhasingType != cOCT6100_SINGLE_PHASING && 
			 f_pCodecConfig->ulPhasingType != cOCT6100_DUAL_PHASING &&
			 f_pCodecConfig->ulPhasingType != cOCT6100_NO_PHASING )
			return cOCT6100_ERR_CHANNEL_PHASING_TYPE;

		/* Verify the silence suppression parameters.*/
		if ( f_pCodecConfig->fEnableSilenceSuppression != TRUE && 
			 f_pCodecConfig->fEnableSilenceSuppression != FALSE )
			return cOCT6100_ERR_CHANNEL_SIL_SUP_ENABLE;

		if ( f_pCodecConfig->fEnableSilenceSuppression == TRUE &&
			 f_pApiInstance->pSharedInfo->ImageInfo.fSilenceSuppression == FALSE )
			return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_SIL_SUP;

		if ( f_pCodecConfig->fEnableSilenceSuppression == TRUE &&
			 f_pCodecConfig->ulPhasingType == cOCT6100_NO_PHASING )
			return cOCT6100_ERR_CHANNEL_PHASE_TYPE_REQUIRED;

		if ( f_pCodecConfig->fEnableSilenceSuppression == TRUE &&
			 f_pCodecConfig->ulPhasingTsstHndl == cOCT6100_INVALID_HANDLE )
			return cOCT6100_ERR_CHANNEL_PHASING_TSST_REQUIRED;

		if ( f_pCodecConfig->ulPhasingTsstHndl == cOCT6100_INVALID_HANDLE &&
			 f_pCodecConfig->ulPhasingType != cOCT6100_NO_PHASING )
			return cOCT6100_ERR_CHANNEL_PHASING_TSST_REQUIRED;

		/* Silence suppression can only be performed if the encoder is using the SOUT port.*/
		if ( f_pCodecConfig->fEnableSilenceSuppression == TRUE &&
			 f_pCodecConfig->ulEncoderPort != cOCT6100_CHANNEL_PORT_SOUT )
			return cOCT6100_ERR_CHANNEL_SIL_SUP_INVALID_ENCODER_PORT;

		/* Check phasing TSST info if phasing is required.*/
		if ( f_pCodecConfig->ulPhasingTsstHndl != cOCT6100_INVALID_HANDLE )
		{
			tPOCT6100_API_PHASING_TSST	pPhasingEntry;
			UINT32						ulEntryOpenCnt;

			/* Check the provided handle. */
			if ( (f_pCodecConfig->ulPhasingTsstHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_PHASING_TSST )
				return cOCT6100_ERR_CHANNEL_INVALID_PHASING_HANDLE;

			*f_pusPhasingTsstIndex = (UINT16)( f_pCodecConfig->ulPhasingTsstHndl & cOCT6100_HNDL_INDEX_MASK );
			if ( *f_pusPhasingTsstIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxPhasingTssts )
				return cOCT6100_ERR_CHANNEL_INVALID_PHASING_HANDLE;

			mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingEntry, *f_pusPhasingTsstIndex );

			/* Extract the entry open count from the provided handle. */
			ulEntryOpenCnt = (f_pCodecConfig->ulPhasingTsstHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;
			
			/* Verify if the state of the phasing TSST.*/
			if ( pPhasingEntry->fReserved != TRUE )
				return cOCT6100_ERR_CHANNEL_PHASING_TSST_NOT_OPEN;
			if ( ulEntryOpenCnt != pPhasingEntry->byEntryOpenCnt )
				return cOCT6100_ERR_CHANNEL_INVALID_PHASING_HANDLE;
			
			/* Check the specified phase value against the phasing length of the phasing TSST.*/
			if ( ( f_pCodecConfig->ulPhase == 0 )
				|| ( f_pCodecConfig->ulPhase >= pPhasingEntry->usPhasingLength ) )
				return cOCT6100_ERR_CHANNEL_PHASING_INVALID_PHASE;
		}
		else
		{
			*f_pusPhasingTsstIndex = cOCT6100_INVALID_INDEX;
		}
	}
	else
	{
		*f_pusPhasingTsstIndex = cOCT6100_INVALID_INDEX;
	}


	/* Check Decoder related parameter if one is used.*/
	if ( f_pCodecConfig->ulDecoderPort != cOCT6100_NO_DECODING )
	{
		/* Check the Decoding rate.*/
		if ( f_pCodecConfig->ulDecodingRate  != cOCT6100_G711_64KBPS &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G726_40KBPS &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G726_32KBPS &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G726_24KBPS &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G726_16KBPS &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G726_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G711_G726_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G727_2C_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G727_3C_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G727_4C_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G711_G727_2C_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G711_G727_3C_ENCODED &&
			 f_pCodecConfig->ulDecodingRate  != cOCT6100_G711_G727_4C_ENCODED )
			return cOCT6100_ERR_CHANNEL_DECODING_RATE;

		/* Make sure that two timeslot are allocated if PCM-ECHO encoded is selected.*/
		if ( (f_pCodecConfig->ulDecodingRate == cOCT6100_G711_G726_ENCODED ||
			  f_pCodecConfig->ulDecodingRate == cOCT6100_G711_G727_2C_ENCODED ||
			  f_pCodecConfig->ulDecodingRate == cOCT6100_G711_G727_3C_ENCODED ||
			  f_pCodecConfig->ulDecodingRate == cOCT6100_G711_G727_4C_ENCODED ) &&
			  f_ulDecoderNumTssts != 2 )
			return cOCT6100_ERR_CHANNEL_MISSING_TSST;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteInputTsstControlMemory

Description:    This function configure a TSST control memory entry in internal memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usTsstIndex			TSST index within the TSST control memory.
f_usTsiMemIndex			TSI index within the TSI chariot memory.
f_ulTsstInputLaw		PCM law of the input TSST.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteInputTsstControlMemory
UINT32 Oct6100ApiWriteInputTsstControlMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT16							f_usTsstIndex,
				IN	UINT16							f_usTsiMemIndex,
				IN	UINT32							f_ulTsstInputLaw )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (f_usTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_TSST_CONTROL_MEM_INPUT_TSST;
	WriteParams.usWriteData |= f_usTsiMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK;

	/* Set the PCM law.*/
	WriteParams.usWriteData |= f_ulTsstInputLaw << cOCT6100_TSST_CONTROL_MEM_PCM_LAW_OFFSET;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteOutputTsstControlMemory

Description:    This function configure a TSST control memory entry in internal memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteOutputTsstControlMemory
UINT32 Oct6100ApiWriteOutputTsstControlMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT16							f_usTsstIndex,
				IN	UINT32							f_ulAdpcmNibblePosition,
				IN	UINT32							f_ulNumTssts,
				IN	UINT16							f_usTsiMemIndex )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( (f_usTsstIndex & cOCT6100_TSST_INDEX_MASK) * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_TSST_CONTROL_MEM_OUTPUT_TSST;
	WriteParams.usWriteData |= f_ulAdpcmNibblePosition << cOCT6100_TSST_CONTROL_MEM_NIBBLE_POS_OFFSET;
	WriteParams.usWriteData |= (f_ulNumTssts - 1) << cOCT6100_TSST_CONTROL_MEM_TSST_NUM_OFFSET;
	WriteParams.usWriteData |= f_usTsiMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteEncoderMemory

Description:    This function configure a Encoded memory entry in internal memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance					Pointer to API instance. This memory is used to keep
								the present state of the chip and all its resources.

f_ulEncoderIndex				Index of the encoder block within the ADPCM context memory.
f_ulCompType					Compression rate of the encoder.
f_usTsiMemIndex					TSI index within the TSI chariot memory used by the encoder.
f_ulEnableSilenceSuppression	Silence suppression enable flag.
f_ulAdpcmNibblePosition			ADPCM nibble position.
f_usPhasingTsstIndex			Phasing TSST index within the API's Phassing TSST list.
f_ulPhasingType					Type of the Phasing TSST.
f_ulPhase						Phase used with this encoder.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteEncoderMemory
UINT32 Oct6100ApiWriteEncoderMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT32							f_ulEncoderIndex,
				IN	UINT32							f_ulCompType,
				IN	UINT16							f_usTsiMemIndex,
				IN	UINT32							f_ulEnableSilenceSuppression,
				IN	UINT32							f_ulAdpcmNibblePosition,
				IN  UINT16							f_usPhasingTsstIndex,
				IN	UINT32							f_ulPhasingType,
				IN	UINT32							f_ulPhase )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/*==============================================================================*/
	/* Conversion Control Base */
	WriteParams.ulWriteAddress = cOCT6100_CONVERSION_CONTROL_MEM_BASE + ( f_ulEncoderIndex * cOCT6100_CONVERSION_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_CONVERSION_CONTROL_MEM_ENCODER;
	WriteParams.usWriteData |= f_ulCompType << cOCT6100_CONVERSION_CONTROL_MEM_COMP_OFFSET;
	WriteParams.usWriteData |= f_usTsiMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/*==============================================================================*/
	/* Conversion Control Base + 2 */
	WriteParams.ulWriteAddress += 2;

	/* Set the phasing TSST number.*/
	if ( f_usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
		WriteParams.usWriteData = (UINT16)( f_usPhasingTsstIndex << cOCT6100_CONVERSION_CONTROL_MEM_PHASE_OFFSET );
	else
		WriteParams.usWriteData = 0;
	
	/* Set the phasing type and the phase value if required.*/
	switch( f_ulPhasingType )
	{
	case cOCT6100_NO_PHASING:
		WriteParams.usWriteData |= 0x1 << 10;
		break;
	case cOCT6100_SINGLE_PHASING:
		WriteParams.usWriteData |= f_ulPhase;
		break;
	case cOCT6100_DUAL_PHASING:
		WriteParams.usWriteData |= 0x1 << 11;
		WriteParams.usWriteData |= f_ulPhase;
		break;
	default:
		/* No problem. */
		break;
	}

	/* Set the silence suppression flag.*/
	WriteParams.usWriteData |= f_ulEnableSilenceSuppression << cOCT6100_CONVERSION_CONTROL_MEM_SIL_SUP_OFFSET;

	/* Set the nibble position.*/
	WriteParams.usWriteData |= f_ulAdpcmNibblePosition << cOCT6100_CONVERSION_CONTROL_MEM_NIBBLE_POS_OFFSET;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/
	/* Conversion Control Base + 4 */
	WriteParams.ulWriteAddress += 2;
		
	/* Set the reset mode */
	WriteParams.usWriteData	= cOCT6100_CONVERSION_CONTROL_MEM_RST_ON_NEXT_FR;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/
	/* Conversion Control Base + 6 */
	WriteParams.ulWriteAddress += 2;
		
	/* Set the reset mode */
	WriteParams.usWriteData	= cOCT6100_CONVERSION_CONTROL_MEM_ACTIVATE_ENTRY;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteDecoderMemory

Description:    This function configure a Decoder memory entry in internal memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usDecoderIndex		Index of the decoder block within the ADPCM context memory.
f_ulCompType			Decompression rate of the decoder.
f_usTsiMemIndex			TSI index within the TSI chariot memory.
f_ulPcmLaw				PCM law of the decoded samples.
f_ulAdpcmNibblePosition	ADPCM nibble position.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteDecoderMemory
UINT32 Oct6100ApiWriteDecoderMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT16							f_usDecoderIndex,
				IN	UINT32							f_ulCompType,
				IN	UINT16							f_usTsiMemIndex,
				IN	UINT32							f_ulPcmLaw,
				IN	UINT32							f_ulAdpcmNibblePosition )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;


	/*==============================================================================*/
	/* Conversion Control Base */
	WriteParams.ulWriteAddress = cOCT6100_CONVERSION_CONTROL_MEM_BASE + ( f_usDecoderIndex * cOCT6100_CONVERSION_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_CONVERSION_CONTROL_MEM_DECODER;
	WriteParams.usWriteData |= f_ulCompType << cOCT6100_CONVERSION_CONTROL_MEM_COMP_OFFSET;
	WriteParams.usWriteData |= f_usTsiMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/*==============================================================================*/
	/* Conversion Control Base + 2 */
	WriteParams.ulWriteAddress += 2;

	/* Set the nibble position.*/
	WriteParams.usWriteData = (UINT16)( f_ulAdpcmNibblePosition << cOCT6100_CONVERSION_CONTROL_MEM_NIBBLE_POS_OFFSET );

	/* Set the law.*/
	WriteParams.usWriteData |= f_ulPcmLaw << cOCT6100_CONVERSION_CONTROL_MEM_LAW_OFFSET;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/
	/* Conversion Control Base + 4 */
	WriteParams.ulWriteAddress += 2;
		
	/* Set the reset mode */
	WriteParams.usWriteData	= cOCT6100_CONVERSION_CONTROL_MEM_RST_ON_NEXT_FR;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/
	/* Conversion Control Base + 6 */
	WriteParams.ulWriteAddress += 2;
		
	/* Set the reset mode */
	WriteParams.usWriteData	= cOCT6100_CONVERSION_CONTROL_MEM_ACTIVATE_ENTRY;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiClearConversionMemory

Description:    This function clears a conversion memory entry in internal 
				memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_usConversionMemIndex	Index of the block within the conversion memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiClearConversionMemory
UINT32 Oct6100ApiClearConversionMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT16							f_usConversionMemIndex )
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32						ulResult;
	UINT32						ulBaseAddress;
	UINT16						usReadData;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	WriteParams.usWriteData = 0;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/*==============================================================================*/
	/* Clear the entry */
	ulBaseAddress = cOCT6100_CONVERSION_CONTROL_MEM_BASE + ( f_usConversionMemIndex * cOCT6100_CONVERSION_CONTROL_MEM_ENTRY_SIZE );
	/* The "activate" bit at offset +6 must be cleared first. */
	WriteParams.ulWriteAddress = ulBaseAddress + 6;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/* Read at 0x200 to make sure there is no corruption on channel 0. */
	ReadParams.ulReadAddress = 0x200;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;	

	/* Then clear the rest of the structure. */
	WriteParams.ulWriteAddress = ulBaseAddress + 4;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress = ulBaseAddress + 2;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress = ulBaseAddress;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/*==============================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteVqeMemory

Description:    This function configure an echo memory entry in internal memory and
				external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pVqeConfig				Pointer to a VQE config structure.
f_pChannelOpen				Pointer to a channel configuration structure.
f_usChanIndex				Index of the echo channel in the API instance.
f_usEchoMemIndex			Index of the echo channel within the SSPX memory.
f_fClearPlayoutPointers		Flag indicating if the playout pointer should be cleared.
f_fModifyOnly				Flag indicating if the configuration should be
							modified only.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteVqeMemory
UINT32 Oct6100ApiWriteVqeMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_VQE		f_pVqeConfig,
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN	UINT16							f_usChanIndex,
				IN	UINT16							f_usEchoMemIndex,
				IN	BOOL							f_fClearPlayoutPointers,
				IN	BOOL							f_fModifyOnly )
{
	UINT32	ulResult;

	/* Write the NLP software configuration structure. */
	ulResult = Oct6100ApiWriteVqeNlpMemory(
							f_pApiInstance,
							f_pVqeConfig,
							f_pChannelOpen,
							f_usChanIndex,
							f_usEchoMemIndex,
							f_fClearPlayoutPointers,
							f_fModifyOnly );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write the AF software configuration structure. */
	ulResult = Oct6100ApiWriteVqeAfMemory(
							f_pApiInstance,
							f_pVqeConfig,
							f_pChannelOpen,
							f_usChanIndex,
							f_usEchoMemIndex,
							f_fClearPlayoutPointers,
							f_fModifyOnly );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

UINT32 oct6100_retrieve_nlp_conf_dword(tPOCT6100_INSTANCE_API f_pApiInst,
								tPOCT6100_API_CHANNEL f_pChanEntry,
								UINT32 f_ulAddress,
								UINT32 *f_pulConfigDword)
{
	tOCT6100_READ_PARAMS	_ReadParams;
	UINT16					_usReadData;
	UINT32 ulResult = cOCT6100_ERR_FATAL_8E;
	(*f_pulConfigDword) = cOCT6100_INVALID_VALUE;

	_ReadParams.pProcessContext = f_pApiInst->pProcessContext;
	mOCT6100_ASSIGN_USER_READ_WRITE_OBJ(f_pApiInst, _ReadParams);
	_ReadParams.ulUserChipId = f_pApiInst->pSharedInfo->ChipConfig.ulUserChipId;
	_ReadParams.pusReadData = &_usReadData;

	/* Read the first 16 bits.*/
	_ReadParams.ulReadAddress = f_ulAddress;
	mOCT6100_DRIVER_READ_API(_ReadParams, ulResult);
	if (ulResult == cOCT6100_ERR_OK) {
		/* Save data.*/
		(*f_pulConfigDword) = _usReadData << 16;

		/* Read the last 16 bits .*/
		_ReadParams.ulReadAddress += 2;
		mOCT6100_DRIVER_READ_API(_ReadParams, ulResult);
		if (ulResult == cOCT6100_ERR_OK) {
			/* Save data.*/
			(*f_pulConfigDword) |= _usReadData;
			ulResult = cOCT6100_ERR_OK;
		}
	}
	return ulResult;
}

UINT32 oct6100_save_nlp_conf_dword(tPOCT6100_INSTANCE_API f_pApiInst,
								tPOCT6100_API_CHANNEL f_pChanEntry,
								UINT32 f_ulAddress,
								UINT32 f_ulConfigDword)
{
	UINT32 ulResult;

	/* Write the config DWORD. */
	tOCT6100_WRITE_PARAMS	_WriteParams;

	_WriteParams.pProcessContext = f_pApiInst->pProcessContext;
	mOCT6100_ASSIGN_USER_READ_WRITE_OBJ(f_pApiInst, _WriteParams)
	_WriteParams.ulUserChipId = f_pApiInst->pSharedInfo->ChipConfig.ulUserChipId;

	/* Write the first 16 bits. */
	_WriteParams.ulWriteAddress = f_ulAddress;
	_WriteParams.usWriteData = (UINT16)((f_ulConfigDword >> 16) & 0xFFFF);
	mOCT6100_DRIVER_WRITE_API(_WriteParams, ulResult);

	if (ulResult == cOCT6100_ERR_OK) {
		/* Write the last word. */
		_WriteParams.ulWriteAddress = f_ulAddress + 2;
		_WriteParams.usWriteData = (UINT16)(f_ulConfigDword & 0xFFFF);
		mOCT6100_DRIVER_WRITE_API(_WriteParams, ulResult);
	}
	return ulResult;
}



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteVqeNlpMemory

Description:    This function configures the NLP related VQE features of an 
				echo channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pVqeConfig				Pointer to a VQE config structure.
f_pChannelOpen				Pointer to a channel configuration structure.
f_usChanIndex				Index of the echo channel in the API instance.
f_usEchoMemIndex			Index of the echo channel within the SSPX memory.
f_fClearPlayoutPointers		Flag indicating if the playout pointer should be cleared.
f_fModifyOnly				Flag indicating if the configuration should be
							modified only.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteVqeNlpMemory
UINT32 Oct6100ApiWriteVqeNlpMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_VQE		f_pVqeConfig,
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN	UINT16							f_usChanIndex,
				IN	UINT16							f_usEchoMemIndex,
				IN	BOOL							f_fClearPlayoutPointers,
				IN	BOOL							f_fModifyOnly )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_BUFFER_PLAYOUT_STOP	BufferPlayoutStop;
	UINT32							ulResult;
	UINT32							ulTempData;
	UINT32							ulNlpConfigBaseAddress;
	UINT32							ulFeatureBytesOffset;
	UINT32							ulFeatureBitOffset;
	UINT32							ulFeatureFieldLength;
	UINT32							ulMask;
	UINT16							usTempData;
	BOOL							fEchoOperationModeChanged;
	
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex );

	/*==============================================================================*/
	/*	Configure the CPU NLP configuration of the channel feature by feature.*/

	ulNlpConfigBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;
	
	/* Set initial value to zero.*/
	ulTempData = 0;

	/* Configure Adaptive Noise Reduction.*/
	if (pSharedInfo->ImageInfo.fAdaptiveNoiseReduction == TRUE) {
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE ) 
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->fSoutAdaptiveNoiseReduction != pChanEntry->VqeConfig.fSoutAdaptiveNoiseReduction ) 
					|| ( f_pVqeConfig->fSoutNoiseBleaching != pChanEntry->VqeConfig.fSoutNoiseBleaching )
					) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AdaptiveNoiseReductionOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AdaptiveNoiseReductionOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AdaptiveNoiseReductionOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set adaptive noise reduction on the SOUT port.*/
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fSoutAdaptiveNoiseReduction ) << ulFeatureBitOffset );

			/* If SOUT noise bleaching is requested, ANR must be activated. */
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fSoutNoiseBleaching ) << ulFeatureBitOffset );

			/* First read the DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Configure Rout Noise Reduction. */
	if (pSharedInfo->ImageInfo.fRoutNoiseReduction == TRUE) {
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fRoutNoiseReduction != pChanEntry->VqeConfig.fRoutNoiseReduction ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinAnrOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinAnrOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.RinAnrOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set noise reduction on the Rout port. */
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fRoutNoiseReduction ) << ulFeatureBitOffset );

			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	if (pSharedInfo->ImageInfo.fRoutNoiseReductionLevel == TRUE)
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( (f_pVqeConfig->lRoutNoiseReductionLevelGainDb != pChanEntry->VqeConfig.chRoutNoiseReductionLevelGainDb ) 
				   ||( f_pVqeConfig->fRoutNoiseReduction != pChanEntry->VqeConfig.fRoutNoiseReduction ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinAnrValOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinAnrValOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.RinAnrValOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			if (f_pVqeConfig->fRoutNoiseReduction == TRUE)
			{
				switch( f_pVqeConfig->lRoutNoiseReductionLevelGainDb)
				{
				case  0:	ulTempData |= ( 0 << ulFeatureBitOffset );
					break;
				case -6:	ulTempData |= ( 1 << ulFeatureBitOffset );
					break;
				case -12:	ulTempData |= ( 2 << ulFeatureBitOffset );
					break;
				case -18:	ulTempData |= ( 3 << ulFeatureBitOffset );
					break;
				default:	ulTempData |= ( 0 << ulFeatureBitOffset );
					break;
				}
			}
			else 
				ulTempData |= ( 0 << ulFeatureBitOffset );

			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}

	}

	/* Configure Sout ANR SNR enhancement. */
	if ( pSharedInfo->ImageInfo.fAnrSnrEnhancement == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->lAnrSnrEnhancementDb != pChanEntry->VqeConfig.chAnrSnrEnhancementDb ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AnrSnrEnhancementOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AnrSnrEnhancementOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AnrSnrEnhancementOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set ANR SNR enhancement on the Sout port. */
			switch( f_pVqeConfig->lAnrSnrEnhancementDb )
			{
			case -9:	ulTempData |= ( 7 << ulFeatureBitOffset );
				break;
			case -12:	ulTempData |= ( 6 << ulFeatureBitOffset );
				break;
			case -15:	ulTempData |= ( 5 << ulFeatureBitOffset );
				break;
			case -21:	ulTempData |= ( 3 << ulFeatureBitOffset );
				break;
			case -24:	ulTempData |= ( 2 << ulFeatureBitOffset );
				break;
			case -27:	ulTempData |= ( 1 << ulFeatureBitOffset );
				break;
			case -30:	ulTempData |= ( 0 << ulFeatureBitOffset );
				break;
			default:	ulTempData |= ( 4 << ulFeatureBitOffset );
				/* -18 */
				break;
			}

			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Configure Sout ANR voice-noise segregation. */
	if ( pSharedInfo->ImageInfo.fAnrVoiceNoiseSegregation == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->ulAnrVoiceNoiseSegregation != pChanEntry->VqeConfig.byAnrVoiceNoiseSegregation ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AnrVoiceNoiseSegregationOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AnrVoiceNoiseSegregationOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AnrVoiceNoiseSegregationOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set ANR voice-noise segregation on the Sout port. */
			ulTempData |= ( ( (UINT32)f_pVqeConfig->ulAnrVoiceNoiseSegregation ) << ulFeatureBitOffset );

			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Configure the tone disabler VQE activation delay. */
	if ( pSharedInfo->ImageInfo.fToneDisablerVqeActivationDelay == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->ulToneDisablerVqeActivationDelay != pChanEntry->VqeConfig.usToneDisablerVqeActivationDelay ) 
					|| ( f_pChannelOpen->fEnableToneDisabler != pChanEntry->fEnableToneDisabler ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.ToneDisablerVqeActivationDelayOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ToneDisablerVqeActivationDelayOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.ToneDisablerVqeActivationDelayOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set the tone disabler VQE activation delay.  The VQE activation delay */
			/* is only set if the tone disabler is activated. */
			if ( f_pChannelOpen->fEnableToneDisabler == TRUE )
				ulTempData |= ( ( (UINT32)( ( f_pVqeConfig->ulToneDisablerVqeActivationDelay - 300 ) / 512 ) ) << ulFeatureBitOffset );
			else
				ulTempData |= ( 0 ) << ulFeatureBitOffset;

			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Configure Conferencing Noise Reduction.*/
	if ( pSharedInfo->ImageInfo.fConferencingNoiseReduction == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->fSoutConferencingNoiseReduction != pChanEntry->VqeConfig.fSoutConferencingNoiseReduction ) 
					|| ( f_pVqeConfig->fSoutNoiseBleaching != pChanEntry->VqeConfig.fSoutNoiseBleaching ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.ConferencingNoiseReductionOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ConferencingNoiseReductionOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.ConferencingNoiseReductionOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set conferencing noise reduction on the SOUT port. */
			ulTempData |= (f_pVqeConfig->fSoutConferencingNoiseReduction << ulFeatureBitOffset );

			/* If SOUT noise bleaching is requested, CNR must be activated. */
			ulTempData |= (f_pVqeConfig->fSoutNoiseBleaching << ulFeatureBitOffset );

			/* Save the DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	
	/* Set the DC removal on RIN ports.*/
	if ( pSharedInfo->ImageInfo.fRinDcOffsetRemoval == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fRinDcOffsetRemoval != pChanEntry->VqeConfig.fRinDcOffsetRemoval ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinDcOffsetRemovalOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinDcOffsetRemovalOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.RinDcOffsetRemovalOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set adaptive noise reduction on the SOUT port.*/
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fRinDcOffsetRemoval ) << ulFeatureBitOffset );

			/* The write the new DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the DC removal on SIN ports.*/
	if ( pSharedInfo->ImageInfo.fSinDcOffsetRemoval == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fSinDcOffsetRemoval != pChanEntry->VqeConfig.fSinDcOffsetRemoval ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.SinDcOffsetRemovalOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.SinDcOffsetRemovalOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.SinDcOffsetRemovalOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set adaptive noise reduction on the SOUT port.*/
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fSinDcOffsetRemoval ) << ulFeatureBitOffset );

			/* Save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the level control. */
	if ( ( pChanEntry->byEchoOperationMode != f_pChannelOpen->ulEchoOperationMode )
		&& ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NORMAL ) )
		fEchoOperationModeChanged = TRUE;
	else
		fEchoOperationModeChanged = FALSE;

	/* If opening the channel, all level control configuration must be written. */
	if ( f_fModifyOnly == FALSE )
		fEchoOperationModeChanged = TRUE;
	ulResult = Oct6100ApiSetChannelLevelControl( f_pApiInstance, 
												 f_pVqeConfig, 
												 f_usChanIndex,
												 f_usEchoMemIndex,
												 fEchoOperationModeChanged );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the background noise freeze.*/
	if ( pSharedInfo->ImageInfo.fComfortNoise == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->ulComfortNoiseMode != pChanEntry->VqeConfig.byComfortNoiseMode ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.ComfortNoiseModeOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ComfortNoiseModeOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.ComfortNoiseModeOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= ( f_pVqeConfig->ulComfortNoiseMode << ulFeatureBitOffset );

			/* Save the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the state of the NLP */
	if ( pSharedInfo->ImageInfo.fNlpControl == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fEnableNlp != pChanEntry->VqeConfig.fEnableNlp ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.NlpControlFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.NlpControlFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.NlpControlFieldOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			if ( f_pVqeConfig->fEnableNlp == FALSE )
				ulTempData |= 0x1 << ulFeatureBitOffset;

			/* Save the new DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Set the tail configuration. */
	ulResult = Oct6100ApiSetChannelTailConfiguration(
												f_pApiInstance,
												f_pVqeConfig,
												f_usChanIndex,
												f_usEchoMemIndex,
												f_fModifyOnly );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the Default ERL. */
	if ( ( pSharedInfo->ImageInfo.fDefaultErl == TRUE ) && ( f_pVqeConfig->fAcousticEcho == FALSE ) )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->lDefaultErlDb != pChanEntry->VqeConfig.chDefaultErlDb ) 
					|| ( f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode ) 
					|| ( f_pVqeConfig->fAcousticEcho != pChanEntry->VqeConfig.fAcousticEcho ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.DefaultErlFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.DefaultErlFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.DefaultErlFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Convert the DB value to octasic's float format. (In energy) */
			if ( ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NO_ECHO )
				&& ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION ) )
			{
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lDefaultErlDb );
			}
			else
			{
				/* Clear the defautl ERL when using the no echo cancellation operation mode. */
				usTempData = 0x0;
			}

			if ( ulFeatureFieldLength < 16 )
				usTempData = (UINT16)( usTempData >> ( 16 - ulFeatureFieldLength ) );

			ulTempData |= ( usTempData << ulFeatureBitOffset );

			/* Save the new DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the Acoustic echo control.*/
	if ( pSharedInfo->ImageInfo.fAcousticEcho == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fAcousticEcho != pChanEntry->VqeConfig.fAcousticEcho ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AecFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AecFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AecFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field. */
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fAcousticEcho ) << ulFeatureBitOffset );

			/* Then save the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the Acoustic Echo Default ERL. */
	if ( ( pSharedInfo->ImageInfo.fAecDefaultErl == TRUE ) && ( f_pVqeConfig->fAcousticEcho == TRUE ) )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->lAecDefaultErlDb != pChanEntry->VqeConfig.chAecDefaultErlDb ) 
					|| ( f_pVqeConfig->fAcousticEcho != pChanEntry->VqeConfig.fAcousticEcho ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AecDefaultErlFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AecDefaultErlFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AecDefaultErlFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field. */
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			
			if ( ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NO_ECHO )
				&& ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION ) )
			{
				/* Convert the DB value to octasic's float format. (In energy) */
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lAecDefaultErlDb );
			}
			else
			{
				/* Clear the AEC defautl ERL when using the no echo cancellation operation mode. */
				usTempData = 0x0;
			}

			if ( ulFeatureFieldLength < 16 )
				usTempData = (UINT16)( usTempData >> ( 16 - ulFeatureFieldLength ) );

			ulTempData |= ( usTempData << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the DTMF tone removal bit.*/
	if ( pSharedInfo->ImageInfo.fToneRemoval == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fDtmfToneRemoval != pChanEntry->VqeConfig.fDtmfToneRemoval ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.ToneRemovalFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ToneRemovalFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.ToneRemovalFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fDtmfToneRemoval ) << ulFeatureBitOffset );

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}



	/* Set the non-linear behavior A.*/
	if ( pSharedInfo->ImageInfo.fNonLinearityBehaviorA == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->ulNonLinearityBehaviorA != pChanEntry->VqeConfig.byNonLinearityBehaviorA ) 
					|| ( f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode ) ) ) )
		{
			UINT16	ausLookupTable[ 14 ] = { 0x3663, 0x3906, 0x399C, 0x3A47, 0x3B06, 0x3B99, 0x3C47, 0x3D02, 0x3D99, 0x3E47, 0x3F00, 0x3F99, 0x4042, 0x4100 };
			
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.PcmLeakFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PcmLeakFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.PcmLeakFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			
			/*If we support ANR level the TLV is shared over 2 bits*/
			if (ulFeatureBitOffset == 18)
			{
				ulFeatureBitOffset -= 2;
				ausLookupTable[ f_pVqeConfig->ulNonLinearityBehaviorA ] &= 0xFFFC;
			}

			if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
				|| ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION ) )
				ulTempData |= ( 0x0 << ulFeatureBitOffset );
			else
				ulTempData |= ( ausLookupTable[ f_pVqeConfig->ulNonLinearityBehaviorA ] << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	
	/* Synch all the buffer playout field.*/
	if ( pSharedInfo->ImageInfo.fBufferPlayout == TRUE && f_fClearPlayoutPointers == TRUE )
	{	
		Oct6100BufferPlayoutStopDef( &BufferPlayoutStop );

		BufferPlayoutStop.ulChannelHndl = cOCT6100_INVALID_HANDLE;
		BufferPlayoutStop.fStopCleanly = TRUE;
		
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;
		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
														f_pApiInstance, 
														&BufferPlayoutStop, 
														f_usChanIndex, 
														f_usEchoMemIndex 

														);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		BufferPlayoutStop.ulPlayoutPort = cOCT6100_CHANNEL_PORT_SOUT;
		ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
														f_pApiInstance, 
														&BufferPlayoutStop, 
														f_usChanIndex, 
														f_usEchoMemIndex 

														);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*==============================================================================*/
	/* Write the 2100 Hz Echo Disabling mode */

	/* Check if the configuration has been changed. */
	if ( ( f_fModifyOnly == FALSE )
		|| ( ( f_fModifyOnly == TRUE ) 
			&& ( f_pChannelOpen->fEnableToneDisabler != pChanEntry->fEnableToneDisabler ) ) )
	{
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.ToneDisablerControlOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ToneDisablerControlOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.ToneDisablerControlOfst.byFieldSize;

		/* First read the DWORD where the field is located.*/
		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= (~ulMask);
		
		/* This is a disable bit, so it must be set only if the enable flag is set to false. */
		if ( f_pChannelOpen->fEnableToneDisabler == FALSE )
			ulTempData |= 0x1 << ulFeatureBitOffset;

		/* Save the DWORD where the field is located. */
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pChanEntry,
										ulNlpConfigBaseAddress + ulFeatureBytesOffset,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Write the Nlp Trivial enable flag. */

	/* Check if the configuration has been changed. */
	if ( ( f_fModifyOnly == FALSE )
		|| ( ( f_fModifyOnly == TRUE ) 
			&& ( 

				( f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode ) ) ) )
	{
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.NlpTrivialFieldOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.NlpTrivialFieldOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.NlpTrivialFieldOfst.byFieldSize;

		/* First read the DWORD where the field is located.*/
		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= (~ulMask);
		if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
			|| ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION ) )
		{
			ulTempData |= TRUE << ulFeatureBitOffset;
		}


		/* Then write the DWORD where the field is located. */
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pChanEntry,
										ulNlpConfigBaseAddress + ulFeatureBytesOffset,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Set the double talk behavior mode. */
	if ( pSharedInfo->ImageInfo.fDoubleTalkBehaviorFieldOfst == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->ulDoubleTalkBehavior != pChanEntry->VqeConfig.byDoubleTalkBehavior ) ) )
		{
			/* The field is located in the CPURO structure. */
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.DoubleTalkBehaviorFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.DoubleTalkBehaviorFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.DoubleTalkBehaviorFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= (f_pVqeConfig->ulDoubleTalkBehavior  << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}			
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Set the music protection enable. */
	if ( ( pSharedInfo->ImageInfo.fMusicProtection == TRUE )
		&& ( pSharedInfo->ImageInfo.fMusicProtectionConfiguration == TRUE ) )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fEnableMusicProtection != pChanEntry->VqeConfig.fEnableMusicProtection ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.MusicProtectionFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.MusicProtectionFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.MusicProtectionFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			if ( f_pVqeConfig->fEnableMusicProtection == TRUE )
				ulTempData |= ( 1 << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteVqeAfMemory

Description:    This function configures the AF related VQE features of an 
				echo channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_pVqeConfig				Pointer to a VQE config structure.
f_pChannelOpen				Pointer to a channel configuration structure.
f_usChanIndex				Index of the echo channel in the API instance.
f_usEchoMemIndex			Index of the echo channel within the SSPX memory.
f_fClearPlayoutPointers		Flag indicating if the playout pointer should be cleared.
f_fModifyOnly				Flag indicating if the configuration should be
							modified only.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteVqeAfMemory
UINT32 Oct6100ApiWriteVqeAfMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_VQE		f_pVqeConfig,
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN	UINT16							f_usChanIndex,
				IN	UINT16							f_usEchoMemIndex,
				IN	BOOL							f_fClearPlayoutPointers,
				IN	BOOL							f_fModifyOnly )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	UINT32							ulResult;
	UINT32							ulTempData;
	UINT32							ulAfConfigBaseAddress;
	UINT32							ulFeatureBytesOffset;
	UINT32							ulFeatureBitOffset;
	UINT32							ulFeatureFieldLength;
	UINT32							ulMask;
	UINT16							usTempData;
	
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex );

	/*==============================================================================*/
	/*	Write the AF CPU configuration of the channel feature by feature.*/

	/* Calculate AF CPU configuration base address. */
	ulAfConfigBaseAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst;
	
	/* Set initial value to zero.*/
	ulTempData = 0;

	/*==============================================================================*/
	/* Program the Maximum echo point within the Main channel memory.*/
	if ( pSharedInfo->ImageInfo.fMaxEchoPoint == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->lDefaultErlDb != pChanEntry->VqeConfig.chDefaultErlDb ) 
					|| ( f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode ) ) ) )
		{
			/* Write the echo tail length */
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.ChanMainIoMaxEchoPointOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.ChanMainIoMaxEchoPointOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.ChanMainIoMaxEchoPointOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Convert the DB value to octasic's float format.*/
			if ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_NO_ECHO )
			{
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lDefaultErlDb );
			}
			else
			{
				/* Clear max echo point.  No echo cancellation here. */
				usTempData = 0x0;
			}

			if ( ulFeatureFieldLength < 16 )
				usTempData = (UINT16)( usTempData >> ( 16 - ulFeatureFieldLength ) );

			ulTempData |= usTempData << ulFeatureBitOffset;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Set the non-linear behavior B.*/
	if ( pSharedInfo->ImageInfo.fNonLinearityBehaviorB == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->ulNonLinearityBehaviorB != pChanEntry->VqeConfig.byNonLinearityBehaviorB ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.NlpConvCapFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.NlpConvCapFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.NlpConvCapFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= (f_pVqeConfig->ulNonLinearityBehaviorB  << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/

	
	/*==============================================================================*/
	/* Set the listener enhancement feature. */
	if ( pSharedInfo->ImageInfo.fListenerEnhancement == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != pChanEntry->VqeConfig.bySoutAutomaticListenerEnhancementGainDb ) 
					|| ( f_pVqeConfig->fSoutNaturalListenerEnhancement != pChanEntry->VqeConfig.fSoutNaturalListenerEnhancement ) 
					|| ( f_pVqeConfig->ulSoutNaturalListenerEnhancementGainDb != pChanEntry->VqeConfig.bySoutNaturalListenerEnhancementGainDb ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AdaptiveAleOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AdaptiveAleOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AdaptiveAleOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );
			
			ulTempData &= (~ulMask);

			if ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != 0 )
			{
				UINT32 ulGainDb;

				ulGainDb = f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb / 3;

				/* Round up. */
				if ( ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb % 3 ) != 0x0 )
					ulGainDb ++;

				ulTempData |= ( ulGainDb << ulFeatureBitOffset );
			}
			else if ( f_pVqeConfig->fSoutNaturalListenerEnhancement != 0 )
			{
				UINT32 ulGainDb;

				ulGainDb = f_pVqeConfig->ulSoutNaturalListenerEnhancementGainDb / 3;

				/* Round up. */
				if ( ( f_pVqeConfig->ulSoutNaturalListenerEnhancementGainDb % 3 ) != 0x0 )
					ulGainDb ++;

				ulTempData |= ( ( 0x80 | ulGainDb ) << ulFeatureBitOffset );
			}

			/* Now write the DWORD where the field is located containing the new configuration. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Set the idle code detection enable. */
	if ( ( pSharedInfo->ImageInfo.fIdleCodeDetection == TRUE )
		&& ( pSharedInfo->ImageInfo.fIdleCodeDetectionConfiguration == TRUE ) )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->fIdleCodeDetection != pChanEntry->VqeConfig.fIdleCodeDetection ) ) )
		{
			/* Calculate base address in the AF software configuration. */
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.IdleCodeDetectionFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.IdleCodeDetectionFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.IdleCodeDetectionFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			if ( f_pVqeConfig->fIdleCodeDetection == FALSE )
				ulTempData |= ( 1 << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/


	/*==============================================================================*/
	/* Set the AFT control field. */
	if ( pSharedInfo->ImageInfo.fAftControl == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pChannelOpen->ulEchoOperationMode != pChanEntry->byEchoOperationMode ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AftControlOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AftControlOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AftControlOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfigBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			
			/* If the operation mode is no echo, set the field such that echo cancellation is disabled. */
			if ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
			{
				ulTempData |= ( 0x1234 << ulFeatureBitOffset );
			}
			else if ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION )
			{
				/* For clarity. */
				ulTempData |= ( 0x0 << ulFeatureBitOffset );
			}

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfigBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}
	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif




/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteEchoMemory

Description:    This function configure an echo memory entry in internal memory.and
				external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pTdmConfig			Pointer to a TDM config structure.
f_pChannelOpen			Pointer to a channel configuration structure.
f_usEchoIndex			Echo channel index within the SSPX memory.
f_usRinRoutTsiIndex		RIN/ROUT TSI index within the TSI chariot memory
f_usSinSoutTsiIndex		SIN/SOUT TSI index within the TSI chariot memory

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteEchoMemory
UINT32 Oct6100ApiWriteEchoMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_TDM		f_pTdmConfig,
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN	UINT16							f_usEchoIndex,
				IN	UINT16							f_usRinRoutTsiIndex,
				IN	UINT16							f_usSinSoutTsiIndex )

{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						ulResult;
	UINT32						ulTempData;
	UINT32						ulBaseAddress;
	UINT32						ulRinPcmLaw;
	UINT32						ulRoutPcmLaw;
	UINT32						ulSinPcmLaw;
	UINT32						ulSoutPcmLaw;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Set immediately the PCM law to be programmed in the SSPX and NLP memory.*/
	if ( f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_RIN )
	{
		ulRinPcmLaw		= f_pChannelOpen->TdmConfig.ulRoutPcmLaw;
		ulRoutPcmLaw	= f_pChannelOpen->TdmConfig.ulRoutPcmLaw;
		ulSinPcmLaw		= f_pChannelOpen->TdmConfig.ulSinPcmLaw;
		ulSoutPcmLaw	= f_pChannelOpen->TdmConfig.ulSinPcmLaw;
	}
	else /* f_pChannelOpen->CodecConfig.ulDecoderPort == cOCT6100_CHANNEL_PORT_SIN */
	{
		ulRinPcmLaw		= f_pChannelOpen->TdmConfig.ulRinPcmLaw;
		ulRoutPcmLaw	= f_pChannelOpen->TdmConfig.ulRinPcmLaw;
		ulSinPcmLaw		= f_pChannelOpen->TdmConfig.ulSoutPcmLaw;
		ulSoutPcmLaw	= f_pChannelOpen->TdmConfig.ulSoutPcmLaw;
	}

	/*==============================================================================*/
	/*	Configure the Global Static Configuration of the channel.*/

	ulBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usEchoIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + cOCT6100_CHANNEL_ROOT_GLOBAL_CONF_OFFSET;

	/* Set the PGSP context base address. */
	ulTempData = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + cOCT6100_CH_MAIN_PGSP_CONTEXT_OFFSET;
	
	WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_PGSP_CONTEXT_BASE_ADD_OFFSET;
	WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Set the PGSP init context base address. */
	ulTempData = ( cOCT6100_IMAGE_FILE_BASE + 0x200 ) & 0x07FFFFFF;
	
	WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_PGSP_INIT_CONTEXT_BASE_ADD_OFFSET;
	WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	/* Set the RIN circular buffer base address. */
	ulTempData  = ( pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainRinCBMemOfst) & 0x07FFFF00;
	ulTempData |= ( ulRoutPcmLaw << cOCT6100_GSC_BUFFER_LAW_OFFSET );
	
	/* Set the circular buffer size.*/
	if (( pSharedInfo->MemoryMap.ulChanMainRinCBMemSize & 0xFFFF00FF ) != 0 )
		return cOCT6100_ERR_CHANNEL_INVALID_RIN_CB_SIZE;
	ulTempData |= pSharedInfo->MemoryMap.ulChanMainRinCBMemSize >> 8;
		
	WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_RIN_CIRC_BUFFER_BASE_ADD_OFFSET;
	WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Set the SIN circular buffer base address. */
	ulTempData  = ( pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainSinCBMemOfst) & 0x07FFFF00;
	ulTempData |= ( ulSinPcmLaw << cOCT6100_GSC_BUFFER_LAW_OFFSET );

	WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_SIN_CIRC_BUFFER_BASE_ADD_OFFSET;
	WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Set the SOUT circular buffer base address. */
	ulTempData  = ( pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainSoutCBMemOfst ) & 0x07FFFF00;
	ulTempData |= ( ulSoutPcmLaw << cOCT6100_GSC_BUFFER_LAW_OFFSET );

	WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_SOUT_CIRC_BUFFER_BASE_ADD_OFFSET;
	WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/

	
	/*==============================================================================*/
	/*	ECHO SSPX Memory configuration.*/
	
	WriteParams.ulWriteAddress  = cOCT6100_ECHO_CONTROL_MEM_BASE + ( f_usEchoIndex * cOCT6100_ECHO_CONTROL_MEM_ENTRY_SIZE );

	/* ECHO memory BASE + 2 */
	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = 0x0000;

	/* Set the echo control field.*/
	if ( ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_NO_ECHO )
		|| ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_SPEECH_RECOGNITION ) )
	{
		WriteParams.usWriteData |= cOCT6100_ECHO_OP_MODE_NORMAL << cOCT6100_ECHO_CONTROL_MEM_AF_CONTROL;
	}
	else if ( f_pChannelOpen->ulEchoOperationMode != cOCT6100_ECHO_OP_MODE_EXTERNAL )
	{
		WriteParams.usWriteData |= f_pChannelOpen->ulEchoOperationMode << cOCT6100_ECHO_CONTROL_MEM_AF_CONTROL;
	}

	/* Set the SIN/SOUT law.*/
	WriteParams.usWriteData |= ulSinPcmLaw << cOCT6100_ECHO_CONTROL_MEM_INPUT_LAW_OFFSET;
	WriteParams.usWriteData |= ulSoutPcmLaw << cOCT6100_ECHO_CONTROL_MEM_OUTPUT_LAW_OFFSET;

	/* Set the TSI chariot memory field.*/
	WriteParams.usWriteData |= f_usSinSoutTsiIndex & cOCT6100_ECHO_CONTROL_MEM_TSI_MEM_MASK; 

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* ECHO memory BASE */
	WriteParams.ulWriteAddress -= 2;
	WriteParams.usWriteData  = cOCT6100_ECHO_CONTROL_MEM_ACTIVATE_ENTRY;

	/* Set the RIN/ROUT law.*/
	WriteParams.usWriteData |= ulRinPcmLaw << cOCT6100_ECHO_CONTROL_MEM_INPUT_LAW_OFFSET;
	WriteParams.usWriteData |= ulRoutPcmLaw << cOCT6100_ECHO_CONTROL_MEM_OUTPUT_LAW_OFFSET;

	/* Set the RIN external echo control bit.*/
	if ( f_pChannelOpen->ulEchoOperationMode == cOCT6100_ECHO_OP_MODE_EXTERNAL )
		WriteParams.usWriteData |= cOCT6100_ECHO_CONTROL_MEM_EXTERNAL_AF_CTRL;

	/* Set the TSI chariot memory field.*/
	WriteParams.usWriteData |= f_usRinRoutTsiIndex & cOCT6100_ECHO_CONTROL_MEM_TSI_MEM_MASK; 

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateOpenStruct

Description:    This function will copy the new parameter from the modify structure 
				into a channel open structure to be processed later by the same path 
				as the channel open function.  
				If a parameter is set to keep previous, it's current value will be 
				extracted from the channel entry in the API.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

IN		f_pApiInstance				Pointer to an API instance structure.
IN		f_pChanModify			Pointer to a channel modify structure.
IN OUT	f_pChanOpen				Pointer to a channel open structure.
IN		f_pChanEntry			Pointer to an API channel structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateOpenStruct
UINT32 Oct6100ApiUpdateOpenStruct( 
				IN		tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_CHANNEL_MODIFY		f_pChanModify,
				IN OUT	tPOCT6100_CHANNEL_OPEN			f_pChanOpen,
				IN		tPOCT6100_API_CHANNEL			f_pChanEntry )
{
	
	/* Check the generic Echo parameters.*/
	if ( f_pChanModify->ulEchoOperationMode == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->ulEchoOperationMode = f_pChanEntry->byEchoOperationMode;
	else
		f_pChanOpen->ulEchoOperationMode = f_pChanModify->ulEchoOperationMode;


	if ( f_pChanModify->fEnableToneDisabler == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->fEnableToneDisabler = f_pChanEntry->fEnableToneDisabler;
	else
		f_pChanOpen->fEnableToneDisabler = f_pChanModify->fEnableToneDisabler;


	if ( f_pChanModify->ulUserChanId == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->ulUserChanId = f_pChanEntry->ulUserChanId;
	else
		f_pChanOpen->ulUserChanId = f_pChanModify->ulUserChanId;


	
	/*======================================================================*/
	/* Now update the TDM config.*/
	/* Rin PCM LAW */
	if ( f_pChanModify->TdmConfig.ulRinPcmLaw == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRinPcmLaw = f_pChanEntry->TdmConfig.byRinPcmLaw;
	else
		f_pChanOpen->TdmConfig.ulRinPcmLaw = f_pChanModify->TdmConfig.ulRinPcmLaw;
	
	/* Sin PCM LAW */
	if ( f_pChanModify->TdmConfig.ulSinPcmLaw == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSinPcmLaw = f_pChanEntry->TdmConfig.bySinPcmLaw;
	else
		f_pChanOpen->TdmConfig.ulSinPcmLaw = f_pChanModify->TdmConfig.ulSinPcmLaw;
	
	/* Rout PCM LAW */
	if ( f_pChanModify->TdmConfig.ulRoutPcmLaw == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRoutPcmLaw = f_pChanEntry->TdmConfig.byRoutPcmLaw;
	else
		f_pChanOpen->TdmConfig.ulRoutPcmLaw = f_pChanModify->TdmConfig.ulRoutPcmLaw;

	/* Sout PCM LAW */
	if ( f_pChanModify->TdmConfig.ulSoutPcmLaw == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSoutPcmLaw = f_pChanEntry->TdmConfig.bySoutPcmLaw;
	else
		f_pChanOpen->TdmConfig.ulSoutPcmLaw = f_pChanModify->TdmConfig.ulSoutPcmLaw;


	/* Rin Timeslot */
	if ( f_pChanModify->TdmConfig.ulRinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRinTimeslot = f_pChanEntry->TdmConfig.usRinTimeslot;
	else
		f_pChanOpen->TdmConfig.ulRinTimeslot = f_pChanModify->TdmConfig.ulRinTimeslot;
	
	/* Rin Stream */
	if ( f_pChanModify->TdmConfig.ulRinStream == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRinStream = f_pChanEntry->TdmConfig.usRinStream;
	else
		f_pChanOpen->TdmConfig.ulRinStream = f_pChanModify->TdmConfig.ulRinStream;

	/* Rin Num TSSTs */
	if ( f_pChanModify->TdmConfig.ulRinNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRinNumTssts = f_pChanEntry->TdmConfig.byRinNumTssts;
	else
		f_pChanOpen->TdmConfig.ulRinNumTssts = f_pChanModify->TdmConfig.ulRinNumTssts;


	/* Sin Timeslot */
	if ( f_pChanModify->TdmConfig.ulSinTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSinTimeslot = f_pChanEntry->TdmConfig.usSinTimeslot;
	else
		f_pChanOpen->TdmConfig.ulSinTimeslot = f_pChanModify->TdmConfig.ulSinTimeslot;
	
	/* Sin Stream */
	if ( f_pChanModify->TdmConfig.ulSinStream == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSinStream = f_pChanEntry->TdmConfig.usSinStream;
	else
		f_pChanOpen->TdmConfig.ulSinStream = f_pChanModify->TdmConfig.ulSinStream;

	/* Sin Num TSSTs */
	if ( f_pChanModify->TdmConfig.ulSinNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSinNumTssts = f_pChanEntry->TdmConfig.bySinNumTssts;
	else
		f_pChanOpen->TdmConfig.ulSinNumTssts = f_pChanModify->TdmConfig.ulSinNumTssts;


	/* Rout Timeslot */
	if ( f_pChanModify->TdmConfig.ulRoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRoutTimeslot = f_pChanEntry->TdmConfig.usRoutTimeslot;
	else
		f_pChanOpen->TdmConfig.ulRoutTimeslot = f_pChanModify->TdmConfig.ulRoutTimeslot;
	
	/* Rout Stream */
	if ( f_pChanModify->TdmConfig.ulRoutStream == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRoutStream = f_pChanEntry->TdmConfig.usRoutStream;
	else
		f_pChanOpen->TdmConfig.ulRoutStream = f_pChanModify->TdmConfig.ulRoutStream;

	/* Rout Num TSSTs */
	if ( f_pChanModify->TdmConfig.ulRoutNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulRoutNumTssts = f_pChanEntry->TdmConfig.byRoutNumTssts;
	else
		f_pChanOpen->TdmConfig.ulRoutNumTssts = f_pChanModify->TdmConfig.ulRoutNumTssts;


	/* Sout Timeslot */
	if ( f_pChanModify->TdmConfig.ulSoutTimeslot == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSoutTimeslot = f_pChanEntry->TdmConfig.usSoutTimeslot;
	else
		f_pChanOpen->TdmConfig.ulSoutTimeslot = f_pChanModify->TdmConfig.ulSoutTimeslot;
	
	/* Sout Stream */
	if ( f_pChanModify->TdmConfig.ulSoutStream == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSoutStream = f_pChanEntry->TdmConfig.usSoutStream;
	else
		f_pChanOpen->TdmConfig.ulSoutStream = f_pChanModify->TdmConfig.ulSoutStream;

	/* Sout Num TSSTs */
	if ( f_pChanModify->TdmConfig.ulSoutNumTssts == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->TdmConfig.ulSoutNumTssts = f_pChanEntry->TdmConfig.bySoutNumTssts;
	else
		f_pChanOpen->TdmConfig.ulSoutNumTssts = f_pChanModify->TdmConfig.ulSoutNumTssts;

	/*======================================================================*/
	
	/*======================================================================*/
	/* Now update the VQE config.*/
	
	if ( f_pChanModify->VqeConfig.ulComfortNoiseMode == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulComfortNoiseMode = f_pChanEntry->VqeConfig.byComfortNoiseMode;
	else
		f_pChanOpen->VqeConfig.ulComfortNoiseMode = f_pChanModify->VqeConfig.ulComfortNoiseMode;

	if ( f_pChanModify->VqeConfig.fEnableNlp == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fEnableNlp = f_pChanEntry->VqeConfig.fEnableNlp;
	else
		f_pChanOpen->VqeConfig.fEnableNlp = f_pChanModify->VqeConfig.fEnableNlp;
	
	if ( f_pChanModify->VqeConfig.fEnableTailDisplacement == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fEnableTailDisplacement = f_pChanEntry->VqeConfig.fEnableTailDisplacement;
	else
		f_pChanOpen->VqeConfig.fEnableTailDisplacement = f_pChanModify->VqeConfig.fEnableTailDisplacement;

	if ( f_pChanModify->VqeConfig.ulTailDisplacement == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulTailDisplacement = f_pChanEntry->VqeConfig.usTailDisplacement;
	else
		f_pChanOpen->VqeConfig.ulTailDisplacement = f_pChanModify->VqeConfig.ulTailDisplacement;

	/* Tail length cannot be modifed. */
	f_pChanOpen->VqeConfig.ulTailLength = f_pChanEntry->VqeConfig.usTailLength;


	
	if ( f_pChanModify->VqeConfig.fRinDcOffsetRemoval == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fRinDcOffsetRemoval = f_pChanEntry->VqeConfig.fRinDcOffsetRemoval;
	else
		f_pChanOpen->VqeConfig.fRinDcOffsetRemoval = f_pChanModify->VqeConfig.fRinDcOffsetRemoval;
	

	if ( f_pChanModify->VqeConfig.fRinLevelControl == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fRinLevelControl = f_pChanEntry->VqeConfig.fRinLevelControl;
	else
		f_pChanOpen->VqeConfig.fRinLevelControl = f_pChanModify->VqeConfig.fRinLevelControl;


	if ( f_pChanModify->VqeConfig.fRinAutomaticLevelControl == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fRinAutomaticLevelControl = f_pChanEntry->VqeConfig.fRinAutomaticLevelControl;
	else
		f_pChanOpen->VqeConfig.fRinAutomaticLevelControl = f_pChanModify->VqeConfig.fRinAutomaticLevelControl;


	if ( f_pChanModify->VqeConfig.fRinHighLevelCompensation == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fRinHighLevelCompensation = f_pChanEntry->VqeConfig.fRinHighLevelCompensation;
	else
		f_pChanOpen->VqeConfig.fRinHighLevelCompensation = f_pChanModify->VqeConfig.fRinHighLevelCompensation;


	if ( f_pChanModify->VqeConfig.lRinHighLevelCompensationThresholdDb == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.lRinHighLevelCompensationThresholdDb = f_pChanEntry->VqeConfig.chRinHighLevelCompensationThresholdDb;
	else
		f_pChanOpen->VqeConfig.lRinHighLevelCompensationThresholdDb = f_pChanModify->VqeConfig.lRinHighLevelCompensationThresholdDb;

	
	if ( f_pChanModify->VqeConfig.fSinDcOffsetRemoval == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSinDcOffsetRemoval = f_pChanEntry->VqeConfig.fSinDcOffsetRemoval;
	else
		f_pChanOpen->VqeConfig.fSinDcOffsetRemoval = f_pChanModify->VqeConfig.fSinDcOffsetRemoval;
	

	if ( f_pChanModify->VqeConfig.fSoutAdaptiveNoiseReduction == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutAdaptiveNoiseReduction = f_pChanEntry->VqeConfig.fSoutAdaptiveNoiseReduction;
	else
		f_pChanOpen->VqeConfig.fSoutAdaptiveNoiseReduction = f_pChanModify->VqeConfig.fSoutAdaptiveNoiseReduction;

	
	if ( f_pChanModify->VqeConfig.fSoutConferencingNoiseReduction == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutConferencingNoiseReduction = f_pChanEntry->VqeConfig.fSoutConferencingNoiseReduction;
	else
		f_pChanOpen->VqeConfig.fSoutConferencingNoiseReduction = f_pChanModify->VqeConfig.fSoutConferencingNoiseReduction;


	if ( f_pChanModify->VqeConfig.fSoutNoiseBleaching == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutNoiseBleaching = f_pChanEntry->VqeConfig.fSoutNoiseBleaching;
	else
		f_pChanOpen->VqeConfig.fSoutNoiseBleaching = f_pChanModify->VqeConfig.fSoutNoiseBleaching;
	

	if ( f_pChanModify->VqeConfig.fSoutLevelControl == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutLevelControl = f_pChanEntry->VqeConfig.fSoutLevelControl;
	else
		f_pChanOpen->VqeConfig.fSoutLevelControl = f_pChanModify->VqeConfig.fSoutLevelControl;


	if ( f_pChanModify->VqeConfig.fSoutAutomaticLevelControl == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutAutomaticLevelControl = f_pChanEntry->VqeConfig.fSoutAutomaticLevelControl;
	else
		f_pChanOpen->VqeConfig.fSoutAutomaticLevelControl = f_pChanModify->VqeConfig.fSoutAutomaticLevelControl;


	if ( f_pChanModify->VqeConfig.lRinLevelControlGainDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lRinLevelControlGainDb = f_pChanEntry->VqeConfig.chRinLevelControlGainDb;
	else
		f_pChanOpen->VqeConfig.lRinLevelControlGainDb = f_pChanModify->VqeConfig.lRinLevelControlGainDb;
	

	if ( f_pChanModify->VqeConfig.lSoutLevelControlGainDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lSoutLevelControlGainDb = f_pChanEntry->VqeConfig.chSoutLevelControlGainDb;
	else
		f_pChanOpen->VqeConfig.lSoutLevelControlGainDb = f_pChanModify->VqeConfig.lSoutLevelControlGainDb;


	if ( f_pChanModify->VqeConfig.lRinAutomaticLevelControlTargetDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lRinAutomaticLevelControlTargetDb = f_pChanEntry->VqeConfig.chRinAutomaticLevelControlTargetDb;
	else
		f_pChanOpen->VqeConfig.lRinAutomaticLevelControlTargetDb = f_pChanModify->VqeConfig.lRinAutomaticLevelControlTargetDb;
	

	if ( f_pChanModify->VqeConfig.lSoutAutomaticLevelControlTargetDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lSoutAutomaticLevelControlTargetDb = f_pChanEntry->VqeConfig.chSoutAutomaticLevelControlTargetDb;
	else
		f_pChanOpen->VqeConfig.lSoutAutomaticLevelControlTargetDb = f_pChanModify->VqeConfig.lSoutAutomaticLevelControlTargetDb;


	if ( f_pChanModify->VqeConfig.lDefaultErlDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lDefaultErlDb = f_pChanEntry->VqeConfig.chDefaultErlDb;
	else
		f_pChanOpen->VqeConfig.lDefaultErlDb = f_pChanModify->VqeConfig.lDefaultErlDb;


	if ( f_pChanModify->VqeConfig.lAecDefaultErlDb == ( (INT32)cOCT6100_KEEP_PREVIOUS_SETTING ) )
		f_pChanOpen->VqeConfig.lAecDefaultErlDb = f_pChanEntry->VqeConfig.chAecDefaultErlDb;
	else
		f_pChanOpen->VqeConfig.lAecDefaultErlDb = f_pChanModify->VqeConfig.lAecDefaultErlDb;


	if ( f_pChanModify->VqeConfig.ulAecTailLength == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulAecTailLength = f_pChanEntry->VqeConfig.usAecTailLength;
	else
		f_pChanOpen->VqeConfig.ulAecTailLength = f_pChanModify->VqeConfig.ulAecTailLength;


	if ( f_pChanModify->VqeConfig.fAcousticEcho == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fAcousticEcho = f_pChanEntry->VqeConfig.fAcousticEcho;
	else
		f_pChanOpen->VqeConfig.fAcousticEcho = f_pChanModify->VqeConfig.fAcousticEcho;


	if ( f_pChanModify->VqeConfig.fDtmfToneRemoval == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fDtmfToneRemoval = f_pChanEntry->VqeConfig.fDtmfToneRemoval;
	else
		f_pChanOpen->VqeConfig.fDtmfToneRemoval = f_pChanModify->VqeConfig.fDtmfToneRemoval;





	if ( f_pChanModify->VqeConfig.ulNonLinearityBehaviorA == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulNonLinearityBehaviorA = f_pChanEntry->VqeConfig.byNonLinearityBehaviorA;
	else
		f_pChanOpen->VqeConfig.ulNonLinearityBehaviorA = f_pChanModify->VqeConfig.ulNonLinearityBehaviorA;


	if ( f_pChanModify->VqeConfig.ulNonLinearityBehaviorB == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulNonLinearityBehaviorB = f_pChanEntry->VqeConfig.byNonLinearityBehaviorB;
	else
		f_pChanOpen->VqeConfig.ulNonLinearityBehaviorB = f_pChanModify->VqeConfig.ulNonLinearityBehaviorB;


	if ( f_pChanModify->VqeConfig.ulDoubleTalkBehavior == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulDoubleTalkBehavior = f_pChanEntry->VqeConfig.byDoubleTalkBehavior;
	else
		f_pChanOpen->VqeConfig.ulDoubleTalkBehavior = f_pChanModify->VqeConfig.ulDoubleTalkBehavior;


	if ( f_pChanModify->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb = f_pChanEntry->VqeConfig.bySoutAutomaticListenerEnhancementGainDb;
	else
		f_pChanOpen->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb = f_pChanModify->VqeConfig.ulSoutAutomaticListenerEnhancementGainDb;


	if ( f_pChanModify->VqeConfig.ulSoutNaturalListenerEnhancementGainDb == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulSoutNaturalListenerEnhancementGainDb = f_pChanEntry->VqeConfig.bySoutNaturalListenerEnhancementGainDb;
	else
		f_pChanOpen->VqeConfig.ulSoutNaturalListenerEnhancementGainDb = f_pChanModify->VqeConfig.ulSoutNaturalListenerEnhancementGainDb;


	if ( f_pChanModify->VqeConfig.fSoutNaturalListenerEnhancement == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fSoutNaturalListenerEnhancement = f_pChanEntry->VqeConfig.fSoutNaturalListenerEnhancement;
	else
		f_pChanOpen->VqeConfig.fSoutNaturalListenerEnhancement = f_pChanModify->VqeConfig.fSoutNaturalListenerEnhancement;


	if ( f_pChanModify->VqeConfig.fRoutNoiseReduction == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fRoutNoiseReduction = f_pChanEntry->VqeConfig.fRoutNoiseReduction;
	else
		f_pChanOpen->VqeConfig.fRoutNoiseReduction = f_pChanModify->VqeConfig.fRoutNoiseReduction;

	if ( f_pChanModify->VqeConfig.lRoutNoiseReductionLevelGainDb == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.lRoutNoiseReductionLevelGainDb = f_pChanEntry->VqeConfig.chRoutNoiseReductionLevelGainDb;
	else
		f_pChanOpen->VqeConfig.lRoutNoiseReductionLevelGainDb = f_pChanModify->VqeConfig.lRoutNoiseReductionLevelGainDb;


	if ( f_pChanModify->VqeConfig.lAnrSnrEnhancementDb == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.lAnrSnrEnhancementDb = f_pChanEntry->VqeConfig.chAnrSnrEnhancementDb;
	else
		f_pChanOpen->VqeConfig.lAnrSnrEnhancementDb = f_pChanModify->VqeConfig.lAnrSnrEnhancementDb;


	if ( f_pChanModify->VqeConfig.ulAnrVoiceNoiseSegregation == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulAnrVoiceNoiseSegregation = f_pChanEntry->VqeConfig.byAnrVoiceNoiseSegregation;
	else
		f_pChanOpen->VqeConfig.ulAnrVoiceNoiseSegregation = f_pChanModify->VqeConfig.ulAnrVoiceNoiseSegregation;


	if ( f_pChanModify->VqeConfig.ulToneDisablerVqeActivationDelay == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.ulToneDisablerVqeActivationDelay = f_pChanEntry->VqeConfig.usToneDisablerVqeActivationDelay;
	else
		f_pChanOpen->VqeConfig.ulToneDisablerVqeActivationDelay = f_pChanModify->VqeConfig.ulToneDisablerVqeActivationDelay;


	if ( f_pChanModify->VqeConfig.fEnableMusicProtection == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fEnableMusicProtection = f_pChanEntry->VqeConfig.fEnableMusicProtection;
	else
		f_pChanOpen->VqeConfig.fEnableMusicProtection = f_pChanModify->VqeConfig.fEnableMusicProtection;


	if ( f_pChanModify->VqeConfig.fIdleCodeDetection == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->VqeConfig.fIdleCodeDetection = f_pChanEntry->VqeConfig.fIdleCodeDetection;
	else
		f_pChanOpen->VqeConfig.fIdleCodeDetection = f_pChanModify->VqeConfig.fIdleCodeDetection;

	/*======================================================================*/


	/*======================================================================*/
	/* Finaly the codec config.*/

	if ( f_pChanModify->CodecConfig.ulDecoderPort == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulDecoderPort = f_pChanEntry->CodecConfig.byDecoderPort;
	else
		f_pChanOpen->CodecConfig.ulDecoderPort = f_pChanModify->CodecConfig.ulDecoderPort;
	

	if ( f_pChanModify->CodecConfig.ulDecodingRate == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulDecodingRate = f_pChanEntry->CodecConfig.byDecodingRate;
	else
		f_pChanOpen->CodecConfig.ulDecodingRate = f_pChanModify->CodecConfig.ulDecodingRate;
	

	if ( f_pChanModify->CodecConfig.ulEncoderPort == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulEncoderPort = f_pChanEntry->CodecConfig.byEncoderPort;
	else
		f_pChanOpen->CodecConfig.ulEncoderPort = f_pChanModify->CodecConfig.ulEncoderPort;
	

	if ( f_pChanModify->CodecConfig.ulEncodingRate == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulEncodingRate = f_pChanEntry->CodecConfig.byEncodingRate;
	else
		f_pChanOpen->CodecConfig.ulEncodingRate = f_pChanModify->CodecConfig.ulEncodingRate;

	if ( f_pChanModify->CodecConfig.fEnableSilenceSuppression == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.fEnableSilenceSuppression = f_pChanEntry->CodecConfig.fEnableSilenceSuppression;
	else
		f_pChanOpen->CodecConfig.fEnableSilenceSuppression = f_pChanModify->CodecConfig.fEnableSilenceSuppression;

	if ( f_pChanModify->CodecConfig.ulPhasingType == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulPhasingType = f_pChanEntry->CodecConfig.byPhasingType;
	else
		f_pChanOpen->CodecConfig.ulPhasingType = f_pChanModify->CodecConfig.ulPhasingType;

	if ( f_pChanModify->CodecConfig.ulPhase == cOCT6100_KEEP_PREVIOUS_SETTING )
		f_pChanOpen->CodecConfig.ulPhase = f_pChanEntry->CodecConfig.byPhase;
	else
		f_pChanOpen->CodecConfig.ulPhase = f_pChanModify->CodecConfig.ulPhase;
	
	if ( f_pChanModify->CodecConfig.ulPhasingTsstHndl == cOCT6100_KEEP_PREVIOUS_SETTING )
	{
		if ( f_pChanEntry->usPhasingTsstIndex != cOCT6100_INVALID_INDEX )
		{
			tPOCT6100_API_PHASING_TSST	pPhasingEntry;

			mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingEntry, f_pChanEntry->usPhasingTsstIndex );

			/* Now recreate the Phasing TSST handle.*/
			f_pChanOpen->CodecConfig.ulPhasingTsstHndl = cOCT6100_HNDL_TAG_PHASING_TSST | (pPhasingEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_pChanEntry->usPhasingTsstIndex;
		}
		else
		{
			f_pChanOpen->CodecConfig.ulPhasingTsstHndl = cOCT6100_INVALID_HANDLE;
		}
	}
	else
	{
		f_pChanOpen->CodecConfig.ulPhasingTsstHndl		= f_pChanModify->CodecConfig.ulPhasingTsstHndl;
	}
	
	f_pChanOpen->CodecConfig.ulAdpcmNibblePosition	= f_pChanEntry->CodecConfig.byAdpcmNibblePosition;
	/*======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif







/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRetrieveNlpConfDword

Description:    This function is used by the API to store on a per channel basis
				the various confguration DWORD from the device. The API performs 
				less read to the chip that way since it is always in synch with the 
				chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChanEntry			Pointer to an API channel structure..
f_ulAddress				Address that needs to be modified..
f_pulConfigDword		Pointer to the content stored in the API located at the
						desired address.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRetrieveNlpConfDword
UINT32	Oct6100ApiRetrieveNlpConfDword( 
									   
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_API_CHANNEL				f_pChanEntry,
				IN		UINT32								f_ulAddress,
				OUT		PUINT32								f_pulConfigDword )
{
	UINT32	ulResult;
	UINT32	ulFirstEmptyIndex = 0xFFFFFFFF;
	UINT32	i;

	/* Search for the Dword.*/
	for ( i = 0; i < cOCT6100_MAX_NLP_CONF_DWORD; i++ )
	{
		if ( ( ulFirstEmptyIndex == 0xFFFFFFFF ) && ( f_pChanEntry->aulNlpConfDword[ i ][ 0 ] == 0x0 ) )
			ulFirstEmptyIndex = i;
		
		if ( f_pChanEntry->aulNlpConfDword[ i ][ 0 ] == f_ulAddress )
		{
			/* We found the matching Dword.*/
			*f_pulConfigDword = f_pChanEntry->aulNlpConfDword[ i ][ 1 ];
			return cOCT6100_ERR_OK;
		}
	}

	if ( i == cOCT6100_MAX_NLP_CONF_DWORD && ulFirstEmptyIndex == 0xFFFFFFFF )
		return cOCT6100_ERR_FATAL_8E;

	/* We did not found any entry, let's create a new entry.*/
	f_pChanEntry->aulNlpConfDword[ ulFirstEmptyIndex ][ 0 ] = f_ulAddress;


	/* Read the DWORD where the field is located.*/
	ulResult = Oct6100ApiReadDword( f_pApiInstance,
									f_ulAddress,
									f_pulConfigDword );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiSaveNlpConfDword

Description:    This function stores a configuration Dword within an API channel
				structure and then writes it into the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChanEntry			Pointer to an API channel structure..
f_ulAddress				Address that needs to be modified..
f_pulConfigDword		content to be stored in the API located at the
						desired address.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiSaveNlpConfDword
UINT32	Oct6100ApiSaveNlpConfDword( 
									   
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_API_CHANNEL				f_pChanEntry,
				IN		UINT32								f_ulAddress,
				IN		UINT32								f_ulConfigDword )
{
	UINT32	ulResult;
	UINT32	i;

	/* Search for the Dword.*/
	for ( i = 0; i < cOCT6100_MAX_NLP_CONF_DWORD; i++ )
	{

		if ( f_pChanEntry->aulNlpConfDword[ i ][ 0 ] == f_ulAddress )
		{
			/* We found the matching Dword.*/
			f_pChanEntry->aulNlpConfDword[ i ][ 1 ] = f_ulConfigDword;
			break;
		}
	}

	if ( i == cOCT6100_MAX_NLP_CONF_DWORD )
		return cOCT6100_ERR_FATAL_8F;


	/* Write the config DWORD.*/
	ulResult = Oct6100ApiWriteDword( f_pApiInstance,
									f_ulAddress,
									f_ulConfigDword );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelCreateBiDirSer

Description:    Creates a bidirectional echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelCreateBiDir	Pointer to a create bidirectionnal channel structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelCreateBiDirSer
UINT32 Oct6100ChannelCreateBiDirSer(
				IN tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT tPOCT6100_CHANNEL_CREATE_BIDIR		f_pChannelCreateBiDir )
{
	UINT16	usFirstChanIndex;
	UINT16	usFirstChanExtraTsiIndex;
	UINT16	usFirstChanSinCopyEventIndex;
	UINT16	usFirstChanSoutCopyEventIndex;
	UINT16	usSecondChanIndex;
	UINT16	usSecondChanExtraTsiIndex;
	UINT16	usSecondChanSinCopyEventIndex;
	UINT16	usSecondChanSoutCopyEventIndex;
	UINT16	usBiDirChanIndex;
	UINT32	ulResult;

	
	/* Check the user's configuration of the bidir channel for errors. */
	ulResult = Oct6100ApiCheckChannelCreateBiDirParams(		f_pApiInstance, 
															f_pChannelCreateBiDir, 
															&usFirstChanIndex, 
															&usFirstChanExtraTsiIndex, 
															&usFirstChanSinCopyEventIndex,
															&usSecondChanIndex, 
															&usSecondChanExtraTsiIndex,
															&usSecondChanSinCopyEventIndex 

															);
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Reserve all resources needed by the bidir channel. */
	ulResult = Oct6100ApiReserveChannelCreateBiDirResources(f_pApiInstance, 

															&usBiDirChanIndex, 
															&usFirstChanExtraTsiIndex,
															&usFirstChanSinCopyEventIndex,
															&usFirstChanSoutCopyEventIndex,
															&usSecondChanExtraTsiIndex,
															&usSecondChanSinCopyEventIndex,
															&usSecondChanSoutCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Write all necessary structures to activate the echo cancellation channel. */
	ulResult = Oct6100ApiWriteChannelCreateBiDirStructs(	f_pApiInstance, 

															usFirstChanIndex,
															usFirstChanExtraTsiIndex,
															usFirstChanSinCopyEventIndex,
															usFirstChanSoutCopyEventIndex,
															usSecondChanIndex, 
															usSecondChanExtraTsiIndex,
															usSecondChanSinCopyEventIndex,
															usSecondChanSoutCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Update the new echo cancellation channels's entry in the ECHO channel list. */
	ulResult = Oct6100ApiUpdateBiDirChannelEntry(			f_pApiInstance, 
															f_pChannelCreateBiDir,
															usBiDirChanIndex,
															usFirstChanIndex,
															usFirstChanExtraTsiIndex,
															usFirstChanSinCopyEventIndex,
															usFirstChanSoutCopyEventIndex,
															usSecondChanIndex, 
															usSecondChanExtraTsiIndex,
															usSecondChanSinCopyEventIndex,
															usSecondChanSoutCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif




/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChannelCreateBiDirParams

Description:    Checks the user's parameter passed to the create bidirectional channel
				function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pChannelCreateBiDir				Pointer to a create bidirectionnal channel structure.
f_pusFirstChanIndex					Pointer to the first channel index.
f_pusFirstChanExtraTsiIndex			Pointer to the first channel extra TSI index.
f_pusFirstChanSinCopyEventIndex		Pointer to the first channel Sin copy event index.
f_pusSecondChanIndex				Pointer to the second channel index.
f_pusSecondChanExtraTsiIndex		Pointer to the second channel extra TSI index.
f_pusSecondChanSinCopyEventIndex	Pointer to the second channel Sin copy event index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChannelCreateBiDirParams
UINT32 Oct6100ApiCheckChannelCreateBiDirParams(
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_CHANNEL_CREATE_BIDIR		f_pChannelCreateBiDir, 
				OUT	PUINT16								f_pusFirstChanIndex, 
				OUT	PUINT16								f_pusFirstChanExtraTsiIndex, 
				OUT	PUINT16								f_pusFirstChanSinCopyEventIndex,
				OUT	PUINT16								f_pusSecondChanIndex, 
				OUT	PUINT16								f_pusSecondChanExtraTsiIndex,
				OUT	PUINT16								f_pusSecondChanSinCopyEventIndex 

				)
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tPOCT6100_API_CHANNEL	pFirstChanEntry;
	tPOCT6100_API_CHANNEL	pSecondChanEntry;
	UINT32					ulEntryOpenCnt;
	BOOL					fCheckTssts = TRUE;

	/* Obtain shared resources pointer.*/
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* validate the bidirectional channel handle memory.*/
	if ( f_pChannelCreateBiDir->pulBiDirChannelHndl == NULL ) 
		return cOCT6100_ERR_CHANNEL_BIDIR_CHANNEL_HANDLE;


	
	/* Check if bi-dir channels are activated. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxBiDirChannels == 0 )
		return cOCT6100_ERR_CHANNEL_BIDIR_DISABLED;

	/*=======================================================================*/
	/* Verify the first channel handle. */

	if ( (f_pChannelCreateBiDir->ulFirstChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_BIDIR_FIRST_CHANNEL_HANDLE;

	*f_pusFirstChanIndex = (UINT16)( f_pChannelCreateBiDir->ulFirstChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusFirstChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_BIDIR_FIRST_CHANNEL_HANDLE;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pFirstChanEntry, *f_pusFirstChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelCreateBiDir->ulFirstChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pFirstChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pFirstChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_BIDIR_FIRST_CHANNEL_HANDLE;
	
	/* Check the specific state of the channel.*/
	if ( pFirstChanEntry->fRinRoutCodecActive == TRUE && pFirstChanEntry->CodecConfig.byEncoderPort != cOCT6100_CHANNEL_PORT_ROUT)
		return cOCT6100_ERR_CHANNEL_CODEC_ACTIVATED;
	if ( pFirstChanEntry->fSinSoutCodecActive == TRUE && pFirstChanEntry->CodecConfig.byEncoderPort != cOCT6100_CHANNEL_PORT_SOUT)
		return cOCT6100_ERR_CHANNEL_CODEC_ACTIVATED;
	if ( pFirstChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CHANNEL_ALREADY_BIDIR;
	
	if ( pFirstChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CHANNEL_FIRST_CHAN_IN_CONFERENCE;

	if ( fCheckTssts == TRUE )
	{
		if ( pFirstChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CHANNEL_FIRST_CHAN_SOUT_PORT;
		if ( pFirstChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CHANNEL_FIRST_CHAN_RIN_PORT;
	}

	/* Return the desired info.*/
	*f_pusFirstChanExtraTsiIndex		= pFirstChanEntry->usExtraSinTsiMemIndex;
	*f_pusFirstChanSinCopyEventIndex	= pFirstChanEntry->usSinCopyEventIndex;
	/*=======================================================================*/

	/*=======================================================================*/
	/* Verify the second channel handle. */

	if ( (f_pChannelCreateBiDir->ulSecondChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_BIDIR_SECOND_CHANNEL_HANDLE;

	*f_pusSecondChanIndex = (UINT16)( f_pChannelCreateBiDir->ulSecondChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusSecondChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_BIDIR_SECOND_CHANNEL_HANDLE;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSecondChanEntry, *f_pusSecondChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelCreateBiDir->ulSecondChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pSecondChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pSecondChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_BIDIR_SECOND_CHANNEL_HANDLE;

	/* Check the specific state of the channel.*/
	if ( pSecondChanEntry->fRinRoutCodecActive == TRUE && pSecondChanEntry->CodecConfig.byEncoderPort != cOCT6100_CHANNEL_PORT_ROUT)
		return cOCT6100_ERR_CHANNEL_CODEC_ACTIVATED;
	if ( pSecondChanEntry->fSinSoutCodecActive == TRUE && pSecondChanEntry->CodecConfig.byEncoderPort != cOCT6100_CHANNEL_PORT_SOUT)
	{

			return cOCT6100_ERR_CHANNEL_CODEC_ACTIVATED;
	}
		
	if ( pSecondChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CHANNEL_ALREADY_BIDIR;

	if ( fCheckTssts == TRUE )
	{
		if ( pSecondChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CHANNEL_SECOND_CHAN_SOUT_PORT;
		if ( pSecondChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CHANNEL_SECOND_CHAN_RIN_PORT;
	}

	if ( pSecondChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CHANNEL_SECOND_CHAN_IN_CONFERENCE;

	/* Return the desired info.*/
	*f_pusSecondChanExtraTsiIndex		= pSecondChanEntry->usExtraSinTsiMemIndex;
	*f_pusSecondChanSinCopyEventIndex	= pSecondChanEntry->usSinCopyEventIndex;
	/*=======================================================================*/

	/* Check the law compatibility.*/
	if ( pFirstChanEntry->TdmConfig.bySoutPcmLaw != pSecondChanEntry->TdmConfig.byRinPcmLaw ||
		 pFirstChanEntry->TdmConfig.byRinPcmLaw  != pSecondChanEntry->TdmConfig.bySoutPcmLaw )
		return cOCT6100_ERR_CHANNEL_BIDIR_PCM_LAW;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveChannelCreateBiDirResources

Description:    Reserves all resources needed for the new bidirectional channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pusBiDirChanIndex					Pointer to the index of the bidirectionnal channel within the API instance.
f_pusFirstChanExtraTsiIndex			Pointer to the first channel extra TSI index.
f_pusFirstChanSinCopyEventIndex		Pointer to the first channel Sin copy event index.
f_pusFirstChanSoutCopyEventIndex	Pointer to the first channel Sout copy event index.
f_pusSecondChanExtraTsiIndex		Pointer to the second channel extra TSI index.
f_pusSecondChanSinCopyEventIndex	Pointer to the second channel Sin copy event index.
f_pusSecondChanSoutCopyEventIndex	Pointer to the second channel Sout copy event index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveChannelCreateBiDirResources
UINT32 Oct6100ApiReserveChannelCreateBiDirResources(	
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,

				OUT		PUINT16								f_pusBiDirChanIndex, 
				IN OUT	PUINT16								f_pusFirstChanExtraTsiIndex, 
				IN OUT	PUINT16								f_pusFirstChanSinCopyEventIndex, 
				OUT		PUINT16								f_pusFirstChanSoutCopyEventIndex,
				IN OUT	PUINT16								f_pusSecondChanExtraTsiIndex, 
				IN OUT	PUINT16								f_pusSecondChanSinCopyEventIndex,
				OUT		PUINT16								f_pusSecondChanSoutCopyEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulTempVar;

	BOOL	fBiDirChanIndex			= FALSE;
	BOOL	fFirstExtraTsi			= FALSE;
	BOOL	fSecondExtraTsi			= FALSE;
	BOOL	fFirstSinCopyEvent		= FALSE;
	BOOL	fSecondSinCopyEvent		= FALSE;
	BOOL	fFirstSoutCopyEvent		= FALSE;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/*===============================================================================*/
	/* Verify and reserve the resources that might already by allocated. */


	{
		if ( *f_pusFirstChanExtraTsiIndex == cOCT6100_INVALID_INDEX )
		{
			/* Reserve the first Extra TSI memory entry */
			ulResult = Oct6100ApiReserveTsiMemEntry(	f_pApiInstance, 
														f_pusFirstChanExtraTsiIndex );
			if ( ulResult == cOCT6100_ERR_OK )
				fFirstExtraTsi = TRUE;
		}

		if ( *f_pusFirstChanSinCopyEventIndex == cOCT6100_INVALID_INDEX && ulResult == cOCT6100_ERR_OK )
		{
			/* Reserve the Sin copy event for the first channel.*/
			ulResult = Oct6100ApiReserveMixerEventEntry ( f_pApiInstance, 
														  f_pusFirstChanSinCopyEventIndex );
			if ( ulResult == cOCT6100_ERR_OK )
				fFirstSinCopyEvent = TRUE;
		}
	}

	if ( *f_pusSecondChanExtraTsiIndex == cOCT6100_INVALID_INDEX && ulResult == cOCT6100_ERR_OK )
	{
		/* Reserve the second Extra TSI memory entry */
		ulResult = Oct6100ApiReserveTsiMemEntry(	f_pApiInstance, 
													f_pusSecondChanExtraTsiIndex );
		if ( ulResult == cOCT6100_ERR_OK )
			fSecondExtraTsi = TRUE;
	}

	if ( *f_pusSecondChanSinCopyEventIndex == cOCT6100_INVALID_INDEX && ulResult == cOCT6100_ERR_OK )
	{
		/* Reserve the Sin copy event for the second channel.*/
		ulResult = Oct6100ApiReserveMixerEventEntry ( f_pApiInstance, 
													  f_pusSecondChanSinCopyEventIndex );
		if ( ulResult == cOCT6100_ERR_OK )
			fSecondSinCopyEvent = TRUE;
	}
	/*===============================================================================*/

	
	/*===============================================================================*/
	/* Now reserve all the resources specific to bidirectional channels */
	
	if ( ulResult == cOCT6100_ERR_OK )
	{
		ulResult = Oct6100ApiReserveBiDirChanEntry(  f_pApiInstance, 
												 f_pusBiDirChanIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			fBiDirChanIndex = TRUE;


			{

				/* Reserve the first channel Sout copy mixer event.*/
				ulResult = Oct6100ApiReserveMixerEventEntry ( f_pApiInstance, 
															  f_pusFirstChanSoutCopyEventIndex );
			}
			
			if ( ulResult == cOCT6100_ERR_OK )
			{
				fFirstSoutCopyEvent = TRUE;

				/* Reserve the second channel Sout copy mixer event.*/
				ulResult = Oct6100ApiReserveMixerEventEntry ( f_pApiInstance, 
															  f_pusSecondChanSoutCopyEventIndex );
			}
		}
	}

	/*===============================================================================*/


	/*===============================================================================*/
	/* Release the resources if something went wrong */		
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		/*===============================================================================*/
		/* Release the previously reserved echo resources .*/
		if ( fBiDirChanIndex == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseBiDirChanEntry( f_pApiInstance, 
														 *f_pusBiDirChanIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( fFirstExtraTsi == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance,
														*f_pusFirstChanExtraTsiIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( fSecondExtraTsi == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance,
														*f_pusSecondChanExtraTsiIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( fFirstSinCopyEvent == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseMixerEventEntry(  f_pApiInstance,
														   *f_pusFirstChanSinCopyEventIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( fSecondSinCopyEvent == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseMixerEventEntry(  f_pApiInstance,
														   *f_pusSecondChanSinCopyEventIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if ( ( fFirstSoutCopyEvent == TRUE )

			)
		{
			ulTempVar = Oct6100ApiReleaseMixerEventEntry(  f_pApiInstance,
														   *f_pusFirstChanSoutCopyEventIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		/*===============================================================================*/

		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteChannelCreateBiDirStructs

Description:    Performs all the required structure writes to configure the
				new echo cancellation channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance						Pointer to API instance. This memory is used to keep the
									present state of the chip and all its resources.

f_usFirstChanIndex					Pointer to the first channel index.
f_usFirstChanExtraTsiIndex			Pointer to the first channel extra TSI index.
f_usFirstChanSinCopyEventIndex		Pointer to the first channel Sin copy event index.
f_usFirstChanSoutCopyEventIndex		Pointer to the first channel Sout copy event index.
f_usFirstChanIndex					Pointer to the second channel index.
f_usSecondChanExtraTsiIndex			Pointer to the second channel extra TSI index.
f_usSecondChanSinCopyEventIndex		Pointer to the second channel Sin copy event index.
f_usSecondChanSoutCopyEventIndex	Pointer to the second channel Sout copy event index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteChannelCreateBiDirStructs
UINT32 Oct6100ApiWriteChannelCreateBiDirStructs(
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,

				IN	UINT16								f_usFirstChanIndex,
				IN	UINT16								f_usFirstChanExtraTsiIndex, 
				IN	UINT16								f_usFirstChanSinCopyEventIndex, 
				IN	UINT16								f_usFirstChanSoutCopyEventIndex,
				IN	UINT16								f_usSecondChanIndex,
				IN	UINT16								f_usSecondChanExtraTsiIndex, 
				IN	UINT16								f_usSecondChanSinCopyEventIndex,
				IN	UINT16								f_usSecondChanSoutCopyEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pFirstChanEntry;
	tPOCT6100_API_CHANNEL			pSecondChanEntry;

	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32	ulResult;
	

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	
	/*==============================================================================*/
	/* Get a pointer to the two channel entry.*/
	
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pFirstChanEntry, f_usFirstChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSecondChanEntry, f_usSecondChanIndex );




	{
		/*==============================================================================*/
		/* Configure the Tsst control memory and add the Sin copy event if necessary. */
		
		/*=======================================================================*/
		/* Program the Sin Copy event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usFirstChanSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
		WriteParams.usWriteData |= f_usFirstChanExtraTsiIndex;
		WriteParams.usWriteData |= pFirstChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( pFirstChanEntry->usSinSoutTsiMemIndex );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*=======================================================================*/

		/* Configure the TSST memory.*/
		if ( pFirstChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pFirstChanEntry->usSinTsstIndex,
															  f_usFirstChanExtraTsiIndex,
															  pFirstChanEntry->TdmConfig.bySinPcmLaw );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Now insert the event into the event list.*/
		ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
											f_usFirstChanSinCopyEventIndex,
											cOCT6100_EVENT_TYPE_SIN_COPY,
											f_usFirstChanIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*==============================================================================*/
	}



	/*==============================================================================*/
	/* Configure the Tsst control memory and add the Sin copy event if necessary.*/
	
	/*=======================================================================*/
	/* Program the Sin Copy event.*/
	WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSecondChanSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
	WriteParams.usWriteData |= f_usSecondChanExtraTsiIndex;
	WriteParams.usWriteData |= pSecondChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( pSecondChanEntry->usSinSoutTsiMemIndex );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/*=======================================================================*/

	/* Configure the TSST memory.*/
	if ( pSecondChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
	{

		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pSecondChanEntry->usSinTsstIndex,
															  f_usSecondChanExtraTsiIndex,
															  pSecondChanEntry->TdmConfig.bySinPcmLaw );
		}
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Now insert the event into the event list.*/
	ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
										f_usSecondChanSinCopyEventIndex,
										cOCT6100_EVENT_TYPE_SIN_COPY,
										f_usSecondChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/



	/*==============================================================================*/
	/* Now, let's configure the two Sout copy events.*/


		/* First event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usFirstChanSoutCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
		WriteParams.usWriteData |= pFirstChanEntry->usSinSoutTsiMemIndex;
		WriteParams.usWriteData |= pFirstChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = pSecondChanEntry->usRinRoutTsiMemIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
											f_usFirstChanSoutCopyEventIndex,
											cOCT6100_EVENT_TYPE_SOUT_COPY,
											f_usFirstChanIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;


	/* Second event.*/	
	WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSecondChanSoutCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
	WriteParams.usWriteData |= pSecondChanEntry->usSinSoutTsiMemIndex;
	WriteParams.usWriteData |= pSecondChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = pFirstChanEntry->usRinRoutTsiMemIndex;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	
	
	ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
										f_usSecondChanSoutCopyEventIndex,
										cOCT6100_EVENT_TYPE_SOUT_COPY,
										f_usSecondChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*==============================================================================*/


	/*==============================================================================*/
	/* Clear + release the silence events if they were created. */

	if ( pFirstChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX )
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pFirstChanEntry->usRinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pFirstChanEntry->usRinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_E0;

		pFirstChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	if ( ( pSecondChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX )

		)
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pSecondChanEntry->usRinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pSecondChanEntry->usRinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_E0;

		pSecondChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBiDirChannelEntry

Description:    Updates the new bidir channel and the channel used to create that channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBiDirChannelEntry
UINT32 Oct6100ApiUpdateBiDirChannelEntry(
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
				OUT	tPOCT6100_CHANNEL_CREATE_BIDIR		f_pChannelCreateBiDir,
				IN	UINT16								f_usBiDirChanIndex,
				IN	UINT16								f_usFirstChanIndex,
				IN	UINT16								f_usFirstChanExtraTsiIndex, 
				IN	UINT16								f_usFirstChanSinCopyEventIndex, 
				IN	UINT16								f_usFirstChanSoutCopyEventIndex,
				IN	UINT16								f_usSecondChanIndex,
				IN	UINT16								f_usSecondChanExtraTsiIndex, 
				IN	UINT16								f_usSecondChanSinCopyEventIndex,
				IN	UINT16								f_usSecondChanSoutCopyEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pFirstChanEntry;
	tPOCT6100_API_CHANNEL			pSecondChanEntry;
	tPOCT6100_API_BIDIR_CHANNEL		pBiDirChanEntry;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_BIDIR_CHANNEL_ENTRY_PNT( pSharedInfo, pBiDirChanEntry, f_usBiDirChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pFirstChanEntry, f_usFirstChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSecondChanEntry, f_usSecondChanIndex );
	
	/*=======================================================================*/
	/* Copy the channel's configuration and allocated resources. */

	pFirstChanEntry->usExtraSinTsiMemIndex	= f_usFirstChanExtraTsiIndex;
	pFirstChanEntry->usSinCopyEventIndex	= f_usFirstChanSinCopyEventIndex;
	pFirstChanEntry->usSoutCopyEventIndex	= f_usFirstChanSoutCopyEventIndex;

	pSecondChanEntry->usExtraSinTsiMemIndex	= f_usSecondChanExtraTsiIndex;
	pSecondChanEntry->usSinCopyEventIndex	= f_usSecondChanSinCopyEventIndex;
	pSecondChanEntry->usSoutCopyEventIndex	= f_usSecondChanSoutCopyEventIndex;

	/* Save the channel info in the bidir channel.*/
	pBiDirChanEntry->usFirstChanIndex = f_usFirstChanIndex;
	pBiDirChanEntry->usSecondChanIndex = f_usSecondChanIndex;



	/* Increment the extra TSI memory dependency count.*/

		pFirstChanEntry->usExtraSinTsiDependencyCnt++;
	pSecondChanEntry->usExtraSinTsiDependencyCnt++;

	/* Set the bidir flag in the channel structure.*/
	pFirstChanEntry->fBiDirChannel = TRUE;	
	pSecondChanEntry->fBiDirChannel = TRUE;

	/*=======================================================================*/
	
	/* Form handle returned to user. */
	*f_pChannelCreateBiDir->pulBiDirChannelHndl = cOCT6100_HNDL_TAG_BIDIR_CHANNEL | (pBiDirChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usBiDirChanIndex;

	/* Finally, mark the channel as open. */
	pBiDirChanEntry->fReserved = TRUE;
	
	/* Increment the number of channel open.*/
	f_pApiInstance->pSharedInfo->ChipStats.usNumberBiDirChannels++;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif





/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelDestroyBiDirSer

Description:    Closes a bidirectional channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelDestroyBiDir	Pointer to a destroy bidirectionnal channel structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelDestroyBiDirSer
UINT32 Oct6100ChannelDestroyBiDirSer(
				IN tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN tPOCT6100_CHANNEL_DESTROY_BIDIR		f_pChannelDestroyBiDir )
{
	UINT16	usBiDirChanIndex;
	UINT16	usFirstChanIndex;
	UINT16	usSecondChanIndex;


	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertDestroyBiDirChanParams(	f_pApiInstance, 
														f_pChannelDestroyBiDir, 
														&usBiDirChanIndex,

														&usFirstChanIndex,
														&usSecondChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiInvalidateBiDirChannelStructs( f_pApiInstance, 

														usFirstChanIndex,
														usSecondChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiReleaseBiDirChannelResources(	f_pApiInstance, 
														usBiDirChanIndex,

														usFirstChanIndex,
														usSecondChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Invalidate the handle.*/
	f_pChannelDestroyBiDir->ulBiDirChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertDestroyBiDirChanParams

Description:    Validate the handle given by the user and verify the state of 
				the channel about to be closed.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChannelDestroyBiDir	Pointer to a destroy bidirectional channel structure.
f_pusBiDirChanIndex		Pointer to the bidir channel entry within the API's list.
f_pusFirstChanIndex		Pointer to the first channel index part of the bidir channel.
f_pusFirstChanIndex		Pointer to the second channel index part of the bidir channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertDestroyBiDirChanParams
UINT32 Oct6100ApiAssertDestroyBiDirChanParams( 
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CHANNEL_DESTROY_BIDIR		f_pChannelDestroyBiDir,
				IN OUT	PUINT16								f_pusBiDirChanIndex,

				IN OUT	PUINT16								f_pusFirstChanIndex,
				IN OUT	PUINT16								f_pusSecondChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_BIDIR_CHANNEL		pBiDirChanEntry;
	UINT32							ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pChannelDestroyBiDir->ulBiDirChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_BIDIR_CHANNEL )
		return cOCT6100_ERR_CHANNEL_BIDIR_CHANNEL_HANDLE;

	*f_pusBiDirChanIndex = (UINT16)( f_pChannelDestroyBiDir->ulBiDirChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusBiDirChanIndex  >= pSharedInfo->ChipConfig.usMaxBiDirChannels )
		return cOCT6100_ERR_CHANNEL_BIDIR_CHANNEL_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the bidir channel's list entry. */

	mOCT6100_GET_BIDIR_CHANNEL_ENTRY_PNT( pSharedInfo, pBiDirChanEntry, *f_pusBiDirChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelDestroyBiDir->ulBiDirChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pBiDirChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_BIDIR_CHAN_NOT_OPEN;
	if ( ulEntryOpenCnt != pBiDirChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_BIDIR_CHANNEL_HANDLE;

	/*=======================================================================*/
	
	/* Return the index of the channel used to create this bidirectional channel.*/
	*f_pusFirstChanIndex = pBiDirChanEntry->usFirstChanIndex;
	*f_pusSecondChanIndex = pBiDirChanEntry->usSecondChanIndex;



	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateBiDirChannelStructs

Description:	Destroy the link between the two channels.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateBiDirChannelStructs
UINT32 Oct6100ApiInvalidateBiDirChannelStructs( 
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,

				IN		UINT16								f_usFirstChanIndex,
				IN		UINT16								f_usSecondChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pFirstChanEntry;
	tPOCT6100_API_CHANNEL			pSecondChanEntry;

	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32	ulResult;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get pointers to the API entry of the two channel used to create the bidir channel.*/
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pFirstChanEntry,  f_usFirstChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSecondChanEntry, f_usSecondChanIndex );

	/* Clear the SIN copy event of the first channel and release the Extra TSI memory if 
	  this feature was the only one using it. */

	{
		if ( pFirstChanEntry->usExtraSinTsiDependencyCnt == 1 )
		{
			/*=======================================================================*/
			/* Clear the Sin Copy event.*/
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pFirstChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;

			/*=======================================================================*/

			/* Configure the TSST memory.*/
			if ( pFirstChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																  pFirstChanEntry->usSinTsstIndex,
																  pFirstChanEntry->usSinSoutTsiMemIndex,
																  pFirstChanEntry->TdmConfig.bySinPcmLaw );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			/* Remove the event from the list.*/
			ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
													pFirstChanEntry->usSinCopyEventIndex,
													cOCT6100_EVENT_TYPE_SIN_COPY );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

		}
	}

	/* Clear the SIN copy event of the first channel and release the Extra TSI memory if 
	  this feature was the only one using it. */
	if ( pSecondChanEntry->usExtraSinTsiDependencyCnt == 1 )
	{
		/*=======================================================================*/
		/* Clear the Sin Copy event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSecondChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*=======================================================================*/

		/* Configure the TSST memory.*/
		if ( pSecondChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pSecondChanEntry->usSinTsstIndex,
															  pSecondChanEntry->usSinSoutTsiMemIndex,
															  pSecondChanEntry->TdmConfig.bySinPcmLaw );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pSecondChanEntry->usSinCopyEventIndex,
												cOCT6100_EVENT_TYPE_SIN_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

	}

	/* Now remove the sout copy of the first channel.*/


	{
		/*=======================================================================*/
		/* Clear the Sout Copy event of the first channel.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pFirstChanEntry->usSoutCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
		/*=======================================================================*/

		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pFirstChanEntry->usSoutCopyEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}



	/* Now remove the sout copy of the second channel.*/

	/*=======================================================================*/
	/* Clear the Sout Copy event of the second channel.*/
	WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSecondChanEntry->usSoutCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;
	/*=======================================================================*/

	/* Remove the event from the list.*/
	ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
											pSecondChanEntry->usSoutCopyEventIndex,
											cOCT6100_EVENT_TYPE_SOUT_COPY );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;



	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBiDirChannelResources

Description:	Release and clear the API entry associated to the bidirectional channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usBiDirChanIndex		Index of the bidirectionnal channel in the API's bidir channel list.
f_usFirstChanIndex		Index of the first channel used to create the bidir channel.
f_usSecondChanIndex		Index of the second channel used to create the bidir channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBiDirChannelResources
UINT32 Oct6100ApiReleaseBiDirChannelResources( 
				IN		tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usBiDirChanIndex,

				IN		UINT16								f_usFirstChanIndex,
				IN		UINT16								f_usSecondChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_BIDIR_CHANNEL		pBiDirChanEntry;
	tPOCT6100_API_CHANNEL			pFirstChanEntry;
	tPOCT6100_API_CHANNEL			pSecondChanEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEventEntry;
	UINT32	ulResult;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_BIDIR_CHANNEL_ENTRY_PNT( pSharedInfo, pBiDirChanEntry, f_usBiDirChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pFirstChanEntry,  f_usFirstChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSecondChanEntry, f_usSecondChanIndex );

	/* Release the bidir entry.*/
	ulResult = Oct6100ApiReleaseBiDirChanEntry( f_pApiInstance, f_usBiDirChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return cOCT6100_ERR_FATAL_AC;
	
	/* Release the Extra TSI memory and the SIN copy event if required.*/

	{
		if ( pFirstChanEntry->usExtraSinTsiDependencyCnt == 1 )
		{
			/* Release the two TSI chariot memory entries.*/
			ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pFirstChanEntry->usExtraSinTsiMemIndex );
			if ( ulResult != cOCT6100_ERR_OK  )
				return cOCT6100_ERR_FATAL_A3;

			/* Relese the SIN copy event.*/
			ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pFirstChanEntry->usSinCopyEventIndex );
			if ( ulResult != cOCT6100_ERR_OK  )
				return cOCT6100_ERR_FATAL_A4;

			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, pFirstChanEntry->usSinCopyEventIndex );

			/* Invalidate the entry.*/
			pTempEventEntry->fReserved		= FALSE;
			pTempEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
			pTempEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

			pFirstChanEntry->usExtraSinTsiDependencyCnt--;
			pFirstChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
			pFirstChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;
		}
		else
		{
			pFirstChanEntry->usExtraSinTsiDependencyCnt--;
		}
	}

	if ( pSecondChanEntry->usExtraSinTsiDependencyCnt == 1 )
	{
		/* Release the two TSI chariot memory entries.*/
		ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pSecondChanEntry->usExtraSinTsiMemIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_A5;

		/* Relese the SIN copy event.*/
		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pSecondChanEntry->usSinCopyEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_A6;

		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, pSecondChanEntry->usSinCopyEventIndex );
		/* Invalidate the entry.*/
		pTempEventEntry->fReserved		= FALSE;
		pTempEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
		pTempEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

		pSecondChanEntry->usExtraSinTsiDependencyCnt--;
		pSecondChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
		pSecondChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;
	}
	else
	{
		pSecondChanEntry->usExtraSinTsiDependencyCnt--;
	}


	{
		/* Release the SOUT copy event of the first channel.*/
		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pFirstChanEntry->usSoutCopyEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_A7;

		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, pFirstChanEntry->usSoutCopyEventIndex );
		/* Invalidate the entry.*/
		pTempEventEntry->fReserved		= FALSE;
		pTempEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
		pTempEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;
	}

	/* Release the SOUT copy event of the second channel.*/
	ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pSecondChanEntry->usSoutCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return cOCT6100_ERR_FATAL_A8;

	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, pSecondChanEntry->usSoutCopyEventIndex );
	/* Invalidate the entry.*/
	pTempEventEntry->fReserved		= FALSE;
	pTempEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
	pTempEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;


	/*=======================================================================*/
	/* Update the first channel's list entry. */

	/* Mark the channel as closed. */
	pFirstChanEntry->usSoutCopyEventIndex = cOCT6100_INVALID_INDEX;
	pFirstChanEntry->fBiDirChannel = FALSE;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Update the second channel's list entry. */

	/* Mark the channel as closed. */

	pSecondChanEntry->usSoutCopyEventIndex = cOCT6100_INVALID_INDEX;
	pSecondChanEntry->fBiDirChannel = FALSE;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Update the bidirectional channel's list entry. */

	/* Mark the channel as closed. */
	pBiDirChanEntry->fReserved = FALSE;
	pBiDirChanEntry->byEntryOpenCnt++;

	pBiDirChanEntry->usFirstChanIndex = cOCT6100_INVALID_INDEX;
	pBiDirChanEntry->usSecondChanIndex = cOCT6100_INVALID_INDEX;
	
	/* Decrement the number of channel open.*/
	f_pApiInstance->pSharedInfo->ChipStats.usNumberBiDirChannels--;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Check if some of the ports must be muted back. */
	
	ulResult = Oct6100ApiMutePorts( f_pApiInstance,
									f_usFirstChanIndex,
									pFirstChanEntry->usRinTsstIndex,
									pFirstChanEntry->usSinTsstIndex,
									FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiMutePorts( f_pApiInstance,
									f_usSecondChanIndex,
									pSecondChanEntry->usRinTsstIndex,
									pSecondChanEntry->usSinTsstIndex,
									FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ApiOctFloatToDbEnergyByte
INT32 Oct6100ApiOctFloatToDbEnergyByte(UINT8 x)
{
	INT32	lResult;

	lResult = Oct6100ApiOctFloatToDbEnergyHalf( (UINT16)(x << 8) );
	return lResult;
}
#endif

#if !SKIP_Oct6100ApiOctFloatToDbEnergyHalf
INT32 Oct6100ApiOctFloatToDbEnergyHalf(UINT16 x)
{
	INT32 y;
	UINT16 m;

	y = (((x >> 8) & 0x7F) - 0x41) * 3;

	m = (UINT16)((x & 0x00E0) >> 5);
	if (m < 2) y += 0;
	else if (m < 5) y += 1;
	else y += 2;

	return y;
}
#endif

#if !SKIP_Oct6100ApiDbAmpHalfToOctFloat
UINT16 Oct6100ApiDbAmpHalfToOctFloat(INT32 x)
{
	INT32 db_div6;
	INT32 db_mod6;
	UINT16 rval;
	INT32 x_unsigned;

	if(x < 0)
	{
		x_unsigned = -x;
	}
	else
	{
		x_unsigned = x;
	}

	db_div6 = x_unsigned / 6;
	db_mod6 = x_unsigned % 6;

	if(x < 0)
	{
		if(db_mod6 == 0)
		{
			/* Change nothing! */
			db_div6 = -db_div6;
		}
		else
		{
			/* When we are negative, round down, and then adjust modulo. For example, if
			 x is -1, then db_div6 is 0 and db_mod6 is 1. We adjust so db_div6 = -1 and
			 db_mod6 = 5, which gives the correct adjustment. */
			db_div6 = -db_div6-1;
			db_mod6 = 6 - db_mod6;
		}
	}

	rval = (UINT16)(0x4100 + db_div6 * 0x100);

	if(db_mod6 == 0)
	{
		rval += 0x0000;
	}
	else if(db_mod6 == 1)
	{
		rval += 0x0020;
	}		
	else if(db_mod6 == 2)
	{
		rval += 0x0040;
	}		
	else if(db_mod6 == 3)
	{
		rval += 0x0070;
	}		
	else if(db_mod6 == 4)
	{
		rval += 0x0090;
	}		
	else /* if(db_mod6 == 5) */
	{
		rval += 0x00D0;
	}

	return rval;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteDebugChanMemory

Description:    This function configure a debug channel echo memory entry 
				in internal memory.and external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pTdmConfig			Pointer to a TDM configuration structure.
f_pVqeConfig			Pointer to a VQE configuration structure.
f_pChannelOpen			Pointer to a channel configuration structure.
f_usChanIndex			Index of the echo channel in the API instance.
f_usEchoMemIndex		Index of the echo channel within the SSPX memory.
f_usRinRoutTsiIndex		RIN/ROUT TSI index within the TSI chariot memory.
f_usSinSoutTsiIndex		SIN/SOUT TSI index within the TSI chariot memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteDebugChanMemory
UINT32 Oct6100ApiWriteDebugChanMemory( 
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_TDM		f_pTdmConfig,
				IN	tPOCT6100_CHANNEL_OPEN_VQE		f_pVqeConfig,
				IN	tPOCT6100_CHANNEL_OPEN			f_pChannelOpen,
				IN	UINT16							f_usChanIndex,
				IN	UINT16							f_usEchoMemIndex,
				IN	UINT16							f_usRinRoutTsiIndex,
				IN	UINT16							f_usSinSoutTsiIndex )
{
	UINT32					ulResult;

	/*==============================================================================*/
	/* Write the VQE configuration of the debug channel. */

	ulResult = Oct6100ApiWriteVqeMemory( 
										f_pApiInstance, 
										f_pVqeConfig, 
										f_pChannelOpen, 
										f_usChanIndex,
										f_usEchoMemIndex,
										TRUE, 
										FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	/*==============================================================================*/


	/*==============================================================================*/

	/* Write the echo memory configuration of the debug channel. */
	ulResult = Oct6100ApiWriteEchoMemory(
										f_pApiInstance,
										f_pTdmConfig,
										f_pChannelOpen,
										f_usEchoMemIndex,
										f_usRinRoutTsiIndex,
										f_usSinSoutTsiIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiDebugChannelOpen

Description:    Internal function used to open a debug channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiDebugChannelOpen
UINT32	Oct6100ApiDebugChannelOpen( 
					IN	tPOCT6100_INSTANCE_API f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_CHANNEL_OPEN		TempChanOpen;
	
	UINT32	ulResult;
	UINT16	usChanIndex;
	UINT16	usDummyEchoIndex;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Let's program the channel memory.*/
	Oct6100ChannelOpenDef( &TempChanOpen );
	
	TempChanOpen.ulEchoOperationMode = cOCT6100_ECHO_OP_MODE_HT_RESET;	/* Activate the channel in reset.*/
	TempChanOpen.VqeConfig.fEnableNlp = FALSE;
	TempChanOpen.VqeConfig.ulComfortNoiseMode = cOCT6100_COMFORT_NOISE_NORMAL;
	TempChanOpen.VqeConfig.fSinDcOffsetRemoval = FALSE;
	TempChanOpen.VqeConfig.fRinDcOffsetRemoval = FALSE;
	TempChanOpen.VqeConfig.lDefaultErlDb = 0;
	
	/* Loop to reserve the proper entry for the debug channel */
	for( usChanIndex = 0; usChanIndex < ( pSharedInfo->DebugInfo.usRecordChanIndex + 1 ); usChanIndex ++ )
	{
		ulResult = Oct6100ApiReserveEchoEntry( f_pApiInstance, &usDummyEchoIndex );
		if( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Loop to free all entries except the one for the debug channel */
	for( usChanIndex = pSharedInfo->DebugInfo.usRecordChanIndex; usChanIndex > 0; ) 
	{
		usChanIndex--;
		ulResult = Oct6100ApiReleaseEchoEntry( f_pApiInstance, usChanIndex );
		if( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	ulResult = Oct6100ApiWriteDebugChanMemory( f_pApiInstance,
										  &TempChanOpen.TdmConfig,
										  &TempChanOpen.VqeConfig,
										  &TempChanOpen,
										  pSharedInfo->DebugInfo.usRecordChanIndex,
										  pSharedInfo->DebugInfo.usRecordMemIndex,
										  pSharedInfo->DebugInfo.usRecordRinRoutTsiMemIndex,
										  pSharedInfo->DebugInfo.usRecordSinSoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMuteChannelPort

Description:	This function will verify if a input TSST is bound to the RIN and
				SIN port. If not, the port will be muted.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMutePorts
UINT32	Oct6100ApiMutePorts( 
					IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
					IN	UINT16						f_usEchoIndex,
					IN	UINT16						f_usRinTsstIndex,
					IN	UINT16						f_usSinTsstIndex,
					IN	BOOL						f_fCheckBridgeIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						ulResult;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usEchoIndex );

	/* Mute the Rin port. */
	if ( ( f_fCheckBridgeIndex == FALSE ) 
		|| ( ( f_fCheckBridgeIndex == TRUE ) && ( pChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX ) ) )
	{
		/* If the channel is in bidir mode, do not create the Rin silence event!!! */
		if ( pChanEntry->fBiDirChannel == FALSE )
		{
			if ( ( ( f_usRinTsstIndex == cOCT6100_INVALID_INDEX ) || ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_RIN ) != 0x0 ) ) 
				&& ( pChanEntry->usRinSilenceEventIndex == cOCT6100_INVALID_INDEX ) ) 
			{
				ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, 
															 &pChanEntry->usRinSilenceEventIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Now, write the mixer event used to copy the RIN signal of the silence channel
				   into the RIN signal of the current channel. */

				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usRinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
				WriteParams.usWriteData |= 1534;
				WriteParams.usWriteData |= cOCT6100_PCM_U_LAW << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				WriteParams.ulWriteAddress += 2;
				WriteParams.usWriteData = pChanEntry->usRinRoutTsiMemIndex;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK  )
					return ulResult;

				/*=======================================================================*/


				/*=======================================================================*/
				/* Now insert the Sin copy event into the list.*/

				ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
													pChanEntry->usRinSilenceEventIndex,
													cOCT6100_EVENT_TYPE_SOUT_COPY,
													f_usEchoIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}

	/* Mute the Sin port. */
	if ( ( ( f_usSinTsstIndex == cOCT6100_INVALID_INDEX ) || ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 ) ) 
		&& ( pChanEntry->usSinSilenceEventIndex == cOCT6100_INVALID_INDEX ) )
	{
		ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, 
													 &pChanEntry->usSinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Now, write the mixer event used to copy the SIN signal of the silence channel
		   into the SIN signal of the current channel. */

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
		WriteParams.usWriteData |= 1534;
		WriteParams.usWriteData |= cOCT6100_PCM_U_LAW << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = pChanEntry->usSinSoutTsiMemIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Now insert the Sin copy event into the list.*/

		ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
											pChanEntry->usSinSilenceEventIndex,
											cOCT6100_EVENT_TYPE_SOUT_COPY,
											f_usEchoIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Unmute the Rin port if it was muted. */
	if ( ( ( f_usRinTsstIndex != cOCT6100_INVALID_INDEX ) && ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_RIN ) == 0x0 ) ) 
		&& ( pChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX ) )
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pChanEntry->usRinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usRinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_E1;

		pChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	/* Unmute the Sin port if it was muted. */
	if ( ( ( f_usSinTsstIndex != cOCT6100_INVALID_INDEX ) && ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN ) == 0x0 ) )
		&& ( pChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX ) )
	{
		/* Remove the event from the list.*/
		ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
												pChanEntry->usSinSilenceEventIndex,
												cOCT6100_EVENT_TYPE_SOUT_COPY );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usSinSilenceEventIndex );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_E2;

		pChanEntry->usSinSilenceEventIndex = cOCT6100_INVALID_INDEX;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiSetChannelLevelControl

Description:	This function will configure the level control on a given
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pVqeConfig			VQE config of the channel.
f_usChanIndex			Index of the channel within the API instance.
f_usEchoMemIndex		Index of the echo channel within the SSPX memory.
f_fClearAlcHlcStatusBit	If this is set, the ALC-HLC status bit must be
						incremented.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiSetChannelLevelControl
UINT32 Oct6100ApiSetChannelLevelControl(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_VQE	f_pVqeConfig,
				IN	UINT16						f_usChanIndex,
				IN	UINT16						f_usEchoMemIndex,
				IN	BOOL						f_fClearAlcHlcStatusBit )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	UINT32							ulResult;
	UINT32							ulTempData;
	UINT32							ulBaseAddress;
	UINT32							ulFeatureBytesOffset;
	UINT32							ulFeatureBitOffset;
	UINT32							ulFeatureFieldLength;
	UINT32							ulMask;
	UINT32							i;
	UINT16							usTempData;
	UINT8							byLastStatus;
	BOOL							fDisableAlcFirst;
	
	/* Get local pointer to shared portion of the API instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the channel list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex );

	/* Before doing anything, check if the configuration has changed. */
	if ( ( f_fClearAlcHlcStatusBit == TRUE )
		|| ( f_pVqeConfig->fRinLevelControl != pChanEntry->VqeConfig.fRinLevelControl ) 
		|| ( f_pVqeConfig->lRinLevelControlGainDb != pChanEntry->VqeConfig.chRinLevelControlGainDb ) 
		|| ( f_pVqeConfig->fRinAutomaticLevelControl != pChanEntry->VqeConfig.fRinAutomaticLevelControl ) 
		|| ( f_pVqeConfig->lRinAutomaticLevelControlTargetDb != pChanEntry->VqeConfig.chRinAutomaticLevelControlTargetDb ) 
		|| ( f_pVqeConfig->fRinHighLevelCompensation != pChanEntry->VqeConfig.fRinHighLevelCompensation ) 
		|| ( f_pVqeConfig->lRinHighLevelCompensationThresholdDb != pChanEntry->VqeConfig.chRinHighLevelCompensationThresholdDb ) 
		|| ( f_pVqeConfig->fSoutLevelControl != pChanEntry->VqeConfig.fSoutLevelControl ) 
		|| ( f_pVqeConfig->lSoutLevelControlGainDb != pChanEntry->VqeConfig.chSoutLevelControlGainDb ) 
		|| ( f_pVqeConfig->fSoutAutomaticLevelControl != pChanEntry->VqeConfig.fSoutAutomaticLevelControl ) 
		|| ( f_pVqeConfig->lSoutAutomaticLevelControlTargetDb != pChanEntry->VqeConfig.chSoutAutomaticLevelControlTargetDb ) 
		|| ( f_pVqeConfig->fSoutNaturalListenerEnhancement != pChanEntry->VqeConfig.fSoutNaturalListenerEnhancement ) 
		|| ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != pChanEntry->VqeConfig.bySoutAutomaticListenerEnhancementGainDb )
		|| ( f_pVqeConfig->ulSoutNaturalListenerEnhancementGainDb != pChanEntry->VqeConfig.bySoutNaturalListenerEnhancementGainDb ) )
	{
		/* Calculate base address for manual level control configuration. */
		ulBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

		/* Set the Level control on RIN port.*/
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinLevelControlOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinLevelControlOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.RinLevelControlOfst.byFieldSize;

		/* First read the DWORD where the field is located.*/
		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= (~ulMask);

		if ( ( f_pVqeConfig->fRinLevelControl == TRUE ) 
			|| ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE ) 
			|| ( f_pVqeConfig->fRinHighLevelCompensation == TRUE ) )
		{
			/* Set the level control value.*/
			if ( ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE ) 
				|| ( f_pVqeConfig->fRinHighLevelCompensation == TRUE ) )
				ulTempData |= ( 0xFF << ulFeatureBitOffset );
			else 
			{
				/* Convert the dB value into OctFloat format.*/
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( f_pVqeConfig->lRinLevelControlGainDb );
				usTempData -= 0x3800;
				usTempData &= 0x0FF0;
				usTempData >>= 4;

				ulTempData |= ( usTempData << ulFeatureBitOffset ); 
			}
		}
		else /* ( ( f_pVqeConfig->fRinLevelControl == FALSE ) && ( f_pVqeConfig->fRinAutomaticLevelControl == FALSE ) && ( f_pVqeConfig->fRinHighLevelCompensation == FALSE ) ) */
		{
			ulTempData |= ( cOCT6100_PASS_THROUGH_LEVEL_CONTROL << ulFeatureBitOffset );
		}

		/* Save the DWORD where the field is located.*/
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pChanEntry,
										ulBaseAddress + ulFeatureBytesOffset,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	

		/* Set the Level control on SOUT port.*/
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.SoutLevelControlOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.SoutLevelControlOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.SoutLevelControlOfst.byFieldSize;

		/* First read the DWORD where the field is located.*/
		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		ulTempData &= (~ulMask);

		if ( ( f_pVqeConfig->fSoutLevelControl == TRUE ) 
			|| ( f_pVqeConfig->fSoutAutomaticLevelControl == TRUE )
			|| ( f_pVqeConfig->fSoutNaturalListenerEnhancement == TRUE )
			|| ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != 0x0 ) )
		{
			/* Set the level control value.*/
			if ( ( f_pVqeConfig->fSoutAutomaticLevelControl == TRUE ) 
				|| ( f_pVqeConfig->fSoutNaturalListenerEnhancement == TRUE ) 
				|| ( f_pVqeConfig->ulSoutAutomaticListenerEnhancementGainDb != 0x0 ) )
				ulTempData |= ( 0xFF << ulFeatureBitOffset );
			else 
			{
				/* Convert the dB value into OctFloat format.*/
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( f_pVqeConfig->lSoutLevelControlGainDb );
				usTempData -= 0x3800;
				usTempData &= 0x0FF0;
				usTempData >>= 4;

				ulTempData |= ( usTempData << ulFeatureBitOffset ); 
			}
		}
		else
		{
			ulTempData |= ( cOCT6100_PASS_THROUGH_LEVEL_CONTROL << ulFeatureBitOffset );
		}

		/* Save the DWORD where the field is located.*/
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pChanEntry,
										ulBaseAddress + ulFeatureBytesOffset,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
		
		/* Calculate base address for auto level control + high level compensation configuration. */
		ulBaseAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst;

		/* Check which one is to be disabled first. */
		if ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE )
			fDisableAlcFirst = FALSE;
		else
			fDisableAlcFirst = TRUE;

		for ( i = 0; i < 2; i ++ )
		{
			/* Set the auto level control target Db for the Rin port. */
			if ( ( ( i == 0 ) && ( fDisableAlcFirst == TRUE ) ) || ( ( i == 1 ) && ( fDisableAlcFirst == FALSE ) ) )
			{
				if ( pSharedInfo->ImageInfo.fRinAutoLevelControl == TRUE )
				{
					ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinAutoLevelControlTargetOfst.usDwordOffset * 4;
					ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinAutoLevelControlTargetOfst.byBitOffset;
					ulFeatureFieldLength = pSharedInfo->MemoryMap.RinAutoLevelControlTargetOfst.byFieldSize;

					/* First read the DWORD where the field is located.*/
					ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
														pChanEntry,
														ulBaseAddress + ulFeatureBytesOffset,
														&ulTempData);
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					/* Clear previous value set in the feature field.*/
					mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

					ulTempData &= (~ulMask);

					if ( f_pVqeConfig->fRinAutomaticLevelControl == TRUE )
					{
						/* Convert the dB value into OctFloat format.*/
						usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lRinAutomaticLevelControlTargetDb );

						/* Set auto level control target on the Rin port. */
						ulTempData |= ( usTempData << ulFeatureBitOffset ); 
					}
					else /* if ( f_pVqeConfig->fRinAutomaticLevelControl == FALSE ) */
					{
						/* Disable auto level control. */
						ulTempData |= ( 0xFFFF << ulFeatureBitOffset ); 
					}

					/* Save the DWORD where the field is located.*/
					ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
													pChanEntry,
													ulBaseAddress + ulFeatureBytesOffset,
													ulTempData);
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;	
				}
			}
			else
			{
				/* Set the high level compensation threshold Db for the Rin port. */
				if ( pSharedInfo->ImageInfo.fRinHighLevelCompensation == TRUE )
				{
					ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinHighLevelCompensationThresholdOfst.usDwordOffset * 4;
					ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinHighLevelCompensationThresholdOfst.byBitOffset;
					ulFeatureFieldLength = pSharedInfo->MemoryMap.RinHighLevelCompensationThresholdOfst.byFieldSize;

					/* First read the DWORD where the field is located.*/
					ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
														pChanEntry,
														ulBaseAddress + ulFeatureBytesOffset,
														&ulTempData);
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					/* Clear previous value set in the feature field.*/
					mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

					ulTempData &= (~ulMask);

					if ( f_pVqeConfig->fRinHighLevelCompensation == TRUE )
					{
						/* Convert the dB value into OctFloat format.*/
						usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lRinHighLevelCompensationThresholdDb );

						/* Set high level compensation threshold on the Rin port. */
						ulTempData |= ( usTempData << ulFeatureBitOffset ); 
					}
					else /* if ( f_pVqeConfig->fRinHighLevelCompensation == FALSE ) */
					{
						/* Disable high level compensation. */
						ulTempData |= ( 0xFFFF << ulFeatureBitOffset ); 
					}

					/* Save the DWORD where the field is located.*/
					ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
													pChanEntry,
													ulBaseAddress + ulFeatureBytesOffset,
													ulTempData);
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;	
				}
			}
		}

		/* Set the auto level control target Db for the Sout port. */
		if ( pSharedInfo->ImageInfo.fRinAutoLevelControl == TRUE )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.SoutAutoLevelControlTargetOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.SoutAutoLevelControlTargetOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.SoutAutoLevelControlTargetOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			if ( f_pVqeConfig->fSoutAutomaticLevelControl == TRUE )
			{
				/* Convert the dB value into OctFloat format.*/
				usTempData = Oct6100ApiDbAmpHalfToOctFloat( 2 * f_pVqeConfig->lSoutAutomaticLevelControlTargetDb );

				/* Set auto level control target on the Sout port. */
				ulTempData |= ( usTempData << ulFeatureBitOffset ); 
			}
			else /* if ( f_pVqeConfig->fSoutAutomaticLevelControl == FALSE ) */
			{
				/* Disable auto level control. */
				ulTempData |= ( 0xFFFF << ulFeatureBitOffset ); 
			}

			/* Save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}

		/* Set the high level compensation threshold Db for the Sout port. */
		if ( pSharedInfo->ImageInfo.fSoutHighLevelCompensation == TRUE )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.SoutHighLevelCompensationThresholdOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.SoutHighLevelCompensationThresholdOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.SoutHighLevelCompensationThresholdOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Disable high level compensation on Sout for now. */
			ulTempData |= ( 0xFFFF << ulFeatureBitOffset ); 

			/* Save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}

		/* Check if have to clear the ALC-HLC status. */
		if ( ( pSharedInfo->ImageInfo.fAlcHlcStatus == TRUE ) 
			&& ( ( f_fClearAlcHlcStatusBit == TRUE )

				) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AlcHlcStatusOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AlcHlcStatusOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AlcHlcStatusOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Get previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			/* Retrieve last status. */
			byLastStatus = (UINT8)( ( ( ulTempData & ulMask ) >> ulFeatureBitOffset ) & 0xFF );

			/* Increment to reset context. */
			byLastStatus ++;

			/* Just in case, not to overwrite some context in external memory. */
			byLastStatus &= ( 0x1 << ulFeatureFieldLength ) - 1;

			/* Clear last status. */
			ulTempData &= (~ulMask);

			/* Set new status. */
			ulTempData |= ( byLastStatus << ulFeatureBitOffset ); 

			/* Save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;			
		}
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiSetChannelTailConfiguration

Description:	This function will configure the tail displacement and length
				on a given channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pVqeConfig			VQE config of the channel.
f_usChanIndex			Index of the channel within the API instance.
f_usEchoMemIndex		Index of the echo channel within the SSPX memory.
f_fModifyOnly			Function called from a modify or open?	

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiSetChannelTailConfiguration
UINT32 Oct6100ApiSetChannelTailConfiguration(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	tPOCT6100_CHANNEL_OPEN_VQE	f_pVqeConfig,
				IN	UINT16						f_usChanIndex,
				IN	UINT16						f_usEchoMemIndex,
				IN	BOOL						f_fModifyOnly )
{
	tPOCT6100_API_CHANNEL	pChanEntry;
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32					ulResult;
	UINT32					ulTempData;
	UINT32					ulNlpConfBaseAddress;
	UINT32					ulAfConfBaseAddress;
	UINT32					ulFeatureBytesOffset;
	UINT32					ulFeatureBitOffset;
	UINT32					ulFeatureFieldLength;
	UINT32					ulMask;
	UINT32					ulTailSum;
	BOOL					fTailDisplacementModified = FALSE;
	
	/* Get local pointer to shared portion of the API instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the channel list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex );

	/* Calculate base addresses of NLP + AF configuration structure for the specified channel. */
	ulNlpConfBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;
	ulAfConfBaseAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainIoMemOfst;

	/* Set the tail displacement.*/
	if ( pSharedInfo->ImageInfo.fTailDisplacement == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->fEnableTailDisplacement != pChanEntry->VqeConfig.fEnableTailDisplacement ) 
					|| ( f_pVqeConfig->ulTailDisplacement != pChanEntry->VqeConfig.usTailDisplacement ) 
					|| ( f_pVqeConfig->fAcousticEcho != pChanEntry->VqeConfig.fAcousticEcho ) ) ) )
		{
			/* Remember that the tail displacement parameters were changed. */
			fTailDisplacementModified = TRUE;

			/* Check if we must set the tail displacement value. */
			if ( ( f_pVqeConfig->fEnableTailDisplacement == TRUE ) 
				&& ( pSharedInfo->ImageInfo.fPerChannelTailDisplacement == TRUE ) )
			{
				ulFeatureBytesOffset = pSharedInfo->MemoryMap.PerChanTailDisplacementFieldOfst.usDwordOffset * 4;
				ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PerChanTailDisplacementFieldOfst.byBitOffset;
				ulFeatureFieldLength = pSharedInfo->MemoryMap.PerChanTailDisplacementFieldOfst.byFieldSize;

				/* First read the DWORD where the field is located.*/
				ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
													pChanEntry,
													ulNlpConfBaseAddress + ulFeatureBytesOffset,
													&ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Clear previous value set in the feature field.*/
				mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

				ulTempData &= (~ulMask);
				if ( ( f_pVqeConfig->fEnableTailDisplacement == TRUE ) 
					&& ( f_pVqeConfig->ulTailDisplacement != 0x0 ) )
				{
					if ( pSharedInfo->ImageInfo.fAfTailDisplacement == FALSE )
					{
						if ( f_pVqeConfig->ulTailDisplacement == cOCT6100_AUTO_SELECT_TAIL )
						{
							ulTempData |= ( ( ( pSharedInfo->ChipConfig.usTailDisplacement / 16 ) ) << ulFeatureBitOffset );
						}
						else
						{
							ulTempData |= ( ( ( f_pVqeConfig->ulTailDisplacement / 16 ) ) << ulFeatureBitOffset );
						}
					}
					else /* if ( pSharedInfo->ImageInfo.fAfTailDisplacement == TRUE ) */
					{
						/* If AEC is not activated, this must be set to the requested tail displacement. */
						if ( f_pVqeConfig->fAcousticEcho == FALSE )
						{
							if ( f_pVqeConfig->ulTailDisplacement == cOCT6100_AUTO_SELECT_TAIL )
							{
								ulTailSum = pSharedInfo->ChipConfig.usTailDisplacement;
							}
							else
							{
								ulTailSum = f_pVqeConfig->ulTailDisplacement;
							}
							
							if ( ulTailSum == 0 )
							{
								ulTempData |= ( ( 0 ) << ulFeatureBitOffset );
							}
							else if ( ulTailSum <= 128 )
							{
								ulTempData |= ( ( 1 ) << ulFeatureBitOffset );
							}
							else if ( ulTailSum <= 384 )
							{
								ulTempData |= ( ( 3 ) << ulFeatureBitOffset );
							}
							else /* if ( ulTailSum <= 896 ) */
							{
								ulTempData |= ( ( 7 ) << ulFeatureBitOffset );
							}
						}
						else /* if ( f_pVqeConfig->fAcousticEcho == FALSE ) */
						{
							/* Otherwise, the tail displacement is configured differently.  This field stays to 0. */
							ulTempData |= ( 0x0 << ulFeatureBitOffset );
						}
					}
				}

				/* Then save the new DWORD where the field is located.*/
				ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfBaseAddress + ulFeatureBytesOffset,
												ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;	
			}

			if ( pSharedInfo->ImageInfo.fAfTailDisplacement == TRUE )
			{
				/* Set the tail displacement offset in the AF. */
				ulFeatureBytesOffset = pSharedInfo->MemoryMap.AfTailDisplacementFieldOfst.usDwordOffset * 4;
				ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AfTailDisplacementFieldOfst.byBitOffset;
				ulFeatureFieldLength = pSharedInfo->MemoryMap.AfTailDisplacementFieldOfst.byFieldSize;

				/* First read the DWORD where the field is located.*/
				ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
													pChanEntry,
													ulAfConfBaseAddress + ulFeatureBytesOffset,
													&ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Clear previous value set in the feature field.*/
				mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

				ulTempData &= (~ulMask);

				if ( f_pVqeConfig->ulTailDisplacement == cOCT6100_AUTO_SELECT_TAIL )
				{
					ulTempData |= ( ( ( pSharedInfo->ChipConfig.usTailDisplacement / 16 ) ) << ulFeatureBitOffset );
				}
				else
				{
					ulTempData |= ( ( ( f_pVqeConfig->ulTailDisplacement / 16 ) ) << ulFeatureBitOffset );
				}

				/* Then save the DWORD where the field is located.*/
				ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfBaseAddress + ulFeatureBytesOffset,
												ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			ulFeatureBytesOffset = pSharedInfo->MemoryMap.TailDisplEnableOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.TailDisplEnableOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.TailDisplEnableOfst.byFieldSize;

			/* First read the DWORD where the field is located.*/
			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			ulTempData |= ( ( (UINT32)f_pVqeConfig->fEnableTailDisplacement ) << ulFeatureBitOffset );

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Set the tail length. */
	if ( pSharedInfo->ImageInfo.fPerChannelTailLength == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( f_pVqeConfig->ulTailLength != pChanEntry->VqeConfig.usTailLength ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.PerChanTailLengthFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PerChanTailLengthFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.PerChanTailLengthFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulAfConfBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);
			/* Check if must automatically select maximum or if must use user specific value. */
			if ( f_pVqeConfig->ulTailLength == cOCT6100_AUTO_SELECT_TAIL )
			{
				ulTempData |= ( ( ( pSharedInfo->ImageInfo.usMaxTailLength - 32 ) / 4 )  << ulFeatureBitOffset );
			}
			else
			{
				ulTempData |= ( ( ( f_pVqeConfig->ulTailLength - 32 ) / 4 )  << ulFeatureBitOffset );
			}

			/* Then save the DWORD where the field is located.*/
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulAfConfBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	/* Configure AEC tail length. */
	if ( pSharedInfo->ImageInfo.fAecTailLength == TRUE )
	{
		/* Check if the configuration has been changed. */
		if ( ( f_fModifyOnly == FALSE )
			|| ( fTailDisplacementModified == TRUE )
			|| ( ( f_fModifyOnly == TRUE ) 
				&& ( ( f_pVqeConfig->ulAecTailLength != pChanEntry->VqeConfig.usAecTailLength ) 
				|| ( f_pVqeConfig->fAcousticEcho != pChanEntry->VqeConfig.fAcousticEcho ) ) ) )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.AecTailLengthFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.AecTailLengthFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.AecTailLengthFieldOfst.byFieldSize;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pChanEntry,
												ulNlpConfBaseAddress + ulFeatureBytesOffset,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear previous value set in the feature field.*/
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			ulTempData &= (~ulMask);

			/* Set acoustic echo tail length. */
			if ( f_pVqeConfig->fAcousticEcho == TRUE )
			{
				switch( f_pVqeConfig->ulAecTailLength )
				{
				case 1024:
					ulTempData |= ( ( 3 ) << ulFeatureBitOffset );
					break;
				case 512:
					ulTempData |= ( ( 2 ) << ulFeatureBitOffset );
					break;
				case 256:
					ulTempData |= ( ( 1 ) << ulFeatureBitOffset );
					break;
				case 128:
				default:
					ulTempData |= ( ( 0 ) << ulFeatureBitOffset );
					break;
				}
			}
			else if ( f_pVqeConfig->fEnableTailDisplacement == TRUE )
			{
				/* No acoustic echo case. */

				/* Start with requested tail displacement. */
				if ( f_pVqeConfig->ulTailDisplacement == cOCT6100_AUTO_SELECT_TAIL )
				{
					ulTailSum = pSharedInfo->ChipConfig.usTailDisplacement;
				}
				else
				{
					ulTailSum = f_pVqeConfig->ulTailDisplacement;
				}

				/* Add requested tail length. */
				if ( f_pVqeConfig->ulTailLength == cOCT6100_AUTO_SELECT_TAIL )
				{
					ulTailSum += pSharedInfo->ImageInfo.usMaxTailLength;
				}
				else
				{
					ulTailSum += f_pVqeConfig->ulTailLength;
				}

				/* Round this value up. */
				if ( ulTailSum <= 128 )
				{
					ulTempData |= ( ( 0 ) << ulFeatureBitOffset );
				}
				else if ( ulTailSum <= 256 )
				{
					ulTempData |= ( ( 1 ) << ulFeatureBitOffset );
				}
				else if ( ulTailSum <= 512 )
				{
					ulTempData |= ( ( 2 ) << ulFeatureBitOffset );
				}
				else /* if ( ulTailSum <= 1024 ) */
				{
					ulTempData |= ( ( 3 ) << ulFeatureBitOffset );
				}
			}
			else
			{
				/* Keep this to zero. */
				ulTempData |= ( ( 0 ) << ulFeatureBitOffset );
			}
			
			/* Write the new DWORD where the field is located. */
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulNlpConfBaseAddress + ulFeatureBytesOffset,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelMuteSer

Description:	This function will mute some of the ports on a given
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChannelMute			What channel/ports to mute.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelMuteSer
UINT32 Oct6100ChannelMuteSer(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	tPOCT6100_CHANNEL_MUTE		f_pChannelMute )
{
	UINT32	ulResult;
	UINT16	usChanIndex;
	UINT16	usPortMask;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertChannelMuteParams(	f_pApiInstance, 
													f_pChannelMute, 
													&usChanIndex,
													&usPortMask );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Call the actual channel mute ports function. */
	ulResult = Oct6100ApiMuteChannelPorts(	f_pApiInstance, 
											usChanIndex,
											usPortMask,
											TRUE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertChannelMuteParams

Description:	Check the user parameters passed to the channel mute function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChannelMute			What channel/ports to mute.
f_pusChanIndex			Resulting channel index where the muting should
						be applied.
f_pusPorts				Port mask on which to apply the muting.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertChannelMuteParams
UINT32 Oct6100ApiAssertChannelMuteParams(	
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance, 
				IN	tPOCT6100_CHANNEL_MUTE		f_pChannelMute, 
				OUT PUINT16						f_pusChanIndex,
				OUT PUINT16						f_pusPorts )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pChanEntry;
	UINT32							ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pChannelMute->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelMute->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, *f_pusChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelMute->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;
	if ( pChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CHANNEL_PART_OF_BIDIR_CHANNEL;

	/*=======================================================================*/

	/* Check the provided port mask. */

	if ( ( f_pChannelMute->ulPortMask &
		~(	cOCT6100_CHANNEL_MUTE_PORT_NONE | 
			cOCT6100_CHANNEL_MUTE_PORT_RIN |
			cOCT6100_CHANNEL_MUTE_PORT_ROUT |
			cOCT6100_CHANNEL_MUTE_PORT_SIN | 
			cOCT6100_CHANNEL_MUTE_PORT_SOUT |
			cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) ) != 0 )
		return cOCT6100_ERR_CHANNEL_MUTE_MASK;

	/* Sin + Sin with features cannot be muted simultaneously. */
	if ( ( ( f_pChannelMute->ulPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 )
		&& ( ( f_pChannelMute->ulPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 ) )
		return cOCT6100_ERR_CHANNEL_MUTE_MASK_SIN;

	/* Check if Sin mute with features is supported by the firmware. */
	if ( ( ( f_pChannelMute->ulPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 )
		&& ( pSharedInfo->ImageInfo.fSinMute == FALSE ) )
		return cOCT6100_ERR_NOT_SUPPORTED_CHANNEL_SIN_MUTE_FEATURES;

	/* Return the ports to the calling function. */
	*f_pusPorts = (UINT16)( f_pChannelMute->ulPortMask & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChannelUnMuteSer

Description:	This function will unmute some of the ports on a given
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChannelUnMute		What channel/ports to unmute.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChannelUnMuteSer
UINT32 Oct6100ChannelUnMuteSer(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	tPOCT6100_CHANNEL_UNMUTE	f_pChannelUnMute )
{
	UINT32	ulResult;
	UINT16	usChanIndex;
	UINT16	usPortMask;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertChannelUnMuteParams(	f_pApiInstance, 
													f_pChannelUnMute, 
													&usChanIndex,
													&usPortMask );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Call the actual channel mute ports function. */
	ulResult = Oct6100ApiMuteChannelPorts(	f_pApiInstance, 
											usChanIndex,
											usPortMask,
											FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertChannelUnMuteParams

Description:	Check the user parameters passed to the channel unmute function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pChannelUnMute		What channel/ports to Unmute.
f_pusChanIndex			Resulting channel index where the muting should
						be applied.
f_pusPorts				Port mask on which to apply the muting.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertChannelUnMuteParams
UINT32 Oct6100ApiAssertChannelUnMuteParams(	
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance, 
				IN	tPOCT6100_CHANNEL_UNMUTE	f_pChannelUnMute, 
				OUT PUINT16						f_pusChanIndex,
				OUT PUINT16						f_pusPorts )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pChanEntry;
	UINT32							ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pChannelUnMute->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	*f_pusChanIndex = (UINT16)( f_pChannelUnMute->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, *f_pusChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pChannelUnMute->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;
	if ( pChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CHANNEL_PART_OF_BIDIR_CHANNEL;

	/*=======================================================================*/

	/* Check the provided port mask. */

	if ( ( f_pChannelUnMute->ulPortMask &
		~(	cOCT6100_CHANNEL_MUTE_PORT_NONE | 
			cOCT6100_CHANNEL_MUTE_PORT_RIN |
			cOCT6100_CHANNEL_MUTE_PORT_ROUT |
			cOCT6100_CHANNEL_MUTE_PORT_SIN | 
			cOCT6100_CHANNEL_MUTE_PORT_SOUT |
			cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) ) != 0 )
		return cOCT6100_ERR_CHANNEL_MUTE_MASK;
	
	/* Return the ports to the calling function. */
	*f_pusPorts = (UINT16)( f_pChannelUnMute->ulPortMask & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMuteSinWithFeatures

Description:	Mute or Unmute the sin with features port.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.

f_usChanIndex				Resulting channel index where the muting should
							be applied.
f_fEnableSinWithFeatures	Whether to enable the feature or not.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMuteSinWithFeatures
UINT32 Oct6100ApiMuteSinWithFeatures(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	UINT16						f_usChanIndex,
				IN	BOOL						f_fEnableSinWithFeatures )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pChanEntry;
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;

	UINT32							ulTempData;
	UINT32							ulBaseAddress;
	UINT32							ulFeatureBytesOffset;
	UINT32							ulFeatureBitOffset;
	UINT32							ulFeatureFieldLength;
	UINT32							ulMask;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex  )

	ulBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( pChanEntry->usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;
	
	if ( pSharedInfo->ImageInfo.fSinMute == TRUE )
	{
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.SinMuteOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.SinMuteOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.SinMuteOfst.byFieldSize;

		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pChanEntry,
											ulBaseAddress + ulFeatureBytesOffset,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
		
		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		/* Clear the mute flag. */
		ulTempData &= (~ulMask);

		/* Set the mute flag on the Sin port.*/
		if ( f_fEnableSinWithFeatures == TRUE )
			ulTempData |= ( 0x1 << ulFeatureBitOffset );

		/* Write the new DWORD where the field is located. */
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pChanEntry,
										ulBaseAddress + ulFeatureBytesOffset,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMuteChannelPorts

Description:	Mute or Unmute the specified ports, according to the mask.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usChanIndex			Resulting channel index where the muting should
						be applied.
f_usPortMask			Port mask on which to apply the muting/unmuting.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMuteChannelPorts
UINT32 Oct6100ApiMuteChannelPorts(	
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN	UINT16						f_usChanIndex,
				IN	UINT16						f_usPortMask,
				IN	BOOL						f_fMute )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pChanEntry;
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32							ulResult;
	BOOL							fDisableSinWithFeatures = FALSE;
	BOOL							fEnableSinWithFeatures = FALSE;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex  )

	/* Rin port. */
	if ( ( f_fMute == TRUE )
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_RIN ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_RIN ) == 0x0 ) )
	{
		/* Mute this port. */
		pChanEntry->usMutedPorts |= cOCT6100_CHANNEL_MUTE_PORT_RIN;

		ulResult = Oct6100ApiMutePorts( f_pApiInstance, f_usChanIndex, pChanEntry->usRinTsstIndex, pChanEntry->usSinTsstIndex, TRUE );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_RIN;
			return ulResult;
		}
	}
	else if ( ( f_fMute == FALSE ) 
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_RIN ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_RIN ) != 0x0 ) )
	{
		/* Unmute this port. */
		pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_RIN;

		ulResult = Oct6100ApiMutePorts( f_pApiInstance, f_usChanIndex, pChanEntry->usRinTsstIndex, pChanEntry->usSinTsstIndex, TRUE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Rout port. */
	if ( ( f_fMute == TRUE )
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) == 0x0 ) )
	{
		/* Mute this port. */

		if ( pChanEntry->usRoutTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
															   pChanEntry->usRoutTsstIndex,
															   pChanEntry->CodecConfig.byAdpcmNibblePosition,
															   pChanEntry->TdmConfig.byRoutNumTssts,
															   1534 );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		pChanEntry->usMutedPorts |= cOCT6100_CHANNEL_MUTE_PORT_ROUT;
	}
	else if ( ( f_fMute == FALSE ) 
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_ROUT ) != 0x0 ) )
	{
		/* Unmute this port. */

		if ( pChanEntry->usRoutTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
															   pChanEntry->usRoutTsstIndex,
															   pChanEntry->CodecConfig.byAdpcmNibblePosition,
															   pChanEntry->TdmConfig.byRoutNumTssts,
															   pChanEntry->usRinRoutTsiMemIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_ROUT;
	}

	/* Sin port. */
	if ( ( f_fMute == TRUE )
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN ) == 0x0 ) )
	{
		/* Mute this port. */
		pChanEntry->usMutedPorts |= cOCT6100_CHANNEL_MUTE_PORT_SIN;

		ulResult = Oct6100ApiMutePorts( f_pApiInstance, f_usChanIndex, pChanEntry->usRinTsstIndex, pChanEntry->usSinTsstIndex, TRUE );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_SIN;
			return ulResult;
		}
	}
	else if ( 
			( ( f_fMute == FALSE ) 
			&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 )
			&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 ) ) 
		|| 
			( ( f_fMute == TRUE )  
			&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 ) 
			&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 ) ) )
	{
		/* Unmute this port. */
		pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_SIN;

		ulResult = Oct6100ApiMutePorts( f_pApiInstance, f_usChanIndex, pChanEntry->usRinTsstIndex, pChanEntry->usSinTsstIndex, TRUE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Sout port. */
	if ( ( f_fMute == TRUE )
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) == 0x0 ) )
	{
		/* Mute this port. */

		if ( pChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
															   pChanEntry->usSoutTsstIndex,
															   pChanEntry->CodecConfig.byAdpcmNibblePosition,
															   pChanEntry->TdmConfig.bySoutNumTssts,
															   1534 );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		pChanEntry->usMutedPorts |= cOCT6100_CHANNEL_MUTE_PORT_SOUT;
	}
	else if ( ( f_fMute == FALSE ) 
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SOUT ) != 0x0 ) )
	{
		/* Unmute this port. */

		if ( pChanEntry->usSoutTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
															   pChanEntry->usSoutTsstIndex,
															   pChanEntry->CodecConfig.byAdpcmNibblePosition,
															   pChanEntry->TdmConfig.bySoutNumTssts,
															   pChanEntry->usSinSoutTsiMemIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_SOUT;
	}

	/* Sin with features port. */
	if ( ( f_fMute == TRUE )
		&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 )
		&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) == 0x0 ) )
	{
		/* Mute this port. */
		pChanEntry->usMutedPorts |= cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES;
		fEnableSinWithFeatures = TRUE;
	}
	else if ( 
			( ( f_fMute == FALSE ) 
			&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 )
			&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 ) )
		|| 
			( ( f_fMute == TRUE )  
			&& ( ( f_usPortMask & cOCT6100_CHANNEL_MUTE_PORT_SIN ) != 0x0 ) 
			&& ( ( pChanEntry->usMutedPorts & cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES ) != 0x0 ) ) )
	{
		/* Unmute this port. */
		pChanEntry->usMutedPorts &= ~cOCT6100_CHANNEL_MUTE_PORT_SIN_WITH_FEATURES;

		fDisableSinWithFeatures = TRUE;
	}

	/* Check if must enable or disable SIN mute with features. */
	if ( fDisableSinWithFeatures == TRUE || fEnableSinWithFeatures == TRUE )
	{
		ulResult = Oct6100ApiMuteSinWithFeatures( f_pApiInstance, f_usChanIndex, fEnableSinWithFeatures );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}

	return cOCT6100_ERR_OK;
}
#endif
