/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_tsst_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_tsst.c.  All elements defined in 
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

$Octasic_Revision: 14 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_TSST_PRIV_H__
#define __OCT6100_TSST_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

/* TSST allocation and serialization pointer macros. */
#define mOCT6100_GET_TSST_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PUINT32 )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsstAllocOfst);

#define mOCT6100_GET_TSST_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_TSST_ENTRY )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsstListOfst );

#define mOCT6100_GET_TSST_LIST_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_TSST_ENTRY )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsstListOfst)) + ulIndex;

#define mOCT6100_GET_TSST_LIST_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsstListAllocOfst);

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetTsstSwSizes(
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiTsstSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiValidateTsst( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulNumTssts,
				IN		UINT32							f_ulTimeslot,
				IN		UINT32							f_ulStream,
				IN		UINT32							f_ulDirection );

UINT32 Oct6100ApiReserveTsst( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulTimeslot,
				IN		UINT32							f_ulStream,
				IN		UINT32							f_ulNumTsst,
				IN		UINT32							f_ulDirection,
				OUT		PUINT16							f_pusTsstMemIndex,
				OUT		PUINT16							f_pusTsstListIndex );

UINT32 Oct6100ApiReleaseTsst( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulTimeslot,
				IN		UINT32							f_ulStream,
				IN		UINT32							f_ulNumTsst,
				IN		UINT32							f_ulDirection,
				IN		UINT16							f_usTsstListIndex );

#endif /* __OCT6100_TSST_PRIV_H__ */
