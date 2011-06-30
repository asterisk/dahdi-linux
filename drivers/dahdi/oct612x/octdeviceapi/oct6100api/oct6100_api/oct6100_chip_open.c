/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_chip_open.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions used to power-up the chip according to the
	user's configuration.  Also, the API instance is initialized to reflect the
	desired configuration.

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

$Octasic_Revision: 347 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <dahdi/compat/bsd.h>
#else
#ifndef __KERNEL__
#include <stdlib.h>
#define kmalloc(size, type)	malloc(size)
#define kfree(ptr)		free(ptr)
#define GFP_ATOMIC		0 /*Dummy */
#else
#include <linux/slab.h>
#include <linux/kernel.h>
#endif
#endif

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "apilib/octapi_bt0.h"
#include "apilib/octapi_llman.h"

#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_tsi_cnct_inst.h"
#include "oct6100api/oct6100_events_inst.h"
#include "oct6100api/oct6100_conf_bridge_inst.h"
#include "oct6100api/oct6100_playout_buf_inst.h"

#include "oct6100api/oct6100_mixer_inst.h"
#include "oct6100api/oct6100_channel_inst.h"
#include "oct6100api/oct6100_adpcm_chan_inst.h"
#include "oct6100api/oct6100_phasing_tsst_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_api_inst.h"

#include "oct6100api/oct6100_chip_stats_pub.h"
#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_tsi_cnct_pub.h"
#include "oct6100api/oct6100_events_pub.h"
#include "oct6100api/oct6100_conf_bridge_pub.h"
#include "oct6100api/oct6100_playout_buf_pub.h"

#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_adpcm_chan_pub.h"
#include "oct6100api/oct6100_phasing_tsst_pub.h"
#include "oct6100api/oct6100_remote_debug_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_mixer_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_debug_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_interrupts_priv.h"
#include "oct6100_chip_stats_priv.h"
#include "octrpc/rpc_protocol.h"
#include "oct6100_remote_debug_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_tsi_cnct_priv.h"
#include "oct6100_mixer_priv.h"
#include "oct6100_events_priv.h"
#include "oct6100_conf_bridge_priv.h"
#include "oct6100_playout_buf_priv.h"

#include "oct6100_channel_priv.h"
#include "oct6100_adpcm_chan_priv.h"
#include "oct6100_phasing_tsst_priv.h"
#include "oct6100_tlv_priv.h"
#include "oct6100_debug_priv.h"
#include "oct6100_version.h"


/****************************  PUBLIC FUNCTIONS  *****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100GetInstanceSizeDef

Description:    Retrieves the size of the required API instance structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pGetSize			Structure containing API instance size. 
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100GetInstanceSizeDef
UINT32 Oct6100GetInstanceSizeDef(
				tPOCT6100_GET_INSTANCE_SIZE		f_pGetSize )
{
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100GetInstanceSize
UINT32 Oct6100GetInstanceSize(
				tPOCT6100_CHIP_OPEN				f_pChipOpen,
				tPOCT6100_GET_INSTANCE_SIZE		f_pGetSize )
{
	tOCT6100_API_INSTANCE_SIZES	InstanceSizes;
	UINT32						ulResult;

	/* Check user configuration for errors and conflicts. */
	ulResult = Oct6100ApiCheckChipConfiguration( f_pChipOpen );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Calculate the instance size required for user's configuration. */
	ulResult = Oct6100ApiCalculateInstanceSizes( f_pChipOpen, &InstanceSizes );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Return required size to user. */
	f_pGetSize->ulApiInstanceSize = InstanceSizes.ulApiInstTotal;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChipOpenDef

Description:    Inserts default chip configuration parameters into the
				structure pointed to by f_pChipOpen.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pChipOpen			Structure containing user chip configuration. 
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipOpenDef
UINT32 Oct6100ChipOpenDef(
				tPOCT6100_CHIP_OPEN	f_pChipOpen )
{
	UINT32	i;

	f_pChipOpen->ulUserChipId = 0;
	f_pChipOpen->fMultiProcessSystem = FALSE;
	f_pChipOpen->pProcessContext = NULL;

	f_pChipOpen->ulMaxRwAccesses = 8;

	f_pChipOpen->pbyImageFile = NULL;
	f_pChipOpen->ulImageSize = 0;
	
	f_pChipOpen->ulMemClkFreq = 133000000;							/* 133 Mhz */
	f_pChipOpen->ulUpclkFreq = cOCT6100_UPCLK_FREQ_33_33_MHZ;		/* 33.33  Mhz */
	f_pChipOpen->fEnableMemClkOut = TRUE;

	f_pChipOpen->ulMemoryType = cOCT6100_MEM_TYPE_DDR;
	f_pChipOpen->ulNumMemoryChips = 1;
	f_pChipOpen->ulMemoryChipSize = cOCT6100_MEMORY_CHIP_SIZE_64MB;

	/* Set the tail displacement to zero. */
	f_pChipOpen->ulTailDisplacement = 0;
	
	/* Disable acoustic echo by default. */
	f_pChipOpen->fEnableAcousticEcho = FALSE;

	/* Resource allocation parameters. */
	f_pChipOpen->ulMaxChannels = 256;
	f_pChipOpen->ulMaxTsiCncts = 0;
	f_pChipOpen->ulMaxBiDirChannels = 0;
	f_pChipOpen->ulMaxConfBridges = 0;
	f_pChipOpen->ulMaxFlexibleConfParticipants = 0;
	f_pChipOpen->ulMaxPlayoutBuffers = 0;

	f_pChipOpen->ulMaxPhasingTssts	= 0;
	f_pChipOpen->ulMaxAdpcmChannels = 0;
	f_pChipOpen->ulMaxTdmStreams = 32;
	f_pChipOpen->fUseSynchTimestamp = FALSE;
	for ( i = 0; i < 4; i++ )
	{
		f_pChipOpen->aulTimestampTimeslots[ i ] = cOCT6100_INVALID_TIMESLOT;
		f_pChipOpen->aulTimestampStreams[ i ] = cOCT6100_INVALID_STREAM;
	}
	f_pChipOpen->fEnableFastH100Mode = FALSE;

	/* Configure the soft tone event buffer. */
	f_pChipOpen->ulSoftToneEventsBufSize = 128;
	f_pChipOpen->fEnableExtToneDetection = FALSE;
	f_pChipOpen->fEnable2100StopEvent = FALSE;

	/* Configure the soft playout event buffer. */
	f_pChipOpen->ulSoftBufferPlayoutEventsBufSize = cOCT6100_INVALID_VALUE;

	/* Interrupt configuration. */
	f_pChipOpen->ulInterruptPolarity = cOCT6100_ACTIVE_LOW_POLARITY;

	f_pChipOpen->InterruptConfig.ulErrorMemoryConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pChipOpen->InterruptConfig.ulFatalGeneralConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pChipOpen->InterruptConfig.ulFatalMemoryConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pChipOpen->InterruptConfig.ulFatalMemoryConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pChipOpen->InterruptConfig.ulErrorH100Config = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;

	f_pChipOpen->InterruptConfig.ulErrorMemoryTimeout = 100;
	f_pChipOpen->InterruptConfig.ulFatalMemoryTimeout = 100;
	f_pChipOpen->InterruptConfig.ulErrorH100Timeout = 100;
	f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsTimeout = 100;
	f_pChipOpen->ulMaxRemoteDebugSessions = 0;
	f_pChipOpen->ulTdmSampling = cOCT6100_TDM_SAMPLE_AT_3_QUARTERS;
	for ( i = 0; i < cOCT6100_TDM_STREAM_MAX_GROUPS; i++ )
		f_pChipOpen->aulTdmStreamFreqs[ i ] = cOCT6100_TDM_STREAM_FREQ_8MHZ;



	f_pChipOpen->fEnableChannelRecording = FALSE;
	f_pChipOpen->fEnableProductionBist = FALSE;
	f_pChipOpen->ulProductionBistMode = cOCT6100_PRODUCTION_BIST_STANDARD;
	f_pChipOpen->ulNumProductionBistLoops = 1;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChipOpen

Description:    Configures the chip according to the user specified
				configuration f_pChipOpen. This function will perform all I/O
				accesses necessary and initialize the API instance to reflect
				the configuration.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChipOpen				Structure containing user chip configuration. 
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipOpen
UINT32 Oct6100ChipOpen(
				tPOCT6100_INSTANCE_API	f_pApiInstance,
				tPOCT6100_CHIP_OPEN		f_pChipOpen )
{
	tOCT6100_API_INSTANCE_SIZES	*InstanceSizes;
	UINT32						ulStructSize;
	UINT32						ulResult;
	UINT32						ulTempVar;

	/* Check user chip configuration parameters for errors. */
	ulResult = Oct6100ApiCheckChipConfiguration( f_pChipOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Check if the host system is multi-process or not and adjust instance accordingly. */
	if ( f_pChipOpen->fMultiProcessSystem != TRUE )
	{
		/* Set pointer to tOCT6100_SHARED_INFO structure within instance. */
		ulStructSize = sizeof( tOCT6100_INSTANCE_API );
		mOCT6100_ROUND_MEMORY_SIZE( ulStructSize, ulTempVar )

		f_pApiInstance->pSharedInfo = ( tPOCT6100_SHARED_INFO )(( PUINT8 )f_pApiInstance + ulStructSize);

		/* Save the process context specified by the user. */
		f_pApiInstance->pProcessContext = f_pChipOpen->pProcessContext;

		/* Create serialization object handles. */
		ulResult = Oct6100ApiCreateSerializeObjects( f_pApiInstance, f_pChipOpen->ulUserChipId );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Copy the configuration structure. */
	ulResult = Oct6100ApiCopyChipConfiguration( f_pApiInstance, f_pChipOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Perform various calculations based on user chip configuration. */
	ulResult = Oct6100ApiInitializeMiscellaneousVariables( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	InstanceSizes = kmalloc(sizeof(tOCT6100_API_INSTANCE_SIZES), GFP_ATOMIC);
	if (!InstanceSizes)
		return cOCT6100_ERR_FATAL_0;

	/* Calculate the amount of memory needed for the API instance structure. */
	ulResult = Oct6100ApiCalculateInstanceSizes( f_pChipOpen, InstanceSizes );
	if ( ulResult != cOCT6100_ERR_OK ) {
		kfree(InstanceSizes);
		return ulResult;
	}

	/* Allocate the memory for the API instance structure internal pointers. */
	ulResult = Oct6100ApiAllocateInstanceMemory( f_pApiInstance, InstanceSizes );
	kfree(InstanceSizes);

	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Initialize the allocated instance structure memory. */
	ulResult = Oct6100ApiInitializeInstanceMemory( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Initialize the tone information structure. */
	ulResult = Oct6100ApiInitToneInfo( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Test the CPU registers. */
	ulResult = Oct6100ApiCpuRegisterBist( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Boot the FC2 PLL. */
	ulResult = Oct6100ApiBootFc2Pll( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Program the FC1 PLL. */
	ulResult = Oct6100ApiProgramFc1Pll( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Decode the key and bist internal memories. */
	ulResult = Oct6100ApiDecodeKeyAndBist( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Boot the FC1 PLL. */
	ulResult = Oct6100ApiBootFc1Pll( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Boot the SDRAM. */
	ulResult = Oct6100ApiBootSdram( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Bist the external memory. */
	ulResult = Oct6100ApiExternalMemoryBist( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Initialize the external memory. */
	ulResult = Oct6100ApiExternalMemoryInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Load the image into the chip. */
	ulResult = Oct6100ApiLoadImage( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write the clock distribution registers. */
	ulResult = Oct6100ApiEnableClocks( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Program the NLP processor. */
	ulResult = Oct6100ApiProgramNLP( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
	{		
		if ( ulResult == cOCT6100_ERR_OPEN_EGO_TIMEOUT )
			ulResult = Oct6100ApiProgramNLP( f_pApiInstance );
	}
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	if ( f_pChipOpen->fEnableProductionBist == FALSE )
	{
		/* Read all TLV fields present in external memory. */
		ulResult = Oct6100ApiProcessTlvRegion( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
		
		/* Configure the H.100 interface. */
		ulResult = Oct6100ApiSetH100Register( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Write miscellaneous registers. */
	ulResult = Oct6100ApiWriteMiscellaneousRegisters( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Proceed with the rest only if the production BIST has not been requested. */
	if ( f_pChipOpen->fEnableProductionBist == FALSE )
	{
		/* Initialize the errors counters. */
		ulResult = Oct6100ApiChipStatsSwInit( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Get revision number of chip. */
		ulResult = Oct6100ApiGetChipRevisionNum( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;


		


		/* Initialize the channels. */
		ulResult = Oct6100ApiInitChannels( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Initialize the mixer memory. */
		ulResult = Oct6100ApiInitMixer( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Initialize the mixer memory. */
		ulResult = Oct6100ApiInitRecordResources( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Initialize free external memory for buffer playout. */
		ulResult = Oct6100ApiBufferPlayoutMemorySwInit( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;




		/*Clear all interrupts that could have occured during startup*/
		ulResult = Oct6100ApiClearInterrupts( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Configure the interrupt registers. */
		ulResult = Oct6100ApiIsrHwInit( f_pApiInstance, &f_pChipOpen->InterruptConfig );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ChipCloseDef

Description:    Puts the chip into soft reset.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChipClose			Pointer to a tOCT6100_CHIP_CLOSE structure.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ChipCloseDef
UINT32 Oct6100ChipCloseDef(
				tPOCT6100_CHIP_CLOSE	f_pChipClose )
{
	f_pChipClose->ulDummyVariable = 0;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ChipClose
UINT32 Oct6100ChipClose(
				tPOCT6100_INSTANCE_API	f_pApiInstance,
				tPOCT6100_CHIP_CLOSE	f_pChipClose )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	WriteParams.ulWriteAddress = 0x100;
	WriteParams.usWriteData = 0x0000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Destroy the allocated ressources used for serialization. */
	ulResult = Oct6100ApiDestroySerializeObjects( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100CreateLocalInstance

Description:    Creates a local instance for a process in a multi-process
				host system.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pCreateLocal			Structure used to create process' local instance.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100CreateLocalInstanceDef
UINT32 Oct6100CreateLocalInstanceDef(
				tPOCT6100_CREATE_LOCAL_INSTANCE		f_pCreateLocal )
{
	f_pCreateLocal->pApiInstShared = NULL;
	f_pCreateLocal->pApiInstLocal = NULL;
	f_pCreateLocal->pProcessContext = NULL;
	f_pCreateLocal->ulUserChipId = 0;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100CreateLocalInstance
UINT32 Oct6100CreateLocalInstance(
				tPOCT6100_CREATE_LOCAL_INSTANCE		f_pCreateLocal )
{
	tPOCT6100_INSTANCE_API	pApiInstLocal;
	UINT32					ulApiInstSize;
	UINT32					ulTempVar;
	UINT32					ulResult;

	/* Check user's structure for errors. */
	if ( f_pCreateLocal->pApiInstShared == NULL )
		return cOCT6100_ERR_MULTIPROC_API_INST_SHARED;

	if ( f_pCreateLocal->pApiInstLocal == NULL )
		return cOCT6100_ERR_MULTIPROC_API_INST_LOCAL;

	/* Get local pointer to local instance. */
	pApiInstLocal = f_pCreateLocal->pApiInstLocal;

	/* Assign pointers to local structure. */
	ulApiInstSize = sizeof( tOCT6100_INSTANCE_API );
	mOCT6100_ROUND_MEMORY_SIZE( ulApiInstSize, ulTempVar )

	pApiInstLocal->pSharedInfo = ( tPOCT6100_SHARED_INFO )(( PUINT8 )f_pCreateLocal->pApiInstShared + ulApiInstSize);
	pApiInstLocal->pProcessContext = f_pCreateLocal->pProcessContext;

	/* Create serialization object handles needed. */
	ulResult = Oct6100ApiCreateSerializeObjects( pApiInstLocal, f_pCreateLocal->ulUserChipId );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100DestroyLocalInstance

Description:    Release local instance for a process in a multi-process
				host system.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pDestroyLocal			Structure used to destroy the process' local instance.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100DestroyLocalInstanceDef
UINT32 Oct6100DestroyLocalInstanceDef(
				tPOCT6100_DESTROY_LOCAL_INSTANCE		f_pDestroyLocal )
{
	f_pDestroyLocal->ulDummy = 0;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100DestroyLocalInstance
UINT32 Oct6100DestroyLocalInstance(
				tPOCT6100_INSTANCE_API					f_pApiInstance,
				tPOCT6100_DESTROY_LOCAL_INSTANCE		f_pDestroyLocal )
{
	UINT32					ulResult;

	/* Destroy the allocated ressources used for serialization. */
	ulResult = Oct6100ApiDestroySerializeObjects( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100GetHwRevision

Description:    Gets the hardware revision number of the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pGetHwRev				Pointer to user structure in which to return revision
						number.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100GetHwRevisionDef
UINT32 Oct6100GetHwRevisionDef(
				tPOCT6100_GET_HW_REVISION		f_pGetHwRev )
{
	f_pGetHwRev->ulUserChipId = cOCT6100_INVALID_CHIP_ID;
	f_pGetHwRev->pProcessContext = NULL;
	f_pGetHwRev->ulRevisionNum = cOCT6100_INVALID_VALUE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100GetHwRevision
UINT32 Oct6100GetHwRevision(
				tPOCT6100_GET_HW_REVISION		f_pGetHwRev )
{
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	ulResult;
	UINT16	usReadData;

	/* Read the hardware revision register. */
	ReadParams.pProcessContext = f_pGetHwRev->pProcessContext;

	ReadParams.ulUserChipId = f_pGetHwRev->ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	ReadParams.ulReadAddress = 0x17E;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	f_pGetHwRev->ulRevisionNum = ( usReadData >> 8 ) & 0xFF;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100FreeResources

Description:    This function closes all opened channels and frees all 
				specified global resources used by the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pFreeResources		Pointer to user structure in which to choose what 
						to free.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100FreeResourcesDef
UINT32 Oct6100FreeResourcesDef(
				tPOCT6100_FREE_RESOURCES			f_pFreeResources )
{
	f_pFreeResources->fFreeTsiConnections = FALSE;
	f_pFreeResources->fFreeConferenceBridges = FALSE;
	f_pFreeResources->fFreePlayoutBuffers = FALSE;
	f_pFreeResources->fFreePhasingTssts = FALSE;
	f_pFreeResources->fFreeAdpcmChannels = FALSE;
	
	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100FreeResources
UINT32 Oct6100FreeResources(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_FREE_RESOURCES			f_pFreeResources )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure. */
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100FreeResourcesSer( f_pApiInstance, f_pFreeResources );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ProductionBist

Description:    This function retrieves the current BIST status of the
				firmware.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pProductionBist		Pointer to user structure where the bist information
						will be returned.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ProductionBistDef
UINT32 Oct6100ProductionBistDef(
				tPOCT6100_PRODUCTION_BIST		f_pProductionBist )
{
	f_pProductionBist->ulCurrentAddress = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulCurrentLoop = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulFailedAddress = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulReadValue = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulExpectedValue = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulBistStatus = cOCT6100_BIST_IN_PROGRESS;
	f_pProductionBist->ulCurrentTest = cOCT6100_INVALID_VALUE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ProductionBist
UINT32 Oct6100ProductionBist(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_PRODUCTION_BIST			f_pProductionBist )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure. */
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100ProductionBistSer( f_pApiInstance, f_pProductionBist );
	}
	else
	{
		return ulSerRes;
	}

	/* Release the seized semaphores. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulSerRes = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );

	/* If an error occured then return the error code. */
	if ( ulSerRes != cOCT6100_ERR_OK )
		return ulSerRes;
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetVersion

Description:    Retrieves the API version.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pApiGetVersion	 Pointer to structure that will receive version information.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetVersionDef
UINT32 Oct6100ApiGetVersionDef(
				tPOCT6100_API_GET_VERSION			f_pApiGetVersion )
{
	UINT32	i;

	/* Initialize the string. */
	for ( i = 0; i < cOCT6100_API_VERSION_STRING_LENGTH; i++ )
		f_pApiGetVersion->achApiVersion[ i ] = 0;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ApiGetVersion
UINT32 Oct6100ApiGetVersion(
				tPOCT6100_API_GET_VERSION		f_pApiGetVersion )
{
	/* Copy API version information to user. */
	Oct6100UserMemCopy( f_pApiGetVersion->achApiVersion, cOCT6100_API_VERSION, sizeof(cOCT6100_API_VERSION) );

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetCapacityPins

Description:    Retrieves the Capcity Pins value.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pGetCapacityPins	 Pointer to the parameters structure needed
					 by GetCapacityPins().

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetCapacityPinsDef
UINT32 Oct6100ApiGetCapacityPinsDef(
				tPOCT6100_API_GET_CAPACITY_PINS			f_pGetCapacityPins)
{

	f_pGetCapacityPins->pProcessContext = NULL;
	f_pGetCapacityPins->ulUserChipId = 0;
	f_pGetCapacityPins->ulMemoryType = cOCT6100_MEM_TYPE_DDR;
	f_pGetCapacityPins->ulCapacityValue = cOCT6100_INVALID_VALUE;
	f_pGetCapacityPins->fEnableMemClkOut = TRUE;
	f_pGetCapacityPins->ulMemClkFreq = 133000000;
	
	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ApiGetCapacityPins
UINT32 Oct6100ApiGetCapacityPins(
				tPOCT6100_API_GET_CAPACITY_PINS		f_pGetCapacityPins  )
{

	UINT32					ulResult;

	tOCT6100_INSTANCE_API	ApiInstance;
	
	Oct6100UserMemSet(&ApiInstance,0,sizeof(tOCT6100_INSTANCE_API));

	/*Check parameters*/
	if ( f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_133_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_125_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_117_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_108_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_100_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_92_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_83_MHZ &&
		 f_pGetCapacityPins->ulMemClkFreq != cOCT6100_MCLK_FREQ_75_MHZ )
		return cOCT6100_ERR_OPEN_MEM_CLK_FREQ;

	if ( f_pGetCapacityPins->fEnableMemClkOut != TRUE &&
		 f_pGetCapacityPins->fEnableMemClkOut != FALSE )
		return cOCT6100_ERR_OPEN_ENABLE_MEM_CLK_OUT;

	if ( f_pGetCapacityPins->ulMemoryType != cOCT6100_MEM_TYPE_SDR &&
		 f_pGetCapacityPins->ulMemoryType != cOCT6100_MEM_TYPE_DDR &&
		 f_pGetCapacityPins->ulMemoryType != cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		return cOCT6100_ERR_OPEN_MEMORY_TYPE;
	

	
	ApiInstance.pProcessContext = f_pGetCapacityPins->pProcessContext;



	ulResult = Oct6100ApiReadCapacity(&ApiInstance, f_pGetCapacityPins);
	

	
	return ulResult;
}
#endif

/***************************  PRIVATE FUNCTIONS  *****************************/
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReadCapacity

Description:    Read the capacity pins using modified functions from the openchip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pChipOpen			Pointer to chip configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OCT6100ApiReadCapacity
UINT32 Oct6100ApiReadCapacity(	IN	tPOCT6100_INSTANCE_API f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS	f_pGetCapacityPins)
{
	UINT32					ulResult;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT16					usReadData;
	
	/*Read capacity Pins*/


	ReadParams.pProcessContext = f_pGetCapacityPins->pProcessContext;
	ReadParams.ulUserChipId = f_pGetCapacityPins->ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	/*Check the Reset register*/
	ReadParams.ulReadAddress = 0x100;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ((usReadData & 0xFFFF) != 0x0000)
		return cOCT6100_ERR_CAP_PINS_INVALID_CHIP_STATE;

	/* Test the CPU registers. */
	ulResult = Oct6100ApiCpuRegisterBistReadCap( f_pApiInstance, f_pGetCapacityPins );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Boot the FC2 PLL. */
	ulResult = Oct6100ApiBootFc2PllReadCap( f_pApiInstance,f_pGetCapacityPins );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Program the FC1 PLL. */
	ulResult = Oct6100ApiProgramFc1PllReadCap( f_pApiInstance,f_pGetCapacityPins );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
			
	if ( (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR) ||
		 (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS) )
	{
		ReadParams.ulReadAddress = 0x168;
	}
	else
		ReadParams.ulReadAddress = 0x166;
	
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
					
	switch (usReadData & 0xF)
	{
	case 0x9:
		f_pGetCapacityPins->ulCapacityValue = 16;
		break;
	case 0x8:
		f_pGetCapacityPins->ulCapacityValue = 32;
		break;
	case 0xE:
		f_pGetCapacityPins->ulCapacityValue = 64;
		break;
	case 0x0:
		f_pGetCapacityPins->ulCapacityValue = 128;
		break;
	case 0x2:
		f_pGetCapacityPins->ulCapacityValue = 256;
		break;
	case 0x5:
		f_pGetCapacityPins->ulCapacityValue = 512;
		break;
	case 0x6:
		f_pGetCapacityPins->ulCapacityValue = 672;
		break;
	default:
		f_pGetCapacityPins->ulCapacityValue = (usReadData & 0xF);
		return  cOCT6100_ERR_CAP_PINS_INVALID_CAPACITY_VALUE;
	}
	
	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChipConfiguration

Description:    Checks the user chip configuration structure for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pChipOpen			Pointer to chip configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChipConfiguration
UINT32 Oct6100ApiCheckChipConfiguration(
				IN		tPOCT6100_CHIP_OPEN		f_pChipOpen )
{
	UINT32	ulTempVar;
	UINT32	i;
	
	/*-----------------------------------------------------------------------------*/
	/* Check general parameters. */
	if ( f_pChipOpen->fMultiProcessSystem != TRUE &&
		 f_pChipOpen->fMultiProcessSystem != FALSE )
		return cOCT6100_ERR_OPEN_MULTI_PROCESS_SYSTEM;

	if ( f_pChipOpen->ulMaxRwAccesses < 1 ||
		 f_pChipOpen->ulMaxRwAccesses > 1024)
		return cOCT6100_ERR_OPEN_MAX_RW_ACCESSES;

	/* Check the clocks. */
	if ( f_pChipOpen->ulUpclkFreq != cOCT6100_UPCLK_FREQ_33_33_MHZ )
		return cOCT6100_ERR_OPEN_UP_CLK_FREQ;

	if ( f_pChipOpen->ulMemClkFreq != cOCT6100_MCLK_FREQ_133_MHZ )
		return cOCT6100_ERR_OPEN_MEM_CLK_FREQ;

	if ( f_pChipOpen->fEnableMemClkOut != TRUE &&
		 f_pChipOpen->fEnableMemClkOut != FALSE )
		return cOCT6100_ERR_OPEN_ENABLE_MEM_CLK_OUT;

	/* Check the image file. */
	if ( f_pChipOpen->ulImageSize < cOCT6100_MIN_IMAGE_SIZE ||
		 f_pChipOpen->ulImageSize > cOCT6100_MAX_IMAGE_SIZE )
		return cOCT6100_ERR_OPEN_IMAGE_SIZE;

	if ( f_pChipOpen->pbyImageFile == NULL )
		return cOCT6100_ERR_OPEN_IMAGE_FILE;

	ulTempVar = Oct6100ApiCheckImageFileHeader(f_pChipOpen);
	if (ulTempVar != cOCT6100_ERR_OK)
		return ulTempVar;

	/* Check the acoustic echo activation flag. */
	if ( f_pChipOpen->fEnableAcousticEcho != TRUE && 
		f_pChipOpen->fEnableAcousticEcho != FALSE )
		return cOCT6100_ERR_OPEN_ENABLE_ACOUSTIC_ECHO;

	/* Check the tail displacement parameter. */
	if ( f_pChipOpen->ulTailDisplacement > cOCT6100_MAX_TAIL_DISPLACEMENT )
		return cOCT6100_ERR_OPEN_TAIL_DISPLACEMENT;

	/*-----------------------------------------------------------------------------*/
	/* Check TDM bus configuration parameters. */
	for ( i = 0; i < 8; i++ )
	{
		if ( f_pChipOpen->aulTdmStreamFreqs[ i ] != cOCT6100_TDM_STREAM_FREQ_2MHZ &&
			 f_pChipOpen->aulTdmStreamFreqs[ i ] != cOCT6100_TDM_STREAM_FREQ_4MHZ &&
			 f_pChipOpen->aulTdmStreamFreqs[ i ] != cOCT6100_TDM_STREAM_FREQ_8MHZ)
			return cOCT6100_ERR_OPEN_TDM_STREAM_FREQS;
	}

	if ( f_pChipOpen->ulTdmSampling != cOCT6100_TDM_SAMPLE_AT_3_QUARTERS &&
		 f_pChipOpen->ulTdmSampling != cOCT6100_TDM_SAMPLE_AT_RISING_EDGE &&
		 f_pChipOpen->ulTdmSampling != cOCT6100_TDM_SAMPLE_AT_FALLING_EDGE )
		return cOCT6100_ERR_OPEN_TDM_SAMPLING;

	if ( f_pChipOpen->fEnableFastH100Mode != TRUE && 
		 f_pChipOpen->fEnableFastH100Mode != FALSE )
		return cOCT6100_ERR_OPEN_FAST_H100_MODE;

	/*-----------------------------------------------------------------------------*/
	/* Check external memory configuration parameters. */
	if ( f_pChipOpen->ulMemoryType != cOCT6100_MEM_TYPE_SDR &&
		 f_pChipOpen->ulMemoryType != cOCT6100_MEM_TYPE_DDR &&
		 f_pChipOpen->ulMemoryType != cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		return cOCT6100_ERR_OPEN_MEMORY_TYPE;
	
	if ( f_pChipOpen->ulMemoryChipSize != cOCT6100_MEMORY_CHIP_SIZE_8MB &&
		 f_pChipOpen->ulMemoryChipSize != cOCT6100_MEMORY_CHIP_SIZE_16MB &&
		 f_pChipOpen->ulMemoryChipSize != cOCT6100_MEMORY_CHIP_SIZE_32MB &&
		 f_pChipOpen->ulMemoryChipSize != cOCT6100_MEMORY_CHIP_SIZE_64MB &&
		 f_pChipOpen->ulMemoryChipSize != cOCT6100_MEMORY_CHIP_SIZE_128MB )
		return cOCT6100_ERR_OPEN_MEMORY_CHIP_SIZE;
	
	if ( f_pChipOpen->ulMemoryChipSize == cOCT6100_MEMORY_CHIP_SIZE_8MB &&
		 f_pChipOpen->ulMemoryType == cOCT6100_MEM_TYPE_DDR )
		return cOCT6100_ERR_OPEN_MEMORY_CHIP_SIZE;

	if ( f_pChipOpen->ulNumMemoryChips < 1 ||
		 f_pChipOpen->ulNumMemoryChips > cOCT6100_MAX_NUM_MEMORY_CHIP )
		return cOCT6100_ERR_OPEN_MEMORY_CHIPS_NUMBER;

	/* Check the total memory size. */
	ulTempVar = f_pChipOpen->ulMemoryChipSize * f_pChipOpen->ulNumMemoryChips;
	if ( ulTempVar < cOCT6100_MEMORY_CHIP_SIZE_16MB ||
		 ulTempVar > cOCT6100_MEMORY_CHIP_SIZE_128MB )
		return cOCT6100_ERR_OPEN_TOTAL_MEMORY_SIZE;

	if ( f_pChipOpen->ulMaxTdmStreams != 4 &&
		 f_pChipOpen->ulMaxTdmStreams != 8 &&
		 f_pChipOpen->ulMaxTdmStreams != 16 &&
		 f_pChipOpen->ulMaxTdmStreams != 32 )
		return cOCT6100_ERR_OPEN_MAX_TDM_STREAM;

	if ( f_pChipOpen->ulMaxTdmStreams > 8 && 
		 f_pChipOpen->ulMemClkFreq == cOCT6100_MCLK_FREQ_75_MHZ )
		return cOCT6100_ERR_OPEN_MAX_TDM_STREAM;

	if ( f_pChipOpen->fUseSynchTimestamp != TRUE &&
		 f_pChipOpen->fUseSynchTimestamp != FALSE )
		return cOCT6100_ERR_OPEN_USE_SYNCH_TIMESTAMP;

	if ( f_pChipOpen->fUseSynchTimestamp == TRUE )
	{
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_USE_SYNCH_TIMESTAMP;
	}

	/*-----------------------------------------------------------------------------*/
	/* Check soft buffer for tone events size. */
	if (f_pChipOpen->ulSoftToneEventsBufSize < 64 ||
		 f_pChipOpen->ulSoftToneEventsBufSize > cOCT6100_ABSOLUTE_MAX_NUM_PGSP_EVENT_OUT )
		return cOCT6100_ERR_OPEN_SOFT_TONE_EVENT_SIZE;

	if ( f_pChipOpen->fEnableExtToneDetection != TRUE && 
		 f_pChipOpen->fEnableExtToneDetection != FALSE )
		return cOCT6100_ERR_OPEN_ENABLE_EXT_TONE_DETECTION;

	if ( f_pChipOpen->fEnable2100StopEvent != TRUE &&
		 f_pChipOpen->fEnable2100StopEvent != FALSE)
		return cOCT6100_ERR_OPEN_ENABLE_2100_STOP_EVENT;

	/* Check soft buffer for playout events size. */
	if ( ( f_pChipOpen->ulSoftBufferPlayoutEventsBufSize != cOCT6100_INVALID_VALUE )
		&& ( f_pChipOpen->ulSoftBufferPlayoutEventsBufSize < cOCT6100_MIN_BUFFER_PLAYOUT_EVENT ||
		 f_pChipOpen->ulSoftBufferPlayoutEventsBufSize > cOCT6100_MAX_BUFFER_PLAYOUT_EVENT ) )
		return cOCT6100_ERR_OPEN_SOFT_PLAYOUT_STOP_EVENT_SIZE;

	/*-----------------------------------------------------------------------------*/
	/* Check interrupt configuration parameters. */
	if ( f_pChipOpen->ulInterruptPolarity != cOCT6100_ACTIVE_LOW_POLARITY &&
		 f_pChipOpen->ulInterruptPolarity != cOCT6100_ACTIVE_HIGH_POLARITY )
		return cOCT6100_ERR_OPEN_INTERRUPT_POLARITY;

	if ( f_pChipOpen->InterruptConfig.ulFatalGeneralConfig != cOCT6100_INTERRUPT_NO_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulFatalGeneralConfig != cOCT6100_INTERRUPT_DISABLE )
		return cOCT6100_ERR_OPEN_FATAL_GENERAL_CONFIG;

	if ( f_pChipOpen->InterruptConfig.ulFatalMemoryConfig != cOCT6100_INTERRUPT_NO_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulFatalMemoryConfig != cOCT6100_INTERRUPT_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulFatalMemoryConfig != cOCT6100_INTERRUPT_DISABLE )
		return cOCT6100_ERR_OPEN_FATAL_MEMORY_CONFIG;

	if ( f_pChipOpen->InterruptConfig.ulErrorMemoryConfig != cOCT6100_INTERRUPT_NO_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorMemoryConfig != cOCT6100_INTERRUPT_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorMemoryConfig != cOCT6100_INTERRUPT_DISABLE )
		return cOCT6100_ERR_OPEN_ERROR_MEMORY_CONFIG;

	if ( f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_NO_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_DISABLE )
		return cOCT6100_ERR_OPEN_ERROR_OVERFLOW_TONE_EVENTS_CONFIG;

	if ( f_pChipOpen->InterruptConfig.ulErrorH100Config != cOCT6100_INTERRUPT_NO_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorH100Config != cOCT6100_INTERRUPT_TIMEOUT &&
		 f_pChipOpen->InterruptConfig.ulErrorH100Config != cOCT6100_INTERRUPT_DISABLE )
		return cOCT6100_ERR_OPEN_ERROR_H100_CONFIG;

	/* Check the timeout value. */
	if ( f_pChipOpen->InterruptConfig.ulFatalMemoryTimeout < 10 ||
		 f_pChipOpen->InterruptConfig.ulFatalMemoryTimeout > 10000 )
		return cOCT6100_ERR_OPEN_FATAL_MEMORY_TIMEOUT;

	if ( f_pChipOpen->InterruptConfig.ulErrorMemoryTimeout < 10 ||
		 f_pChipOpen->InterruptConfig.ulErrorMemoryTimeout > 10000 )
		return cOCT6100_ERR_OPEN_ERROR_MEMORY_TIMEOUT;

	if ( f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsTimeout < 10 ||
		 f_pChipOpen->InterruptConfig.ulErrorOverflowToneEventsTimeout > 10000 )
		return cOCT6100_ERR_OPEN_ERROR_OVERFLOW_TONE_EVENTS_TIMEOUT;

	if ( f_pChipOpen->InterruptConfig.ulErrorH100Timeout < 10 ||
		 f_pChipOpen->InterruptConfig.ulErrorH100Timeout > 10000 )
		return cOCT6100_ERR_OPEN_ERROR_H100_TIMEOUT;

	/*-----------------------------------------------------------------------------*/
	/* Check maximum resources. */

	switch ( f_pChipOpen->ulMemClkFreq )
	{
	case 133000000:
		ulTempVar = 672;
		break;
	case 125000000:
		ulTempVar = 624;
		break;
	case 117000000:
		ulTempVar = 576;
		break;
	case 108000000:
		ulTempVar = 528;
		break;
	case 100000000:
		ulTempVar = 480;
		break;
	case 92000000:
		ulTempVar = 432;
		break;
	case 83000000:
		ulTempVar = 384;
		break;
	case 75000000:
		ulTempVar = 336;
		break;
	default:
		return cOCT6100_ERR_FATAL_DA;
	}

	if ( f_pChipOpen->ulMaxChannels > ulTempVar )
		return cOCT6100_ERR_OPEN_MAX_ECHO_CHANNELS;

	if ( f_pChipOpen->ulMaxTsiCncts > cOCT6100_MAX_TSI_CNCTS )
		return cOCT6100_ERR_OPEN_MAX_TSI_CNCTS;


	if ( f_pChipOpen->ulMaxBiDirChannels > 255 )
		return cOCT6100_ERR_OPEN_MAX_BIDIR_CHANNELS;
	
	if ( f_pChipOpen->ulMaxBiDirChannels > (f_pChipOpen->ulMaxChannels / 2) )
		return cOCT6100_ERR_OPEN_MAX_BIDIR_CHANNELS;

	if ( f_pChipOpen->ulMaxConfBridges > cOCT6100_MAX_CONF_BRIDGE )
		return cOCT6100_ERR_OPEN_MAX_CONF_BRIDGES;

	if ( f_pChipOpen->ulMaxFlexibleConfParticipants > cOCT6100_MAX_FLEX_CONF_PARTICIPANTS )
		return cOCT6100_ERR_OPEN_MAX_FLEXIBLE_CONF_PARTICIPANTS;

	if ( f_pChipOpen->ulMaxPlayoutBuffers > cOCT6100_MAX_PLAYOUT_BUFFERS )
		return cOCT6100_ERR_OPEN_MAX_PLAYOUT_BUFFERS;



	if ( f_pChipOpen->ulMaxPhasingTssts > cOCT6100_MAX_PHASING_TSST )
		return cOCT6100_ERR_OPEN_MAX_PHASING_TSSTS;

	if ( f_pChipOpen->ulMaxAdpcmChannels > cOCT6100_MAX_ADPCM_CHANNELS )
		return cOCT6100_ERR_OPEN_MAX_ADPCM_CHANNELS;

	if ( f_pChipOpen->ulMaxRemoteDebugSessions > 256 )
		return cOCT6100_ERR_OPEN_MAX_REMOTE_DEBUG_SESSIONS;





	/* Check the channel recording flag. */
	if ( f_pChipOpen->fEnableChannelRecording != TRUE &&
		 f_pChipOpen->fEnableChannelRecording != FALSE )
		return cOCT6100_ERR_OPEN_DEBUG_CHANNEL_RECORDING;

	/* Check the enable production BIST flag. */
	if ( ( f_pChipOpen->fEnableProductionBist != TRUE )
		&& ( f_pChipOpen->fEnableProductionBist != FALSE ) )
		return cOCT6100_ERR_OPEN_ENABLE_PRODUCTION_BIST;

	/* Check number of loops for the production BIST. */
	if ( f_pChipOpen->fEnableProductionBist == TRUE )
	{
		if ( f_pChipOpen->ulNumProductionBistLoops == 0 )
			return cOCT6100_ERR_OPEN_NUM_PRODUCTION_BIST_LOOPS;

		if ( (f_pChipOpen->ulProductionBistMode != cOCT6100_PRODUCTION_BIST_STANDARD) &&
			 (f_pChipOpen->ulProductionBistMode != cOCT6100_PRODUCTION_BIST_SHORT) )
			return cOCT6100_ERR_OPEN_PRODUCTION_BIST_MODE;
	}

	/* If the production BIST has been requested, make sure all */
	/* other resources are disabled. */
	if ( f_pChipOpen->fEnableProductionBist == TRUE )
	{
		/* All must be disabled. */
		f_pChipOpen->ulMaxChannels = 0;
		f_pChipOpen->ulMaxTsiCncts = 0;
		f_pChipOpen->fEnableChannelRecording = FALSE;
		f_pChipOpen->ulMaxBiDirChannels = 0;
		f_pChipOpen->ulMaxConfBridges = 0;
		f_pChipOpen->ulMaxPlayoutBuffers = 0;
		f_pChipOpen->ulSoftBufferPlayoutEventsBufSize = cOCT6100_INVALID_VALUE;
		f_pChipOpen->ulMaxPhasingTssts = 0;
		f_pChipOpen->ulMaxAdpcmChannels = 0;


	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCopyChipConfiguration

Description:    Copies the chip configuration from the user supplied config
				structure to the instance structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pChipOpen				Pointer to chip configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCopyChipConfiguration
UINT32 Oct6100ApiCopyChipConfiguration(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		tPOCT6100_CHIP_OPEN		f_pChipOpen )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	pSharedInfo->ChipConfig.ulUserChipId = f_pChipOpen->ulUserChipId;
	pSharedInfo->ChipConfig.fMultiProcessSystem = (UINT8)( f_pChipOpen->fMultiProcessSystem & 0xFF );

	pSharedInfo->ChipConfig.usMaxRwAccesses = (UINT16)( f_pChipOpen->ulMaxRwAccesses & 0xFFFF );

	pSharedInfo->ChipConfig.pbyImageFile = f_pChipOpen->pbyImageFile;
	pSharedInfo->ChipConfig.ulImageSize = f_pChipOpen->ulImageSize;	
	
	pSharedInfo->ChipConfig.ulMemClkFreq = f_pChipOpen->ulMemClkFreq;
	pSharedInfo->ChipConfig.ulUpclkFreq = f_pChipOpen->ulUpclkFreq;

	pSharedInfo->ChipConfig.byMemoryType = (UINT8)( f_pChipOpen->ulMemoryType & 0xFF );
	pSharedInfo->ChipConfig.byNumMemoryChips = (UINT8)( f_pChipOpen->ulNumMemoryChips & 0xFF );
	pSharedInfo->ChipConfig.ulMemoryChipSize = f_pChipOpen->ulMemoryChipSize;

	pSharedInfo->ChipConfig.usTailDisplacement = (UINT16)( f_pChipOpen->ulTailDisplacement & 0xFFFF );
	pSharedInfo->ChipConfig.fEnableAcousticEcho = (UINT8)( f_pChipOpen->fEnableAcousticEcho & 0xFF );
	/* Resource allocation parameters. */
	if ( f_pChipOpen->fEnableChannelRecording == TRUE && f_pChipOpen->ulMaxChannels == 672 )
		pSharedInfo->ChipConfig.usMaxChannels = (UINT16)( ( f_pChipOpen->ulMaxChannels - 1 ) & 0xFFFF );
	else
		pSharedInfo->ChipConfig.usMaxChannels = (UINT16)( f_pChipOpen->ulMaxChannels & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxTsiCncts = (UINT16)( f_pChipOpen->ulMaxTsiCncts & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxBiDirChannels = (UINT16)( f_pChipOpen->ulMaxBiDirChannels & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxConfBridges = (UINT16)( f_pChipOpen->ulMaxConfBridges & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxFlexibleConfParticipants = (UINT16)( f_pChipOpen->ulMaxFlexibleConfParticipants & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxPlayoutBuffers = (UINT16)( f_pChipOpen->ulMaxPlayoutBuffers & 0xFFFF );

	pSharedInfo->ChipConfig.usMaxPhasingTssts = (UINT16)( f_pChipOpen->ulMaxPhasingTssts & 0xFFFF );
	pSharedInfo->ChipConfig.usMaxAdpcmChannels = (UINT16)( f_pChipOpen->ulMaxAdpcmChannels & 0xFFFF );
	pSharedInfo->ChipConfig.byMaxTdmStreams = (UINT8)( f_pChipOpen->ulMaxTdmStreams & 0xFF );
	pSharedInfo->ChipConfig.fUseSynchTimestamp = (UINT8)( f_pChipOpen->fUseSynchTimestamp & 0xFF );
	for ( i = 0; i < 4; i++ )
	{
		pSharedInfo->ChipConfig.ausTimestampTimeslots[ i ] = (UINT16)( f_pChipOpen->aulTimestampTimeslots[ i ] & 0xFFFF );
		pSharedInfo->ChipConfig.ausTimestampStreams[ i ]  = (UINT16)( f_pChipOpen->aulTimestampStreams[ i ] & 0xFFFF );
	}
	pSharedInfo->ChipConfig.byInterruptPolarity = (UINT8)( f_pChipOpen->ulInterruptPolarity & 0xFF );

	pSharedInfo->ChipConfig.byTdmSampling = (UINT8)( f_pChipOpen->ulTdmSampling & 0xFF );
	pSharedInfo->ChipConfig.fEnableFastH100Mode = (UINT8)( f_pChipOpen->fEnableFastH100Mode & 0xFF );

	for ( i = 0; i < cOCT6100_TDM_STREAM_MAX_GROUPS; i++ )
	{
		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->ChipConfig.aulTdmStreamFreqs[ i ] = cOCT6100_TDM_STREAM_FREQ_16MHZ;
		else
			pSharedInfo->ChipConfig.aulTdmStreamFreqs[ i ] = f_pChipOpen->aulTdmStreamFreqs[ i ];
	}
	
	pSharedInfo->ChipConfig.fEnableFastH100Mode = (UINT8)( f_pChipOpen->fEnableFastH100Mode & 0xFF );
	pSharedInfo->ChipConfig.fEnableMemClkOut = (UINT8)( f_pChipOpen->fEnableMemClkOut & 0xFF );

	/* Add 1 to the circular buffer such that all user requested events can fit in the circular queue. */
	pSharedInfo->ChipConfig.ulSoftToneEventsBufSize = f_pChipOpen->ulSoftToneEventsBufSize + 1;
	pSharedInfo->ChipConfig.fEnableExtToneDetection = (UINT8)( f_pChipOpen->fEnableExtToneDetection & 0xFF );
	pSharedInfo->ChipConfig.fEnable2100StopEvent = (UINT8)( f_pChipOpen->fEnable2100StopEvent & 0xFF );

	if ( f_pChipOpen->ulSoftBufferPlayoutEventsBufSize != cOCT6100_INVALID_VALUE )
		pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize = f_pChipOpen->ulSoftBufferPlayoutEventsBufSize + 1;
	else
		pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize = 0;
	pSharedInfo->ChipConfig.usMaxRemoteDebugSessions = (UINT16)( f_pChipOpen->ulMaxRemoteDebugSessions & 0xFFFF );

	pSharedInfo->ChipConfig.fEnableChannelRecording = (UINT8)( f_pChipOpen->fEnableChannelRecording & 0xFF );



	pSharedInfo->ChipConfig.fEnableProductionBist = (UINT8)( f_pChipOpen->fEnableProductionBist & 0xFF );
	pSharedInfo->ChipConfig.ulProductionBistMode = f_pChipOpen->ulProductionBistMode;
	pSharedInfo->ChipConfig.ulNumProductionBistLoops  = f_pChipOpen->ulNumProductionBistLoops;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitializeMiscellaneousVariables

Description:    Function where all the various parameters from the API instance 
				are set to their defaults value.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitializeMiscellaneousVariables
UINT32 Oct6100ApiInitializeMiscellaneousVariables(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32	i;

	/* Obtain pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Calculate the total memory available. */
	pSharedInfo->MiscVars.ulTotalMemSize = pSharedInfo->ChipConfig.ulMemoryChipSize * pSharedInfo->ChipConfig.byNumMemoryChips;

	/* Software buffers initialization. */

	/* Tones */
	pSharedInfo->SoftBufs.ulToneEventBufferWritePtr = 0;
	pSharedInfo->SoftBufs.ulToneEventBufferReadPtr = 0;
	pSharedInfo->SoftBufs.ulToneEventBufferSize = pSharedInfo->ChipConfig.ulSoftToneEventsBufSize;
	pSharedInfo->SoftBufs.ulToneEventBufferOverflowCnt = 0;

	/* Playout */
	pSharedInfo->SoftBufs.ulBufPlayoutEventBufferWritePtr = 0;
	pSharedInfo->SoftBufs.ulBufPlayoutEventBufferReadPtr = 0;
	pSharedInfo->SoftBufs.ulBufPlayoutEventBufferSize = pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize;
	pSharedInfo->SoftBufs.ulBufPlayoutEventBufferOverflowCnt = 0;

	/* Set the number of conference bridges opened to zero. */
	pSharedInfo->MiscVars.usNumBridgesOpened = 0;
	pSharedInfo->MiscVars.usFirstBridge = cOCT6100_INVALID_INDEX;
	
	/* Set the H.100 slave mode. */
	pSharedInfo->MiscVars.ulH100SlaveMode = cOCT6100_H100_TRACKA;

	/* Save the Mclk value.*/
	pSharedInfo->MiscVars.ulMclkFreq = pSharedInfo->ChipConfig.ulMemClkFreq;

	/* Init the NLP params. */
	pSharedInfo->MiscVars.usCodepoint = 0;
	pSharedInfo->MiscVars.usCpuLsuWritePtr = 0;

	/* Pouch counter not present until TLVs are read. */
	pSharedInfo->DebugInfo.fPouchCounter = FALSE;
	pSharedInfo->DebugInfo.fIsIsrCalledField = FALSE;

	/* Initialize the image info parameters */
	pSharedInfo->ImageInfo.fAdaptiveNoiseReduction		= FALSE;
	pSharedInfo->ImageInfo.fSoutNoiseBleaching			= FALSE;
	pSharedInfo->ImageInfo.fComfortNoise				= FALSE;
	pSharedInfo->ImageInfo.fBufferPlayout				= TRUE;
	pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip	= FALSE; 
	pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip	= FALSE;
	pSharedInfo->ImageInfo.fNlpControl					= FALSE;
	pSharedInfo->ImageInfo.fRinAutoLevelControl			= FALSE;
	pSharedInfo->ImageInfo.fSoutAutoLevelControl		= FALSE;
	pSharedInfo->ImageInfo.fRinHighLevelCompensation	= FALSE;
	pSharedInfo->ImageInfo.fSoutHighLevelCompensation	= FALSE;
	pSharedInfo->ImageInfo.fAlcHlcStatus				= FALSE;
	pSharedInfo->ImageInfo.fRinDcOffsetRemoval			= FALSE;
	pSharedInfo->ImageInfo.fSilenceSuppression			= FALSE;
	pSharedInfo->ImageInfo.fSinDcOffsetRemoval			= FALSE;
	pSharedInfo->ImageInfo.fToneDisabler				= FALSE;
	pSharedInfo->ImageInfo.fAdpcm						= FALSE;
	pSharedInfo->ImageInfo.fTailDisplacement			= FALSE;
	pSharedInfo->ImageInfo.fConferencing				= FALSE;
	pSharedInfo->ImageInfo.fConferencingNoiseReduction	= FALSE;
	pSharedInfo->ImageInfo.fDominantSpeakerEnabled		= FALSE;
	pSharedInfo->ImageInfo.fAecEnabled					= FALSE;
	pSharedInfo->ImageInfo.fAcousticEcho				= FALSE;
	pSharedInfo->ImageInfo.fToneRemoval					= FALSE;

	pSharedInfo->ImageInfo.fDefaultErl					= FALSE;
	pSharedInfo->ImageInfo.fMaxEchoPoint				= FALSE;
	pSharedInfo->ImageInfo.fNonLinearityBehaviorA		= FALSE;
	pSharedInfo->ImageInfo.fNonLinearityBehaviorB		= FALSE;
	pSharedInfo->ImageInfo.fPerChannelTailDisplacement	= FALSE;
	pSharedInfo->ImageInfo.fPerChannelTailLength		= FALSE;
	pSharedInfo->ImageInfo.fAfTailDisplacement			= FALSE;
	pSharedInfo->ImageInfo.fMusicProtection				= FALSE;
	pSharedInfo->ImageInfo.fAftControl					= FALSE;
	pSharedInfo->ImageInfo.fSinVoiceDetectedStat		= FALSE;
	pSharedInfo->ImageInfo.fRinAppliedGainStat			= FALSE;
	pSharedInfo->ImageInfo.fSoutAppliedGainStat			= FALSE;
	pSharedInfo->ImageInfo.fListenerEnhancement			= FALSE;
	pSharedInfo->ImageInfo.fRoutNoiseReduction			= FALSE;
	pSharedInfo->ImageInfo.fRoutNoiseReductionLevel		= FALSE;
	pSharedInfo->ImageInfo.fAnrSnrEnhancement			= FALSE;
	pSharedInfo->ImageInfo.fAnrVoiceNoiseSegregation	= FALSE;
	pSharedInfo->ImageInfo.fRinMute						= FALSE;
	pSharedInfo->ImageInfo.fSinMute						= FALSE;
	pSharedInfo->ImageInfo.fToneDisablerVqeActivationDelay = FALSE;
	pSharedInfo->ImageInfo.fAecTailLength				= FALSE;
	pSharedInfo->ImageInfo.fMusicProtectionConfiguration= FALSE;
	pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents	= FALSE;
	pSharedInfo->ImageInfo.fRinEnergyStat				= FALSE;
	pSharedInfo->ImageInfo.fSoutEnergyStat				= FALSE;
	pSharedInfo->ImageInfo.fDoubleTalkBehavior			= FALSE;
	pSharedInfo->ImageInfo.fDoubleTalkBehaviorFieldOfst	= FALSE;
	pSharedInfo->ImageInfo.fIdleCodeDetection			= TRUE;
	pSharedInfo->ImageInfo.fIdleCodeDetectionConfiguration = FALSE;
	pSharedInfo->ImageInfo.fSinLevel					= TRUE;

	pSharedInfo->ImageInfo.usMaxNumberOfChannels		= 0;
	pSharedInfo->ImageInfo.ulToneProfileNumber			= cOCT6100_INVALID_VALUE;
	pSharedInfo->ImageInfo.ulBuildId					= cOCT6100_INVALID_VALUE;
	pSharedInfo->ImageInfo.byImageType					= cOCT6100_IMAGE_TYPE_WIRELINE;
	pSharedInfo->ImageInfo.usMaxTailDisplacement		= 0;
	pSharedInfo->ImageInfo.usMaxTailLength				= cOCT6100_TAIL_LENGTH_128MS;
	pSharedInfo->DebugInfo.ulDebugEventSize				= 0x100;
	pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents		= 32;
	pSharedInfo->DebugInfo.ulMatrixBaseAddress			= cOCT6100_MATRIX_DWORD_BASE;
	pSharedInfo->DebugInfo.ulDebugChanStatsByteSize		= cOCT6100_DEBUG_CHAN_STATS_EVENT_BYTE_SIZE;
	pSharedInfo->DebugInfo.ulDebugChanLiteStatsByteSize	= cOCT6100_DEBUG_CHAN_STATS_LITE_EVENT_BYTE_SIZE;
	pSharedInfo->DebugInfo.ulHotChannelSelectBaseAddress= cOCT6100_MATRIX_CHAN_SELECT_DWORD_ADD;
	pSharedInfo->DebugInfo.ulMatrixTimestampBaseAddress = cOCT6100_MATRIX_TIMESTAMP_DWORD_ADD;
	pSharedInfo->DebugInfo.ulMatrixWpBaseAddress		= cOCT6100_MATRIX_WRITE_PTR_DWORD_ADD;
	pSharedInfo->DebugInfo.ulAfWritePtrByteOffset		= 206;
	pSharedInfo->DebugInfo.ulRecordedPcmEventByteSize	= 4096;
	pSharedInfo->DebugInfo.ulAfEventCbByteSize			= 0x100000;

	/* Set all tones to invalid. */
	pSharedInfo->ImageInfo.byNumToneDetectors = 0;
	for ( i = 0; i < cOCT6100_MAX_TONE_EVENT; i++ )
	{
		pSharedInfo->ImageInfo.aToneInfo[ i ].ulToneID = cOCT6100_INVALID_VALUE;
		pSharedInfo->ImageInfo.aToneInfo[ i ].ulDetectionPort = cOCT6100_INVALID_PORT;
		Oct6100UserMemSet( pSharedInfo->ImageInfo.aToneInfo[ i ].aszToneName, 0x00, cOCT6100_TLV_MAX_TONE_NAME_SIZE );
	}
	/* Initialize the channel recording info. */
	pSharedInfo->DebugInfo.usRecordChanIndex = pSharedInfo->ChipConfig.usMaxChannels;
	pSharedInfo->DebugInfo.usRecordMemIndex = cOCT6100_INVALID_INDEX;
	
	pSharedInfo->DebugInfo.usCurrentDebugChanIndex = cOCT6100_INVALID_INDEX;
	/* Initialize the mixer information. */
	pSharedInfo->MixerInfo.usFirstBridgeEventPtr	= cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usFirstSinCopyEventPtr	= cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr  = cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usLastBridgeEventPtr		= cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usLastSinCopyEventPtr	= cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usLastSoutCopyEventPtr	= cOCT6100_INVALID_INDEX;
	
	pSharedInfo->MixerInfo.usRecordCopyEventIndex	= cOCT6100_INVALID_INDEX;
	pSharedInfo->MixerInfo.usRecordSinEventIndex	= cOCT6100_INVALID_INDEX;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCalculateInstanceSizes

Description:    Calculates the amount of memory needed for the instance
				structure memory block based on the user's configuration.  

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pChipOpen			Pointer to user chip configuration structure.

f_pInstSizes		Pointer to structure containing the size of memory needed
					by all pointers internal to the API instance.  The memory
					is needed to keep track of the present state of all the
					chip's resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCalculateInstanceSizes
UINT32 Oct6100ApiCalculateInstanceSizes(
				IN OUT	tPOCT6100_CHIP_OPEN				f_pChipOpen,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulApiInstProcessSpecific;
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Start with all instance sizes set to 0. */
	Oct6100UserMemSet( f_pInstSizes, 0x00, sizeof( tOCT6100_API_INSTANCE_SIZES ) );

	/* All memory sizes are rounded up to the next multiple of 64 bytes. */

	/*-----------------------------------------------------------------------------*/
	/* Obtain size of static members of API instance. */
	f_pInstSizes->ulApiInstStatic = sizeof( tOCT6100_SHARED_INFO );
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulApiInstStatic, ulTempVar )

	/* Calculate memory needed by pointers internal to the API instance. */

	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for the EC channels. */
	ulResult = Oct6100ApiGetChannelsEchoSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Memory needed by the TSI structures. */
	ulResult = Oct6100ApiGetTsiCnctSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for the conference bridges. */
	ulResult = Oct6100ApiGetConfBridgeSwSizes( f_pChipOpen, f_pInstSizes );
	/* Calculate memory needed for list and allocation software serialization. */
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Memory needed by the buffer playout structures. */
	ulResult = Oct6100ApiGetPlayoutBufferSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Memory needed by soft Rx Event buffers. */
	ulResult = Oct6100ApiGetEventsSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for phasing tssts. */
	ulResult = Oct6100ApiGetPhasingTsstSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for the ADPCM channels. */
	ulResult = Oct6100ApiGetAdpcmChanSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for the management of TSSTs. */
	ulResult = Oct6100ApiGetTsstSwSizes( f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Calculate memory needed for the management of the mixer. */
	ulResult = Oct6100ApiGetMixerSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Determine amount of memory needed for memory allocation softwares.  These
		pieces of software will be responsible for the allocation of the chip's
		external memory and API memory. */
	ulResult = Oct6100ApiGetMemorySwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*-----------------------------------------------------------------------------*/
	/* Memory needed for remote debugging sessions. */
	ulResult = Oct6100ApiGetRemoteDebugSwSizes( f_pChipOpen, f_pInstSizes );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*-----------------------------------------------------------------------------*/
	/* Calculate total memory needed by pointers internal to API instance.  The
		total contains both the process specific portion of the instance
		(tOCT6100_INSTANCE_API) and the shared portion (tOCT6100_SHARED_INFO).  The
		process specific portion will be used only in the case where the host system
		is a single-process one. */

	ulApiInstProcessSpecific = sizeof( tOCT6100_INSTANCE_API );
	mOCT6100_ROUND_MEMORY_SIZE( ulApiInstProcessSpecific, ulTempVar )
	f_pInstSizes->ulApiInstTotal = 
									f_pInstSizes->ulChannelList +
									f_pInstSizes->ulChannelAlloc +
									f_pInstSizes->ulTsiCnctList +
									f_pInstSizes->ulTsiCnctAlloc +
									f_pInstSizes->ulSoftToneEventsBuffer + 
									f_pInstSizes->ulSoftBufPlayoutEventsBuffer +
									f_pInstSizes->ulBiDirChannelList +
									f_pInstSizes->ulBiDirChannelAlloc +
									f_pInstSizes->ulConfBridgeList +
									f_pInstSizes->ulConfBridgeAlloc +
									f_pInstSizes->ulFlexConfParticipantsList +
									f_pInstSizes->ulFlexConfParticipantsAlloc +
									f_pInstSizes->ulPlayoutBufList +
									f_pInstSizes->ulPlayoutBufAlloc +
									f_pInstSizes->ulPlayoutBufMemoryNodeList +

									f_pInstSizes->ulCopyEventList +
									f_pInstSizes->ulCopyEventAlloc +
									f_pInstSizes->ulMixerEventList +
									f_pInstSizes->ulMixerEventAlloc +
									f_pInstSizes->ulPhasingTsstList +
									f_pInstSizes->ulPhasingTsstAlloc +
									f_pInstSizes->ulAdpcmChannelList + 
									f_pInstSizes->ulAdpcmChannelAlloc + 
									f_pInstSizes->ulConversionMemoryAlloc +
									f_pInstSizes->ulTsiMemoryAlloc +
									f_pInstSizes->ulRemoteDebugList +
									f_pInstSizes->ulRemoteDebugTree +
									f_pInstSizes->ulRemoteDebugPktCache +
									f_pInstSizes->ulRemoteDebugDataBuf +
									f_pInstSizes->ulTsstEntryList +
									f_pInstSizes->ulTsstEntryAlloc +									
									f_pInstSizes->ulTsstAlloc + 
									f_pInstSizes->ulApiInstStatic + 
									ulApiInstProcessSpecific;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAllocateInstanceMemory

Description:    Allocates the API instance memory to the various members of
				the structure f_pApiInstance according to the sizes contained
				in f_pInstSizes.  No initialization of this memory is
				performed.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_pInstSizes		Pointer to structure containing the size of memory needed
					by all pointers internal to the API instance.  The memory
					is needed to keep track of the present state of all the
					chip's resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAllocateInstanceMemory
UINT32 Oct6100ApiAllocateInstanceMemory(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulOffset;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get address of first UINT32 of memory in API instance structure following */
	/* the static members of the API instance structure. */
	ulOffset = f_pInstSizes->ulApiInstStatic;

	/*===================================================================*/
	/* Allocate memory for the echo channels.*/
	pSharedInfo->ulChannelListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulChannelList;
	pSharedInfo->ulChannelAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulChannelAlloc;

	/*===================================================================*/
	/* Allocate memory for the TSI connections */
	pSharedInfo->ulTsiCnctListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsiCnctList;
	pSharedInfo->ulTsiCnctAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsiCnctAlloc;
	pSharedInfo->ulMixerEventListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulMixerEventList;
	pSharedInfo->ulMixerEventAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulMixerEventAlloc;

	pSharedInfo->ulBiDirChannelListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulBiDirChannelList;
	pSharedInfo->ulBiDirChannelAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulBiDirChannelAlloc;
	pSharedInfo->ulCopyEventListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulCopyEventList;
	pSharedInfo->ulCopyEventAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulCopyEventAlloc;

	/*===================================================================*/
	/* Allocate memory for the conference bridges */
	pSharedInfo->ulConfBridgeListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulConfBridgeList;
	pSharedInfo->ulConfBridgeAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulConfBridgeAlloc;

	/*===================================================================*/
	/* Allocate memory for the flexible conferencing participants. */
	pSharedInfo->ulFlexConfParticipantListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulFlexConfParticipantsList;
	pSharedInfo->ulFlexConfParticipantAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulFlexConfParticipantsAlloc;

	/*===================================================================*/
	/* Allocate memory for the play-out buffers */
	pSharedInfo->ulPlayoutBufListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulPlayoutBufList;
	pSharedInfo->ulPlayoutBufAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulPlayoutBufAlloc;
	pSharedInfo->ulPlayoutBufMemoryNodeListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulPlayoutBufMemoryNodeList;


	
	/*===================================================================*/
	/* Allocate memory for the phasing TSSTs */
	pSharedInfo->ulPhasingTsstListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulPhasingTsstList;
	pSharedInfo->ulPhasingTsstAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulPhasingTsstAlloc;

	/*===================================================================*/
	/* Allocate memory for the ADPCM channel */
	pSharedInfo->ulAdpcmChanAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulAdpcmChannelAlloc;
	pSharedInfo->ulAdpcmChanListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulAdpcmChannelList;
	
	/*===================================================================*/
	/* Allocate memory for the conversion memory */
	pSharedInfo->ulConversionMemoryAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulConversionMemoryAlloc;

	/*===================================================================*/
	/* Allocate memory for the TSI chariot memory */
	pSharedInfo->ulTsiMemoryAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsiMemoryAlloc;
	
	/*===================================================================*/
	/* Allocate memory for the TSST management */
	pSharedInfo->ulTsstAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsstAlloc;
	pSharedInfo->ulTsstListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsstEntryList;
	pSharedInfo->ulTsstListAllocOfst = ulOffset;
	ulOffset += f_pInstSizes->ulTsstEntryAlloc;

	/*===================================================================*/
	pSharedInfo->SoftBufs.ulToneEventBufferMemOfst = ulOffset;
	ulOffset += f_pInstSizes->ulSoftToneEventsBuffer;

	pSharedInfo->SoftBufs.ulBufPlayoutEventBufferMemOfst = ulOffset;
	ulOffset += f_pInstSizes->ulSoftBufPlayoutEventsBuffer;
	/*===================================================================*/
	pSharedInfo->RemoteDebugInfo.ulSessionListOfst = ulOffset;
	ulOffset += f_pInstSizes->ulRemoteDebugList;

	pSharedInfo->RemoteDebugInfo.ulSessionTreeOfst = ulOffset;
	ulOffset += f_pInstSizes->ulRemoteDebugTree;

	pSharedInfo->RemoteDebugInfo.ulDataBufOfst = ulOffset;
	ulOffset += f_pInstSizes->ulRemoteDebugDataBuf;

	pSharedInfo->RemoteDebugInfo.ulPktCacheOfst = ulOffset;
	ulOffset += f_pInstSizes->ulRemoteDebugPktCache;
	/*===================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitializeInstanceMemory

Description:    Initializes the various members of the structure f_pApiInstance
				to reflect the current state of the chip and its resources.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitializeInstanceMemory
UINT32 Oct6100ApiInitializeInstanceMemory(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	UINT32	ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize API EC channels. */
	ulResult = Oct6100ApiChannelsEchoSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Initialize the API TSI connection structures. */
	ulResult = Oct6100ApiTsiCnctSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Initialize the API conference bridges. */
	ulResult = Oct6100ApiConfBridgeSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize the API buffer playout structures. */
	ulResult = Oct6100ApiPlayoutBufferSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize the API phasing tssts. */
	ulResult = Oct6100ApiPhasingTsstSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*-----------------------------------------------------------------------------*/
	/* Initialize the API ADPCM channels. */
	ulResult = Oct6100ApiAdpcmChanSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize the external memory management structures. */
	ulResult = Oct6100ApiMemorySwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize TSST management stuctures. */
	ulResult = Oct6100ApiTsstSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize the mixer management stuctures. */
	ulResult = Oct6100ApiMixerSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Initialize the remote debugging session management variables. */
	ulResult = Oct6100ApiRemoteDebuggingSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*-----------------------------------------------------------------------------*/
	/* Configure the interrupt registers. */
	ulResult = Oct6100ApiIsrSwInit( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetChipRevisionNum

Description:    Reads the chip's revision number register.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetChipRevisionNum
UINT32 Oct6100ApiGetChipRevisionNum(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32					ulResult;
	UINT16					usReadData;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get the chip revision number. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.ulReadAddress = cOCT6100_CHIP_ID_REVISION_REG;
	ReadParams.pusReadData = &usReadData;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Save the info in the API miscellaneous structure. */
	pSharedInfo->MiscVars.usChipId = (UINT16)( usReadData & 0xFF );
	pSharedInfo->MiscVars.usChipRevision = (UINT16)( usReadData >> 8 );

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckImageFileHeader

Description:    This function check if the image loaded is valid

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckImageFileHeader
UINT32 Oct6100ApiCheckImageFileHeader(
				IN		tPOCT6100_CHIP_OPEN		f_pChipOpen )
{
	
	PUINT8	pszImageInfoStart = NULL;
	UINT32	ulStrLen;
	
	ulStrLen = Oct6100ApiStrLen( (PUINT8)cOCT6100_IMAGE_START_STRING );
	pszImageInfoStart = (PUINT8) Oct6100ApiStrStr(f_pChipOpen->pbyImageFile,(PUINT8)cOCT6100_IMAGE_START_STRING,
										 f_pChipOpen->pbyImageFile + ulStrLen);
	if (pszImageInfoStart == NULL)
		return cOCT6100_ERR_OPEN_IMAGE_FILE;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiDecodeKeyAndBist

Description:    This function decodes the key and runs the automatic BIST.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiDecodeKeyAndBist
UINT32 Oct6100ApiDecodeKeyAndBist(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT16						ausBistData[ 3 ];
	UINT16						usReadData;
	UINT32						ulResult;
	BOOL						fBitEqual;
	UINT32						i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain a local pointer to the chip config structure */
	/* contained in the instance structure. */
	pChipConfig = &pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pChipConfig->ulUserChipId;

	/* Set the process context and user chip ID parameters once and for all. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pChipConfig->ulUserChipId;

	/* Write key in CPU internal memory. */
	for(i=0; i<8; i++)
	{
		WriteParams.ulWriteAddress = 0x150;
		WriteParams.usWriteData = 0x0000;
		if (( i % 2 ) == 0)
		{
			WriteParams.usWriteData  |= ((UINT16)pChipConfig->pbyImageFile[0x100 + ((i/2)*4) + 2]) << 8;
			WriteParams.usWriteData  |= ((UINT16)pChipConfig->pbyImageFile[0x100 + ((i/2)*4) + 3]) << 0;
		}
		else
		{
			WriteParams.usWriteData  |= ((UINT16)pChipConfig->pbyImageFile[0x100 + ((i/2)*4) + 0]) << 8;
			WriteParams.usWriteData  |= ((UINT16)pChipConfig->pbyImageFile[0x100 + ((i/2)*4) + 1]) << 0;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		WriteParams.ulWriteAddress = 0x152;
		WriteParams.usWriteData = (UINT16)( 0x8000 | i );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Write one in CPU internal memory. */
	for(i=0; i<8; i++)
	{
		WriteParams.ulWriteAddress = 0x150;
		if (i == 0) 
		{
			WriteParams.usWriteData = 0x0001;
		}
		else
		{
			WriteParams.usWriteData = 0x0000;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x152;
		WriteParams.usWriteData = (UINT16)( 0x8000 | ( i + 8 ));

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Clear memory access registers: */
	WriteParams.ulWriteAddress = 0x150;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x152;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Run BISTs and key decode. */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData = 0x0081;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for the key decode PC to clear. */
	ulResult = Oct6100ApiWaitForPcRegisterBit( f_pApiInstance, 0x160, 0, 0, 100000, &fBitEqual );
	if ( TRUE != fBitEqual )
		return cOCT6100_ERR_FATAL_13;

	/* Read the key valid bit to make sure everything is ok. */
	ReadParams.ulReadAddress = 0x160;
	ReadParams.pusReadData = &usReadData;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Either the firmware image was not loaded correctly (from pointer given by user) */
	/* or the channel capacity pins of the chip do not match what the firmware is expecting. */
	if ( ( usReadData & 0x4 ) == 0 )
		return cOCT6100_ERR_OPEN_INVALID_FIRMWARE_OR_CAPACITY_PINS;

	/* Read the result of the internal memory bist. */
	ReadParams.ulReadAddress = 0x110;
	ReadParams.pusReadData = &ausBistData[ 0 ];

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ReadParams.ulReadAddress = 0x114;
	ReadParams.pusReadData = &ausBistData[ 1 ];

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	ReadParams.ulReadAddress = 0x118;
	ReadParams.pusReadData = &ausBistData[ 2 ];

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check if an error was reported. */
	if (ausBistData[0] != 0x0000 || ausBistData[1] != 0x0000 || ausBistData[2] != 0x0000)
		return cOCT6100_ERR_OPEN_INTERNAL_MEMORY_BIST;

	/* Put key decoder in powerdown. */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData = 0x008A;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBootFc2PllReadCap

Description:    Configures the chip's FC2 PLL. Special version for GetcapacityPins.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
UINT32 Oct6100ApiBootFc2PllReadCap(
				IN 	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS	f_pGetCapacityPins)
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						aulWaitTime[ 2 ];
	UINT32						ulResult;
	UINT32						ulFc2PllDivisor = 0;
	UINT32						ulMtDivisor = 0;
	UINT32						ulFcDivisor = 0;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pGetCapacityPins->pProcessContext;

	WriteParams.ulUserChipId = f_pGetCapacityPins->ulUserChipId;

	/* First put the chip and main registers in soft-reset. */
	WriteParams.ulWriteAddress = 0x100;
	WriteParams.usWriteData = 0x0;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulFc2PllDivisor = 0x1050;
	ulMtDivisor = 0x4300;
	ulFcDivisor = 0x4043;

	/* Setup delay chains. */
	if ( (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR) ||  (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS) )
	{
		/* SDRAM */
		WriteParams.ulWriteAddress = 0x1B0;
		WriteParams.usWriteData = 0x1003;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B2;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B4;
		WriteParams.usWriteData = 0x4030;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B6;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	else /* if ( cOCT6100_MEM_TYPE_DDR == pChipConfig->byMemoryType ) */
	{
		/* DDR */
		WriteParams.ulWriteAddress = 0x1B0;
		WriteParams.usWriteData = 0x201F;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B2;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B4;
		WriteParams.usWriteData = 0x1000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B6;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* udqs */
	WriteParams.ulWriteAddress = 0x1B8;
	WriteParams.usWriteData = 0x1003;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x1BA;
	WriteParams.usWriteData = 0x0021;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* ldqs */
	WriteParams.ulWriteAddress = 0x1BC;
	WriteParams.usWriteData = 0x1000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x1BE;
	WriteParams.usWriteData = 0x0021;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x12C;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x12E;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select fc2pll for fast_clk and mtsclk sources. Select mem_clk_i for afclk. */
	WriteParams.ulWriteAddress = 0x140;
	WriteParams.usWriteData = (UINT16)ulMtDivisor;

	if ( f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		WriteParams.usWriteData |= 0x0001;
	else
		WriteParams.usWriteData |= 0x0004;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x144;
	WriteParams.usWriteData = (UINT16)ulFcDivisor;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x13E;
	WriteParams.usWriteData = 0x0001;	/*  Remove reset from above divisors */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select upclk directly as ref source for fc2pll. */
	WriteParams.ulWriteAddress = 0x134;
	WriteParams.usWriteData = 0x0001;
		
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Setup fc2pll. */
	WriteParams.ulWriteAddress = 0x132;
	WriteParams.usWriteData = (UINT16)ulFc2PllDivisor;
		
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.usWriteData |= 0x02;	/* Raise fb divisor reset. */
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.usWriteData |= 0x80;	/* Raise IDDTN signal.*/
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for fc2pll to stabilize. */
	aulWaitTime[ 0 ] = 2000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Drive mem_clk_o out on proper interface. */
	if ( TRUE == f_pGetCapacityPins->fEnableMemClkOut )
	{
		if ( (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR) || (f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS)  )
		{
			WriteParams.ulWriteAddress = 0x128;
			WriteParams.usWriteData = 0x0301;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	
		if ( f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_DDR || f_pGetCapacityPins->ulMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		{
			WriteParams.ulWriteAddress = 0x12A;
			WriteParams.usWriteData = 0x000F;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	return cOCT6100_ERR_OK;
}
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBootFc2Pll

Description:    Configures the chip's FC2 PLL.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBootFc2Pll
UINT32 Oct6100ApiBootFc2Pll(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						aulWaitTime[ 2 ];
	UINT32						ulResult;
	UINT32						ulFc2PllDivisor = 0;
	UINT32						ulMtDivisor = 0;
	UINT32						ulFcDivisor = 0;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain local pointer to chip configuration structure. */
	pChipConfig = &pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pChipConfig->ulUserChipId;

	/* First put the chip and main registers in soft-reset. */
	WriteParams.ulWriteAddress = 0x100;
	WriteParams.usWriteData = 0x0;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select register configuration based on the memory frequency. */
	switch ( f_pApiInstance->pSharedInfo->ChipConfig.ulMemClkFreq )
	{
	case 133000000:
		ulFc2PllDivisor = 0x1050;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 672;
		pSharedInfo->MiscVars.usMaxH100Speed = 124;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x050B;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0516;

		break;
	case 125000000:
		ulFc2PllDivisor = 0x0F50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 624;
		pSharedInfo->MiscVars.usMaxH100Speed = 116;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x04CA;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x04D4;

		break;
	case 117000000:
		ulFc2PllDivisor = 0x0E50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 576;
		pSharedInfo->MiscVars.usMaxH100Speed = 108;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0489;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0492;

		break;
	case 108000000:
		ulFc2PllDivisor = 0x0D50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 528;
		pSharedInfo->MiscVars.usMaxH100Speed = 99;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0408;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0410;

		break;
	case 100000000:
		ulFc2PllDivisor = 0x0C50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 480;
		pSharedInfo->MiscVars.usMaxH100Speed = 91;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x03C8;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x03D0;

		break;
	case 92000000:
		ulFc2PllDivisor = 0x0B50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 432;
		pSharedInfo->MiscVars.usMaxH100Speed = 83;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0387;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x038E;

		break;
	case 83000000:
		ulFc2PllDivisor = 0x0A50;
		ulMtDivisor = 0x4300;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 384;
		pSharedInfo->MiscVars.usMaxH100Speed = 74;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0346;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x034C;

		break;
	case 75000000:
		ulFc2PllDivisor = 0x0950;
		ulMtDivisor = 0x4200;
		ulFcDivisor = 0x4043;
		pSharedInfo->MiscVars.usMaxNumberOfChannels = 336;
		pSharedInfo->MiscVars.usMaxH100Speed = 64;

		if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x0306;
		else
			pSharedInfo->MiscVars.usTdmClkBoundary = 0x030C;

		break;
	default:
		return cOCT6100_ERR_FATAL_DB;
	}

	/* Verify that the max channel is not too big based on the chip frequency. */
	if ( pSharedInfo->ChipConfig.usMaxChannels > pSharedInfo->MiscVars.usMaxNumberOfChannels )
		return cOCT6100_ERR_OPEN_MAX_ECHO_CHANNELS;
	
	/* Setup delay chains. */
	if ( (pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_SDR) ||  (pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS) )
	{
		/* SDRAM */
		WriteParams.ulWriteAddress = 0x1B0;
		WriteParams.usWriteData = 0x1003;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B2;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B4;
		WriteParams.usWriteData = 0x4030;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B6;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	else /* if ( cOCT6100_MEM_TYPE_DDR == pChipConfig->byMemoryType ) */
	{
		/* DDR */
		WriteParams.ulWriteAddress = 0x1B0;
		WriteParams.usWriteData = 0x201F;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B2;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B4;
		WriteParams.usWriteData = 0x1000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x1B6;
		WriteParams.usWriteData = 0x0021;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* udqs */
	WriteParams.ulWriteAddress = 0x1B8;
	WriteParams.usWriteData = 0x1003;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x1BA;
	WriteParams.usWriteData = 0x0021;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* ldqs */
	WriteParams.ulWriteAddress = 0x1BC;
	WriteParams.usWriteData = 0x1000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x1BE;
	WriteParams.usWriteData = 0x0021;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x12C;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x12E;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select fc2pll for fast_clk and mtsclk sources. Select mem_clk_i for afclk. */
	WriteParams.ulWriteAddress = 0x140;
	WriteParams.usWriteData = (UINT16)ulMtDivisor;

	if ( f_pApiInstance->pSharedInfo->ChipConfig.byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		WriteParams.usWriteData |= 0x0001;
	else
		WriteParams.usWriteData |= 0x0004;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x144;
	WriteParams.usWriteData = (UINT16)ulFcDivisor;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x13E;
	WriteParams.usWriteData = 0x0001;	/*  Remove reset from above divisors */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select upclk directly as ref source for fc2pll. */
	WriteParams.ulWriteAddress = 0x134;
	WriteParams.usWriteData = 0x0001;
		
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Setup fc2pll. */
	WriteParams.ulWriteAddress = 0x132;
	WriteParams.usWriteData = (UINT16)ulFc2PllDivisor;
		
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.usWriteData |= 0x02;	/* Raise fb divisor reset. */
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.usWriteData |= 0x80;	/* Raise IDDTN signal.*/
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for fc2pll to stabilize. */
	aulWaitTime[ 0 ] = 2000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Drive mem_clk_o out on proper interface. */
	if ( TRUE == pChipConfig->fEnableMemClkOut )
	{
		if ( (pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_SDR) || (pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS)  )
		{
			WriteParams.ulWriteAddress = 0x128;
			WriteParams.usWriteData = 0x0301;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	
		if ( pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_DDR || pChipConfig->byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		{
			WriteParams.ulWriteAddress = 0x12A;
			WriteParams.usWriteData = 0x000F;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiProgramFc1PllReadCap

Description:    Configures the chip's FC1 PLL. Special version for getCapacityPins.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
UINT32 Oct6100ApiProgramFc1PllReadCap(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS	f_pGetCapacityPins)
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						aulWaitTime[ 2 ];
	UINT32						ulResult;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pGetCapacityPins->ulUserChipId;

	/* Programm P/Z bits. */
	WriteParams.ulWriteAddress = 0x130;
	
	if ( f_pGetCapacityPins->ulMemoryType  == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		WriteParams.usWriteData  = 0x0041;
	else
		WriteParams.usWriteData  = 0x0040;

	WriteParams.usWriteData |= ( f_pGetCapacityPins->ulMemoryType << 8 );
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Raise FB divisor. */
	WriteParams.usWriteData |= 0x0002;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Raise IDDTN. */
	WriteParams.usWriteData |= 0x0080;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for fc1pll to stabilize. */ 
	aulWaitTime[ 0 ] = 2000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Enable all the clock domains to do reset procedure. */
	WriteParams.ulWriteAddress = 0x186;
	WriteParams.usWriteData  = 0x015F;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	aulWaitTime[ 0 ] = 15000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiProgramFc1Pll

Description:    Configures the chip's FC1 PLL.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiProgramFc1Pll
UINT32 Oct6100ApiProgramFc1Pll(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						aulWaitTime[ 2 ];
	UINT32						ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain local pointer to chip configuration structure. */
	pChipConfig = &pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pChipConfig->ulUserChipId;

	/* Programm P/Z bits. */
	WriteParams.ulWriteAddress = 0x130;
	
	if ( f_pApiInstance->pSharedInfo->ChipConfig.byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS )
		WriteParams.usWriteData  = 0x0041;
	else
		WriteParams.usWriteData  = 0x0040;

	WriteParams.usWriteData |= ( pChipConfig->byMemoryType << 8 );
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Raise FB divisor. */
	WriteParams.usWriteData |= 0x0002;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Raise IDDTN. */
	WriteParams.usWriteData |= 0x0080;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for fc1pll to stabilize. */ 
	aulWaitTime[ 0 ] = 2000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Enable all the clock domains to do reset procedure. */
	WriteParams.ulWriteAddress = 0x186;
	WriteParams.usWriteData  = 0x015F;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	aulWaitTime[ 0 ] = 15000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBootFc1Pll

Description:    Boot the chip's FC1 PLL.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBootFc1Pll
UINT32 Oct6100ApiBootFc1Pll(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						aulWaitTime[ 2 ];
	UINT32						ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Obtain local pointer to chip configuration structure. */
	pChipConfig = &pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pChipConfig->ulUserChipId;

	/* Force bist_clk also (it too is used on resetable flops). */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData  = 0x0188;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Force all cpu clocks on chariot controllers. */
	WriteParams.ulWriteAddress = 0x182;
	WriteParams.usWriteData  = 0x0002;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x184;
	WriteParams.usWriteData  = 0x0202;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	aulWaitTime[ 0 ] = 1000;
	aulWaitTime[ 1 ] = 0;
	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Remove the reset on the entire chip and disable CPU access caching. */
	WriteParams.ulWriteAddress = 0x100;
	WriteParams.usWriteData  = 0x2003;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Remove the bist_clk. It is no longer needed.*/
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData  = 0x0088;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Disable all clks to prepare for bist clock switchover. */
	WriteParams.ulWriteAddress = 0x182;
	WriteParams.usWriteData  = 0x0001;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x186;
	WriteParams.usWriteData  = 0x0000;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x184;
	WriteParams.usWriteData  = 0x0101;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Deassert bist_active */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData  = 0x0008;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Change CPU interface to normal mode (from boot mode). */
	WriteParams.ulWriteAddress = 0x154;
	WriteParams.usWriteData  = 0x0000;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Give a couple of BIST clock cycles to turn off the BIST permanently. */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData  = 0x0108;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Turn BIST clock off for the last time. */
	WriteParams.ulWriteAddress = 0x160;
	WriteParams.usWriteData  = 0x0008;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reset procedure done! */

	/* Enable mclk for cpu interface and external memory controller. */
	WriteParams.ulWriteAddress = 0x186;
	WriteParams.usWriteData  = 0x0100;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiLoadImage

Description:    This function writes the firmware image in the external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiLoadImage
UINT32 Oct6100ApiLoadImage(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32						ulResult;
	UINT32						ulTempPtr;
	UINT32						ulNumWrites;
	PUINT16						pusSuperArray;
	unsigned char const				*pbyImageFile;
	UINT32						ulByteCount = 0;
	UINT16						usReadData;
	UINT32						ulAddressOfst;
	UINT32						i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Breakdown image into subcomponents. */
	ulTempPtr = cOCT6100_IMAGE_FILE_BASE + cOCT6100_IMAGE_AF_CST_OFFSET;

	for(i=0;i<cOCT6100_MAX_IMAGE_REGION;i++)
	{
		pSharedInfo->ImageRegion[ i ].ulPart1Size = pSharedInfo->ChipConfig.pbyImageFile[ 0x110 + ( i * 4 ) + 0 ];
		pSharedInfo->ImageRegion[ i ].ulPart2Size = pSharedInfo->ChipConfig.pbyImageFile[ 0x110 + ( i * 4 ) + 1 ];
		pSharedInfo->ImageRegion[ i ].ulClockInfo = pSharedInfo->ChipConfig.pbyImageFile[ 0x110 + ( i * 4 ) + 2 ];
		pSharedInfo->ImageRegion[ i ].ulReserved  = pSharedInfo->ChipConfig.pbyImageFile[ 0x110 + ( i * 4 ) + 3 ];

		if (i == 0)		/* AF constant. */
		{
			pSharedInfo->ImageRegion[ i ].ulPart1BaseAddress = ulTempPtr & 0x07FFFFFF;
			pSharedInfo->ImageRegion[ i ].ulPart2BaseAddress = 0;

			ulTempPtr += ( pSharedInfo->ImageRegion[ i ].ulPart1Size * 612 );
		}
		else if (i == 1)	/* NLP image */
		{
			pSharedInfo->ImageRegion[ i ].ulPart1BaseAddress = ulTempPtr & 0x07FFFFFF;
			pSharedInfo->ImageRegion[ i ].ulPart2BaseAddress = 0;

			ulTempPtr += ( pSharedInfo->ImageRegion[ i ].ulPart1Size * 2056 );
		}
		else	/* Others */
		{
			pSharedInfo->ImageRegion[ i ].ulPart1BaseAddress = ulTempPtr & 0x07FFFFFF;
			ulTempPtr += ( pSharedInfo->ImageRegion[ i ].ulPart1Size * 2064 );

			pSharedInfo->ImageRegion[ i ].ulPart2BaseAddress = ulTempPtr & 0x07FFFFFF;
			ulTempPtr += ( pSharedInfo->ImageRegion[ i ].ulPart2Size * 2448 );
		}
	}

	/* Write the image in external memory. */
	ulNumWrites = pSharedInfo->ChipConfig.ulImageSize / 2;

	BurstParams.ulWriteAddress = cOCT6100_IMAGE_FILE_BASE;
	BurstParams.pusWriteData = pSharedInfo->MiscVars.ausSuperArray;
	
	pusSuperArray = pSharedInfo->MiscVars.ausSuperArray;
	pbyImageFile = pSharedInfo->ChipConfig.pbyImageFile;

	while ( ulNumWrites != 0 )
	{
		if ( ulNumWrites >= pSharedInfo->ChipConfig.usMaxRwAccesses )
			BurstParams.ulWriteLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
		else
			BurstParams.ulWriteLength = ulNumWrites;

		for ( i = 0; i < BurstParams.ulWriteLength; i++ )
		{
			pusSuperArray[ i ]  = ( UINT16 )(( pbyImageFile [ ulByteCount++ ]) << 8);
			pusSuperArray[ i ] |= ( UINT16 )pbyImageFile [ ulByteCount++ ];
		}

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		BurstParams.ulWriteAddress += 2 * BurstParams.ulWriteLength;
		ulNumWrites -= BurstParams.ulWriteLength;
	}

	/* Perform a serie of reads to make sure the image was correclty written into memory. */
	ulAddressOfst = ( pSharedInfo->ChipConfig.ulImageSize / 2 ) & 0xFFFFFFFE;
	while ( ulAddressOfst != 0 )
	{
		ReadParams.ulReadAddress = cOCT6100_IMAGE_FILE_BASE + ulAddressOfst;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( (usReadData >> 8) != pbyImageFile[ ulAddressOfst ] )
			return cOCT6100_ERR_OPEN_IMAGE_WRITE_FAILED;

		ulAddressOfst = (ulAddressOfst / 2) & 0xFFFFFFFE;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCpuRegisterBistReadCap

Description:    Tests the operation of the CPU registers. Special Version for
				GetCapacityPins

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
UINT32 Oct6100ApiCpuRegisterBistReadCap(
				IN tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_API_GET_CAPACITY_PINS	f_pGetCapacityPins
				)
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	ulResult;
	UINT16	i;
	UINT16	usReadData;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pGetCapacityPins->ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pGetCapacityPins->ulUserChipId;

	/* Assign read data pointer that will be used throughout the function. */
	ReadParams.pusReadData = &usReadData;

	/* Start with a walking bit test. */
	for ( i = 0; i < 16; i ++ )
	{
		/* Write at address 0x150.*/
		WriteParams.ulWriteAddress = 0x150;
		WriteParams.usWriteData = (UINT16)( 0x1 << i );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Write at address 0x180.*/
		WriteParams.ulWriteAddress = 0x180;
		WriteParams.usWriteData = (UINT16)( 0x1 << ( 15 - i ) );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Now read back the two registers to make sure the acceses were successfull. */
		ReadParams.ulReadAddress = 0x150;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData != ( 0x1 << i ) )
			return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

		ReadParams.ulReadAddress = 0x180;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData != ( 0x1 << ( 15 - i ) ) )
			return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;
	}

	/* Write at address 0x150. */
	WriteParams.ulWriteAddress = 0x150;
	WriteParams.usWriteData = 0xCAFE;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write at address 0x180. */
	WriteParams.ulWriteAddress = 0x180;
	WriteParams.usWriteData = 0xDECA;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Now read back the two registers to make sure the acceses were successfull. */
	ReadParams.ulReadAddress = 0x150;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( usReadData != 0xCAFE )
		return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

	ReadParams.ulReadAddress = 0x180;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( usReadData != 0xDECA )
		return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

	return cOCT6100_ERR_OK;
}
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCpuRegisterBist

Description:    Tests the operation of the CPU registers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCpuRegisterBist
UINT32 Oct6100ApiCpuRegisterBist(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	ulResult;
	UINT16	i;
	UINT16	usReadData;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Assign read data pointer that will be used throughout the function. */
	ReadParams.pusReadData = &usReadData;

	/* Start with a walking bit test. */
	for ( i = 0; i < 16; i ++ )
	{
		/* Write at address 0x150.*/
		WriteParams.ulWriteAddress = 0x150;
		WriteParams.usWriteData = (UINT16)( 0x1 << i );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Write at address 0x180.*/
		WriteParams.ulWriteAddress = 0x180;
		WriteParams.usWriteData = (UINT16)( 0x1 << ( 15 - i ) );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Now read back the two registers to make sure the acceses were successfull. */
		ReadParams.ulReadAddress = 0x150;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData != ( 0x1 << i ) )
			return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

		ReadParams.ulReadAddress = 0x180;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData != ( 0x1 << ( 15 - i ) ) )
			return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;
	}

	/* Write at address 0x150. */
	WriteParams.ulWriteAddress = 0x150;
	WriteParams.usWriteData = 0xCAFE;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write at address 0x180. */
	WriteParams.ulWriteAddress = 0x180;
	WriteParams.usWriteData = 0xDECA;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Now read back the two registers to make sure the acceses were successfull. */
	ReadParams.ulReadAddress = 0x150;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( usReadData != 0xCAFE )
		return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

	ReadParams.ulReadAddress = 0x180;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( usReadData != 0xDECA )
		return cOCT6100_ERR_OPEN_CPU_REG_BIST_ERROR;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBootSdram

Description:    Configure and test the SDRAM.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBootSdram
UINT32 Oct6100ApiBootSdram(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32	ulResult;
	UINT16	usReadData;
	UINT16	usWriteData23E;
	UINT16	usWriteData230;
	UINT32	i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get local pointer to the chip configuration structure.*/
	pChipConfig = &f_pApiInstance->pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	usWriteData23E = 0x0000;
	usWriteData230 = 0x0000;

	if ( (pSharedInfo->ChipConfig.byMemoryType == cOCT6100_MEM_TYPE_SDR) || (pSharedInfo->ChipConfig.byMemoryType == cOCT6100_MEM_TYPE_SDR_PLL_BYPASS)  )
	{
		/* SDRAM: */
		switch( pChipConfig->ulMemoryChipSize )
		{
		case cOCT6100_MEMORY_CHIP_SIZE_8MB:
			usWriteData230 |= ( cOCT6100_16MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_16MB:
			usWriteData230 |= ( cOCT6100_32MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_32MB:
			usWriteData230 |= ( cOCT6100_64MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_64MB:
			usWriteData230 |= ( cOCT6100_128MB_MEMORY_BANKS << 2 );
			break;
		default:
			return cOCT6100_ERR_FATAL_16;
		}

		usWriteData230 |= 0x0002;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Precharge all banks. */
		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0010;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;

		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Program the mode register. */
		usWriteData23E = 0x0030;
		WriteParams.usWriteData = usWriteData23E;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0000;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;

		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		
		/* Do CBR refresh (twice) */
		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0040;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;

		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	else
	{
		/* DDR: */
		switch( pChipConfig->ulMemoryChipSize )
		{
		case cOCT6100_MEMORY_CHIP_SIZE_16MB:
			usWriteData230 |= ( cOCT6100_16MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_32MB:
			usWriteData230 |= ( cOCT6100_32MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_64MB:
			usWriteData230 |= ( cOCT6100_64MB_MEMORY_BANKS << 2 );
			break;
		case cOCT6100_MEMORY_CHIP_SIZE_128MB:
			usWriteData230 |= ( cOCT6100_128MB_MEMORY_BANKS << 2 );
			break;
		default:
			return cOCT6100_ERR_FATAL_17;
		}
		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Precharge all banks. */
		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0010;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Program DDR mode register. */
		usWriteData23E = 0x4000;
		
		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0000;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		
		/* Program SDR mode register. */
		usWriteData23E = 0x0161;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0000;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
		
		/* Precharge all banks. */
		usWriteData23E = 0xFFFF;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0010;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Do CBR refresh (twice) */
		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0040;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle.*/
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Program SDR mode register. */
		usWriteData23E = 0x0061;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 &= 0x000C;
		usWriteData230 |= 0x0000;

		WriteParams.ulWriteAddress = 0x230;
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		usWriteData230 |= 0x0002;
	
		WriteParams.usWriteData = usWriteData230;
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x23E;
		WriteParams.usWriteData = usWriteData23E;
		for ( i = 0; i < 5; i++ )
		{
			/* Wait cycle. */
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Set the refresh frequency. */
	WriteParams.ulWriteAddress = 0x242;
	WriteParams.usWriteData = 0x0400;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x244;
	WriteParams.usWriteData = 0x0200;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x248;
	WriteParams.usWriteData = 0x800;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x246;
	WriteParams.usWriteData = 0x0012;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Enable the SDRAM and refreshes. */
	usWriteData230 &= 0x000C;
	usWriteData230 |= 0x0001;

	WriteParams.ulWriteAddress = 0x230;
	WriteParams.usWriteData = usWriteData230;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x246;
	WriteParams.usWriteData = 0x0013;
	
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiEnableClocks

Description:    This function will disable clock masking for all the modules
				of the chip. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiEnableClocks
UINT32 Oct6100ApiEnableClocks(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Initialize the process context and user chip ID once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Enable tdmie / adpcm mclk clocks. */
	WriteParams.ulWriteAddress = 0x186;
	WriteParams.usWriteData = 0x015F;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Configure the DQS register for the DDR memory */
	WriteParams.ulWriteAddress = 0x180;
	WriteParams.usWriteData = 0xFF00;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Enable pgsp chariot clocks */
	WriteParams.ulWriteAddress = 0x182;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Enable af/mt chariot clocks */
	WriteParams.ulWriteAddress = 0x184;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiProgramNLP

Description:    This function will write image values to configure the NLP.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiProgramNLP
UINT32 Oct6100ApiProgramNLP(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	UINT32	ulResult;
	UINT16	usReadData;
	UINT16	usReadHighData;
	BOOL	fBitEqual;
	UINT32	ulEgoEntry[4];
	UINT32	ulTempAddress;
	UINT32  ulAfCpuUp = FALSE;
	UINT32	i;
	UINT32	ulLoopCounter = 0;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get local pointer to the chip configuration structure.*/
	pChipConfig = &f_pApiInstance->pSharedInfo->ChipConfig;

	/* Initialize the process context and user chip ID once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Initialize the process context and user chip ID once and for all. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	if ( pSharedInfo->ChipConfig.fEnableProductionBist == TRUE )
	{
		UINT32	ulReadData;
		UINT32	ulBitPattern;
		UINT32	j, k;

		/* Since the pouch section (256 bytes) will not be tested by the firmware, */
		/* the API has to make sure this section is working correctly. */
		for ( k = 0; k < 2; k ++ )
		{
			if ( k == 0 )
				ulBitPattern = 0x1;
			else
				ulBitPattern = 0xFFFFFFFE;

			for ( j = 0; j < 32; j ++ )
			{
				/* Write the DWORDs. */
				for ( i = 0; i < 64; i ++ )
				{
					ulResult = Oct6100ApiWriteDword( f_pApiInstance, cOCT6100_POUCH_BASE + i * 4, ulBitPattern << j );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				/* Read the DWORDs. */
				for ( i = 0; i < 64; i ++ )
				{
					ulResult = Oct6100ApiReadDword( f_pApiInstance, cOCT6100_POUCH_BASE + i * 4, &ulReadData );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					/* Check if the value matches. */
					if ( ( ulBitPattern << j ) != ulReadData )
						return cOCT6100_ERR_OPEN_PRODUCTION_BIST_POUCH_ERROR;
				}
			}
		}
	}

	/* Write the image info in the chip. */
	WriteParams.ulWriteAddress = cOCT6100_PART1_END_STATICS_BASE;
	WriteParams.usWriteData = (UINT16)( ( pSharedInfo->ImageRegion[ 0 ].ulPart1BaseAddress >> 16 ) & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( pSharedInfo->ImageRegion[ 0 ].ulPart1BaseAddress & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	for( i = 0; i < 8; i++ )
	{
		if ( pSharedInfo->ImageRegion[ i + 2 ].ulPart1Size != 0 )
		{
			WriteParams.ulWriteAddress = cOCT6100_PART1_END_STATICS_BASE + 0x4 + ( i * 0xC );
			WriteParams.usWriteData = (UINT16)(( pSharedInfo->ImageRegion[ i + 2 ].ulPart1BaseAddress >> 16 ) & 0xFFFF );

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = (UINT16)( pSharedInfo->ImageRegion[ i + 2 ].ulPart1BaseAddress & 0xFFFF );

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		if ( pSharedInfo->ImageRegion[ i + 2 ].ulPart2Size != 0 )
		{
			WriteParams.ulWriteAddress = cOCT6100_PART1_END_STATICS_BASE + 0x4 + ( i * 0xC ) + 4;
			WriteParams.usWriteData = (UINT16)(( pSharedInfo->ImageRegion[ i + 2 ].ulPart2BaseAddress >> 16 ) & 0xFFFF );

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = (UINT16)( pSharedInfo->ImageRegion[ i + 2 ].ulPart2BaseAddress & 0xFFFF );

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		WriteParams.ulWriteAddress = cOCT6100_PART1_END_STATICS_BASE + 0x4 + ( i * 0xC ) + 8;
		WriteParams.usWriteData  = 0x0000;
		WriteParams.usWriteData |= ( pSharedInfo->ImageRegion[ i + 2 ].ulPart1Size << 8 );
		WriteParams.usWriteData |= pSharedInfo->ImageRegion[ i + 2 ].ulPart2Size;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData  = 0x0000;
		WriteParams.usWriteData |= ( pSharedInfo->ImageRegion[ i + 2 ].ulClockInfo << 8 );
		WriteParams.usWriteData |= pSharedInfo->ImageRegion[ i + 2 ].ulReserved;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Put NLP in config mode. */
	WriteParams.ulWriteAddress = 0x2C2;
	WriteParams.usWriteData = 0x160E;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x692;
	WriteParams.usWriteData = 0x010A;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Upload the up to 8 NLP pages + 1 AF page (for timing reasons). */
	for ( i = 0; i < pSharedInfo->ImageRegion[ 1 ].ulPart1Size; i++ )
	{
		ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 1 ].ulPart1BaseAddress + 1028 * ( i * 2 ), 0x1280, 1024, &(ulEgoEntry[0]));
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 1 ].ulPart1BaseAddress + 1028 * (( i * 2 ) + 1 ), 0x1680, 1024, &(ulEgoEntry[2]));
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiRunEgo( f_pApiInstance, FALSE, 2, ulEgoEntry );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Shift mt chariot memories. This process will complete by the time */
		/* the next LSU transfer is done. */
		WriteParams.ulWriteAddress = 0x692;
		WriteParams.usWriteData = 0x010B;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		ulResult = Oct6100ApiWaitForPcRegisterBit( f_pApiInstance, 0x692, 0, 0, 100000, &fBitEqual );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		if ( TRUE != fBitEqual )
			return cOCT6100_ERR_FATAL_1A;
	}

	/* 1 AF page (for timing reasons). */
	ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 2 ].ulPart1BaseAddress + (516 * 0), 0x1280, 512, &(ulEgoEntry[0]));
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 2 ].ulPart1BaseAddress + (516 * 1), 0x1480, 512, &(ulEgoEntry[2]));
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiRunEgo( f_pApiInstance, FALSE, 2, ulEgoEntry );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 2 ].ulPart1BaseAddress + (516 * 2), 0x1680, 512, &(ulEgoEntry[0]));
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiCreateEgoEntry( cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + pSharedInfo->ImageRegion[ 2 ].ulPart1BaseAddress + (516 * 3), 0x1880, 512, &(ulEgoEntry[2]));
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiRunEgo( f_pApiInstance, FALSE, 2, ulEgoEntry );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write constant memory init context position in channel "672" for pgsp. */
	WriteParams.ulWriteAddress = 0x71A;
	WriteParams.usWriteData = 0x8000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set fixed PGSP event_in base address to 800 on a 2k boundary */
	WriteParams.ulWriteAddress = 0x716;
	WriteParams.usWriteData = 0x800 >> 11;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set fixed PGSP event_out to 0x2C0000h on a 16k boundary */
	WriteParams.ulWriteAddress = 0x71C;
	WriteParams.usWriteData = 0x2C0000 >> 14;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Give chariot control of the chip. */
	WriteParams.ulWriteAddress = 0x712;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + 0x2C0000 + 0xC;
	ulTempAddress = 0x300000 + 0x0800;
	WriteParams.usWriteData = (UINT16)( ( ulTempAddress >> 16 ) & 0x07FF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + 0x2C0000 + 0xE;
	WriteParams.usWriteData = (UINT16)( ( ulTempAddress >> 0 ) & 0xFF00 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write the init PGSP event in place. */
	WriteParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + 0x800;
	WriteParams.usWriteData = 0x0200;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + 0x802;
	WriteParams.usWriteData = 0x02A0;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Also write the register 710, which tells PGSP how many tones are supported. */
	WriteParams.ulWriteAddress = 0x710;
	WriteParams.usWriteData = 0x0000;
	WriteParams.usWriteData |= pChipConfig->pbyImageFile[ 0x7FA ] << 8;
	WriteParams.usWriteData |= pChipConfig->pbyImageFile[ 0x7FB ] << 0;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Start both processors in the NLP. */
	WriteParams.ulWriteAddress = 0x373FE;
	WriteParams.usWriteData = 0x00FF;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x37BFE;
	WriteParams.usWriteData = 0x00FE;	/* Tell processor 1 to just go to sleep. */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x37FC6;
	WriteParams.usWriteData = 0x8004;	/* First PC.*/

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x37FD0;
	WriteParams.usWriteData = 0x0002;	/* Take out of reset. */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x37FD2;
	WriteParams.usWriteData = 0x0002;	/* Take out of reset. */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Start processor in the AF. */
	for ( i = 0; i < 16; i ++ )
	{
		WriteParams.ulWriteAddress = cOCT6100_POUCH_BASE + ( i * 2 );
		if ( i == 9 )
		{
			if ( pSharedInfo->ChipConfig.fEnableProductionBist == TRUE )
			{
				if (pSharedInfo->ChipConfig.ulProductionBistMode == cOCT6100_PRODUCTION_BIST_SHORT)
					WriteParams.usWriteData = cOCT6100_PRODUCTION_SHORT_BOOT_TYPE;
				else
					WriteParams.usWriteData = cOCT6100_PRODUCTION_BOOT_TYPE;
			}
			else
			{
				WriteParams.usWriteData = cOCT6100_AF_BOOT_TYPE;
			}
		}
		else
			WriteParams.usWriteData = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Check if the production BIST mode was requested. */
	if ( pSharedInfo->ChipConfig.fEnableProductionBist == TRUE )
	{
		UINT32	ulTotalElements = 3;
		UINT32	ulCrcKey;
		UINT32	aulMessage[ 4 ];
		UINT32	ulWriteAddress = 0x20 + cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;

		/* Magic key. */
		aulMessage[ 0 ] = 0xCAFECAFE; 
		/* Memory size. */
		aulMessage[ 1 ] = pSharedInfo->MiscVars.ulTotalMemSize;
		/* Loop count. */
		aulMessage[ 2 ] = pSharedInfo->ChipConfig.ulNumProductionBistLoops;
		/* CRC initialized. */
		aulMessage[ 3 ] = 0;

		ulResult = Oct6100ApiProductionCrc( f_pApiInstance, aulMessage, ulTotalElements, &ulCrcKey );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		aulMessage[ 3 ] = ulCrcKey;

		/* Write the message to the external memory. */
		for ( i = 0; i < ulTotalElements + 1; i ++ )
		{
			ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulWriteAddress + i * 4, aulMessage[ i ] );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	WriteParams.ulWriteAddress = 0xFFFC6;
	WriteParams.usWriteData = 0x1284;	/* First PC.*/

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	WriteParams.ulWriteAddress = 0xFFFD0;
	WriteParams.usWriteData = 0x0002;	/* Take out of reset. */

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	while ( ulAfCpuUp == FALSE )
	{
		if ( ulAfCpuUp == FALSE )
		{
			ReadParams.ulReadAddress = cOCT6100_POUCH_BASE;
			ReadParams.pusReadData = &usReadHighData;

			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			ReadParams.ulReadAddress += 2;
			ReadParams.pusReadData = &usReadData;

			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			if ( pSharedInfo->ChipConfig.fEnableProductionBist == TRUE )
			{
				/* Should read 0x0007 when bisting. */
				if ( (( usReadHighData & 0xFFFF ) == cOCT6100_PRODUCTION_BOOT_TYPE) ||
					(( usReadHighData & 0xFFFF ) == cOCT6100_PRODUCTION_SHORT_BOOT_TYPE) )
				{
					/* Verify if the bist has started successfully. */
					if ( ( usReadData & 0xFFFF ) == 0x0002 )
						return cOCT6100_ERR_OPEN_PRODUCTION_BIST_CONF_FAILED;
					else if ( ( usReadData & 0xFFFF ) != 0xEEEE )
						return cOCT6100_ERR_OPEN_PRODUCTION_BOOT_FAILED;

					ulAfCpuUp = TRUE;
				}
			}
			else /* if ( pSharedInfo->ChipConfig.fEnableProductionBist == FALSE ) */
			{
				if ( ( usReadHighData & 0xFFFF ) == cOCT6100_AF_BOOT_TYPE )
				{
					/* Verify if the bist succeeded. */
					if ( ( usReadData & 0xFFFF ) != 0x0000 )
						return cOCT6100_ERR_OPEN_FUNCTIONAL_BIST_FAILED;

					ulAfCpuUp = TRUE;
				}
			}
		}

		ulLoopCounter++;

		if ( ulLoopCounter == cOCT6100_MAX_LOOP_CPU_TIMEOUT )
			return cOCT6100_ERR_OPEN_AF_CPU_TIMEOUT;
	}

	/* Return NLP in operationnal mode. */
	WriteParams.ulWriteAddress = 0x2C2;
	WriteParams.usWriteData = 0x060E;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x692;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiSetH100Register

Description:    This function will configure the H.100 registers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiSetH100Register
UINT32 Oct6100ApiSetH100Register(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						ulResult;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	UINT32						i;
	UINT32						ulOffset;
	BOOL						fAllStreamAt2Mhz = TRUE;
	const UINT16	ausAdpcmResetContext[32] = { 0x1100, 0x0220, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0000, 0x0008, 0x0000, 0x0000, 0x0100, 0x0000, 0x0020, 0x0000, 0x0000, 0x0000, 0x0002, 0x0000, 0x0000, 0x0040, 
												 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000};

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get local pointer to the chip configuration structure. */
	pChipConfig = &f_pApiInstance->pSharedInfo->ChipConfig;

	/* Initialize the process context and user chip ID once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Set the Global OE bit. */
	WriteParams.ulWriteAddress = 0x300;
	WriteParams.usWriteData = 0x0004;

	/* Set the number of streams. */
	switch( pChipConfig->byMaxTdmStreams )
	{
	case 32:
		WriteParams.usWriteData |= ( 0 << 3 );
		break;
	case 16:
		WriteParams.usWriteData |= ( 1 << 3 );
		break;
	case 8:
		WriteParams.usWriteData |= ( 2 << 3 );
		break;
	case 4:
		WriteParams.usWriteData |= ( 3 << 3 );
		break;
	default:
		break;
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Configure the stream frequency. */
	WriteParams.ulWriteAddress = 0x330;
	WriteParams.usWriteData = 0x0000;
	for ( i = 0; i < (UINT32)(pChipConfig->byMaxTdmStreams / 4); i++)
	{
		ulOffset = i*2;
		switch( pChipConfig->aulTdmStreamFreqs[ i ] )
		{
		case cOCT6100_TDM_STREAM_FREQ_2MHZ:
			WriteParams.usWriteData |= ( 0x0 << ulOffset );
			break;
		case cOCT6100_TDM_STREAM_FREQ_4MHZ:
			WriteParams.usWriteData |= ( 0x1 << ulOffset );
			fAllStreamAt2Mhz = FALSE;
			break;
		case cOCT6100_TDM_STREAM_FREQ_8MHZ:
			WriteParams.usWriteData |= ( 0x2 << ulOffset );
			fAllStreamAt2Mhz = FALSE;
			break;
		default:
			break;
		}
	}

	/* Set the stream to 16 MHz if the fast H.100 mode is selected. */
	if ( pChipConfig->fEnableFastH100Mode == TRUE )
	{
		fAllStreamAt2Mhz = FALSE;
		WriteParams.usWriteData = 0xFFFF;
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
	{
		/* Make the chip track both clock A and B to perform fast H.100 mode. */
		WriteParams.ulWriteAddress = 0x322;
		WriteParams.usWriteData = 0x0004;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Enable the fast H.100 mode. */
		WriteParams.ulWriteAddress = 0x332;
		WriteParams.usWriteData = 0x0003;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	WriteParams.ulWriteAddress = 0x376;
	WriteParams.usWriteData = (UINT16)( pSharedInfo->MiscVars.usTdmClkBoundary );
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Select delay for early clock (90 and 110). */
	WriteParams.ulWriteAddress = 0x378;
	if ( pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
		WriteParams.usWriteData = 0x000A;
	else
	{
		/* Set the TDM sampling. */
		if ( pSharedInfo->ChipConfig.byTdmSampling == cOCT6100_TDM_SAMPLE_AT_RISING_EDGE )
		{
			WriteParams.usWriteData = 0x0AF0;
		}
		else if ( pSharedInfo->ChipConfig.byTdmSampling == cOCT6100_TDM_SAMPLE_AT_FALLING_EDGE )
		{
			WriteParams.usWriteData = 0x0A0F;
		}
		else /* pSharedInfo->ChipConfig.ulTdmSampling == cOCT6100_TDM_SAMPLE_AT_3_QUARTERS */
		{
			WriteParams.usWriteData = 0x0A08;
		}
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Protect chip by preventing too rapid timeslot arrival (mclk == 133 MHz). */
	WriteParams.ulWriteAddress = 0x37A;
	WriteParams.usWriteData = (UINT16)pSharedInfo->MiscVars.usMaxH100Speed;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Allow H.100 TS to progress. */
	WriteParams.ulWriteAddress = 0x382;
	WriteParams.usWriteData = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set by-pass mode. */
	WriteParams.ulWriteAddress = 0x50E;
	WriteParams.usWriteData = 0x0001;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* TDMIE bits. */
	WriteParams.ulWriteAddress = 0x500;
	WriteParams.usWriteData = 0x0003;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write normal ADPCM reset values in ADPCM context 1344. */
	for(i=0;i<32;i++)
	{
		WriteParams.ulWriteAddress = 0x140000 + ( 0x40 * 1344 ) + ( i * 2 );
		WriteParams.usWriteData = ausAdpcmResetContext[i];

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Make sure delay flops are configured correctly if all streams are at 2 MHz. */
	if ( fAllStreamAt2Mhz == TRUE )
	{
		/* Setup H.100 sampling to lowest value. */
		WriteParams.ulWriteAddress = 0x144;
		WriteParams.usWriteData = 0x4041;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		WriteParams.ulWriteAddress = 0x378;
		WriteParams.usWriteData = 0x0A00;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteMiscellaneousRegisters

Description:    This function will write to various registers to activate the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteMiscellaneousRegisters
UINT32 Oct6100ApiWriteMiscellaneousRegisters(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Initialize the process context and user chip ID once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/* Free the interrupt pin of the chip (i.e. remove minimum time requirement between interrupts). */
	WriteParams.ulWriteAddress = 0x214;
	WriteParams.usWriteData = 0x0000;
	if ( f_pApiInstance->pSharedInfo->ChipConfig.byInterruptPolarity == cOCT6100_ACTIVE_HIGH_POLARITY )
		WriteParams.usWriteData |= 0x4000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write MT chariot interval */
	WriteParams.ulWriteAddress = 0x2C2;
	if ( f_pApiInstance->pSharedInfo->ImageInfo.usMaxNumberOfChannels > 640 )
		WriteParams.usWriteData = 0x05EA;
	else if ( f_pApiInstance->pSharedInfo->ImageInfo.usMaxNumberOfChannels > 513 )
		WriteParams.usWriteData = 0x0672;
	else /* if ( f_pApiInstance->pSharedInfo->ImageInfo.usMaxNumberOfChannels <= 513 ) */
		WriteParams.usWriteData = 0x0750;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write set second part5 time. */
	WriteParams.ulWriteAddress = 0x2C4;
	WriteParams.usWriteData = 0x04A0;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write CPU bucket timer to guarantee 200 cycles between each CPU access. */
	WriteParams.ulWriteAddress = 0x234;
	WriteParams.usWriteData = 0x0804;	

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x236;
	WriteParams.usWriteData = 0x0100;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCreateSerializeObjects

Description:    Creates a handle to each serialization object used by the API.

				Note that in a multi-process system the user's process context
				structure pointer is needed by this function.  Thus, the
				pointer must be valid.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_ulUserChipId		User chip ID for this serialization object.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCreateSerializeObjects
UINT32 Oct6100ApiCreateSerializeObjects(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulUserChipId )
{
	tOCT6100_CREATE_SERIALIZE_OBJECT	CreateSerObj;
	UINT32	ulResult;
	CHAR	szSerObjName[ 64 ] = "Oct6100ApiXXXXXXXXApiSerObj";


	/* Set some parameters of the create structure once and for all. */
	CreateSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	CreateSerObj.pszSerialObjName = szSerObjName;

	/*----------------------------------------------------------------------*/
	/* Set the chip ID in the semaphore name. */
	szSerObjName[ 10 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >> 28 ) & 0xFF );
	szSerObjName[ 11 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >> 24 ) & 0xFF );
	szSerObjName[ 12 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >> 20 ) & 0xFF );
	szSerObjName[ 13 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >> 16 ) & 0xFF );
	szSerObjName[ 14 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >> 12 ) & 0xFF );
	szSerObjName[ 15 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >>  8 ) & 0xFF );
	szSerObjName[ 16 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >>  4 ) & 0xFF );
	szSerObjName[ 17 ] = (CHAR) Oct6100ApiHexToAscii( (f_ulUserChipId >>  0 ) & 0xFF );

	ulResult = Oct6100UserCreateSerializeObject( &CreateSerObj );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	f_pApiInstance->ulApiSerObj = CreateSerObj.ulSerialObjHndl;
	/*----------------------------------------------------------------------*/



	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiDestroySerializeObjects

Description:    Destroy handles to each serialization object used by the API.

				Note that in a multi-process system the user's process context
				structure pointer is needed by this function.  Thus, the
				pointer must be valid.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiDestroySerializeObjects
UINT32 Oct6100ApiDestroySerializeObjects(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tOCT6100_DESTROY_SERIALIZE_OBJECT	DestroySerObj;
	UINT32	ulResult;

	/* Set some parameters of the create structure once and for all. */
	DestroySerObj.pProcessContext = f_pApiInstance->pProcessContext;
	DestroySerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;	

	ulResult = Oct6100UserDestroySerializeObject( &DestroySerObj );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRunEgo

Description:    Private function used to communicate with the internal processors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_fStoreFlag		Type of access performed. (Load or Store)
f_ulNumEntry		Number of access.
f_aulEntry			Array of access to perform.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRunEgo
UINT32 Oct6100ApiRunEgo( 
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance, 
				IN		BOOL							f_fStoreFlag, 
				IN		UINT32							f_ulNumEntry, 
				OUT		PUINT32							f_aulEntry )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHIP_CONFIG	pChipConfig;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32	ulResult;
	UINT32	aulCpuLsuCmd[ 2 ];
	UINT16	usReadData;
	UINT32	i;
	BOOL	fConditionFlag = TRUE;
	UINT32	ulLoopCounter = 0;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get local pointer to the chip configuration structure. */
	pChipConfig = &f_pApiInstance->pSharedInfo->ChipConfig;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* No more than 2 entries may be requested. */
	if ( f_ulNumEntry > 2 )
		return cOCT6100_ERR_FATAL_1B;

	/* Write the requested entries at address reserved for CPU. */
	for( i = 0; i < f_ulNumEntry; i++ )
	{
		WriteParams.ulWriteAddress = cOCT6100_PART1_API_SCRATCH_PAD + ( 0x8 * i );
		WriteParams.usWriteData = (UINT16)(( f_aulEntry[ i * 2 ] >> 16 ) & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( f_aulEntry[ i * 2 ] & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)(( f_aulEntry[ (i * 2) + 1] >> 16 ) & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( f_aulEntry[ (i * 2) + 1] & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Preincrement code point. */
	pSharedInfo->MiscVars.usCodepoint++;

	/* Create DWORD 0 of command. */
	aulCpuLsuCmd[0] = 0x00000000;
	if ( f_fStoreFlag == FALSE )
		aulCpuLsuCmd[0] |= 0xC0000000;	/* EGO load. */
	else
		aulCpuLsuCmd[0] |= 0xE0000000;	/* EGO store. */

	aulCpuLsuCmd[0] |= (f_ulNumEntry - 1) << 19;
	aulCpuLsuCmd[0] |= cOCT6100_PART1_API_SCRATCH_PAD;

	/* Create DWORD 1 of command. */
	aulCpuLsuCmd[1] = 0x00000000;
	aulCpuLsuCmd[1] |= ( ( cOCT6100_PART1_API_SCRATCH_PAD + 0x10 ) & 0xFFFF ) << 16;
	aulCpuLsuCmd[1] |= pSharedInfo->MiscVars.usCodepoint;

	/* Write the EGO command in the LSU CB. */
	WriteParams.ulWriteAddress = cOCT6100_PART1_CPU_LSU_CB_BASE + ((pSharedInfo->MiscVars.usCpuLsuWritePtr & 0x7) * 0x8 );
	WriteParams.usWriteData = (UINT16)(( aulCpuLsuCmd[ 0 ] >> 16 ) & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( aulCpuLsuCmd[ 0 ] & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)(( aulCpuLsuCmd[ 1 ] >> 16 ) & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress += 2;
	WriteParams.usWriteData = (UINT16)( aulCpuLsuCmd[ 1 ] & 0xFFFF );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Post increment the write pointer. */
	pSharedInfo->MiscVars.usCpuLsuWritePtr++;

	/* Indicate new write pointer position to HW. */
	WriteParams.ulWriteAddress = cOCT6100_PART1_EGO_REG + 0x5A;
	WriteParams.usWriteData = (UINT16)( pSharedInfo->MiscVars.usCpuLsuWritePtr & 0x7 );

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Wait for codepoint to be updated before returning. */
	while( fConditionFlag )
	{
		ReadParams.ulReadAddress = cOCT6100_PART1_API_SCRATCH_PAD + 0x12;
		usReadData = (UINT16)( pSharedInfo->MiscVars.usCodepoint );

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	
		if ( usReadData == pSharedInfo->MiscVars.usCodepoint )
			fConditionFlag = FALSE;

		ulLoopCounter++;

		if ( ulLoopCounter == cOCT6100_MAX_LOOP )
			return cOCT6100_ERR_OPEN_EGO_TIMEOUT;
	}

	/* CRC error bit must be zero. */
	ReadParams.ulReadAddress = 0x202;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( ( usReadData & 0x0400 ) != 0 )
		return cOCT6100_ERR_OPEN_CORRUPTED_IMAGE;
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCreateEgoEntry

Description:    Private function used to create an access structure to be sent
				to the internal processors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_ulExternalAddress		External memory address for the access.
f_ulInternalAddress		Which process should receive the command.
f_ulNumBytes			Number of bytes associated to the access.
f_aulEntry			Array of access to perform.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCreateEgoEntry
UINT32 Oct6100ApiCreateEgoEntry( 
				IN		UINT32						f_ulExternalAddress, 
				IN		UINT32						f_ulInternalAddress, 
				IN		UINT32						f_ulNumBytes, 
				OUT		UINT32						f_aulEntry[ 2 ] )
{
	f_aulEntry[0] = 0x80000000;
	f_aulEntry[0] |= f_ulExternalAddress & 0x07FFFFFC;
	
	f_aulEntry[1] = 0x0011C000;
	f_aulEntry[1] |= (f_ulNumBytes / 8) << 23;
	f_aulEntry[1] |= (f_ulInternalAddress >> 2) & 0x3FFF;

	return cOCT6100_ERR_OK;
}
#endif








/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitChannels

Description:    This function will initialize all the channels to power down.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitChannels
UINT32 Oct6100ApiInitChannels(
				IN OUT	tPOCT6100_INSTANCE_API f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	UINT32 i;
	UINT32 ulResult;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT16						usReadData;
	UINT32						ulTempData;
	UINT32						ulBaseAddress;
	UINT32						ulFeatureBytesOffset;
	UINT32						ulFeatureBitOffset;
	UINT32						ulFeatureFieldLength;
	UINT32						ulMask;
	UINT16						ausWriteData[ 4 ];
	UINT16						usLoopCount = 0;
	UINT16						usWriteData = 0;
	UINT16						usMclkRead;
	UINT16						usLastMclkRead;
	UINT16						usMclkDiff;
	UINT32						ulNumberOfCycleToWait;
	UINT32						ulTimeoutCounter;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	BurstParams.pusWriteData = ausWriteData;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	/* Verify that the image has enough memory to work correctly. */
	if ( ( pSharedInfo->MiscVars.ulTotalMemSize + cOCT6100_EXTERNAL_MEM_BASE_ADDRESS ) < pSharedInfo->MemoryMap.ulFreeMemBaseAddress )
		return cOCT6100_ERR_OPEN_INSUFFICIENT_EXTERNAL_MEMORY;

	/* Verify that the tail length is supported by the device.*/
	if ( pSharedInfo->ChipConfig.usTailDisplacement > pSharedInfo->ImageInfo.usMaxTailDisplacement )
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_TAIL_DISPLACEMENT_VALUE;

	/* Verify that acoustic echo is supported by the device. */
	if ( pSharedInfo->ChipConfig.fEnableAcousticEcho == TRUE && pSharedInfo->ImageInfo.fAcousticEcho == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_ACOUSTIC_ECHO;
	
	/* Verify that the image supports all the requested channels. */
	if ( pSharedInfo->ChipConfig.usMaxChannels > pSharedInfo->ImageInfo.usMaxNumberOfChannels )
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_MAX_ECHO_CHANNELS_VALUE;

	/* Max number of channels the image supports + 1 for channel recording, if requested */
	if ( ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
	  && ( pSharedInfo->ImageInfo.usMaxNumberOfChannels < cOCT6100_MAX_ECHO_CHANNELS )
	  && ( pSharedInfo->ChipConfig.usMaxChannels == pSharedInfo->ImageInfo.usMaxNumberOfChannels ) )
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_MAX_ECHO_CHANNELS_VALUE; 
	
	/* Initialize the memory for all required channels. */
	for( i = 0; i < f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels; i++ )
	{
		/*==============================================================================*/
		/*	Configure the Global Static Configuration memory of the channel. */

		ulBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( i * cOCT6100_CHANNEL_ROOT_SIZE ) + cOCT6100_CHANNEL_ROOT_GLOBAL_CONF_OFFSET;

		/* Set the PGSP context base address. */
		ulTempData = pSharedInfo->MemoryMap.ulChanMainMemBase + ( i * pSharedInfo->MemoryMap.ulChanMainMemSize ) + cOCT6100_CH_MAIN_PGSP_CONTEXT_OFFSET;
		
		WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_PGSP_CONTEXT_BASE_ADD_OFFSET;
		WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Set the PGSP init context base address. */
		ulTempData = ( cOCT6100_IMAGE_FILE_BASE + 0x200 ) & 0x07FFFFFF;
		
		WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_PGSP_INIT_CONTEXT_BASE_ADD_OFFSET;
		WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Set the RIN circular buffer base address. */
		ulTempData = pSharedInfo->MemoryMap.ulChanMainMemBase + ( i * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainRinCBMemOfst;

		/* Set the circular buffer size. */
		ulTempData &= 0xFFFFFF00;
		if (( pSharedInfo->MemoryMap.ulChanMainRinCBMemSize & 0xFFFF00FF ) != 0 )
			return cOCT6100_ERR_CHANNEL_INVALID_RIN_CB_SIZE;
		ulTempData |= pSharedInfo->MemoryMap.ulChanMainRinCBMemSize >> 8;
			
		WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_RIN_CIRC_BUFFER_BASE_ADD_OFFSET;
		WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Set the SIN circular buffer base address. */
		ulTempData = pSharedInfo->MemoryMap.ulChanMainMemBase + ( i * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainSinCBMemOfst;

		WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_SIN_CIRC_BUFFER_BASE_ADD_OFFSET;
		WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Set the SOUT circular buffer base address. */
		ulTempData = pSharedInfo->MemoryMap.ulChanMainMemBase + ( i * pSharedInfo->MemoryMap.ulChanMainMemSize ) + pSharedInfo->MemoryMap.ulChanMainSoutCBMemOfst;;

		WriteParams.ulWriteAddress = ulBaseAddress + cOCT6100_GSC_SOUT_CIRC_BUFFER_BASE_ADD_OFFSET;
		WriteParams.usWriteData = (UINT16)( ulTempData >> 16 );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = (UINT16)( ulTempData & 0xFFFF );

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;

		/*==============================================================================*/
	}

	/* Put all channel in powerdown mode "3". */
	for( i = 0; i < f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels; i++ )
	{
		WriteParams.ulWriteAddress = 0x014000 + (i*4) + 0;
		WriteParams.usWriteData = 0x85FF;		/* TSI index 1535 reserved for power-down mode */

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x014000 + (i*4) + 2;
		WriteParams.usWriteData = 0xC5FF;		/* TSI index 1535 reserved for power-down mode */

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Set the maximum number of channels. */
	WriteParams.ulWriteAddress = 0x690;
	if ( pSharedInfo->ImageInfo.usMaxNumberOfChannels < 384 )
		WriteParams.usWriteData = 384;
	else
		WriteParams.usWriteData = (UINT16)pSharedInfo->ImageInfo.usMaxNumberOfChannels;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Set power-dowm TSI chariot memory to silence. */
	for( i = 0; i < 6; i++ )
	{
		WriteParams.ulWriteAddress = 0x20000 + ( i * 0x1000 ) + ( 1534 * 2 );
		WriteParams.usWriteData = 0x3EFF;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x20000 + ( i * 0x1000 ) + ( 1535 * 2 );
		WriteParams.usWriteData = 0x3EFF;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}

	/* Remove chariot hold. */
	WriteParams.ulWriteAddress = 0x500;
	WriteParams.usWriteData = 0x0001;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	for( usLoopCount = 0; usLoopCount < 4096; usLoopCount++ )
	{
		if ( (usLoopCount % 16) < 8 )
		{
			usWriteData  = (UINT16)((usLoopCount / 16) << 7);
			usWriteData |= (UINT16)((usLoopCount % 8));
		}
		else
		{
			usWriteData  = (UINT16)((usLoopCount / 16) << 7);
			usWriteData |= (UINT16)((usLoopCount % 8));
			usWriteData |= 0x78;
		}

		/* Set timeslot pointer. */
		WriteParams.ulWriteAddress = 0x50E;
		WriteParams.usWriteData  = 0x0003;
		WriteParams.usWriteData |= usWriteData << 2;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Now read the mclk counter. */
		ReadParams.ulReadAddress = 0x30A;
		ReadParams.pusReadData = &usLastMclkRead;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Reset loop timeout counter. */
		ulTimeoutCounter = 0x0;

		do {
			ReadParams.pusReadData = &usMclkRead;

			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			if ( ( usLoopCount % 16 ) != 15 )
			{
				ulNumberOfCycleToWait = 133;
			}
			else
			{
				ulNumberOfCycleToWait = 20000;
			}

			/* Evaluate the difference. */
			usMclkDiff = (UINT16)(( usMclkRead - usLastMclkRead ) & 0xFFFF);

			/* Check for loop timeout. Bad mclk? */
			ulTimeoutCounter++;
			if ( ulTimeoutCounter == cOCT6100_MAX_LOOP_CPU_TIMEOUT )
				return cOCT6100_ERR_FATAL_EA;
			
		} while( usMclkDiff <= ulNumberOfCycleToWait );
	}
	
	/* Back to normal mode. */
	WriteParams.ulWriteAddress = 0x50E;
	WriteParams.usWriteData  = 0x0000;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check for CRC errors. */
	ReadParams.pusReadData = &usReadData;
	ReadParams.ulReadAddress = 0x202;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( (usReadData & 0x400) != 0x0000 )
		return cOCT6100_ERR_OPEN_CRC_ERROR;

	/* Clear the error rol raised by manually moving the clocks. */
	WriteParams.ulWriteAddress = 0x502;
	WriteParams.usWriteData  = 0x0002;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/*======================================================================*/
	/* Write the tail displacement value in external memory. */

	ulFeatureBytesOffset = pSharedInfo->MemoryMap.PouchTailDisplOfst.usDwordOffset * 4;
	ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PouchTailDisplOfst.byBitOffset;
	ulFeatureFieldLength = pSharedInfo->MemoryMap.PouchTailDisplOfst.byFieldSize;

	ulResult = Oct6100ApiReadDword(	f_pApiInstance,
									cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
									&ulTempData );
	
	/* Clear previous value set in the feature field.*/
	mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

	ulTempData &= (~ulMask);

	/* Set the tail displacement. */
	ulTempData |= (pSharedInfo->ChipConfig.usTailDisplacement << ulFeatureBitOffset );

	/* Write the DWORD where the field is located. */
	ulResult = Oct6100ApiWriteDword( f_pApiInstance,
									 cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
									 ulTempData );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	/*======================================================================*/


	/*======================================================================*/
	/* Clear the pouch counter, if present. */
	
	if ( pSharedInfo->DebugInfo.fPouchCounter == TRUE )
	{
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.PouchCounterFieldOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byFieldSize;

		ulResult = Oct6100ApiReadDword(	f_pApiInstance,
										cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
										&ulTempData );
		
		/* Clear previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		/* Clear counter! */
		ulTempData &= (~ulMask);

		/* Write the DWORD where the field is located.*/
		ulResult = Oct6100ApiWriteDword( f_pApiInstance,
										 cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
										 ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}

	/* The ISR has not yet been called.  Set the appropriate bit in external memory. */
	if ( pSharedInfo->DebugInfo.fIsIsrCalledField == TRUE )
	{
		ulFeatureBytesOffset = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.usDwordOffset * 4;
		ulFeatureBitOffset	 = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.byBitOffset;
		ulFeatureFieldLength = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.byFieldSize;

		ulResult = Oct6100ApiReadDword(	f_pApiInstance,
										cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
										&ulTempData );
		
		/* Read previous value set in the feature field.*/
		mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

		/* Toggle the bit to '1'. */
		ulTempData |= 1 << ulFeatureBitOffset;

		/* Write the DWORD where the field is located.*/
		ulResult = Oct6100ApiWriteDword( f_pApiInstance,
										 cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
										 ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	
	}

	/*======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitToneInfo

Description:    This function will parse the software image and retrieve 
				the information about the tones that it supports.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitToneInfo
UINT32 Oct6100ApiInitToneInfo(
				IN OUT	tPOCT6100_INSTANCE_API f_pApiInstance )
{
	UINT32	ulResult;
	
	unsigned char const	*pszToneInfoStart = NULL;
	unsigned char const	*pszToneInfoEnd = NULL;
	
	unsigned char const 	*pszCurrentInfo;
	unsigned char const	*pszNextInfo;
	
	UINT32	ulToneEventNumber;
	UINT32	ulTempValue;
	UINT32	ulNumCharForValue;
	UINT32	ulUniqueToneId;
	UINT32	ulToneNameSize;
	UINT32	ulOffset = 0;

	UINT32	i;

	/* Init the tone detector parameter. */
	f_pApiInstance->pSharedInfo->ImageInfo.byNumToneDetectors = 0;

	/* Find the start and the end of the tone info section. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize > 4096 )
	{
		/* For performance reasons, and since the tone detector information */
		/* is always located at the end of the image file, try to start from the end */
		/* of the buffer. */

		ulOffset = f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize - 2048;
		pszToneInfoStart = Oct6100ApiStrStr( f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + ulOffset,
										 (PUINT8)cOCT6100_TONE_INFO_START_STRING,
										 f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize );

		/* Check if the information was found. */
		if ( pszToneInfoStart == NULL )
		{
			/* Try again, but giving a larger string to search. */
			ulOffset = f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize - 4096;
			pszToneInfoStart = Oct6100ApiStrStr( f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + ulOffset,
											 (PUINT8)cOCT6100_TONE_INFO_START_STRING,
											 f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize );

		}
	}

	if ( pszToneInfoStart == NULL )
	{
		/* Travel through the whole file buffer. */
		pszToneInfoStart = Oct6100ApiStrStr( f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile,
										 (PUINT8)cOCT6100_TONE_INFO_START_STRING,
										 f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize );
	}
	/* We have to return immediatly if no tones are found. */
	if ( pszToneInfoStart == NULL )
		return cOCT6100_ERR_OK;

	/* The end of the tone detector information is after the beginning of the tone information. */
	pszToneInfoEnd = Oct6100ApiStrStr(	 pszToneInfoStart,
										 (PUINT8)cOCT6100_TONE_INFO_STOP_STRING,
										 f_pApiInstance->pSharedInfo->ChipConfig.pbyImageFile + f_pApiInstance->pSharedInfo->ChipConfig.ulImageSize );
	if ( pszToneInfoEnd == NULL )
		return cOCT6100_ERR_OPEN_TONE_INFO_STOP_TAG_NOT_FOUND;

	/* Find and process all tone events within the region. */
	pszCurrentInfo = Oct6100ApiStrStr( pszToneInfoStart, (PUINT8)cOCT6100_TONE_INFO_EVENT_STRING, pszToneInfoEnd );

	while ( pszCurrentInfo != NULL )
	{
		/* Skip the string. */
		pszCurrentInfo += ( Oct6100ApiStrLen( (PUINT8)cOCT6100_TONE_INFO_EVENT_STRING ) );

		/* Extract the number of char used to represent the tone event number ( 1 or 2 ). */
		pszNextInfo = Oct6100ApiStrStr( pszCurrentInfo, (PUINT8)",", pszToneInfoEnd );
		ulNumCharForValue = (UINT32)( pszNextInfo - pszCurrentInfo );
		
		/* Retreive the event number */
		ulToneEventNumber = 0;
		for ( i = ulNumCharForValue; i > 0; i-- )
		{
			ulResult = Oct6100ApiAsciiToHex( *pszCurrentInfo, &ulTempValue );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			ulToneEventNumber |= ( ulTempValue << (( i - 1) * 4 ) );
			pszCurrentInfo++;
		}

		if ( ulToneEventNumber >= cOCT6100_MAX_TONE_EVENT )
			return cOCT6100_ERR_OPEN_INVALID_TONE_EVENT;
		
		/* Skip the comma and the 0x. */
		pszCurrentInfo += 3;

		/*======================================================================*/
		/* Retreive the unique tone id. */
		ulUniqueToneId = 0;
		for ( i = 0; i < 8; i++ )
		{
			ulResult = Oct6100ApiAsciiToHex( *pszCurrentInfo, &ulTempValue );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			ulOffset = 28 - ( i * 4 );
			ulUniqueToneId |= ( ulTempValue << ulOffset );
			pszCurrentInfo++;
		}
		
		/*======================================================================*/

		/* Skip the comma. */
		pszCurrentInfo++;
		
		/* Find out where the next event info starts */
		pszNextInfo = Oct6100ApiStrStr( pszCurrentInfo,(PUINT8) cOCT6100_TONE_INFO_EVENT_STRING, pszToneInfoEnd );
		if ( pszNextInfo == NULL )
			pszNextInfo = pszToneInfoEnd;

		/* Extract the name size. */
		ulToneNameSize = (UINT32)( pszNextInfo - pszCurrentInfo - 2 );	/* - 2 for 0x0D and 0x0A.*/

		if ( ulToneNameSize > cOCT6100_TLV_MAX_TONE_NAME_SIZE )
			return cOCT6100_ERR_OPEN_INVALID_TONE_NAME;

		/* Copy the tone name into the image info structure. */
		ulResult = Oct6100UserMemCopy( f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].aszToneName,
									   pszCurrentInfo,
									   ulToneNameSize );



		/* Update the tone info into the image info structure. */
		f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulToneID = ulUniqueToneId;
		/* Find out the port on which this tone detector is associated. */
		switch( (ulUniqueToneId >> 28) & 0xF )
		{
		case 1:
			f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulDetectionPort = cOCT6100_CHANNEL_PORT_ROUT;
			break;

		case 2:
			f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulDetectionPort = cOCT6100_CHANNEL_PORT_SIN;
			break;

		case 4:
			f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulDetectionPort = cOCT6100_CHANNEL_PORT_SOUT;
			break;

		case 5:
			f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulDetectionPort = cOCT6100_CHANNEL_PORT_ROUT_SOUT;
			break;
		
		default:
			f_pApiInstance->pSharedInfo->ImageInfo.aToneInfo[ ulToneEventNumber ].ulDetectionPort = cOCT6100_INVALID_PORT;
			break;
		}
		
		/* Find out where the next event info starts */
		pszNextInfo = Oct6100ApiStrStr( pszCurrentInfo,(PUINT8) cOCT6100_TONE_INFO_EVENT_STRING, pszToneInfoEnd );
		/* Update the current info pointer. */
		pszCurrentInfo = pszNextInfo;

		f_pApiInstance->pSharedInfo->ImageInfo.byNumToneDetectors++;
	}

	return	cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiExternalMemoryBist

Description:    Tests the functionality of the external memories.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiExternalMemoryBist
UINT32 Oct6100ApiExternalMemoryBist(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulMemSize = 0;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Test the external memory. */
	switch ( pSharedInfo->ChipConfig.ulMemoryChipSize )
	{
	case cOCT6100_MEMORY_CHIP_SIZE_8MB:			
		ulMemSize = cOCT6100_SIZE_8M;		
		break;
	case cOCT6100_MEMORY_CHIP_SIZE_16MB:		
		ulMemSize = cOCT6100_SIZE_16M;		
		break;
	case cOCT6100_MEMORY_CHIP_SIZE_32MB:		
		ulMemSize = cOCT6100_SIZE_32M;		
		break;
	case cOCT6100_MEMORY_CHIP_SIZE_64MB:		
		ulMemSize = cOCT6100_SIZE_64M;		
		break;
	case cOCT6100_MEMORY_CHIP_SIZE_128MB:		
		ulMemSize = cOCT6100_SIZE_128M;		
		break;
	default:									
		return cOCT6100_ERR_FATAL_D9;
	}

	ulMemSize *= pSharedInfo->ChipConfig.byNumMemoryChips;

	ulResult = Oct6100ApiRandomMemoryWrite( f_pApiInstance, cOCT6100_EXTERNAL_MEM_BASE_ADDRESS, ulMemSize, 16, 1000, cOCT6100_ERR_OPEN_EXTERNAL_MEM_BIST_FAILED );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Make sure the user I/O functions are working as required. */
	ulResult = Oct6100ApiUserIoTest( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGenerateNumber

Description:    Generate a number using an index.  Passing the same
				index generates the same number.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.
f_ulIndex			Index used to generate the random number.
f_ulDataMask		Data mask to apply to generated number.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGenerateNumber
UINT16 Oct6100ApiGenerateNumber( 
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulIndex,
				IN		UINT32					f_ulDataMask )
{
	UINT16 usGeneratedNumber;

	usGeneratedNumber = (UINT16)( ( ( ~( f_ulIndex - 1 ) ) & 0xFF00 ) | ( ( f_ulIndex + 1 ) & 0xFF ) );

	return (UINT16)( usGeneratedNumber & f_ulDataMask );
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiRandomMemoryWrite

Description:    Writes to f_ulNumAccesses random locations in the indicated 
				memory and read back to test the operation of that memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.
f_ulMemBase			Base address of the memory access.
f_ulMemSize			Size of the memory to be tested.
f_ulNumDataBits		Number of data bits.
f_ulNumAccesses		Number of random access to be perform.
f_ulErrorCode		Error code to be returned if the bist fails.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiRandomMemoryWrite
UINT32 Oct6100ApiRandomMemoryWrite(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulMemBase,
				IN		UINT32					f_ulMemSize,
				IN		UINT32					f_ulNumDataBits,
				IN		UINT32					f_ulNumAccesses,
				IN		UINT32					f_ulErrorCode )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	ulDataMask;
	UINT32	ulResult, i, j;
	UINT32	ulBistAddress;
	UINT16	usReadData;
	UINT32	aulBistAddress[20]={0x00000000, 0x00000002, 0x00000004, 0x007FFFFE,
		                        0x00900000, 0x00900006, 0x00900008, 0x009FFFFE,
								0x01000000, 0x0100000A, 0x0200000C, 0x01FFFFFE,
								0x03000000, 0x03000002, 0x04000004, 0x03FFFFFE,
								0x04000000, 0x05000006, 0x06000008, 0x07FFFFFE};	

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Determine mask for number of data bits. */
	ulDataMask = (1 << f_ulNumDataBits) - 1;
	
	/* Write specific data to specific address */
	WriteParams.ulWriteAddress = f_ulMemBase | 0x00001000;
	WriteParams.usWriteData = 0xCAFE;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	for(j=0; j<20; j++)
	{		
		/* Change address to test lower and higher part of the 32 bit bus */
		ulBistAddress  = aulBistAddress[j];		
		ulBistAddress &= f_ulMemSize - 2;
		ulBistAddress |= f_ulMemBase;

		/* Bist 16 data pins of this address */
		for ( i = 0; i < 16; i ++)
		{
			WriteParams.ulWriteAddress = ulBistAddress;
			WriteParams.usWriteData = (UINT16)(0x1 << i);

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Read back the specific data to flush the data bus.*/
			ReadParams.ulReadAddress = f_ulMemBase | 0x00001000;
			ReadParams.pusReadData = &usReadData;
			mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			if ( usReadData != 0xCAFE )
				return f_ulErrorCode;
			
			/* Read back the data written.*/
			ReadParams.ulReadAddress = WriteParams.ulWriteAddress;
			ReadParams.pusReadData = &usReadData;
			mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			if ( usReadData != (UINT16)(0x1 << i) )
				return f_ulErrorCode;
		}
	}

	/* Perform the first write at address 0 + mem base */
	j = 0;
	WriteParams.ulWriteAddress = f_ulMemBase;
	WriteParams.usWriteData = Oct6100ApiGenerateNumber( f_pApiInstance, j, ulDataMask );
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Try each address line of the memory. */
	for ( i = 2, j = 1; i < f_ulMemSize; i <<= 1, j++ )
	{
		WriteParams.ulWriteAddress = ( f_ulMemBase + i );
		WriteParams.usWriteData = Oct6100ApiGenerateNumber( f_pApiInstance, j, ulDataMask );
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	for ( i = 0; i < j; i++ )
	{
		if ( i > 0 )
			ReadParams.ulReadAddress = ( f_ulMemBase + ( 0x1 << i ) );
		else
			ReadParams.ulReadAddress = f_ulMemBase;
		ReadParams.pusReadData = &usReadData;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( usReadData != Oct6100ApiGenerateNumber( f_pApiInstance, i, ulDataMask ) )
			return f_ulErrorCode;
	}

	/* Write to random addresses of the memory. */
	for ( i = 0; i < f_ulNumAccesses; i++ )
	{
		ulBistAddress  = (UINT16)Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF ) << 16;
		ulBistAddress |= (UINT16)Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF );
		ulBistAddress &= f_ulMemSize - 2;
		ulBistAddress |= f_ulMemBase;

		WriteParams.ulWriteAddress = ulBistAddress;
		WriteParams.usWriteData = Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF );
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	for ( i = 0; i < f_ulNumAccesses; i++ )
	{
		ulBistAddress  = (UINT16)Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF ) << 16;
		ulBistAddress |= (UINT16)Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF );
		ulBistAddress &= f_ulMemSize - 2;
		ulBistAddress |= f_ulMemBase;

		ReadParams.ulReadAddress = ulBistAddress;
		ReadParams.pusReadData = &usReadData;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		if ( ( usReadData & ulDataMask ) != ( Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF ) & ulDataMask ) )
			return f_ulErrorCode;
	}
	
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUserIoTest

Description:    This function will verify the correct functionality of 
				the following user functions:

				- Oct6100UserDriverWriteBurstApi
				- Oct6100UserDriverWriteSmearApi
				- Oct6100UserDriverReadBurstApi

				The Oct6100UserDriverWriteApi and Oct6100UserDriverReadApi
				functions do not need to be tested here as this has be done in 
				the external memory bisting function above.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUserIoTest
UINT32 Oct6100ApiUserIoTest( 
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_BURST_PARAMS	WriteBurstParams;
	tOCT6100_WRITE_SMEAR_PARAMS	WriteSmearParams;
	tOCT6100_READ_PARAMS		ReadParams;
	tOCT6100_READ_BURST_PARAMS	ReadBurstParams;
	UINT32	ulResult, i;
	UINT16	usReadData;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteBurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteBurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	/* Test what the user has specified is the maximum that can be used for a burst. */
	WriteBurstParams.ulWriteLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
	WriteBurstParams.pusWriteData = pSharedInfo->MiscVars.ausSuperArray;

	WriteSmearParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteSmearParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	/* Test what the user has specified is the maximum that can be used for a smear. */
	WriteSmearParams.ulWriteLength = pSharedInfo->ChipConfig.usMaxRwAccesses;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	ReadBurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadBurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	/* Test what the user has specified is the maximum that can be used for a burst. */
	ReadBurstParams.ulReadLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
	ReadBurstParams.pusReadData = pSharedInfo->MiscVars.ausSuperArray;


	/*======================================================================*/
	/* Write burst check. */

	WriteBurstParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;
	/* Set the random data to be written. */
	for ( i = 0; i < WriteBurstParams.ulWriteLength; i++ )
	{
		WriteBurstParams.pusWriteData[ i ] = Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF );
	}
	mOCT6100_DRIVER_WRITE_BURST_API( WriteBurstParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Read back pattern using simple read function and make sure we are reading what's expected. */
	ReadParams.ulReadAddress = WriteBurstParams.ulWriteAddress;
	for ( i = 0; i < WriteBurstParams.ulWriteLength; i++ )
	{
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if the data matches. */
		if ( usReadData != WriteBurstParams.pusWriteData[ i ] )
		{
			/* The values do not match.  Something seems to be wrong with the WriteBurst user function. */
			return cOCT6100_ERR_OPEN_USER_WRITE_BURST_FAILED;
		}

		/* Next address to check. */
		ReadParams.ulReadAddress += 2;
	}

	/*======================================================================*/


	/*======================================================================*/
	/* Write smear check. */

	WriteSmearParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS + ( WriteBurstParams.ulWriteLength * 2 );
	/* Set the random data to be written. */
	WriteSmearParams.usWriteData = Oct6100ApiGenerateNumber( f_pApiInstance, Oct6100ApiRand( 0xFFFF ), 0xFFFF );
	mOCT6100_DRIVER_WRITE_SMEAR_API( WriteSmearParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Read back pattern using simple read function and make sure we are reading what's expected. */
	ReadParams.ulReadAddress = WriteSmearParams.ulWriteAddress;
	for ( i = 0; i < WriteSmearParams.ulWriteLength; i++ )
	{
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if the data matches. */
		if ( usReadData != WriteSmearParams.usWriteData )
		{
			/* The values do not match.  Something seems to be wrong with the WriteSmear user function. */
			return cOCT6100_ERR_OPEN_USER_WRITE_SMEAR_FAILED;
		}

		/* Next address to check. */
		ReadParams.ulReadAddress += 2;
	}

	/*======================================================================*/


	/*======================================================================*/
	/* Read burst check. */

	/* First check with what the WriteBurst function wrote. */
	ReadBurstParams.ulReadAddress = WriteBurstParams.ulWriteAddress;
	mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	for ( i = 0; i < ReadBurstParams.ulReadLength; i++ )
	{
		/* Check if the data matches. */
		if ( ReadBurstParams.pusReadData[ i ] != Oct6100ApiGenerateNumber( f_pApiInstance, i, 0xFFFF ) )
		{
			/* The values do not match.  Something seems to be wrong with the ReadBurst user function. */
			return cOCT6100_ERR_OPEN_USER_READ_BURST_FAILED;
		}
	}

	/* Then check with what the WriteSmear function wrote. */
	ReadBurstParams.ulReadAddress = WriteSmearParams.ulWriteAddress;
	mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	for ( i = 0; i < ReadBurstParams.ulReadLength; i++ )
	{
		/* Check if the data matches. */
		if ( ReadBurstParams.pusReadData[ i ] != WriteSmearParams.usWriteData )
		{
			/* The values do not match.  Something seems to be wrong with the ReadBurst user function. */
			return cOCT6100_ERR_OPEN_USER_READ_BURST_FAILED;
		}
	}

	/*======================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiExternalMemoryInit

Description:    Initialize the external memory before uploading the image.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiExternalMemoryInit
UINT32 Oct6100ApiExternalMemoryInit(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_SMEAR_PARAMS		SmearParams;
	UINT32	ulTotalWordToWrite;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	SmearParams.pProcessContext = f_pApiInstance->pProcessContext;

	SmearParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Clear the first part of the memory. */
	ulTotalWordToWrite = 0x400;
	SmearParams.ulWriteAddress = cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;

	while ( ulTotalWordToWrite != 0 )
	{
		if ( ulTotalWordToWrite >= pSharedInfo->ChipConfig.usMaxRwAccesses )
			SmearParams.ulWriteLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
		else
			SmearParams.ulWriteLength = ulTotalWordToWrite;

		SmearParams.usWriteData = 0x0;

		mOCT6100_DRIVER_WRITE_SMEAR_API( SmearParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Update the number of words to write. */
		ulTotalWordToWrite -= SmearParams.ulWriteLength;
		/* Update the address. */
		SmearParams.ulWriteAddress += ( SmearParams.ulWriteLength * 2 );
	}

	/* Clear the TLV flag.*/
	ulResult = Oct6100ApiWriteDword( f_pApiInstance, cOCT6100_TLV_BASE, 0x0 );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitMixer

Description:    This function will initialize the mixer memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitMixer
UINT32 Oct6100ApiInitMixer(
				IN OUT	tPOCT6100_INSTANCE_API f_pApiInstance )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	UINT16						ausWriteData[ 4 ];
	UINT32						ulResult;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	BurstParams.pusWriteData = ausWriteData;
	/*======================================================================*/
	/* Initialize the mixer memory if required. */
	if ( pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
	{
		/* Modify the mixer pointer by adding the record event into the link list. */
		pSharedInfo->MixerInfo.usFirstSinCopyEventPtr	= pSharedInfo->MixerInfo.usRecordSinEventIndex;
		pSharedInfo->MixerInfo.usLastSinCopyEventPtr	= pSharedInfo->MixerInfo.usRecordSinEventIndex;
		pSharedInfo->MixerInfo.usFirstSoutCopyEventPtr	= pSharedInfo->MixerInfo.usRecordCopyEventIndex; 
		pSharedInfo->MixerInfo.usLastSoutCopyEventPtr	= pSharedInfo->MixerInfo.usRecordCopyEventIndex;

		/* Program the Sin copy event. */
		BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordSinEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		BurstParams.ulWriteLength = 4;

		ausWriteData[ 0 ] = 0x0000;
		ausWriteData[ 1 ] = 0x0000;
		ausWriteData[ 2 ] = (UINT16)(cOCT6100_MIXER_TAIL_NODE & 0x7FF);	/* Head node.*/
		ausWriteData[ 3 ] = 0x0000;

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Program the Sout copy event. */
		BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pSharedInfo->MixerInfo.usRecordCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		BurstParams.ulWriteLength = 4;

		ausWriteData[ 0 ] = 0x0000;
		ausWriteData[ 1 ] = 0x0000;
		ausWriteData[ 2 ] = (UINT16)(pSharedInfo->MixerInfo.usRecordSinEventIndex & 0x7FF);
		ausWriteData[ 3 ] = 0x0000;

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Configure the head node. */
		BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE;
		BurstParams.ulWriteLength = 4;

		ausWriteData[ 0 ] = 0x0000;
		ausWriteData[ 1 ] = 0x0000;
		ausWriteData[ 2 ] = (UINT16)(pSharedInfo->MixerInfo.usRecordCopyEventIndex & 0x7FF);
		ausWriteData[ 3 ] = 0x0000;

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Init the mixer pointer */
		pSharedInfo->MixerInfo.usFirstSinCopyEventPtr = pSharedInfo->MixerInfo.usRecordSinEventIndex;
	}
	else
	{
		/* Configure the head node. */
		BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE;
		BurstParams.ulWriteLength = 4;

		ausWriteData[ 0 ] = 0x0000;
		ausWriteData[ 1 ] = 0x0000;
		ausWriteData[ 2 ] = (UINT16)(cOCT6100_MIXER_TAIL_NODE & 0x7FF);	/* Head node. */
		ausWriteData[ 3 ] = 0x0000;

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Configure the tail node. */
		BurstParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + 0x10;
		BurstParams.ulWriteLength = 4;

		ausWriteData[ 0 ] = 0x0000;
		ausWriteData[ 1 ] = 0x0000;
		ausWriteData[ 2 ] = (UINT16)(cOCT6100_MIXER_HEAD_NODE & 0x7FF);	/* Head node. */
		ausWriteData[ 3 ] = 0x0000;

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInitRecordResources

Description:    This function will initialize the resources required to 
				perform recording on a debug channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInitRecordResources
UINT32 Oct6100ApiInitRecordResources(
				IN OUT	tPOCT6100_INSTANCE_API f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32					ulResult;

	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check if recording is enabled. */
	if ( pSharedInfo->ChipConfig.fEnableChannelRecording == FALSE )
		return cOCT6100_ERR_OK;

	if ( pSharedInfo->DebugInfo.usRecordMemIndex == cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_NOT_SUPPORTED_OPEN_DEBUG_RECORD;
	
	/* Check the provided recording memory index within the SSPX. */
	if ( pSharedInfo->DebugInfo.usRecordMemIndex != ( pSharedInfo->ImageInfo.usMaxNumberOfChannels - 1 ) )
		return cOCT6100_ERR_OPEN_DEBUG_MEM_INDEX;

	/* Reserve the TSI entries for the channel. */
	ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, &pSharedInfo->DebugInfo.usRecordRinRoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	ulResult = Oct6100ApiReserveTsiMemEntry( f_pApiInstance, &pSharedInfo->DebugInfo.usRecordSinSoutTsiMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Open the debug channel. */
	ulResult = Oct6100ApiDebugChannelOpen( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100FreeResourcesSer

Description:    This function closes all opened channels and frees all 
				specified global resources used by the chip.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pFreeResources		Pointer to user structure in which to choose what 
						to free.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100FreeResourcesSer
UINT32 Oct6100FreeResourcesSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_FREE_RESOURCES			f_pFreeResources )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pChanEntry;
	
	UINT32	ulResult;
	UINT32	i;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Close all bidirectional channels. */
	for ( i = 0; i < pSharedInfo->ChipConfig.usMaxBiDirChannels; i ++ )
	{
		tPOCT6100_API_BIDIR_CHANNEL		pBiDirChanEntry;

		mOCT6100_GET_BIDIR_CHANNEL_ENTRY_PNT( pSharedInfo, pBiDirChanEntry, i );

		if ( pBiDirChanEntry->fReserved == TRUE )
		{
			tOCT6100_CHANNEL_DESTROY_BIDIR DestroyBidir;

			Oct6100ChannelDestroyBiDirDef( &DestroyBidir );

			DestroyBidir.ulBiDirChannelHndl = cOCT6100_HNDL_TAG_BIDIR_CHANNEL | (pBiDirChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

			ulResult = Oct6100ChannelDestroyBiDirSer( f_pApiInstance, &DestroyBidir );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Close all bridge participants. */
	for ( i = 0; i < pSharedInfo->ChipConfig.usMaxChannels; i ++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, i  );
		if ( pChanEntry->fReserved == TRUE && pChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		{
			/* This channel is on a bridge. */
			tOCT6100_CONF_BRIDGE_CHAN_REMOVE	BridgeChanRemove;
			tPOCT6100_API_CONF_BRIDGE			pBridgeEntry;

			Oct6100ConfBridgeChanRemoveDef( &BridgeChanRemove );

			/* Obtain a pointer to the conference bridge's list entry. */
			mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, pChanEntry->usBridgeIndex );

			BridgeChanRemove.fRemoveAll = TRUE;
			BridgeChanRemove.ulConfBridgeHndl = cOCT6100_HNDL_TAG_CONF_BRIDGE | (pBridgeEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | pChanEntry->usBridgeIndex;

			ulResult = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, &BridgeChanRemove );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Close all opened channels.  This will bring the broadcast TSSTs with it. */
	for ( i = 0; i < pSharedInfo->ChipConfig.usMaxChannels; i ++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, i  );

		if ( pChanEntry->fReserved == TRUE )
		{
			tOCT6100_CHANNEL_CLOSE	ChannelClose;

			/* Generate handle. */
			ChannelClose.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | (pChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

			/* Call serialized close channel function. */
			ulResult = Oct6100ChannelCloseSer( f_pApiInstance, &ChannelClose );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	/* Close all TSI connections. */
	if ( f_pFreeResources->fFreeTsiConnections == TRUE )
	{
		tPOCT6100_API_TSI_CNCT		pTsiCnct;
		tOCT6100_TSI_CNCT_CLOSE		TsiCnctClose;

		Oct6100TsiCnctCloseDef( &TsiCnctClose );

		for ( i = 0; i < pSharedInfo->ChipConfig.usMaxTsiCncts; i ++ )
		{
			/* Obtain a pointer to the TSI connection list entry. */
			mOCT6100_GET_TSI_CNCT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTsiCnct, i );

			if ( pTsiCnct->fReserved == TRUE )
			{
				TsiCnctClose.ulTsiCnctHndl = cOCT6100_HNDL_TAG_TSI_CNCT | (pTsiCnct->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

				ulResult = Oct6100TsiCnctCloseSer( f_pApiInstance, &TsiCnctClose );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}
	/* Close all conference bridges. */
	if ( f_pFreeResources->fFreeConferenceBridges == TRUE )
	{
		tPOCT6100_API_CONF_BRIDGE	pConfBridge;
		tOCT6100_CONF_BRIDGE_CLOSE	ConfBridgeClose;

		Oct6100ConfBridgeCloseDef( &ConfBridgeClose );

		for ( i = 0; i < pSharedInfo->ChipConfig.usMaxConfBridges; i ++ )
		{
			/* Obtain a pointer to the conference bridge's list entry. */
			mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pConfBridge, i );

			if ( pConfBridge->fReserved == TRUE )
			{
				ConfBridgeClose.ulConfBridgeHndl = cOCT6100_HNDL_TAG_CONF_BRIDGE | (pConfBridge->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

				ulResult = Oct6100ConfBridgeCloseSer( f_pApiInstance, &ConfBridgeClose );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}

	/* Free all playout buffers loaded in external memory. */
	if ( f_pFreeResources->fFreePlayoutBuffers == TRUE )
	{
		tPOCT6100_API_BUFFER pBuffer;
		tOCT6100_BUFFER_UNLOAD BufferUnload;

		Oct6100BufferPlayoutUnloadDef( &BufferUnload );

		for ( i = 0; i < pSharedInfo->ChipConfig.usMaxPlayoutBuffers; i ++ )
		{


			/* Obtain a pointer to the buffer list entry. */
			mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBuffer, i );

			if ( pBuffer->fReserved == TRUE )
			{
				BufferUnload.ulBufferIndex = i;
				ulResult = Oct6100BufferUnloadSer( f_pApiInstance, &BufferUnload, TRUE );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}

	/* Close all phasing TSSTs. */
	if ( f_pFreeResources->fFreePhasingTssts == TRUE )
	{
		tPOCT6100_API_PHASING_TSST	pPhasingTsst;
		tOCT6100_PHASING_TSST_CLOSE	PhasingTsstClose;

		Oct6100PhasingTsstCloseDef( &PhasingTsstClose );

		for ( i = 0; i < pSharedInfo->ChipConfig.usMaxPhasingTssts; i ++ )
		{
			mOCT6100_GET_PHASING_TSST_ENTRY_PNT( pSharedInfo, pPhasingTsst, i );

			if ( pPhasingTsst->fReserved == TRUE )
			{
				PhasingTsstClose.ulPhasingTsstHndl = cOCT6100_HNDL_TAG_PHASING_TSST | (pPhasingTsst->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

				ulResult = Oct6100PhasingTsstCloseSer( f_pApiInstance, &PhasingTsstClose );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}
	/* Close all ADPCM channels. */
	if ( f_pFreeResources->fFreeAdpcmChannels == TRUE )
	{
		tPOCT6100_API_ADPCM_CHAN	pAdpcmChannel;
		tOCT6100_ADPCM_CHAN_CLOSE	AdpcmChanClose;

		Oct6100AdpcmChanCloseDef( &AdpcmChanClose );

		for ( i = 0; i < pSharedInfo->ChipConfig.usMaxAdpcmChannels; i ++ )
		{
			mOCT6100_GET_ADPCM_CHAN_ENTRY_PNT( pSharedInfo, pAdpcmChannel, i );
			if ( pAdpcmChannel->fReserved == TRUE )
			{
				AdpcmChanClose.ulChanHndl = cOCT6100_HNDL_TAG_ADPCM_CHANNEL | (pAdpcmChannel->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | i;

				ulResult = Oct6100AdpcmChanCloseSer( f_pApiInstance, &AdpcmChanClose );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ProductionBistSer

Description:    This function returns the instantaneous production BIST status.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pProductionBist		Pointer to user structure in which BIST status will
						be returned.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ProductionBistSer
UINT32 Oct6100ProductionBistSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_PRODUCTION_BIST			f_pProductionBist )
{
	UINT32 ulCalculatedCrc = cOCT6100_INVALID_VALUE;
	UINT32 ulResult;
	UINT32 ulLoopCnt = 0x0;
	UINT32 i = 1;
	UINT32 ulTotalElements = 4;
	UINT32 ulReadAddress = cOCT6100_POUCH_BASE;
	UINT32 aulMessage[ 5 ];

	/* Check if the production bist has been activated. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableProductionBist == FALSE )
		return cOCT6100_ERR_PRODUCTION_BIST_DISABLED;

	f_pProductionBist->ulCurrentAddress = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulCurrentLoop = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulCurrentTest = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulFailedAddress = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulReadValue = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulExpectedValue = cOCT6100_INVALID_VALUE;
	f_pProductionBist->ulBistStatus = cOCT6100_BIST_IN_PROGRESS;

	/* The API knows that the firmware might be writing a status event. */
	/* The firmware does write a status event every 200ms (approximately). */
	/* So the status is read a couple of times to make sure an event was not read while */
	/* it was written. */
	while ( ulLoopCnt != 2 )
	{
		/* Read the BIST status in the external memory. */
		for ( i = 0; i < ulTotalElements + 1; i ++ )
		{
			ulResult = Oct6100ApiReadDword( f_pApiInstance, ulReadAddress + i * 4, &aulMessage[ i ] );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* Calculate the CRC of this message. */
		ulResult = Oct6100ApiProductionCrc( f_pApiInstance, aulMessage, ulTotalElements, &ulCalculatedCrc );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* If the CRCs do match, break off the while.  We have a valid status event. */
		if ( aulMessage[ i - 1 ] == ulCalculatedCrc )
			break;

		ulLoopCnt++;
	}

	/* Check if the CRC matches */
	if ( aulMessage[ i - 1 ] != ulCalculatedCrc )
	{
		/* Well, the exchange memory at the base of the external memory is corrupted.  */
		/* Something very basic is not working correctly with this chip! */
		f_pProductionBist->ulBistStatus = cOCT6100_BIST_STATUS_CRC_FAILED;
	}
	else
	{
		/* Check for problems. */
		switch ( aulMessage[ 0 ] & 0xFFFF )
		{
		case ( 0x2 ):

			/* The initial configuration failed. */
			f_pProductionBist->ulBistStatus = cOCT6100_BIST_CONFIGURATION_FAILED;
			break;
			
		case ( 0x1 ):

			/* A memory location failed.  Return useful information to the user. */
			f_pProductionBist->ulBistStatus = cOCT6100_BIST_MEMORY_FAILED;

			f_pProductionBist->ulFailedAddress = ( aulMessage[ 1 ] & ( ~0x80000000 ) ) + cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;
			f_pProductionBist->ulReadValue = aulMessage[ 2 ];
			f_pProductionBist->ulExpectedValue = aulMessage[ 3 ];
			break;

		case ( 0xFFFF ):

			/* Bist is completed! */
			f_pProductionBist->ulBistStatus = cOCT6100_BIST_SUCCESS;
			break;

		default:
			/* Bist is in progress. All seems to be working fine up to now. */

			/* Return progress status. */
			f_pProductionBist->ulCurrentAddress = ( aulMessage[ 1 ] & ( ~0x80000000 ) ) + cOCT6100_EXTERNAL_MEM_BASE_ADDRESS;
			f_pProductionBist->ulCurrentTest = aulMessage[ 2 ];
			f_pProductionBist->ulCurrentLoop = aulMessage[ 3 ];
			break;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiProductionCrc

Description:    This function calculates the crc for a production BIST
				message.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pulMessage			Message to be exchanged with the firmware.  The CRC
						will be calculated on this.
f_ulMessageLength		Length of the message to be exchanged.  This value
						does not include the CRC value at the end
f_pulCrcResult			Resulting calculated CRC value.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiProductionCrc
UINT32 Oct6100ApiProductionCrc(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		PUINT32						f_pulMessage,
				IN		UINT32						f_ulMessageLength,
				OUT		PUINT32						f_pulCrcResult )
{
	UINT32	ulWidth = 32;
	UINT32	ulKey, i, j;
	UINT32	ulRemainder = 0;
	
	/* CRC the message. */
	ulRemainder = f_pulMessage[ f_ulMessageLength - 1 ]; 
	for ( j = f_ulMessageLength - 1; j != 0xFFFFFFFF ; j-- ) 
	{
		for ( i = 0; i < ulWidth; i++ )
		{			
			if ( ( ( ulRemainder >> 0x1F ) & 0x1 ) == 0x1 ) 
			{
				/* Division is by something meaningful */
				ulKey = 0x8765DCBA;
			}
			else	
			{
				/* Remainder is less than our divisor */
				ulKey = 0;
			}
			ulRemainder = ulRemainder ^ ulKey;
			
			ulRemainder = ulRemainder << 1;
			if ( j != 0 )
			{
				ulRemainder = ulRemainder | ( ( f_pulMessage[ j - 1 ] ) >> ( 0x1F - i ) );
			}
		}
	}

	*f_pulCrcResult = ulRemainder;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiClearInterrupts

Description:    Called only by the Oct6100OpenChip function, this function
				writes to all register ROLs to clear them.  This is necessary
				because some ROLs are set during the startup.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
IN f_pApiInst		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiClearInterrupts
UINT32 Oct6100ApiClearInterrupts(
					IN	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	WriteParams.ulWriteAddress = 0x102;
	WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_102H;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	WriteParams.ulWriteAddress = 0x202;
	WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_202H;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	WriteParams.ulWriteAddress = 0x302;
	WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_302H;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x502;
	WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_502H;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x702;
	WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_702H;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	return cOCT6100_ERR_OK;

}
#endif
