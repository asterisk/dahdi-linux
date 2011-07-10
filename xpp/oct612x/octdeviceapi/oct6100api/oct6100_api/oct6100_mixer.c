/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_mixer.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions used to manage the allocation of mixer
	blocks in memories.

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

$Octasic_Revision: 42 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "apilib/octapi_llman.h"

#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"
#include "oct6100api/oct6100_channel_inst.h"
#include "oct6100api/oct6100_mixer_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_mixer_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_mixer_priv.h"

/****************************  PUBLIC FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100MixerCopyEventCreate

Description:    This function creates a mixer copy event used to copy 
				information from one channel port to another channel port.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pCopyEventCreate		Pointer to a mixer copy event structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100MixerCopyEventCreateDef
UINT32 Oct6100MixerCopyEventCreateDef(
				tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate )
{
	f_pCopyEventCreate->pulCopyEventHndl = NULL;

	f_pCopyEventCreate->ulSourceChanHndl = cOCT6100_INVALID_HANDLE;
	f_pCopyEventCreate->ulSourcePort	 = cOCT6100_INVALID_PORT;

	f_pCopyEventCreate->ulDestinationChanHndl = cOCT6100_INVALID_HANDLE;
	f_pCopyEventCreate->ulDestinationPort	  = cOCT6100_INVALID_PORT;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100MixerCopyEventCreate
UINT32 Oct6100MixerCopyEventCreate(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate )
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
		ulFncRes = Oct6100MixerCopyEventCreateSer( f_pApiInstance, f_pCopyEventCreate );
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

Function:		Oct6100MixerCopyEventDestroy

Description:    This function destroys a mixer copy event used to copy 
				information from one channel port to another.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pCopyEventDestroy		Pointer to a destroy copy event structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100MixerCopyEventDestroyDef
UINT32 Oct6100MixerCopyEventDestroyDef(
				tPOCT6100_COPY_EVENT_DESTROY			f_pCopyEventDestroy )
{
	f_pCopyEventDestroy->ulCopyEventHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100MixerCopyEventDestroy
UINT32 Oct6100MixerCopyEventDestroy(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_COPY_EVENT_DESTROY		f_pCopyEventDestroy )
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
		ulFncRes = Oct6100MixerCopyEventDestroySer( f_pApiInstance, f_pCopyEventDestroy );
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

Function:		Oct6100ApiGetMixerSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of mixer events.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pOpenChip				User chip configuration.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetMixerSwSizes
UINT32 Oct6100ApiGetMixerSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Calculate the API memory required for the resource entry lists. */
	f_pInstSizes->ulMixerEventList = cOCT6100_MAX_MIXER_EVENTS * sizeof( tOCT6100_API_MIXER_EVENT );

	/* Calculate memory needed for mixers entry allocation. */
	ulResult = OctapiLlmAllocGetSize( cOCT6100_MAX_MIXER_EVENTS, &f_pInstSizes->ulMixerEventAlloc );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_1D;

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulMixerEventList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulMixerEventAlloc, ulTempVar )


	f_pInstSizes->ulCopyEventList = cOCT6100_MAX_MIXER_EVENTS * sizeof( tOCT6100_API_COPY_EVENT );

	ulResult = OctapiLlmAllocGetSize( cOCT6100_MAX_MIXER_EVENTS, &f_pInstSizes->ulCopyEventAlloc );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_1D;

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulCopyEventList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulCopyEventAlloc, ulTempVar )


	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMixerSwInit

Description:    Initializes all elements of the instance structure associated
				to the mixer events.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This mixer is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMixerSwInit
UINT32 Oct6100ApiMixerSwInit(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_MIXER_EVENT		pMixerEventList;
	PVOID	pMixerEventAlloc;
	PVOID	pCopyEventAlloc;
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/*===================================================================*/
	/* Initialize the mixer event list. */
	mOCT6100_GET_MIXER_EVENT_LIST_PNT( pSharedInfo, pMixerEventList );

	/* Initialize the mixer event allocation software to "all free". */
	Oct6100UserMemSet( pMixerEventList, 0x00, cOCT6100_MAX_MIXER_EVENTS * sizeof( tOCT6100_API_MIXER_EVENT ));

	mOCT6100_GET_MIXER_EVENT_ALLOC_PNT( pSharedInfo, pMixerEventAlloc )
	
	ulResult = OctapiLlmAllocInit( &pMixerEventAlloc, cOCT6100_MAX_MIXER_EVENTS );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_1F;

	/* Now reserve the first entry as the first node. */
	ulResult = OctapiLlmAllocAlloc( pMixerEventAlloc, &ulTempVar );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_20;
	}

	/* Check that we obtain the first event. */
	if ( ulTempVar != 0 )
		return cOCT6100_ERR_FATAL_21;

	/* Now reserve the tail entry. */
	ulResult = OctapiLlmAllocAlloc( pMixerEventAlloc, &ulTempVar );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_AA;
	}
	/* Check that we obtain the first event. */
	if ( ulTempVar != 1 )
		return cOCT6100_ERR_FATAL_AB;

	/* Program the head node. */
	pMixerEventList[ cOCT6100_MIXER_HEAD_NODE ].fReserved = TRUE;
	pMixerEventList[ cOCT6100_MIXER_HEAD_NODE ].usNextEventPtr = cOCT6100_MIXER_TAIL_NODE;
	pMixerEventList[ cOCT6100_MIXER_HEAD_NODE ].usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

	/* Program the tail node. */
	pMixerEventList[ cOCT6100_MIXER_TAIL_NODE ].fReserved = TRUE;
	pMixerEventList[ cOCT6100_MIXER_TAIL_NODE ].usNextEventPtr = cOCT6100_INVALID_INDEX;
	pMixerEventList[ cOCT6100_MIXER_TAIL_NODE ].usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

	/* Now reserve the entry used for channel recording if the feature is enabled. */
	if ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
	{
		UINT32 ulAllocIndex;

		/* Reserve an entry to copy the desire SOUT signal to the SIN signal of the recording channel. */
		ulResult = OctapiLlmAllocAlloc( pMixerEventAlloc, &ulAllocIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			return cOCT6100_ERR_FATAL_90;
		}

		pSharedInfo->MixerInfo.usRecordCopyEventIndex = (UINT16)( ulAllocIndex & 0xFFFF );

		/* Reserve an entry to copy the saved SIN signal of the debugged channel into it's original location. */
		ulResult = OctapiLlmAllocAlloc( pMixerEventAlloc, &ulAllocIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			return cOCT6100_ERR_FATAL_90;
		}

		pSharedInfo->MixerInfo.usRecordSinEventIndex = (UINT16)( ulAllocIndex & 0xFFFF );
	
		/* Configure the SIN event. */
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordSinEventIndex ].fReserved = TRUE;
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordSinEventIndex ].usNextEventPtr = cOCT6100_MIXER_TAIL_NODE;
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordSinEventIndex ].usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		/* Configure the SOUT copy event. */
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordCopyEventIndex ].fReserved = TRUE;
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordCopyEventIndex ].usNextEventPtr = pSharedInfo->MixerInfo.usRecordSinEventIndex;
		pMixerEventList[ pSharedInfo->MixerInfo.usRecordCopyEventIndex ].usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
		
		/* Program the head node. */
		pMixerEventList[ cOCT6100_MIXER_HEAD_NODE ].usNextEventPtr = pSharedInfo->MixerInfo.usRecordCopyEventIndex;
	}

	/* Initialize the copy event list. */
	mOCT6100_GET_COPY_EVENT_ALLOC_PNT( pSharedInfo, pCopyEventAlloc )
	
	ulResult = OctapiLlmAllocInit( &pCopyEventAlloc, cOCT6100_MAX_MIXER_EVENTS );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_B4;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMixerEventAdd

Description:    This function adds a mixer event event to the list of events 
				based on the event type passed to the function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep 
							the present state of the chip and all its resources.
f_usEventIndex				Index of the event within the API's mixer event list.
f_usEventType				Type of mixer event.
f_usDestinationChanIndex	Index of the destination channel within the API's 
							channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMixerEventAdd
UINT32	Oct6100ApiMixerEventAdd( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex,
				IN		UINT16							f_usEventType,
				IN		UINT16							f_usDestinationChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_MIXER_EVENT		pCurrentEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEventEntry;
	tPOCT6100_API_CHANNEL			pDestinationEntry;
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32	ulResult;
	UINT16	usTempEventIndex;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get a pointer to the event entry. */
	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pCurrentEventEntry, f_usEventIndex );

	/* Get a pointer to the destination channel entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pDestinationEntry, f_usDestinationChanIndex );

	/* Now proceed according to the event type. */
	switch ( f_usEventType )
	{
	case cOCT6100_EVENT_TYPE_SOUT_COPY:

		/* Now insert the Sin copy event */
		if ( pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
		{
			/* The only node in the list before the point where the node needs to */
			/* be inserted is the head node. */
			usTempEventIndex = cOCT6100_MIXER_HEAD_NODE;

			/* This node will be the first one in the Sout copy section. */
			pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr = f_usEventIndex;
			pSharedInfo->MixerInfo.usLastSoutCopyEventPtr  = f_usEventIndex;
		}
		else /* pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr != cOCT6100_INVALID_INDEX */
		{
			usTempEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
			pSharedInfo->MixerInfo.usLastSoutCopyEventPtr  = f_usEventIndex;
		}

		break;

	case cOCT6100_EVENT_TYPE_SIN_COPY:

		/* Now insert the Sin copy event. */
		if ( pSharedInfo->MixerInfo.usFirstSinCopyEventPtr == cOCT6100_INVALID_INDEX )
		{
			/* This is the first Sin copy event. We must find the event that comes before */
			/* the event we want to add. First let's check for a bridge event. */
			if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == cOCT6100_INVALID_INDEX )
			{
				/* No event in the bridge section, now let's check in the Sout copy section. */
				if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
				{
					/* The only node in the list then is the head node. */
					usTempEventIndex = cOCT6100_MIXER_HEAD_NODE;
				}
				else
				{
					usTempEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
				}
			}
			else
			{
				usTempEventIndex = pSharedInfo->MixerInfo.usLastBridgeEventPtr;
			}

			/* This node will be the first one in the Sin copy section. */
			pSharedInfo->MixerInfo.usFirstSinCopyEventPtr = f_usEventIndex;
			pSharedInfo->MixerInfo.usLastSinCopyEventPtr  = f_usEventIndex;
		}
		else /* pSharedInfo->MixerInfo.usFirstSinCopyEventPtr != cOCT6100_INVALID_INDEX */
		{
			usTempEventIndex = pSharedInfo->MixerInfo.usLastSinCopyEventPtr;
			pSharedInfo->MixerInfo.usLastSinCopyEventPtr = f_usEventIndex;
		}

		break;

	default:
		return cOCT6100_ERR_FATAL_AF;

	}

	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, usTempEventIndex );

	/*=======================================================================*/
	/* Program the Copy event. */

	/* Set the Copy event first. */
	pCurrentEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_COPY;
	pCurrentEventEntry->usNextEventPtr = pTempEventEntry->usNextEventPtr;

	WriteParams.ulWriteAddress  = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.ulWriteAddress += 4;
	WriteParams.usWriteData		= pCurrentEventEntry->usNextEventPtr;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/

	/*=======================================================================*/
	/* Modify the previous node. */

	/* Set the last Sub-store entry. */
	pTempEventEntry->usNextEventPtr = f_usEventIndex;

	WriteParams.ulWriteAddress  = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usTempEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.ulWriteAddress += 4;
	WriteParams.usWriteData		= f_usEventIndex;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/	

	/* Save the destination channel index, needed when removing the event from the mixer. */
	pCurrentEventEntry->usDestinationChanIndex = f_usDestinationChanIndex;

	/* Mark the entry as reserved. */
	pCurrentEventEntry->fReserved = TRUE;

	/* Increment the event count on that particular destination channel */
	pDestinationEntry->usMixerEventCnt++;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMixerEventRemove

Description:    This function removes a mixer event event from the list of events based
				on the event type passed to the function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usEventIndex			Index of event within the API's mixer event list.
f_usEventType			Type of mixer event.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMixerEventRemove
UINT32 Oct6100ApiMixerEventRemove( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex,
				IN		UINT16							f_usEventType )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_MIXER_EVENT		pCurrentEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEventEntry;
	tPOCT6100_API_CHANNEL			pDestinationEntry;
	tOCT6100_WRITE_BURST_PARAMS		BurstWriteParams;
	tOCT6100_WRITE_PARAMS			WriteParams;
	BOOL	fFirstSinCopyEvent = FALSE;
	UINT32	ulResult;
	UINT16	usTempEventIndex;
	UINT32	ulLoopCount = 0;
	UINT16	ausWriteData[ 4 ] = { 0 };

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	BurstWriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstWriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	BurstWriteParams.pusWriteData = ausWriteData;
	BurstWriteParams.ulWriteLength = 4;

	/* Get a pointer to the event entry. */
	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pCurrentEventEntry, f_usEventIndex );

	/* Get the pointer to the channel entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pDestinationEntry, pCurrentEventEntry->usDestinationChanIndex );

	/* Now proceed according to the event type. */
	switch ( f_usEventType )
	{
	case cOCT6100_EVENT_TYPE_SOUT_COPY:

		if ( f_usEventIndex == pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr )
		{
			usTempEventIndex = cOCT6100_MIXER_HEAD_NODE;
		}
		else
		{
			/* Now insert the Sin copy event. */
			usTempEventIndex = pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr;
		}

		/* Find the copy entry before the entry to remove. */
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, usTempEventIndex );

		while( pTempEventEntry->usNextEventPtr != f_usEventIndex )
		{
			usTempEventIndex = pTempEventEntry->usNextEventPtr;

			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, usTempEventIndex );

			ulLoopCount++;
			if ( ulLoopCount == cOCT6100_MAX_LOOP )
				return cOCT6100_ERR_FATAL_B2;
		}

		/*=======================================================================*/
		/* Update the global mixer pointers. */
		if ( f_usEventIndex == pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr )
		{
			if ( f_usEventIndex == pSharedInfo->MixerInfo.usLastSoutCopyEventPtr )
			{
				/* This event was the only of the list.*/
				pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr = cOCT6100_INVALID_INDEX;
				pSharedInfo->MixerInfo.usLastSoutCopyEventPtr  = cOCT6100_INVALID_INDEX;
			}
			else
			{
				pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr = pCurrentEventEntry->usNextEventPtr;
			}
		}
		else if ( f_usEventIndex == pSharedInfo->MixerInfo.usLastSoutCopyEventPtr )
		{
			pSharedInfo->MixerInfo.usLastSoutCopyEventPtr = usTempEventIndex;
		}
		/*=======================================================================*/

		break;


	case cOCT6100_EVENT_TYPE_SIN_COPY:

		if ( f_usEventIndex == pSharedInfo->MixerInfo.usFirstSinCopyEventPtr )
		{
			fFirstSinCopyEvent = TRUE;

			if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr != cOCT6100_INVALID_INDEX )
			{
				usTempEventIndex = pSharedInfo->MixerInfo.usLastBridgeEventPtr;
			}
			else if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr != cOCT6100_INVALID_INDEX )
			{
				usTempEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
			}
			else
			{
				usTempEventIndex = cOCT6100_MIXER_HEAD_NODE;
			}	
		}
		else
		{
			/* Now insert the Sin copy event. */
			usTempEventIndex = pSharedInfo->MixerInfo.usFirstSinCopyEventPtr;
		}

		/* Find the copy entry before the entry to remove. */
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, usTempEventIndex );
		
		/* If we are not the first event of the Sin copy list. */
		if ( fFirstSinCopyEvent == FALSE )
		{
			while( pTempEventEntry->usNextEventPtr != f_usEventIndex )
			{
				usTempEventIndex = pTempEventEntry->usNextEventPtr;
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, usTempEventIndex );

				ulLoopCount++;
				if ( ulLoopCount == cOCT6100_MAX_LOOP )
					return cOCT6100_ERR_FATAL_B1;
			}
		}

		/*=======================================================================*/
		/* Update the global mixer pointers. */
		if ( f_usEventIndex == pSharedInfo->MixerInfo.usFirstSinCopyEventPtr )
		{
			if ( f_usEventIndex == pSharedInfo->MixerInfo.usLastSinCopyEventPtr )
			{
				/* This event was the only of the list. */
				pSharedInfo->MixerInfo.usFirstSinCopyEventPtr = cOCT6100_INVALID_INDEX;
				pSharedInfo->MixerInfo.usLastSinCopyEventPtr  = cOCT6100_INVALID_INDEX;
			}
			else
			{
				pSharedInfo->MixerInfo.usFirstSinCopyEventPtr = pCurrentEventEntry->usNextEventPtr;
			}
		}
		else if ( f_usEventIndex == pSharedInfo->MixerInfo.usLastSinCopyEventPtr )
		{
			pSharedInfo->MixerInfo.usLastSinCopyEventPtr = usTempEventIndex;
		}
		/*=======================================================================*/

		break;

	default:
		return cOCT6100_ERR_FATAL_B0;

	}

	/*=======================================================================*/
	/* Modify the previous event. */

	pTempEventEntry->usNextEventPtr = pCurrentEventEntry->usNextEventPtr;

	WriteParams.ulWriteAddress  = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usTempEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.ulWriteAddress += 4;
	WriteParams.usWriteData		= pTempEventEntry->usNextEventPtr;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Clear the current event. */

	BurstWriteParams.ulWriteAddress  = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	
	mOCT6100_DRIVER_WRITE_BURST_API( BurstWriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/


	/*=======================================================================*/
	/* Decrement the mixer event count active on that channel. */
	pDestinationEntry->usMixerEventCnt--;

	/*=======================================================================*/

	
	/*=======================================================================*/

	/* This index of this channel is not valid anymore! */
	pCurrentEventEntry->usDestinationChanIndex = cOCT6100_INVALID_INDEX;

	/* Mark this entry as free. */
	pCurrentEventEntry->fReserved = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100MixerCopyEventCreateSer

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pCopyEventCreate		Pointer to a create copy event structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100MixerCopyEventCreateSer
UINT32 Oct6100MixerCopyEventCreateSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate )
{
	UINT16	usCopyEventIndex = 0;
	UINT16	usMixerEventIndex = 0;
	UINT16	usSourceChanIndex;
	UINT16	usDestinationChanIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the copy event for errors. */
	ulResult = Oct6100ApiCheckCopyEventCreateParams(		f_pApiInstance, 
															f_pCopyEventCreate, 
															&usSourceChanIndex, 
															&usDestinationChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the copy event. */
	ulResult = Oct6100ApiReserveCopyEventCreateResources(	f_pApiInstance, 
															&usCopyEventIndex, 
															&usMixerEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write all necessary structures to activate the echo cancellation channel. */
	ulResult = Oct6100ApiWriteCopyEventCreateStructs(		f_pApiInstance, 
															f_pCopyEventCreate,
															usMixerEventIndex,
															usSourceChanIndex,
															usDestinationChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the new echo cancellation channels's entry in the ECHO channel list. */
	ulResult = Oct6100ApiUpdateCopyEventCreateEntry(		f_pApiInstance, 
															f_pCopyEventCreate,
															usCopyEventIndex,
															usMixerEventIndex,
															usSourceChanIndex,
															usDestinationChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckCopyEventCreateParams

Description:    Checks the user's parameter passed to the create 
				copy event function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pCopyEventCreate			Pointer to a create copy event structure.
f_pusSourceChanIndex		Pointer to the index of the input channel.
f_pusDestinationChanIndex	Pointer to the index of the output channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckCopyEventCreateParams
UINT32 Oct6100ApiCheckCopyEventCreateParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate, 
				OUT		PUINT16								f_pusSourceChanIndex, 
				OUT		PUINT16								f_pusDestinationChanIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tPOCT6100_API_CHANNEL	pSourceEntry;
	tPOCT6100_API_CHANNEL	pDestinationEntry;
	UINT32		ulEntryOpenCnt;

	/* Obtain shared resources pointer. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	if ( f_pCopyEventCreate->pulCopyEventHndl == NULL ) 
		return cOCT6100_ERR_MIXER_COPY_EVENT_HANDLE;

	if ( f_pCopyEventCreate->ulSourceChanHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_MIXER_SOURCE_CHAN_HANDLE;
	if ( f_pCopyEventCreate->ulDestinationChanHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_MIXER_DESTINATION_CHAN_HANDLE;

	if ( f_pCopyEventCreate->ulSourcePort != cOCT6100_CHANNEL_PORT_RIN &&
		 f_pCopyEventCreate->ulSourcePort != cOCT6100_CHANNEL_PORT_SIN )
		return cOCT6100_ERR_MIXER_SOURCE_PORT;

	if ( f_pCopyEventCreate->ulDestinationPort != cOCT6100_CHANNEL_PORT_RIN &&
		 f_pCopyEventCreate->ulDestinationPort != cOCT6100_CHANNEL_PORT_SIN )
		return cOCT6100_ERR_MIXER_DESTINATION_PORT;

	/*=======================================================================*/
	/* Verify the first channel handle. */

	if ( (f_pCopyEventCreate->ulSourceChanHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_MIXER_SOURCE_CHAN_HANDLE;

	*f_pusSourceChanIndex = (UINT16)( f_pCopyEventCreate->ulSourceChanHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusSourceChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_MIXER_SOURCE_CHAN_HANDLE;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSourceEntry, *f_pusSourceChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pCopyEventCreate->ulSourceChanHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pSourceEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pSourceEntry->byEntryOpenCnt )
		return cOCT6100_ERR_MIXER_SOURCE_CHAN_HANDLE;
	if ( pSourceEntry->CodecConfig.byDecoderPort == f_pCopyEventCreate->ulSourcePort )
		return cOCT6100_ERR_MIXER_SOURCE_ADPCM_RESOURCES_ACTIVATED;
	
	/*=======================================================================*/

	/*=======================================================================*/
	/* Verify the second channel handle. */

	if ( (f_pCopyEventCreate->ulDestinationChanHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_MIXER_DESTINATION_CHAN_HANDLE;

	*f_pusDestinationChanIndex = (UINT16)( f_pCopyEventCreate->ulDestinationChanHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusDestinationChanIndex  >= pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_MIXER_DESTINATION_CHAN_HANDLE;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pDestinationEntry, *f_pusDestinationChanIndex  )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pCopyEventCreate->ulDestinationChanHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pDestinationEntry->fReserved != TRUE )
		return cOCT6100_ERR_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pDestinationEntry->byEntryOpenCnt )
		return cOCT6100_ERR_MIXER_DESTINATION_CHAN_HANDLE;
	if ( pDestinationEntry->CodecConfig.byDecoderPort == f_pCopyEventCreate->ulDestinationPort )
		return cOCT6100_ERR_MIXER_DEST_ADPCM_RESOURCES_ACTIVATED;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveCopyEventCreateResources

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pusCopyEntryIndex		Pointer to the index of the copy entry within the API's list.
f_pusCopyEventIndex		Pointer to the index of the mixer copy event.
.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveCopyEventCreateResources
UINT32 Oct6100ApiReserveCopyEventCreateResources(	
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				OUT		PUINT16								f_pusCopyEntryIndex, 
				IN OUT	PUINT16								f_pusCopyEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulTempVar;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/*===============================================================================*/
	/* Verify and reserve the resources that might already be allocated. */

	ulResult = Oct6100ApiReserveCopyEventEntry( f_pApiInstance, 
												f_pusCopyEntryIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		/* Reserve the source copy event for the first channel. */
		ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, 
													  f_pusCopyEventIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			/* Reserve the Sin copy event for the first channel. */
			ulTempVar = Oct6100ApiReleaseCopyEventEntry ( f_pApiInstance, 
														*f_pusCopyEntryIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteCopyEventCreateStructs

Description:    Performs all the required structure writes to configure the
				new copy event

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pCopyEventCreate			Pointer to a create copy event structure.
f_usMixerEventIndex			Index of the copy event within the mixer memory.
f_usSourceChanIndex			Index of the source channel within the API's channel list.
f_usDestinationChanIndex	Index of the destination channel within the API's channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteCopyEventCreateStructs
UINT32 Oct6100ApiWriteCopyEventCreateStructs(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate, 
				IN		UINT16								f_usMixerEventIndex,
				IN		UINT16								f_usSourceChanIndex, 
				IN		UINT16								f_usDestinationChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pSourceEntry;
	tPOCT6100_API_CHANNEL			pDestinationEntry;
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32	ulResult;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	
	/*==============================================================================*/
	/* Get a pointer to the two channel entry. */
	
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pSourceEntry, f_usSourceChanIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pDestinationEntry, f_usDestinationChanIndex );

	/*==============================================================================*/
	/* Configure the TSST control memory and add the Sin copy event if necessary. */

	WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usMixerEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
	
	if ( f_pCopyEventCreate->ulSourcePort == cOCT6100_CHANNEL_PORT_RIN )
	{
		WriteParams.usWriteData |= pSourceEntry->usRinRoutTsiMemIndex;
		WriteParams.usWriteData |= pSourceEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
	}
	else /* f_pCopyEventCreate->ulSourcePort == cOCT6100_CHANNEL_PORT_SIN */
	{
		if ( pSourceEntry->usExtraSinTsiMemIndex != cOCT6100_INVALID_INDEX )
		{
			WriteParams.usWriteData |= pSourceEntry->usExtraSinTsiMemIndex;
		}
		else
		{		
			WriteParams.usWriteData |= pSourceEntry->usSinSoutTsiMemIndex;
		}

		WriteParams.usWriteData |= pSourceEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress += 2;

	if ( f_pCopyEventCreate->ulDestinationPort == cOCT6100_CHANNEL_PORT_RIN )
	{
		WriteParams.usWriteData = (UINT16)( pDestinationEntry->usRinRoutTsiMemIndex );
	}
	else /* f_pCopyEventCreate->ulDestinationPort == cOCT6100_CHANNEL_PORT_SIN */
	{
		WriteParams.usWriteData = (UINT16)( pDestinationEntry->usSinSoutTsiMemIndex );
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/

	/* Now insert the event into the event list. */
	ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
										f_usMixerEventIndex,
										cOCT6100_EVENT_TYPE_SIN_COPY,
										f_usDestinationChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Increment the copy event count on this channel. */
	pDestinationEntry->usCopyEventCnt++;

	/*==============================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateCopyEventCreateEntry

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.
f_pCopyEventCreate			Pointer to a create copy event structure.
f_usCopyEventIndex			Index of the copy event within the API's event list.
f_usMixerEventIndex			Index of the copy event within the mixer memory.
f_usSourceChanIndex			Index of the source channel within the API's channel list.
f_usDestinationChanIndex	Index of the destination channel within the API's channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateCopyEventCreateEntry
UINT32 Oct6100ApiUpdateCopyEventCreateEntry(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE			f_pCopyEventCreate, 
				IN		UINT16								f_usCopyEventIndex,
				IN		UINT16								f_usMixerEventIndex,
				IN		UINT16								f_usSourceChanIndex,
				IN		UINT16								f_usDestinationChanIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_COPY_EVENT		pCopyEventEntry;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_COPY_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, f_usCopyEventIndex );
	
	/*=======================================================================*/
	/* Copy the channel's configuration and allocated resources. */

	/* Save the channel info in the copy event. */
	pCopyEventEntry->usSourceChanIndex	= f_usSourceChanIndex;
	pCopyEventEntry->bySourcePort		= (UINT8)( f_pCopyEventCreate->ulSourcePort & 0xFF );

	pCopyEventEntry->usDestinationChanIndex = f_usDestinationChanIndex;
	pCopyEventEntry->byDestinationPort		= (UINT8)( f_pCopyEventCreate->ulDestinationPort & 0xFF );

	pCopyEventEntry->usMixerEventIndex		= f_usMixerEventIndex;

	/*=======================================================================*/
	
	/* Form handle returned to user. */
	*f_pCopyEventCreate->pulCopyEventHndl = cOCT6100_HNDL_TAG_COPY_EVENT | (pCopyEventEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usCopyEventIndex;

	/* Finally, mark the event as used. */
	pCopyEventEntry->fReserved = TRUE;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100MixerCopyEventDestroySer

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pCopyEventDestroy		Pointer to a destroy copy event structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100MixerCopyEventDestroySer
UINT32 Oct6100MixerCopyEventDestroySer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_COPY_EVENT_DESTROY		f_pCopyEventDestroy )
{
	UINT16	usCopyEventIndex;
	UINT16	usMixerEventIndex;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertCopyEventDestroyParams(	f_pApiInstance, 
														f_pCopyEventDestroy, 
														&usCopyEventIndex,
														&usMixerEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiInvalidateCopyEventStructs(	f_pApiInstance, 
														usCopyEventIndex,
														usMixerEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the echo cancellation channel. */
	ulResult = Oct6100ApiReleaseCopyEventResources(		f_pApiInstance, 
														usCopyEventIndex,
														usMixerEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Invalidate the handle. */
	f_pCopyEventDestroy->ulCopyEventHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertCopyEventDestroyParams

Description:    

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pCopyEventDestroy			Pointer to a destroy copy event structure.
f_pusCopyEventIndex			Pointer to the index of the copy event in the API.
f_pusMixerEventIndex		Pointer to the index of the copy event in the mixer memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertCopyEventDestroyParams
UINT32 Oct6100ApiAssertCopyEventDestroyParams( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_COPY_EVENT_DESTROY		f_pCopyEventDestroy,
				IN OUT	PUINT16								f_pusCopyEventIndex,
				IN OUT	PUINT16								f_pusMixerEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_COPY_EVENT		pCopyEventEntry;
	UINT32							ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pCopyEventDestroy->ulCopyEventHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_COPY_EVENT )
		return cOCT6100_ERR_MIXER_COPY_EVENT_HANDLE;

	*f_pusCopyEventIndex = (UINT16)( f_pCopyEventDestroy->ulCopyEventHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusCopyEventIndex  >= cOCT6100_MAX_MIXER_EVENTS )
		return cOCT6100_ERR_MIXER_COPY_EVENT_HANDLE;

	/*=======================================================================*/

	mOCT6100_GET_COPY_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, *f_pusCopyEventIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = ( f_pCopyEventDestroy->ulCopyEventHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pCopyEventEntry->fReserved != TRUE )
		return cOCT6100_ERR_MIXER_EVENT_NOT_OPEN;
	if ( ulEntryOpenCnt != pCopyEventEntry->byEntryOpenCnt )
		return cOCT6100_ERR_MIXER_COPY_EVENT_HANDLE;

	/*=======================================================================*/
	
	/* Return the index of the associated event. */
	*f_pusMixerEventIndex = pCopyEventEntry->usMixerEventIndex;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateCopyEventStructs

Description:	Destroy the link between the two channels.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_usCopyEventIndex			Index of the copy event in the API.
f_usMixerEventIndex			Index of the copy event in the mixer memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateCopyEventStructs
UINT32 Oct6100ApiInvalidateCopyEventStructs( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usCopyEventIndex,
				IN		UINT16								f_usMixerEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;
	UINT32	ulResult;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*=======================================================================*/
	/* Clear the Copy event. */
	WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usMixerEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
	WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*=======================================================================*/

	/* Remove the event from the list. */
	ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
											f_usMixerEventIndex,
											cOCT6100_EVENT_TYPE_SIN_COPY );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseCopyEventResources

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
						
f_usCopyEventIndex		Index of the copy event in the API.
f_usMixerEventIndex		Index of the copy event in the mixer memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseCopyEventResources
UINT32 Oct6100ApiReleaseCopyEventResources( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usCopyEventIndex,
				IN		UINT16								f_usMixerEventIndex )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pDestinationEntry;
	tPOCT6100_API_COPY_EVENT		pCopyEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEventEntry;
	UINT32	ulResult;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_COPY_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, f_usCopyEventIndex );

	ulResult = Oct6100ApiReleaseCopyEventEntry( f_pApiInstance, f_usCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_AC;

	/* Relese the SIN copy event. */
	ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usMixerEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	return cOCT6100_ERR_FATAL_B3;

	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEventEntry, f_usMixerEventIndex );
	
	/* Invalidate the entry. */
	pTempEventEntry->fReserved		= FALSE;
	pTempEventEntry->usEventType	= cOCT6100_INVALID_INDEX;
	pTempEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pDestinationEntry, pCopyEventEntry->usDestinationChanIndex );

	/* Decrement the copy event count on this channel. */
	pDestinationEntry->usCopyEventCnt--;

	/*=======================================================================*/

	/* Mark the event entry as unused. */
	pCopyEventEntry->fReserved = FALSE;
	pCopyEventEntry->byEntryOpenCnt++;

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveMixerEventEntry

Description:    Reserves a free entry in the mixer event list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pusEventIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveMixerEventEntry
UINT32 Oct6100ApiReserveMixerEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusEventIndex )
{
	PVOID	pMixerEventAlloc;
	UINT32	ulResult;
	UINT32	ulEventIndex;

	mOCT6100_GET_MIXER_EVENT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pMixerEventAlloc )

	ulResult = OctapiLlmAllocAlloc( pMixerEventAlloc, &ulEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_MIXER_ALL_MIXER_EVENT_ENTRY_OPENED;
		else
			return cOCT6100_ERR_FATAL_2B;
	}

	*f_pusEventIndex = (UINT16)( ulEventIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseMixerEventEntry

Description:    Release an entry from the mixer event list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_usEventIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseMixerEventEntry
UINT32 Oct6100ApiReleaseMixerEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex )
{
	PVOID	pMixerEventAlloc;
	UINT32	ulResult;

	mOCT6100_GET_MIXER_EVENT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pMixerEventAlloc )

	ulResult = OctapiLlmAllocDealloc( pMixerEventAlloc, f_usEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_2C;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetFreeMixerEventCnt

Description:    Retrieve the number of events left in the list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pulFreeEventCnt		How many events left.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetFreeMixerEventCnt
UINT32 Oct6100ApiGetFreeMixerEventCnt(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT32							f_pulFreeEventCnt )
{
	PVOID	pMixerEventAlloc;
	UINT32	ulResult;
	UINT32	ulAllocatedEvents;
	UINT32	ulAvailableEvents;

	mOCT6100_GET_MIXER_EVENT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pMixerEventAlloc )

	ulResult = OctapiLlmAllocInfo( pMixerEventAlloc, &ulAllocatedEvents, &ulAvailableEvents );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_E8;

	/* Return number of free events. */
	*f_pulFreeEventCnt = ulAvailableEvents;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveCopyEventEntry

Description:    Reserves a free entry in the copy event list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.
					
f_pusEventIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveCopyEventEntry
UINT32 Oct6100ApiReserveCopyEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusEventIndex )
{
	PVOID	pCopyEventAlloc;
	UINT32	ulResult;
	UINT32	ulEventIndex;

	mOCT6100_GET_COPY_EVENT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pCopyEventAlloc )

	ulResult = OctapiLlmAllocAlloc( pCopyEventAlloc, &ulEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_MIXER_ALL_COPY_EVENT_ENTRY_OPENED;
		else
			return cOCT6100_ERR_FATAL_AD;
	}

	*f_pusEventIndex = (UINT16)( ulEventIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseCopyEventEntry

Description:    Release an entry from the copy event list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_usEventIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseCopyEventEntry
UINT32 Oct6100ApiReleaseCopyEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex )
{
	PVOID	pCopyEventAlloc;
	UINT32	ulResult;

	mOCT6100_GET_COPY_EVENT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pCopyEventAlloc )

	ulResult = OctapiLlmAllocDealloc( pCopyEventAlloc, f_usEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_AE;

	return cOCT6100_ERR_OK;
}
#endif
