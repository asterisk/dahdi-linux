/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_tsi_cnct_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_tsi_cnct.c.  All elements defined in 
	this  file are for private usage of the API.  All public elements are 
	defined in the oct6100_tsi_cnct_pub.h file.

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

#ifndef __OCT6100_TSI_CNCT_PRIV_H__
#define __OCT6100_TSI_CNCT_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

/* TSI connection list pointer macros. */
#define mOCT6100_GET_TSI_CNCT_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_TSI_CNCT )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsiCnctListOfst );

#define mOCT6100_GET_TSI_CNCT_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_TSI_CNCT )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsiCnctListOfst)) + ulIndex;

#define mOCT6100_GET_TSI_CNCT_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulTsiCnctAllocOfst);

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetTsiCnctSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiTsiCnctSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );


UINT32 Oct6100TsiCnctOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen );

UINT32 Oct6100ApiCheckTsiParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen );

UINT32 Oct6100ApiReserveTsiResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				OUT		PUINT16							f_pusTsiChanIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex );

UINT32 Oct6100ApiWriteTsiStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );

UINT32 Oct6100ApiUpdateTsiEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_OPEN			f_pTsiCnctOpen,
				IN		UINT16							f_usTsiChanIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );


UINT32 Oct6100TsiCnctCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_TSI_CNCT_CLOSE		f_pTsiCnctClose );

UINT32 Oct6100ApiAssertTsiParams( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_TSI_CNCT_CLOSE		f_pTsiCnctClose,
				OUT		PUINT16							f_pusTsiChanIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex );

UINT32 Oct6100ApiInvalidateTsiStructs( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );

UINT32 Oct6100ApiReleaseTsiResources( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usTsiChanIndex,
				IN		UINT16							f_usTsiMemIndex );

UINT32 Oct6100ApiReserveTsiCnctEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusTsiChanIndex );

UINT32 Oct6100ApiReleaseTsiCnctEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usTsiChanIndex );

#endif /* __OCT6100_TSI_CNCT_PRIV_H__ */
