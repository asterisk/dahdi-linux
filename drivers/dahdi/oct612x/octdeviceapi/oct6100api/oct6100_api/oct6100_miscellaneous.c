/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_miscellaneous.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains miscellaneous functions used in various files.

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

$Octasic_Revision: 35 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "apilib/octapi_largmath.h"

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

#include "oct6100_chip_open_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_miscellaneous_priv.h"


/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWaitForTime

Description:    Waits for the specified amount of time.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_aulWaitTime[ 2 ]	The amout of time to be waited.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWaitForTime
UINT32 Oct6100ApiWaitForTime(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_aulWaitTime[ 2 ] )
{
	tOCT6100_GET_TIME	StartTime;
	tOCT6100_GET_TIME	CurrentTime;
	UINT32				aulTimeDelta[ 2 ];
	UINT32				ulResult;
	UINT16				usTempVar;
	BOOL				fConditionFlag = TRUE;

	/* Copy the process context. */
	StartTime.pProcessContext	= f_pApiInstance->pProcessContext;
	CurrentTime.pProcessContext	= f_pApiInstance->pProcessContext;

	ulResult = Oct6100UserGetTime( &StartTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	while ( fConditionFlag )
	{
		ulResult = Oct6100UserGetTime( &CurrentTime );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = octapi_lm_subtract(
								CurrentTime.aulWallTimeUs, 1,
								StartTime.aulWallTimeUs, 1,
								aulTimeDelta, 1,
								&usTempVar );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_37;

		if ( aulTimeDelta[ 1 ] >= f_aulWaitTime[ 1 ] &&
			 aulTimeDelta[ 0 ] >= f_aulWaitTime[ 0 ] )
			fConditionFlag = FALSE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWaitForPcRegisterBit

Description:    Polls the specified PC register bit.  The function exits once
				the bit is cleared by hardware, or when the specified timeout
				period has been expired.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulPcRegAdd			Address of the register containing the PC bit.
f_ulPcBitNum			Number of the PC bit within the register.
f_ulValue				Expected value of the bit.
f_ulTimeoutUs			The timeout period, in usec.
f_pfBitEqual			Pointer to the result of the bit comparison.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWaitForPcRegisterBit
UINT32 Oct6100ApiWaitForPcRegisterBit(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulPcRegAdd,
				IN		UINT32					f_ulPcBitNum,
				IN		UINT32					f_ulValue,
				IN		UINT32					f_ulTimeoutUs,
				OUT		PBOOL					f_pfBitEqual )
{
	tOCT6100_READ_PARAMS	ReadParams;
	tOCT6100_GET_TIME		StartTime;
	tOCT6100_GET_TIME		TimeoutTime;
	tOCT6100_GET_TIME		CurrentTime;
	UINT32					ulResult;
	UINT16					usReadData;
	BOOL					fConditionFlag = TRUE;

	/* Copy the process context. */
	StartTime.pProcessContext	= f_pApiInstance->pProcessContext;
	CurrentTime.pProcessContext	= f_pApiInstance->pProcessContext;

	/* Get the current system time. */
	ulResult = Oct6100UserGetTime( &StartTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Mark the bit as not being equal, for now. */
	*f_pfBitEqual = FALSE;

	/* Determine the time at which the timeout has expired. */
	ulResult = octapi_lm_add(
						StartTime.aulWallTimeUs, 1,
						&f_ulTimeoutUs, 0,
						TimeoutTime.aulWallTimeUs, 1 );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Prepare read structure. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.ulReadAddress = f_ulPcRegAdd;
	ReadParams.pusReadData = &usReadData;

	/* Read the PC bit while the timeout period hasn't expired. */
	while ( fConditionFlag )
	{
		/* Read the current time again to check for timeout. */
		ulResult = Oct6100UserGetTime( &CurrentTime );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100UserDriverReadApi( &ReadParams );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( ( UINT16 )((usReadData >> f_ulPcBitNum) & 0x1) == ( UINT16 )f_ulValue )
		{
			/* Mark the bit as being equal. */
			*f_pfBitEqual = TRUE;
			fConditionFlag = FALSE;
		}

		if ( CurrentTime.aulWallTimeUs[ 1 ] > TimeoutTime.aulWallTimeUs[ 1 ] ||
			 (CurrentTime.aulWallTimeUs[ 1 ] == TimeoutTime.aulWallTimeUs[ 1 ] &&
			  CurrentTime.aulWallTimeUs[ 0 ] >= TimeoutTime.aulWallTimeUs[ 0 ]) )
			fConditionFlag = FALSE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReadDword

Description:    Read a DWORD at specified address in external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulAddress				DWORD address where to read.
f_pulReadData			Resulting data.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReadDword
UINT32 Oct6100ApiReadDword( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulAddress,
				OUT		PUINT32						f_pulReadData )
{
	tOCT6100_READ_PARAMS	ReadParams;
	UINT16	usReadData;
	
	UINT32	ulResult;
	UINT32	ulTempData;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/*==================================================================================*/
	/* Read the first 16 bits. */
	ReadParams.ulReadAddress = f_ulAddress;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Save data. */
	ulTempData = usReadData << 16;

	/* Read the last 16 bits. */
	ReadParams.ulReadAddress += 2;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save data. */
	ulTempData |= usReadData;
		
	/*==================================================================================*/
	
	/* Return the read value.*/
	*f_pulReadData = ulTempData;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteDword

Description:    Write a DWORD at specified address in external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulAddress				DWORD address where to write.
f_ulWriteData			DWORD data to write.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteDword
UINT32 Oct6100ApiWriteDword( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulAddress,
				IN		UINT32						f_ulWriteData )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Write the first 16 bits. */
	WriteParams.ulWriteAddress = f_ulAddress;
	WriteParams.usWriteData = (UINT16)((f_ulWriteData >> 16) & 0xFFFF);
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Write the last word. */
	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)(f_ulWriteData & 0xFFFF);
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCreateFeatureMask

Description:    

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_ulFieldSize			Size of the field, in bits.
f_ulFieldBitOffset		Bit offset, from the least significant bit.
f_pulFieldMask			Resulting mask.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCreateFeatureMask
VOID Oct6100ApiCreateFeatureMask( 
							IN		UINT32				f_ulFieldSize,
							IN		UINT32				f_ulFieldBitOffset,
							OUT		PUINT32				f_pulFieldMask )
{
	UINT32	ulMask;
	UINT32	i;

	ulMask = 0;

	/* Create the mask based on the field size. */
	for ( i = 0; i < f_ulFieldSize; i++ )
	{
		ulMask <<= 1;
		ulMask  |= 1;
	}

	/* Once the mask is of the desired size, offset it to fit the field */
	/* within the DWORD read. */
	ulMask <<= f_ulFieldBitOffset;
	  
	/* Return the mask. */
	*f_pulFieldMask = ulMask;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiStrStr

Description:    OCT6100 API version of strstr()

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pszSource				Source string to analyze.
f_pszString				String to look for.
f_pszLastCharPtr		Last character in the source string.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiStrStr
unsigned char const *Oct6100ApiStrStr( 
				IN		unsigned char const		*f_pszSource, 
				IN		unsigned char const		*f_pszString, 
				IN		unsigned char const		*f_pszLastCharPtr )
{
	UINT32	ulCurrentPos;
	UINT32	ulStringLength;
	UINT32	ulNumMatchingCharFound = 0;
	unsigned char const	*pchFirstChar = NULL;
	UINT32	ulSourceLength;

	if ( f_pszLastCharPtr < f_pszSource )
		return NULL;

	ulSourceLength = (UINT32)( f_pszLastCharPtr - f_pszSource );
	ulStringLength = Oct6100ApiStrLen( f_pszString );

	for ( ulCurrentPos = 0; ulCurrentPos < ulSourceLength; ulCurrentPos++ )
	{
		/* Check if the character matches. */
		if ( f_pszSource[ ulCurrentPos ] == f_pszString[ ulNumMatchingCharFound ] )
		{
			if ( ulNumMatchingCharFound == 0 )
				pchFirstChar = ( f_pszSource + ulCurrentPos );

			ulNumMatchingCharFound++;

			/* Check if the whole string matched. */
			if ( ulNumMatchingCharFound == ulStringLength )
				break;
		}
		else if ( ulNumMatchingCharFound != 0 )
		{
			ulNumMatchingCharFound = 0;

			/* Reset the search, but take a look at the current character.  It might */
			/* be the beginning of the string we are looking for. */
			if ( f_pszSource[ ulCurrentPos ] == f_pszString[ ulNumMatchingCharFound ] )
			{
				pchFirstChar = ( f_pszSource + ulCurrentPos );
				ulNumMatchingCharFound++;

				/* Check if the whole string matched. */
				/* This check must be done in case we have the 1 character strstr */
				if ( ulNumMatchingCharFound == ulStringLength )
					break;
			}
		}
	}

	if ( ulCurrentPos == ulSourceLength )
		return NULL;
	else
		return pchFirstChar;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiStrLen

Description:    OCT6100 API version of strlen()

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pszString				Source string to count length of.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiStrLen
UINT32 Oct6100ApiStrLen( 
				IN		unsigned char const	*f_pszString )
{
	UINT32	ulCount = 0;

	while( f_pszString[ ulCount ] != '\0' )
		ulCount++;

	return ulCount;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAsciiToHex

Description:    Convert an ASCII character to an hexadecimal value.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_chCharacter			ASCII character to convert.
f_pulValue				Resulting hexadecimal value.	

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAsciiToHex
UINT32 Oct6100ApiAsciiToHex( 
				IN		UINT8		f_chCharacter, 
				OUT		PUINT32		f_pulValue )
{
	switch ( f_chCharacter ) 
	{
	case '0':
		(*f_pulValue) = 0x0;
		break;
	case '1':
		(*f_pulValue) = 0x1;
		break;
	case '2':
		(*f_pulValue) = 0x2;
		break;
	case '3':
		(*f_pulValue) = 0x3;
		break;
	case '4':
		(*f_pulValue) = 0x4;
		break;
	case '5':
		(*f_pulValue) = 0x5;
		break;
	case '6':
		(*f_pulValue) = 0x6;
		break;
	case '7':
		(*f_pulValue) = 0x7;
		break;
	case '8':
		(*f_pulValue) = 0x8;
		break;
	case '9':
		(*f_pulValue) = 0x9;
		break;
	case 'A':
	case 'a':
		(*f_pulValue) = 0xA;
		break;
	case 'B':
	case 'b':
		(*f_pulValue) = 0xB;
		break;
	case 'C':
	case 'c':
		(*f_pulValue) = 0xC;
		break;
	case 'D':
	case 'd':
		(*f_pulValue) = 0xD;
		break;
	case 'E':
	case 'e':
		(*f_pulValue) = 0xE;
		break;
	case 'F':
	case 'f':
		(*f_pulValue) = 0xF;
		break;
	default:
		(*f_pulValue) = 0x0;
		return cOCT6100_ERR_MISC_ASCII_CONVERSION_FAILED;
	}		

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiHexToAscii

Description:    Convert an hexadecimal value to an ASCII character.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_ulNumber				Hexadecimal value to convert.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiHexToAscii
UINT8 Oct6100ApiHexToAscii(
				IN		UINT32	f_ulNumber )
{
	if ( f_ulNumber >= 0xA )
		return (UINT8)( 55 + f_ulNumber );		/* Hex values from 0xA to 0xF */
	else
		return (UINT8)( 48 + f_ulNumber );		/* Hex values from 0x0 to 0x9 */
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRand

Description:    Random number generator.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_ulRange				Range of the random number to be generated.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRand
UINT32 Oct6100ApiRand( 
				IN		UINT32				f_ulRange )
{
	static UINT32	ulRandomSeed = 0x12345678;
	UINT32			ulBit0;

	UINT32			i, j;
	UINT16			ulWithinRange = FALSE;

	UINT32			ulResult = cOCT6100_ERR_OK;
	UINT16			ulLoop;

	UINT32			ulRangeMask;
	UINT32			ulAddedValue;


	ulRangeMask = 1;
	ulLoop = TRUE;
	i = 1;

	while ( ulLoop )
	{
		
		ulAddedValue = 2;
		for ( j = 1; j < i; j++ )
			ulAddedValue *= 2;

		ulRangeMask = ulRangeMask + ulAddedValue;
		
		if ( ulRangeMask >= f_ulRange )
			ulLoop = FALSE;

		i++;
	}

	while ( !ulWithinRange )
	{
		ulBit0 = ((ulRandomSeed >> 19) & 0x1) ^ ((ulRandomSeed >> 16) & 0x1);
		ulRandomSeed = ((ulRandomSeed << 1) & 0xFFFFF) | ulBit0;

		ulResult = ulRandomSeed & ulRangeMask;
		
		if ( ulResult <= f_ulRange )
			ulWithinRange = TRUE;
	}

	return ulResult;
}
#endif
