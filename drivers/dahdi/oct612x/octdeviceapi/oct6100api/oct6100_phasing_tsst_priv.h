/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_phasing_tsst_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_phasing_tsst.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_phasing_tsst_pub.h file.

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

$Octasic_Revision: 12 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_PHASING_TSST_PRIV_H__
#define __OCT6100_PHASING_TSST_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_PHASING_TSST_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_PHASING_TSST )(( PUINT8 )pSharedInfo + pSharedInfo->ulPhasingTsstListOfst);

#define mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_PHASING_TSST )(( PUINT8 )pSharedInfo + pSharedInfo->ulPhasingTsstListOfst)) + ulIndex;

#define mOCT6100_GET_PHASING_TSST_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulPhasingTsstAllocOfst);

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetPhasingTsstSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiPhasingTsstSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100PhasingTsstOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen );

UINT32 Oct6100ApiCheckPhasingParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen );

UINT32 Oct6100ApiReservePhasingResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				OUT		PUINT16							f_pusPhasingIndex,
				OUT		PUINT16							f_pusTsstIndex );

UINT32 Oct6100ApiWritePhasingStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				IN		UINT16							f_usPhasingIndex,
				IN		UINT16							f_usTsstIndex );

UINT32 Oct6100ApiUpdatePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_OPEN		f_pPhasingTsstOpen,
				IN		UINT16							f_usPhasingIndex,
				IN		UINT16							f_usTsstIndex );

UINT32 Oct6100PhasingTsstCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_PHASING_TSST_CLOSE	f_pPhasingTsstClose );

UINT32 Oct6100ApiAssertPhasingParams( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_PHASING_TSST_CLOSE	f_pPhasingTsstClose,
				OUT		PUINT16							f_pusPhasingIndex,
				OUT		PUINT16							f_pusTsstIndex );

UINT32 Oct6100ApiInvalidatePhasingStructs( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usTsstIndex );

UINT32 Oct6100ApiReleasePhasingResources( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	UINT16							f_usPhasingIndex );

UINT32 Oct6100ApiReservePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusPhasingIndex );

UINT32 Oct6100ApiReleasePhasingEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usPhasingIndex );

#endif /* #ifndef cOCT6100_REMOVE_PHASING_TSST */
