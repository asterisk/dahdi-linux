/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_tone_detection.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to enable and disable tone detection on 
	an echo channel.

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

$Octasic_Revision: 51 $

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
#include "oct6100api/oct6100_tone_detection_inst.h"
#include "oct6100api/oct6100_events_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_tone_detection_pub.h"
#include "oct6100api/oct6100_events_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_tone_detection_priv.h"
#include "oct6100_events_priv.h"


/****************************  PUBLIC FUNCTIONS  *****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ToneDetectionEnable

Description:    This function enables the generation of event for a selected 
				tone on the specified channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectEnable		Pointer to tone detection enable structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ToneDetectionEnableDef
UINT32 Oct6100ToneDetectionEnableDef(
				tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable )
{
	f_pToneDetectEnable->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pToneDetectEnable->ulToneNumber = cOCT6100_INVALID_TONE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ToneDetectionEnable
UINT32 Oct6100ToneDetectionEnable(
				tPOCT6100_INSTANCE_API					f_pApiInstance,
				tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable )
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
		ulFncRes = Oct6100ToneDetectionEnableSer( f_pApiInstance, f_pToneDetectEnable );
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

Function:		Oct6100ToneDetectionDisable

Description:    This function disables the detection of a tone for a specific 
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectDisable	Pointer to tone detection disable structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ToneDetectionDisableDef
UINT32 Oct6100ToneDetectionDisableDef(
				tPOCT6100_TONE_DETECTION_DISABLE			f_pToneDetectDisable )
{
	f_pToneDetectDisable->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pToneDetectDisable->ulToneNumber = cOCT6100_INVALID_VALUE;
	f_pToneDetectDisable->fDisableAll = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ToneDetectionDisable
UINT32 Oct6100ToneDetectionDisable(
				tPOCT6100_INSTANCE_API						f_pApiInstance,
				tPOCT6100_TONE_DETECTION_DISABLE			f_pToneDetectDisable )
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
		ulFncRes = Oct6100ToneDetectionDisableSer( f_pApiInstance, f_pToneDetectDisable );
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

Function:		Oct6100ToneDetectionEnableSer

Description:	Activate the detection of a tone on the specified channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectEnable		Pointer to tone detect enable structure.  This structure
						contains, among other things, the tone ID to enable
						and the channel handle where detection should be
						enabled.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ToneDetectionEnableSer
UINT32 Oct6100ToneDetectionEnableSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable )
{
	UINT32	ulChanIndex;
	UINT32	ulExtToneChanIndex;
	UINT32	ulToneEventNumber = 0;

	UINT32	ulResult;

	/* Check the user's configuration of the tone detection for errors. */
	ulResult = Oct6100ApiCheckToneEnableParams( 
											f_pApiInstance, 
											f_pToneDetectEnable, 
											&ulChanIndex, 
											&ulToneEventNumber, 

											&ulExtToneChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write to  all resources needed to enable tone detection. */
	ulResult = Oct6100ApiWriteToneDetectEvent( 
											f_pApiInstance, 
											ulChanIndex, 
											ulToneEventNumber, 

											ulExtToneChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the channel entry to indicate that a new tone has been activated. */
	ulResult = Oct6100ApiUpdateChanToneDetectEntry( 
											f_pApiInstance, 
											ulChanIndex, 
											ulToneEventNumber, 
											ulExtToneChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckToneEnableParams

Description:	Check the validity of the channel and tone requested.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectEnable		Pointer to tone detection enable structure.  
f_pulChannelIndex		Pointer to the channel index.
f_pulToneEventNumber	Pointer to the Index of the tone associated to the requested tone.
f_pulExtToneChanIndex	Pointer to the index of the extended channel index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckToneEnableParams
UINT32 Oct6100ApiCheckToneEnableParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable,
				OUT		PUINT32									f_pulChannelIndex,
				OUT		PUINT32									f_pulToneEventNumber,

				OUT		PUINT32									f_pulExtToneChanIndex )
{
	tPOCT6100_API_CHANNEL		pEchoChannel;
	UINT32	ulEntryOpenCnt;
	UINT32	i;

	/*=====================================================================*/
	/* Check the channel handle. */

	if ( (f_pToneDetectEnable->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	*f_pulChannelIndex = f_pToneDetectEnable->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK;
	if ( *f_pulChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, *f_pulChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pToneDetectEnable->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChannel->fReserved != TRUE )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChannel->byEntryOpenCnt )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	/* Set the extended tone detection info if it is activated on the channel. */
	*f_pulExtToneChanIndex = pEchoChannel->usExtToneChanIndex;

	/*=====================================================================*/
	/* Check the tone information. */

	/* Find out if the tone is present in the build. */
	for ( i = 0; i < cOCT6100_MAX_TONE_EVENT; i++ )
	{
		if ( f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ i ].ulToneID == f_pToneDetectEnable->ulToneNumber )
		{
			*f_pulToneEventNumber = i;
			break;
		}
	}
	
	/* Check if tone is present. */
	if ( i == cOCT6100_MAX_TONE_EVENT )
		return cOCT6100_ERR_NOT_SUPPORTED_TONE_NOT_PRESENT_IN_FIRMWARE;

	/* Check if the requested tone is actually detected. */
	if ((( pEchoChannel->aulToneConf[ *f_pulToneEventNumber / 32 ] >> ( 31 - ( *f_pulToneEventNumber % 32 ))) & 0x1) == 1 )
		return cOCT6100_ERR_TONE_DETECTION_TONE_ACTIVATED;


	
	/*=====================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteToneDetectEvent

Description:    Write the tone detection event in the channel main structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulChannelIndex		Index of the channel within the API's channel list.
f_ulToneEventNumber		Event number of the tone to be activated.
f_ulExtToneChanIndex	Index of the extended tone detection channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteToneDetectEvent
UINT32 Oct6100ApiWriteToneDetectEvent(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,

				IN		UINT32									f_ulExtToneChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_READ_PARAMS			ReadParams;
	UINT32	ulResult;
	UINT16	usReadData;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;



	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*=======================================================================*/
	/* Read the current event config about to be modified. */

	ReadParams.ulReadAddress  = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_ulChannelIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
	ReadParams.ulReadAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;
	ReadParams.ulReadAddress += (f_ulToneEventNumber / 16) * 2;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*=======================================================================*/
	/* Set the tone event in the channel main memory for the requested direction. */
	
	WriteParams.ulWriteAddress  = ReadParams.ulReadAddress;
	WriteParams.usWriteData  = usReadData;
	WriteParams.usWriteData |= ( 0x1 << ( 15 - ( f_ulToneEventNumber % 16 )));

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*=======================================================================*/
	/* Also program the extended channel if one is present. */

	if ( f_ulExtToneChanIndex != cOCT6100_INVALID_INDEX )
	{
		/* Read the current event config about to be modified. */
		ReadParams.ulReadAddress  = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_ulExtToneChanIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
		ReadParams.ulReadAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;
		ReadParams.ulReadAddress += (f_ulToneEventNumber / 16) * 2;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Write the tone event in the channel main memory for the requested direction. */
		WriteParams.ulWriteAddress  = ReadParams.ulReadAddress;
		WriteParams.usWriteData  = usReadData;
		WriteParams.usWriteData |= ( 0x1 << ( 15 - ( f_ulToneEventNumber % 16 )));

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateChanToneDetectEntry

Description:    Update the echo channel entry to store the info about the tone
				being configured to generate detection events.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_ulChannelIndex		Index of the channel within the API's channel list.
f_ulToneEventNumber		Enabled tone event number.
f_ulExtToneChanIndex	Index of the extended tone detection channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateChanToneDetectEntry
UINT32 Oct6100ApiUpdateChanToneDetectEntry (
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex )
{
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32	ulToneEntry;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Update the channel entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_ulChannelIndex );

	/* Set the corresponding bit in the channel array. */
	ulToneEntry = pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ];

	/* Modify the entry. */
	ulToneEntry |= ( 0x1 << ( 31 - ( f_ulToneEventNumber % 32 )));

	/* Copy back the new value. */
	pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ] = ulToneEntry;
	
	/* Configure also the extended channel if necessary. */
	if ( f_ulExtToneChanIndex != cOCT6100_INVALID_INDEX )
	{
		/* Update the channel entry. */
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_ulExtToneChanIndex );

		/* Set the corresponding bit in the channel array. */
		ulToneEntry = pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ];

		/* Modify the entry. */
		ulToneEntry |= ( 0x1 << ( 31 - ( f_ulToneEventNumber % 32 )));

		/* Copy back the new value. */
		pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ] = ulToneEntry;
	}

	/* Check for the SS tone events that could have been generated before. */
	if ( f_ulExtToneChanIndex == cOCT6100_INVALID_INDEX )
	{
		BOOL fSSTone;
		UINT32 ulResult;

		ulResult = Oct6100ApiIsSSTone( f_pApiInstance, pSharedInfo->ImageInfo.aToneInfo[ f_ulToneEventNumber ].ulToneID, &fSSTone );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Is this a signaling system tone? */
		if ( fSSTone == TRUE )
		{
			/* Check if must generate an event for the last detected SS tone. */
			if ( ( pEchoChanEntry->ulLastSSToneDetected != cOCT6100_INVALID_INDEX )
				&& ( pEchoChanEntry->ulLastSSToneDetected == pSharedInfo->ImageInfo.aToneInfo[ f_ulToneEventNumber ].ulToneID ) )
			{
				/* Must write an event for this. */
				tPOCT6100_API_TONE_EVENT pSoftEvent;
				
				/* If enough space. */
				if ( ( ( pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1 ) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr ) &&
					( ( pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1 ) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0 ) )
				{
					/* Form the event for this captured tone. */
					mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
					pSoftEvent += pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

					pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_ulChannelIndex; 
					pSoftEvent->ulUserChanId = pEchoChanEntry->ulUserChanId;
					pSoftEvent->ulToneDetected = pSharedInfo->ImageInfo.aToneInfo[ f_ulToneEventNumber ].ulToneID;
					pSoftEvent->ulTimestamp = pEchoChanEntry->ulLastSSToneTimestamp;
					pSoftEvent->ulExtToneDetectionPort = cOCT6100_INVALID_VALUE;
					pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;

					/* Update the control variables of the buffer. */
					pSharedInfo->SoftBufs.ulToneEventBufferWritePtr++;
					if ( pSharedInfo->SoftBufs.ulToneEventBufferWritePtr == pSharedInfo->SoftBufs.ulToneEventBufferSize )
						pSharedInfo->SoftBufs.ulToneEventBufferWritePtr = 0;

					/* Set the interrupt manager such that the user knows that some tone events */
					/* are pending in the software Q. */
					pSharedInfo->IntrptManage.fToneEventsPending = TRUE;
				}
				else
				{
					/* Set the overflow flag of the buffer. */
					pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt++;
				}
			}
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ToneDetectionDisableSer

Description:    Disable the generation of events for a selected tone on the 
				specified channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectDisable	Pointer to tOCT6100_TONE_DETECTION_DISABLE structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ToneDetectionDisableSer
UINT32 Oct6100ToneDetectionDisableSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_DISABLE		f_pToneDetectDisable )
{
	UINT32	ulChanIndex;
	UINT32	ulExtToneChanIndex;
	UINT32	ulToneEventNumber = 0;
	UINT32	ulResult;
	BOOL	fDisableAll;


	/* Check the user's configuration of the tone detection disable structure for errors. */
	ulResult = Oct6100ApiAssertToneDetectionParams( 
												f_pApiInstance, 
												f_pToneDetectDisable, 
												&ulChanIndex, 
												&ulToneEventNumber, 
												&ulExtToneChanIndex, 

												&fDisableAll );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Clear the event to detect the specified tone. */
	ulResult = Oct6100ApiClearToneDetectionEvent( 
												f_pApiInstance, 
												ulChanIndex, 
												ulToneEventNumber, 
												ulExtToneChanIndex, 

												fDisableAll );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the channel structure to indicate that the tone is no longer detected. */
	ulResult = Oct6100ApiReleaseToneDetectionEvent( 
												f_pApiInstance, 
												ulChanIndex, 
												ulToneEventNumber, 
												ulExtToneChanIndex, 
												fDisableAll );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertToneDetectionParams

Description:	Check the validity of the tone detection disable command.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pToneDetectDisable	Pointer to tone detection disable structure.  
f_pulChannelIndex		Pointer to the channel index
f_pulToneEventNumber	Pointer to the tone event number.
f_pulExtToneChanIndex	Pointer to the extended channel index.
f_pfDisableAll			Pointer to the flag specifying whether all tones
						should be disabled.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertToneDetectionParams
UINT32 Oct6100ApiAssertToneDetectionParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_DISABLE		f_pToneDetectDisable,
				OUT		PUINT32									f_pulChannelIndex,
				OUT		PUINT32									f_pulToneEventNumber,
				OUT		PUINT32									f_pulExtToneChanIndex,

				OUT		PBOOL									f_pfDisableAll )
{
	tPOCT6100_API_CHANNEL		pEchoChannel;
	UINT32	ulEntryOpenCnt;
	UINT32	i;

	/*=====================================================================*/
	/* Check the echo channel handle. */

	if ( (f_pToneDetectDisable->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	*f_pulChannelIndex = f_pToneDetectDisable->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK;
	if ( *f_pulChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, *f_pulChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pToneDetectDisable->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChannel->fReserved != TRUE )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChannel->byEntryOpenCnt )
		return cOCT6100_ERR_TONE_DETECTION_CHANNEL_HANDLE_INVALID;

	/* Return the extended channel index. */
	*f_pulExtToneChanIndex = pEchoChannel->usExtToneChanIndex;

	/* Check the disable all flag. */
	if ( f_pToneDetectDisable->fDisableAll != TRUE && f_pToneDetectDisable->fDisableAll != FALSE )
		return cOCT6100_ERR_TONE_DETECTION_DISABLE_ALL;
	
	/*=====================================================================*/
	/* Check the tone information. */

	/* Find out if the tone is present in the build. */
	if ( f_pToneDetectDisable->fDisableAll == FALSE )
	{
		for ( i = 0; i < cOCT6100_MAX_TONE_EVENT; i++ )
		{
			if ( f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ i ].ulToneID == f_pToneDetectDisable->ulToneNumber )
			{
				*f_pulToneEventNumber = i;
				break;
			}
		}
		
		/* Check if tone is present. */
		if ( i == cOCT6100_MAX_TONE_EVENT )
			return cOCT6100_ERR_NOT_SUPPORTED_TONE_NOT_PRESENT_IN_FIRMWARE;



		/* Check if the requested tone is actually detected. */
		if ((( pEchoChannel->aulToneConf[ *f_pulToneEventNumber / 32 ] >> ( 31 - ( *f_pulToneEventNumber % 32 ))) & 0x1) == 0 )
			return cOCT6100_ERR_TONE_DETECTION_TONE_NOT_ACTIVATED;
	}

	
	/*=====================================================================*/

	/* Return the disable all flag as requested. */
	*f_pfDisableAll = f_pToneDetectDisable->fDisableAll;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiClearToneDetectionEvent

Description:    Clear the buffer playout event in the channel main structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_ulChannelIndex			Index of the channel within the API's channel list.
f_ulToneEventNumber			Tone event number to be deactivated.
f_ulExtToneChanIndex		Index of the extended tone detection channel.
f_fDisableAll				Clear all activated tones.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiClearToneDetectionEvent
UINT32 Oct6100ApiClearToneDetectionEvent(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex,

				IN		BOOL									f_fDisableAll )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_READ_PARAMS			ReadParams;
	tOCT6100_WRITE_SMEAR_PARAMS		SmearParams;
	UINT32	ulResult;
	UINT32	ulToneEventBaseAddress;
	UINT16	usReadData;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;


		
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	SmearParams.pProcessContext = f_pApiInstance->pProcessContext;

	SmearParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*=======================================================================*/
	/* Read the current event config about to be modified. */

	ulToneEventBaseAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_ulChannelIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
	ulToneEventBaseAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;

	/* Check if must disable all tone events or not. */
	if ( f_fDisableAll == FALSE )
	{
		ReadParams.ulReadAddress  = ulToneEventBaseAddress;
		ReadParams.ulReadAddress += (f_ulToneEventNumber / 16) * 2;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Clear the event in the channel main memory.*/
		WriteParams.ulWriteAddress  = ReadParams.ulReadAddress;
		WriteParams.usWriteData  = usReadData;
		WriteParams.usWriteData &= (~( 0x1 << ( 15 - ( f_ulToneEventNumber % 16 ))));

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	else /* if ( f_fDisableAll == TRUE ) */
	{
		/* Clear all events in the channel main memory. */
		SmearParams.ulWriteLength = 4;
		SmearParams.usWriteData = 0x0000;
		SmearParams.ulWriteAddress = ulToneEventBaseAddress;
		mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	
	/*=======================================================================*/
	/* Also program the extended channel if one is present. */

	if ( f_ulExtToneChanIndex != cOCT6100_INVALID_INDEX )
	{
		ulToneEventBaseAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_ulExtToneChanIndex * pSharedInfo->MemoryMap.ulChanMainMemSize );
		ulToneEventBaseAddress += cOCT6100_CH_MAIN_TONE_EVENT_OFFSET;

		/* Check if must disable all tone events or not. */
		if ( f_fDisableAll == FALSE )
		{
			/* Read the current event config about to be modified. */
			ReadParams.ulReadAddress  = ulToneEventBaseAddress;
			ReadParams.ulReadAddress += (f_ulToneEventNumber / 16) * 2;

			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Clear the event in the channel main memory.*/
			WriteParams.ulWriteAddress  = ReadParams.ulReadAddress;
			WriteParams.usWriteData  = usReadData;
			WriteParams.usWriteData &= (~( 0x1 << ( 15 - ( f_ulToneEventNumber % 16 ))));

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		else /* if ( f_fDisableAll == TRUE ) */
		{
			/* Clear all events in the channel main memory.*/
			SmearParams.ulWriteLength = 4;
			SmearParams.usWriteData = 0x0000;
			SmearParams.ulWriteAddress = ulToneEventBaseAddress;
			mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseToneDetectionEvent

Description:    Clear the entry made for this tone in the channel tone 
				enable array. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulChannelIndex		Index of the channel within the API's channel list.
f_ulToneEventNumber		Tone event number to be deactivated.
f_ulExtToneChanIndex	Index of the extended tone detection channel.
f_fDisableAll			Release all activated tones.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseToneDetectionEvent
UINT32 Oct6100ApiReleaseToneDetectionEvent (
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex,
				IN		BOOL									f_fDisableAll )
{
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32						ulToneEntry;
	UINT32						ulResult;
	UINT32						ulToneEventNumber;
	BOOL						fSSTone;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Update the channel entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_ulChannelIndex );

	/* Check if must release all tone events. */
	if ( f_fDisableAll == FALSE )
	{
		/* Set the corresponding bit in the channel array. */
		ulToneEntry = pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ];

		/* Modify the entry. */
		ulToneEntry &= (~( 0x1 << ( 31 - ( f_ulToneEventNumber % 32 ))));

		/* Copy back the new value. */
		pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ] = ulToneEntry;
	}
	else /* if ( f_fDisableAll == TRUE ) */
	{
		/* Clear all events. */
		Oct6100UserMemSet( pEchoChanEntry->aulToneConf, 0x00, sizeof( pEchoChanEntry->aulToneConf ) );
	}

	/* Configure also the extended channel if necessary. */
	if ( f_ulExtToneChanIndex != cOCT6100_INVALID_INDEX )
	{
		/* Update the channel entry. */
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_ulExtToneChanIndex );

		/* Check if must release all tone events. */
		if ( f_fDisableAll == FALSE )
		{
			/* Set the corresponding bit in the channel array. */
			ulToneEntry = pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ];

			/* Modify the entry. */
			ulToneEntry &= (~( 0x1 << ( 31 - ( f_ulToneEventNumber % 32 ))));

			/* Copy back the new value. */
			pEchoChanEntry->aulToneConf[ f_ulToneEventNumber / 32 ] = ulToneEntry;
		}
		else /* if ( f_fDisableAll == TRUE ) */
		{
			/* Clear all events. */
			Oct6100UserMemSet( pEchoChanEntry->aulToneConf, 0x00, sizeof( pEchoChanEntry->aulToneConf ) );
		}
	}
	
	/* Re-enable the SS7 tones */
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
													f_ulChannelIndex, 
													ulToneEventNumber,

													cOCT6100_INVALID_INDEX );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiIsSSTone

Description:    Check if specified tone number is a special signaling
				system tone.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_ulToneEventNumber			Tone event number to be checked against.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiIsSSTone
UINT32 Oct6100ApiIsSSTone(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulToneEventNumber,
				OUT		PBOOL									f_fSSTone )
{
	*f_fSSTone = FALSE;

	switch( f_ulToneEventNumber )
	{
	case cOCT6100_TONE_SIN_SYSTEM7_2000				:
	case cOCT6100_TONE_SIN_SYSTEM7_1780				:	
	case cOCT6100_TONE_ROUT_G168_2100GB_ON			:
	case cOCT6100_TONE_ROUT_G168_2100GB_WSPR		:	
	case cOCT6100_TONE_ROUT_G168_1100GB_ON			:	
	case cOCT6100_TONE_ROUT_G168_2100GB_ON_WIDE_A	:	
	case cOCT6100_TONE_ROUT_G168_2100GB_ON_WIDE_B	:
	case cOCT6100_TONE_ROUT_G168_2100GB_WSPR_WIDE	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON			:	
	case cOCT6100_TONE_SOUT_G168_2100GB_WSPR		:	
	case cOCT6100_TONE_SOUT_G168_1100GB_ON			:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON_WIDE_A	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON_WIDE_B	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_WSPR_WIDE	:	
	case cOCT6100_TONE_SIN_SYSTEM5_2400				:
	case cOCT6100_TONE_SIN_SYSTEM5_2600				:
	case cOCT6100_TONE_SIN_SYSTEM5_2400_2600		:	
		*f_fSSTone = TRUE;
		break;
	default:
		break;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiIsSSTone

Description:    Check if specified tone number is a 2100 special signaling
				system tone.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_ulToneEventNumber			Tone event number to be checked against.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiIs2100Tone
UINT32 Oct6100ApiIs2100Tone(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulToneEventNumber,
				OUT		PBOOL									f_fIs2100Tone )
{
	*f_fIs2100Tone = FALSE;

	switch( f_ulToneEventNumber )
	{
	case cOCT6100_TONE_ROUT_G168_2100GB_ON			:
	case cOCT6100_TONE_ROUT_G168_2100GB_WSPR		:	
	case cOCT6100_TONE_ROUT_G168_2100GB_ON_WIDE_A	:	
	case cOCT6100_TONE_ROUT_G168_2100GB_ON_WIDE_B	:
	case cOCT6100_TONE_ROUT_G168_2100GB_WSPR_WIDE	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON			:	
	case cOCT6100_TONE_SOUT_G168_2100GB_WSPR		:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON_WIDE_A	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_ON_WIDE_B	:	
	case cOCT6100_TONE_SOUT_G168_2100GB_WSPR_WIDE	:	
		*f_fIs2100Tone = TRUE;
		break;
	default:
		break;
	}

	return cOCT6100_ERR_OK;
}
#endif
