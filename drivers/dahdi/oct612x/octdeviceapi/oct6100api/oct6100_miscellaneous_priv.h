/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_miscellaneous_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_miscellaneous.c.  All elements defined in 
	this file are for private usage of the API.

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

$Octasic_Revision: 20 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_MISCELLANEOUS_PRIV_H__
#define __OCT6100_MISCELLANEOUS_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*---------------------------------------------------------------------------*\
	Macros used to shell the user function calls.  These macros are used to
	assert that the user does not change any of the members of the function's
	parameter structure, as required and indicated in the API specification.
	Ofcourse, these macros make the code heavier and thus slower.  That is why
	there is a compile option for disabling the extra checking.  These can be
	very helpful tools in debugging.
\*---------------------------------------------------------------------------*/

#ifndef cOCT6100_REMOVE_USER_FUNCTION_CHECK
#define mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )					\
{																			\
	PVOID	_pProcessContext;												\
	UINT32	_ulUserChipId;													\
	UINT32	_ulWriteAddress;												\
	UINT16	_usWriteData;													\
																			\
	/* Store the data that is to be passed to the user. */					\
	_pProcessContext = WriteParams.pProcessContext;							\
	_ulUserChipId = WriteParams.ulUserChipId;								\
	_ulWriteAddress = WriteParams.ulWriteAddress;							\
	_usWriteData = WriteParams.usWriteData;									\
																			\
	/* Call user function. */												\
	ulResult = Oct6100UserDriverWriteApi( &WriteParams );					\
																			\
	/* Check if user changed members of function's parameter structure. */	\
	if ( WriteParams.pProcessContext != _pProcessContext ||					\
		 WriteParams.ulUserChipId != _ulUserChipId ||						\
		 WriteParams.ulWriteAddress != _ulWriteAddress ||					\
		 WriteParams.ulWriteAddress != _ulWriteAddress ||					\
		 WriteParams.usWriteData != _usWriteData )							\
		ulResult = cOCT6100_ERR_FATAL_DRIVER_WRITE_API;						\
}
#else																		
#define mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )					\
	ulResult = Oct6100UserDriverWriteApi( &WriteParams );
#endif /* cOCT6100_REMOVE_USER_FUNCTION_CHECK */


#ifndef cOCT6100_REMOVE_USER_FUNCTION_CHECK
#define mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult )				\
{																				\
	PVOID	_pProcessContext;													\
	UINT32	_ulUserChipId;														\
	UINT32	_ulWriteAddress;													\
	UINT16	_usWriteData;														\
	UINT32	_ulWriteLength;														\
																				\
	/* Store the data that is to be passed to the user. */						\
	_pProcessContext = SmearParams.pProcessContext;								\
	_ulUserChipId = SmearParams.ulUserChipId;									\
	_ulWriteAddress = SmearParams.ulWriteAddress;								\
	_usWriteData = SmearParams.usWriteData;										\
	_ulWriteLength = SmearParams.ulWriteLength;									\
																				\
	/* Call user function. */													\
	ulResult = Oct6100UserDriverWriteSmearApi( &SmearParams );					\
																				\
	/* Check if user changed members of function's paraeter structure. */		\
	if ( SmearParams.pProcessContext != _pProcessContext ||						\
		 SmearParams.ulUserChipId != _ulUserChipId ||							\
		 SmearParams.usWriteData != _usWriteData ||								\
		 SmearParams.ulWriteLength != _ulWriteLength)							\
		ulResult = cOCT6100_ERR_FATAL_DRIVER_WRITE_SMEAR_API;					\
}
#else																		
#define mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult )				\
	ulResult = Oct6100UserDriverWriteSmearApi( &SmearParams );
#endif /* cOCT6100_REMOVE_USER_FUNCTION_CHECK */


#ifndef cOCT6100_REMOVE_USER_FUNCTION_CHECK
#define mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult )			\
{																			\
	PVOID	_pProcessContext;												\
	UINT32	_ulUserChipId;													\
	UINT32	_ulWriteAddress;												\
	PUINT16	_pusWriteData;													\
	UINT32	_ulWriteLength;													\
																			\
	/* Store the data that is to be passed to the user. */					\
	_pProcessContext = BurstParams.pProcessContext;							\
	_ulUserChipId = BurstParams.ulUserChipId;								\
	_ulWriteAddress = BurstParams.ulWriteAddress;							\
	_pusWriteData = BurstParams.pusWriteData;								\
	_ulWriteLength = BurstParams.ulWriteLength;								\
																			\
	/* Call user function. */												\
	ulResult = Oct6100UserDriverWriteBurstApi( &BurstParams );				\
																			\
	/* Check if user changed members of function's parameter structure. */	\
	if ( BurstParams.pProcessContext != _pProcessContext ||					\
		 BurstParams.ulUserChipId != _ulUserChipId ||						\
		 BurstParams.ulWriteAddress != _ulWriteAddress ||					\
		 BurstParams.pusWriteData != _pusWriteData ||						\
		 BurstParams.ulWriteLength != _ulWriteLength )						\
		ulResult = cOCT6100_ERR_FATAL_DRIVER_WRITE_BURST_API;				\
}
#else																		
#define mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult )			\
	ulResult = Oct6100UserDriverWriteBurstApi( &BurstParams );
#endif /* cOCT6100_REMOVE_USER_FUNCTION_CHECK */


#ifndef cOCT6100_REMOVE_USER_FUNCTION_CHECK
#define mOCT6100_DRIVER_READ_API( ReadParams, ulResult )					\
{																			\
	PVOID	_pProcessContext;												\
	UINT32	_ulUserChipId;													\
	UINT32	_ulReadAddress;													\
	PUINT16	_pusReadData;													\
																			\
	/* Store the data that is to be passed to the user. */					\
	_pProcessContext = ReadParams.pProcessContext;							\
	_ulUserChipId = ReadParams.ulUserChipId;								\
	_ulReadAddress = ReadParams.ulReadAddress;								\
	_pusReadData = ReadParams.pusReadData;									\
																			\
	/* Call user function. */												\
	ulResult = Oct6100UserDriverReadApi( &ReadParams );						\
																			\
	/* Check if user changed members of function's parameter structure. */	\
	if ( ReadParams.pProcessContext != _pProcessContext ||					\
		 ReadParams.ulUserChipId != _ulUserChipId ||						\
		 ReadParams.ulReadAddress != _ulReadAddress ||						\
		 ReadParams.pusReadData != _pusReadData )							\
		ulResult = cOCT6100_ERR_FATAL_DRIVER_READ_API;						\
}
#else																		
#define mOCT6100_DRIVER_READ_API( ReadParams, ulResult )					\
	ulResult = Oct6100UserDriverReadApi( &ReadParams );
#endif /* cOCT6100_REMOVE_USER_FUNCTION_CHECK */


#ifndef cOCT6100_REMOVE_USER_FUNCTION_CHECK
#define mOCT6100_DRIVER_READ_BURST_API( BurstParams, ulResult )				\
{																			\
	PVOID	_pProcessContext;												\
	UINT32	_ulUserChipId;													\
	UINT32	_ulReadAddress;													\
	PUINT16	_pusReadData;													\
	UINT32	_ulReadLength;													\
																			\
	/* Store the data that is to be passed to the user. */					\
	_pProcessContext = BurstParams.pProcessContext;							\
	_ulUserChipId = BurstParams.ulUserChipId;								\
	_ulReadAddress = BurstParams.ulReadAddress;								\
	_pusReadData = BurstParams.pusReadData;									\
	_ulReadLength = BurstParams.ulReadLength;								\
																			\
	/* Call user function. */												\
	ulResult = Oct6100UserDriverReadBurstApi( &BurstParams );				\
																			\
	/* Check if user changed members of function's parameter structure. */	\
	if ( BurstParams.pProcessContext != _pProcessContext ||					\
		 BurstParams.ulUserChipId != _ulUserChipId ||						\
		 BurstParams.ulReadAddress != _ulReadAddress ||						\
		 BurstParams.pusReadData != _pusReadData ||							\
		 BurstParams.ulReadLength != _ulReadLength )						\
		ulResult = cOCT6100_ERR_FATAL_DRIVER_READ_BURST_API;				\
}
#else																		
#define mOCT6100_DRIVER_READ_BURST_API( BurstParams, ulResult )				\
	ulResult = Oct6100UserDriverReadBurstApi( &BurstParams );
#endif /* cOCT6100_REMOVE_USER_FUNCTION_CHECK */

#define mOCT6100_ASSIGN_USER_READ_WRITE_OBJ( f_pApiInst, Params )


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		mOCT6100_RETRIEVE_NLP_CONF_DWORD

Description:    This function is used by the API to store on a per channel basis
				the various confguration DWORD from the device. The API performs 
				less read to the chip that way since it is always in synch 
				with the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

IN	f_pApiInst				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.
IN	f_pChanEntry			Pointer to an API channel structure..
IN	f_ulAddress				Address that needs to be modified..
IN	f_pulConfigDword		Pointer to the content stored in the API located at the
							desired address.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#define mOCT6100_RETRIEVE_NLP_CONF_DWORD( f_pApiInst, f_pChanEntry, f_ulAddress, f_pulConfigDword, f_ulResult )	\
{																												\
	UINT32	_ulFirstEmptyIndex = 0xFFFFFFFF;																	\
	UINT32	_i;																									\
																												\
	f_ulResult = cOCT6100_ERR_FATAL_8E;																			\
	(*f_pulConfigDword) = cOCT6100_INVALID_VALUE;																\
																												\
	/* Search for the Dword.*/																					\
	for ( _i = 0; _i < cOCT6100_MAX_NLP_CONF_DWORD; _i++ )														\
	{																											\
		if ( ( _ulFirstEmptyIndex == 0xFFFFFFFF ) && ( f_pChanEntry->aulNlpConfDword[ _i ][ 0 ] == 0x0 ) )		\
			_ulFirstEmptyIndex = _i;																			\
																												\
		if ( f_pChanEntry->aulNlpConfDword[ _i ][ 0 ] == f_ulAddress )											\
		{																										\
			/* We found the matching Dword.*/																	\
			(*f_pulConfigDword) = f_pChanEntry->aulNlpConfDword[ _i ][ 1 ];										\
			f_ulResult = cOCT6100_ERR_OK;																		\
		}																										\
	}																											\
																												\
	if ( ( _i == cOCT6100_MAX_NLP_CONF_DWORD ) && ( _ulFirstEmptyIndex == 0xFFFFFFFF ) )						\
	{																											\
		/* Nothing to do here, a fatal error occured, no memory was left. */									\
	}																											\
	else																										\
	{																											\
		if ( f_ulResult != cOCT6100_ERR_OK )																	\
		{																										\
			tOCT6100_READ_PARAMS	_ReadParams;																\
			UINT16					_usReadData;																\
																												\
			/* We did not found any entry, let's create a new entry.*/											\
			f_pChanEntry->aulNlpConfDword[ _ulFirstEmptyIndex ][ 0 ] = f_ulAddress;								\
																												\
			_ReadParams.pProcessContext = f_pApiInst->pProcessContext;											\
			mOCT6100_ASSIGN_USER_READ_WRITE_OBJ( f_pApiInst, _ReadParams );										\
			_ReadParams.ulUserChipId = f_pApiInst->pSharedInfo->ChipConfig.ulUserChipId;						\
			_ReadParams.pusReadData = &_usReadData;																\
																												\
			/* Read the first 16 bits.*/																		\
			_ReadParams.ulReadAddress = f_ulAddress;															\
			mOCT6100_DRIVER_READ_API( _ReadParams, f_ulResult );												\
			if ( f_ulResult == cOCT6100_ERR_OK )																\
			{																									\
				/* Save data.*/																					\
				(*f_pulConfigDword) = _usReadData << 16;														\
																												\
				/* Read the last 16 bits .*/																	\
				_ReadParams.ulReadAddress += 2;																	\
				mOCT6100_DRIVER_READ_API( _ReadParams, f_ulResult );											\
				if ( f_ulResult == cOCT6100_ERR_OK )															\
				{																								\
					/* Save data.*/																				\
					(*f_pulConfigDword) |= _usReadData;															\
					f_ulResult = cOCT6100_ERR_OK;																\
				}																								\
			}																									\
		}																										\
	}																											\
}


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		mOCT6100_SAVE_NLP_CONF_DWORD

Description:    This function stores a configuration Dword within an API channel
				structure and then writes it into the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

IN	f_pApiInst				Pointer to API instance. This memory is used to keep
							the present state of the chip and all its resources.
IN	f_pChanEntry			Pointer to an API channel structure..
IN	f_ulAddress				Address that needs to be modified..
IN	f_pulConfigDword		content to be stored in the API located at the
							desired address.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#define mOCT6100_SAVE_NLP_CONF_DWORD( f_pApiInst, f_pChanEntry, f_ulAddress, f_ulConfigDword, f_ulResult )	\
{																											\
	UINT32	_i;																								\
	UINT32	_ulLastValue = 0x0;																				\
																											\
	/* Search for the Dword.*/																				\
	for ( _i = 0; _i < cOCT6100_MAX_NLP_CONF_DWORD; _i++ )													\
	{																										\
		if ( f_pChanEntry->aulNlpConfDword[ _i ][ 0 ] == f_ulAddress )										\
		{																									\
			/* We found the matching Dword.*/																\
			_ulLastValue = f_pChanEntry->aulNlpConfDword[ _i ][ 1 ];										\
			f_pChanEntry->aulNlpConfDword[ _i ][ 1 ] = f_ulConfigDword;										\
			break;																							\
		}																									\
	}																										\
																											\
	if ( _i == cOCT6100_MAX_NLP_CONF_DWORD )																\
	{																										\
		f_ulResult = cOCT6100_ERR_FATAL_8F;																	\
	}																										\
	else																									\
	{																										\
		/* Write the config DWORD. */																		\
		tOCT6100_WRITE_PARAMS	_WriteParams;																\
																											\
		_WriteParams.pProcessContext = f_pApiInst->pProcessContext;											\
		mOCT6100_ASSIGN_USER_READ_WRITE_OBJ( f_pApiInst, _WriteParams )										\
		_WriteParams.ulUserChipId = f_pApiInst->pSharedInfo->ChipConfig.ulUserChipId;						\
																											\
			/* Check if it is worth calling the user function. */											\
		if ( ( f_ulConfigDword & 0xFFFF0000 ) != ( _ulLastValue & 0xFFFF0000 ) )							\
		{																									\
			/* Write the first 16 bits. */																	\
			_WriteParams.ulWriteAddress = f_ulAddress;														\
			_WriteParams.usWriteData = (UINT16)((f_ulConfigDword >> 16) & 0xFFFF);							\
			mOCT6100_DRIVER_WRITE_API( _WriteParams, f_ulResult );											\
		}																									\
		else																								\
		{																									\
			f_ulResult = cOCT6100_ERR_OK;																	\
		}																									\
																											\
		if ( f_ulResult == cOCT6100_ERR_OK )																\
		{																									\
			if ( ( f_ulConfigDword & 0x0000FFFF ) != ( _ulLastValue & 0x0000FFFF ) )						\
			{																								\
				/* Write the last word. */																	\
				_WriteParams.ulWriteAddress = f_ulAddress + 2;												\
				_WriteParams.usWriteData = (UINT16)(f_ulConfigDword & 0xFFFF);								\
				mOCT6100_DRIVER_WRITE_API( _WriteParams, f_ulResult );										\
			}																								\
		}																									\
	}																										\
}


#define mOCT6100_CREATE_FEATURE_MASK( f_ulFieldSize, f_ulFieldBitOffset, f_pulFieldMask )					\
{																											\
	(*f_pulFieldMask) = ( 1 << f_ulFieldSize );																\
	(*f_pulFieldMask) --;																					\
	(*f_pulFieldMask) <<= f_ulFieldBitOffset;																\
}


/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiWaitForTime(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_aulWaitTime[ 2 ] );

UINT32 Oct6100ApiWaitForPcRegisterBit(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulPcRegAdd,
				IN		UINT32						f_ulPcBitNum,
				IN		UINT32						f_ulValue,
				IN		UINT32						f_ulTimeoutUs,
				OUT		PBOOL						f_pfBitEqual );

UINT32 Oct6100ApiWriteDword( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulAddress,
				IN		UINT32						f_ulWriteData );

UINT32 Oct6100ApiReadDword( 
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT32						f_ulAddress,
				OUT		PUINT32						f_pulReadData );

VOID Oct6100ApiCreateFeatureMask( 
				IN		UINT32						f_ulFieldSize,
				IN		UINT32						f_ulFieldBitOffset,
				OUT		PUINT32						f_pulFieldMask );

unsigned char const *Oct6100ApiStrStr( 
				IN		unsigned char const				*f_pszSource, 
				IN		unsigned char const				*f_pszString, 
				IN		unsigned char const				*f_pszLastCharPtr );

UINT32 Oct6100ApiStrLen( 
				IN		unsigned char const				*f_pszString );

UINT32 Oct6100ApiAsciiToHex( 
				IN		UINT8						f_chCharacter, 
				IN		PUINT32						f_pulValue );

UINT8 Oct6100ApiHexToAscii( 
				IN		UINT32						f_ulNumber );

UINT32 Oct6100ApiRand( 
				IN		UINT32						f_ulRange );

#endif /* __OCT6100_MISCELLANEOUS_PRIV_H__ */
