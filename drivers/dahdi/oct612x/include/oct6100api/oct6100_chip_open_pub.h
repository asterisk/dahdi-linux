/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_chip_open_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_chip_open.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_chip_open_priv.h file.

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

$Octasic_Revision: 54 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHIP_OPEN_PUB_H__
#define __OCT6100_CHIP_OPEN_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_CHIP_OPEN_
{
	UINT32	ulUserChipId;
	BOOL	fMultiProcessSystem;
	PVOID	pProcessContext;

	UINT32	ulMaxRwAccesses;

	unsigned char const *pbyImageFile;	/* Byte pointer to the image file to be uploaded into the chip. */
	UINT32	ulImageSize;		/* Size of the image file (in bytes). */
	
	UINT32	ulMemClkFreq;		/*  10 - 133.3 MHz. */
	UINT32	ulUpclkFreq;		/*  1  - 66.6 MHz. */
	BOOL	fEnableMemClkOut;

	UINT32	ulMemoryType;		/* SDRAM or DDR type external memory. */
	UINT32	ulNumMemoryChips;	/* Number of memory chips present. */
	UINT32	ulMemoryChipSize;	/* The size of the memory chips. */
	
	UINT32	ulTailDisplacement;	/* Tail displacement supported by the chip. */

	BOOL	fEnableAcousticEcho;/* Acoustic echo cancellation enabled. */

	/* Resource allocation parameters. */
	UINT32	ulMaxChannels;
	UINT32	ulMaxTsiCncts;
	UINT32	ulMaxBiDirChannels;
	UINT32	ulMaxConfBridges;
	UINT32	ulMaxFlexibleConfParticipants;
	UINT32	ulMaxPlayoutBuffers;


	UINT32	ulMaxPhasingTssts;
	UINT32	ulMaxAdpcmChannels;
	BOOL	fUseSynchTimestamp;
	UINT32	aulTimestampTimeslots[ 4 ];
	UINT32	aulTimestampStreams[ 4 ];
	UINT32							ulInterruptPolarity;
	tOCT6100_INTERRUPT_CONFIGURE	InterruptConfig;

	UINT32	aulTdmStreamFreqs[ cOCT6100_TDM_STREAM_MAX_GROUPS ];
	UINT32	ulMaxTdmStreams;
	UINT32	ulTdmSampling;

	BOOL	fEnableFastH100Mode;

	UINT32	ulSoftToneEventsBufSize;		/* In events. */
	BOOL	fEnableExtToneDetection;
	BOOL	fEnable2100StopEvent;


	UINT32	ulSoftBufferPlayoutEventsBufSize;	/* In events. */
	UINT32	ulMaxRemoteDebugSessions;

	BOOL	fEnableChannelRecording;

	BOOL	fEnableProductionBist;
	UINT32	ulProductionBistMode;
	UINT32	ulNumProductionBistLoops;

} tOCT6100_CHIP_OPEN, *tPOCT6100_CHIP_OPEN;

typedef struct _OCT6100_GET_INSTANCE_SIZE_
{
	UINT32	ulApiInstanceSize;

} tOCT6100_GET_INSTANCE_SIZE, *tPOCT6100_GET_INSTANCE_SIZE;

typedef struct _OCT6100_CHIP_CLOSE_
{
	UINT32	ulDummyVariable;

} tOCT6100_CHIP_CLOSE, *tPOCT6100_CHIP_CLOSE;

typedef struct _OCT6100_CREATE_LOCAL_INSTANCE_
{
	tPOCT6100_INSTANCE_API	pApiInstShared;
	tPOCT6100_INSTANCE_API	pApiInstLocal;
	PVOID					pProcessContext;
	UINT32					ulUserChipId;

} tOCT6100_CREATE_LOCAL_INSTANCE, *tPOCT6100_CREATE_LOCAL_INSTANCE;

typedef struct _OCT6100_DESTROY_LOCAL_INSTANCE_
{
	UINT32					ulDummy;

} tOCT6100_DESTROY_LOCAL_INSTANCE, *tPOCT6100_DESTROY_LOCAL_INSTANCE;

typedef struct _OCT6100_GET_HW_REVISION_
{
	UINT32	ulUserChipId;
	PVOID	pProcessContext;
	UINT32	ulRevisionNum;

} tOCT6100_GET_HW_REVISION, *tPOCT6100_GET_HW_REVISION;

typedef struct _OCT6100_FREE_RESOURCES_
{
	BOOL	fFreeTsiConnections;
	BOOL	fFreeConferenceBridges;
	BOOL	fFreePlayoutBuffers;
	BOOL	fFreePhasingTssts;
	BOOL	fFreeAdpcmChannels;

} tOCT6100_FREE_RESOURCES, *tPOCT6100_FREE_RESOURCES;

typedef struct _OCT6100_PRODUCTION_BIST_
{
	UINT32	ulCurrentAddress;
	UINT32	ulCurrentLoop;
	UINT32	ulCurrentTest;
	UINT32	ulBistStatus;
	UINT32	ulFailedAddress;
	UINT32	ulReadValue;
	UINT32	ulExpectedValue;

} tOCT6100_PRODUCTION_BIST, *tPOCT6100_PRODUCTION_BIST;

typedef struct _OCT6100_API_GET_VERSION_
{
	UINT8	achApiVersion[ cOCT6100_API_VERSION_STRING_LENGTH ];

} tOCT6100_API_GET_VERSION, *tPOCT6100_API_GET_VERSION;

typedef struct _OCT6100_API_GET_CAPACITY_PINS_
{
	UINT32	ulUserChipId;
	PVOID	pProcessContext;
	UINT32	ulMemoryType;		/* SDRAM or DDR type external memory. */
	BOOL	fEnableMemClkOut;
	UINT32	ulMemClkFreq;
	UINT32	ulCapacityValue;
} tOCT6100_API_GET_CAPACITY_PINS, *tPOCT6100_API_GET_CAPACITY_PINS;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ChipOpenDef(
				OUT		tPOCT6100_CHIP_OPEN					f_pChipOpen );
UINT32 Oct6100ChipOpen(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_CHIP_OPEN					f_pChipOpen );

UINT32 Oct6100ChipCloseDef(
				OUT		tPOCT6100_CHIP_CLOSE				f_pChipClose );
UINT32 Oct6100ChipClose(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance, 
				IN OUT	tPOCT6100_CHIP_CLOSE				f_pChipClose );

UINT32 Oct6100GetInstanceSizeDef(
				OUT		tPOCT6100_GET_INSTANCE_SIZE			f_pInstanceSize );
UINT32 Oct6100GetInstanceSize(
				IN OUT	tPOCT6100_CHIP_OPEN					f_pChipOpen,
				IN OUT	tPOCT6100_GET_INSTANCE_SIZE			f_pInstanceSize );

UINT32 Oct6100CreateLocalInstanceDef(
				OUT		tPOCT6100_CREATE_LOCAL_INSTANCE		f_pCreateLocal );
UINT32 Oct6100CreateLocalInstance(
				IN OUT	tPOCT6100_CREATE_LOCAL_INSTANCE		f_pCreateLocal );

UINT32 Oct6100DestroyLocalInstanceDef(
				OUT		tPOCT6100_DESTROY_LOCAL_INSTANCE	f_pDestroyLocal );
UINT32 Oct6100DestroyLocalInstance(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance, 
				IN OUT	tPOCT6100_DESTROY_LOCAL_INSTANCE	f_pDestroyLocal );

UINT32 Oct6100ApiGetVersionDef(
				OUT		tPOCT6100_API_GET_VERSION			f_pApiGetVersion );
UINT32 Oct6100ApiGetVersion(
				IN OUT	tPOCT6100_API_GET_VERSION			f_pApiGetVersion );

UINT32 Oct6100GetHwRevisionDef(
				OUT		tPOCT6100_GET_HW_REVISION			f_pRevision );
UINT32 Oct6100GetHwRevision(
				IN OUT	tPOCT6100_GET_HW_REVISION			f_pRevision );

UINT32 Oct6100FreeResourcesDef(
				OUT	tPOCT6100_FREE_RESOURCES				f_pFreeResources );
UINT32 Oct6100FreeResources(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_FREE_RESOURCES			f_pFreeResources );

UINT32 Oct6100ProductionBistDef(
				OUT	tPOCT6100_PRODUCTION_BIST				f_pProductionBist );
UINT32 Oct6100ProductionBist(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_PRODUCTION_BIST			f_pProductionBist );

UINT32 Oct6100ApiGetCapacityPinsDef(
				tPOCT6100_API_GET_CAPACITY_PINS			f_pGetCapacityPins);

UINT32 Oct6100ApiGetCapacityPins(
				tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins  );


#endif /* __OCT6100_CHIP_OPEN_PUB_H__ */
	
