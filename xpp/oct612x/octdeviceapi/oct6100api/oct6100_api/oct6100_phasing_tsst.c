/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_phasing_tsst.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to open and close phasing TSSTs.

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

$Octasic_Revision: 46 $

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
#include "oct6100api/oct6100_phasing_tsst_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_phasing_tsst_pub.h"
#include "oct6100api/oct6100_channel_inst.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_phasing_tsst_priv.h"

/****************************  PUBLIC FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100PhasingTsstOpen

Description:    This function opens a phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstOpen		Pointer to phasing TSST open structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100PhasingTsstOpenDef
UINT32 Oct6100PhasingTsstOpenDef(
				tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen )
{
	f_pPhasingTsstOpen->pulPhasingTsstHndl = NULL;
	
	f_pPhasingTsstOpen->ulTimeslot = cOCT6100_INVALID_TIMESLOT;
	f_pPhasingTsstOpen->ulStream = cOCT6100_INVALID_STREAM;
	
	f_pPhasingTsstOpen->ulPhasingLength = 88;



	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100PhasingTsstOpen
UINT32 Oct6100PhasingTsstOpen(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen )
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
		ulFncRes = Oct6100PhasingTsstOpenSer( f_pApiInstance, f_pPhasingTsstOpen );
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

Function:		Oct6100PhasingTsstClose

Description:    This function closes a phasing TSST

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstClose		Pointer to phasing TSST close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100PhasingTsstCloseDef
UINT32 Oct6100PhasingTsstCloseDef(
				tPOCT6100_PHASING_TSST_CLOSE		f_pPhasingTsstClose )
{
	f_pPhasingTsstClose->ulPhasingTsstHndl = cOCT6100_INVALID_HANDLE;
	
	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100PhasingTsstClose
UINT32 Oct6100PhasingTsstClose(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_PHASING_TSST_CLOSE		f_pPhasingTsstClose )
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
		ulFncRes = Oct6100PhasingTsstCloseSer( f_pApiInstance, f_pPhasingTsstClose );
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

Function:		Oct6100ApiGetPhasingTsstSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of Phasing TSSTs.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetPhasingTsstSwSizes
UINT32 Oct6100ApiGetPhasingTsstSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;
	
	/* Determine the amount of memory required for the API phasing TSST list. */
	f_pInstSizes->ulPhasingTsstList = f_pOpenChip->ulMaxPhasingTssts * sizeof( tOCT6100_API_PHASING_TSST );

	if ( f_pOpenChip->ulMaxPhasingTssts > 0 )
	{
		/* Calculate memory needed for Phasing TSST API memory allocation */
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxPhasingTssts, &f_pInstSizes->ulPhasingTsstAlloc );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_38;
	}
	else
	{
		f_pInstSizes->ulPhasingTsstAlloc = 0;
	}

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulPhasingTsstList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulPhasingTsstAlloc, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiPhasingTsstSwInit

Description:    Initializes all elements of the instance structure associated
				to phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiPhasingTsstSwInit
UINT32 Oct6100ApiPhasingTsstSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_PHASING_TSST	pPhasingTsstList;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32	ulMaxPhasingTssts;
	PVOID	pPhasingTsstAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize the phasing TSST API list. */
	ulMaxPhasingTssts = pSharedInfo->ChipConfig.usMaxPhasingTssts;

	/* Set all entries in the phasing TSST list to unused. */
	mOCT6100_GET_PHASING_TSST_LIST_PNT( pSharedInfo, pPhasingTsstList )

	/* Clear the memory */
	Oct6100UserMemSet( pPhasingTsstList, 0x00, sizeof(tOCT6100_API_PHASING_TSST) * ulMaxPhasingTssts );

	/* Initialize the phasing TSST allocation software to "all free". */
	if ( ulMaxPhasingTssts > 0 )
	{
		mOCT6100_GET_PHASING_TSST_ALLOC_PNT( pSharedInfo, pPhasingTsstAlloc )
		
		ulResult = OctapiLlmAllocInit( &pPhasingTsstAlloc, ulMaxPhasingTssts );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_39;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100PhasingTsstOpenSer

Description:    Opens a phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstOpen		Pointer to phasing TSST open configuration structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100PhasingTsstOpenSer
UINT32 Oct6100PhasingTsstOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_OPEN			f_pPhasingTsstOpen )
{
	UINT16	usPhasingIndex;
	UINT16	usTsstIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the phasing TSST for errors. */
	ulResult = Oct6100ApiCheckPhasingParams( f_pApiInstance, f_pPhasingTsstOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the phasing TSST. */
	ulResult = Oct6100ApiReservePhasingResources( f_pApiInstance, f_pPhasingTsstOpen, &usPhasingIndex, &usTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write all necessary structures to activate the phasing TSST. */
	ulResult = Oct6100ApiWritePhasingStructs( f_pApiInstance, f_pPhasingTsstOpen, usPhasingIndex, usTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the new phasing TSST entry in the API list. */
	ulResult = Oct6100ApiUpdatePhasingEntry( f_pApiInstance, f_pPhasingTsstOpen, usPhasingIndex, usTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckPhasingParams

Description:    Checks the user's phasing TSST open configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstOpen		Pointer to phasing TSST open configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckPhasingParams
UINT32 Oct6100ApiCheckPhasingParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen )
{
	UINT32	ulResult;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxPhasingTssts == 0 )
		return cOCT6100_ERR_PHASING_TSST_DISABLED;

	if ( f_pPhasingTsstOpen->pulPhasingTsstHndl == NULL )
		return cOCT6100_ERR_PHASING_TSST_INVALID_HANDLE;

	/* Check the phasing length. */
	if ( f_pPhasingTsstOpen->ulPhasingLength > 240 ||
		 f_pPhasingTsstOpen->ulPhasingLength < 2 )
		return cOCT6100_ERR_PHASING_TSST_PHASING_LENGTH;



	/* Check the input TDM streams, timeslots component for errors. */
	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   cOCT6100_NUMBER_TSSTS_1,
									   f_pPhasingTsstOpen->ulTimeslot, 
									   f_pPhasingTsstOpen->ulStream,
									   cOCT6100_INPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_PHASING_TSST_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_PHASING_TSST_STREAM;
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

Function:		Oct6100ApiReservePhasingResources

Description:    Reserves all resources needed for the new phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pPhasingTsstOpen		Pointer to phasing TSST configuration structure.
f_pusPhasingIndex		Allocated entry in Phasing TSST API list.
f_pusTsstIndex			TSST memory index of the counter.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReservePhasingResources
UINT32 Oct6100ApiReservePhasingResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				OUT		PUINT16							f_pusPhasingIndex,
				OUT		PUINT16							f_pusTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulResult;
	UINT32	ulTempVar;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Reserve an entry in the phasing TSST list. */
	ulResult = Oct6100ApiReservePhasingEntry( f_pApiInstance, 
											  f_pusPhasingIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		/* Reserve the input TSST entry. */	
		ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
										  f_pPhasingTsstOpen->ulTimeslot, 
										  f_pPhasingTsstOpen->ulStream, 
										  cOCT6100_NUMBER_TSSTS_1,
										  cOCT6100_INPUT_TSST,
										  f_pusTsstIndex, 
										  NULL );
		if ( ulResult != cOCT6100_ERR_OK  )
		{
			/* Release the previously reserved entries. */
			ulTempVar = Oct6100ApiReleasePhasingEntry( f_pApiInstance, *f_pusPhasingIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;

			return ulResult;
		}
	}
	else
	{
		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWritePhasingStructs

Description:    Performs all the required structure writes to configure the
				new phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pPhasingTsstOpen		Pointer to phasing TSST configuration structure.
f_usPhasingIndex		Allocated entry in phasing TSST API list.
f_usTsstIndex			TSST memory index of the counter.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWritePhasingStructs
UINT32 Oct6100ApiWritePhasingStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				IN		UINT16							f_usPhasingIndex,
				IN		UINT16							f_usTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulPhasingTsstChariotMemIndex;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*------------------------------------------------------------------------------*/
	/* Configure the TSST control memory of the phasing TSST. */
	
	/* Find the asociated entry in the chariot memory for the phasing TSST. */
	ulPhasingTsstChariotMemIndex = cOCT6100_TSST_CONTROL_PHASING_TSST_BASE_ENTRY + f_usPhasingIndex;

	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = cOCT6100_TSST_CONTROL_MEM_INPUT_TSST;
	WriteParams.usWriteData |= ulPhasingTsstChariotMemIndex & cOCT6100_TSST_CONTROL_MEM_TSI_MEM_MASK;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*------------------------------------------------------------------------------*/

	/*------------------------------------------------------------------------------*/
	/* Write the phasing length of the TSST in the ADPCM / MIXER memory. */

	WriteParams.ulWriteAddress = cOCT6100_CONVERSION_CONTROL_PHASE_SIZE_BASE_ADD + ( f_usPhasingIndex * 2 );
	WriteParams.usWriteData  = (UINT16)( f_pPhasingTsstOpen->ulPhasingLength );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdatePhasingEntry

Description:    Updates the new phasing TSST in the API phasing TSST list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pPhasingTsstOpen		Pointer to phasing TSST open structure.
f_usPhasingIndex		Allocated entry in phasing TSST API list.
f_usTsstIndex			TSST memory index of the counter.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdatePhasingEntry
UINT32 Oct6100ApiUpdatePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				IN		UINT16							f_usPhasingIndex,
				IN		UINT16							f_usTsstIndex )
{
	tPOCT6100_API_PHASING_TSST	pPhasingTsstEntry;

	/*================================================================================*/
	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_PHASING_TSST_ENTRY_PNT( f_pApiInstance->pSharedInfo, pPhasingTsstEntry, f_usPhasingIndex )

	/* Copy the phasing TSST's configuration and allocated resources. */
	pPhasingTsstEntry->usTimeslot = (UINT16)( f_pPhasingTsstOpen->ulTimeslot & 0xFFFF );
	pPhasingTsstEntry->usStream = (UINT16)( f_pPhasingTsstOpen->ulStream & 0xFFFF );
	
	pPhasingTsstEntry->usPhasingLength = (UINT16)( f_pPhasingTsstOpen->ulPhasingLength & 0xFFFF );

	/* Store hardware related information. */
	pPhasingTsstEntry->usPhasingTsstIndex = f_usTsstIndex;
	
	/* Form handle returned to user. */
	*f_pPhasingTsstOpen->pulPhasingTsstHndl = cOCT6100_HNDL_TAG_PHASING_TSST | (pPhasingTsstEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usPhasingIndex;
	pPhasingTsstEntry->usDependencyCnt = 0;			/* Nobody is using the phasing TSST.*/
	
	/* Finally, mark the phasing TSST as open. */
	pPhasingTsstEntry->fReserved = TRUE;
	
	/* Increment the number of phasing TSSTs opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberPhasingTssts++;

	/*================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100PhasingTsstCloseSer

Description:    Closes a phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstClose		Pointer to phasing TSST close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100PhasingTsstCloseSer
UINT32 Oct6100PhasingTsstCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_CLOSE		f_pPhasingTsstClose )
{
	UINT16	usPhasingIndex;
	UINT16	usTsstIndex;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertPhasingParams( f_pApiInstance, f_pPhasingTsstClose, &usPhasingIndex, &usTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the phasing TSST. */
	ulResult = Oct6100ApiInvalidatePhasingStructs( f_pApiInstance, usTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the phasing TSST. */
	ulResult = Oct6100ApiReleasePhasingResources( f_pApiInstance, usPhasingIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	f_pPhasingTsstClose->ulPhasingTsstHndl = cOCT6100_INVALID_VALUE;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertPhasingParams

Description:    Validate the handle given by the user and verify the state of 
				the phasing TSST about to be closed. Also returns all 
				required information to deactivate the phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pPhasingTsstClose		Pointer to phasing TSST  close structure.
f_pusPhasingIndex		Index of the phasing TSST structure in the API list.
f_pusTsstIndex			Index of the entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertPhasingParams
UINT32 Oct6100ApiAssertPhasingParams( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_CLOSE		f_pPhasingTsstClose,
				OUT		PUINT16								f_pusPhasingIndex,
				OUT		PUINT16								f_pusTsstIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_PHASING_TSST	pPhasingEntry;
	UINT32						ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pPhasingTsstClose->ulPhasingTsstHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_PHASING_TSST )
		return cOCT6100_ERR_PHASING_TSST_INVALID_HANDLE;

	*f_pusPhasingIndex = (UINT16)( f_pPhasingTsstClose->ulPhasingTsstHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusPhasingIndex >= pSharedInfo->ChipConfig.usMaxPhasingTssts )
		return cOCT6100_ERR_PHASING_TSST_INVALID_HANDLE;

	/*=======================================================================*/
	/* Get a pointer to the phasing TSST's list entry. */

	mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pPhasingEntry, *f_pusPhasingIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pPhasingTsstClose->ulPhasingTsstHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pPhasingEntry->fReserved != TRUE )
		return cOCT6100_ERR_PHASING_TSST_NOT_OPEN;
	if ( pPhasingEntry->usDependencyCnt != 0 )
		return cOCT6100_ERR_PHASING_TSST_ACTIVE_DEPENDENCIES;
	if ( ulEntryOpenCnt != pPhasingEntry->byEntryOpenCnt )
		return cOCT6100_ERR_PHASING_TSST_INVALID_HANDLE;

	/* Return info needed to close the phasing TSST and release all resources. */
	*f_pusTsstIndex = pPhasingEntry->usPhasingTsstIndex;

	/*=======================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidatePhasingStructs

Description:    Closes a phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usTsstIndex			Index of the entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidatePhasingStructs
UINT32 Oct6100ApiInvalidatePhasingStructs( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*------------------------------------------------------------------------------*/
	/* Deactivate the TSST control memory. */
	
	/* Set the input TSST control entry to unused. */
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleasePhasingResources

Description:	Release and clear the API entry associated to the phasing TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usPhasingIndex		Index of the phasing TSST in the API list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleasePhasingResources
UINT32 Oct6100ApiReleasePhasingResources( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usPhasingIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_PHASING_TSST	pPhasingEntry;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pPhasingEntry, f_usPhasingIndex );

	/* Release the entry in the phasing TSST list. */
	ulResult = Oct6100ApiReleasePhasingEntry( f_pApiInstance, f_usPhasingIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		/* Release the entry. */
		ulResult = Oct6100ApiReleaseTsst( f_pApiInstance, 
										  pPhasingEntry->usTimeslot,
										  pPhasingEntry->usStream,
 									      cOCT6100_NUMBER_TSSTS_1,
										  cOCT6100_INPUT_TSST,
										  cOCT6100_INVALID_INDEX );		/* Release the TSST entry */	
		if ( ulResult != cOCT6100_ERR_OK )
		{
			return cOCT6100_ERR_FATAL;
		}
	}
	else
	{
		return ulResult;
	}

	/*=============================================================*/
	/* Update the phasing TSST's list entry. */

	/* Mark the entry as closed. */
	pPhasingEntry->fReserved = FALSE;
	pPhasingEntry->byEntryOpenCnt++;

	/* Decrement the number of phasing TSSTs opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberPhasingTssts--;

	/*=============================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReservePhasingEntry

Description:    Reserves a phasing TSST API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusPhasingIndex		Resulting index reserved in the phasing TSST list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReservePhasingEntry
UINT32 Oct6100ApiReservePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT		PUINT16						f_pusPhasingIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pPhasingAlloc;
	UINT32	ulResult;
	UINT32	ulPhasingIndex;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_PHASING_TSST_ALLOC_PNT( pSharedInfo, pPhasingAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pPhasingAlloc, &ulPhasingIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_PHASING_TSST_ALL_ENTRIES_ARE_OPENED;
		else
			return cOCT6100_ERR_FATAL_3A;
	}

	*f_pusPhasingIndex = (UINT16)( ulPhasingIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleasePhasingEntry

Description:    Releases the specified phasing TSST API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usPhasingIndex		Index reserved in the phasing TSST API list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleasePhasingEntry
UINT32 Oct6100ApiReleasePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT16						f_usPhasingIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pPhasingAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_PHASING_TSST_ALLOC_PNT( pSharedInfo, pPhasingAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pPhasingAlloc, f_usPhasingIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_3B;
	}

	return cOCT6100_ERR_OK;
}
#endif
