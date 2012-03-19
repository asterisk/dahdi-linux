/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_interrupts.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the API's interrupt service routine and all of its
	sub-functions.

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

$Octasic_Revision: 81 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"
#include "oct6100api/oct6100_apiud.h"

#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_events_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_channel_inst.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_events_priv.h"
#include "oct6100_interrupts_priv.h"

/****************************  PUBLIC FUNCTIONS  *****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100InterruptConfigure

Description:    Configure the operation of all possible interrupt sources.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntrptConfig			Pointer to interrupt configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100InterruptConfigureDef
UINT32 Oct6100InterruptConfigureDef(
				tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig )
{
	f_pIntrptConfig->ulFatalGeneralConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pIntrptConfig->ulFatalMemoryConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;

	f_pIntrptConfig->ulErrorMemoryConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pIntrptConfig->ulErrorOverflowToneEventsConfig = cOCT6100_INTERRUPT_NO_TIMEOUT;
	f_pIntrptConfig->ulErrorH100Config = cOCT6100_INTERRUPT_NO_TIMEOUT;

	f_pIntrptConfig->ulFatalMemoryTimeout = 100;

	f_pIntrptConfig->ulErrorMemoryTimeout = 100;
	f_pIntrptConfig->ulErrorOverflowToneEventsTimeout = 100;
	f_pIntrptConfig->ulErrorH100Timeout = 100;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100InterruptConfigure
UINT32 Oct6100InterruptConfigure(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32	ulResult;
	UINT32	ulFncRes;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Create serialization object for ISR. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulResult = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Call serialized sub-function. */
	ulFncRes = Oct6100InterruptConfigureSer( f_pApiInstance, f_pIntrptConfig, TRUE );
	/* Release serialization object. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulResult = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check if an error occured in sub-function. */
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100InterruptServiceRoutine

Description:    The API's interrupt service routine.  This function clears all
				register ROLs which have generated an interrupt and report the
				events in the user supplied structure.  Also, the tone event
				and/or playout event buffer will be emptied if valid events 
				are present.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntFlags				Pointer to structure containing event flags returned
						to user.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100InterruptServiceRoutineDef
UINT32 Oct6100InterruptServiceRoutineDef(
				tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags )
{
	f_pIntFlags->fFatalGeneral = FALSE;
	f_pIntFlags->ulFatalGeneralFlags = 0x0;
	f_pIntFlags->fFatalReadTimeout = FALSE;
	
	f_pIntFlags->fErrorRefreshTooLate = FALSE;
	f_pIntFlags->fErrorPllJitter = FALSE;
	
	f_pIntFlags->fErrorOverflowToneEvents = FALSE;

	f_pIntFlags->fErrorH100OutOfSync = FALSE;
	f_pIntFlags->fErrorH100ClkA = FALSE;
	f_pIntFlags->fErrorH100ClkB = FALSE;
	f_pIntFlags->fErrorH100FrameA = FALSE;

	f_pIntFlags->fToneEventsPending = FALSE;
	f_pIntFlags->fBufferPlayoutEventsPending = FALSE;

	f_pIntFlags->fApiSynch = FALSE;



	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100InterruptServiceRoutine
UINT32 Oct6100InterruptServiceRoutine(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32	ulResult;
	UINT32	ulFncRes;

	/* Set the process context of the serialize structure. */
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize the serialization object for the ISR. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulResult = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulResult == cOCT6100_ERR_OK )
	{	
		/* Call the serialized sub-function. */
		ulFncRes = Oct6100InterruptServiceRoutineSer( f_pApiInstance, f_pIntFlags );
	}
	else
	{
		return ulResult;
	}

	/* Release the serialization object. */
	ReleaseSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	ulResult = Oct6100UserReleaseSerializeObject( &ReleaseSerObj );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check for an error in the sub-function. */
	if ( ulFncRes != cOCT6100_ERR_OK )
		return ulFncRes;

	return cOCT6100_ERR_OK;
}
#endif


/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiIsrSwInit

Description:    Initializes portions of API instance associated to the API's 
				interrupt service routine.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiIsrSwInit
UINT32 Oct6100ApiIsrSwInit(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the state of each interrupt group to disabled.  The state will */
	/* be updated to the true configuration once the configure interrupts function is called. */
	pSharedInfo->IntrptManage.byFatalGeneralState = cOCT6100_INTRPT_DISABLED;
	pSharedInfo->IntrptManage.byFatalMemoryState = cOCT6100_INTRPT_DISABLED;
	pSharedInfo->IntrptManage.byErrorMemoryState = cOCT6100_INTRPT_DISABLED;
	pSharedInfo->IntrptManage.byErrorH100State = cOCT6100_INTRPT_DISABLED;
	pSharedInfo->IntrptManage.byErrorOverflowToneEventsState = cOCT6100_INTRPT_DISABLED;

	/* Indicate that the mclk interrupt is not active at the moment. */
	pSharedInfo->IntrptManage.fMclkIntrptActive = FALSE;

	/* Indicate that no buffer playout events are pending for the moment. */
	pSharedInfo->IntrptManage.fBufferPlayoutEventsPending = FALSE;

	/* Indicate that no tone events are pending for the moment. */
	pSharedInfo->IntrptManage.fToneEventsPending = FALSE;

	/* The ISR has never been called. */
	pSharedInfo->IntrptManage.fIsrCalled = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiIsrHwInit

Description:    Initializes the chip's interrupt registers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntrptConfig			Pointer to structure defining how the interrupts
						should be configured.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiIsrHwInit
UINT32 Oct6100ApiIsrHwInit(
			IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
			IN		tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulResult;

	/* Set some parameters of write struct. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/*==================================================================================*/
	/* Enable all the interrupts */
	
	WriteParams.ulWriteAddress = 0x104;
	WriteParams.usWriteData = 0x0001;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x204;
	WriteParams.usWriteData = 0x1C05;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x304;
	WriteParams.usWriteData = 0xFFFF;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x504;
	WriteParams.usWriteData = 0x0002;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	WriteParams.ulWriteAddress = 0x704;
	WriteParams.usWriteData = 0x0007;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*==================================================================================*/

	/* Calculate the number of mclk cycles in 1 ms. */
	f_pApiInstance->pSharedInfo->IntrptManage.ulNumMclkCyclesIn1Ms = f_pApiInstance->pSharedInfo->MiscVars.ulMclkFreq / 1000;

	/* Configure the interrupt registers as requested by the user. */
	ulResult = Oct6100InterruptConfigureSer( f_pApiInstance, f_pIntrptConfig, TRUE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100InterruptConfigureSer

Description:    Configure the operation of interrupt groups.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntrptConfig			Pointer to interrupt configuration structure.
f_fCheckParams			Check parameter enable flag.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100InterruptConfigureSer
UINT32 Oct6100InterruptConfigureSer(
			IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
			IN		tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig,
			IN		BOOL								f_fCheckParams )
{
	tPOCT6100_API_INTRPT_CONFIG	pIntrptConfig;
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	UINT32	ulResult;

	/* Check for errors. */
	if ( f_fCheckParams == TRUE )
	{
		if ( f_pIntrptConfig->ulFatalGeneralConfig != cOCT6100_INTERRUPT_DISABLE &&
			 f_pIntrptConfig->ulFatalGeneralConfig != cOCT6100_INTERRUPT_NO_TIMEOUT )
			return cOCT6100_ERR_INTRPTS_FATAL_GENERAL_CONFIG;
		if ( f_pIntrptConfig->ulFatalMemoryConfig != cOCT6100_INTERRUPT_DISABLE &&
			 f_pIntrptConfig->ulFatalMemoryConfig != cOCT6100_INTERRUPT_TIMEOUT &&
			 f_pIntrptConfig->ulFatalMemoryConfig != cOCT6100_INTERRUPT_NO_TIMEOUT )
			return cOCT6100_ERR_INTRPTS_FATAL_MEMORY_CONFIG;
		if ( f_pIntrptConfig->ulErrorMemoryConfig != cOCT6100_INTERRUPT_DISABLE &&
			 f_pIntrptConfig->ulErrorMemoryConfig != cOCT6100_INTERRUPT_TIMEOUT &&
			 f_pIntrptConfig->ulErrorMemoryConfig != cOCT6100_INTERRUPT_NO_TIMEOUT )
			return cOCT6100_ERR_INTRPTS_DATA_ERR_MEMORY_CONFIG;
		if ( f_pIntrptConfig->ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_DISABLE &&
			 f_pIntrptConfig->ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_TIMEOUT &&
			 f_pIntrptConfig->ulErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_NO_TIMEOUT )
			return cOCT6100_ERR_INTRPTS_OVERFLOW_TONE_EVENTS_CONFIG;
		if ( f_pIntrptConfig->ulErrorH100Config != cOCT6100_INTERRUPT_DISABLE &&
			 f_pIntrptConfig->ulErrorH100Config != cOCT6100_INTERRUPT_TIMEOUT &&
			 f_pIntrptConfig->ulErrorH100Config != cOCT6100_INTERRUPT_NO_TIMEOUT )
			return cOCT6100_ERR_INTRPTS_H100_ERROR_CONFIG;

		if ( f_pIntrptConfig->ulFatalMemoryTimeout < 10 ||
			 f_pIntrptConfig->ulFatalMemoryTimeout > 10000 )
			return cOCT6100_ERR_INTRPTS_FATAL_MEMORY_TIMEOUT;
		if ( f_pIntrptConfig->ulErrorMemoryTimeout < 10 ||
			 f_pIntrptConfig->ulErrorMemoryTimeout > 10000 )
			return cOCT6100_ERR_INTRPTS_DATA_ERR_MEMORY_TIMEOUT;
		if ( f_pIntrptConfig->ulErrorOverflowToneEventsTimeout < 10 ||
			 f_pIntrptConfig->ulErrorOverflowToneEventsTimeout > 10000 )
			return cOCT6100_ERR_INTRPTS_OVERFLOW_TONE_EVENTS_TIMEOUT;
		if ( f_pIntrptConfig->ulErrorH100Timeout < 10 ||
			 f_pIntrptConfig->ulErrorH100Timeout > 10000 )
			return cOCT6100_ERR_INTRPTS_H100_ERROR_TIMEOUT;
	}

	/* Copy the configuration to the API instance. */
	pIntrptConfig = &f_pApiInstance->pSharedInfo->IntrptConfig;
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	pIntrptConfig->byFatalGeneralConfig = (UINT8)( f_pIntrptConfig->ulFatalGeneralConfig & 0xFF );
	pIntrptConfig->byFatalMemoryConfig = (UINT8)( f_pIntrptConfig->ulFatalMemoryConfig & 0xFF );
	pIntrptConfig->byErrorMemoryConfig = (UINT8)( f_pIntrptConfig->ulErrorMemoryConfig & 0xFF );
	pIntrptConfig->byErrorOverflowToneEventsConfig = (UINT8)( f_pIntrptConfig->ulErrorOverflowToneEventsConfig & 0xFF );
	pIntrptConfig->byErrorH100Config = (UINT8)( f_pIntrptConfig->ulErrorH100Config & 0xFF );

	f_pIntrptConfig->ulFatalMemoryTimeout = ((f_pIntrptConfig->ulFatalMemoryTimeout + 9) / 10) * 10;
	pIntrptConfig->ulFatalMemoryTimeoutMclk = f_pIntrptConfig->ulFatalMemoryTimeout * pIntrptManage->ulNumMclkCyclesIn1Ms;

	f_pIntrptConfig->ulErrorMemoryTimeout = ((f_pIntrptConfig->ulErrorMemoryTimeout + 9) / 10) * 10;
	pIntrptConfig->ulErrorMemoryTimeoutMclk = f_pIntrptConfig->ulErrorMemoryTimeout * pIntrptManage->ulNumMclkCyclesIn1Ms;

	f_pIntrptConfig->ulErrorOverflowToneEventsTimeout = ((f_pIntrptConfig->ulErrorOverflowToneEventsTimeout + 9) / 10) * 10;
	pIntrptConfig->ulErrorOverflowToneEventsTimeoutMclk = f_pIntrptConfig->ulErrorOverflowToneEventsTimeout * pIntrptManage->ulNumMclkCyclesIn1Ms;

	f_pIntrptConfig->ulErrorH100Timeout = ((f_pIntrptConfig->ulErrorH100Timeout + 9) / 10) * 10;
	pIntrptConfig->ulErrorH100TimeoutMclk = f_pIntrptConfig->ulErrorH100Timeout * pIntrptManage->ulNumMclkCyclesIn1Ms;


	/*Clear all interrupts that were already enabled*/
	ulResult = Oct6100ApiClearEnabledInterrupts( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Before writing the new configuration to the chip's registers, make sure that any */
	/* interrupts which are either disabled or have no timeout period are not on the */
	/* disabled interrupt list. */

	/*==================================================================================*/
	if ( pIntrptConfig->byFatalGeneralConfig == cOCT6100_INTERRUPT_DISABLE )
		pIntrptManage->byFatalGeneralState = cOCT6100_INTRPT_DISABLED;
	else /* pIntrptConfig->byFatalGeneralConfig == cOCT6100_INTERRUPT_NO_TIMEOUT */
		pIntrptManage->byFatalGeneralState = cOCT6100_INTRPT_ACTIVE;

	/*==================================================================================*/
	if ( pIntrptConfig->byFatalMemoryConfig == cOCT6100_INTERRUPT_DISABLE )
		pIntrptManage->byFatalMemoryState = cOCT6100_INTRPT_DISABLED;
	else if ( pIntrptConfig->byFatalMemoryConfig == cOCT6100_INTERRUPT_NO_TIMEOUT )
		pIntrptManage->byFatalMemoryState = cOCT6100_INTRPT_ACTIVE;
	else /* ( pIntrptConfig->byFatalMemoryConfig == cOCT6100_INTERRUPT_TIMEOUT ) */
	{
		if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_DISABLED )
			pIntrptManage->byFatalMemoryState = cOCT6100_INTRPT_ACTIVE;
	}

	/*==================================================================================*/
	if ( pIntrptConfig->byErrorMemoryConfig == cOCT6100_INTERRUPT_DISABLE )
		pIntrptManage->byErrorMemoryState = cOCT6100_INTRPT_DISABLED;
	else if ( pIntrptConfig->byErrorMemoryConfig == cOCT6100_INTERRUPT_NO_TIMEOUT )
		pIntrptManage->byErrorMemoryState = cOCT6100_INTRPT_ACTIVE;
	else /* (pIntrptConfig->byErrorMemoryConfig == cOCT6100_INTERRUPT_TIMEOUT ) */
	{
		if ( pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_DISABLED )
			pIntrptManage->byErrorMemoryState = cOCT6100_INTRPT_ACTIVE;
	}

	/*==================================================================================*/
	if ( pIntrptConfig->byErrorOverflowToneEventsConfig == cOCT6100_INTERRUPT_DISABLE )
		pIntrptManage->byErrorOverflowToneEventsState = cOCT6100_INTRPT_DISABLED;
	else if ( pIntrptConfig->byErrorOverflowToneEventsConfig == cOCT6100_INTERRUPT_NO_TIMEOUT )
		pIntrptManage->byErrorOverflowToneEventsState = cOCT6100_INTRPT_ACTIVE;
	else /* (pIntrptConfig->byErrorOverflowToneEventsConfig == cOCT6100_INTERRUPT_TIMEOUT ) */
	{
		if ( pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_DISABLED )
			pIntrptManage->byErrorOverflowToneEventsState = cOCT6100_INTRPT_ACTIVE;
	}

	/*==================================================================================*/
	if ( pIntrptConfig->byErrorH100Config == cOCT6100_INTERRUPT_DISABLE )
		pIntrptManage->byErrorH100State = cOCT6100_INTRPT_DISABLED;
	else if ( pIntrptConfig->byErrorH100Config == cOCT6100_INTERRUPT_NO_TIMEOUT )
		pIntrptManage->byErrorH100State = cOCT6100_INTRPT_ACTIVE;
	else /* (pIntrptConfig->byErrorH100Config == cOCT6100_INTERRUPT_TIMEOUT ) */
	{
		if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_DISABLED )
			pIntrptManage->byErrorH100State = cOCT6100_INTRPT_ACTIVE;
	}


	/* Write to the interrupt registers to update the state of each interrupt group. */
	ulResult = Oct6100ApiWriteIeRegs( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiClearEnabledInterrupts

Description:    Disabled interruption are not reported but still available. This 
				function will clear the interrupts that were disabled and wish
				to enable now.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntrptConfig			Pointer to interrupt configuration structure.
f_pIntrptManage			Pointer to interrupt manager structure.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#if !SKIP_Oct6100ApiClearEnabledInterrupts
UINT32 Oct6100ApiClearEnabledInterrupts(
			IN	tPOCT6100_INSTANCE_API				f_pApiInstance ) 
{
	
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_PARAMS		WriteParams;
	tPOCT6100_API_INTRPT_CONFIG	pIntrptConfig;
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	UINT32						ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Copy the configuration to the API instance. */
	pIntrptConfig = &f_pApiInstance->pSharedInfo->IntrptConfig;
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	if ( pIntrptConfig->byFatalGeneralConfig != cOCT6100_INTERRUPT_DISABLE &&
		 pIntrptManage->byFatalGeneralState != cOCT6100_INTRPT_DISABLED )
	{
		WriteParams.ulWriteAddress = 0x102;
		WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_102H;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;	

		WriteParams.ulWriteAddress = 0x202;
		WriteParams.usWriteData = 0x1800;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress = 0x502;
		WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_502H;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	if ( pIntrptConfig->byErrorMemoryConfig != cOCT6100_INTERRUPT_DISABLE &&
		 pIntrptManage->byErrorMemoryState != cOCT6100_INTRPT_DISABLED )
	{
		WriteParams.ulWriteAddress = 0x202;
		WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_202H;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	
	if ( pIntrptConfig->byErrorH100Config != cOCT6100_INTERRUPT_DISABLE &&
		 pIntrptManage->byErrorH100State != cOCT6100_INTRPT_DISABLED )
	{
		WriteParams.ulWriteAddress = 0x302;
		WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_302H;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	if ( pIntrptConfig->byErrorOverflowToneEventsConfig != cOCT6100_INTERRUPT_DISABLE &&
		 pIntrptManage->byErrorOverflowToneEventsState != cOCT6100_INTRPT_DISABLED )
	{
		WriteParams.ulWriteAddress = 0x702;
		WriteParams.usWriteData = cOCT6100_INTRPT_MASK_REG_702H;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}


	return cOCT6100_ERR_OK;

}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100InterruptServiceRoutineSer

Description:    Serialized sub-function of API's interrupt service routine.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntFlags				Pointer to structure containing event flags returned
						to user.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100InterruptServiceRoutineSer
UINT32 Oct6100InterruptServiceRoutineSer(
			IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
			IN		tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_READ_PARAMS	ReadParams;
	tOCT6100_WRITE_PARAMS	WriteParams;
	UINT32	ulRegister210h;
	UINT32	ulResult;
	UINT16	usReadData;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Must update the statistics.  Set parameters in read and write structs. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	/* Set all the flags to default values to make sure the variables are initialized. */
	f_pIntFlags->fFatalGeneral = FALSE;
	f_pIntFlags->ulFatalGeneralFlags = 0x0;
	f_pIntFlags->fFatalReadTimeout = FALSE;
	
	f_pIntFlags->fErrorRefreshTooLate = FALSE;
	f_pIntFlags->fErrorPllJitter = FALSE;

	f_pIntFlags->fErrorH100OutOfSync = FALSE;
	f_pIntFlags->fErrorH100ClkA = FALSE;
	f_pIntFlags->fErrorH100ClkB = FALSE;
	f_pIntFlags->fErrorH100FrameA = FALSE;
	f_pIntFlags->fApiSynch = FALSE;
	
	f_pIntFlags->fErrorOverflowToneEvents = FALSE;

	/* Start by reading registers 210h to determine if any modules have flagged an interrupt. */
	ReadParams.ulReadAddress = 0x210;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	ulRegister210h = usReadData;

	/* Update the extended mclk counter. */
	ulResult = Oct6100ApiReadChipMclkTime( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* If the mclk interrupt is active then check which interrupt timeout periods have expired. */
	ReadParams.ulReadAddress = 0x302;
	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	if ( (usReadData & 0x1) != 0 && pSharedInfo->IntrptManage.fMclkIntrptActive == TRUE )
	{
		/* Update timeout periods. */
		ulResult = Oct6100ApiUpdateIntrptTimeouts( f_pApiInstance );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		f_pIntFlags->fApiSynch = TRUE;

		/* Read registers 210h and 212h again to determine if any modules have flagged an interrupt. */
		ReadParams.ulReadAddress = 0x210;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		ulRegister210h = usReadData;
	}

	/* Read the interrupt registers to determine what interrupt conditions have occured. */
	ulResult = Oct6100ApiReadIntrptRegs( f_pApiInstance, f_pIntFlags, ulRegister210h );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Empty the tone buffer if any events are pending. */
	ulResult = Oct6100ApiTransferToneEvents( f_pApiInstance, FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Set the tone events pending flag. */
	f_pIntFlags->fToneEventsPending = pSharedInfo->IntrptManage.fToneEventsPending;

	/* Check for buffer playout events and insert in the software queue -- if activated. */
	if ( pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize != 0 )
	{
		ulResult = Oct6100BufferPlayoutTransferEvents( f_pApiInstance, FALSE );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Set the buffer playout events pending flag. */
		f_pIntFlags->fBufferPlayoutEventsPending = pSharedInfo->IntrptManage.fBufferPlayoutEventsPending;
	}
	else
	{
		f_pIntFlags->fBufferPlayoutEventsPending = FALSE;
	}

	/* Update the states of each interrupt group. */
	ulResult = Oct6100ApiUpdateIntrptStates( f_pApiInstance, f_pIntFlags );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Check the state of the NLP timestamp if required.*/
	ulResult = Oct6100ApiCheckProcessorState( f_pApiInstance, f_pIntFlags );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write to the necessary IE registers. */
	ulResult = Oct6100ApiWriteIntrptRegs( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Schedule the next mclk interrupt, if one is needed. */
	ulResult = Oct6100ApiScheduleNextMclkIntrptSer( f_pApiInstance );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Free the interrupt pin of the chip (i.e. remove minimum time requirement between interrupts). */
	WriteParams.ulWriteAddress = 0x214;
	WriteParams.usWriteData = 0x0000;
	if ( pSharedInfo->ChipConfig.byInterruptPolarity == cOCT6100_ACTIVE_HIGH_POLARITY )
		WriteParams.usWriteData |= 0x4000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Indicate that the interrupt ROLs have been treated. */
	WriteParams.ulWriteAddress = 0x212;
	WriteParams.usWriteData = 0x8000;
	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReadIntrptRegs

Description:    Reads the interrupt registers of all modules currently
				indicating an interrupt condition.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntFlags				Pointer to an interrupt flag structure.
f_ulRegister210h		Value of register 0x210.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReadIntrptRegs
UINT32 Oct6100ApiReadIntrptRegs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags,
				IN		UINT32							f_ulRegister210h )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHIP_ERROR_STATS	pErrorStats;
	tPOCT6100_API_INTRPT_MANAGE		pIntrptManage;
	tOCT6100_READ_PARAMS			ReadParams;

	UINT32	ulResult;
	UINT16	usReadData;
	UINT32	ulFeatureBytesOffset;
	UINT32	ulFeatureBitOffset;
	UINT32	ulFeatureFieldLength;
	UINT32	ulTempData;
	UINT32	ulCounterValue;
	UINT32	ulMask;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	pErrorStats = &pSharedInfo->ErrorStats;
	pIntrptManage = &pSharedInfo->IntrptManage;

	/* Set some parameters of read struct. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* CPU registers. */
	if ( (f_ulRegister210h & 0x00001) != 0 )
	{
		/*=======================================================================*/
		/* Read registers of this module. */
		ReadParams.ulReadAddress = 0x102;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check which interrupt(s) were set. */
		if ( (usReadData & 0x0001) != 0 )
		{
			f_pIntFlags->fFatalReadTimeout = TRUE;
			pErrorStats->ulInternalReadTimeoutCnt++;
		}

		pIntrptManage->usRegister102h = usReadData;
		/*=======================================================================*/
	}
	else
	{
		pIntrptManage->usRegister102h = 0x0;
	}

	/* MAIN registers. */
	if ( (f_ulRegister210h & 0x00002) != 0 )
	{
		/*=======================================================================*/
		/* Read registers of this module. */
		ReadParams.ulReadAddress = 0x202;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Save current value in instance. */
		pIntrptManage->usRegister202h = usReadData;

		/* Check which interrupts were set. */
		if ( (usReadData & 0x0001) != 0 )
		{
			f_pIntFlags->fErrorRefreshTooLate = TRUE;
			pErrorStats->ulSdramRefreshTooLateCnt++;
		}
		if ( (usReadData & 0x0800) != 0 )
		{
			f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_1;
			f_pIntFlags->fFatalGeneral = TRUE;
			pErrorStats->fFatalChipError = TRUE;
		}
		if ( (usReadData & 0x1000) != 0 )
		{
			f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_2;
			f_pIntFlags->fFatalGeneral = TRUE;
			pErrorStats->fFatalChipError = TRUE;
		}
		if ( (usReadData & 0x0400) != 0 )
		{
			f_pIntFlags->fErrorPllJitter = TRUE;
			pErrorStats->ulPllJitterErrorCnt++;

			/* Update the PLL jitter error count here. */
			if ( pSharedInfo->DebugInfo.fPouchCounter == TRUE )
			{
				ulFeatureBytesOffset = pSharedInfo->MemoryMap.PouchCounterFieldOfst.usDwordOffset * 4;
				ulFeatureBitOffset	 = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byBitOffset;
				ulFeatureFieldLength = pSharedInfo->MemoryMap.PouchCounterFieldOfst.byFieldSize;

				ulResult = Oct6100ApiReadDword(	f_pApiInstance,
												cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
												&ulTempData );
				
				/* Read previous value set in the feature field. */
				mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

				/* Update counter. */
				ulCounterValue = ulTempData & ulMask;
				ulCounterValue = ulCounterValue >> ulFeatureBitOffset;
				ulCounterValue ++;
				/* Handle wrap around case. */
				ulCounterValue &= ( 1 << ulFeatureFieldLength ) - 1;

				/* Clear old counter value. */
				ulTempData &= (~ulMask);
				ulTempData |= ulCounterValue << ulFeatureBitOffset;

				/* Write the DWORD where the field is located.*/
				ulResult = Oct6100ApiWriteDword( f_pApiInstance,
												 cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
												 ulTempData );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;	
			}
		}

		/*=======================================================================*/
	}
	else
	{
		pIntrptManage->usRegister202h = 0x0;
	}

	/* H.100 registers. */
	if ( (f_ulRegister210h & 0x00004) != 0 )
	{
		/*=======================================================================*/
		/* Read registers of this module. */
		ReadParams.ulReadAddress = 0x302;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check which interrupts were set. */
		if ( (usReadData & 0x0100) != 0 )
		{
			f_pIntFlags->fErrorH100OutOfSync = TRUE;
			pErrorStats->ulH100OutOfSyncCnt++;
		}
		if ( (usReadData & 0x1000) != 0 )
		{
			f_pIntFlags->fErrorH100FrameA = TRUE;
			pErrorStats->ulH100FrameABadCnt++;
		}
		if ( (usReadData & 0x4000) != 0 )
		{
			f_pIntFlags->fErrorH100ClkA = TRUE;
			pErrorStats->ulH100ClkABadCnt++;
		}
		if ( (usReadData & 0x8000) != 0 )
		{
			if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			{
				f_pIntFlags->fErrorH100ClkB = TRUE;
				pErrorStats->ulH100ClkBBadCnt++;
			}
		}

		pIntrptManage->usRegister302h = usReadData;
		/*=======================================================================*/
	}
	else
	{
		pIntrptManage->usRegister302h = 0x0;
	}

	/* TDMIE registers. */
	if ( (f_ulRegister210h & 0x00010) != 0 )
	{
		/*=======================================================================*/
		/* Read register. */
		ReadParams.ulReadAddress = 0x502;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check which interrupts were set. */
		if ( (usReadData & 0x0002) != 0 )
		{
			f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_3;
			f_pIntFlags->fFatalGeneral = TRUE;
			pErrorStats->fFatalChipError = TRUE;
		}

		pIntrptManage->usRegister502h = usReadData;
		/*=======================================================================*/
	}
	else
	{
		pIntrptManage->usRegister502h = 0x0;
	}

	/* PGSP registers. */
	if ( (f_ulRegister210h & 0x00080) != 0 )
	{
		/*=======================================================================*/
		/* Read register. */
		ReadParams.ulReadAddress = 0x702;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check which interrupts were set. */
		if ( (usReadData & 0x0002) != 0 )
		{
			f_pIntFlags->fErrorOverflowToneEvents = TRUE;
			pErrorStats->ulOverflowToneEventsCnt++;
		}
	
		pIntrptManage->usRegister702h = usReadData;
		/*=======================================================================*/
	}
	else
	{
		pIntrptManage->usRegister702h = 0x0;
	}
	


	/* If this is the first time the ISR is called, clear the ISR is not called bit */
	/* in external memory to signal the remote client that we are called. */
	if ( pSharedInfo->IntrptManage.fIsrCalled == FALSE )
	{
		/* Remember that we are being called. */
		pSharedInfo->IntrptManage.fIsrCalled = TRUE;

		if ( pSharedInfo->DebugInfo.fIsIsrCalledField == TRUE )
		{
			ulFeatureBytesOffset = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.usDwordOffset * 4;
			ulFeatureBitOffset	 = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.byBitOffset;
			ulFeatureFieldLength = pSharedInfo->MemoryMap.IsIsrCalledFieldOfst.byFieldSize;

			ulResult = Oct6100ApiReadDword(	f_pApiInstance,
											cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
											&ulTempData );
			
			/* Read previous value set in the feature field. */
			mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

			/* Clear the field. */
			ulTempData &= (~ulMask);

			/* Write the DWORD where the field is located.*/
			ulResult = Oct6100ApiWriteDword( f_pApiInstance,
											 cOCT6100_POUCH_BASE + ulFeatureBytesOffset,
											 ulTempData );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;	
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateIntrptStates

Description:    Updates the state of all interrupt register groups.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntFlags				Interrupt flags.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateIntrptStates
UINT32 Oct6100ApiUpdateIntrptStates(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags )
{
	tPOCT6100_API_INTRPT_CONFIG	pIntrptConfig;
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;

	pIntrptConfig = &f_pApiInstance->pSharedInfo->IntrptConfig;
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	/*-----------------------------------------------------------------------*/
	if ( ( f_pIntFlags->fFatalReadTimeout == TRUE) &&
		 pIntrptConfig->byFatalMemoryConfig == cOCT6100_INTERRUPT_TIMEOUT &&
		 pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
	{
		pIntrptManage->byFatalMemoryState = cOCT6100_INTRPT_WILL_TIMEOUT;
		pIntrptManage->ulFatalMemoryDisableMclkHigh = pIntrptManage->ulRegMclkTimeHigh;
		pIntrptManage->ulFatalMemoryDisableMclkLow = pIntrptManage->ulRegMclkTimeLow;
	}
	/*-----------------------------------------------------------------------*/
	if ( (f_pIntFlags->fErrorRefreshTooLate == TRUE || 
		  f_pIntFlags->fErrorPllJitter == TRUE ) &&
		 pIntrptConfig->byErrorMemoryConfig == cOCT6100_INTERRUPT_TIMEOUT &&
		 pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_ACTIVE )
	{
		pIntrptManage->byErrorMemoryState = cOCT6100_INTRPT_WILL_TIMEOUT;
		pIntrptManage->ulErrorMemoryDisableMclkHigh = pIntrptManage->ulRegMclkTimeHigh;
		pIntrptManage->ulErrorMemoryDisableMclkLow = pIntrptManage->ulRegMclkTimeLow;
	}
	/*-----------------------------------------------------------------------*/
	if ( (f_pIntFlags->fErrorOverflowToneEvents == TRUE) &&
		 pIntrptConfig->byErrorOverflowToneEventsConfig == cOCT6100_INTERRUPT_TIMEOUT &&
		 pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_ACTIVE )
	{
		pIntrptManage->byErrorOverflowToneEventsState = cOCT6100_INTRPT_WILL_TIMEOUT;
		pIntrptManage->ulErrorOverflowToneEventsDisableMclkHigh = pIntrptManage->ulRegMclkTimeHigh;
		pIntrptManage->ulErrorOverflowToneEventsDisableMclkLow = pIntrptManage->ulRegMclkTimeLow;
	}
	/*-----------------------------------------------------------------------*/
	if ( (f_pIntFlags->fErrorH100OutOfSync == TRUE ||
		  f_pIntFlags->fErrorH100ClkA == TRUE ||
		  f_pIntFlags->fErrorH100ClkB == TRUE ||
		  f_pIntFlags->fErrorH100FrameA == TRUE ) &&
		 pIntrptConfig->byErrorH100Config == cOCT6100_INTERRUPT_TIMEOUT &&
		 pIntrptManage->byErrorH100State == cOCT6100_INTRPT_ACTIVE )
	{
		pIntrptManage->byErrorH100State = cOCT6100_INTRPT_WILL_TIMEOUT;
		pIntrptManage->ulErrorH100DisableMclkHigh = pIntrptManage->ulRegMclkTimeHigh;
		pIntrptManage->ulErrorH100DisableMclkLow = pIntrptManage->ulRegMclkTimeLow;
	}
	/*-----------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteIntrptRegs

Description:    Writes to interrupt registers to clear interrupt condition, and
				writes to an interrupt's IE register if interrupt is to time
				out.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep
					the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteIntrptRegs
UINT32 Oct6100ApiWriteIntrptRegs(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	tOCT6100_WRITE_PARAMS		WriteParams;

	UINT32	ulResult;

	/* Get some local pointers. */
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	/* Set some parameters of write struct. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;



	/*===========================================================================*/
	if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_WILL_TIMEOUT )
	{
		WriteParams.ulWriteAddress = 0x104;
		WriteParams.usWriteData = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	if ( (pIntrptManage->usRegister102h & cOCT6100_INTRPT_MASK_REG_102H) != 0 )
	{
		WriteParams.ulWriteAddress = 0x102;
		WriteParams.usWriteData = pIntrptManage->usRegister102h;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	/*===========================================================================*/

	/*===========================================================================*/
	if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_WILL_TIMEOUT ||
		 pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_WILL_TIMEOUT )
	{
		WriteParams.ulWriteAddress = 0x204;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x1800;

		if ( pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x0401;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	if ( (pIntrptManage->usRegister202h & cOCT6100_INTRPT_MASK_REG_202H) != 0 )
	{
		WriteParams.ulWriteAddress = 0x202;
		WriteParams.usWriteData = pIntrptManage->usRegister202h;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	/*===========================================================================*/

	/*===========================================================================*/
	if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_WILL_TIMEOUT )
	{
		WriteParams.ulWriteAddress = 0x304;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->fMclkIntrptActive == TRUE )
			WriteParams.usWriteData |= 0x0001;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	if ( (pIntrptManage->usRegister302h & cOCT6100_INTRPT_MASK_REG_302H) != 0 )
	{
		WriteParams.ulWriteAddress = 0x302;
		WriteParams.usWriteData = pIntrptManage->usRegister302h;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	/*===========================================================================*/

	/*===========================================================================*/
	if ( (pIntrptManage->usRegister502h & cOCT6100_INTRPT_MASK_REG_502H) != 0 )
	{
		WriteParams.ulWriteAddress = 0x502;
		WriteParams.usWriteData = pIntrptManage->usRegister502h;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	/*===========================================================================*/

	/*===========================================================================*/
	if ( pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_WILL_TIMEOUT )
	{
		WriteParams.ulWriteAddress = 0x704;
		WriteParams.usWriteData = 0x0000;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	if ( (pIntrptManage->usRegister702h & cOCT6100_INTRPT_MASK_REG_702H) != 0 )
	{
		WriteParams.ulWriteAddress = 0x702;
		WriteParams.usWriteData = pIntrptManage->usRegister702h;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	/*===========================================================================*/



	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteIeRegs

Description:    Writes the IE field of each interrupt register.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteIeRegs
UINT32 Oct6100ApiWriteIeRegs(
				tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32	ulResult;

	/* Get some local pointers. */
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	/* Set some parameters of write struct. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	/*==================================================================================*/
	WriteParams.ulWriteAddress = 0x104;
	WriteParams.usWriteData = 0x0000;

	if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
		WriteParams.usWriteData |= 0x0001;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/

	/*==================================================================================*/
	WriteParams.ulWriteAddress = 0x204;
	WriteParams.usWriteData = 0x0000;

	if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
		WriteParams.usWriteData |= 0x1800;
	if ( pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_ACTIVE )
		WriteParams.usWriteData |= 0x0401;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/

	/*==================================================================================*/
	WriteParams.ulWriteAddress = 0x304;
	WriteParams.usWriteData = 0x0000;

	if ( pIntrptManage->fMclkIntrptActive == TRUE )
		WriteParams.usWriteData |= 0x0001;
	if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_ACTIVE )
	{
		if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
			WriteParams.usWriteData |= 0xD100;
		else
			WriteParams.usWriteData |= 0x5100;
	}

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/

	/*==================================================================================*/
	WriteParams.ulWriteAddress = 0x504;
	WriteParams.usWriteData = 0x0000;

	if ( pIntrptManage->byFatalGeneralState == cOCT6100_INTRPT_ACTIVE )
		WriteParams.usWriteData |= 0x0002;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/

	/*==================================================================================*/
	WriteParams.ulWriteAddress = 0x704;
	WriteParams.usWriteData = 0x0000;

	if ( pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_ACTIVE )
		WriteParams.usWriteData |= 0x0002;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/


	/*==================================================================================*/
	/* Enable the GLOBAL IEs for the interrupt pin. */
	WriteParams.ulWriteAddress = 0x218;
	WriteParams.usWriteData = 0x00D7;

	mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	/*==================================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReadChipMclkTime

Description:    Reads the chip's mclk cycle count to construct a chip time.
				The time is used to manage interrupts.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReadChipMclkTime
UINT32 Oct6100ApiReadChipMclkTime(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32	ulCheckData;
	UINT32	ulResult;
	UINT32	i;
	UINT16	usReadData;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	pIntrptManage = &pSharedInfo->IntrptManage;

	/* Assign memory for read data. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Perform reads. */
	for ( i = 0; i < 100; i++ )
	{
		ReadParams.ulReadAddress = 0x306;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		pIntrptManage->ulRegMclkTimeHigh = usReadData & 0xFF;
		ulCheckData = usReadData;

		ReadParams.ulReadAddress = 0x308;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		pIntrptManage->ulRegMclkTimeLow = (usReadData & 0xFFFF) << 16;

		ReadParams.ulReadAddress = 0x306;
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		if ( ulCheckData == usReadData )
			break;
	}

	if ( i == 100 )
		return cOCT6100_ERR_FATAL_2F;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateIntrptTimeouts

Description:    Checks which interrupt groups have finished their timeout
				period.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateIntrptTimeouts
UINT32 Oct6100ApiUpdateIntrptTimeouts(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_API_INTRPT_MANAGE pIntrptManage;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32	ulRegMclkTimePlus5MsHigh;
	UINT32	ulRegMclkTimePlus5MsLow;
	UINT32	ulResult;
	BOOL	fFatalMemoryChange = FALSE;
	BOOL	fDataErrMemoryChange = FALSE;
	BOOL	fErrorOverflowToneEventsChange = FALSE;
	BOOL	fH100ErrorChange = FALSE;

	/* Get local pointer to interrupt management structure. */
	pIntrptManage = &f_pApiInstance->pSharedInfo->IntrptManage;

	/* Calculate mclk time + 5 ms. */
	ulRegMclkTimePlus5MsLow = pIntrptManage->ulRegMclkTimeLow + (5 * pIntrptManage->ulNumMclkCyclesIn1Ms);
	if ( ulRegMclkTimePlus5MsLow < pIntrptManage->ulRegMclkTimeLow )
		ulRegMclkTimePlus5MsHigh = pIntrptManage->ulRegMclkTimeHigh + 1;
	else /* ( ulRegMclkTimePlus5MsLow >= pIntrptManage->ulRegMclkTimeLow ) */
		ulRegMclkTimePlus5MsHigh = pIntrptManage->ulRegMclkTimeHigh;

	/* Check which interrupts are timed out and need to be reenabled now. */
	if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_IN_TIMEOUT )
	{
		mOCT6100_CHECK_INTRPT_TIMEOUT( ulRegMclkTimePlus5MsHigh, ulRegMclkTimePlus5MsLow, pIntrptManage->ulFatalMemoryDisableMclkHigh, pIntrptManage->ulFatalMemoryDisableMclkLow, pIntrptManage->ulFatalMemoryEnableMclkHigh, pIntrptManage->ulFatalMemoryEnableMclkLow, pIntrptManage->byFatalMemoryState, fFatalMemoryChange )
	}
	if ( pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_IN_TIMEOUT )
	{
		mOCT6100_CHECK_INTRPT_TIMEOUT( ulRegMclkTimePlus5MsHigh, ulRegMclkTimePlus5MsLow, pIntrptManage->ulErrorMemoryDisableMclkHigh, pIntrptManage->ulErrorMemoryDisableMclkLow, pIntrptManage->ulErrorMemoryEnableMclkHigh, pIntrptManage->ulErrorMemoryEnableMclkLow, pIntrptManage->byErrorMemoryState, fDataErrMemoryChange )
	}
	if ( pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_IN_TIMEOUT )
	{
		mOCT6100_CHECK_INTRPT_TIMEOUT( ulRegMclkTimePlus5MsHigh, ulRegMclkTimePlus5MsLow, pIntrptManage->ulErrorOverflowToneEventsDisableMclkHigh, pIntrptManage->ulErrorOverflowToneEventsDisableMclkLow, pIntrptManage->ulErrorOverflowToneEventsEnableMclkHigh, pIntrptManage->ulErrorOverflowToneEventsEnableMclkLow, pIntrptManage->byErrorOverflowToneEventsState, fErrorOverflowToneEventsChange )
	}
	if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_IN_TIMEOUT )
	{
		mOCT6100_CHECK_INTRPT_TIMEOUT( ulRegMclkTimePlus5MsHigh, ulRegMclkTimePlus5MsLow, pIntrptManage->ulErrorH100DisableMclkHigh, pIntrptManage->ulErrorH100DisableMclkLow, pIntrptManage->ulErrorH100EnableMclkHigh, pIntrptManage->ulErrorH100EnableMclkLow, pIntrptManage->byErrorH100State, fH100ErrorChange )
	}

	/* Set some parameters of write struct. */
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;
	
	/* Write to the IE registers which have changed. */

	/*==================================================================================*/
	if ( fFatalMemoryChange == TRUE )
	{
		WriteParams.ulWriteAddress = 0x104;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x0001;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

	}
	/*==================================================================================*/

	/*==================================================================================*/
	if ( fFatalMemoryChange == TRUE || 
		 fDataErrMemoryChange == TRUE )
	{
		WriteParams.ulWriteAddress = 0x204;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->byFatalMemoryState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x1800;
		if ( pIntrptManage->byErrorMemoryState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x0401;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

	}
	/*==================================================================================*/

	/*==================================================================================*/
	if ( pIntrptManage->fMclkIntrptActive == TRUE ||
		 fH100ErrorChange == TRUE )
	{
		WriteParams.ulWriteAddress = 0x304;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->fMclkIntrptActive == TRUE )
			WriteParams.usWriteData |= 0x0001;
		
		if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_ACTIVE )
		{
			if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
				WriteParams.usWriteData |= 0xD100;
			else
				WriteParams.usWriteData |= 0x5100;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	
	}
	/*==================================================================================*/


	/*==================================================================================*/
	if ( fErrorOverflowToneEventsChange == TRUE )
	{
		WriteParams.ulWriteAddress = 0x704;
		WriteParams.usWriteData = 0x0000;

		if ( pIntrptManage->byErrorOverflowToneEventsState == cOCT6100_INTRPT_ACTIVE )
			WriteParams.usWriteData |= 0x0002;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	
	}
	/*==================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiScheduleNextMclkIntrptSer

Description:    Serialized sub-function of Oct6100ApiScheduleNextMclkIntrpt.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiScheduleNextMclkIntrptSer
UINT32 Oct6100ApiScheduleNextMclkIntrptSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_INTRPT_CONFIG	pIntrptConfig;
	tPOCT6100_API_INTRPT_MANAGE	pIntrptManage;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32	ulTimeDiff;
	UINT32	ulRegMclkTimeHigh;
	UINT32	ulRegMclkTimeLow;
	UINT32	ulResult;
	BOOL	fConditionFlag = TRUE;
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Obtain temporary pointers to reduce indirection, thus speeding up processing. */
	pIntrptConfig = &pSharedInfo->IntrptConfig;
	pIntrptManage = &pSharedInfo->IntrptManage;
	ulRegMclkTimeHigh = pIntrptManage->ulRegMclkTimeHigh;
	ulRegMclkTimeLow = pIntrptManage->ulRegMclkTimeLow;

	/* First, check if any interrupts have just been disabled.  If there are any, */
	/* determine the time at which they should be reenabled. */
	pIntrptManage->ulNextMclkIntrptTimeHigh = cOCT6100_INVALID_VALUE;
	pIntrptManage->ulNextMclkIntrptTimeLow = cOCT6100_INVALID_VALUE;

	while ( fConditionFlag )
	{
		/* Indicate that no mclk interrupt is needed, yet. */
		ulTimeDiff = cOCT6100_INVALID_VALUE;

		/* Check each interrupt category to see if an mclk interrupt is needed to */
		/* reenable an interrupt at a later time. */
		if ( pIntrptManage->byFatalMemoryState != cOCT6100_INTRPT_ACTIVE &&
			 pIntrptManage->byFatalMemoryState != cOCT6100_INTRPT_DISABLED )
		{
			mOCT6100_GET_INTRPT_ENABLE_TIME( ulRegMclkTimeHigh, ulRegMclkTimeLow, pIntrptManage->byFatalMemoryState, pIntrptManage->ulFatalMemoryEnableMclkHigh, pIntrptManage->ulFatalMemoryEnableMclkLow, pIntrptConfig->ulFatalMemoryTimeoutMclk, ulTimeDiff )
		}
		if ( pIntrptManage->byErrorMemoryState != cOCT6100_INTRPT_ACTIVE &&
			 pIntrptManage->byErrorMemoryState != cOCT6100_INTRPT_DISABLED )
		{
			mOCT6100_GET_INTRPT_ENABLE_TIME( ulRegMclkTimeHigh, ulRegMclkTimeLow, pIntrptManage->byErrorMemoryState, pIntrptManage->ulErrorMemoryEnableMclkHigh, pIntrptManage->ulErrorMemoryEnableMclkLow, pIntrptConfig->ulErrorMemoryTimeoutMclk, ulTimeDiff )
		}
		if ( pIntrptManage->byErrorOverflowToneEventsState != cOCT6100_INTRPT_ACTIVE &&
			 pIntrptManage->byErrorOverflowToneEventsState != cOCT6100_INTRPT_DISABLED )
		{	 
			mOCT6100_GET_INTRPT_ENABLE_TIME( ulRegMclkTimeHigh, ulRegMclkTimeLow, pIntrptManage->byErrorOverflowToneEventsState, pIntrptManage->ulErrorOverflowToneEventsEnableMclkHigh, pIntrptManage->ulErrorOverflowToneEventsEnableMclkLow, pIntrptConfig->ulErrorOverflowToneEventsTimeoutMclk, ulTimeDiff )
		}
		if ( pIntrptManage->byErrorH100State != cOCT6100_INTRPT_ACTIVE &&
			 pIntrptManage->byErrorH100State != cOCT6100_INTRPT_DISABLED )
		{
			mOCT6100_GET_INTRPT_ENABLE_TIME( ulRegMclkTimeHigh, ulRegMclkTimeLow, pIntrptManage->byErrorH100State, pIntrptManage->ulErrorH100EnableMclkHigh, pIntrptManage->ulErrorH100EnableMclkLow, pIntrptConfig->ulErrorH100TimeoutMclk, ulTimeDiff )
		}

		/* Set some parameters of write struct. */
		WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

		WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

		/* Schedule next mclk interrupt, if any is needed. */
		if ( ulTimeDiff != cOCT6100_INVALID_VALUE )
		{
			UINT32	ulMclkTimeTest;
			UINT32	ulAlarmTimeTest;
			UINT32	ulTimeDiffTest;
			BOOL	fAlarmTimePassed;

			/* Indicate that an mclk interrupt is scheduled.*/
			pIntrptManage->fMclkIntrptActive = TRUE;

			pIntrptManage->ulNextMclkIntrptTimeLow = ulRegMclkTimeLow + ulTimeDiff;
			if ( pIntrptManage->ulNextMclkIntrptTimeLow < ulRegMclkTimeLow )
				pIntrptManage->ulNextMclkIntrptTimeHigh = ulRegMclkTimeHigh + 1;
			else /* ( pIntrptManage->ulNextMclkIntrptTimeLow >= ulRegMclkTimeLow ) */
				pIntrptManage->ulNextMclkIntrptTimeHigh = ulRegMclkTimeHigh;
			
			WriteParams.ulWriteAddress = 0x30C;
			WriteParams.usWriteData  = (UINT16)( (pIntrptManage->ulNextMclkIntrptTimeLow >> 24) & 0xFF );
			WriteParams.usWriteData |= (UINT16)( (pIntrptManage->ulNextMclkIntrptTimeHigh & 0xFF) << 8 );
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			WriteParams.ulWriteAddress = 0x30E;
			WriteParams.usWriteData = (UINT16)( (pIntrptManage->ulNextMclkIntrptTimeLow >> 8) & 0xFFFF );
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			WriteParams.ulWriteAddress = 0x304;
			WriteParams.usWriteData = 0;
			
			if ( pIntrptManage->fMclkIntrptActive == TRUE )
				WriteParams.usWriteData = 0x0001;

			if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_ACTIVE )
			{
				if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
					WriteParams.usWriteData |= 0xD100;
				else
					WriteParams.usWriteData |= 0x5100;
			}
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Disable the ROL if previously set. */
			WriteParams.ulWriteAddress = 0x302;
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Check if already passed the next interrupt time. */
			ulResult = Oct6100ApiReadChipMclkTime( f_pApiInstance );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			ulMclkTimeTest	= (pIntrptManage->ulRegMclkTimeLow >> 16) & 0xFFFF;
			ulAlarmTimeTest = (pIntrptManage->ulNextMclkIntrptTimeLow >> 16) & 0xFFFF;

			/* Update the local Mlck timer values.*/
			ulRegMclkTimeHigh	= pIntrptManage->ulRegMclkTimeHigh;
			ulRegMclkTimeLow	= pIntrptManage->ulRegMclkTimeLow;

			fAlarmTimePassed = FALSE;

			if ( ulMclkTimeTest > ulAlarmTimeTest )
			{
				ulTimeDiffTest = ulMclkTimeTest - ulAlarmTimeTest;
				if ( ulTimeDiffTest <= 0x8000 )
					fAlarmTimePassed = TRUE;
			}
			else /* ( ulMclkTimeTest <= ulAlarmTimeTest ) */
			{
				ulTimeDiffTest = ulAlarmTimeTest - ulMclkTimeTest;
				if ( ulTimeDiffTest > 0x8000 )
					fAlarmTimePassed = TRUE;
			}
			
			if ( fAlarmTimePassed == TRUE )
			{
				/* Passed the interrupt time.  Schedule next interrupt (if needed). */
				ulResult = Oct6100ApiUpdateIntrptTimeouts( f_pApiInstance );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				continue;
			}
			else
			{
				fConditionFlag = FALSE;
			}
		}
		else
		{
			/* Indicate that no mclk interrupt is scheduled. */
			pIntrptManage->fMclkIntrptActive = FALSE;

			/* Insure that the mclk interrupt is not enabled. */
			WriteParams.ulWriteAddress = 0x304;
			WriteParams.usWriteData = 0x0000;
			if ( pIntrptManage->byErrorH100State == cOCT6100_INTRPT_ACTIVE )
			{
				if ( f_pApiInstance->pSharedInfo->ChipConfig.fEnableFastH100Mode == TRUE )
					WriteParams.usWriteData |= 0xD100;
				else
					WriteParams.usWriteData |= 0x5100;
			}
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			fConditionFlag = FALSE;
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckProcessorState

Description:    This function verifies if the NLP and AF processors are operating
				correctly.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pIntFlags				Pointer to a tOCT6100_INTERRUPT_FLAGS structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckProcessorState
UINT32	Oct6100ApiCheckProcessorState(
	  			IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_READ_PARAMS			ReadParams;
	tOCT6100_READ_BURST_PARAMS		ReadBurstParams;

	UINT32		ulNlpTimestamp;
	UINT32		ulAfTimestamp;
	UINT32		ulTimestampDiff;

	UINT32		ulResult;
	UINT32		i;

	UINT16		usReadData;
	UINT16		ausReadData[ 2 ];

	UINT32		aulWaitTime[ 2 ];
	
	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set some parameters of write struct. */
	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	/* Set some parameters of write struct. */
	ReadBurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadBurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadBurstParams.pusReadData = ausReadData;

	/*-----------------------------------------------------------------------*/
	/* Check if chip is in reset. */

	/* Read the main control register. */
	ReadParams.ulReadAddress = 0x100;

	mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	if ( usReadData == 0x0000 )
	{
		/* Chip was resetted. */
		f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_4;
		f_pIntFlags->fFatalGeneral = TRUE;
		pSharedInfo->ErrorStats.fFatalChipError = TRUE;
	}

	/*-----------------------------------------------------------------------*/


	/*-----------------------------------------------------------------------*/
	/* Reading the AF timestamp.*/

	for ( i = 0; i < cOCT6100_MAX_LOOP; i++ )
	{
		/* Read the timestamp.*/
		ReadBurstParams.ulReadAddress	= 0x082E0008;
		ReadBurstParams.ulReadLength	= 2;

		mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Read the high part again to make sure it didn't wrap. */
		ReadParams.ulReadAddress		= 0x082E0008;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if the low part wrapped. */
		if ( ausReadData[ 0 ] == usReadData )
			break;
	}

	if ( i == cOCT6100_MAX_LOOP )
		return cOCT6100_ERR_INTRPTS_AF_TIMESTAMP_READ_TIMEOUT;

	/* Save the AF timestamp. */
	ulAfTimestamp = (ausReadData[ 0 ] << 16) | ausReadData[ 1 ];

	/*-----------------------------------------------------------------------*/

	
	/*-----------------------------------------------------------------------*/
	/* Reading the NLP timestamp. */

	for ( i = 0; i < cOCT6100_MAX_LOOP; i++ )
	{
		/* Read the timestamp. */
		ReadBurstParams.ulReadAddress	= 0x08000008;
		ReadBurstParams.ulReadLength	= 2;

		mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Read the high part again to make sure it didn't wrap. */
		ReadParams.ulReadAddress		= 0x08000008;

		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if the low part wrapped. */
		if ( ausReadData[ 0 ] == usReadData )
			break;
	}

	if ( i == cOCT6100_MAX_LOOP )
		return cOCT6100_ERR_INTRPTS_NLP_TIMESTAMP_READ_TIMEOUT;

	/* Save the NLP timestamp. */
	ulNlpTimestamp = (ausReadData[ 0 ] << 16) | ausReadData[ 1 ];

	/*-----------------------------------------------------------------------*/


	/*-----------------------------------------------------------------------*/
	/* Check the validity of the timestamp. */
	
	if ( ulAfTimestamp > ulNlpTimestamp )
	{
		/* The NLP timestamp wrapped. */
		ulTimestampDiff  = 0xFFFFFFFF - ulAfTimestamp + 1;
		ulTimestampDiff += ulNlpTimestamp;
	}
	else
		ulTimestampDiff = ulNlpTimestamp - ulAfTimestamp;

	if ( ulTimestampDiff > 0x2000 )
	{
		f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_5;
		f_pIntFlags->fFatalGeneral = TRUE;
		pSharedInfo->ErrorStats.fFatalChipError = TRUE;
	}

	/*Check if AF and NLP are both stuck*/
	if ( f_pIntFlags->fErrorH100ClkA == FALSE &&
		 f_pIntFlags->fErrorH100ClkB == FALSE &&
		 f_pIntFlags->fErrorH100FrameA == FALSE &&
		 f_pIntFlags->fErrorH100OutOfSync == FALSE )
		
	{
		if ( ulAfTimestamp == 0 && ulNlpTimestamp == 0 )
		{
			/*Give some time to the counters*/
			aulWaitTime[ 0 ] = 250;
			aulWaitTime[ 1 ] = 0;
			ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*Let's read again the AF timestamp to be sure. Maybe they were at 0 at the same time*/
			ReadBurstParams.ulReadAddress	= 0x082E0008;
			ReadBurstParams.ulReadLength	= 2;

			mOCT6100_DRIVER_READ_BURST_API( ReadBurstParams, ulResult )
			if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

			ulAfTimestamp = (ausReadData[ 0 ] << 16) | ausReadData[ 1 ];
	
			if ( ulAfTimestamp == 0 )
			{
				/*TDM Clocks are ok but NLP and AF timestamps are both at 0*/
				f_pIntFlags->ulFatalGeneralFlags |= cOCT6100_FATAL_GENERAL_ERROR_TYPE_9;
				f_pIntFlags->fFatalGeneral = TRUE;
				pSharedInfo->ErrorStats.fFatalChipError = TRUE;
			}
		}
			
	}

	/*-----------------------------------------------------------------------*/

	return cOCT6100_ERR_OK;
}
#endif
