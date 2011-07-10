/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_tsst.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions used to manage the allocation of TSST
	control structures in internal memory.

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

$Octasic_Revision: 39 $

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
#include "oct6100api/oct6100_tsst_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_tsst_priv.h"


/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetTsstSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of TSSTs.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetTsstSwSizes
UINT32 Oct6100ApiGetTsstSwSizes(
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Determine amount of TSST needed for TSST allocation table. */
	f_pInstSizes->ulTsstAlloc = 4096 / 8;

	/* Calculate the API memory required for the TSST entry list. */
	f_pInstSizes->ulTsstEntryList = cOCT6100_MAX_TSSTS * sizeof( tOCT6100_API_TSST_ENTRY );

	/* Calculate memory needed for TSST entry allocation. */
	ulResult = OctapiLlmAllocGetSize( cOCT6100_MAX_TSSTS, &f_pInstSizes->ulTsstEntryAlloc );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_4D;
	
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsstAlloc, ulTempVar );
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsstEntryList, ulTempVar );
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsstEntryAlloc, ulTempVar );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiTsstSwInit

Description:    Initializes all elements of the instance structure associated
				to the TSST control entries.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This tsst is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiTsstSwInit
UINT32 Oct6100ApiTsstSwInit(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_TSST_ENTRY	pTsstList;
	PUINT32	pulTsstAlloc;
	PVOID	pTsstListAlloc;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize the TSST allocation table to "all free". */
	mOCT6100_GET_TSST_ALLOC_PNT( pSharedInfo, pulTsstAlloc );
	Oct6100UserMemSet( pulTsstAlloc, 0x00, 512 );

	/* Initialize all the TSST list entries. */
	mOCT6100_GET_TSST_LIST_PNT( pSharedInfo, pTsstList );
	Oct6100UserMemSet( pTsstList, 0xFF, cOCT6100_MAX_TSSTS * sizeof(tOCT6100_API_TSST_ENTRY) );

	/* Initialize the allocation list to manage the TSST entries.*/
	mOCT6100_GET_TSST_LIST_ALLOC_PNT( pSharedInfo, pTsstListAlloc )
	
	ulResult = OctapiLlmAllocInit( &pTsstListAlloc, cOCT6100_MAX_TSSTS );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_4E;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiValidateTsst

Description:    Validates a timeslot, stream combination.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This tsst is used to keep
						the present state of the chip and all its resources.

f_ulTimeslot			Timeslot component of the TDM TSST.
f_ulStream				Stream component of the TDM TSST.
f_ulNumTssts			Number of TSST required.
f_ulDirection			Direction of the TSST (Input or Output).

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiValidateTsst
UINT32 Oct6100ApiValidateTsst( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulNumTssts,
				IN		UINT32						f_ulTimeslot,
				IN		UINT32						f_ulStream,
				IN		UINT32						f_ulDirection )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;	
	PUINT32						pulTsstAlloc;
	
	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	mOCT6100_GET_TSST_ALLOC_PNT( f_pApiInstance->pSharedInfo, pulTsstAlloc );

	/* Obtain local pointer to chip configuration. */
	pChipConfig = &pSharedInfo->ChipConfig;

	/* Check the TDM streams, timeslots component for errors. */
	if ( f_ulTimeslot == cOCT6100_UNASSIGNED &&
		 f_ulStream != cOCT6100_UNASSIGNED )
		return cOCT6100_ERR_TSST_TIMESLOT;

	if ( f_ulTimeslot != cOCT6100_UNASSIGNED &&
		 f_ulStream == cOCT6100_UNASSIGNED )
		return cOCT6100_ERR_TSST_STREAM;

	if ( f_ulStream >= pChipConfig->byMaxTdmStreams )
		return cOCT6100_ERR_TSST_STREAM;

	/* Check timeslot value based on the frequenccy of the selected stream. */
	switch ( pChipConfig->aulTdmStreamFreqs[ f_ulStream / 4 ] )
	{
	case cOCT6100_TDM_STREAM_FREQ_2MHZ:
		if ( f_ulTimeslot >= 32 )
			return cOCT6100_ERR_TSST_TIMESLOT;
		break;
	case cOCT6100_TDM_STREAM_FREQ_4MHZ:
		if ( f_ulTimeslot >= 64 )
			return cOCT6100_ERR_TSST_TIMESLOT;
		break;
	case cOCT6100_TDM_STREAM_FREQ_8MHZ:
		if ( f_ulTimeslot >= 128 )
			return cOCT6100_ERR_TSST_TIMESLOT;
		break;
	case cOCT6100_TDM_STREAM_FREQ_16MHZ:
		if ( f_ulTimeslot >= 256 )
			return cOCT6100_ERR_TSST_TIMESLOT;

		/* Check the stream value based on the direction. */
		if ( f_ulDirection == cOCT6100_INPUT_TSST && f_ulStream >= 16 )
		{
			return cOCT6100_ERR_TSST_STREAM;
		}
		else if( f_ulDirection == cOCT6100_OUTPUT_TSST && f_ulStream < 16 )
		{
			return cOCT6100_ERR_TSST_STREAM;
		}

		break;
	default:
		return cOCT6100_ERR_FATAL_DC;
	}

	/* Stream must be odd if two TSSTs are required. */
	if ( f_ulNumTssts == 2 && ( ( f_ulStream & 0x1) != 0x1 ) )
		return cOCT6100_ERR_TSST_STREAM;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveTsst

Description:    Reserves a TSST, only one TSI entry can access a TSST at any one
				time.
				If the pointer f_pulTsstListIndex is set to NULL, no TSST list
				entry will be reserved.

				The index in TSST control memory returned is based on the frequency
				of the streams where the TSST is located and on the direction of 
				the TSST ( input or output ).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This tsst is used to keep
						the present state of the chip and all its resources.

f_ulTimeslot			Timeslot component of the TDM TSST.
f_ulNumTssts			Number of TSSTs required.
f_ulStream				Stream component of the TDM TSST.
f_ulDirection			Whether the TSST in and input TSST or output TSST.
f_pusTsstMemIndex		Index of the resulting TSST in the TSST control memory.
f_pusTsstListIndex		Index in the TSST list of the current entry.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveTsst
UINT32 Oct6100ApiReserveTsst( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulTimeslot,
				IN		UINT32						f_ulStream,
				IN		UINT32						f_ulNumTsst,
				IN		UINT32						f_ulDirection,
				OUT		PUINT16						f_pusTsstMemIndex,
				OUT		PUINT16						f_pusTsstListIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	PVOID	pTsstListAlloc;
	PUINT32	pulTsstAlloc;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulStream;
	UINT32	ulTimeslot;

	/* Get local pointer to shared portion of API instance structure. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_TSST_ALLOC_PNT( f_pApiInstance->pSharedInfo, pulTsstAlloc );

	/*==================================================================================*/
	/* Now make the proper conversion to obtain the TSST value. */

	/* Save the timeslot and stream value received. */
	ulStream	= f_ulStream;
	ulTimeslot	= f_ulTimeslot;

	/* Set the TSST index associated to this stream, timeslot combination. */
	switch ( f_pApiInstance->pSharedInfo->ChipConfig.aulTdmStreamFreqs[ f_ulStream / 4 ] )
	{
	case cOCT6100_TDM_STREAM_FREQ_16MHZ:
		if ( f_ulDirection == cOCT6100_INPUT_TSST )
		{
			ulStream	= f_ulStream + ( f_ulTimeslot % 2 ) * 16;
			ulTimeslot	= f_ulTimeslot / 2;
		}
		else /* f_ulDirection == cOCT6100_OUTPUT_TSST */
		{
			ulStream	= ( f_ulStream - 16 ) + ( f_ulTimeslot % 2 ) * 16;
			
			if ( f_ulStream < 28 && ((f_ulTimeslot % 2) == 1) )
			{
				ulTimeslot	= ((f_ulTimeslot / 2) + 4) % 128;
			}
			else
			{
				ulTimeslot	= f_ulTimeslot / 2 ;
			}
		}

		*f_pusTsstMemIndex = (UINT16)( ulTimeslot * 32 + ulStream );
		break;

	case cOCT6100_TDM_STREAM_FREQ_8MHZ:
		*f_pusTsstMemIndex = (UINT16)( ulTimeslot * 32 + ulStream );
		break;
	
	case cOCT6100_TDM_STREAM_FREQ_4MHZ:
		*f_pusTsstMemIndex = (UINT16)( ulTimeslot * 32 * 2 );
		if ( f_ulDirection == cOCT6100_OUTPUT_TSST )
		{
			*f_pusTsstMemIndex = (UINT16)( *f_pusTsstMemIndex + ulStream );
		}
		else /* if ( f_ulDirection == cOCT6100_INPUT_TSST ) */
		{
			*f_pusTsstMemIndex = (UINT16)( ( 1 * 32 + ulStream ) + *f_pusTsstMemIndex );
		}
		break;
	
	case cOCT6100_TDM_STREAM_FREQ_2MHZ:
		*f_pusTsstMemIndex = (UINT16)( ulTimeslot * 32 * 4 );
		if ( f_ulDirection == cOCT6100_OUTPUT_TSST )
		{
			*f_pusTsstMemIndex = (UINT16)( ulStream + *f_pusTsstMemIndex );
		}
		else /* if ( f_ulDirection == cOCT6100_INPUT_TSST ) */
		{
			*f_pusTsstMemIndex = (UINT16)( ( 3 * 32 + ulStream ) + *f_pusTsstMemIndex );
		}
		break;
	
	default:
		ulResult =  cOCT6100_ERR_FATAL_8B;
	}
	/*======================================================================*/


	/*======================================================================*/
	/* First reserve the TSST. */

	/* Get local pointer to TSST's entry in allocation table. */
	switch ( pSharedInfo->ChipConfig.aulTdmStreamFreqs[ ulStream / 4 ] )
	{
	case cOCT6100_TDM_STREAM_FREQ_2MHZ:		
		ulTimeslot *= 4;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_4MHZ:		
		ulTimeslot *= 2;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_8MHZ:		
		ulTimeslot *= 1;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_16MHZ:	
		ulTimeslot *= 1;	
		break;
	default:
		return cOCT6100_ERR_FATAL_DD;
	}

	/* Check if entry is already reserved. */
	if ( ((pulTsstAlloc[ ulTimeslot ] >> ulStream) & 0x1) == 0x1 )
		return cOCT6100_ERR_TSST_TSST_RESERVED;
	
	/* Check and reserve the associated TSST if required. */
	if ( f_ulNumTsst == 2 )
	{
		/* Check if entry is already reserved. */
		if ( ((pulTsstAlloc[ ulTimeslot ] >> (ulStream - 1) ) & 0x1) == 0x1 )
			return cOCT6100_ERR_TSST_ASSOCIATED_TSST_RESERVED;

		/* The entry is free, it won't anymore. */
		pulTsstAlloc[ ulTimeslot ] |= (0x1 << (ulStream - 1));
	}

	/* The entry is free, it won't anymore.*/
	pulTsstAlloc[ ulTimeslot ] |= (0x1 << ulStream);

	/*======================================================================*/

	
	/*======================================================================*/
	/* Now reserve a TSST entry if requested. */

	if ( f_pusTsstListIndex != NULL && ulResult == cOCT6100_ERR_OK )
	{
		UINT32 ulTsstListIndex;

		/* Reserve a TSST entry in the API TSST list. */
		mOCT6100_GET_TSST_LIST_ALLOC_PNT( f_pApiInstance->pSharedInfo, pTsstListAlloc );

		ulResult = OctapiLlmAllocAlloc( pTsstListAlloc, &ulTsstListIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
				ulResult = cOCT6100_ERR_TSST_ALL_TSSTS_ARE_OPENED;
			else
				ulResult = cOCT6100_ERR_FATAL_52;
		}

		*f_pusTsstListIndex = (UINT16)( ulTsstListIndex & 0xFFFF );
	}
	/*======================================================================*/
	

	/*======================================================================*/
	/* Check the result of the TSST list reservation. */

	if ( ulResult != cOCT6100_ERR_OK )
	{
		/* Release the previously reserved TSST. */
		if ( f_ulNumTsst == 2  )
		{
			/* Clear the entry. */
			pulTsstAlloc[ ulTimeslot ] &= ~(0x1 << (ulStream - 1) );

		}

		/* Clear the entry. */
		pulTsstAlloc[ ulTimeslot ] &= ~(0x1 << ulStream);
	}

	/*======================================================================*/

	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseTsst

Description:    Releases a TSST.

				If f_usTsstListIndex is set to cOCT6100_INVALID_INDEX, the API
				will assume that no TSST list entry was reserved for this TSST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This tsst is used to keep
						the present state of the chip and all its resources.

f_ulNumTssts			Number of TSSTs to be released.
f_ulStream				Stream component of the TDM TSST.
f_ulTimeslot			Timeslot component of the TDM TSST.
f_ulDirection			Whether the TSST is an input TSST or output TSST.
f_usTsstListIndex		Index in the TSST list of the current entry.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseTsst
UINT32 Oct6100ApiReleaseTsst( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulTimeslot,
				IN		UINT32						f_ulStream,
				IN		UINT32						f_ulNumTsst,
				IN		UINT32						f_ulDirection,
				IN		UINT16						f_usTsstListIndex)
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	PUINT32	pulTsstAlloc;
	PVOID	pTsstListAlloc;
	UINT32	ulResult;
	UINT32	ulStream;
	UINT32	ulTimeslot;

	/* Get local pointer to shared portion of API instance structure. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	if ( f_usTsstListIndex != cOCT6100_INVALID_INDEX )
	{
		mOCT6100_GET_TSST_LIST_ALLOC_PNT( pSharedInfo, pTsstListAlloc )
	
		ulResult = OctapiLlmAllocDealloc( pTsstListAlloc, f_usTsstListIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			return cOCT6100_ERR_FATAL_53;
		}
	}

	mOCT6100_GET_TSST_ALLOC_PNT( f_pApiInstance->pSharedInfo, pulTsstAlloc );

	/*==================================================================================*/
	/* Now make the proper conversion to obtain the TSST value. */

	/* Save the timeslot and stream value received. */
	ulStream	= f_ulStream;
	ulTimeslot	= f_ulTimeslot;
	
	/* Set the TSST index associated to this stream, timeslot combination. */
	if ( pSharedInfo->ChipConfig.aulTdmStreamFreqs[ f_ulStream / 4 ] == cOCT6100_TDM_STREAM_FREQ_16MHZ )
	{
		if ( f_ulDirection == cOCT6100_INPUT_TSST )
		{
			ulStream	= f_ulStream + ( f_ulTimeslot % 2 ) * 16;
			ulTimeslot	= f_ulTimeslot / 2;
		}
		else /* f_ulDirection == cOCT6100_OUTPUT_TSST */
		{
			ulStream	= ( f_ulStream - 16 ) + ( f_ulTimeslot % 2 ) * 16;
			
			if ( f_ulStream < 28 && ((f_ulTimeslot % 2) == 1) )
			{
				ulTimeslot	= ((f_ulTimeslot / 2) + 4) % 128;
			}
			else
			{
				ulTimeslot	= f_ulTimeslot / 2 ;
			}
		}
	}

	/* Get local pointer to TSST's entry in allocation table. */
	switch ( pSharedInfo->ChipConfig.aulTdmStreamFreqs[ ulStream / 4 ] )
	{
	case cOCT6100_TDM_STREAM_FREQ_2MHZ:		
		ulTimeslot *= 4;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_4MHZ:		
		ulTimeslot *= 2;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_8MHZ:		
		ulTimeslot *= 1;	
		break;
	case cOCT6100_TDM_STREAM_FREQ_16MHZ:	
		ulTimeslot *= 1;	
		break;
	default:
		return cOCT6100_ERR_FATAL_DE;
	}

	/* Check if entry is actualy reserved. */
	if ( ((pulTsstAlloc[ ulTimeslot ] >> ulStream) & 0x1) != 0x1 )
		return cOCT6100_ERR_FATAL_55;

	/*==================================================================================*/

	/* Clear the entry. */
	pulTsstAlloc[ ulTimeslot ] &= ~(0x1 << ulStream);

	/* Check and release the associated TSST if required. */
	if ( f_ulNumTsst == 2 )
	{
		/* Check if entry is actualy reserved. */
		if ( ((pulTsstAlloc[ ulTimeslot ] >> ( ulStream - 1)) & 0x1) != 0x1 )
			return cOCT6100_ERR_FATAL_54;
		
		/* Clear the entry. */
		pulTsstAlloc[ ulTimeslot ] &= ~(0x1 << (ulStream - 1));

	}

	return cOCT6100_ERR_OK;
}
#endif
