/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_memory_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_memory.c.  All elements defined in this 
	file are for private usage of the API.

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

$Octasic_Revision: 17 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_MEMORY_PRIV_H__
#define __OCT6100_MEMORY_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

/* TSI allocation pointer macros. */
#define mOCT6100_GET_TSI_MEMORY_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsiMemoryAllocOfst );

/* Conversion memory allocation pointer macros. */
#define mOCT6100_GET_CONVERSION_MEMORY_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulConversionMemoryAllocOfst );

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetMemorySwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiMemorySwInit(
				IN OUT  tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiBufferPlayoutMemorySwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiReserveBufferPlayoutMemoryNode( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT32							f_pulNewNode );

UINT32 Oct6100ApiReleaseBufferPlayoutMemoryNode( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulOldNode );

UINT32 Oct6100ApiReserveBufferPlayoutMemory( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulSize, 
				OUT		PUINT32							f_pulMallocAddress );

UINT32 Oct6100ApiReleaseBufferPlayoutMemory(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulMallocAddress );

UINT32 Oct6100ApiReserveTsiMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusTsiMemIndex );

UINT32 Oct6100ApiReleaseTsiMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usTsiMemIndex );

UINT32 Oct6100ApiReserveConversionMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusConversionMemIndex );

UINT32 Oct6100ApiReleaseConversionMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usConversionMemIndex );

#endif /* __OCT6100_MEMORY_PRIV_H__ */
