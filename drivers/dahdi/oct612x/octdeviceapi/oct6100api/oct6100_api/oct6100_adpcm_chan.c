/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_adpcm_chan.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to open and close ADPCM channels.

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

$Octasic_Revision: 16 $

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
#include "oct6100api/oct6100_adpcm_chan_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_adpcm_chan_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_adpcm_chan_priv.h"

/****************************  PUBLIC FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100AdpcmChanOpen

Description:    This function opens an ADPCM channel between two TDM timeslots.
				This channel will perform ADPCM compression or decompression 
				depending on the channel mode.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pAdpcmChanOpen		Pointer to ADPCM channel open structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100AdpcmChanOpenDef
UINT32 Oct6100AdpcmChanOpenDef(
				tPOCT6100_ADPCM_CHAN_OPEN			f_pAdpcmChanOpen )
{
	f_pAdpcmChanOpen->pulChanHndl = NULL;
	
	f_pAdpcmChanOpen->ulInputTimeslot	= cOCT6100_INVALID_TIMESLOT;
	f_pAdpcmChanOpen->ulInputStream		= cOCT6100_INVALID_STREAM;
	f_pAdpcmChanOpen->ulInputNumTssts	= 1;
	f_pAdpcmChanOpen->ulInputPcmLaw		= cOCT6100_PCM_U_LAW;

	f_pAdpcmChanOpen->ulOutputTimeslot	= cOCT6100_INVALID_TIMESLOT;
	f_pAdpcmChanOpen->ulOutputStream	= cOCT6100_INVALID_STREAM;
	f_pAdpcmChanOpen->ulOutputPcmLaw	= cOCT6100_PCM_U_LAW;
	f_pAdpcmChanOpen->ulOutputNumTssts	= 1;

	f_pAdpcmChanOpen->ulChanMode		= cOCT6100_ADPCM_ENCODING;
	f_pAdpcmChanOpen->ulEncodingRate	= cOCT6100_G726_32KBPS;
	f_pAdpcmChanOpen->ulDecodingRate	= cOCT6100_G726_32KBPS;
	
	f_pAdpcmChanOpen->ulAdpcmNibblePosition = cOCT6100_ADPCM_IN_LOW_BITS;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100AdpcmChanOpen
UINT32 Oct6100AdpcmChanOpen(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_ADPCM_CHAN_OPEN			f_pAdpcmChanOpen )
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
		ulFncRes = Oct6100AdpcmChanOpenSer( f_pApiInstance, f_pAdpcmChanOpen );
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

Function:		Oct6100AdpcmChanClose

Description:    This function closes an opened ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pAdpcmChanClose		Pointer to ADPCM channel close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100AdpcmChanCloseDef
UINT32 Oct6100AdpcmChanCloseDef(
				tPOCT6100_ADPCM_CHAN_CLOSE			f_pAdpcmChanClose )
{
	f_pAdpcmChanClose->ulChanHndl = cOCT6100_INVALID_HANDLE;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100AdpcmChanClose
UINT32 Oct6100AdpcmChanClose(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_ADPCM_CHAN_CLOSE			f_pAdpcmChanClose )
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
		ulFncRes = Oct6100AdpcmChanCloseSer( f_pApiInstance, f_pAdpcmChanClose );
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

Function:		Oct6100ApiGetAdpcmChanSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of the ADPCM memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetAdpcmChanSwSizes
UINT32 Oct6100ApiGetAdpcmChanSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;
	
	/* Determine the amount of memory required for the API ADPCM channel list.*/
	f_pInstSizes->ulAdpcmChannelList = f_pOpenChip->ulMaxAdpcmChannels * sizeof( tOCT6100_API_ADPCM_CHAN );

	if ( f_pOpenChip->ulMaxAdpcmChannels > 0 )
	{
		/* Calculate memory needed for ADPCM memory allocation */
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxAdpcmChannels, &f_pInstSizes->ulAdpcmChannelAlloc );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_48;
	}
	else
	{
		f_pInstSizes->ulAdpcmChannelAlloc = 0;
	}
	
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulAdpcmChannelList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulAdpcmChannelAlloc, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAdpcmChanSwInit

Description:    Initializes all elements of the instance structure associated
				to the ADPCM memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAdpcmChanSwInit
UINT32 Oct6100ApiAdpcmChanSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_ADPCM_CHAN	pChannelsTsiList;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32	ulMaxAdpcmChannels;
	PVOID	pAdpcmChannelsAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Initialize the ADPCM channel API list.*/
	ulMaxAdpcmChannels = pSharedInfo->ChipConfig.usMaxAdpcmChannels;

	/* Set all entries in the ADPCM channel list to unused. */
	mOCT6100_GET_ADPCM_CHAN_LIST_PNT( pSharedInfo, pChannelsTsiList )

	/* Clear the memory */
	Oct6100UserMemSet( pChannelsTsiList, 0x00, sizeof(tOCT6100_API_ADPCM_CHAN) * ulMaxAdpcmChannels );

	/* Initialize the ADPCM channel allocation structures to "all free". */
	if ( ulMaxAdpcmChannels > 0 )
	{
		mOCT6100_GET_ADPCM_CHAN_ALLOC_PNT( pSharedInfo, pAdpcmChannelsAlloc )
		
		ulResult = OctapiLlmAllocInit( &pAdpcmChannelsAlloc, ulMaxAdpcmChannels );
		if ( ulResult != cOCT6100_ERR_OK  )
			return cOCT6100_ERR_FATAL_BD;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100AdpcmChanOpenSer

Description:    Opens an ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pAdpcmChanOpen		Pointer to an ADPCM channel open structure

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100AdpcmChanOpenSer
UINT32 Oct6100AdpcmChanOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen )
{
	UINT16	usAdpcmChanIndex;
	UINT16	usTsiMemIndex;
	UINT16	usAdpcmMemIndex;
	UINT16	usInputTsstIndex;
	UINT16	usOutputTsstIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the ADPCM channel open structure for errors. */
	ulResult = Oct6100ApiCheckAdpcmChanParams( f_pApiInstance, f_pAdpcmChanOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the ADPCM channel. */
	ulResult = Oct6100ApiReserveAdpcmChanResources( f_pApiInstance, f_pAdpcmChanOpen, &usAdpcmChanIndex, &usAdpcmMemIndex, &usTsiMemIndex, &usInputTsstIndex, &usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write all necessary structures to activate the ADPCM channel. */
	ulResult = Oct6100ApiWriteAdpcmChanStructs( f_pApiInstance, f_pAdpcmChanOpen, usAdpcmMemIndex, usTsiMemIndex, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the ADPCM channel entry in the API list. */
	ulResult = Oct6100ApiUpdateAdpcmChanEntry( f_pApiInstance, f_pAdpcmChanOpen, usAdpcmChanIndex, usAdpcmMemIndex, usTsiMemIndex, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckAdpcmChanParams

Description:    Checks the user's ADPCM channel open configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pAdpcmChanOpen		Pointer to ADPCM channel open configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckAdpcmChanParams
UINT32 Oct6100ApiCheckAdpcmChanParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_OPEN			f_pAdpcmChanOpen )
{
	UINT32	ulResult;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxAdpcmChannels == 0 )
		return cOCT6100_ERR_ADPCM_CHAN_DISABLED;

	if ( f_pAdpcmChanOpen->pulChanHndl == NULL )
		return cOCT6100_ERR_ADPCM_CHAN_INVALID_HANDLE;

	/* Check the input TDM streams, timeslots component for errors. */
	if ( f_pAdpcmChanOpen->ulInputNumTssts != 1 &&
		 f_pAdpcmChanOpen->ulInputNumTssts != 2 )
		return cOCT6100_ERR_ADPCM_CHAN_INPUT_NUM_TSSTS;

	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   f_pAdpcmChanOpen->ulInputNumTssts,
									   f_pAdpcmChanOpen->ulInputTimeslot, 
									   f_pAdpcmChanOpen->ulInputStream,
									   cOCT6100_INPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_ADPCM_CHAN_INPUT_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_ADPCM_CHAN_INPUT_STREAM;
		}
		else
		{
			return ulResult;
		}
	}

	if( f_pAdpcmChanOpen->ulInputPcmLaw != cOCT6100_PCM_U_LAW && 
		f_pAdpcmChanOpen->ulInputPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_ADPCM_CHAN_INPUT_PCM_LAW;

	/* Check the output TDM streams, timeslots component for errors. */
	if ( f_pAdpcmChanOpen->ulOutputNumTssts != 1 &&
		 f_pAdpcmChanOpen->ulOutputNumTssts != 2 )
		return cOCT6100_ERR_ADPCM_CHAN_OUTPUT_NUM_TSSTS;

	ulResult = Oct6100ApiValidateTsst( f_pApiInstance, 
									   f_pAdpcmChanOpen->ulOutputNumTssts, 
									   f_pAdpcmChanOpen->ulOutputTimeslot, 
									   f_pAdpcmChanOpen->ulOutputStream,
									   cOCT6100_OUTPUT_TSST );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == cOCT6100_ERR_TSST_TIMESLOT )
		{
			return cOCT6100_ERR_ADPCM_CHAN_OUTPUT_TIMESLOT;
		}
		else if ( ulResult == cOCT6100_ERR_TSST_STREAM )
		{
			return cOCT6100_ERR_ADPCM_CHAN_OUTPUT_STREAM;
		}
		else
		{
			return ulResult;
		}
	}
	if( f_pAdpcmChanOpen->ulOutputPcmLaw != cOCT6100_PCM_U_LAW && 
		f_pAdpcmChanOpen->ulOutputPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_ADPCM_CHAN_OUTPUT_PCM_LAW;

	/* Now, check the channel mode. */
	if ( f_pAdpcmChanOpen->ulChanMode != cOCT6100_ADPCM_ENCODING && 
		 f_pAdpcmChanOpen->ulChanMode != cOCT6100_ADPCM_DECODING )
		return cOCT6100_ERR_ADPCM_CHAN_MODE;

	if ( f_pAdpcmChanOpen->ulChanMode == cOCT6100_ADPCM_ENCODING )
	{
		/* Check the encoding rate. */
		if ( ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G711_64KBPS ) && 
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G726_40KBPS ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G726_32KBPS ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G726_24KBPS ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G726_16KBPS ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_40KBPS_4_1 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_40KBPS_3_2 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_40KBPS_2_3 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_32KBPS_4_0 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_32KBPS_3_1 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_32KBPS_2_2 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_24KBPS_3_0 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_24KBPS_2_1 ) &&
			 ( f_pAdpcmChanOpen->ulEncodingRate  != cOCT6100_G727_16KBPS_2_0 ) )
			return cOCT6100_ERR_ADPCM_CHAN_ENCODING_RATE;
	}
	else /* if ( f_pAdpcmChanOpen->ulChanMode != cOCT6100_ADPCM_DECODING ) */
	{
		/* Check the decoding rate. */
		if ( f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G711_64KBPS &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G726_40KBPS &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G726_32KBPS &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G726_24KBPS &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G726_16KBPS &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G726_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G711_G726_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G727_2C_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G727_3C_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G727_4C_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G711_G727_2C_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G711_G727_3C_ENCODED &&
			 f_pAdpcmChanOpen->ulDecodingRate  != cOCT6100_G711_G727_4C_ENCODED )
			return cOCT6100_ERR_ADPCM_CHAN_DECODING_RATE;

		/* Make sure that two timeslots are allocated if PCM-ECHO encoded is selected. */
		if ( (f_pAdpcmChanOpen->ulDecodingRate == cOCT6100_G711_G726_ENCODED ||
			  f_pAdpcmChanOpen->ulDecodingRate == cOCT6100_G711_G727_2C_ENCODED ||
			  f_pAdpcmChanOpen->ulDecodingRate == cOCT6100_G711_G727_3C_ENCODED ||
			  f_pAdpcmChanOpen->ulDecodingRate == cOCT6100_G711_G727_4C_ENCODED ) &&
			  f_pAdpcmChanOpen->ulInputNumTssts != 2 )
			return cOCT6100_ERR_ADPCM_CHAN_INCOMPATIBLE_NUM_TSSTS;
	}
		
	/* Check the nibble position. */
	if ( f_pAdpcmChanOpen->ulAdpcmNibblePosition != cOCT6100_ADPCM_IN_LOW_BITS && 
		 f_pAdpcmChanOpen->ulAdpcmNibblePosition != cOCT6100_ADPCM_IN_HIGH_BITS )
		return cOCT6100_ERR_ADPCM_CHAN_ADPCM_NIBBLE_POSITION;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveAdpcmChanResources

Description:    Reserves all resources needed for the new ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pAdpcmChanOpen		Pointer to ADPCM channel configuration structure.
f_pusAdpcmChanIndex		Allocated entry in ADPCM channel list.
f_pusAdpcmMemIndex		Allocated entry in the ADPCM control memory.
f_pusTsiMemIndex		Allocated entry in the TSI chariot memory.
f_pusInputTsstIndex		TSST memory index of the input samples.
f_pusOutputTsstIndex	TSST memory index of the output samples.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveAdpcmChanResources
UINT32 Oct6100ApiReserveAdpcmChanResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				OUT		PUINT16							f_pusAdpcmChanIndex,
				OUT		PUINT16							f_pusAdpcmMemIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulResult;
	UINT32	ulTempVar;
	BOOL	fAdpcmChanEntry = FALSE;
	BOOL	fAdpcmMemEntry = FALSE;
	BOOL	fTsiMemEntry = FALSE;
	BOOL	fInputTsst = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Reserve an entry in the ADPCM channel list. */
	ulResult = Oct6100ApiReserveAdpcmChanEntry( f_pApiInstance, f_pusAdpcmChanIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		fAdpcmChanEntry = TRUE;
		
		/* Find a TSI memory entry.*/
		ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, f_pusTsiMemIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			fTsiMemEntry = TRUE;
		
			/* Find a conversion memory entry. */
			ulResult = Oct6100ApiReserveConversionMemEntry( f_pApiInstance, f_pusAdpcmMemIndex );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				fAdpcmMemEntry = TRUE;

				/* Reserve the input TSST entry. */	
				ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
												  f_pAdpcmChanOpen->ulInputTimeslot, 
												  f_pAdpcmChanOpen->ulInputStream, 
												  f_pAdpcmChanOpen->ulInputNumTssts, 
  												  cOCT6100_INPUT_TSST,
												  f_pusInputTsstIndex, 
												  NULL );
				if ( ulResult == cOCT6100_ERR_OK )
				{
					fInputTsst = TRUE;

					/* Reserve the output TSST entry. */
					ulResult = Oct6100ApiReserveTsst( f_pApiInstance, 
													  f_pAdpcmChanOpen->ulOutputTimeslot, 
													  f_pAdpcmChanOpen->ulOutputStream, 
													  f_pAdpcmChanOpen->ulOutputNumTssts, 
  													  cOCT6100_OUTPUT_TSST,
													  f_pusOutputTsstIndex, 
													  NULL );
				}
			}
		}
		else
		{
			/* Return an error other than a fatal error. */
			ulResult = cOCT6100_ERR_ADPCM_CHAN_NO_MORE_TSI_AVAILABLE;
		}
	}

	if ( ulResult != cOCT6100_ERR_OK )
	{
		if( fAdpcmChanEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseAdpcmChanEntry( f_pApiInstance, *f_pusAdpcmChanIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fTsiMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, *f_pusTsiMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fAdpcmMemEntry == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, *f_pusAdpcmMemIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;
		}

		if( fInputTsst == TRUE )
		{
			ulTempVar = Oct6100ApiReleaseTsst( f_pApiInstance, 
											   f_pAdpcmChanOpen->ulInputTimeslot, 
											   f_pAdpcmChanOpen->ulInputStream, 
											   f_pAdpcmChanOpen->ulInputNumTssts,
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

Function:		Oct6100ApiWriteAdpcmChanStructs

Description:    Performs all the required structure writes to configure the
				new ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pAdpcmChanOpen		Pointer to ADPCM channel configuration structure.
f_pusAdpcmChanIndex		Allocated entry in ADPCM channel list.
f_pusAdpcmMemIndex		Allocated entry in the ADPCM control memory.
f_pusTsiMemIndex		Allocated entry in the TSI chariot memory.
f_pusInputTsstIndex		TSST memory index of the input samples.
f_pusOutputTsstIndex	TSST memory index of the output samples.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteAdpcmChanStructs
UINT32 Oct6100ApiWriteAdpcmChanStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				IN		UINT16							f_usAdpcmMemIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;
	UINT32	ulCompType = 0;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/*------------------------------------------------------------------------------*/
	/* Configure the TSST control memory. */
	
	/* Set the input TSST control entry. */
	ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
													  f_usInputTsstIndex,
													  f_usTsiMemIndex,
													  f_pAdpcmChanOpen->ulInputPcmLaw );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Set the output TSST control entry. */
	ulResult = Oct6100ApiWriteOutputTsstControlMemory( f_pApiInstance,
													   f_usOutputTsstIndex,
													   f_pAdpcmChanOpen->ulAdpcmNibblePosition,
													   f_pAdpcmChanOpen->ulOutputNumTssts,
													   f_usTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*------------------------------------------------------------------------------*/


	/*------------------------------------------------------------------------------*/
	/* Configure the ADPCM memory. */

	if ( f_pAdpcmChanOpen->ulChanMode == cOCT6100_ADPCM_ENCODING )
	{
		switch( f_pAdpcmChanOpen->ulEncodingRate )
		{
		case cOCT6100_G711_64KBPS:
			
			if ( f_pAdpcmChanOpen->ulOutputPcmLaw == cOCT6100_PCM_U_LAW )
					ulCompType = 0x4;
				else /* if ( f_pAdpcmChanOpen->ulOutputPcmLaw != cOCT6100_PCM_U_LAW ) */
					ulCompType = 0x5;
			break;
		case cOCT6100_G726_40KBPS:				ulCompType = 0x3;		break;
		case cOCT6100_G726_32KBPS:				ulCompType = 0x2;		break;
		case cOCT6100_G726_24KBPS:				ulCompType = 0x1;		break;
		case cOCT6100_G726_16KBPS:				ulCompType = 0x0;		break;		
		case cOCT6100_G727_40KBPS_4_1:			ulCompType = 0xD;		break;
		case cOCT6100_G727_40KBPS_3_2:			ulCompType = 0xA;		break;
		case cOCT6100_G727_40KBPS_2_3:			ulCompType = 0x6;		break;
		case cOCT6100_G727_32KBPS_4_0:			ulCompType = 0xE;		break;
		case cOCT6100_G727_32KBPS_3_1:			ulCompType = 0xB;		break;
		case cOCT6100_G727_32KBPS_2_2:			ulCompType = 0x7;		break;
		case cOCT6100_G727_24KBPS_3_0:			ulCompType = 0xC;		break;
		case cOCT6100_G727_24KBPS_2_1:			ulCompType = 0x8;		break;
		case cOCT6100_G727_16KBPS_2_0:			ulCompType = 0x9;		break;
		}

		ulResult = Oct6100ApiWriteEncoderMemory(		f_pApiInstance,
														f_usAdpcmMemIndex,
														ulCompType,
														f_usTsiMemIndex,
														FALSE,
														f_pAdpcmChanOpen->ulAdpcmNibblePosition,
														cOCT6100_INVALID_INDEX,
														cOCT6100_INVALID_VALUE,
														cOCT6100_INVALID_VALUE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	else /* if ( f_pAdpcmChanOpen->ulChanMode != cOCT6100_ADPCM_DECODING ) */
	{
		switch( f_pAdpcmChanOpen->ulDecodingRate )
		{
		case cOCT6100_G711_64KBPS:				ulCompType = 0x8;		break;
		case cOCT6100_G726_40KBPS:				ulCompType = 0x3;		break;
		case cOCT6100_G726_32KBPS:				ulCompType = 0x2;		break;
		case cOCT6100_G726_24KBPS:				ulCompType = 0x1;		break;
		case cOCT6100_G726_16KBPS:				ulCompType = 0x0;		break;		
		case cOCT6100_G727_2C_ENCODED:			ulCompType = 0x4;		break;
		case cOCT6100_G727_3C_ENCODED:			ulCompType = 0x5;		break;
		case cOCT6100_G727_4C_ENCODED:			ulCompType = 0x6;		break;
		case cOCT6100_G726_ENCODED:				ulCompType = 0x9;		break;
		case cOCT6100_G711_G726_ENCODED:		ulCompType = 0xA;		break;
		case cOCT6100_G711_G727_2C_ENCODED:		ulCompType = 0xC;		break;
		case cOCT6100_G711_G727_3C_ENCODED:		ulCompType = 0xD;		break;
		case cOCT6100_G711_G727_4C_ENCODED:		ulCompType = 0xE;		break;
		}

		ulResult = Oct6100ApiWriteDecoderMemory(		f_pApiInstance,
														f_usAdpcmMemIndex,
														ulCompType,
														f_usTsiMemIndex,
														f_pAdpcmChanOpen->ulOutputPcmLaw,
														f_pAdpcmChanOpen->ulAdpcmNibblePosition );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateAdpcmChanEntry

Description:    Updates the new ADPCM channel in the ADPCM channel list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pAdpcmChanOpen		Pointer to ADPCM channel open configuration structure.
f_usAdpcmChanIndex		Allocated entry in the ADPCM channel list.
f_usAdpcmMemIndex		Allocated entry in ADPCM memory.
f_usTsiMemIndex			Allocated entry in TSI chariot memory.
f_usInputTsstIndex		TSST control memory index of the input TSST.
f_usOutputTsstIndex		TSST control memory index of the output TSST.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateAdpcmChanEntry
UINT32 Oct6100ApiUpdateAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				IN		UINT16							f_usAdpcmChanIndex,
				IN		UINT16							f_usAdpcmMemIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex )
{
	tPOCT6100_API_ADPCM_CHAN	pAdpcmChanEntry;

	/*------------------------------------------------------------------------------*/
	/* Obtain a pointer to the new ADPCM channel's list entry. */

	mOCT6100_GET_ADPCM_CHAN_ENTRY_PNT( f_pApiInstance->pSharedInfo, pAdpcmChanEntry, f_usAdpcmChanIndex )

	/* Copy the buffer's configuration and allocated resources. */
	pAdpcmChanEntry->usInputTimeslot	= (UINT16)( f_pAdpcmChanOpen->ulInputTimeslot & 0xFFFF );
	pAdpcmChanEntry->usInputStream		= (UINT16)( f_pAdpcmChanOpen->ulInputStream & 0xFFFF );
	pAdpcmChanEntry->byInputNumTssts	= (UINT8)( f_pAdpcmChanOpen->ulInputNumTssts & 0xFF );
	pAdpcmChanEntry->byInputPcmLaw		= (UINT8)( f_pAdpcmChanOpen->ulInputPcmLaw & 0xFF );

	pAdpcmChanEntry->usOutputTimeslot	= (UINT16)( f_pAdpcmChanOpen->ulOutputTimeslot & 0xFFFF );
	pAdpcmChanEntry->usOutputStream		= (UINT16)( f_pAdpcmChanOpen->ulOutputStream & 0xFFFF );
	pAdpcmChanEntry->byOutputNumTssts	= (UINT8)( f_pAdpcmChanOpen->ulOutputNumTssts & 0xFF );
	pAdpcmChanEntry->byOutputPcmLaw		= (UINT8)( f_pAdpcmChanOpen->ulOutputPcmLaw & 0xFF );

	/* Store hardware related information. */
	pAdpcmChanEntry->usTsiMemIndex		= f_usTsiMemIndex;
	pAdpcmChanEntry->usAdpcmMemIndex	= f_usAdpcmMemIndex;
	pAdpcmChanEntry->usInputTsstIndex	= f_usInputTsstIndex;
	pAdpcmChanEntry->usOutputTsstIndex	= f_usOutputTsstIndex;

	/* Form handle returned to user. */
	*f_pAdpcmChanOpen->pulChanHndl = cOCT6100_HNDL_TAG_ADPCM_CHANNEL | (pAdpcmChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usAdpcmChanIndex;

	/* Finally, mark the ADPCM channel as opened. */
	pAdpcmChanEntry->fReserved = TRUE;
	
	/* Increment the number of ADPCM channel opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberAdpcmChans++;

	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100AdpcmChanCloseSer

Description:    Closes an ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pAdpcmChanClose	Pointer to ADPCM channel close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100AdpcmChanCloseSer
UINT32 Oct6100AdpcmChanCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_CLOSE			f_pAdpcmChanClose )
{
	UINT16	usAdpcmChanIndex;
	UINT16	usTsiMemIndex;
	UINT16	usAdpcmMemIndex;
	UINT16	usInputTsstIndex;
	UINT16	usOutputTsstIndex;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertAdpcmChanParams( f_pApiInstance, f_pAdpcmChanClose, &usAdpcmChanIndex, &usAdpcmMemIndex, &usTsiMemIndex, &usInputTsstIndex, &usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the ADPCM channel. */
	ulResult = Oct6100ApiInvalidateAdpcmChanStructs( f_pApiInstance, usAdpcmMemIndex, usInputTsstIndex, usOutputTsstIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the ADPCM channel. */
	ulResult = Oct6100ApiReleaseAdpcmChanResources( f_pApiInstance, usAdpcmChanIndex, usAdpcmMemIndex, usTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Invalidate the handle. */
	f_pAdpcmChanClose->ulChanHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertAdpcmChanParams

Description:    Validate the handle given by the user and verify the state of 
				the ADPCM channel about to be closed. 
				Also return all required information to deactivate the channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_pAdpcmChanClose		Pointer to ADPCM channel close structure.
f_pusAdpcmChanIndex		Index of the ADPCM channel structure in the API list.
f_pusAdpcmMemIndex		Index of the ADPCM memory structure in the API list.
f_pusTsiMemIndex		Index of the TSI chariot memory used for this channel.
f_pusInputTsstIndex		Index of the input entry in the TSST control memory.
f_pusOutputTsstIndex	Index of the output entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertAdpcmChanParams
UINT32 Oct6100ApiAssertAdpcmChanParams( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_CLOSE			f_pAdpcmChanClose,
				OUT		PUINT16								f_pusAdpcmChanIndex,
				OUT		PUINT16								f_pusAdpcmMemIndex,
				OUT		PUINT16								f_pusTsiMemIndex,
				OUT		PUINT16								f_pusInputTsstIndex,
				OUT		PUINT16								f_pusOutputTsstIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_ADPCM_CHAN	pAdpcmChanEntry;
	UINT32						ulEntryOpenCnt;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check the provided handle. */
	if ( (f_pAdpcmChanClose->ulChanHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_ADPCM_CHANNEL )
		return cOCT6100_ERR_ADPCM_CHAN_INVALID_HANDLE;

	*f_pusAdpcmChanIndex = (UINT16)( f_pAdpcmChanClose->ulChanHndl & cOCT6100_HNDL_INDEX_MASK );
	
	if ( *f_pusAdpcmChanIndex >= pSharedInfo->ChipConfig.usMaxAdpcmChannels )
		return cOCT6100_ERR_ADPCM_CHAN_INVALID_HANDLE;

	/*------------------------------------------------------------------------------*/
	/* Get a pointer to the channel's list entry. */

	mOCT6100_GET_ADPCM_CHAN_ENTRY_PNT( pSharedInfo, pAdpcmChanEntry, *f_pusAdpcmChanIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pAdpcmChanClose->ulChanHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pAdpcmChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_ADPCM_CHAN_NOT_OPEN;
	if ( ulEntryOpenCnt != pAdpcmChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_ADPCM_CHAN_INVALID_HANDLE;

	/* Return info needed to close the channel and release all resources. */
	*f_pusInputTsstIndex	= pAdpcmChanEntry->usInputTsstIndex;
	*f_pusOutputTsstIndex	= pAdpcmChanEntry->usOutputTsstIndex;
	*f_pusTsiMemIndex		= pAdpcmChanEntry->usTsiMemIndex;
	*f_pusAdpcmMemIndex		= pAdpcmChanEntry->usAdpcmMemIndex;
	
	/*------------------------------------------------------------------------------*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateAdpcmChanStructs

Description:    Closes an ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usAdpcmMemIndex		Index of the ADPCM memory.
f_usInputTsstIndex		Index of the input entry in the TSST control memory.
f_usOutputTsstIndex		Index of the output entry in the TSST control memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateAdpcmChanStructs
UINT32 Oct6100ApiInvalidateAdpcmChanStructs( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usAdpcmMemIndex,
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

	/*------------------------------------------------------------------------------*/
	/* Deactivate the TSST control memory. */
	
	/* Set the input TSST control entry to unused. */
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usInputTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the output TSST control entry to unused. */
	WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( f_usOutputTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
	
	WriteParams.usWriteData  = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*------------------------------------------------------------------------------*/


	/*------------------------------------------------------------------------------*/
	/* Clear the ADPCM memory. */
	
	ulResult = Oct6100ApiClearConversionMemory( f_pApiInstance, f_usAdpcmMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseAdpcmChanResources

Description:	Release and clear the API entry associated to the ADPCM channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_ulAdpcmChanIndex		Index of the ADPCM channel in the API list.
f_usAdpcmMemIndex		Index of the ADPCM memory used.
f_usTsiMemIndex			Index of the TSI memory used.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseAdpcmChanResources
UINT32 Oct6100ApiReleaseAdpcmChanResources( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		UINT16								f_usAdpcmChanIndex,
				IN		UINT16								f_usAdpcmMemIndex,
				IN		UINT16								f_usTsiMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_ADPCM_CHAN	pAdpcmChanEntry;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_ADPCM_CHAN_ENTRY_PNT( pSharedInfo, pAdpcmChanEntry, f_usAdpcmChanIndex );

	/*------------------------------------------------------------------------------*/
	/* Release all resources associated with ADPCM channel. */

	/* Release the entry in the ADPCM channel list. */
	ulResult = Oct6100ApiReleaseAdpcmChanEntry( f_pApiInstance, f_usAdpcmChanIndex );
	if ( ulResult == cOCT6100_ERR_OK )
	{
		ulResult = Oct6100ApiReleaseConversionMemEntry( f_pApiInstance, f_usAdpcmMemIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, f_usTsiMemIndex );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				/* Release the input TSST entry. */
				ulResult = Oct6100ApiReleaseTsst( 
													f_pApiInstance, 
													pAdpcmChanEntry->usInputTimeslot,
													pAdpcmChanEntry->usInputStream,
 													pAdpcmChanEntry->byInputNumTssts,
													cOCT6100_INPUT_TSST,
													cOCT6100_INVALID_INDEX );
				if ( ulResult == cOCT6100_ERR_OK )
				{
					/* Release the output TSST entry. */
					ulResult = Oct6100ApiReleaseTsst( 
													f_pApiInstance, 
													pAdpcmChanEntry->usOutputTimeslot,
													pAdpcmChanEntry->usOutputStream,
 													pAdpcmChanEntry->byOutputNumTssts,
													cOCT6100_OUTPUT_TSST,
													cOCT6100_INVALID_INDEX );
				}
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

	/*------------------------------------------------------------------------------*/


	/*------------------------------------------------------------------------------*/
	/* Update the ADPCM channel's list entry. */

	/* Mark the channel as closed. */
	pAdpcmChanEntry->fReserved = FALSE;
	pAdpcmChanEntry->byEntryOpenCnt++;

	/* Decrement the number of ADPCM channels opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberAdpcmChans--;

	/*------------------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveAdpcmChanEntry

Description:    Reserves one of the ADPCM channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusAdpcmChanIndex		Resulting index reserved in the ADPCM channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveAdpcmChanEntry
UINT32 Oct6100ApiReserveAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT		PUINT16						f_pusAdpcmChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pAdpcmChanAlloc;
	UINT32	ulResult;
	UINT32	ulAdpcmChanIndex;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_ADPCM_CHAN_ALLOC_PNT( pSharedInfo, pAdpcmChanAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pAdpcmChanAlloc, &ulAdpcmChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_ADPCM_CHAN_ALL_ADPCM_CHAN_ARE_OPENED;
		else
			return cOCT6100_ERR_FATAL_BE;
	}

	*f_pusAdpcmChanIndex = (UINT16)( ulAdpcmChanIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseAdpcmChanEntry

Description:    Releases the specified ADPCM channel API entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usAdpcmChanIndex		Index reserved in the ADPCM channel list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseAdpcmChanEntry
UINT32 Oct6100ApiReleaseAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT16						f_usAdpcmChanIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pAdpcmChanAlloc;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_ADPCM_CHAN_ALLOC_PNT( pSharedInfo, pAdpcmChanAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pAdpcmChanAlloc, f_usAdpcmChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_BF;
	}

	return cOCT6100_ERR_OK;
}
#endif
