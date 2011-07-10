/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_chip_open_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_chip_open.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_chip_open_pub.h file.

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

$Octasic_Revision: 63 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHIP_OPEN_PRIV_H__
#define __OCT6100_CHIP_OPEN_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_INSTANCE_SIZES_
{
	/* Each of the following elements indicates the size of the instance memory */
	/* needed by the corresponding API module. All sizes are in bytes. */
	UINT32	ulChannelList;
	UINT32	ulChannelAlloc;

	UINT32	ulTsiCnctList;
	UINT32	ulTsiCnctAlloc;

	UINT32	ulMixerEventList;
	UINT32	ulMixerEventAlloc;

	UINT32	ulBiDirChannelList;
	UINT32	ulBiDirChannelAlloc;

	UINT32	ulAdpcmChannelList;
	UINT32	ulAdpcmChannelAlloc;

	UINT32	ulSoftBufPlayoutEventsBuffer;

	UINT32	ulCopyEventList;
	UINT32	ulCopyEventAlloc;
	
	UINT32	ulConfBridgeList;
	UINT32	ulConfBridgeAlloc;

	UINT32	ulFlexConfParticipantsList;
	UINT32	ulFlexConfParticipantsAlloc;

	UINT32	ulPlayoutBufList;
	UINT32	ulPlayoutBufAlloc;
	UINT32	ulPlayoutBufMemoryNodeList;


	
	UINT32	ulSoftToneEventsBuffer;

	UINT32	ulPhasingTsstList;
	UINT32	ulPhasingTsstAlloc;

	UINT32	ulConversionMemoryAlloc;

	UINT32	ulTsiMemoryAlloc;
	UINT32	ulTsstAlloc;
	
	UINT32	ulTsstEntryList;
	UINT32	ulTsstEntryAlloc;

	UINT32	ulRemoteDebugList;
	UINT32	ulRemoteDebugTree;
	UINT32	ulRemoteDebugPktCache;
	UINT32	ulRemoteDebugDataBuf;

	/* Memory consumed by static members of API instance. */
	UINT32	ulApiInstStatic;

	/* Total memory size for API instance. */
	UINT32	ulApiInstTotal;

} tOCT6100_API_INSTANCE_SIZES, *tPOCT6100_API_INSTANCE_SIZES;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiCheckChipConfiguration(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip );

UINT32 Oct6100ApiCheckImageFileHeader(
				IN		tPOCT6100_CHIP_OPEN				f_pChipOpen );

UINT32 Oct6100ApiCopyChipConfiguration(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip );

UINT32 Oct6100ApiInitializeMiscellaneousVariables(
				IN OUT	tPOCT6100_INSTANCE_API			f_pInstance );

UINT32 Oct6100ApiCalculateInstanceSizes(
				IN OUT	tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstanceSizes );

UINT32 Oct6100ApiAllocateInstanceMemory(
				IN OUT	tPOCT6100_INSTANCE_API			f_pInstance,
				IN		tPOCT6100_API_INSTANCE_SIZES	f_pInstanceSizes );

UINT32 Oct6100ApiInitializeInstanceMemory(
				IN OUT	tPOCT6100_INSTANCE_API			f_pInstance );

UINT32 Oct6100ApiGetChipRevisionNum(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiMapExternalMemory(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiDecodeKeyAndBist(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiBootFc2Pll(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiProgramFc1Pll(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiBootFc1Pll(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiWriteH100Registers(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiExternalMemoryBist(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiExternalMemoryInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiLoadImage(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiCpuRegisterBist(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiBootSdram(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiEnableClocks(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiProgramNLP(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiSetH100Register(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiWriteMiscellaneousRegisters(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT16 Oct6100ApiGenerateNumber( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulIndex,
				IN		UINT32							f_ulDataMask );

UINT32 Oct6100ApiRandomMemoryWrite(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulMemBase,
				IN		UINT32							f_ulMemSize,
				IN		UINT32							f_ulNumDataBits,
				IN		UINT32							f_ulNumAccesses,
				IN		UINT32							f_ulErrorCode );

UINT32 Oct6100ApiUserIoTest( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiCreateSerializeObjects(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulUserChipId );

UINT32 Oct6100ApiDestroySerializeObjects(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiRunEgo(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance, 
				IN		BOOL							f_fStoreFlag, 
				IN		UINT32							f_ulNumEntry, 
				OUT		PUINT32							f_aulEntry );

UINT32 Oct6100ApiCreateEgoEntry( 
				IN OUT	UINT32							f_ulExternalAddress, 
				IN		UINT32							f_ulInternalAddress, 
				IN		UINT32							f_ulNumBytes, 
				IN		UINT32							f_aulEntry[ 2 ] );





UINT32 Oct6100ApiInitChannels(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiInitMixer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiInitRecordResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100FreeResourcesSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_FREE_RESOURCES		f_pFreeResources );

UINT32 Oct6100ProductionBistSer(
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_PRODUCTION_BIST			f_pProductionBist );

UINT32 Oct6100ApiProductionCrc(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		PUINT32							f_pulMessage,
				IN		UINT32							f_ulMessageLength,
				OUT		PUINT32							f_pulCrcResult );

UINT32 Oct6100ApiReadCapacity(	
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins );

UINT32 Oct6100ApiCpuRegisterBistReadCap(
				IN  tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins );

UINT32 Oct6100ApiBootFc2PllReadCap(
				IN 	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins );

UINT32 Oct6100ApiProgramFc1PllReadCap(
				IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins );

UINT32 Oct6100ApiInitToneInfo(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiClearInterrupts(
					IN	tPOCT6100_INSTANCE_API			f_pApiInstance );
#endif /* __OCT6100_CHIP_OPEN_PRIV_H__ */
