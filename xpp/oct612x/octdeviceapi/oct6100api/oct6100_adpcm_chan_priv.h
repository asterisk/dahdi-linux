/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_adpcm_chan_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_adpcm_chan.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_adpcm_chan_pub.h file.

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

$Octasic_Revision: 7 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_ADPCM_CHAN_PRIV_H__
#define __OCT6100_ADPCM_CHAN_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

/* ADPCM channel list pointer macros. */
#define mOCT6100_GET_ADPCM_CHAN_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_ADPCM_CHAN )(( PUINT8 )pSharedInfo + pSharedInfo->ulAdpcmChanListOfst );

#define mOCT6100_GET_ADPCM_CHAN_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_ADPCM_CHAN )(( PUINT8 )pSharedInfo + pSharedInfo->ulAdpcmChanListOfst)) + ulIndex;

#define mOCT6100_GET_ADPCM_CHAN_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulAdpcmChanAllocOfst);

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetAdpcmChanSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiAdpcmChanSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );


UINT32 Oct6100AdpcmChanOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen );

UINT32 Oct6100ApiCheckAdpcmChanParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen );

UINT32 Oct6100ApiReserveAdpcmChanResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				OUT		PUINT16							f_pusTsiChanIndex,
				OUT		PUINT16							f_pusAdpcmMemIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex );

UINT32 Oct6100ApiWriteAdpcmChanStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				IN		UINT16							f_usAdpcmMemIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );

UINT32 Oct6100ApiUpdateAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN		f_pAdpcmChanOpen,
				IN		UINT16							f_usTsiChanIndex,
				IN		UINT16							f_usAdpcmMemIndex,
				IN		UINT16							f_usTsiMemIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );

UINT32 Oct6100AdpcmChanCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_CLOSE		f_pAdpcmChanClose );

UINT32 Oct6100ApiAssertAdpcmChanParams( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_ADPCM_CHAN_CLOSE		f_pAdpcmChanClose,
				OUT		PUINT16							f_pusTsiChanIndex,
				OUT		PUINT16							f_pusAdpcmMemIndex,
				OUT		PUINT16							f_pusTsiMemIndex,
				OUT		PUINT16							f_pusInputTsstIndex,
				OUT		PUINT16							f_pusOutputTsstIndex );

UINT32 Oct6100ApiInvalidateAdpcmChanStructs( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usAdpcmChanIndex,
				IN		UINT16							f_usInputTsstIndex,
				IN		UINT16							f_usOutputTsstIndex );

UINT32 Oct6100ApiReleaseAdpcmChanResources( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usTsiChanIndex,
				IN		UINT16							f_usAdpcmMemIndex,
				IN		UINT16							f_usTsiMemIndex );

UINT32 Oct6100ApiReserveAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusAdpcmChanIndex );

UINT32 Oct6100ApiReleaseAdpcmChanEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usAdpcmChanIndex );

#endif /* __OCT6100_ADPCM_CHAN_PRIV_H__ */
