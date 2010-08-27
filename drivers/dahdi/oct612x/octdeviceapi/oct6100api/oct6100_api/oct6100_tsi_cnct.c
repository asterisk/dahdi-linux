/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_tsi_cnct.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to open and close TSI connections

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

$Octasic_Revision: 38 $

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
#include "oct6100api/oct6100_channel_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"
#include "oct6100api/oct6100_tsi_cnct_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_tsi_cnct_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_tsi_cnct_priv.h"

/****************************  PUBLIC FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100TsiCnctOpen

Description:    This function opens a TSI connection between two TDM timeslots.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctOpen			Pointer to TSI connection open structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100TsiCnctOpenDef
UINT32 Oct6100TsiCnctOpenDef(
				tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen )
{
	f_pTsiCnctOpen->pulTsiCnctHndl = NULL;
	
	f_pTsiCnctOpen->ulInputTimeslot = cOCT6100_INVALID_TIMESLOT;
	f_pTsiCnctOpen->ulInputStream = cOCT6100_INVALID_STREAM;
	f_pTsiCnctOpen->ulOutputTimeslot = cOCT6100_INVALID_TIMESLOT;
	f_pTsiCnctOpen->ulOutputStream = cOCT6100_INVALID_STREAM;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100TsiCnctOpen
UINT32 Oct6100TsiCnctOpen(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen )
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
		ulFncRes = Oct6100TsiCnctOpenSer( f_pApiInstance, f_pTsiCnctOpen );
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

Function:		Oct6100TsiCnctClose

Description:    This function closes a TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctClose			Pointer to TSI connection close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100TsiCnctCloseDef
UINT32 Oct6100TsiCnctCloseDef(
				tPOCT6100_TSI_CNCT_CLOSE			f_pTsiCnctClose )
{
	f_pTsiCnctClose->ulTsiCnctHndl = cOCT6100_INVALID_HANDLE;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100TsiCnctClose
UINT32 Oct6100TsiCnctClose(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_TSI_CNCT_CLOSE			f_pTsiCnctClose )
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
		ulFncRes = Oct6100TsiCnctCloseSer( f_pApiInstance, f_pTsiCnctClose );
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

Function:		Oct6100ApiGetTsiCnctSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management the TSI memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetTsiCnctSwSizes
UINT32 Oct6100ApiGetTsiCnctSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;
	
	/* Determine the amount of memory required for the API TSI connection list. */
	f_pInstSizes->ulTsiCnctList = f_pOpenChip->ulMaxTsiCncts * sizeof( tOCT6100_API_TSI_CNCT );

	if ( f_pOpenChip->ulMaxTsiCncts > 0 )
	{
		/* Calculate memory needed for TSI memory allocation. */
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxTsiCncts, &f_pInstSizes->ulTsiCnctAlloc );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_48;
	}
	else
	{
		f_pInstSizes->ulTsiCnctAlloc = 0;
	}
	
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsiCnctList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsiCnctAlloc, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiTsiCnctSwInit

Description:    Initializes all elements of the instance structure associated
				to the TSI memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiTsiCnctSwInit
UINT32 Oct6100ApiTsiCnctSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_TSI_CNCT	pChannelsTsiList;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32	ulMaxTsiChannels;
	PVOID	pTsiChannelsAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize the TSI connections API list. */
	ulMaxTsiChannels = pSharedInfo->ChipConfig.usMaxTsiCncts;

	mOCT6100_GET_TSI_CNCT_LIST_PNT( pSharedInfo, pChannelsTsiList )

	/* Clear the memory. */
	Oct6100UserMemSet( pChannelsTsiList, 0x00, sizeof(tOCT6100_API_TSI_CNCT) * ulMaxTsiChannels );

	/* Set all entries in the TSI connections list to unused. */
	if ( ulMaxTsiChannels > 0 )
	{
		mOCT6100_GET_TSI_CNCT_ALLOC_PNT( pSharedInfo, pTsiChannelsAlloc )
		
		ulResult = OctapiLlmAllocInit( &pTsiChannelsAlloc, ulMaxTsiChannels );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_49;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100TsiCnctOpenSer

Description:    Opens a TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctOpen			Pointer to a tOCT6100_TSI_CNCT_OPEN structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100TsiCnctOpenSer
UINT32 Oct6100TsiCnctOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen )
{
	UINT16	usTsiChanIndex;
	UINT16	usTsiMemIndex;
	UINT16	usInputTsstIndex;
	UINT16	usOutputTsstIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the TSI connection open structure for errors. */
	ulResult = Oct6100ApiCheckTsiParams( f_pApiInstance, f_pTsiCnctOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the TSI connection. */
	ulResult = Oct6100ApiReserveTsiResources( f_pApiInstance, f_pTsiCnctOpen, &usTsiChanIndex, &usTsiMemIndex, &usInputTsstIndex, &usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write all necessary structures to activate the TSI connection. */
	ulResult = Oct6100ApiWriteTsiStructs( f_pApiInstance, f_pTsiCnctOpen, usTsiMemIndex, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the TSI connection entry in the API list. */
	ulResult = Oct6100ApiUpdateTsiEntry( f_pApiInstance, f_pTsiCnctOpen, usTsiChanIndex, usTsiMemIndex, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckTsiParams

Description:    Checks the user's TSI connection open configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctOpen			Pointer to TSI connection open configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckTsiParams
UINT32 Oct6100ApiCheckTsiParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen )
{
	UINT32	ulResult;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxTsiCncts == 0 )
		return cOCT6100_ERR_TSI_CNCT_DISABLED;

	if ( f_pTsiCnctOpen->pulTsiCnctHndl == NULL )
		return cOCT6100_ERR_TSI_CNCT_INVALID_HANDLE;

	/* Check the input TDM streams, timeslots component for errors. */
	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   cOCT6100_NUMBER_TSSTS_1,
									   f_pTsiCnctOpen->ulInputTimeslot, 
									   f_pTsiCnctOpen->ulInputStream,
									   cOCT6100_INPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_TSI_CNCT_INPUT_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_TSI_CNCT_INPUT_STREAM;
		}
		else
		{
			return ulResult;
		}
	}

	/* Check the output TDM streams, timeslots component for errors. */
	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   cOCT6100_NUMBER_TSSTS_1, 
									   f_pTsiCnctOpen->ulOutputTimeslot, 
									   f_pTsiCnctOpen->ulOutputStream,
									   cOCT6100_OUTPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_TSI_CNCT_OUTPUT_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_TSI_CNCT_OUTPUT_STREAM;
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

Function:		Oct6100ApiReserveTsiResources

Description:    Reserves all resources needed for the new TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pTsiCnctOpen			Pointer to tsi channel configuration structure.
f_pusTsiChanIndex		Allocated entry in TSI channel list.
f_pusTsiMemIndex		Allocated entry in the TSI control memory.
f_pusInputTsstIndex		TSST memory index of the input samples.
f_pusOutputTsstIndex	TSST memory index of the output samples.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveTsiResources
UINT32 Oct6100ApiReserveTsiResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				OUT		PUINT16							f_pusTsiChanIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulResult;
	UINT32	ulTempVar;
	BOOL	fTsiChanEntry = FALSE;
	BOOL	fTsiMemEntry = FALSE;
	BOOL	fInputTsst = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Reserve an entry in the TSI connection list. */
	ulResult = Oct6100ApiReserveTsiCnctEntry( f_pApiInstance, f_pusTsiChanIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		fTsiChanEntry = TRUE;
		
		/* Find a TSI memory entry. */
		ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, f_pusTsiMemIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			fTsiMemEntry = TRUE;
		
			/* Reserve the input TSST entry. */	
			ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
											  f_pTsiCnctOpen->ulInputTimeslot, 
											  f_pTsiCnctOpen->ulInputStream, 
											  cOCT6100_NUMBER_TSSTS_1, 
  											  cOCT6100_INPUT_TSST,
											  f_pusInputTsstIndex, 
											  NULL );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				fInputTsst = TRUE;

				/* Reserve the output TSST entry. */
				ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  f_pTsiCnctOpen->ulOutputTimeslot, 
												  f_pTsiCnctOpen->ulOutputStream, 
												  cOCT6100_NUMBER_TSSTS_1, 
  												  cOCT6100_OUTPUT_TSST,
												  f_pusOutputTsstIndex, 
												  NULL );
			}
		}
		else
		{
			/* Return an error other then a fatal. */
			ulResult = cOCT6100_ERR_TSI_CNCT_NO_MORE_TSI_AVAILABLE;
		}
	}

	if ( ulResult != cOCT6100_ERR_OK )
	{
		if( fTsiChanEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiCnctEntry( f_pApiInstance, *f_pusTsiChanIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fTsiMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, *f_pusTsiMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fInputTsst == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											   f_pTsiCnctOpen->ulInputTimeslot, 
											   f_pTsiCnctOpen->ulInputStream, 
											   cOCT6100_NUMBER_TSSTS_1, 
											   cOCT6100_INPUT_TSST,
											   cOCT6100_INVALID_INDEX );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteTsiStructs

Description:    Performs all the required structure writes to configure the
				new TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pTsiCnctOpen			Pointer to tsi connection open structure.
f_usTsiMemIndex			Allocated entry in the TSI control memory.
f_usInputTsstIndex		TSST memory index of the input samples.
f_usOutputTsstIndex		TSST memory index of the output samples.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteTsiStructs
UINT32 Oct6100ApiWriteTsiStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*==================================================================================*/
	/* Configure the TSST control memory.*/
	
	/* Set the input TSST control entry.*/
	ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
													  f_usInputTsstIndex,
													  f_usTsiMemIndex,
													  cOCT6100_PCM_U_LAW );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the output TSST control entry. */
	ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
													   f_usOutputTsstIndex,
													   cOCT6100_ADPCM_IN_LOW_BITS,
													   1,
													   f_usTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*==================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateTsiEntry

Description:    Updates the new TSI connection in the TSI connection list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pTsiCnctOpen			Pointer to TSI connection open configuration structure.
f_usTsiMemIndex			Allocated entry in TSI chariot memory.
f_usTsiChanIndex		Allocated entry in the TSI channel list.
f_usInputTsstIndex		TSST control memory index of the input TSST.
f_usOutputTsstIndex		TSST control memory index of the output TSST.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateTsiEntry
UINT32 Oct6100ApiUpdateTsiEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				IN		UINT16							f_usTsiChanIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex )
{
	tPOCT6100_API_TSI_CNCT	pTsiCnctEntry;

	/*================================================================================*/
	/* Obtain a pointer to the new TSI connection's list entry. */

	mOCT6100_GET_TSI_CNCT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsiCnctEntry, f_usTsiChanIndex )

	/* Copy the TSI's configuration and allocated resources. */
	pTsiCnctEntry->usInputTimeslot = (UINT16)( f_pTsiCnctOpen->ulInputTimeslot & 0xFFFF );
	pTsiCnctEntry->usInputStream = (UINT16)( f_pTsiCnctOpen->ulInputStream & 0xFFFF );

	pTsiCnctEntry->usOutputTimeslot = (UINT16)( f_pTsiCnctOpen->ulOutputTimeslot & 0xFFFF );
	pTsiCnctEntry->usOutputStream = (UINT16)( f_pTsiCnctOpen->ulOutputStream & 0xFFFF );

	/* Store hardware related information. */
	pTsiCnctEntry->usTsiMemIndex = f_usTsiMemIndex;
	pTsiCnctEntry->usInputTsstIndex = f_usInputTsstIndex;
	pTsiCnctEntry->usOutputTsstIndex = f_usOutputTsstIndex;
	
	/* Form handle returned to user. */
	*f_pTsiCnctOpen->pulTsiCnctHndl = cOCT6100_HNDL_TAG_TSI_CNCT | (pTsiCnctEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usTsiChanIndex;

	/* Finally, mark the connection as opened. */
	pTsiCnctEntry->fReserved = TRUE;
	
	/* Increment the number of TSI connection opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberTsiCncts++;

	/*================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100TsiCnctCloseSer

Description:    Closes a TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctClose			Pointer to TSI connection close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100TsiCnctCloseSer
UINT32 Oct6100TsiCnctCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_CLOSE			f_pTsiCnctClose )
{
	UINT16	usTsiChanIndex;
	UINT16	usTsiMemIndex;
	UINT16	usInputTsstIndex;
	UINT16	usOutputTsstIndex;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertTsiParams( f_pApiInstance, f_pTsiCnctClose, &usTsiChanIndex, &usTsiMemIndex, &usInputTsstIndex, &usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the TSI channel. */
	ulResult = Oct6100ApiInvalidateTsiStructs( f_pApiInstance, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the TSI connection. */
	ulResult = Oct6100ApiReleaseTsiResources( f_pApiInstance, usTsiChanIndex, usTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Invalidate the handle. */
	f_pTsiCnctClose->ulTsiCnctHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertTsiParams

Description:    Validate the handle given by the user and verify the state of 
				the TSI connection about to be closed. 
				Also returns all required information to deactivate the connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pTsiCnctClose			Pointer to TSI connection close structure.
f_pusTsiChanIndex		Index of the TSI connection structure in the API list.
f_pusTsiMemIndex		Index of the TSI entry within the TSI chariot memory
f_pusInputTsstIndex		Index of the input entry in the TSST control memory.
f_pusOutputTsstIndex	Index of the output entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertTsiParams
UINT32 Oct6100ApiAssertTsiParams( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_CLOSE			f_pTsiCnctClose,
				OUT		PUINT16								f_pusTsiChanIndex,
				OUT		PUINT16								f_pusTsiMemIndex,
				OUT		PUINT16								f_pusInputTsstIndex,
				OUT		PUINT16								f_pusOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tPOCT6100_API_TSI_CNCT	pTsiEntry;
	UINT32					ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pTsiCnctClose->ulTsiCnctHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_TSI_CNCT )
		return cOCT6100_ERR_TSI_CNCT_INVALID_HANDLE;

	*f_pusTsiChanIndex = (UINT16)( f_pTsiCnctClose->ulTsiCnctHndl & cOCT6100_HNDL_INDEX_MASK );
	
	if ( *f_pusTsiChanIndex >= pSharedInfo->ChipConfig.usMaxTsiCncts )
		return cOCT6100_ERR_TSI_CNCT_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_TSI_CNCT_ENTRY_PNT( pSharedInfo, pTsiEntry, *f_pusTsiChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pTsiCnctClose->ulTsiCnctHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pTsiEntry->fReserved != TRUE )
		return cOCT6100_ERR_TSI_CNCT_NOT_OPEN;
	if ( ulEntryOpenCnt != pTsiEntry->byEntryOpenCnt )
		return cOCT6100_ERR_TSI_CNCT_INVALID_HANDLE;

	/* Return info needed to close the channel and release all resources. */
	*f_pusInputTsstIndex	= pTsiEntry->usInputTsstIndex;
	*f_pusOutputTsstIndex	= pTsiEntry->usOutputTsstIndex;
	*f_pusTsiMemIndex		= pTsiEntry->usTsiMemIndex;
	
	/*=======================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateTsiStructs

Description:    This function closes a TSI connection.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usInputTsstIndex		Index of the input entry in the TSST control memory.
f_usOutputTsstIndex		Index of the output entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateTsiStructs
UINT32 Oct6100ApiInvalidateTsiStructs( 
				IN OUT  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usInputTsstIndex,
				IN		UINT16								f_usOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*==================================================================================*/
	/* Deactivate the TSST control memory. */
	
	/* Set the input TSST control entry to unused. */
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usInputTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the output TSST control entry to unused. */
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usOutputTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*==================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseTsiResources

Description:	Release and clear the API entry associated to the TSI channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usTsiChanIndex		Index of the TSI connection in the API list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseTsiResources
UINT32 Oct6100ApiReleaseTsiResources( 
				IN OUT  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usTsiChanIndex,
				IN		UINT16								f_usTsiMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_TSI_CNCT	pTsiEntry;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_TSI_CNCT_ENTRY_PNT( pSharedInfo, pTsiEntry, f_usTsiChanIndex );

	/* Release the entry in the TSI connection list. */
	ulResult = Oct6100ApiReleaseTsiCnctEntry( f_pApiInstance, f_usTsiChanIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, f_usTsiMemIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			/* Release the input entry. */
			ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
											  pTsiEntry->usInputTimeslot,
											  pTsiEntry->usInputStream,
 											  cOCT6100_NUMBER_TSSTS_1,
											  cOCT6100_INPUT_TSST,
											  cOCT6100_INVALID_INDEX );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				/* Release the output TSST entry. */
				ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
												  pTsiEntry->usOutputTimeslot,
												  pTsiEntry->usOutputStream,
 												  cOCT6100_NUMBER_TSSTS_1,
												  cOCT6100_OUTPUT_TSST,
												  cOCT6100_INVALID_INDEX );
			}
		}
	}

	/* Check if an error occured while releasing the reserved resources. */
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult >= cOCT6100_ERR_FATAL )
			return ulResult;
		else
			return cOCT6100_ERR_FATAL_4A;
	}

	/*=============================================================*/
	/* Update the TSI connection's list entry. */

	/* Mark the connection as closed. */
	pTsiEntry->fReserved = FALSE;
	pTsiEntry->byEntryOpenCnt++;

	/* Decrement the number of TSI connection opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberTsiCncts--;
	
	/*=============================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveTsiCnctEntry

Description:    Reserves one of the TSI connection API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusTsiChanIndex		Resulting index reserved in the TSI channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveTsiCnctEntry
UINT32 Oct6100ApiReserveTsiCnctEntry(
				IN	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT	PUINT16						f_pusTsiChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pTsiChanAlloc;
	UINT32	ulResult;
	UINT32	ulTsiIndex;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_TSI_CNCT_ALLOC_PNT( pSharedInfo, pTsiChanAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pTsiChanAlloc, &ulTsiIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_TSI_CNCT_ALL_CHANNELS_ARE_OPENED;
		else
			return cOCT6100_ERR_FATAL_4B;
	}

	*f_pusTsiChanIndex = (UINT16)( ulTsiIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseTsiCnctEntry

Description:    Releases the specified TSI connection API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usTsiChanIndex		Index reserved in the TSI channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseTsiCnctEntry
UINT32 Oct6100ApiReleaseTsiCnctEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT16						f_usTsiChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pTsiChanAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_TSI_CNCT_ALLOC_PNT( pSharedInfo, pTsiChanAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pTsiChanAlloc, f_usTsiChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_4C;
	}

	return cOCT6100_ERR_OK;
}
#endif
