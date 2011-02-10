/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_chip_stats.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to retreive the OCT6100 chip stats.

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

$Octasic_Revision: 89 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/


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
#include "oct6100api/oct6100_channel_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_chip_stats_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_chip_stats_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_chip_stats_priv.h"

/****************************  PUBLIC FUNCTIONS  *****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChipGetStats

Description:    Retreives the chip statistics and configuration.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pChipStats		Pointer to a tOCT6100_CHIP_STATS structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipGetStatsDef
UINT32 Oct6100ChipGetStatsDef(
				tPOCT6100_CHIP_STATS				f_pChipStats )
{
	f_pChipStats->fResetChipStats				= FALSE;

	f_pChipStats->ulNumberChannels				= cOCT6100_INVALID_STAT;
	f_pChipStats->ulNumberTsiCncts				= cOCT6100_INVALID_STAT;
	f_pChipStats->ulNumberConfBridges			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulNumberPlayoutBuffers		= cOCT6100_INVALID_STAT;
	f_pChipStats->ulPlayoutFreeMemSize			= cOCT6100_INVALID_STAT;

	f_pChipStats->ulNumberPhasingTssts			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulNumberAdpcmChannels			= cOCT6100_INVALID_STAT;

	f_pChipStats->ulH100OutOfSynchCount			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulH100ClockABadCount			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulH100FrameABadCount			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulH100ClockBBadCount			= cOCT6100_INVALID_STAT;
	f_pChipStats->ulInternalReadTimeoutCount	= cOCT6100_INVALID_STAT;
	f_pChipStats->ulSdramRefreshTooLateCount	= cOCT6100_INVALID_STAT;
	f_pChipStats->ulPllJitterErrorCount			= cOCT6100_INVALID_STAT;

	f_pChipStats->ulOverflowToneEventsCount					= cOCT6100_INVALID_STAT;
	f_pChipStats->ulSoftOverflowToneEventsCount				= cOCT6100_INVALID_STAT;
	f_pChipStats->ulSoftOverflowBufferPlayoutEventsCount	= cOCT6100_INVALID_STAT;
	

	
	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ChipGetStats
UINT32 Oct6100ChipGetStats(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_CHIP_STATS			f_pChipStats )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure. */
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ChipGetStatsSer( f_pApiInstance, f_pChipStats );
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

Function:		Oct6100ChipGetImageInfo

Description:    Retrieves the chip image information indicating the supported 
				features and tones.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pChipImageInfo	Pointer to a tPOCT6100_CHIP_IMAGE_INFO structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipGetImageInfoDef
UINT32 Oct6100ChipGetImageInfoDef(
				tPOCT6100_CHIP_IMAGE_INFO				f_pChipImageInfo )
{
	UINT32	i;

	Oct6100UserMemSet( f_pChipImageInfo->szVersionNumber, 0x0, cOCT6100_VERSION_NUMBER_MAX_SIZE );

	f_pChipImageInfo->fBufferPlayout				= FALSE;
	f_pChipImageInfo->fAdaptiveNoiseReduction		= FALSE;
	f_pChipImageInfo->fSoutNoiseBleaching			= FALSE;
	f_pChipImageInfo->fConferencingNoiseReduction	= FALSE;
	f_pChipImageInfo->fAutoLevelControl				= FALSE;
	f_pChipImageInfo->fHighLevelCompensation		= FALSE;
	f_pChipImageInfo->fSilenceSuppression			= FALSE;
	
	f_pChipImageInfo->fAdpcm				= FALSE;
	f_pChipImageInfo->fConferencing			= FALSE;
	f_pChipImageInfo->fDominantSpeaker		= FALSE;
	f_pChipImageInfo->ulMaxChannels			= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulNumTonesAvailable	= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulToneProfileNumber	= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulMaxTailDisplacement = cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulBuildId				= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulMaxTailLength		= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulDebugEventSize		= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulMaxPlayoutEvents	= cOCT6100_INVALID_VALUE;
	f_pChipImageInfo->ulImageType			= cOCT6100_INVALID_VALUE;

	f_pChipImageInfo->fAcousticEcho					= FALSE;
	f_pChipImageInfo->fAecTailLength				= FALSE;
	f_pChipImageInfo->fToneRemoval					= FALSE;

	f_pChipImageInfo->fDefaultErl					= FALSE;
	f_pChipImageInfo->fNonLinearityBehaviorA		= FALSE;
	f_pChipImageInfo->fNonLinearityBehaviorB		= FALSE;
	f_pChipImageInfo->fPerChannelTailDisplacement	= FALSE;
	f_pChipImageInfo->fPerChannelTailLength			= FALSE;
	f_pChipImageInfo->fListenerEnhancement			= FALSE;
	f_pChipImageInfo->fRoutNoiseReduction			= FALSE;
	f_pChipImageInfo->fRoutNoiseReductionLevel		= FALSE;
	f_pChipImageInfo->fAnrSnrEnhancement			= FALSE;
	f_pChipImageInfo->fAnrVoiceNoiseSegregation		= FALSE;
	f_pChipImageInfo->fToneDisablerVqeActivationDelay = FALSE;
	f_pChipImageInfo->fMusicProtection				= FALSE;
	f_pChipImageInfo->fDoubleTalkBehavior			= FALSE;
	f_pChipImageInfo->fIdleCodeDetection			= TRUE;
	f_pChipImageInfo->fSinLevel						= TRUE;

	for ( i = 0; i < cOCT6100_MAX_TONE_EVENT; i++ )
	{
		Oct6100UserMemSet( f_pChipImageInfo->aToneInfo[ i ].aszToneName, 0x00, cOCT6100_TLV_MAX_TONE_NAME_SIZE );
		f_pChipImageInfo->aToneInfo[ i ].ulDetectionPort = cOCT6100_INVALID_PORT;
		f_pChipImageInfo->aToneInfo[ i ].ulToneID = cOCT6100_INVALID_VALUE;
	}

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ChipGetImageInfo
UINT32 Oct6100ChipGetImageInfo(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_CHIP_IMAGE_INFO		f_pChipImageInfo )
{
	tPOCT6100_API_IMAGE_INFO	pImageInfo;
	UINT32	i;

	/* Get local pointer(s). */
	pImageInfo = &f_pApiInstance->pSharedInfo->ImageInfo;

	Oct6100UserMemCopy( f_pChipImageInfo->szVersionNumber, pImageInfo->szVersionNumber, cOCT6100_VERSION_NUMBER_MAX_SIZE );

	/* Copy the customer info. */
	f_pChipImageInfo->ulBuildId = pImageInfo->ulBuildId;

	/* Copy the features list. */
	f_pChipImageInfo->fBufferPlayout				= pImageInfo->fBufferPlayout;
	f_pChipImageInfo->fAdaptiveNoiseReduction		= pImageInfo->fAdaptiveNoiseReduction;
	f_pChipImageInfo->fSoutNoiseBleaching			= pImageInfo->fSoutNoiseBleaching;
	f_pChipImageInfo->fSilenceSuppression			= pImageInfo->fSilenceSuppression;

	f_pChipImageInfo->fAdpcm						= pImageInfo->fAdpcm;
	f_pChipImageInfo->fConferencing					= pImageInfo->fConferencing;
	f_pChipImageInfo->fDominantSpeaker				= pImageInfo->fDominantSpeakerEnabled;
	f_pChipImageInfo->fConferencingNoiseReduction	= pImageInfo->fConferencingNoiseReduction;
	f_pChipImageInfo->fAcousticEcho					= pImageInfo->fAcousticEcho;
	f_pChipImageInfo->fAecTailLength				= pImageInfo->fAecTailLength;
	f_pChipImageInfo->fDefaultErl					= pImageInfo->fDefaultErl;
	f_pChipImageInfo->fToneRemoval					= pImageInfo->fToneRemoval;

	f_pChipImageInfo->fNonLinearityBehaviorA		= pImageInfo->fNonLinearityBehaviorA;
	f_pChipImageInfo->fNonLinearityBehaviorB		= pImageInfo->fNonLinearityBehaviorB;
	f_pChipImageInfo->fPerChannelTailDisplacement	= pImageInfo->fPerChannelTailDisplacement;
	f_pChipImageInfo->fListenerEnhancement			= pImageInfo->fListenerEnhancement;
	f_pChipImageInfo->fRoutNoiseReduction			= pImageInfo->fRoutNoiseReduction;
	f_pChipImageInfo->fRoutNoiseReductionLevel		= pImageInfo->fRoutNoiseReductionLevel;
	f_pChipImageInfo->fAnrSnrEnhancement			= pImageInfo->fAnrSnrEnhancement;
	f_pChipImageInfo->fAnrVoiceNoiseSegregation		= pImageInfo->fAnrVoiceNoiseSegregation;
	f_pChipImageInfo->fMusicProtection				= pImageInfo->fMusicProtection;
	f_pChipImageInfo->fIdleCodeDetection			= pImageInfo->fIdleCodeDetection;
	f_pChipImageInfo->fSinLevel						= pImageInfo->fSinLevel;
	f_pChipImageInfo->fDoubleTalkBehavior			= pImageInfo->fDoubleTalkBehavior;
	f_pChipImageInfo->fHighLevelCompensation		= pImageInfo->fRinHighLevelCompensation;

	if ( ( pImageInfo->fRinAutoLevelControl == TRUE ) && ( pImageInfo->fSoutAutoLevelControl == TRUE ) )
		f_pChipImageInfo->fAutoLevelControl = TRUE;
	else
		f_pChipImageInfo->fAutoLevelControl = FALSE;
	
	f_pChipImageInfo->ulMaxChannels					= pImageInfo->usMaxNumberOfChannels;
	f_pChipImageInfo->ulNumTonesAvailable			= pImageInfo->byNumToneDetectors;
	f_pChipImageInfo->ulToneProfileNumber			= pImageInfo->ulToneProfileNumber;
	f_pChipImageInfo->ulMaxTailDisplacement			= pImageInfo->usMaxTailDisplacement;
	f_pChipImageInfo->ulMaxTailLength				= pImageInfo->usMaxTailLength;
	f_pChipImageInfo->fPerChannelTailLength			= pImageInfo->fPerChannelTailLength;
	f_pChipImageInfo->ulDebugEventSize				= f_pApiInstance->pSharedInfo->DebugInfo.ulDebugEventSize;
	f_pChipImageInfo->fToneDisablerVqeActivationDelay = pImageInfo->fToneDisablerVqeActivationDelay;
	f_pChipImageInfo->ulMaxPlayoutEvents			= pImageInfo->byMaxNumberPlayoutEvents - 1; /* 127 or 31 */
	f_pChipImageInfo->ulImageType					= pImageInfo->byImageType;

	for ( i = 0; i < cOCT6100_MAX_TONE_EVENT; i++ )
	{
		Oct6100UserMemCopy( f_pChipImageInfo->aToneInfo[ i ].aszToneName, pImageInfo->aToneInfo[ i ].aszToneName, cOCT6100_TLV_MAX_TONE_NAME_SIZE );
		f_pChipImageInfo->aToneInfo[ i ].ulDetectionPort = pImageInfo->aToneInfo[ i ].ulDetectionPort;
		f_pChipImageInfo->aToneInfo[ i ].ulToneID = pImageInfo->aToneInfo[ i ].ulToneID;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


/****************************  PRIVATE FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiChipStatsSwInit

Description:    Initializes portions of API instance associated to chip stats.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep
					the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiChipStatsSwInit
UINT32 Oct6100ApiChipStatsSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;

	/* Get local pointer to shared portion of API instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize chip stats. */
	pSharedInfo->ErrorStats.fFatalChipError = FALSE;

	pSharedInfo->ErrorStats.ulH100ClkABadCnt = 0;
	pSharedInfo->ErrorStats.ulH100ClkBBadCnt = 0;
	pSharedInfo->ErrorStats.ulH100FrameABadCnt = 0;
	pSharedInfo->ErrorStats.ulH100OutOfSyncCnt = 0;

	pSharedInfo->ErrorStats.ulInternalReadTimeoutCnt = 0;
	pSharedInfo->ErrorStats.ulSdramRefreshTooLateCnt = 0;
	pSharedInfo->ErrorStats.ulPllJitterErrorCnt	= 0;
	pSharedInfo->ErrorStats.ulOverflowToneEventsCnt = 0;


	
	pSharedInfo->ErrorStats.ulToneDetectorErrorCnt = 0;

	/* Init the chip stats. */
	pSharedInfo->ChipStats.usNumberChannels = 0;
	pSharedInfo->ChipStats.usNumberBiDirChannels = 0;
	pSharedInfo->ChipStats.usNumberTsiCncts = 0;
	pSharedInfo->ChipStats.usNumberConfBridges = 0;
	pSharedInfo->ChipStats.usNumberPlayoutBuffers = 0;
	pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts = 0;
	pSharedInfo->ChipStats.ulPlayoutMemUsed = 0;
	pSharedInfo->ChipStats.usNumEcChanUsingMixer = 0;

	pSharedInfo->ChipStats.usNumberPhasingTssts = 0;
	pSharedInfo->ChipStats.usNumberAdpcmChans	= 0;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChipGetStatsSer

Description:    Serialized function retreiving the chip statistics.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pChipStats		Pointer to master mode configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipGetStatsSer
UINT32 Oct6100ChipGetStatsSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		tPOCT6100_CHIP_STATS			f_pChipStats )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	f_pChipStats->ulNumberChannels = pSharedInfo->ChipStats.usNumberChannels;
	f_pChipStats->ulNumberTsiCncts = pSharedInfo->ChipStats.usNumberTsiCncts;
	f_pChipStats->ulNumberConfBridges = pSharedInfo->ChipStats.usNumberConfBridges;
	f_pChipStats->ulNumberPlayoutBuffers = pSharedInfo->ChipStats.usNumberPlayoutBuffers;
	f_pChipStats->ulPlayoutFreeMemSize = ( f_pApiInstance->pSharedInfo->MiscVars.ulTotalMemSize - ( f_pApiInstance->pSharedInfo->MemoryMap.ulFreeMemBaseAddress - cOCT6100_EXTERNAL_MEM_BASE_ADDRESS ) ) - ( pSharedInfo->ChipStats.ulPlayoutMemUsed );

	f_pChipStats->ulNumberPhasingTssts	= pSharedInfo->ChipStats.usNumberPhasingTssts;
	f_pChipStats->ulNumberAdpcmChannels	= pSharedInfo->ChipStats.usNumberAdpcmChans;

	/* Check the input parameters. */
	if ( f_pChipStats->fResetChipStats != TRUE && 
		 f_pChipStats->fResetChipStats != FALSE )
		return cOCT6100_ERR_CHIP_STATS_RESET;
	
	if ( f_pChipStats->fResetChipStats == TRUE )
	{
		pSharedInfo->ErrorStats.ulH100OutOfSyncCnt = 0;
		pSharedInfo->ErrorStats.ulH100ClkABadCnt = 0;
		pSharedInfo->ErrorStats.ulH100FrameABadCnt = 0;
		pSharedInfo->ErrorStats.ulH100ClkBBadCnt = 0;

		pSharedInfo->ErrorStats.ulInternalReadTimeoutCnt = 0;
		pSharedInfo->ErrorStats.ulPllJitterErrorCnt = 0;
		pSharedInfo->ErrorStats.ulSdramRefreshTooLateCnt = 0;

		pSharedInfo->ErrorStats.ulOverflowToneEventsCnt = 0;	
		pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt = 0;
		pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt = 0;


	}

	f_pChipStats->ulH100OutOfSynchCount = pSharedInfo->ErrorStats.ulH100OutOfSyncCnt;
	f_pChipStats->ulH100ClockABadCount = pSharedInfo->ErrorStats.ulH100ClkABadCnt;
	f_pChipStats->ulH100FrameABadCount = pSharedInfo->ErrorStats.ulH100FrameABadCnt;
	f_pChipStats->ulH100ClockBBadCount = pSharedInfo->ErrorStats.ulH100ClkBBadCnt;

	f_pChipStats->ulInternalReadTimeoutCount	= pSharedInfo->ErrorStats.ulInternalReadTimeoutCnt;
	f_pChipStats->ulPllJitterErrorCount			= pSharedInfo->ErrorStats.ulPllJitterErrorCnt;
	f_pChipStats->ulSdramRefreshTooLateCount	= pSharedInfo->ErrorStats.ulSdramRefreshTooLateCnt;

	f_pChipStats->ulOverflowToneEventsCount = pSharedInfo->ErrorStats.ulOverflowToneEventsCnt;
	f_pChipStats->ulSoftOverflowToneEventsCount = pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt;
	f_pChipStats->ulSoftOverflowBufferPlayoutEventsCount = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt;



	return cOCT6100_ERR_OK;
}
#endif

