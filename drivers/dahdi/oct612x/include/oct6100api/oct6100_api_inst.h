/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_api_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing the definition of the API instance structure.

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

$Octasic_Revision: 40 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_API_INST_H__
#define __OCT6100_API_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_SHARED_INFO_
{
	/* Local copy of chip configuration structure. */
	tOCT6100_API_CHIP_CONFIG		ChipConfig;
	
	/* Miscellaneous calculations and mapping of static structures in external memory. */
	tOCT6100_API_MISCELLANEOUS		MiscVars;
	tOCT6100_API_MEMORY_MAP			MemoryMap;

	/* Error stats structure. */
	tOCT6100_API_CHIP_ERROR_STATS	ErrorStats;
	tOCT6100_API_CHIP_STATS			ChipStats;
	
	/* Mixer information. */
	tOCT6100_API_MIXER				MixerInfo;

	/* Image breakdown information. */
	tOCT6100_API_IMAGE_REGION		ImageRegion[ cOCT6100_MAX_IMAGE_REGION ];
	tOCT6100_API_IMAGE_INFO			ImageInfo;

	/* Configuration and management of interrupts. */
	tOCT6100_API_INTRPT_CONFIG		IntrptConfig;
	tOCT6100_API_INTRPT_MANAGE		IntrptManage;
	/* Remote debugging. */
	tOCT6100_API_REMOTE_DEBUG_INFO	RemoteDebugInfo;
	/* Chip debugging information. */
	tOCT6100_API_DEBUG				DebugInfo;

	/* Management variables of software and hardware buffers. */
	tOCT6100_API_SOFT_BUFS			SoftBufs;

	/* Caller buffer playout memory management structure. */
	tOCT6100_API_BUFFER_PLAYOUT_MALLOC_INFO	PlayoutInfo;
	


	UINT32	ulChannelListOfst;
	UINT32	ulChannelAllocOfst;

	UINT32	ulConversionMemoryAllocOfst;

	UINT32	ulTsiMemoryAllocOfst;
	UINT32	ulExtraTsiMemoryAllocOfst;
	UINT32	ulEchoMemoryAllocOfst;

	UINT32	ulTsstAllocOfst;
	UINT32	ulTsstListOfst;
	UINT32	ulTsstListAllocOfst;

	UINT32	ulTsiCnctListOfst;
	UINT32	ulTsiCnctAllocOfst;

	UINT32	ulMixerEventListOfst;
	UINT32	ulMixerEventAllocOfst;

	UINT32	ulCopyEventListOfst;
	UINT32	ulCopyEventAllocOfst;

	UINT32	ulBiDirChannelListOfst;
	UINT32	ulBiDirChannelAllocOfst;

	UINT32	ulConfBridgeListOfst;
	UINT32	ulConfBridgeAllocOfst;
	
	UINT32	ulFlexConfParticipantListOfst;
	UINT32	ulFlexConfParticipantAllocOfst;

	UINT32	ulPlayoutBufListOfst;
	UINT32	ulPlayoutBufAllocOfst;
	UINT32	ulPlayoutBufMemoryNodeListOfst;
	


	UINT32	ulAdpcmChanListOfst;
	UINT32	ulAdpcmChanAllocOfst;

	UINT32	ulPhasingTsstListOfst;
	UINT32	ulPhasingTsstAllocOfst;

} tOCT6100_SHARED_INFO, *tPOCT6100_SHARED_INFO;

typedef struct _OCT6100_INSTANCE_API_
{
	/* Pointer to portion of API instance structure shared amongst all processes. */
	tPOCT6100_SHARED_INFO	pSharedInfo;

	/* Pointer to user-supplied, process context structure.  The structure is
		a parameter to all user-supplied functions. */
	PVOID	pProcessContext;

	/* Handles to all serialization objects used by the API. */
	tOCT6100_USER_SERIAL_OBJECT	ulApiSerObj;

	
} tOCT6100_INSTANCE_API, *tPOCT6100_INSTANCE_API;

#endif /* __OCT6100_API_INST_H__ */
