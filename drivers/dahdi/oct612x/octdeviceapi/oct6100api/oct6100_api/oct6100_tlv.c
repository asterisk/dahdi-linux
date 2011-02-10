/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_tlv.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions used to read information allowing the 
	API to know where all the features supported by this API version are 
	located in the chip's external memory.

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

$Octasic_Revision: 113 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"

#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_inst.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_tlv_priv.h"

/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiProcessTlvRegion

Description:    This function will read and interpret the TLV memory of the	chip
				to obtain memory offsets and features available of the image
				loaded into the chip.

				The API will read this region until it finds a TLV type of 0 with 
				a length of 0.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiProcessTlvRegion
UINT32 Oct6100ApiProcessTlvRegion(
				tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tOCT6100_READ_PARAMS	ReadParams;
	UINT16	usReadData;
	UINT32	ulResult;

	UINT32	ulTlvTypeField;
	UINT32	ulTlvLengthField;
	UINT32  ulTlvWritingTimeoutCount = 0;
	UINT32	ulConditionFlag = TRUE;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Set the address of the first TLV type. */
	ReadParams.ulReadAddress  = cOCT6100_TLV_BASE;
	ReadParams.ulReadAddress += 2;

	/* Wait for the TLV configuration to be configured in memory. */
	while ( ulConditionFlag )
	{
		/* Read the TLV write done flag. */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData & 0x1 ) 
			break;

		ulTlvWritingTimeoutCount++;
		if ( ulTlvWritingTimeoutCount == 0x100000 )
			return cOCT6100_ERR_TLV_TIMEOUT;
	}

	/*======================================================================*/
	/* Read the first 16 bits of the TLV type. */

	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Save data. */
	ulTlvTypeField = usReadData << 16;

	/* Read the last word of the TLV type. */
	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save data. */
	ulTlvTypeField |= usReadData;
		
	/*======================================================================*/
	

	/*======================================================================*/
	/* Now, read the TLV field length. */

	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Save data. */
	ulTlvLengthField = usReadData << 16;

	/* Read the last word of the TLV length. */
	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save data. */
	ulTlvLengthField |= usReadData;

	/* Modify the address to point at the TLV value field. */
	ReadParams.ulReadAddress += 2;
		
	/*======================================================================*/

	/* Read the TLV value until the end of TLV region is reached. */
	while( !((ulTlvTypeField == 0) && (ulTlvLengthField == 0)) )
	{
		ulResult = Oct6100ApiInterpretTlvEntry( f_pApiInstance, 
						   					    ulTlvTypeField, 
									            ulTlvLengthField,
												ReadParams.ulReadAddress );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Set the address to after the TLV value. */
		ReadParams.ulReadAddress += ulTlvLengthField;

		/*======================================================================*/
		/* Read the first 16 bits of the TLV type. */

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Save data. */
		ulTlvTypeField = usReadData << 16;

		/* Read the last word of the TLV type. */
		ReadParams.ulReadAddress += 2;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Save data. */
		ulTlvTypeField |= usReadData;
			
		/*======================================================================*/

		
		/*======================================================================*/
		/* Now, read the TLV field length. */

		ReadParams.ulReadAddress += 2;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Save data. */
		ulTlvLengthField = usReadData << 16;

		/* Read the last word of the TLV length. */
		ReadParams.ulReadAddress += 2;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Save data. */
		ulTlvLengthField |= usReadData;

		ReadParams.ulReadAddress += 2;

		/*======================================================================*/
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInterpretTlvEntry

Description:    This function will interpret a TLV entry from the chip.  All
				known TLV types by the API are exhaustively listed here.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulTlvFieldType		Type of the TLV field to interpret.
f_ulTlvFieldLength		Byte length of the TLV field.
f_ulTlvValueAddress		Address where the data of the TLV block starts.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInterpretTlvEntry
UINT32 Oct6100ApiInterpretTlvEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulTlvFieldType,
				IN		UINT32							f_ulTlvFieldLength,
				IN		UINT32							f_ulTlvValueAddress )
{
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT16	usReadData;
	UINT32	i;
	UINT32	ulTempValue = 0;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Find out how to interpret the TLV value according to the TLV type. */
	switch( f_ulTlvFieldType )
	{
	case cOCT6100_TLV_TYPE_VERSION_NUMBER:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_VERSION_NUMBER, 
												  cOCT6100_TLV_MAX_LENGTH_VERSION_NUMBER );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ReadParams.ulReadAddress = f_ulTlvValueAddress;

			for( i = 0; i < (f_ulTlvFieldLength/2); i++ )
			{
				/* Perform the actual read. */
				mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				f_pApiInstance->pSharedInfo->ImageInfo.szVersionNumber[ (i * 2) ] = (UINT8)((usReadData >> 8) & 0xFF);
				f_pApiInstance->pSharedInfo->ImageInfo.szVersionNumber[ (i * 2) + 1 ] = (UINT8)((usReadData >> 0) & 0xFF);

				/* Modify the address. */
				ReadParams.ulReadAddress += 2;
			}
		}
		break;

	case cOCT6100_TLV_TYPE_CUSTOMER_PROJECT_ID:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CUSTOMER_PROJECT_ID, 
												  cOCT6100_TLV_MAX_LENGTH_CUSTOMER_PROJECT_ID );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			/* Perform the actual read. */
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->ImageInfo.ulBuildId );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH0_MAIN_BASE_ADDRESS:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH0_MAIN_BASE_ADDRESS, 
												  cOCT6100_TLV_MAX_LENGTH_CH0_MAIN_BASE_ADDRESS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase &= 0x0FFFFFFF;			

			/* Modify the base address to incorporate the external memory offset. */
			f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemBase += cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainMemSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_IO_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_IO_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_IO_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoMemOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_ZCB_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_ZCB_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_ZCB_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainRinCBMemOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_ZCB_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_ZCB_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_ZCB_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainRinCBMemSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_XCB_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_XCB_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_XCB_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSinCBMemOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_XCB_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_XCB_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_XCB_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSinCBMemSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_YCB_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_YCB_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_YCB_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSoutCBMemOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_MAIN_YCB_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_MAIN_YCB_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_CH_MAIN_YCB_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSoutCBMemSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_FREE_MEM_BASE_ADDRESS:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_FREE_MEM_BASE_ADDRESS, 
												  cOCT6100_TLV_MAX_LENGTH_FREE_MEM_BASE_ADDRESS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulFreeMemBaseAddress );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			f_pApiInstance->pSharedInfo->MemoryMap.ulFreeMemBaseAddress &= 0x0FFFFFFF;

		}
		break;

	case cOCT6100_TLV_TYPE_CHAN_MAIN_IO_STATS_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CHAN_MAIN_IO_STATS_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CHAN_MAIN_IO_STATS_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoStatsOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CHAN_MAIN_IO_STATS_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CHAN_MAIN_IO_STATS_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CHAN_MAIN_IO_STATS_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainIoStatsSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_CH_ROOT_CONF_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CH_ROOT_CONF_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_CH_ROOT_CONF_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanRootConfOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	
	case cOCT6100_TLV_TYPE_POA_CH_MAIN_ZPO_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_CH_MAIN_ZPO_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_POA_CH_MAIN_ZPO_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainRinPlayoutMemOfst );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_POA_CH_MAIN_ZPO_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_CH_MAIN_ZPO_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_POA_CH_MAIN_ZPO_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainRinPlayoutMemSize );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		break;

	case cOCT6100_TLV_TYPE_POA_CH_MAIN_YPO_OFFSET:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_CH_MAIN_YPO_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_POA_CH_MAIN_YPO_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSoutPlayoutMemOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_CH_MAIN_YPO_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_CH_MAIN_YPO_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_POA_CH_MAIN_YPO_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											   f_ulTlvValueAddress, 
											   &f_pApiInstance->pSharedInfo->MemoryMap.ulChanMainSoutPlayoutMemSize );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_ZWP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_ZWP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_ZWP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_ZIS:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_ZIS, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_ZIS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_ZSP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_ZSP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_ZSP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_YWP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_YWP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_YWP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_YIS:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_YIS, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_YIS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RW_YSP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RW_YSP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RW_YSP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RO_ZRP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RO_ZRP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RO_ZRP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_POA_BOFF_RO_YRP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POA_BOFF_RO_YRP, 
												  cOCT6100_TLV_MAX_LENGTH_POA_BOFF_RO_YRP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_CNR_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CNR_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_CNR_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ConferencingNoiseReductionOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_ANR_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ANR_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_ANR_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AdaptiveNoiseReductionOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_HZ_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_HZ_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_HZ_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinDcOffsetRemovalOfst );
		}
		/* Set flag indicating that the feature is present.*/
		f_pApiInstance->pSharedInfo->ImageInfo.fRinDcOffsetRemoval = TRUE;
		break;

	case cOCT6100_TLV_TYPE_HX_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_HX_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_HX_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SinDcOffsetRemovalOfst );
		}
		/* Set flag indicating that the feature is present.*/
		f_pApiInstance->pSharedInfo->ImageInfo.fSinDcOffsetRemoval = TRUE;
		break;

	case cOCT6100_TLV_TYPE_LCA_Z_CONF_BOFF_RW_GAIN:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_LCA_Z_CONF_BOFF_RW_GAIN, 
												  cOCT6100_TLV_MAX_LENGTH_LCA_Z_CONF_BOFF_RW_GAIN );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinLevelControlOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_LCA_Y_CONF_BOFF_RW_GAIN:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_LCA_Y_CONF_BOFF_RW_GAIN, 
												  cOCT6100_TLV_MAX_LENGTH_LCA_Y_CONF_BOFF_RW_GAIN );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SoutLevelControlOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_CNA_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CNA_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_CNA_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ComfortNoiseModeOfst );
		}
		/* Set flag indicating that the feature is present.*/
		f_pApiInstance->pSharedInfo->ImageInfo.fComfortNoise = TRUE;
		break;

	case cOCT6100_TLV_TYPE_NOA_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_NOA_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_NOA_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.NlpControlFieldOfst );
		}
		/* Set flag indicating that the feature is present.*/
		f_pApiInstance->pSharedInfo->ImageInfo.fNlpControl = TRUE;
		break;

	case cOCT6100_TLV_TYPE_VFA_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_VFA_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_VFA_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.VadControlFieldOfst );
		}
		/* Set flag indicating that the feature is present.*/
		f_pApiInstance->pSharedInfo->ImageInfo.fSilenceSuppression = TRUE;
		break;

	case cOCT6100_TLV_TYPE_TLA_MAIN_IO_BOFF_RW_TAIL_DISP:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TLA_MAIN_IO_BOFF_RW_TAIL_DISP, 
												  cOCT6100_TLV_MAX_LENGTH_TLA_MAIN_IO_BOFF_RW_TAIL_DISP );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PouchTailDisplOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_BOOTA_POUCH_BOFF_RW_BOOT_INST:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_BOOTA_POUCH_BOFF_RW_BOOT_INST, 
												  cOCT6100_TLV_MAX_LENGTH_BOOTA_POUCH_BOFF_RW_BOOT_INST );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PouchBootInstructionOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_BOOTA_POUCH_BOFF_RW_BOOT_RESULT:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_BOOTA_POUCH_BOFF_RW_BOOT_RESULT, 
												  cOCT6100_TLV_MAX_LENGTH_BOOTA_POUCH_BOFF_RW_BOOT_RESULT );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PouchBootResultOfst );
		}
		break;

	case cOCT6100_TLV_TYPE_TDM_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TDM_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_TDM_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ToneDisablerControlOfst );
		}

		f_pApiInstance->pSharedInfo->ImageInfo.fToneDisabler = TRUE;
		break;
	
	case cOCT6100_TLV_TYPE_DIS_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DIS_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_DIS_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.TailDisplEnableOfst );
		}

		f_pApiInstance->pSharedInfo->ImageInfo.fTailDisplacement = TRUE;
		break;
	
	case cOCT6100_TLV_TYPE_NT_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_NT_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_NT_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.NlpTrivialFieldOfst );
		}

		break;

	case cOCT6100_TLV_TYPE_DEBUG_CHAN_INDEX_VALUE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DEBUG_CHAN_INDEX_VALUE, 
												  cOCT6100_TLV_MAX_LENGTH_DEBUG_CHAN_INDEX_VALUE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );
		}

		f_pApiInstance->pSharedInfo->DebugInfo.usRecordMemIndex = (UINT16)( ulTempValue & 0xFFFF );

		break;

	case cOCT6100_TLV_TYPE_ADPCM_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ADPCM_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_ADPCM_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );
		}

		if ( ulTempValue == 0 )
			f_pApiInstance->pSharedInfo->ImageInfo.fAdpcm = FALSE;
		else
			f_pApiInstance->pSharedInfo->ImageInfo.fAdpcm = TRUE;
		
		break;

	case cOCT6100_TLV_TYPE_CONFERENCING_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CONFERENCING_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_CONFERENCING_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );
		}

		if ( ulTempValue == 0 )
			f_pApiInstance->pSharedInfo->ImageInfo.fConferencing = FALSE;
		else
			f_pApiInstance->pSharedInfo->ImageInfo.fConferencing = TRUE;
		
		break;

	case cOCT6100_TLV_TYPE_TONE_DETECTOR_PROFILE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TONE_DETECTOR_PROFILE, 
												  cOCT6100_TLV_MIN_LENGTH_TONE_DETECTOR_PROFILE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->ImageInfo.ulToneProfileNumber );
		}

		break;

	case cOCT6100_TLV_TYPE_MAX_TAIL_DISPLACEMENT:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MAX_TAIL_DISPLACEMENT, 
												  cOCT6100_TLV_MAX_LENGTH_MAX_TAIL_DISPLACEMENT );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			UINT32	ulTailDispTempValue;
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTailDispTempValue );

			ulTailDispTempValue += 1;		/* Convert the value into milliseconds.*/
			ulTailDispTempValue *= 16;		/* value was given in multiple of 16 ms. */

			if ( ulTailDispTempValue >= 128 )
				f_pApiInstance->pSharedInfo->ImageInfo.usMaxTailDisplacement = (UINT16)( ulTailDispTempValue - 128 );
			else
				f_pApiInstance->pSharedInfo->ImageInfo.usMaxTailDisplacement = 0;

		}	

		break;

	case cOCT6100_TLV_TYPE_AEC_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AEC_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_AEC_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AecFieldOfst );
		}
	
		/* Set the flag. */
		f_pApiInstance->pSharedInfo->ImageInfo.fAecEnabled = TRUE;

		/* Acoustic echo cancellation available! */
		f_pApiInstance->pSharedInfo->ImageInfo.fAcousticEcho = TRUE;

		break;

	case cOCT6100_TLV_TYPE_PCM_LEAK_CONF_BOFF_RW:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_PCM_LEAK_CONF_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_PCM_LEAK_CONF_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PcmLeakFieldOfst );
		}

		f_pApiInstance->pSharedInfo->ImageInfo.fNonLinearityBehaviorA = TRUE;
		break;

	case cOCT6100_TLV_TYPE_DEFAULT_ERL_CONF_BOFF_RW:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DEFAULT_ERL_CONF_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_DEFAULT_ERL_CONF_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.DefaultErlFieldOfst );
		}
	
		/* Set the flag. */
		f_pApiInstance->pSharedInfo->ImageInfo.fDefaultErl = TRUE;

		break;

	case cOCT6100_TLV_TYPE_TONE_REM_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TONE_REM_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_TONE_REM_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ToneRemovalFieldOfst );
		}
	
		/* Set the flag. */
		f_pApiInstance->pSharedInfo->ImageInfo.fToneRemoval = TRUE;

		break;



	case cOCT6100_TLV_TYPE_TLA_MAIN_IO_BOFF_RW_MAX_ECHO_POINT:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TLA_MAIN_IO_BOFF_RW_MAX_ECHO_POINT, 
												  cOCT6100_TLV_MAX_LENGTH_TLA_MAIN_IO_BOFF_RW_MAX_ECHO_POINT );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ChanMainIoMaxEchoPointOfst );
		}
	
		/* Set the flag. */
		f_pApiInstance->pSharedInfo->ImageInfo.fMaxEchoPoint = TRUE;

		break;

	case cOCT6100_TLV_TYPE_NLP_CONV_CAP_CONF_BOFF_RW:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_NLP_CONV_CAP_CONF_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_NLP_CONV_CAP_CONF_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.NlpConvCapFieldOfst );
		}
	
		/* Set the flag. */
		f_pApiInstance->pSharedInfo->ImageInfo.fNonLinearityBehaviorB = TRUE;

		break;

	case cOCT6100_TLV_TYPE_MATRIX_EVENT_SIZE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MATRIX_EVENT_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_MATRIX_EVENT_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulDebugEventSize );
		}

		break;

	case cOCT6100_TLV_TYPE_CNR_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CNR_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_CNR_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.fConferencingNoiseReduction = (UINT8)( ulTempValue & 0xFF );

			if ( f_pApiInstance->pSharedInfo->ImageInfo.fConferencingNoiseReduction == TRUE )
			{
				/* Set flag indicating that the dominant speaker feature is present. */
				f_pApiInstance->pSharedInfo->ImageInfo.fDominantSpeakerEnabled = TRUE;
			}
		}

		break;

	case cOCT6100_TLV_TYPE_MAX_TAIL_LENGTH_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MAX_TAIL_LENGTH_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_MAX_TAIL_LENGTH_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.usMaxTailLength = (UINT16)( ulTempValue & 0xFFFF );
		}

		break;
		
	case cOCT6100_TLV_TYPE_MAX_NUMBER_OF_CHANNELS:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MAX_NUMBER_OF_CHANNELS, 
												  cOCT6100_TLV_MAX_LENGTH_MAX_NUMBER_OF_CHANNELS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.usMaxNumberOfChannels = (UINT16)( ulTempValue & 0xFFFF );
		}

		break;

	case cOCT6100_TLV_TYPE_PLAYOUT_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_PLAYOUT_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_PLAYOUT_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			/* Set flag indicating that the feature is present. */
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );
			f_pApiInstance->pSharedInfo->ImageInfo.fBufferPlayout = (UINT8)( ulTempValue & 0xFF );
		}

		break;

	case cOCT6100_TLV_TYPE_DOMINANT_SPEAKER_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DOMINANT_SPEAKER_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_DOMINANT_SPEAKER_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.DominantSpeakerFieldOfst );
		}

		break;
		
	case cOCT6100_TLV_TYPE_TAIL_DISP_CONF_BOFF_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TAIL_DISP_CONF_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_TAIL_DISP_CONF_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PerChanTailDisplacementFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fPerChannelTailDisplacement = TRUE;
		}
		
		break;

	case cOCT6100_TLV_TYPE_ANR_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ANR_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_ANR_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.fAdaptiveNoiseReduction = (UINT8)( ulTempValue & 0xFF );
		}

		break;

	case cOCT6100_TLV_TYPE_MUSIC_PROTECTION_RW_ENABLE:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MUSIC_PROTECTION_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_MUSIC_PROTECTION_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.fMusicProtection = (UINT8)( ulTempValue & 0xFF );
		}

		break;

	case cOCT6100_TLV_TYPE_AEC_DEFAULT_ERL_BOFF:
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AEC_DEFAULT_ERL_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_AEC_DEFAULT_ERL_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AecDefaultErlFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAecDefaultErl = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Z_ALC_TARGET_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Z_ALC_TARGET_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_Z_ALC_TARGET_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinAutoLevelControlTargetOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinAutoLevelControl = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Y_ALC_TARGET_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Y_ALC_TARGET_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_Y_ALC_TARGET_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SoutAutoLevelControlTargetOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSoutAutoLevelControl = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Z_HLC_TARGET_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Z_HLC_TARGET_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_Z_HLC_TARGET_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinHighLevelCompensationThresholdOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinHighLevelCompensation = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Y_HLC_TARGET_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Y_HLC_TARGET_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_Y_HLC_TARGET_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SoutHighLevelCompensationThresholdOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSoutHighLevelCompensation = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_ALC_HLC_STATUS_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ALC_HLC_STATUS_BOFF_RW_ENABLE, 
												  cOCT6100_TLV_MAX_LENGTH_ALC_HLC_STATUS_BOFF_RW_ENABLE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AlcHlcStatusOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAlcHlcStatus = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Z_PLAYOUT_HARD_SKIP_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Z_PLAYOUT_HARD_SKIP_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_Z_PLAYOUT_HARD_SKIP_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_Y_PLAYOUT_HARD_SKIP_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_Y_PLAYOUT_HARD_SKIP_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_Y_PLAYOUT_HARD_SKIP_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip = TRUE;
		}

		break;
		
	case cOCT6100_TLV_TYPE_AFT_FIELD_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AFT_FIELD_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_AFT_FIELD_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AftControlOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAftControl = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_VOICE_DETECTED_STAT_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_VOICE_DETECTED_STAT_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_VOICE_DETECTED_STAT_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SinVoiceDetectedStatOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSinVoiceDetectedStat = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_GAIN_APPLIED_RIN_STAT_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_GAIN_APPLIED_RIN_STAT_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_GAIN_APPLIED_RIN_STAT_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinAppliedGainStatOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinAppliedGainStat = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_GAIN_APPLIED_SOUT_STAT_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_GAIN_APPLIED_SOUT_STAT_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_GAIN_APPLIED_SOUT_STAT_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SoutAppliedGainStatOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSoutAppliedGainStat = TRUE;
		}

		break;
		
	case cOCT6100_TLV_TYPE_MAX_ADAPT_ALE_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MAX_ADAPT_ALE_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_MAX_ADAPT_ALE_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AdaptiveAleOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fListenerEnhancement = TRUE;
		}

		break;
		
	case cOCT6100_TLV_TYPE_RIN_ANR_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_RIN_ANR_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_RIN_ANR_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinAnrOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRoutNoiseReduction = TRUE;
		}

		break;
	case cOCT6100_TLV_TYPE_RIN_ANR_VALUE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_RIN_ANR_VALUE_RW, 
												  cOCT6100_TLV_MAX_LENGTH_RIN_ANR_VALUE_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinAnrValOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRoutNoiseReductionLevel = TRUE;
		}

		break;
	case cOCT6100_TLV_TYPE_RIN_MUTE_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_RIN_MUTE_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_RIN_MUTE_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.RinMuteOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinMute = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_SIN_MUTE_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_SIN_MUTE_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_SIN_MUTE_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.SinMuteOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSinMute = TRUE;
		}

		break;
		
	case cOCT6100_TLV_TYPE_NUMBER_PLAYOUT_EVENTS:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_NUMBER_PLAYOUT_EVENTS, 
												  cOCT6100_TLV_MAX_LENGTH_NUMBER_PLAYOUT_EVENTS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents = (UINT8)( ulTempValue & 0xFF );
		}

		break;

	case cOCT6100_TLV_TYPE_ANR_SNR_IMPROVEMENT_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ANR_SNR_IMPROVEMENT_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_ANR_SNR_IMPROVEMENT_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AnrSnrEnhancementOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAnrSnrEnhancement = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_ANR_AGRESSIVITY_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ANR_AGRESSIVITY_BOFF_RW, 
												  cOCT6100_TLV_MAX_LENGTH_ANR_AGRESSIVITY_BOFF_RW );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AnrVoiceNoiseSegregationOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAnrVoiceNoiseSegregation = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_CHAN_TAIL_LENGTH_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CHAN_TAIL_LENGTH_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_CHAN_TAIL_LENGTH_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PerChanTailLengthFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fPerChannelTailLength = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_CHAN_VQE_TONE_DISABLING_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_CHAN_VQE_TONE_DIS_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_CHAN_VQE_TONE_DIS_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.ToneDisablerVqeActivationDelayOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fToneDisablerVqeActivationDelay = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_AF_TAIL_DISP_VALUE_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AF_TAIL_DISP_VALUE_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_AF_TAIL_DISP_VALUE_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AfTailDisplacementFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAfTailDisplacement = TRUE;
		}

		break;


	case cOCT6100_TLV_TYPE_POUCH_COUNTER_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_POUCH_COUNTER_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_POUCH_COUNTER_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.PouchCounterFieldOfst );

			f_pApiInstance->pSharedInfo->DebugInfo.fPouchCounter = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_AEC_TAIL_LENGTH_BOFF:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AEC_TAIL_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_AEC_TAIL_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
														 f_ulTlvValueAddress, 
														 &f_pApiInstance->pSharedInfo->MemoryMap.AecTailLengthFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fAecTailLength = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_MATRIX_DWORD_BASE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MATRIX_DWORD_BASE, 
												  cOCT6100_TLV_MAX_LENGTH_MATRIX_DWORD_BASE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulMatrixBaseAddress );

			/* Mask the upper bits set by the firmware. */
			f_pApiInstance->pSharedInfo->DebugInfo.ulMatrixBaseAddress &= 0x0FFFFFFF;			

			/* Modify the base address to incorporate the external memory offset. */
			f_pApiInstance->pSharedInfo->DebugInfo.ulMatrixBaseAddress += cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;
		}

		break;

	case cOCT6100_TLV_TYPE_DEBUG_CHAN_STATS_BYTE_SIZE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DEBUG_CHAN_STATS_BYTE_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_DEBUG_CHAN_STATS_BYTE_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulDebugChanStatsByteSize );
		}

		break;

	case cOCT6100_TLV_TYPE_DEBUG_CHAN_LITE_STATS_BYTE_SIZE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DEBUG_CHAN_LITE_STATS_BYTE_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_DEBUG_CHAN_LITE_STATS_BYTE_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize );
		}

		break;

	case cOCT6100_TLV_TYPE_HOT_CHANNEL_SELECT_DWORD_BASE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_HOT_CHANNEL_SELECT_DWORD_BASE, 
												  cOCT6100_TLV_MAX_LENGTH_HOT_CHANNEL_SELECT_DWORD_BASE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress );
		}

		break;

	case cOCT6100_TLV_TYPE_MATRIX_TIMESTAMP_DWORD_BASE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_TIMESTAMP_DWORD_BASE, 
												  cOCT6100_TLV_MAX_LENGTH_TIMESTAMP_DWORD_BASE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulMatrixTimestampBaseAddress );
		}

		break;

	case cOCT6100_TLV_TYPE_MATRIX_WP_DWORD_BASE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MATRIX_WP_DWORD_BASE, 
												  cOCT6100_TLV_MAX_LENGTH_MATRIX_WP_DWORD_BASE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulMatrixWpBaseAddress );
		}

		break;

	case cOCT6100_TLV_TYPE_AF_WRITE_PTR_BYTE_OFFSET:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AF_WRITE_PTR_BYTE_OFFSET, 
												  cOCT6100_TLV_MAX_LENGTH_AF_WRITE_PTR_BYTE_OFFSET );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulAfWritePtrByteOffset );
		}

		break;

	case cOCT6100_TLV_TYPE_RECORDED_PCM_EVENT_BYTE_SIZE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_RECORDED_PCM_EVENT_BYTE_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_RECORDED_PCM_EVENT_BYTE_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize );
		}

		break;

	case cOCT6100_TLV_TYPE_IS_ISR_CALLED_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_IS_ISR_CALLED_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_IS_ISR_CALLED_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.IsIsrCalledFieldOfst );

			f_pApiInstance->pSharedInfo->DebugInfo.fIsIsrCalledField = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_MUSIC_PROTECTION_ENABLE_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MUSIC_PROTECTION_ENABLE_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_MUSIC_PROTECTION_ENABLE_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.MusicProtectionFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fMusicProtectionConfiguration = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_IDLE_CODE_DETECTION_ENABLE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_IDLE_CODE_DETECTION, 
												  cOCT6100_TLV_MAX_LENGTH_IDLE_CODE_DETECTION );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.fIdleCodeDetection = (UINT8)( ulTempValue & 0xFF );
		}

		break;
		
	case cOCT6100_TLV_TYPE_IDLE_CODE_DETECTION_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_IDLE_CODE_DETECTION_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_IDLE_CODE_DETECTION_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.IdleCodeDetectionFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fIdleCodeDetectionConfiguration = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_IMAGE_TYPE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_IMAGE_TYPE, 
												  cOCT6100_TLV_MAX_LENGTH_IMAGE_TYPE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			/* Check if read image type value is what's expected. */
			if ( ( ulTempValue != cOCT6100_IMAGE_TYPE_WIRELINE )
				&& ( ulTempValue != cOCT6100_IMAGE_TYPE_COMBINED ) )
				return cOCT6100_ERR_FATAL_E9;

			f_pApiInstance->pSharedInfo->ImageInfo.byImageType = (UINT8)( ulTempValue & 0xFF );
		}

		break;

	case cOCT6100_TLV_TYPE_MAX_WIRELINE_CHANNELS:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_MAX_WIRELINE_CHANNELS, 
												  cOCT6100_TLV_MAX_LENGTH_MAX_WIRELINE_CHANNELS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );
		}

		break;
		
	case cOCT6100_TLV_TYPE_AF_EVENT_CB_SIZE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_AF_EVENT_CB_BYTE_SIZE, 
												  cOCT6100_TLV_MAX_LENGTH_AF_EVENT_CB_BYTE_SIZE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->DebugInfo.ulAfEventCbByteSize );
		}

		break;

	case cOCT6100_TLV_TYPE_BUFFER_PLAYOUT_SKIP_IN_EVENTS:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_BUFFER_PLAYOUT_SKIP_IN_EVENTS, 
												  cOCT6100_TLV_MAX_LENGTH_BUFFER_PLAYOUT_SKIP_IN_EVENTS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			f_pApiInstance->pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents = TRUE;
		}

		break;
		
	case cOCT6100_TLV_TYPE_ZZ_ENERGY_CHAN_STATS_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_ZZ_ENERGY_CHAN_STATS_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_ZZ_ENERGY_CHAN_STATS_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.RinEnergyStatFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fRinEnergyStat = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_YY_ENERGY_CHAN_STATS_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_YY_ENERGY_CHAN_STATS_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_YY_ENERGY_CHAN_STATS_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.SoutEnergyStatFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fSoutEnergyStat = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_DOUBLE_TALK_BEH_MODE:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DOUBLE_TALK_BEH_MODE, 
												  cOCT6100_TLV_MAX_LENGTH_DOUBLE_TALK_BEH_MODE );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			if ( ulTempValue != 0 )
				f_pApiInstance->pSharedInfo->ImageInfo.fDoubleTalkBehavior = TRUE;
			else
				f_pApiInstance->pSharedInfo->ImageInfo.fDoubleTalkBehavior = FALSE;

		}

		break;

	case cOCT6100_TLV_TYPE_DOUBLE_TALK_BEH_MODE_BOFF:
		
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_DOUBLE_TALK_BEH_MODE_BOFF, 
												  cOCT6100_TLV_MAX_LENGTH_DOUBLE_TALK_BEH_MODE_BOFF );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiTlvReadBitOffsetStruct( f_pApiInstance,
											f_ulTlvValueAddress, 
											&f_pApiInstance->pSharedInfo->MemoryMap.DoubleTalkBehaviorFieldOfst );

			f_pApiInstance->pSharedInfo->ImageInfo.fDoubleTalkBehaviorFieldOfst = TRUE;
		}

		break;

	case cOCT6100_TLV_TYPE_SOUT_NOISE_BLEACHING:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_SOUT_NOISE_BLEACHING, 
												  cOCT6100_TLV_MAX_LENGTH_SOUT_NOISE_BLEACHING );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			if ( ulTempValue != 0 )
				f_pApiInstance->pSharedInfo->ImageInfo.fSoutNoiseBleaching = TRUE;
			else
				f_pApiInstance->pSharedInfo->ImageInfo.fSoutNoiseBleaching = FALSE;

		}

		break;

	case cOCT6100_TLV_TYPE_NLP_STATISTICS:

		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength, 
												  cOCT6100_TLV_MIN_LENGTH_NLP_STATISTICS, 
												  cOCT6100_TLV_MAX_LENGTH_NLP_STATISTICS );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance,
											f_ulTlvValueAddress, 
											&ulTempValue );

			if ( ulTempValue != 0 )
				f_pApiInstance->pSharedInfo->ImageInfo.fSinLevel = TRUE;
			else
				f_pApiInstance->pSharedInfo->ImageInfo.fSinLevel = FALSE;

		}

		break;

	default:	
		/* Unknown TLV type field... check default length and nothing else. */
		ulResult = Oct6100ApiTlvCheckLengthField( f_ulTlvFieldLength,
												  cOCT6100_TLV_MIN_LENGTH_DEFAULT,
												  cOCT6100_TLV_MAX_LENGTH_DEFAULT );
		break;
	}
	
	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiTlvCheckLengthField

Description:    This function validates the TLV length field.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_ulTlvFieldLength		Length field read from the TLV.
f_ulMinLengthValue		Minimum value supported for the TLV.
f_ulMaxLengthValue		Maximum value supported for the TLV.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiTlvCheckLengthField
UINT32 Oct6100ApiTlvCheckLengthField( 
				IN		UINT32				f_ulTlvFieldLength,
				IN		UINT32				f_ulMinLengthValue,
				IN		UINT32				f_ulMaxLengthValue )
{
	/* Check if the value is too small. */
	if ( f_ulTlvFieldLength < f_ulMinLengthValue )
		return ( cOCT6100_ERR_FATAL_59 );

	/* Check if the value is too big. */
	if ( f_ulTlvFieldLength > f_ulMaxLengthValue )
		return ( cOCT6100_ERR_FATAL_5A );

	/* Check if the value is dword aligned. */
	if ( ( f_ulTlvFieldLength % 4 ) != 0 )
		return ( cOCT6100_ERR_OPEN_INVALID_TLV_LENGTH );
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiTlvReadBitOffsetStruct

Description:    This function extracts a bit offset structure from the TLV.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulAddress				Address where the read the TLV information.
f_pBitOffsetStruct		Pointer to a bit offset stucture.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiTlvReadBitOffsetStruct
UINT32 Oct6100ApiTlvReadBitOffsetStruct( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulAddress,
				OUT		tPOCT6100_TLV_OFFSET			f_pBitOffsetStruct )
{
	tOCT6100_READ_PARAMS	ReadParams;
	UINT16	usReadData;
	
	UINT32	ulResult;
	UINT32	ulOffsetValue;
	UINT32	ulSizeValue;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/*======================================================================*/
	/* Read the first 16 bits of the TLV field. */

	ReadParams.ulReadAddress = f_ulAddress;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Save data. */
	ulOffsetValue = usReadData << 16;

	/* Read the last word of the TLV type. */
	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save data. */
	ulOffsetValue |= usReadData;
		
	/*======================================================================*/
	

	/*======================================================================*/
	/* Read the first 16 bits of the TLV field. */

	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Save data. */
	ulSizeValue = usReadData << 16;

	/* Read the last word of the TLV type. */
	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save data. */
	ulSizeValue |= usReadData;

	/*======================================================================*/

	/* Set the structure fields. */
	f_pBitOffsetStruct->usDwordOffset = (UINT16)(ulOffsetValue / 32);
	f_pBitOffsetStruct->byBitOffset   = (UINT8) (32 - (ulOffsetValue % 32) - ulSizeValue);
	f_pBitOffsetStruct->byFieldSize   = (UINT8) (ulSizeValue);

	return cOCT6100_ERR_OK;
}
#endif
