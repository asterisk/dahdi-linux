/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_events.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to retrieve tone and playout events.

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

$Octasic_Revision: 81 $

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
#include "oct6100api/oct6100_events_inst.h"
#include "oct6100api/oct6100_tone_detection_inst.h"
#include "oct6100api/oct6100_playout_buf_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_events_pub.h"
#include "oct6100api/oct6100_tone_detection_pub.h"
#include "oct6100api/oct6100_playout_buf_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_events_priv.h"
#include "oct6100_tone_detection_priv.h"
#include "oct6100_playout_buf_priv.h"

/****************************  PUBLIC FUNCTIONS  *****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100EventGetTone

Description:    Retreives an array of tone events.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pEventGetTone			Pointer to structure used to store the Tone events.						

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100EventGetToneDef
UINT32 Oct6100EventGetToneDef(
				tPOCT6100_EVENT_GET_TONE		f_pEventGetTone )
{
	f_pEventGetTone->pToneEvent = NULL;
	f_pEventGetTone->ulMaxToneEvent = 1;
	f_pEventGetTone->ulNumValidToneEvent = cOCT6100_INVALID_VALUE;
	f_pEventGetTone->fMoreEvents = FALSE;
	f_pEventGetTone->fResetBufs = FALSE;

	return cOCT6100_ERR_OK; 
}
#endif


#if !SKIP_Oct6100EventGetTone
UINT32 Oct6100EventGetTone(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_EVENT_GET_TONE		f_pEventGetTone )
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
		ulFncRes = Oct6100EventGetToneSer( f_pApiInstance, f_pEventGetTone );
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

Function:		Oct6100BufferPlayoutGetEvent

Description:    Retrieves an array of playout stop events.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_pBufPlayoutGetEvent	Pointer to structure used to store the playout events.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutGetEventDef
UINT32 Oct6100BufferPlayoutGetEventDef(
				tPOCT6100_BUFFER_PLAYOUT_GET_EVENT	f_pBufPlayoutGetEvent )
{
	f_pBufPlayoutGetEvent->pBufferPlayoutEvent = NULL;
	f_pBufPlayoutGetEvent->ulMaxEvent = 1;
	f_pBufPlayoutGetEvent->ulNumValidEvent = cOCT6100_INVALID_VALUE;
	f_pBufPlayoutGetEvent->fMoreEvents = FALSE;
	f_pBufPlayoutGetEvent->fResetBufs = FALSE;

	return cOCT6100_ERR_OK; 
}
#endif


#if !SKIP_Oct6100BufferPlayoutGetEvent
UINT32 Oct6100BufferPlayoutGetEvent(
				tPOCT6100_INSTANCE_API					f_pApiInstance,
				tPOCT6100_BUFFER_PLAYOUT_GET_EVENT		f_pBufPlayoutGetEvent )
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
		ulFncRes = Oct6100BufferPlayoutGetEventSer( f_pApiInstance, f_pBufPlayoutGetEvent );
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

Function:		Oct6100ApiGetEventsSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of the tone events and playout events
				software buffers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetEventsSwSizes
UINT32 Oct6100ApiGetEventsSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{

	{
		UINT32	ulTempVar;
		
		/* Memory needed by soft tone event buffers. */

		/* Add 1 to the circular buffer such that all user requested events can fit in the circular queue. */
		f_pInstSizes->ulSoftToneEventsBuffer = ( f_pOpenChip->ulSoftToneEventsBufSize + 1 ) * sizeof( tOCT6100_API_TONE_EVENT );

		/* Round off the sizes of the soft buffers above. */
		mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulSoftToneEventsBuffer, ulTempVar )
	}

	{
		UINT32	ulTempVar;

		/* Memory needed by soft playout stop event buffers. */
		if ( f_pOpenChip->ulSoftBufferPlayoutEventsBufSize != cOCT6100_INVALID_VALUE )
		{
			f_pInstSizes->ulSoftBufPlayoutEventsBuffer = ( f_pOpenChip->ulSoftBufferPlayoutEventsBufSize + 1 ) * sizeof( tOCT6100_API_BUFFER_PLAYOUT_EVENT );

			/* Round off the sizes of the soft buffers above. */
			mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulSoftBufPlayoutEventsBuffer, ulTempVar )
		}
		else /* if ( f_pInstSizes->ulSoftBufferPlayoutEventsBufSize == cOCT6100_INVALID_VALUE ) */
		{
			f_pInstSizes->ulSoftBufPlayoutEventsBuffer = 0;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100EventGetToneSer

Description:    Retreives an array of tone event from the software event buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pEventGetTone			Pointer to structure which will contain the retreived
						events.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100EventGetToneSer
UINT32 Oct6100EventGetToneSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_EVENT_GET_TONE			f_pEventGetTone )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_TONE_EVENT	pSoftEvent;
	UINT32	ulSoftReadPnt;
	UINT32	ulSoftWritePnt;
	UINT32	ulSoftBufSize;
	UINT32	ulNumEventsReturned;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the parameters given by the user. */
	if ( f_pEventGetTone->fResetBufs != TRUE &&
		 f_pEventGetTone->fResetBufs != FALSE )
		return cOCT6100_ERR_EVENTS_GET_TONE_RESET_BUFS;
	
	/* Check max tones. */
	if ( f_pEventGetTone->ulMaxToneEvent > pSharedInfo->ChipConfig.ulSoftToneEventsBufSize )
		return cOCT6100_ERR_EVENTS_MAX_TONES;

	if ( f_pEventGetTone->fResetBufs == FALSE )
	{
		/* Check if the events need to be fetched from the chip buffer. */
		ulSoftReadPnt = pSharedInfo->SoftBufs.ulToneEventBufferReadPtr;
		ulSoftWritePnt = pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

		if ( ulSoftReadPnt == ulSoftWritePnt )
		{
			ulResult = Oct6100ApiTransferToneEvents( f_pApiInstance, f_pEventGetTone->fResetBufs );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* If there are no events in the soft buffer then there are none in the chip */
		/* either, so return the empty case.  Else, return the events in the buffer. */
		ulSoftReadPnt = pSharedInfo->SoftBufs.ulToneEventBufferReadPtr;
		ulSoftWritePnt = pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;
		ulSoftBufSize = pSharedInfo->SoftBufs.ulToneEventBufferSize;

		if ( ulSoftReadPnt != ulSoftWritePnt )
		{
			ulNumEventsReturned = 0;

			while( (ulSoftReadPnt != ulSoftWritePnt) && ( ulNumEventsReturned != f_pEventGetTone->ulMaxToneEvent) )
			{
				/* Get a pointer to the first event in the buffer. */
				mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
				pSoftEvent += ulSoftReadPnt;
				
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulChannelHndl = pSoftEvent->ulChannelHandle;
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulUserChanId = pSoftEvent->ulUserChanId;
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulTimestamp = pSoftEvent->ulTimestamp;
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulEventType = pSoftEvent->ulEventType;
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulToneDetected = pSoftEvent->ulToneDetected;
				f_pEventGetTone->pToneEvent[ ulNumEventsReturned ].ulExtToneDetectionPort = pSoftEvent->ulExtToneDetectionPort;

				/* Update the pointers of the soft buffer. */
				ulSoftReadPnt++;
				if ( ulSoftReadPnt == ulSoftBufSize )
					ulSoftReadPnt = 0;

				ulNumEventsReturned++;
			}

			pSharedInfo->SoftBufs.ulToneEventBufferReadPtr = ulSoftReadPnt;

			/* Detemine if there are more events pending in the soft buffer. */
			if ( ulSoftReadPnt != ulSoftWritePnt )
				f_pEventGetTone->fMoreEvents = TRUE;
			else /* ( ulSoftReadPnt == ulSoftWritePnt ) */
			{
				f_pEventGetTone->fMoreEvents = FALSE;
				
				/* Remember this state in the interrupt manager. */
				pSharedInfo->IntrptManage.fToneEventsPending = FALSE;
			}

			f_pEventGetTone->ulNumValidToneEvent = ulNumEventsReturned;
		}
		else
		{
			/* No valid tone.*/
			f_pEventGetTone->ulNumValidToneEvent = 0;
			f_pEventGetTone->fMoreEvents = FALSE;

			/* Remember this state in the interrupt manager. */
			pSharedInfo->IntrptManage.fToneEventsPending = FALSE;
			
			return cOCT6100_ERR_EVENTS_TONE_BUF_EMPTY;
		}
	}
	else /* ( f_pEventGetTone->fResetBufs == TRUE ) */
	{
		/* Empty the hardware buffer. */
		ulResult = Oct6100ApiTransferToneEvents( f_pApiInstance, f_pEventGetTone->fResetBufs );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* If the buffers are to be reset then update the pointers and full flag. */
		pSharedInfo->SoftBufs.ulToneEventBufferReadPtr = 0;
		pSharedInfo->SoftBufs.ulToneEventBufferWritePtr = 0;

		f_pEventGetTone->fMoreEvents = FALSE;
		f_pEventGetTone->ulNumValidToneEvent = 0;

		/* Remember this state in the interrupt manager. */
		pSharedInfo->IntrptManage.fToneEventsPending = FALSE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiTransferToneEvents

Description:    Transfers all tone events from the PGSP event out chip buffer 
				to the soft buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulResetBuf			Reset flag.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiTransferToneEvents
UINT32 Oct6100ApiTransferToneEvents(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulResetBuf )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_TONE_EVENT			pSoftEvent;
	tPOCT6100_API_CHANNEL			pEchoChannel;
	tOCT6100_WRITE_PARAMS				WriteParams;
	tOCT6100_READ_PARAMS				ReadParams;
	tOCT6100_READ_BURST_PARAMS			BurstParams;
	UINT32	ulChipBufFill;
	UINT32	ulChipWritePtr = 0;
	UINT32	ulChipReadPtr = 0;
	
	UINT32	usChannelIndex;
	UINT32	ulBaseTimestamp;
	UINT32	ulToneCnt;
	UINT32	ulNumWordsToRead;
	UINT32  ulEventCode;

	UINT32	ulResult;
	UINT32	i, j;
	UINT16	usReadData;
	UINT16	ausReadData[ cOCT6100_NUM_WORDS_PER_TONE_EVENT ];

	UINT32	ulExtToneDetectionPort;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* If the buffer is to be reset then clear the overflow flag. */
	if ( f_ulResetBuf == TRUE )
	{
		pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt = 0;
	}

	/* Set some parameters of read struct. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	/* Get the current read pointer of the chip buffer. */
	ReadParams.ulReadAddress = cOCT6100_TONE_EVENT_READ_PTR_REG;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulChipReadPtr = usReadData;

	/* Now get the current write pointer. */
	ReadParams.ulReadAddress = cOCT6100_TONE_EVENT_WRITE_PTR_REG;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulChipWritePtr = usReadData;

	ulChipBufFill = (( ulChipWritePtr - ulChipReadPtr ) & ( cOCT6100_NUM_PGSP_EVENT_OUT - 1 ));

	/* Set some parameters of write structs. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Read in the tone event one at a time. */
	for ( i = 0; i < ulChipBufFill; i++ )
	{
		/* Skip the event processing if the buffer is to be reset. */
		if ( f_ulResetBuf == TRUE )
		{
			/* Update the control variables of the buffer. */
			ulChipReadPtr++;
			if ( cOCT6100_NUM_PGSP_EVENT_OUT == ulChipReadPtr )
				ulChipReadPtr = 0;
		}
		else
		{
			/* Read in the event only if there's enough room in the soft buffer, and */
			/* the chip buffer is NOT to be reset. */
			if ( ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr) &&
				 ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0) )
			{
				BurstParams.ulReadAddress = cOCT6100_PGSP_EVENT_OUT_BASE + ( ulChipReadPtr * cOCT6100_PGSP_TONE_EVENT_SIZE );
				BurstParams.pusReadData = ausReadData;

				ulNumWordsToRead = cOCT6100_PGSP_TONE_EVENT_SIZE / 2;
				
				while ( ulNumWordsToRead > 0 )
				{
					if ( ulNumWordsToRead > pSharedInfo->ChipConfig.usMaxRwAccesses )
					{				
						BurstParams.ulReadLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
					}
					else
					{
						BurstParams.ulReadLength = ulNumWordsToRead;
					}

					mOCT6100_DRIVER_READ_BURST_API( BurstParams, ulResult );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					BurstParams.pusReadData		+= BurstParams.ulReadLength;
					BurstParams.ulReadAddress	+= BurstParams.ulReadLength * 2;

					ulNumWordsToRead -= BurstParams.ulReadLength;
				}

				/* Verify if the event is valid. */
				if ( ( ausReadData[ 0 ] & cOCT6100_VALID_TONE_EVENT ) == 0x0 )
					return cOCT6100_ERR_FATAL_2D;

				/* First extract the channel number of the tone event. */
				usChannelIndex = ausReadData[ 1 ] & 0x3FF;

				/* Now the timestamp. */
				ulBaseTimestamp  = ausReadData[ 2 ] << 16;
				ulBaseTimestamp |= ausReadData[ 3 ];
				
				/* This timestamp is 256 in adwance, must remove 256 frames. */
				ulBaseTimestamp -= 256;

				/* Fetch the channel stucture to validate which event can be reported. */
				mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, usChannelIndex );

				if ( pEchoChannel->fReserved != TRUE )
				{
					/* Update the control variables of the buffer. */
					ulChipReadPtr++;
					if ( ulChipReadPtr == cOCT6100_NUM_PGSP_EVENT_OUT )
						ulChipReadPtr = 0;
					
					/* This channel has been closed since the generation of the event. */
					continue;
				}

				/* Extract the extended tone detection port if available. */
				if ( pEchoChannel->ulExtToneChanMode == cOCT6100_API_EXT_TONE_SIN_PORT_MODE )
				{
					ulExtToneDetectionPort = cOCT6100_CHANNEL_PORT_SIN;
				}
				else if ( pEchoChannel->ulExtToneChanMode == cOCT6100_API_EXT_TONE_RIN_PORT_MODE )
				{
					ulExtToneDetectionPort = cOCT6100_CHANNEL_PORT_RIN;

					/* Modify the channel index. */
					usChannelIndex = pEchoChannel->usExtToneChanIndex;

					/* Change the channel entry to the original one for statistical purposes. */
					mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, usChannelIndex );

				}
				else /* pEchoChannel->ulExtToneChanMode == cOCT6100_API_EXT_TONE_DISABLED */
				{
					ulExtToneDetectionPort = cOCT6100_INVALID_VALUE;
				}

				ulToneCnt = 0;
				/* Verify all the possible events that might have been detected. */
				for ( j = 4; j < cOCT6100_NUM_WORDS_PER_TONE_EVENT; j++ )
				{
					ulEventCode = ( ausReadData[ j ] >> 8 ) & 0x7;

					if ( ulEventCode != 0x0 )
					{
						/* This tone generated an event, now check if event is masked for the channel. */
						if ((( pEchoChannel->aulToneConf[ ulToneCnt / 32 ] >> ( 31 - ( ulToneCnt % 32 ))) & 0x1) == 1 )
						{
							BOOL f2100Tone;
							
							/* Check if it is a 2100 Tone STOP and if the user wants receive those events*/
							ulResult = Oct6100ApiIs2100Tone(f_pApiInstance,
															pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID,
															&f2100Tone);
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
							
							if (  (f2100Tone == FALSE) ||
								( (f2100Tone == TRUE) && (ulEventCode != 2) ) ||
								( (f2100Tone == TRUE) && pSharedInfo->ChipConfig.fEnable2100StopEvent == TRUE ) )
							{
						
								/* If enough space. */
								if ( ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr) &&
									((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0) )
								{
									/* The tone event is not masked, The API can create a soft tone event. */
									mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
									pSoftEvent += pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

									/* Decode the event type. */
									switch( ulEventCode ) 
									{
									case 1:
										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										break;
									case 2:
										pSoftEvent->ulEventType = cOCT6100_TONE_STOP;
										break;
									case 3:
										/* This one is a little tricky.  We first */
										/* generate the "PRESENT" event and then generate the "STOP" event. */

										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | usChannelIndex; 
										pSoftEvent->ulUserChanId = pEchoChannel->ulUserChanId;
										pSoftEvent->ulToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
										/* We want the timestamp not to be equal to the "STOP" event, so we subtract one to the detector's value. */
										pSoftEvent->ulTimestamp = ( ulBaseTimestamp + ((( ausReadData[ j ] >> 13 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT ) ) - 1;
										pSoftEvent->ulExtToneDetectionPort = ulExtToneDetectionPort;

										/* Update the control variables of the buffer. */
										pSharedInfo->SoftBufs.ulToneEventBufferWritePtr++;
										if ( pSharedInfo->SoftBufs.ulToneEventBufferWritePtr == pSharedInfo->SoftBufs.ulToneEventBufferSize )
											pSharedInfo->SoftBufs.ulToneEventBufferWritePtr = 0;

										/* If enough space for the "STOP" event. */
										if ( ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr) &&
											((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0) )
										{
											mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
											pSoftEvent += pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

											pSoftEvent->ulEventType = cOCT6100_TONE_STOP;
										}
										else
										{
											/* Set the overflow flag of the buffer. */
											pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt++;

											/* We continue in the loop in order to empty the hardware buffer. */
											continue;
										}
										
										break;
									case 4:
										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										break;
									default:
										pSharedInfo->ErrorStats.ulToneDetectorErrorCnt++;
										/* do not process this packet*/
										continue;
									}

									pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | usChannelIndex; 
									pSoftEvent->ulUserChanId = pEchoChannel->ulUserChanId;
									pSoftEvent->ulToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
									pSoftEvent->ulTimestamp = ulBaseTimestamp + ((( ausReadData[ j ] >> 13 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT );
									pSoftEvent->ulExtToneDetectionPort = ulExtToneDetectionPort;

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

									/* We continue in the loop in order to empty the hardware buffer. */
								}
							}
						}
						else
						{
							BOOL fSSTone;

							ulResult = Oct6100ApiIsSSTone( 
														f_pApiInstance, 
														pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID, 
														&fSSTone );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							if ( fSSTone == TRUE )
							{
								/* Check if this is a "PRESENT" or "STOP" event */
								switch( ulEventCode )
								{
								case 1:
									/* This is a signaling system present event.  Keep this in the instance memory. */
									pEchoChannel->ulLastSSToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
									pEchoChannel->ulLastSSToneTimestamp = ulBaseTimestamp + ((( ausReadData[ j ] >> 13 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT );
									break;
								case 2:
									/* This is the "STOP" event, invalidate the last value.  The user does not want to know about this. */
									pEchoChannel->ulLastSSToneDetected = (PTR_TYPE)cOCT6100_INVALID_VALUE;
									pEchoChannel->ulLastSSToneTimestamp = (PTR_TYPE)cOCT6100_INVALID_VALUE;
									break;
								default:
									break;
								}
							}
						}
					}
					ulToneCnt++;
		
					/* Check the other tone of this word. */
					ulEventCode = ausReadData[ j ] & 0x7;
					
					if ( ulEventCode != 0x0 )
					{
						if ((( pEchoChannel->aulToneConf[ ulToneCnt / 32 ] >> ( 31 - ( ulToneCnt % 32 ))) & 0x1) == 1 )
						{
							BOOL f2100Tone;
							
							/* Check if it is a 2100 Tone STOP and if the user wants receive those events*/
							ulResult = Oct6100ApiIs2100Tone(f_pApiInstance,
															pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID,
															&f2100Tone);
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
							
							if (  (f2100Tone == FALSE) ||
								( (f2100Tone == TRUE) && (ulEventCode != 2) ) ||
								( (f2100Tone == TRUE) && pSharedInfo->ChipConfig.fEnable2100StopEvent == TRUE ) )
							{
							
								/* If enough space. */
								if ( ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr) &&
									((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0) )
								{
									/* The tone event is not masked, The API can create a soft tone event. */
									mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
									pSoftEvent += pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

									/* Decode the event type. */
									switch( ulEventCode ) 
									{
									case 1:
										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										break;
									case 2:
										pSoftEvent->ulEventType = cOCT6100_TONE_STOP;
										break;
									case 3:
										/* This one is a little tricky.  We first */
										/* generate the "PRESENT" event and then generate the "STOP" event. */

										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | usChannelIndex; 
										pSoftEvent->ulUserChanId = pEchoChannel->ulUserChanId;
										pSoftEvent->ulToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
										/* We want the timestamp not to be equal to the "STOP" event, so we subtract one to the detector's value. */
										pSoftEvent->ulTimestamp = ( ulBaseTimestamp + ((( ausReadData[ j ] >> 5 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT ) ) - 1;
										pSoftEvent->ulExtToneDetectionPort = ulExtToneDetectionPort;

										/* Update the control variables of the buffer. */
										pSharedInfo->SoftBufs.ulToneEventBufferWritePtr++;
										if ( pSharedInfo->SoftBufs.ulToneEventBufferWritePtr == pSharedInfo->SoftBufs.ulToneEventBufferSize )
											pSharedInfo->SoftBufs.ulToneEventBufferWritePtr = 0;

										/* If enough space for the "STOP" event. */
										if ( ((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferReadPtr) &&
											((pSharedInfo->SoftBufs.ulToneEventBufferWritePtr + 1) != pSharedInfo->SoftBufs.ulToneEventBufferSize || pSharedInfo->SoftBufs.ulToneEventBufferReadPtr != 0) )
										{
											mOCT6100_GET_TONE_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
											pSoftEvent += pSharedInfo->SoftBufs.ulToneEventBufferWritePtr;

											pSoftEvent->ulEventType = cOCT6100_TONE_STOP;
										}
										else
										{
											/* Set the overflow flag of the buffer. */
											pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt++;

											/* We continue in the loop in order to empty the hardware buffer. */
											continue;
										}
										
										break;
									case 4:
										pSoftEvent->ulEventType = cOCT6100_TONE_PRESENT;
										break;
									default:
										pSharedInfo->ErrorStats.ulToneDetectorErrorCnt++;
										/* Do not process this packet. */
										continue;
									}

									pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | usChannelIndex; 
									pSoftEvent->ulUserChanId = pEchoChannel->ulUserChanId;
									pSoftEvent->ulToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
									pSoftEvent->ulTimestamp = ulBaseTimestamp + ((( ausReadData[ j ] >> 5 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT );
									pSoftEvent->ulExtToneDetectionPort = ulExtToneDetectionPort;

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

									/* We continue in the loop in order to empty the hardware buffer. */
								}
							}
						}
						else
						{
							BOOL fSSTone;

							ulResult = Oct6100ApiIsSSTone( 
														f_pApiInstance, 
														pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID, 
														&fSSTone );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							if ( fSSTone == TRUE )
							{
								/* Check if this is a "PRESENT" event. */
								switch ( ulEventCode ) 
								{
								case 1:
									/* This is a signaling system present event.  Keep this in the instance memory. */
									pEchoChannel->ulLastSSToneDetected = pSharedInfo->ImageInfo.aToneInfo[ ulToneCnt ].ulToneID;
									pEchoChannel->ulLastSSToneTimestamp = ulBaseTimestamp + ((( ausReadData[ j ] >> 5 ) & 0x7) * cOCT6100_LOCAL_TIMESTAMP_INCREMENT );
									break;
								case 2:
									/* This is the "STOP" event, invalidate the last value.  The user does not want to know about this. */
									pEchoChannel->ulLastSSToneDetected = (PTR_TYPE)cOCT6100_INVALID_VALUE;
									pEchoChannel->ulLastSSToneTimestamp = (PTR_TYPE)cOCT6100_INVALID_VALUE;
									break;
								default:
									break;
								}
							}
						}
					}
					ulToneCnt++;
				}
			}
			else
			{
				/* Set the overflow flag of the buffer. */
				pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt++;

				/* We continue in the loop in order to empty the hardware buffer. */
			}

			/* Update the control variables of the buffer. */
			ulChipReadPtr++;
			if ( ulChipReadPtr == cOCT6100_NUM_PGSP_EVENT_OUT )
				ulChipReadPtr = 0;
		}
	}

	/* Write the value of the new Read pointer.*/
	WriteParams.ulWriteAddress = cOCT6100_TONE_EVENT_READ_PTR_REG;
	WriteParams.usWriteData = (UINT16)( ulChipReadPtr );
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;



	return cOCT6100_ERR_OK;
}
#endif





/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutGetEventSer

Description:    Retreives an array of buffer playout event from the software 
				event buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pEventGetPlayoutStop	Pointer to structure which will contain the retreived
						events.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutGetEventSer
UINT32 Oct6100BufferPlayoutGetEventSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_GET_EVENT	f_pBufPlayoutGetEvent )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_EVENT	pSoftEvent;
	UINT32	ulSoftReadPnt;
	UINT32	ulSoftWritePnt;
	UINT32	ulSoftBufSize;
	UINT32	ulNumEventsReturned;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the parameters past by the user. */
	if ( f_pBufPlayoutGetEvent->fResetBufs != TRUE &&
		 f_pBufPlayoutGetEvent->fResetBufs != FALSE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_EVENT_RESET;
	
	/* Check if software buffer has been allocated and thus enabled. */
	if ( pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_EVENT_DISABLED;

	/* Checking max playout events. */
	if ( f_pBufPlayoutGetEvent->ulMaxEvent > pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize )
		return cOCT6100_ERR_BUFFER_PLAYOUT_MAX_EVENT;

	if ( f_pBufPlayoutGetEvent->fResetBufs == FALSE )
	{
		/* Check if events need to be fetched from the chip. */
		ulSoftReadPnt = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr;
		ulSoftWritePnt = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr;

		if ( ulSoftReadPnt == ulSoftWritePnt )
		{
			ulResult = Oct6100BufferPlayoutTransferEvents( f_pApiInstance, f_pBufPlayoutGetEvent->fResetBufs );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* If there are no events in the soft buffer then there are none in the chip */
		/* either, so return the empty case.  Else, return the events in the buffer. */
		ulSoftReadPnt = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr;
		ulSoftWritePnt = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr;
		ulSoftBufSize = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferSize;

		if ( ulSoftReadPnt != ulSoftWritePnt )
		{
			ulNumEventsReturned = 0;

			while( (ulSoftReadPnt != ulSoftWritePnt) && ( ulNumEventsReturned != f_pBufPlayoutGetEvent->ulMaxEvent) )
			{
				/* Get a pointer to the first event in the buffer. */
				mOCT6100_GET_BUFFER_PLAYOUT_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
				pSoftEvent += ulSoftReadPnt;
				
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulChannelHndl = pSoftEvent->ulChannelHandle;
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulUserChanId = pSoftEvent->ulUserChanId;
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulChannelPort = pSoftEvent->ulChannelPort;
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulUserEventId = pSoftEvent->ulUserEventId;
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulEventType = pSoftEvent->ulEventType;
				f_pBufPlayoutGetEvent->pBufferPlayoutEvent[ ulNumEventsReturned ].ulTimestamp = pSoftEvent->ulTimestamp;
				
				/* Update the pointers of the soft buffer. */
				ulSoftReadPnt++;
				if ( ulSoftReadPnt == ulSoftBufSize )
					ulSoftReadPnt = 0;

				ulNumEventsReturned++;
			}

			pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr = ulSoftReadPnt;

			/* Detemine if there are more events pending in the soft buffer. */
			if ( ulSoftReadPnt != ulSoftWritePnt )
				f_pBufPlayoutGetEvent->fMoreEvents = TRUE;
			else /* ( ulSoftReadPnt == ulSoftWritePnt ) */
			{
				f_pBufPlayoutGetEvent->fMoreEvents = FALSE;
				
				/* Remember this state in the interrupt manager. */
				pSharedInfo->IntrptManage.fBufferPlayoutEventsPending = FALSE;
			}

			f_pBufPlayoutGetEvent->ulNumValidEvent = ulNumEventsReturned;
		}
		else /* if ( ulSoftReadPnt == ulSoftWritePnt ) */
		{
			/* No valid buffer playout events. */
			f_pBufPlayoutGetEvent->ulNumValidEvent = 0;		
			f_pBufPlayoutGetEvent->fMoreEvents = FALSE;

			/* Remember this state in the interrupt manager. */
			pSharedInfo->IntrptManage.fBufferPlayoutEventsPending = FALSE;
			
			return cOCT6100_ERR_BUFFER_PLAYOUT_EVENT_BUF_EMPTY;
		}
	}
	else /* ( f_pEventGetPlayoutStop->fResetBufs == TRUE ) */
	{
		/* Check with the hardware first. */
		ulResult = Oct6100BufferPlayoutTransferEvents( f_pApiInstance, f_pBufPlayoutGetEvent->fResetBufs );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* If the buffers are to be reset, then update the pointers and full flag. */
		pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr = 0;
		pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr = 0;

		f_pBufPlayoutGetEvent->fMoreEvents = FALSE;
		f_pBufPlayoutGetEvent->ulNumValidEvent = 0;

		/* Remember this state in the interrupt manager. */
		pSharedInfo->IntrptManage.fBufferPlayoutEventsPending = FALSE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutTransferEvents

Description:    Check all channels that are currently playing a buffer and 
				generate an event if a buffer has stopped playing.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulResetBuf			Reset flag.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutTransferEvents
UINT32 Oct6100BufferPlayoutTransferEvents(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulResetBuf )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_CHANNEL				pEchoChannel;

	UINT32	ulChannelIndex;
	UINT32	ulResult;
	UINT32	ulLastBufPlayoutEventBufferOverflowCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* If the buffer is to be reset then clear the overflow flag. */
	if ( f_ulResetBuf == TRUE )
	{
		pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt = 0;
		/* We are done for now. */
		/* No need to check for new events since the user requested to empty the soft buffer. */
		return cOCT6100_ERR_OK;
	}

	/* Check if buffer playout has been activated on some ports. */
	if ( pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts == 0 )
	{
		/* Buffer playout has not been activated on any channel, */
		/* let's not waste time here. */
		return cOCT6100_ERR_OK;
	}

	/* Save the current overflow count.  We want to know if an overflow occured to get out of the loop. */
	ulLastBufPlayoutEventBufferOverflowCnt = pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt;

	/* Search through the list of API channel entry for the ones that need playout event checking. */
	for ( ulChannelIndex = 0; ulChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; ulChannelIndex++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChannel, ulChannelIndex );
		
		/* Check if buffer playout is active on this channel, using the optimization flag. */
		/* This flag is redundant of other flags used for playout, but will make the above loop */
		/* much faster.  This is needed since this function is called very frequently on systems */
		/* which use buffer playout stop events. */
		if ( pEchoChannel->fBufPlayoutActive == TRUE )
		{
			/* Read in the event only if there's enough room in the soft buffer. */
			if ( ulLastBufPlayoutEventBufferOverflowCnt == pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt )
			{
				/* Check Rout buffer playout first. */
				if ( ( pEchoChannel->fRinBufPlayoutNotifyOnStop == TRUE )
					&& ( pEchoChannel->fRinBufPlaying == TRUE ) )
				{
					ulResult = Oct6100BufferPlayoutCheckForSpecificEvent( f_pApiInstance, ulChannelIndex, cOCT6100_CHANNEL_PORT_ROUT, TRUE, NULL );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
			else /* if ( ulLastBufPlayoutEventBufferOverflowCnt != pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt ) */
			{
				/* Get out of the loop, no more events can be inserted in the soft buffer. */
				break;
			}

			/* An overflow might have been detected in the lower level function. */
			/* Check the overflow count once again to make sure there might be room for a next event. */
			if ( ulLastBufPlayoutEventBufferOverflowCnt == pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt )
			{
				/* Check Sout buffer playout. */
				if ( ( pEchoChannel->fSoutBufPlayoutNotifyOnStop == TRUE )
					&& ( pEchoChannel->fSoutBufPlaying == TRUE ) )
				{
					ulResult = Oct6100BufferPlayoutCheckForSpecificEvent( f_pApiInstance, ulChannelIndex, cOCT6100_CHANNEL_PORT_SOUT, TRUE, NULL );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
			else /* if ( ulLastBufPlayoutEventBufferOverflowCnt != pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt ) */
			{
				/* Get out of the loop, no more events can be inserted in the soft buffer. */
				break;
			}
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutCheckForSpecificEvent

Description:    Check a specific channel/port for playout buffer events.
				If asked to, save this event to the software event buffer.
				Return a flag specifying whether the event was detected or not.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulChannelIndex		Index of the channel to be checked.
f_ulChannelPort			Port of the channel to be checked.
f_fSaveToSoftBuffer		Save event to software buffer. 
f_pfEventDetected		Whether or not an event was detected.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutCheckForSpecificEvent
UINT32 Oct6100BufferPlayoutCheckForSpecificEvent(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulChannelIndex,
				IN		UINT32							f_ulChannelPort,
				IN		BOOL							f_fSaveToSoftBuffer,
				OUT		PBOOL							f_pfEventDetected )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_EVENT	pSoftEvent;
	tPOCT6100_API_CHANNEL				pEchoChannel;
	tOCT6100_READ_PARAMS				ReadParams;
	tOCT6100_GET_TIME					GetTimeParms;

	UINT32	ulResult;
	UINT16	usReadData;
	UINT32	ulReadPtrBytesOfst;
	UINT32	ulReadPtrBitOfst;
	UINT32	ulReadPtrFieldSize;

	UINT32	ulWritePtrBytesOfst;
	UINT32	ulWritePtrBitOfst;
	UINT32	ulWritePtrFieldSize;

	UINT32	ulPlayoutBaseAddress;
	UINT32	ulTempData;
	UINT32	ulReadPtr;
	UINT32	ulMask;
	UINT32	ulWritePtr;
	UINT32	ulUserEventId;
	UINT32	ulEventType;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Compare the read and write pointers for matching.  If they matched, playout stopped. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChannel, f_ulChannelIndex );

	/* Set the playout feature base address. */
	ulPlayoutBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_ulChannelIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

	if ( f_ulChannelPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		/* Check on the Rout port. */
		ulUserEventId = pEchoChannel->ulRinUserBufPlayoutEventId;
		ulEventType = pEchoChannel->byRinPlayoutStopEventType;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byFieldSize;

		ulReadPtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byFieldSize;
	}
	else /* if ( f_ulChannelPort == cOCT6100_CHANNEL_PORT_SOUT ) */
	{
		/* Check on the Sout port. */
		ulUserEventId = pEchoChannel->ulSoutUserBufPlayoutEventId;
		ulEventType = pEchoChannel->bySoutPlayoutStopEventType;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byFieldSize;

		ulReadPtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byFieldSize;
	}

	/* Retrieve the current write pointer. */
	ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
										pEchoChannel, 
										ulPlayoutBaseAddress + ulWritePtrBytesOfst, 
										&ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );

	/* Store the write pointer.*/
	ulWritePtr = ( ulTempData & ulMask ) >> ulWritePtrBitOfst;

	/* Read the read pointer.*/
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	ReadParams.ulReadAddress = ulPlayoutBaseAddress + ulReadPtrBytesOfst;

	/* Optimize this access by only reading the word we are interested in. */
	if ( ulReadPtrBitOfst < 16 )
		ReadParams.ulReadAddress += 2;

	/* Must read in memory directly since this value is changed by hardware */
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Move data at correct position according to what was read. */
	if ( ulReadPtrBitOfst < 16 )
		ulTempData = usReadData;
	else
		ulTempData = usReadData << 16;
	
	mOCT6100_CREATE_FEATURE_MASK( ulReadPtrFieldSize, ulReadPtrBitOfst, &ulMask );
	
	/* Store the read pointer. */
	ulReadPtr = ( ulTempData & ulMask ) >> ulReadPtrBitOfst;

	/* Playout has finished when the read pointer reaches the write pointer. */
	if ( ulReadPtr != ulWritePtr )
	{
		/* Still playing -- do not generate an event. */
		if ( f_pfEventDetected != NULL )
			*f_pfEventDetected = FALSE;
	}
	else
	{
		/* Buffer stopped playing, generate an event here, if asked. */
		if ( ( f_fSaveToSoftBuffer == TRUE ) 
				&& ( ( pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr + 1 ) != pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr ) 
				&& ( ( pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr + 1 ) != pSharedInfo->SoftBufs.ulBufPlayoutEventBufferSize || pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr != 0 ) )
		{
			/* The API can create a soft buffer playout event. */
			mOCT6100_GET_BUFFER_PLAYOUT_EVENT_BUF_PNT( pSharedInfo, pSoftEvent )
			pSoftEvent += pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr;

			pSoftEvent->ulChannelHandle = cOCT6100_HNDL_TAG_CHANNEL | (pEchoChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_ulChannelIndex; 
			pSoftEvent->ulUserChanId = pEchoChannel->ulUserChanId;
			pSoftEvent->ulUserEventId = ulUserEventId;
			pSoftEvent->ulChannelPort = f_ulChannelPort;
			/* For now, only this type of event is available. */
			pSoftEvent->ulEventType = ulEventType;
			
			/* Generate millisecond timestamp. */
			GetTimeParms.pProcessContext = f_pApiInstance->pProcessContext;
			ulResult = Oct6100UserGetTime( &GetTimeParms );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			pSoftEvent->ulTimestamp = ( GetTimeParms.aulWallTimeUs[ 0 ] / 1000 );
			pSoftEvent->ulTimestamp += ( GetTimeParms.aulWallTimeUs[ 1 ] ) * ( 0xFFFFFFFF / 1000 );

			/* Update the control variables of the buffer. */
			pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr++;
			if ( pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr == pSharedInfo->SoftBufs.ulBufPlayoutEventBufferSize )
				pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr = 0;

			/* Set the interrupt manager such that the user knows that some playout events */
			/* are pending in the software Q. */
			pSharedInfo->IntrptManage.fBufferPlayoutEventsPending = TRUE;
		}
		else if ( f_fSaveToSoftBuffer == TRUE ) 
		{
			/* Set the overflow flag of the buffer. */
			pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt++;
		}

		/* Update the channel entry to set the playing flag to FALSE. */

		/* Select the port of interest. */
		if ( f_ulChannelPort == cOCT6100_CHANNEL_PORT_ROUT )
		{
			/* Decrement the number of active buffer playout ports. */
			/* No need to check anything here, it's been done in the calling function. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts--;

			pEchoChannel->fRinBufPlaying = FALSE;
			pEchoChannel->fRinBufPlayoutNotifyOnStop = FALSE;

			/* Clear optimization flag if possible. */
			if ( ( pEchoChannel->fSoutBufPlaying == FALSE )
				&& ( pEchoChannel->fSoutBufPlayoutNotifyOnStop == FALSE ) )
			{
				/* Buffer playout is no more active on this channel. */
				pEchoChannel->fBufPlayoutActive = FALSE;
			}
		}
		else /* f_ulChannelPort == cOCT6100_CHANNEL_PORT_SOUT */
		{
			/* Decrement the number of active buffer playout ports. */
			/* No need to check anything here, it's been done in the calling function. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts--;

			pEchoChannel->fSoutBufPlaying = FALSE;
			pEchoChannel->fSoutBufPlayoutNotifyOnStop = FALSE;

			/* Clear optimization flag if possible. */
			if ( ( pEchoChannel->fRinBufPlaying == FALSE )
				&& ( pEchoChannel->fRinBufPlayoutNotifyOnStop == FALSE ) )
			{
				/* Buffer playout is no more active on this channel. */
				pEchoChannel->fBufPlayoutActive = FALSE;
			}
		}

		/* Return that an event was detected. */
		if ( f_pfEventDetected != NULL )
			*f_pfEventDetected = TRUE;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


