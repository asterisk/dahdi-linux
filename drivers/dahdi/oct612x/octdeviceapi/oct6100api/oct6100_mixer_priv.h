/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_mixer_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_mixer.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_mixer_pub.h file.
	
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

$Octasic_Revision: 18 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_MIXER_PRIV_H__
#define __OCT6100_MIXER_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_MIXER_EVENT_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_MIXER_EVENT )(( PUINT8 )pSharedInfo + pSharedInfo->ulMixerEventListOfst);

#define mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_MIXER_EVENT )(( PUINT8 )pSharedInfo + pSharedInfo->ulMixerEventListOfst)) + ulIndex;

#define mOCT6100_GET_MIXER_EVENT_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulMixerEventAllocOfst);

#define mOCT6100_GET_COPY_EVENT_LIST_PNT( pSharedInfo, pList ) \
			pList = ( tPOCT6100_API_COPY_EVENT )(( PUINT8 )pSharedInfo + pSharedInfo->ulCopyEventListOfst);

#define mOCT6100_GET_COPY_EVENT_ENTRY_PNT( pSharedInfo, pEntry, ulIndex ) \
			pEntry = (( tPOCT6100_API_COPY_EVENT )(( PUINT8 )pSharedInfo + pSharedInfo->ulCopyEventListOfst)) + ulIndex;

#define mOCT6100_GET_COPY_EVENT_ALLOC_PNT( pSharedInfo, pAlloc ) \
			pAlloc = ( PVOID )(( PUINT8 )pSharedInfo + pSharedInfo->ulCopyEventAllocOfst);

/*****************************  TYPES  ***************************************/

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiGetMixerSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes );

UINT32 Oct6100ApiMixerSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32	Oct6100ApiMixerEventAdd( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex,
				IN		UINT16							f_usEventType,
				IN		UINT16							f_usDestinationChanIndex );

UINT32	Oct6100ApiMixerEventRemove( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex,
				IN		UINT16							f_usEventType );

UINT32 Oct6100MixerCopyEventCreateSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE		f_pCopyEventCreate );

UINT32 Oct6100ApiCheckCopyEventCreateParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_COPY_EVENT_CREATE		f_pCopyEventCreate, 
				OUT		PUINT16							f_pusSourceChanIndex, 
				OUT		PUINT16							f_pusDestinationChanIndex );

UINT32 Oct6100ApiReserveCopyEventCreateResources(	
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusCopyEntryIndex, 
				IN OUT	PUINT16							f_pusCopyEventIndex );

UINT32 Oct6100ApiWriteCopyEventCreateStructs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_COPY_EVENT_CREATE		f_pCopyEventCreate, 
				IN		UINT16							f_usMixerEventIndex,
				IN		UINT16							f_usSourceChanIndex, 
				IN		UINT16							f_usDestinationChanIndex );

UINT32 Oct6100ApiUpdateCopyEventCreateEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_CREATE		f_pCopyEventCreate, 
				IN		UINT16							f_usCopyEventIndex,
				IN		UINT16							f_usMixerEventIndex,
				IN		UINT16							f_usSourceChanIndex,
				IN		UINT16							f_usDestinationChanIndex );

UINT32 Oct6100MixerCopyEventDestroySer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_COPY_EVENT_DESTROY	f_pCopyEventDestroy );

UINT32 Oct6100ApiAssertCopyEventDestroyParams( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_COPY_EVENT_DESTROY	f_pCopyEventDestroy,
				IN OUT	PUINT16							f_pusCopyEventIndex,
				IN OUT	PUINT16							f_pusMixerEventIndex );

UINT32 Oct6100ApiInvalidateCopyEventStructs( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usCopyEventIndex,
				IN		UINT16							f_usMixerEventIndex );

UINT32 Oct6100ApiReleaseCopyEventResources( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usCopyEventIndex,
				IN		UINT16							f_usMixerEventIndex );

UINT32 Oct6100ApiReserveMixerEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusEventIndex );

UINT32 Oct6100ApiReleaseMixerEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex );

UINT32 Oct6100ApiGetFreeMixerEventCnt(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT32							f_pulFreeEventCnt );

UINT32 Oct6100ApiReserveCopyEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusEventIndex );

UINT32 Oct6100ApiReleaseCopyEventEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEventIndex );
#endif /* __OCT6100_MIXER_PRIV_H__ */
