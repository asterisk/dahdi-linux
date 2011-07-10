/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_debug.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to debug the OCT6100.

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

$Octasic_Revision: 65 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"
#include "oct6100api/oct6100_apiud.h"

#include "oct6100api/oct6100_apiud.h"
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
#include "oct6100api/oct6100_debug_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_debug_priv.h"
#include "oct6100_version.h"


/****************************  PUBLIC FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100DebugSelectChannel

Description:    This function sets the current debug channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pSelectDebugChan		Pointer to select debug channel structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100DebugSelectChannelDef
UINT32 Oct6100DebugSelectChannelDef(
				tPOCT6100_DEBUG_SELECT_CHANNEL	f_pSelectDebugChan )
{
	f_pSelectDebugChan->ulChannelHndl = cOCT6100_INVALID_VALUE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100DebugSelectChannel
UINT32 Oct6100DebugSelectChannel(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_DEBUG_SELECT_CHANNEL	f_pSelectDebugChan )
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
		ulFncRes = Oct6100DebugSelectChannelSer( f_pApiInstance, f_pSelectDebugChan, TRUE );
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

Function:		Oct6100DebugGetData

Description:    This function retrieves the last recorded debug data.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pGetData				Pointer to debug get data structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100DebugGetDataDef
UINT32 Oct6100DebugGetDataDef(
				tPOCT6100_DEBUG_GET_DATA			f_pGetData )
{
	f_pGetData->ulGetDataMode = cOCT6100_DEBUG_GET_DATA_MODE_120S_LITE;
	f_pGetData->ulGetDataContent = cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE;
	f_pGetData->ulRemainingNumBytes = cOCT6100_INVALID_VALUE;
	f_pGetData->ulTotalNumBytes = cOCT6100_INVALID_VALUE;
	f_pGetData->ulMaxBytes = cOCT6100_INVALID_VALUE;
	f_pGetData->ulValidNumBytes = cOCT6100_INVALID_VALUE;
	f_pGetData->pbyData = NULL;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100DebugGetData
UINT32 Oct6100DebugGetData(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_DEBUG_GET_DATA			f_pGetData )
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
		ulFncRes = Oct6100DebugGetDataSer( f_pApiInstance, f_pGetData );
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

Function:		Oct6100DebugSelectChannelSer

Description:	This function sets the debug channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pSelectDebugChan			Pointer to a tOCT6100_DEBUG_SELECT_CHANNEL structure.
f_fCheckChannelRecording	Check if channel recording is enabled or not.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100DebugSelectChannelSer
UINT32 Oct6100DebugSelectChannelSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_DEBUG_SELECT_CHANNEL			f_pSelectDebugChan,
				IN		BOOL									f_fCheckChannelRecording )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry = NULL;
	tPOCT6100_API_CHANNEL		pTempChanEntry;
	tOCT6100_CHANNEL_OPEN		TempChanOpen;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	UINT16						usChanIndex = 0;
	UINT32						ulEntryOpenCnt;
	UINT16						ausWriteData[ 2 ];
	UINT32						ulResult;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	BurstParams.pusWriteData = ausWriteData;

	/* First release the resources reserved for the channel that was previously debugged. */
	if ( pSharedInfo->DebugInfo.usCurrentDebugChanIndex != cOCT6100_INVALID_INDEX &&
		 pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
	{
		/*=======================================================================*/
		/* Get a pointer to the channel's list entry. */

		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempChanEntry, pSharedInfo->DebugInfo.usCurrentDebugChanIndex  )

		/* Release the extra TSI memory entry and reprogram the TSST control memory if required. */
		if ( pTempChanEntry->usExtraSinTsiDependencyCnt >= 1 )
		{
			/*=======================================================================*/
			/* Clear memcpy operations. */

			BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			BurstParams.ulWriteLength = 2;

			ausWriteData[ 0 ] = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
			ausWriteData[ 1 ] = 0x0;

			mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordSinEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

			mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/

			/* If we are the last dependency using the extra Sin TSI, release it */
			if ( pTempChanEntry->usExtraSinTsiDependencyCnt == 1 )
			{
				ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pTempChanEntry->usExtraSinTsiMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Do not forget to reprogram the TSST control memory. */
				if ( pTempChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pTempChanEntry->usSinTsstIndex,
																	  pTempChanEntry->usSinSoutTsiMemIndex,
																	  pTempChanEntry->TdmConfig.bySinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
				pTempChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;

				/* XXX: What about the silence TSI usSinSilenceEventIndex ?? */
			}

			pTempChanEntry->usExtraSinTsiDependencyCnt--;
			
		}
	}

	/* Set the new parameters. */
	if ( f_pSelectDebugChan->ulChannelHndl != cOCT6100_INVALID_HANDLE )
	{
		/* Check the provided handle. */
		if ( (f_pSelectDebugChan->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
			return cOCT6100_ERR_DEBUG_CHANNEL_INVALID_HANDLE;

		usChanIndex = (UINT16)( f_pSelectDebugChan->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
		if ( usChanIndex >= pSharedInfo->ChipConfig.usMaxChannels )
			return cOCT6100_ERR_DEBUG_CHANNEL_INVALID_HANDLE;

		if ( f_fCheckChannelRecording == TRUE )
		{
			if ( pSharedInfo->ChipConfig.fEnableChannelRecording == FALSE )
				return cOCT6100_ERR_DEBUG_CHANNEL_RECORDING_DISABLED;
		}

		/*=======================================================================*/
		/* Get a pointer to the channel's list entry. */

		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, usChanIndex );

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = ( f_pSelectDebugChan->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

		/* Check for errors. */
		if ( pChanEntry->fReserved != TRUE )
			return cOCT6100_ERR_CHANNEL_NOT_OPEN;
		if ( ulEntryOpenCnt != pChanEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CHANNEL_INVALID_HANDLE;
		
		/*=======================================================================*/

		/* First program the mixer entry if the user wants to record. */
		/* Check if the API needs to reserve an extra TSI memory to load the SIN signal. */
		if ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
		{
			/* Reserve the extra Sin TSI memory if it was not already reserved. */
			if ( pChanEntry->usExtraSinTsiMemIndex == cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, &pChanEntry->usExtraSinTsiMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Reprogram the TSST control memory accordingly. */
				if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pChanEntry->usSinTsstIndex,
																	  pChanEntry->usExtraSinTsiMemIndex,
																	  pChanEntry->TdmConfig.bySinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				/* XXX: What about the silence TSI usSinSilenceEventIndex ?? */
			}
		
			
			/*=======================================================================*/
			/* Program the Sout Copy event. */
			BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			BurstParams.ulWriteLength = 2;

			ausWriteData[ 0 ]  = cOCT6100_MIXER_CONTROL_MEM_COPY;
			ausWriteData[ 0 ] |= pChanEntry->usSinSoutTsiMemIndex;
			ausWriteData[ 0 ] |= pChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			ausWriteData[ 1 ]  = (UINT16)( pSharedInfo->DebugInfo.usRecordRinRoutTsiMemIndex );

			mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			/*=======================================================================*/

			/*=======================================================================*/
			/* Program the Sin copy event. */
			BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordSinEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			BurstParams.ulWriteLength = 2;

			ausWriteData[ 0 ]  = cOCT6100_MIXER_CONTROL_MEM_COPY;
			ausWriteData[ 0 ] |= pChanEntry->usExtraSinTsiMemIndex;
			ausWriteData[ 0 ] |= pChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			ausWriteData[ 1 ]  = pChanEntry->usSinSoutTsiMemIndex;

			mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			/*=======================================================================*/

			pChanEntry->usExtraSinTsiDependencyCnt++;
		}
	}
	else
	{
		/* Set the index to invalid to deactivate the recording. */
		usChanIndex = cOCT6100_INVALID_INDEX;
	}

	/* Set law of newly selected hot channel. */
	if ( ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE ) 
		&& ( f_pSelectDebugChan->ulChannelHndl != cOCT6100_INVALID_HANDLE )
		&& ( pChanEntry != NULL ) )
	{
		/* Set the PCM law of the debug channel. */
		/* Let's program the channel memory. */
		Oct6100ChannelOpenDef( &TempChanOpen );
		
		TempChanOpen.ulEchoOperationMode = cOCT6100_ECHO_OP_MODE_HT_RESET;	/* Activate the channel. */
		TempChanOpen.VqeConfig.fEnableNlp = FALSE;
		TempChanOpen.VqeConfig.ulComfortNoiseMode = cOCT6100_COMFORT_NOISE_NORMAL;
		TempChanOpen.VqeConfig.fSinDcOffsetRemoval = FALSE;
		TempChanOpen.VqeConfig.fRinDcOffsetRemoval = FALSE;
		TempChanOpen.VqeConfig.lDefaultErlDb = 0;

		/* Use the law of the channel being recorded. */
		TempChanOpen.TdmConfig.ulRinPcmLaw = pChanEntry->TdmConfig.byRinPcmLaw;
		TempChanOpen.TdmConfig.ulSinPcmLaw = pChanEntry->TdmConfig.bySinPcmLaw;
		TempChanOpen.TdmConfig.ulRoutPcmLaw = pChanEntry->TdmConfig.byRoutPcmLaw;
		TempChanOpen.TdmConfig.ulSoutPcmLaw = pChanEntry->TdmConfig.bySoutPcmLaw;

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
	}		

	ausWriteData[ 0 ] = 0x0;
	ausWriteData[ 1 ] = (UINT16)(( usChanIndex >>  0) & 0xFFFF);

	/* Write the channel number into the Matrix hot channel field.*/
	BurstParams.ulWriteAddress = pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress;
	BurstParams.pusWriteData = ausWriteData;
	BurstParams.ulWriteLength = 2;

	mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	pSharedInfo->DebugInfo.usCurrentDebugChanIndex = usChanIndex;

	/* Cancel data dump request, if there was one. */
	pSharedInfo->DebugInfo.fDebugDataBeingDumped = FALSE;
	pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes = cOCT6100_INVALID_VALUE;

	/* Call from remote client. */
	if ( f_fCheckChannelRecording == FALSE )
	{
		/* If the user has not activated recording, let the remote client know. */
		if ( pSharedInfo->ChipConfig.fEnableChannelRecording == FALSE )
			return cOCT6100_ERR_DEBUG_RC_CHANNEL_RECORDING_DISABLED;
	}
		
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100DebugGetDataSer

Description:	This function retrieves the latest recorded debug data.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pGetData				Pointer to a tOCT6100_DEBUG_GET_DATA structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100DebugGetDataSer
UINT32 Oct6100DebugGetDataSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_DEBUG_GET_DATA			f_pGetData )
{

	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry = NULL;
	tOCT6100_READ_PARAMS		ReadParams;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_BURST_PARAMS	ReadBurstParams;
	tOCT6100_WRITE_BURST_PARAMS	WriteBurstParams;

	UINT16	ausWriteData[ 2 ];
	UINT16	usReadData;
	UINT16	usDebugEventReadPtr;
	UINT16	usTempNumEvents;

	UINT32	ulResult;
	UINT32	ulToneEventIndex;
	UINT32	ulReadPointer;
	UINT32	ulUserBufWriteIndex = 0;
	UINT32	ulTimestamp;
	UINT32	ulDebugEventIndex = 0;
	UINT32	ulStreamIndex;
	UINT32	ulPcmSampleIndex;
	UINT32	ulNumAfEvents;
	UINT32	ulNumReads = 0;
	UINT32	ulTempIndex;
	UINT32	ulCopyIndex;
	UINT32	ulFeatureBytesOffset;
	UINT32	ulFeatureBitOffset;
	UINT32	ulFeatureFieldLength;
	UINT32	ulStreamIndexMin;
	UINT32	ulStreamIndexMax;
	UINT32	ulTempData;
	UINT32	ulMask;
	BOOL	fResetRemainingDataFlag = FALSE;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	ReadBurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadBurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	WriteBurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteBurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Check all user parameters. */

	/* Check if channel recording is enabled. */
	if ( pSharedInfo->ChipConfig.fEnableChannelRecording == FALSE )
		return cOCT6100_ERR_DEBUG_CHANNEL_RECORDING_DISABLED;

	/* Check if a current debugging channel has been selected. */
	/* If not, the user has not yet called Oct6100DebugSelectChannel. */
	if ( pSharedInfo->DebugInfo.usCurrentDebugChanIndex == cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_DEBUG_RECORD_NO_CHAN_SELECTED;

	/* Check that the user supplied a valid max bytes value. */
	if ( f_pGetData->ulMaxBytes == cOCT6100_INVALID_VALUE )
		return cOCT6100_ERR_DEBUG_GET_DATA_MAX_BYTES;

	/* Data buffer must be aligned on 1024 bytes. */
	if ( ( f_pGetData->ulMaxBytes % 1024 ) != 0 )
		return cOCT6100_ERR_DEBUG_GET_DATA_MAX_BYTES;

	/* Check that the user provided the required memory to transfer the information. */
	if ( f_pGetData->pbyData == NULL )
		return cOCT6100_ERR_DEBUG_GET_DATA_PTR_INVALID;

	/* Check dump type. */
	if ( ( f_pGetData->ulGetDataMode != cOCT6100_DEBUG_GET_DATA_MODE_16S_LITE )
		&& ( f_pGetData->ulGetDataMode != cOCT6100_DEBUG_GET_DATA_MODE_120S_LITE )
		&& ( f_pGetData->ulGetDataMode != cOCT6100_DEBUG_GET_DATA_MODE_16S )
		&& ( f_pGetData->ulGetDataMode != cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
		return cOCT6100_ERR_DEBUG_GET_DATA_MODE;

	/* Check dump content. */
	if ( ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
		&& ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_RIN_PCM )
		&& ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_SIN_PCM )
		&& ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_SOUT_PCM ) )
		return cOCT6100_ERR_DEBUG_GET_DATA_CONTENT;

	/* Check if can accomodate the 120 seconds dump. */
	if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S_LITE )
		|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
	{
		if ( pSharedInfo->DebugInfo.ulDebugEventSize != 0x100 )
			return cOCT6100_ERR_NOT_SUPPORTED_DEBUG_DATA_MODE_120S;
	}

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, pSharedInfo->DebugInfo.usCurrentDebugChanIndex )

	/* Lets go dump the requested data. */

	usDebugEventReadPtr = 0;

	/* Check if this is the first time this function is called since the hot channel was set. */
	if ( pSharedInfo->DebugInfo.fDebugDataBeingDumped == FALSE )
	{
		/* Check that the channel is not in POWER_DOWN.  When the channel is in POWER_DOWN, */
		/* the debug events are not recorded correctly in external memory. */
		if ( pChanEntry->byEchoOperationMode == cOCT6100_ECHO_OP_MODE_POWER_DOWN )
			return cOCT6100_ERR_DEBUG_CHANNEL_IN_POWER_DOWN;
		
		pSharedInfo->DebugInfo.fDebugDataBeingDumped = TRUE;
		
		/* Flag the hot channel that it must stop recording.  The data is being transfered. */
		/* This also tells the remote client not to do anything right now. */

		ReadBurstParams.ulReadAddress = pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress;
		ReadBurstParams.ulReadLength = 2;
		ReadBurstParams.pusReadData = pSharedInfo->DebugInfo.ausHotChannelData;

		mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteBurstParams.pusWriteData = ausWriteData;
		WriteBurstParams.ulWriteAddress = pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress;
		WriteBurstParams.ulWriteLength = 2;

		WriteBurstParams.pusWriteData[ 0 ] = 0xFFFF;
		WriteBurstParams.pusWriteData[ 1 ] = 0xFFFF;

		mOCT6100_DRIVER_WRITE_BURST_API( WriteBurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Get the maximum number of events this firmware supports from the TLVs. */
		pSharedInfo->DebugInfo.usMatrixCBMask = (UINT16)( pSharedInfo->DebugInfo.ulDebugEventSize & 0xFFFF );
		pSharedInfo->DebugInfo.usMatrixCBMask -= 1;

		/* Find out the chip log write pointer. */

		/* Now get the current write pointer for matrix events. */
		ReadParams.pusReadData = &pSharedInfo->DebugInfo.usChipDebugEventWritePtr;
		ReadParams.ulReadAddress = pSharedInfo->DebugInfo.ulMatrixWpBaseAddress + 2;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ReadParams.pusReadData = &usReadData;
		
		/* This write pointer might have wrapped, but we don't know for sure.  */
		/* To be confident, the chip frame timestamp is read. */
		ReadParams.ulReadAddress = pSharedInfo->DebugInfo.ulMatrixTimestampBaseAddress;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulTimestamp = usReadData << 16;

		ReadParams.ulReadAddress += 2;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulTimestamp |= usReadData;

		ulTimestamp >>= 12;  /* TDM time for 1 event (512 ms) */

		/* There is a probability here (once very 6.2 days) that the timestamp is close */
		/* to 0, because it has wrapped.  But still, we need a way to workaround the highly */
		/* occuring case of the chip just being opened. This will fix this problem. */
		if ( ulTimestamp < (UINT32)( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) )
		{
			if ( pSharedInfo->DebugInfo.usChipDebugEventWritePtr >= 2 )
			{
				/* Must trash the first 2 events.  The chip is not yet ready. */
				pSharedInfo->DebugInfo.usNumEvents = (UINT16)( pSharedInfo->DebugInfo.usChipDebugEventWritePtr - 2 );
			}
			else
			{
				pSharedInfo->DebugInfo.usNumEvents = 0x0;
			}
		}
		else
		{
			pSharedInfo->DebugInfo.usNumEvents = (UINT16)( pSharedInfo->DebugInfo.usMatrixCBMask + 1 );

			/* Account for event being created right now while the chip is running. */
			/* The event at the write pointer will be discarded. */
			if ( pSharedInfo->DebugInfo.usNumEvents > 0 )
				pSharedInfo->DebugInfo.usNumEvents--;
		}


		/* If the user only requested the last 16 seconds, cap the number of events. */
		if ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S
			|| f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S_LITE )
		{
			/* x events to get the last 16 seconds. */
			if ( pSharedInfo->DebugInfo.usNumEvents > ( 16000 / ( pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize / 8 ) ) )
				pSharedInfo->DebugInfo.usNumEvents = (UINT16)( ( 16000 / ( pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize / 8 ) ) & 0xFFFF );
		}

		/* Make sure that all the events are pertaining to the current hot channel. */
		/* Calculate the event read pointer. */
		ulReadPointer = ( ( pSharedInfo->DebugInfo.usChipDebugEventWritePtr - pSharedInfo->DebugInfo.usNumEvents ) & pSharedInfo->DebugInfo.usMatrixCBMask ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize;
		ulReadPointer %= ( ( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize );

		/* Travel through the events and throw away the bad events. */
		usTempNumEvents = pSharedInfo->DebugInfo.usNumEvents;
		pSharedInfo->DebugInfo.usNumEvents = 0;
		for ( ulDebugEventIndex = 0; ulDebugEventIndex < usTempNumEvents; ulDebugEventIndex ++ )
		{
			/* The HOT channel index for the event is stored at offset 0xF2 (word offset) */

			ReadParams.ulReadAddress = pSharedInfo->DebugInfo.ulMatrixBaseAddress + ulReadPointer;
			ReadParams.ulReadAddress += 0xF2 * sizeof(UINT16);
			ReadParams.pusReadData = &usReadData;

			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Check if the current debug index is the same as the one found in the event. */
			if ( usReadData != pSharedInfo->DebugInfo.usCurrentDebugChanIndex )
				pSharedInfo->DebugInfo.usNumEvents = 0; /* As soon as we hit another channel, we reset the number of valid events. */
			else
				pSharedInfo->DebugInfo.usNumEvents++;

			/* Increment read pointer to get next event. */
			ulReadPointer = ( ulReadPointer + pSharedInfo->DebugInfo.ulDebugChanStatsByteSize ) % ( ( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize );
		}

		/* In heavy mode, the AF log pointer is retrieved. */
		if ( ( pSharedInfo->DebugInfo.usNumEvents >= 2 )
			&& ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) ) )
		{
			/* The latest AF log write pointer is at the latest matrix event. */
			ReadParams.ulReadAddress = pSharedInfo->DebugInfo.ulMatrixBaseAddress + ( ( pSharedInfo->DebugInfo.usChipDebugEventWritePtr  & pSharedInfo->DebugInfo.usMatrixCBMask ) * 1024 );

			/* To get the AF log write pointer, which is at offset pSharedInfo->ImageInfo.ulAfWritePtrByteOffset. */
			ReadParams.ulReadAddress += pSharedInfo->DebugInfo.ulAfWritePtrByteOffset;
			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			pSharedInfo->DebugInfo.usAfLogWritePtr = usReadData;

			/* The AF event read pointer is the AF write pointer +4096 */
			/* This will make sure we do not get mixed up and fetch events that have */
			/* just been written, but we think are old. */

			/* To get the exact AF log pointer, the API would have to wait 512 milliseconds to make */
			/* sure logging had stopped.  This is not required since missing a few last events is not */
			/* important at this point (the user knows that valid data has already been recorded). */
			pSharedInfo->DebugInfo.usLastAfLogReadPtr = (UINT16)( ( pSharedInfo->DebugInfo.usAfLogWritePtr + 4096 ) & 0xFFFF );

			/* Note that if the chip has just been booted, some of the AF events might not be initialized. */
		}
		else
		{
			pSharedInfo->DebugInfo.usLastAfLogReadPtr = 0;
			pSharedInfo->DebugInfo.usAfLogWritePtr = 0;
		}

		/* To be aligned correctly for the bursts. */
		while ( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 ) ) != 0 )
			pSharedInfo->DebugInfo.usLastAfLogReadPtr++;

		/* Remember the data mode for later checks.  Also, the user cannot change this "mode". */
		pSharedInfo->DebugInfo.ulCurrentGetDataMode = f_pGetData->ulGetDataMode;
	}
	else
	{
		/* Check that the user did not change the current data mode. */
		if ( pSharedInfo->DebugInfo.ulCurrentGetDataMode != f_pGetData->ulGetDataMode )
			return cOCT6100_ERR_DEBUG_GET_DATA_MODE_CANNOT_CHANGE;
	}

	/* Check if this is the first pass here. */
	if ( pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes == cOCT6100_INVALID_VALUE )
	{
		/* Calculate how many bytes of data will be returned with respect to the selected data content. */
		
		/* Check what content type the user requested.  */
		if ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
		{
			/* Remember first AF Event Read Pointer. */
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr ) & 0xFF );
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr >> 8 ) & 0xFF );

			/* Remember the AF Event Write Pointer. */
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usAfLogWritePtr ) & 0xFF );
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usAfLogWritePtr >> 8 ) & 0xFF );

			/* Remember law and hot channel */
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( pChanEntry->TdmConfig.bySinPcmLaw | ( ( pSharedInfo->DebugInfo.usCurrentDebugChanIndex >> 2 ) & 0xFE ) );
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( pChanEntry->TdmConfig.bySoutPcmLaw );

			/* Insert light or heavy mode in array. */
			if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S_LITE )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S_LITE ) )
			{
				f_pGetData->pbyData[ ulUserBufWriteIndex - 1 ] |= 0x80;
			}
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( pChanEntry->TdmConfig.byRinPcmLaw | ( ( pSharedInfo->DebugInfo.usCurrentDebugChanIndex & 0x1F ) << 3 ) );

			/* Remember usNumEvents */
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usNumEvents ) & 0xFF );
			f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->DebugInfo.usNumEvents >> 8 ) & 0xFF );
		}
		
		/* Last indexes set to '0'! */
		pSharedInfo->DebugInfo.usLastDebugEventIndex = 0;
		pSharedInfo->DebugInfo.ulLastPcmSampleIndex = 0;

		/* No tone event has been retrieved. */
		pSharedInfo->DebugInfo.usLastToneEventIndex = 0;

		/* The version strings have not yet been copied. */
		pSharedInfo->DebugInfo.fImageVersionCopied = FALSE;
		pSharedInfo->DebugInfo.fApiVersionCopied = FALSE;

		/* Estimate the total size of the buffer that will be returned. */
		f_pGetData->ulTotalNumBytes = ulUserBufWriteIndex;

		/* If the full content is requested, add all the debug data. */
		if ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
		{
			/* Add the matrix events. */
			if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
			{
				/* Heavy mode!  Grab everything! */
				f_pGetData->ulTotalNumBytes += pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize;
			}
			else
			{
				/* Lite mode!  Only the most important stuff. */
				f_pGetData->ulTotalNumBytes += pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize;
			}

			/* Add the PCM samples. */
			f_pGetData->ulTotalNumBytes += pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize * 3;

			/* If requested, add the AF log events. */
			if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
			{
				f_pGetData->ulTotalNumBytes += (UINT32)( ( pSharedInfo->DebugInfo.usAfLogWritePtr - pSharedInfo->DebugInfo.usLastAfLogReadPtr ) & 0xFFFF ) * 16;
			}

			/* Add the tone events strings. */
			f_pGetData->ulTotalNumBytes += cOCT6100_TLV_MAX_TONE_NAME_SIZE * pSharedInfo->ImageInfo.byNumToneDetectors;

			/* Add the image version string. */
			f_pGetData->ulTotalNumBytes += 512;

			/* Add the API version string. */
			f_pGetData->ulTotalNumBytes += sizeof( cOCT6100_API_VERSION );
		}
		else /* if ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE ) */
		{
			/* Add one PCM stream. */
			f_pGetData->ulTotalNumBytes += pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize;
		}

		/* Save this in the instance for further calls. */
		pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes = f_pGetData->ulTotalNumBytes;

		/* Calculate remaining bytes.  All the bytes for now! */
		f_pGetData->ulRemainingNumBytes = f_pGetData->ulTotalNumBytes;

		/* Save this in the instance for the next calls. */
		pSharedInfo->DebugInfo.ulDebugDataRemainingNumBytes = f_pGetData->ulRemainingNumBytes;
	}
	else
	{
		f_pGetData->ulTotalNumBytes = pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes;
	}

	/* Calculate the event read pointer. */
	ulReadPointer = ( ( pSharedInfo->DebugInfo.usChipDebugEventWritePtr - pSharedInfo->DebugInfo.usNumEvents ) & pSharedInfo->DebugInfo.usMatrixCBMask ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize;

	ulReadPointer += pSharedInfo->DebugInfo.ulDebugChanStatsByteSize * pSharedInfo->DebugInfo.usLastDebugEventIndex;
	ulReadPointer %= ( ( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize );

	if ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
	{
		/* Copy the debug events in the user buffer. */
		for( ulDebugEventIndex = pSharedInfo->DebugInfo.usLastDebugEventIndex; ulDebugEventIndex < pSharedInfo->DebugInfo.usNumEvents; ulDebugEventIndex ++ )
		{
			ReadBurstParams.ulReadAddress = pSharedInfo->DebugInfo.ulMatrixBaseAddress + ulReadPointer;

			/* Check if we are in light or heavy mode.  The burst size is not the same. */
			if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
			{
				if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) >= pSharedInfo->DebugInfo.ulDebugChanStatsByteSize )
					ulNumReads = pSharedInfo->DebugInfo.ulDebugChanStatsByteSize / 2;
				else
					break;
			}
			else
			{
				if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) >= pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize )
					ulNumReads = pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize / 2;
				else
					break;
			}

			ulTempIndex = 0;
			while ( ulNumReads != 0 )
			{
				if ( ulNumReads >= pSharedInfo->ChipConfig.usMaxRwAccesses )
					ReadBurstParams.ulReadLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
				else
					ReadBurstParams.ulReadLength = ulNumReads;

				/* Set pointer where to write data. */
				ReadBurstParams.pusReadData = pSharedInfo->MiscVars.ausSuperArray;

				mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Copy data byte per byte to avoid endianess problems. */
				for ( ulCopyIndex = 0; ulCopyIndex < ReadBurstParams.ulReadLength; ulCopyIndex ++ )
				{
					f_pGetData->pbyData[ ulUserBufWriteIndex + ulTempIndex + ( 2 * ulCopyIndex ) ] = (UINT8)( ReadBurstParams.pusReadData[ ulCopyIndex ] & 0xFF );
					f_pGetData->pbyData[ ulUserBufWriteIndex + ulTempIndex + ( 2 * ulCopyIndex ) + 1 ] = (UINT8)( ( ReadBurstParams.pusReadData[ ulCopyIndex ] >> 8 ) & 0xFF );
				}

				/* Update indexes, temp variables, addresses. */
				ulNumReads -= ReadBurstParams.ulReadLength;
				ulTempIndex += ReadBurstParams.ulReadLength * 2;
				ReadBurstParams.ulReadAddress += ReadBurstParams.ulReadLength * 2;
			}

			/* Store register 0x202 in the event structure. */
			f_pGetData->pbyData[ ulUserBufWriteIndex + 255 ] = (UINT8)( pSharedInfo->IntrptManage.usRegister202h & 0xFF );
			f_pGetData->pbyData[ ulUserBufWriteIndex + 256 ] = (UINT8)( ( pSharedInfo->IntrptManage.usRegister202h >> 8 ) & 0xFF );

			/* Increment index. */
			if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
				|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
			{
				ulUserBufWriteIndex += pSharedInfo->DebugInfo.ulDebugChanStatsByteSize;
			}
			else
			{
				ulUserBufWriteIndex += pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize;
			}

			/* Increment read pointer to get next event. */
			ulReadPointer = ( ulReadPointer + pSharedInfo->DebugInfo.ulDebugChanStatsByteSize ) % ( ( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) * pSharedInfo->DebugInfo.ulDebugChanStatsByteSize );

			/* Save in the instance that one of the events was dumped. */
			pSharedInfo->DebugInfo.usLastDebugEventIndex ++;
		}
	}

	/* Check if all debug events have been transfered. */
	if ( ( ulDebugEventIndex == pSharedInfo->DebugInfo.usNumEvents )
		|| ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE ) )
	{
		/* Fetch all streams per event. */
		for ( ulPcmSampleIndex = pSharedInfo->DebugInfo.ulLastPcmSampleIndex; ulPcmSampleIndex < ( (UINT32)pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize ); ulPcmSampleIndex ++ )
		{
			/* Check if enough room for this sample. */
			if ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
			{
				if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) < 1 )
					break;
			}
			else /* if ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE ) */
			{
				if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) < 3 )
					break;
			}
			
			/* Check if must retrieve data from external memory. */
			if ( ( ulPcmSampleIndex % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE * 2 ) ) == 0x0 )
			{
				ulReadPointer = ( ( ( pSharedInfo->DebugInfo.usChipDebugEventWritePtr - pSharedInfo->DebugInfo.usNumEvents ) * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize ) & ( pSharedInfo->DebugInfo.usMatrixCBMask * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize ) );
				ulReadPointer += ( ulPcmSampleIndex / pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize ) * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize;
				ulReadPointer &= ( pSharedInfo->DebugInfo.usMatrixCBMask * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize );
				ulReadPointer += ulPcmSampleIndex % pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize;

				/* Retrieve more data from external memory. */
				switch ( f_pGetData->ulGetDataContent )
				{
				case cOCT6100_DEBUG_GET_DATA_CONTENT_RIN_PCM:
					ulStreamIndexMin = 0;
					ulStreamIndexMax = 1;
					break;
				case cOCT6100_DEBUG_GET_DATA_CONTENT_SIN_PCM:
					ulStreamIndexMin = 1;
					ulStreamIndexMax = 2;
					break;
				case cOCT6100_DEBUG_GET_DATA_CONTENT_SOUT_PCM:
					ulStreamIndexMin = 2;
					ulStreamIndexMax = 3;
					break;
				case cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE:
				default:
					ulStreamIndexMin = 0;
					ulStreamIndexMax = 3;
					break;
				}

				for ( ulStreamIndex = ulStreamIndexMin; ulStreamIndex < ulStreamIndexMax; ulStreamIndex ++ )
				{
					ReadBurstParams.ulReadAddress = pSharedInfo->MemoryMap.ulChanMainMemBase;
					/* To get right channel information. */
					ReadBurstParams.ulReadAddress += ( ( pSharedInfo->DebugInfo.usRecordMemIndex + 2 ) * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->DebugInfo.ulAfEventCbByteSize;
					/* To get correct stream. */
					ReadBurstParams.ulReadAddress += ( ( pSharedInfo->DebugInfo.usMatrixCBMask + 1 ) * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize * ulStreamIndex );
					/* PCM sample pointer in that stream. */
					ReadBurstParams.ulReadAddress += ulReadPointer;

					/* As much as we can for the burst. */
					ulTempIndex = 0;
					ulNumReads = cOCT6100_INTERNAL_SUPER_ARRAY_SIZE;
					while ( ulNumReads != 0 )
					{
						if ( ulNumReads >= pSharedInfo->ChipConfig.usMaxRwAccesses )
							ReadBurstParams.ulReadLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
						else
							ReadBurstParams.ulReadLength = ulNumReads;

						/* Set pointer where to write data. */
						if ( ulStreamIndex == 0 )
							ReadBurstParams.pusReadData = &pSharedInfo->MiscVars.ausSuperArray[ ulTempIndex ];
						else if ( ulStreamIndex == 1 )
							ReadBurstParams.pusReadData = &pSharedInfo->MiscVars.ausSuperArray1[ ulTempIndex ];
						else /* if ( ulStreamIndex == 2 ) */
							ReadBurstParams.pusReadData = &pSharedInfo->MiscVars.ausSuperArray2[ ulTempIndex ];

						mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						/* Update indexes, temp variables, addresses. */
						ulNumReads -= ReadBurstParams.ulReadLength;
						ulTempIndex += ReadBurstParams.ulReadLength;
						ReadBurstParams.ulReadAddress += ReadBurstParams.ulReadLength * 2;
					}
				}
			}

			/* We now have the stream data for all streams for 1 event. */
			/* Return what we can to the user. */
			if ( ( ulPcmSampleIndex % 2 ) == 0 )
			{
				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_RIN_PCM ) )
					f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 8 ) & 0xFF );

				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_SIN_PCM ) )
					f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray1[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 8 ) & 0xFF );
				
				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_SOUT_PCM ) )
					f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray2[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 8 ) & 0xFF );
			}
			else /* if ( ulPcmSampleIndex % 2 == 1 ) */
			{
				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_RIN_PCM ) )
				f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 0 ) & 0xFF );

				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_SIN_PCM ) )
					f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray1[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 0 ) & 0xFF );

				if ( ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
					|| ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_SOUT_PCM ) )
					f_pGetData->pbyData[ ulUserBufWriteIndex++ ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray2[ ( ulPcmSampleIndex / 2 ) % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE ) ] >> 0 ) & 0xFF );
			}

			pSharedInfo->DebugInfo.ulLastPcmSampleIndex++;
		}

		/* Check if we are done dumping the PCM samples! */
		if ( pSharedInfo->DebugInfo.ulLastPcmSampleIndex == ( (UINT32)pSharedInfo->DebugInfo.usNumEvents * pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize ) )
		{
			if ( f_pGetData->ulGetDataContent == cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE )
			{

				/* Go for the AF events.  The AF events are only copied in heavy mode. */
				if ( ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_16S )
					|| ( f_pGetData->ulGetDataMode == cOCT6100_DEBUG_GET_DATA_MODE_120S ) )
				{
					while ( pSharedInfo->DebugInfo.usLastAfLogReadPtr != pSharedInfo->DebugInfo.usAfLogWritePtr )
					{
						/* Check if enough room for an event. */
						if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) < 16 )
							break;

						/* Check if must fill our buffer. */
						if ( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 ) ) == 0x0 )
						{
							ulNumAfEvents = ( pSharedInfo->DebugInfo.usAfLogWritePtr - pSharedInfo->DebugInfo.usLastAfLogReadPtr ) & 0xFFFF;

							/* Check for the size of the available buffer. */
							if ( ulNumAfEvents > ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 ) ) 
								ulNumAfEvents = ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 );

							/* Start at channel main base address. */
							ReadBurstParams.ulReadAddress = pSharedInfo->MemoryMap.ulChanMainMemBase;
							/* To get right channel information. */
							ReadBurstParams.ulReadAddress += ( ( pSharedInfo->DebugInfo.usRecordMemIndex + 2 ) * pSharedInfo->MemoryMap.ulChanMainMemSize );
							/* To get the right AF log. */
							ReadBurstParams.ulReadAddress += ( pSharedInfo->DebugInfo.usLastAfLogReadPtr * 16 );

							ulTempIndex = 0;
							ulNumReads = ulNumAfEvents * 8;

							while ( ulNumReads != 0 )
							{
								if ( ulNumReads >= pSharedInfo->ChipConfig.usMaxRwAccesses )
									ReadBurstParams.ulReadLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
								else
									ReadBurstParams.ulReadLength = ulNumReads;

								/* Set pointer where to write data. */
								ReadBurstParams.pusReadData = &pSharedInfo->MiscVars.ausSuperArray[ ulTempIndex ];

								mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult );
								if ( ulResult != cOCT6100_ERR_OK )
									return ulResult;

								/* Update indexes, temp variables, addresses. */
								ulNumReads -= ReadBurstParams.ulReadLength;
								ulTempIndex += ReadBurstParams.ulReadLength;
								ReadBurstParams.ulReadAddress += ReadBurstParams.ulReadLength * 2;
							}
						}

						/* Copy data byte per byte to avoid endianess problems. */
						for ( ulCopyIndex = 0; ulCopyIndex < 8; ulCopyIndex ++ )
						{
							f_pGetData->pbyData[ ulUserBufWriteIndex + ( 2 * ulCopyIndex ) ] = (UINT8)( pSharedInfo->MiscVars.ausSuperArray[ ( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 ) ) * 8 ) + ulCopyIndex ] & 0xFF );
							f_pGetData->pbyData[ ulUserBufWriteIndex + ( 2 * ulCopyIndex ) + 1 ] = (UINT8)( ( pSharedInfo->MiscVars.ausSuperArray[ ( ( pSharedInfo->DebugInfo.usLastAfLogReadPtr % ( cOCT6100_INTERNAL_SUPER_ARRAY_SIZE / 8 ) ) * 8 ) + ulCopyIndex ] >> 8 ) & 0xFF );
						}

						ulUserBufWriteIndex += 16;

						/* Increment AF log read ptr. */
						pSharedInfo->DebugInfo.usLastAfLogReadPtr = (UINT16)(( pSharedInfo->DebugInfo.usLastAfLogReadPtr + 1 ) & 0xFFFF );
					}
				}

				/* Check if we are done with the AF events. */
				if ( pSharedInfo->DebugInfo.usLastAfLogReadPtr == pSharedInfo->DebugInfo.usAfLogWritePtr )
				{
					/* Insert the tone event information. */
					for ( ulToneEventIndex = pSharedInfo->DebugInfo.usLastToneEventIndex; ulToneEventIndex < pSharedInfo->ImageInfo.byNumToneDetectors; ulToneEventIndex++ )
					{
						if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) < cOCT6100_TLV_MAX_TONE_NAME_SIZE )
							break;

						Oct6100UserMemCopy( &f_pGetData->pbyData[ ulUserBufWriteIndex ], pSharedInfo->ImageInfo.aToneInfo[ ulToneEventIndex ].aszToneName, cOCT6100_TLV_MAX_TONE_NAME_SIZE );

						ulUserBufWriteIndex += cOCT6100_TLV_MAX_TONE_NAME_SIZE;

						pSharedInfo->DebugInfo.usLastToneEventIndex++;
					}

					/* If all the tone information has been copied. */
					if ( ulToneEventIndex == pSharedInfo->ImageInfo.byNumToneDetectors )
					{
						/* Copy the image version. */
						if ( pSharedInfo->DebugInfo.fImageVersionCopied == FALSE )
						{
							if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) >= 512 )
							{
								Oct6100UserMemCopy( &f_pGetData->pbyData[ ulUserBufWriteIndex ], pSharedInfo->ImageInfo.szVersionNumber, 512 );

								/* Get PLL jitter count from external memory. */
								if ( pSharedInfo->DebugInfo.fPouchCounter == TRUE )
								{
									ulFeatureBytesOffset = pSharedInfo->MemoryMap.PouchCounterFieldOfst.usDwordOffset * 4;
									ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byBitOffset;
									ulFeatureFieldLength = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byFieldSize;

									ulResult = Oct6100ApiReadDword(	f_pApiInstance,
																	cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
																	&ulTempData );
									
									/* Create the mask to retrieve the appropriate value. */
									mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

									/* Mask data. */
									ulTempData &= ulMask;
									/* Move to right position. */
									ulTempData = ulTempData >> ulFeatureBitOffset;

									f_pGetData->pbyData[ ulUserBufWriteIndex + 510 ] = (UINT8)( ( ulTempData >> 8 ) & 0xFF );
									f_pGetData->pbyData[ ulUserBufWriteIndex + 511 ] = (UINT8)( ( ulTempData >> 0 ) & 0xFF );
								}

								/* Add "ISR is not called" bit. */
								if ( pSharedInfo->IntrptManage.fIsrCalled == FALSE )
								{
									f_pGetData->pbyData[ ulUserBufWriteIndex + 510 ] |= 0x80;
								}

								ulUserBufWriteIndex += 512;

								/* The version has been copied. */
								pSharedInfo->DebugInfo.fImageVersionCopied = TRUE;
							}
						}

						/* If the image version has been copied, proceed with the API version. */
						if ( pSharedInfo->DebugInfo.fImageVersionCopied == TRUE )
						{
							if ( pSharedInfo->DebugInfo.fApiVersionCopied == FALSE )
							{
								if ( ( f_pGetData->ulMaxBytes - ulUserBufWriteIndex ) >= sizeof(cOCT6100_API_VERSION) )
								{
									Oct6100UserMemCopy( &f_pGetData->pbyData[ ulUserBufWriteIndex ], cOCT6100_API_VERSION, sizeof(cOCT6100_API_VERSION) );
									ulUserBufWriteIndex += sizeof(cOCT6100_API_VERSION);

									/* The API version has been copied. */
									pSharedInfo->DebugInfo.fApiVersionCopied = TRUE;
								}
							}
						}
					}

					/* Check if we are done! */
					if ( pSharedInfo->DebugInfo.fApiVersionCopied == TRUE )
					{
						/* Done dumping. */

						/* Reset data being dumpped flag. */
						pSharedInfo->DebugInfo.fDebugDataBeingDumped = FALSE;

						/* Reset data recording in the chip. */
						WriteBurstParams.ulWriteAddress = pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress;
						WriteBurstParams.ulWriteLength = 2;
						WriteBurstParams.pusWriteData = pSharedInfo->DebugInfo.ausHotChannelData;

						mOCT6100_DRIVER_WRITE_BURST_API( WriteBurstParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						fResetRemainingDataFlag = TRUE;
					}
				}
			}
			else /* if ( f_pGetData->ulGetDataContent != cOCT6100_DEBUG_GET_DATA_CONTENT_COMPLETE ) */
			{
				fResetRemainingDataFlag = TRUE;
			}
		}
	}

	/* Return number of valid bytes in buffer to user. */
	f_pGetData->ulValidNumBytes = ulUserBufWriteIndex;

	/* Update remaining bytes. */
	pSharedInfo->DebugInfo.ulDebugDataRemainingNumBytes -= ulUserBufWriteIndex;

	/* Return remaining bytes. */
	f_pGetData->ulRemainingNumBytes = pSharedInfo->DebugInfo.ulDebugDataRemainingNumBytes;

	/* Return total number of bytes. */
	f_pGetData->ulTotalNumBytes = pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes;

	/* Check if we are done dump the requested content. */
	if ( fResetRemainingDataFlag == TRUE )
		pSharedInfo->DebugInfo.ulDebugDataTotalNumBytes = cOCT6100_INVALID_VALUE;

	return cOCT6100_ERR_OK;
}
#endif
