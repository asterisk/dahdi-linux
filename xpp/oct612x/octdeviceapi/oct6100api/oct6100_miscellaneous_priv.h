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
	UINT16	_usWriteData;														\
	UINT32	_ulWriteLength;														\
																				\
	/* Store the data that is to be passed to the user. */						\
	_pProcessContext = SmearParams.pProcessContext;								\
	_ulUserChipId = SmearParams.ulUserChipId;									\
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

UINT32 oct6100_retrieve_nlp_conf_dword(tPOCT6100_INSTANCE_API f_pApiInst,
								tPOCT6100_API_CHANNEL f_pChanEntry,
								UINT32 f_ulAddress,
								UINT32 *f_pulConfigDword);

UINT32 oct6100_save_nlp_conf_dword(tPOCT6100_INSTANCE_API f_pApiInst,
								tPOCT6100_API_CHANNEL f_pChanEntry,
								UINT32 f_ulAddress,
								UINT32 f_ulConfigDword);

#endif /* __OCT6100_MISCELLANEOUS_PRIV_H__ */
