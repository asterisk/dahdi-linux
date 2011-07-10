/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_playout_buf.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains functions used to manage buffer playout.

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

$Octasic_Revision: 109 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"
#include "oct6100api/oct6100_apiud.h"

#include "apilib/octapi_llman.h"

#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_api_inst.h"
#include "oct6100api/oct6100_channel_inst.h"
#include "oct6100api/oct6100_playout_buf_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_events_pub.h"
#include "oct6100api/oct6100_playout_buf_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_events_priv.h"
#include "oct6100_playout_buf_priv.h"

/****************************  PUBLIC FUNCTIONS  *****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutLoad

Description:    This function loads a playout buffer into external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferLoad			Pointer to buffer playout load structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutLoadDef
UINT32 Oct6100BufferPlayoutLoadDef(
				tPOCT6100_BUFFER_LOAD			f_pBufferLoad )
{
	f_pBufferLoad->pbyBufferPattern = NULL;
	f_pBufferLoad->ulBufferSize = 128;
	f_pBufferLoad->ulBufferPcmLaw = cOCT6100_PCM_U_LAW;
	
	f_pBufferLoad->pulBufferIndex = NULL;
	f_pBufferLoad->pulPlayoutFreeMemSize = NULL;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutLoad
UINT32 Oct6100BufferPlayoutLoad(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_BUFFER_LOAD				f_pBufferLoad )
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
		ulFncRes = Oct6100BufferLoadSer( f_pApiInstance, f_pBufferLoad, TRUE, cOCT6100_INVALID_INDEX );
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

Function:		Oct6100BufferPlayoutLoadBlockInit

Description:    This function allows the user to initialize loading a buffer 
				into external memory using blocks.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_pBufferLoadBlockInit	Pointer to buffer playout load block init structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutLoadBlockInitDef
UINT32 Oct6100BufferPlayoutLoadBlockInitDef(
				tPOCT6100_BUFFER_LOAD_BLOCK_INIT	f_pBufferLoadBlockInit )
{
	f_pBufferLoadBlockInit->ulBufferSize = 128;
	f_pBufferLoadBlockInit->ulBufferPcmLaw = cOCT6100_PCM_U_LAW;
	
	f_pBufferLoadBlockInit->pulBufferIndex = NULL;
	f_pBufferLoadBlockInit->pulPlayoutFreeMemSize = NULL;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutLoadBlockInit
UINT32 Oct6100BufferPlayoutLoadBlockInit(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_BUFFER_LOAD_BLOCK_INIT	f_pBufferLoadBlockInit )
{
	tOCT6100_SEIZE_SERIALIZE_OBJECT		SeizeSerObj;
	tOCT6100_RELEASE_SERIALIZE_OBJECT	ReleaseSerObj;
	UINT32								ulSerRes = cOCT6100_ERR_OK;
	UINT32								ulFncRes = cOCT6100_ERR_OK;

	/* Set the process context of the serialize structure.*/
	SeizeSerObj.pProcessContext = f_pApiInstance->pProcessContext;
	ReleaseSerObj.pProcessContext = f_pApiInstance->pProcessContext;

	/* Seize all list semaphores needed by this function. */
	SeizeSerObj.ulSerialObjHndl = f_pApiInstance->ulApiSerObj;
	SeizeSerObj.ulTryTimeMs = cOCT6100_WAIT_INFINITELY;
	ulSerRes = Oct6100UserSeizeSerializeObject( &SeizeSerObj );
	if ( ulSerRes == cOCT6100_ERR_OK )
	{
		/* Call the serialized function. */
		ulFncRes = Oct6100BufferLoadBlockInitSer( f_pApiInstance, f_pBufferLoadBlockInit );
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

Function:		Oct6100BufferPlayoutLoadBlock

Description:	This function allows the user to load a buffer block into 
				external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_pBufferLoadBlock		Pointer to buffer playout load block structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutLoadBlockDef
UINT32 Oct6100BufferPlayoutLoadBlockDef(
				tPOCT6100_BUFFER_LOAD_BLOCK		f_pBufferLoadBlock )
{
	f_pBufferLoadBlock->ulBufferIndex = cOCT6100_INVALID_VALUE;
	f_pBufferLoadBlock->ulBlockLength = cOCT6100_INVALID_VALUE;
	f_pBufferLoadBlock->ulBlockOffset = cOCT6100_INVALID_VALUE;
	
	f_pBufferLoadBlock->pbyBufferPattern = NULL;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutLoadBlock
UINT32 Oct6100BufferPlayoutLoadBlock(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_BUFFER_LOAD_BLOCK			f_pBufferLoadBlock )
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
		ulFncRes = Oct6100BufferLoadBlockSer( f_pApiInstance, f_pBufferLoadBlock );
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

Function:		Oct6100BufferPlayoutUnload

Description:    This function unloads a playout buffer from external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferUnload			Pointer to buffer playout unload structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutUnloadDef
UINT32 Oct6100BufferPlayoutUnloadDef(
				tPOCT6100_BUFFER_UNLOAD			f_pBufferUnload )
{
	f_pBufferUnload->ulBufferIndex = cOCT6100_INVALID_VALUE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutUnload
UINT32 Oct6100BufferPlayoutUnload(
				tPOCT6100_INSTANCE_API			f_pApiInstance,
				tPOCT6100_BUFFER_UNLOAD			f_pBufferUnload )
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
		ulFncRes = Oct6100BufferUnloadSer( f_pApiInstance, f_pBufferUnload, TRUE );
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

Function:		Oct6100BufferPlayoutAdd

Description:    This function adds a buffer to a port's playout list on the 
				selected channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutAdd		Pointer to buffer playout add structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutAddDef
UINT32 Oct6100BufferPlayoutAddDef(
				tPOCT6100_BUFFER_PLAYOUT_ADD			f_pBufferPlayoutAdd )
{
	f_pBufferPlayoutAdd->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pBufferPlayoutAdd->ulBufferIndex = cOCT6100_INVALID_VALUE;

	f_pBufferPlayoutAdd->ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;
	f_pBufferPlayoutAdd->ulMixingMode = cOCT6100_MIXING_MINUS_6_DB;
	f_pBufferPlayoutAdd->lGainDb = 0;

	f_pBufferPlayoutAdd->fRepeat = FALSE;
	f_pBufferPlayoutAdd->ulRepeatCount = cOCT6100_REPEAT_INFINITELY;

	f_pBufferPlayoutAdd->ulDuration = cOCT6100_INVALID_VALUE;
	
	f_pBufferPlayoutAdd->ulBufferLength = cOCT6100_AUTO_SELECT;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutAdd
UINT32 Oct6100BufferPlayoutAdd(
				tPOCT6100_INSTANCE_API					f_pApiInstance,
				tPOCT6100_BUFFER_PLAYOUT_ADD			f_pBufferPlayoutAdd )
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
		ulFncRes = Oct6100BufferPlayoutAddSer( f_pApiInstance, f_pBufferPlayoutAdd );
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

Function:		Oct6100BufferPlayoutStart

Description:    This function enables playout of the specified buffer on the 
				requested channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutStart	Pointer to buffer playout start structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutStartDef
UINT32 Oct6100BufferPlayoutStartDef(
				tPOCT6100_BUFFER_PLAYOUT_START	f_pBufferPlayoutStart )
{
	f_pBufferPlayoutStart->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pBufferPlayoutStart->ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;
	f_pBufferPlayoutStart->fNotifyOnPlayoutStop = FALSE;
	f_pBufferPlayoutStart->ulUserEventId = cOCT6100_INVALID_VALUE;
	f_pBufferPlayoutStart->fAllowStartWhileActive = FALSE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutStart
UINT32 Oct6100BufferPlayoutStart(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart )
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
		ulFncRes = Oct6100BufferPlayoutStartSer( f_pApiInstance, f_pBufferPlayoutStart, cOCT6100_BUFFER_PLAYOUT_EVENT_STOP );
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

Function:		Oct6100BufferPlayoutStop

Description:    This function disables playout of a buffer on the specified 
				channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutStop	Pointer to buffer playout stop structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutStopDef
UINT32 Oct6100BufferPlayoutStopDef(
				tPOCT6100_BUFFER_PLAYOUT_STOP	f_pBufferPlayoutStop )
{
	f_pBufferPlayoutStop->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pBufferPlayoutStop->ulPlayoutPort = cOCT6100_CHANNEL_PORT_ROUT;
	f_pBufferPlayoutStop->fStopCleanly = TRUE;
	f_pBufferPlayoutStop->pfAlreadyStopped = NULL;
	f_pBufferPlayoutStop->pfNotifyOnPlayoutStop = NULL;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100BufferPlayoutStop
UINT32 Oct6100BufferPlayoutStop(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_BUFFER_PLAYOUT_STOP	f_pBufferPlayoutStop )
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
		ulFncRes = Oct6100BufferPlayoutStopSer( f_pApiInstance, f_pBufferPlayoutStop );
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


/****************************  PRIVATE FUNCTIONS  ****************************/

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetPlayoutBufferSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of playout buffers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetPlayoutBufferSwSizes
UINT32 Oct6100ApiGetPlayoutBufferSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Calculate memory needed for playout buffer list. */
	f_pInstSizes->ulPlayoutBufList = f_pOpenChip->ulMaxPlayoutBuffers * sizeof( tOCT6100_API_BUFFER );

	f_pInstSizes->ulPlayoutBufMemoryNodeList = 0;
	
	/* Calculate memory needed for playout buffer allocation software. */
	if ( f_pOpenChip->ulMaxPlayoutBuffers > 0 )
	{
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxPlayoutBuffers, &f_pInstSizes->ulPlayoutBufAlloc );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_3C;
		
		f_pInstSizes->ulPlayoutBufMemoryNodeList = 2 * f_pOpenChip->ulMaxPlayoutBuffers * sizeof( tOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE );
	}
	else
	{
		f_pInstSizes->ulPlayoutBufAlloc  = 0;
	}

	/* Calculate memory needed for list and allocation software serialization. */
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulPlayoutBufList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulPlayoutBufAlloc, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulPlayoutBufMemoryNodeList, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiPlayoutBufferSwInit

Description:    Initializes all elements of the instance structure associated
				to playout buffers.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiPlayoutBufferSwInit
UINT32 Oct6100ApiPlayoutBufferSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_BUFFER			pBufferList;
	PVOID	pBufferPlayoutAlloc;
	UINT32	ulMaxBufferPlayout;
	UINT32	ulResult, i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get the maximum number of buffer playout. */
	ulMaxBufferPlayout = pSharedInfo->ChipConfig.usMaxPlayoutBuffers;

	/* Set all entries in the buffer playout list to unused. */
	mOCT6100_GET_BUFFER_LIST_PNT( pSharedInfo, pBufferList )

	for ( i = 0; i < ulMaxBufferPlayout; i++ )
	{
		pBufferList[ i ].fReserved = FALSE;
		pBufferList[ i ].ulBufferSize = 0;
		pBufferList[ i ].ulBufferBase = cOCT6100_INVALID_VALUE;
		pBufferList[ i ].usDependencyCnt = 0;
		pBufferList[ i ].byBufferPcmLaw = cOCT6100_PCM_U_LAW;
		
	}

	/* Initialize the buffer playout allocation software to "all free". */
	if ( ulMaxBufferPlayout > 0 )
	{
		mOCT6100_GET_BUFFER_ALLOC_PNT( pSharedInfo, pBufferPlayoutAlloc )
		
		ulResult = OctapiLlmAllocInit( &pBufferPlayoutAlloc, ulMaxBufferPlayout );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_3D;
	}

	/* Initialize the amount of free memory used by playout. */
	f_pApiInstance->pSharedInfo->ChipStats.ulPlayoutMemUsed = 0;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferLoadSer

Description:    Loads a buffer in external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferLoad			Pointer to buffer configuration structure. The handle
						identifying the buffer in all future function calls is
						returned in this structure.
						
f_fReserveListStruct	Flag indicating if a list structure should be reserved
						or if the structure has been reserved before.  If this
						is set, the f_ulBufIndex variable must also be set.

f_ulBufIndex			If the f_fReserveListStruct flag is set, this index
						will identify the buffer playout list structure
						that must be used to load the specified buffer.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferLoadSer
UINT32 Oct6100BufferLoadSer(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD		f_pBufferLoad,
				IN		BOOL						f_fReserveListStruct, 
				IN		UINT32						f_ulBufIndex )
{
	UINT32	ulBufferIndex;
	UINT32	ulBufferBase;
	UINT32	ulResult;

	/* Check the user's configuration of the buffer for errors. */
	ulResult = Oct6100ApiCheckBufferParams( f_pApiInstance, f_pBufferLoad, TRUE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the buffer. */
	ulResult = Oct6100ApiReserveBufferResources( f_pApiInstance, f_pBufferLoad, f_fReserveListStruct, f_ulBufIndex, &ulBufferIndex, &ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/* Write the buffer in external memory. */
	ulResult = Oct6100ApiWriteBufferInMemory( f_pApiInstance, ulBufferBase, f_pBufferLoad->ulBufferSize, f_pBufferLoad->pbyBufferPattern );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the new buffer's entry in the buffer list. */
	ulResult = Oct6100ApiUpdateBufferEntry( f_pApiInstance, f_pBufferLoad, ulBufferIndex, ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferLoadBlockInitSer

Description:    Reserve resources for loading a buffer into external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_pBufferLoadBlockInit	Pointer to buffer configuration structure. The 
						handle identifying the buffer in all future 
						function calls is returned in this structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferLoadBlockInitSer
UINT32 Oct6100BufferLoadBlockInitSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK_INIT	f_pBufferLoadBlockInit )
{
	UINT32					ulBufferIndex;
	UINT32					ulBufferBase;
	UINT32					ulResult;
	tOCT6100_BUFFER_LOAD	BufferLoad;

	Oct6100BufferPlayoutLoadDef( &BufferLoad );

	/* Not to replicate the code, we use the BufferLoad functions directly. */
	BufferLoad.pulBufferIndex			= f_pBufferLoadBlockInit->pulBufferIndex;
	BufferLoad.pulPlayoutFreeMemSize	= f_pBufferLoadBlockInit->pulPlayoutFreeMemSize;
	BufferLoad.ulBufferPcmLaw			= f_pBufferLoadBlockInit->ulBufferPcmLaw;
	BufferLoad.ulBufferSize				= f_pBufferLoadBlockInit->ulBufferSize;
	BufferLoad.pbyBufferPattern			= NULL;  /* Must not check this for now */

	/* Check the user's configuration of the buffer for errors, but do */
	/* not check if the buffer pointer is NULL.  It is NULL for sure! */
	ulResult = Oct6100ApiCheckBufferParams( f_pApiInstance, &BufferLoad, FALSE );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the buffer. */
	ulResult = Oct6100ApiReserveBufferResources( f_pApiInstance, &BufferLoad, TRUE, cOCT6100_INVALID_INDEX, &ulBufferIndex, &ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the new buffer's entry in the buffer list. */
	ulResult = Oct6100ApiUpdateBufferEntry( f_pApiInstance, &BufferLoad, ulBufferIndex, ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferLoadBlockSer

Description:    Loads a buffer in external memory using blocks.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep 
						the present state of the chip and all its resources.

f_pBufferLoadBlock		Pointer to buffer block to be loaded into external 
						memory descriptor. 

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferLoadBlockSer
UINT32 Oct6100BufferLoadBlockSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD_BLOCK			f_pBufferLoadBlock )
{
	UINT32	ulBufferBase;
	UINT32	ulResult;

	/* Check the user's configuration for errors. */
	ulResult = Oct6100ApiCheckBufferLoadBlockParams( f_pApiInstance, f_pBufferLoadBlock, &ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write the buffer in external memory at the appropriate offset - must do some pointer arithmetic. */
	ulResult = Oct6100ApiWriteBufferInMemory( f_pApiInstance, ulBufferBase + f_pBufferLoadBlock->ulBlockOffset, 
					f_pBufferLoadBlock->ulBlockLength, f_pBufferLoadBlock->pbyBufferPattern + f_pBufferLoadBlock->ulBlockOffset );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBufferParams

Description:    Checks the user's buffer playout load configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferLoad			Pointer to buffer configuration structure.
f_fCheckBufferPtr		Check if the buffer pointer is NULL or not.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBufferParams
UINT32 Oct6100ApiCheckBufferParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD			f_pBufferLoad,
				IN		BOOL							f_fCheckBufferPtr )
{
	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_DISABLED;

	if ( f_pApiInstance->pSharedInfo->ImageInfo.fBufferPlayout == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_BUFFER_PLAYOUT;

	if ( f_pBufferLoad->pulBufferIndex == NULL )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_INDEX;

	if( f_fCheckBufferPtr )
	{
		if ( f_pBufferLoad->pbyBufferPattern == NULL )
			return cOCT6100_ERR_BUFFER_PLAYOUT_PATTERN;
	}

	if ( f_pBufferLoad->ulBufferSize < cOCT6100_MINIMUM_BUFFER_SIZE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_TOO_SMALL;

	if ( ( f_pBufferLoad->ulBufferSize % cOCT6100_BUFFER_SIZE_GRANULARITY ) != 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_SIZE;

	if ( f_pBufferLoad->ulBufferPcmLaw != cOCT6100_PCM_U_LAW && 
		 f_pBufferLoad->ulBufferPcmLaw != cOCT6100_PCM_A_LAW )
		return cOCT6100_ERR_BUFFER_PLAYOUT_PCM_LAW;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBufferLoadBlockParams

Description:    Checks the user's buffer playout load block configuration for 
				errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferLoadBlock		Pointer to buffer block descriptor.
f_pulBufferBase			Pointer to the base address of the buffer in external 
						memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBufferLoadBlockParams
UINT32 Oct6100ApiCheckBufferLoadBlockParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_BUFFER_LOAD_BLOCK		f_pBufferLoadBlock,
				OUT		PUINT32							f_pulBufferBase )
{
	/* Check for errors. */
	tPOCT6100_API_BUFFER	pBufEntry;

	if ( f_pBufferLoadBlock->ulBufferIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_INDEX;

	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufEntry, f_pBufferLoadBlock->ulBufferIndex )

	if ( pBufEntry->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOT_OPEN;

	if ( ( f_pBufferLoadBlock->ulBlockLength % 2 ) != 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BLOCK_LENGTH_INVALID;

	if ( ( f_pBufferLoadBlock->ulBlockOffset % 2 ) != 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BLOCK_OFFSET_INVALID;

	if ( f_pBufferLoadBlock->pbyBufferPattern == NULL )
		return cOCT6100_ERR_BUFFER_PLAYOUT_PATTERN;

	/* Check boundaries */
	if ( ( f_pBufferLoadBlock->ulBlockLength + f_pBufferLoadBlock->ulBlockOffset ) > pBufEntry->ulBufferSize )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_SIZE;

	*f_pulBufferBase = pBufEntry->ulBufferBase;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBufferResources

Description:    Reserves all resources needed for the new buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pBufferLoad			Pointer to buffer configuration structure.

f_fReserveListStruct	Flag indicating if a list structure should be reserved
						or if the structure has been reserved before.

f_ulBufIndex			If the f_fReserveListStruct flag is set, this index
						will identifying the buffer playout list structure
						that must be used to load the specified buffer.

f_pulBufferIndex		Allocated entry in buffer playout list.
	
f_pulBufferBase			Allocated external memory block for the buffer.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBufferResources
UINT32 Oct6100ApiReserveBufferResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_BUFFER_LOAD			f_pBufferLoad,
				IN		BOOL							f_fReserveListStruct,
				IN		UINT32							f_ulBufIndex,
				OUT		PUINT32							f_pulBufferIndex,
				OUT		PUINT32							f_pulBufferBase )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulTempVar;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Reserve an entry in the buffer list. */
	if ( f_fReserveListStruct == TRUE )
	{
		ulResult = Oct6100ApiReserveBufPlayoutListEntry( f_pApiInstance, f_pulBufferIndex );
	}
	else
	{
		*f_pulBufferIndex = f_ulBufIndex;
	}
	if ( ulResult == cOCT6100_ERR_OK )
	{
		/* Find a free block to store the buffer. */
		ulResult = Oct6100ApiReserveBufferPlayoutMemory( f_pApiInstance, f_pBufferLoad->ulBufferSize, f_pulBufferBase );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			/* Release the list entry. */
			if ( f_fReserveListStruct == TRUE )
			{
				ulTempVar = Oct6100ApiReleaseBufPlayoutListEntry( f_pApiInstance, *f_pulBufferIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;
			}
		}
	}

	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteBufferInMemory

Description:    Writes the buffer in external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulBufferBase			Allocated external memory address for the buffer.

f_ulBufferLength		Length in bytes of the buffer to be copied in memory.

f_pbyBuffer				Address where the buffer should be copied from.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteBufferInMemory
UINT32 Oct6100ApiWriteBufferInMemory( 
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulBufferBase,
				IN		UINT32					f_ulBufferLength,
				IN		PUINT8					f_pbyBuffer )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_WRITE_BURST_PARAMS	BurstParams;
	tOCT6100_WRITE_PARAMS		WriteParams;
	UINT32						ulResult;
	UINT32						ulNumWrites;
	PUINT16						pusSuperArray;
	PUINT8						pbyPlayoutBuffer;
	UINT32						ulByteCount = 0;
	UINT32						i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Set the process context and user chip ID parameters once and for all. */
	BurstParams.pProcessContext = f_pApiInstance->pProcessContext;

	BurstParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Write the buffer in external memory. */
	ulNumWrites = f_ulBufferLength / 2;

	BurstParams.ulWriteAddress = f_ulBufferBase;
	BurstParams.pusWriteData = pSharedInfo->MiscVars.ausSuperArray;

	pusSuperArray = pSharedInfo->MiscVars.ausSuperArray;
	pbyPlayoutBuffer = f_pbyBuffer;
	
	/* Check if we can maximize the bandwidth through the CPU port. */
	if ( f_pApiInstance->pSharedInfo->ChipStats.usNumberChannels == 0 )
	{
		WriteParams.ulWriteAddress = 0x234;
		WriteParams.usWriteData = 0x08ff;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK  )
			return ulResult;
	}

	while ( ulNumWrites != 0 )
	{
		if ( ulNumWrites >= pSharedInfo->ChipConfig.usMaxRwAccesses )
			BurstParams.ulWriteLength = pSharedInfo->ChipConfig.usMaxRwAccesses;
		else
			BurstParams.ulWriteLength = ulNumWrites;

		for ( i = 0; i < BurstParams.ulWriteLength; i++ )
		{
			pusSuperArray[ i ]  = ( UINT16 )(( pbyPlayoutBuffer [ ulByteCount++ ]) << 8);
			pusSuperArray[ i ] |= ( UINT16 )pbyPlayoutBuffer [ ulByteCount++ ];
		}

		mOCT6100_DRIVER_WRITE_BURST_API( BurstParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		BurstParams.ulWriteAddress += 2 * BurstParams.ulWriteLength;
		ulNumWrites -= BurstParams.ulWriteLength;

	}

	/* Make sure we revert back the changes made to the CPU bandwidth register. */
	if ( f_pApiInstance->pSharedInfo->ChipStats.usNumberChannels == 0 )
	{
		WriteParams.ulWriteAddress = 0x234;
		WriteParams.usWriteData = 0x0804;
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif
	

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBufferEntry

Description:    Updates the new buffer's entry in the buffer playout list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pBufferLoad			Pointer to buffer configuration structure.
f_ulBufferIndex			Allocated entry in buffer playout list.
f_ulBufferBase			Allocated external memory block for the buffer.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBufferEntry
UINT32 Oct6100ApiUpdateBufferEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_LOAD			f_pBufferLoad,
				IN		UINT32							f_ulBufferIndex,
				IN		UINT32							f_ulBufferBase )
{
	tPOCT6100_API_BUFFER	pBufEntry;
	UINT32					ulBufferSize = f_pBufferLoad->ulBufferSize;

	/* Obtain a pointer to the new buffer's list entry. */
	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufEntry, f_ulBufferIndex )

	/* Copy the buffer's configuration and allocated resources. */
	pBufEntry->ulBufferSize = f_pBufferLoad->ulBufferSize;
	pBufEntry->byBufferPcmLaw = (UINT8)( f_pBufferLoad->ulBufferPcmLaw & 0xFF );
	pBufEntry->ulBufferBase = f_ulBufferBase;
	
	/* Update the entries flags. */
	pBufEntry->usDependencyCnt = 0;

	/* Mark the buffer as opened. */
	pBufEntry->fReserved = TRUE;

	/* Increment the number of buffer loaded into the chip.*/
	f_pApiInstance->pSharedInfo->ChipStats.usNumberPlayoutBuffers++;
	
	/* Refresh the amount of memory used by buffer playout. */

	/* Reserved size is divisible by 64. */
	if ( ulBufferSize % 64 )
		ulBufferSize = ulBufferSize + ( 64 - ( ulBufferSize % 64 ) );
	f_pApiInstance->pSharedInfo->ChipStats.ulPlayoutMemUsed += ulBufferSize;
	
	/* Return the buffer index to the user. */
	*f_pBufferLoad->pulBufferIndex = f_ulBufferIndex;

	/* Return the amount of free memory left in the chip. */
	/* Note that this value does not give the "fragmentation" state of the available memory. */
	/* This value only gives the amount of free memory */
	if( f_pBufferLoad->pulPlayoutFreeMemSize )
		*f_pBufferLoad->pulPlayoutFreeMemSize = ( f_pApiInstance->pSharedInfo->MiscVars.ulTotalMemSize - ( f_pApiInstance->pSharedInfo->MemoryMap.ulFreeMemBaseAddress - cOCT6100_EXTERNAL_MEM_BASE_ADDRESS ) ) - ( f_pApiInstance->pSharedInfo->ChipStats.ulPlayoutMemUsed );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferUnloadSer

Description:    Unloads a buffer from external memory.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferUnload			Pointer to buffer unload structure.
f_fReleaseListStruct	Whether to release the buffer playout list structure
						or not.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferUnloadSer
UINT32 Oct6100BufferUnloadSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_UNLOAD				f_pBufferUnload,
				IN		BOOL								f_fReleaseListStruct )
{
	UINT32	ulBufferIndex;
	UINT32	ulBufferBase;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertBufferParams( f_pApiInstance, f_pBufferUnload, &ulBufferIndex, &ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources associated to the unloaded buffer. */
	ulResult = Oct6100ApiReleaseBufferResources( f_pApiInstance, ulBufferIndex, ulBufferBase, f_fReleaseListStruct );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertBufferParams

Description:    Checks the buffer playout unload configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferUnload			Pointer to buffer unload structure.
f_pulBufferIndex		Pointer to the index of the buffer in the API's buffers list.
f_pulBufferBase			Pointer to the base address of the buffer in external memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertBufferParams
UINT32 Oct6100ApiAssertBufferParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_UNLOAD				f_pBufferUnload,
				OUT		PUINT32								f_pulBufferIndex,
				OUT		PUINT32								f_pulBufferBase )
{
	tPOCT6100_API_BUFFER	pBufEntry;

	*f_pulBufferIndex = f_pBufferUnload->ulBufferIndex;

	if ( *f_pulBufferIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_INDEX;

	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufEntry, *f_pulBufferIndex )

	/* Check for errors. */
	if ( pBufEntry->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOT_OPEN;
	if ( pBufEntry->usDependencyCnt != 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ACTIVE_DEPENDENCIES;
	
	/* Return all info needed to invalidate buffer. */
	*f_pulBufferBase = pBufEntry->ulBufferBase;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBufferResources

Description:    Release resources needed by the buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_ulBufferIndex			Allocated entry in buffer playout list.
f_ulBufferBase			Allocated external memory block for the buffer.
f_fReleaseListStruct	Free the list structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBufferResources
UINT32 Oct6100ApiReleaseBufferResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulBufferIndex,
				IN		UINT32							f_ulBufferBase,
				IN		BOOL							f_fReleaseListStruct )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tPOCT6100_API_BUFFER	pBufEntry;
	UINT32	ulResult;
	UINT32	ulBufferSize;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Free the external memory reserved for the buffer. */
	ulResult = Oct6100ApiReleaseBufferPlayoutMemory( f_pApiInstance, f_ulBufferBase );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_3E;

	/* Release the entry from the buffer list. */
	if ( f_fReleaseListStruct == TRUE )
		ulResult = Oct6100ApiReleaseBufPlayoutListEntry( f_pApiInstance, f_ulBufferIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufEntry, f_ulBufferIndex );

	/* Save buffer size before releasing that entry, will be needed to calculate the amount of */
	/* free memory left for the user. */
	ulBufferSize = pBufEntry->ulBufferSize;
	
	/* Flag the buffer entry as free. */
	pBufEntry->fReserved = FALSE;

	/* Decrement the number of buffer loaded into the chip. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberPlayoutBuffers--;

	/* Refresh the amount of memory used by buffer playout. */
	/* Reserved size is divisible by 64. */
	if ( ulBufferSize % 64 )
		ulBufferSize = ulBufferSize + ( 64 - ( ulBufferSize % 64 ) );
	f_pApiInstance->pSharedInfo->ChipStats.ulPlayoutMemUsed -= ulBufferSize;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutAddSer

Description:    This function adds a buffer to a channel buffer list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutAdd		Pointer to buffer playout add structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutAddSer
UINT32 Oct6100BufferPlayoutAddSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_ADD		f_pBufferPlayoutAdd )
{
	UINT32	ulBufferIndex;
	UINT32	ulChannelIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the buffer for errors. */
	ulResult = Oct6100ApiCheckPlayoutAddParams( f_pApiInstance, f_pBufferPlayoutAdd, &ulChannelIndex, &ulBufferIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write to  all resources needed to activate buffer playout. */
	ulResult = Oct6100ApiWriteBufferAddStructs( f_pApiInstance, f_pBufferPlayoutAdd, ulChannelIndex, ulBufferIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckPlayoutAddParams

Description:	Check the validity of the channel and buffer requested.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutAdd		Pointer to buffer playout add structure.  
f_pulChannelIndex		Pointer to the channel index of the selected channel.
f_pulBufferIndex		Pointer to the buffer index within the API's buffer list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckPlayoutAddParams
UINT32 Oct6100ApiCheckPlayoutAddParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_ADD		f_pBufferPlayoutAdd,
				OUT		PUINT32								f_pulChannelIndex, 
				OUT		PUINT32								f_pulBufferIndex )
{
	tPOCT6100_API_BUFFER			pBufferEntry;
	tPOCT6100_API_CHANNEL			pEchoChannel;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_DISABLED;
	
	if ( f_pBufferPlayoutAdd->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	if ( f_pBufferPlayoutAdd->ulPlayoutPort != cOCT6100_CHANNEL_PORT_ROUT && 
		 f_pBufferPlayoutAdd->ulPlayoutPort != cOCT6100_CHANNEL_PORT_SOUT )
		return cOCT6100_ERR_BUFFER_PLAYOUT_PLAYOUT_PORT;

	if ( f_pBufferPlayoutAdd->fRepeat != TRUE && f_pBufferPlayoutAdd->fRepeat != FALSE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_REPEAT;

	if ( f_pBufferPlayoutAdd->fRepeat == TRUE )
	{
		if ( f_pBufferPlayoutAdd->ulRepeatCount != cOCT6100_REPEAT_INFINITELY )
		{
			if ( f_pBufferPlayoutAdd->ulRepeatCount == 0x0 
				|| f_pBufferPlayoutAdd->ulRepeatCount > cOCT6100_REPEAT_MAX)
			{
				return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_REPEAT_COUNT;
			}
		}
	}

	if ( f_pBufferPlayoutAdd->ulMixingMode != cOCT6100_MIXING_0_DB &&
		 f_pBufferPlayoutAdd->ulMixingMode != cOCT6100_MIXING_MINUS_6_DB &&
		 f_pBufferPlayoutAdd->ulMixingMode != cOCT6100_MIXING_MINUS_12_DB &&
		 f_pBufferPlayoutAdd->ulMixingMode != cOCT6100_MIXING_MUTE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_MIXING;

	if ( ( f_pBufferPlayoutAdd->lGainDb < -24 )
		|| ( f_pBufferPlayoutAdd->lGainDb > 24 ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_GAIN_DB;

	/*=====================================================================*/
	/* Check the channel handle. */

	if ( (f_pBufferPlayoutAdd->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	*f_pulChannelIndex = f_pBufferPlayoutAdd->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK;
	if ( *f_pulChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, *f_pulChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pBufferPlayoutAdd->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChannel->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChannel->byEntryOpenCnt )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	/* Check if repeat flag has been used for this port. */
	if ( ( ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT ) && ( pEchoChannel->fRinBufPlayoutRepeatUsed == TRUE ) )
		|| ( ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( pEchoChannel->fSoutBufPlayoutRepeatUsed == TRUE ) ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_REPEAT_USED;

	/*=====================================================================*/

	/*=====================================================================*/
	/* Check the buffer information. */

	*f_pulBufferIndex = f_pBufferPlayoutAdd->ulBufferIndex;
	if ( *f_pulBufferIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_INDEX;

	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufferEntry, *f_pulBufferIndex )

	if ( pBufferEntry->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOT_OPEN;

	/* Check if the play length is not larger then the currently uploaded buffer. */
	if ( ( f_pBufferPlayoutAdd->ulBufferLength > pBufferEntry->ulBufferSize ) &&
		  ( f_pBufferPlayoutAdd->ulBufferLength != cOCT6100_AUTO_SELECT ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_SIZE;

	if( f_pBufferPlayoutAdd->ulBufferLength != cOCT6100_AUTO_SELECT )
	{
		if ( f_pBufferPlayoutAdd->ulBufferLength < cOCT6100_MINIMUM_BUFFER_SIZE )
			return cOCT6100_ERR_BUFFER_PLAYOUT_TOO_SMALL;

		if ( ( f_pBufferPlayoutAdd->ulBufferLength % cOCT6100_BUFFER_SIZE_GRANULARITY ) != 0 )
			return cOCT6100_ERR_BUFFER_PLAYOUT_BUF_SIZE;
	}

	/*=====================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteBufferAddStructs

Description:    Write the buffer playout event in the channel's port playout
				circular buffer.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pBufferPlayoutAdd		Pointer to buffer playout add structure.  
f_ulChannelIndex		Index of the channel on which the buffer is to be added.
f_ulBufferIndex			Index of the buffer structure within the API's buffer list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteBufferAddStructs
UINT32 Oct6100ApiWriteBufferAddStructs(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_ADD		f_pBufferPlayoutAdd,
				IN		UINT32								f_ulChannelIndex, 
				IN		UINT32								f_ulBufferIndex )
{
	tPOCT6100_API_BUFFER		pBufferEntry;
	tPOCT6100_API_CHANNEL		pEchoChannel;
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tOCT6100_READ_PARAMS		ReadParams;
	UINT32	ulResult;
	UINT32	ulTempData;
	UINT32	ulEventBuffer;

	UINT32	ulReadPtrBytesOfst;
	UINT32	ulReadPtrBitOfst;
	UINT32	ulReadPtrFieldSize;

	UINT32	ulWritePtrBytesOfst;
	UINT32	ulWritePtrBitOfst;
	UINT32	ulWritePtrFieldSize;

	UINT32	ulWritePtr;
	UINT32	ulReadPtr;

	UINT32	ulPlayoutBaseAddress;
	UINT32	ulAddress;
	UINT32	ulEventIndex;
	UINT32	ulMask;

	UINT32	ulRepeatCount = 0;
	BOOL	fRepeatCountSet = FALSE;
	UINT32	ulDurationModulo = 0;
	UINT32	ulEventsToCreate = 1;
	UINT32	ulBufferDurationMs;
	
	UINT32	ulBufferLength;
	UINT16	usTempData = 0;

	UINT16	usReadData;
	UINT32	ulChipWritePtr;
	UINT32	ulReadData;
	UINT32	ulLoopCnt = 0;
	BOOL	fStillPlaying = TRUE;
	BOOL	fCheckHardStop = FALSE;
	BOOL	fOldBufferPlayoutVersion = FALSE;

	UINT32			aulWaitTime[ 2 ];

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChannel, f_ulChannelIndex );
	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufferEntry, f_ulBufferIndex );

	/* Select the buffer of interest. */
	if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainRinPlayoutMemOfst;
		ulWritePtr = pEchoChannel->ulRinBufWritePtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byFieldSize;

		ulReadPtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byFieldSize;
	}
	else /* f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainSoutPlayoutMemOfst;
		ulWritePtr = pEchoChannel->ulSoutBufWritePtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byFieldSize;

		ulReadPtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byFieldSize;
	}

	/*=======================================================================*/
	/* Calculate the repeat count. */

	/* The buffer length is either the total buffer size or the value specified by the user */
	if ( f_pBufferPlayoutAdd->ulBufferLength == cOCT6100_AUTO_SELECT )
	{
		ulBufferLength = pBufferEntry->ulBufferSize;
	}
	else
	{
		ulBufferLength = f_pBufferPlayoutAdd->ulBufferLength;
	}

	if ( f_pBufferPlayoutAdd->ulDuration != cOCT6100_INVALID_VALUE )
	{
		/* With duration and buffer length, we can find the number of times we must repeat playing this buffer. */
		ulBufferDurationMs = ulBufferLength / cOCT6100_SAMPLES_PER_MS;
		ulRepeatCount = f_pBufferPlayoutAdd->ulDuration / ulBufferDurationMs;
		fRepeatCountSet = TRUE;

		/* Check if buffer is larger then asked duration. */
		if ( ulRepeatCount != 0x0 )
		{
			/* We might have to create more then 1 event to accomodate for the repeat-max limit. */
			ulEventsToCreate = ( ulRepeatCount / cOCT6100_REPEAT_MAX ) + 1;
		}
		else
		{
			/* No repeat event.  Maybe only the duration modulo! */
			ulEventsToCreate = 0x0;
		}

		/* Check if must create a second event for a buffer that cannot be played completely. */
		ulDurationModulo = f_pBufferPlayoutAdd->ulDuration % ulBufferDurationMs;
		if ( ulDurationModulo != 0x0 )
		{
			ulDurationModulo *= cOCT6100_SAMPLES_PER_MS;
			if ( ulDurationModulo / cOCT6100_BUFFER_SIZE_GRANULARITY )
			{
				/* Round the modulo to be on a buffer size granularity. */
				/* This will round down. */
				ulDurationModulo = ( ulDurationModulo / cOCT6100_BUFFER_SIZE_GRANULARITY ) * cOCT6100_BUFFER_SIZE_GRANULARITY;

				/* If the event about to be created is smaller then the minimum buffer size, */
				/* round up to the minimum required by the hardware. */
				if ( ulDurationModulo < cOCT6100_MINIMUM_BUFFER_SIZE )
					ulDurationModulo = cOCT6100_MINIMUM_BUFFER_SIZE;
				ulEventsToCreate++;
			}
			else
			{
				/* The modulo is too small to be played.  Skip. */
				ulDurationModulo = 0;
			}
		}
	}
	else if ( f_pBufferPlayoutAdd->fRepeat == TRUE 
		&& f_pBufferPlayoutAdd->ulRepeatCount != cOCT6100_REPEAT_INFINITELY )
	{
		/* The repeat count is set directly from the user. */
		ulRepeatCount = f_pBufferPlayoutAdd->ulRepeatCount;
		fRepeatCountSet = TRUE;
	}
	
	/*=======================================================================*/

	/* Set the playout feature base address. */
	ulPlayoutBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_ulChannelIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

	/* Read the read pointer. */
	ulAddress = ulPlayoutBaseAddress + ulReadPtrBytesOfst;

	/* Must read in memory directly since this value is changed by hardware. */
	ulResult = Oct6100ApiReadDword( f_pApiInstance, ulAddress, &ulTempData );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	mOCT6100_CREATE_FEATURE_MASK( ulReadPtrFieldSize, ulReadPtrBitOfst, &ulMask );
	
	/* Store the read pointer. */
	ulReadPtr = ( ulTempData & ulMask ) >> ulReadPtrBitOfst;

	/* Compare the pointers...  Are they different?  If so, there is something already in the list.  */
	if ( ulReadPtr != ulWritePtr )
	{
		/* Check if there is enough room for the playout events. */
		if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == TRUE )
			&& ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == TRUE ) )
		{
			/* 127 or 31 events image. */
			if ( (UINT8)( ( ulWritePtr - ulReadPtr ) & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 ) ) >= ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - (UINT8)ulEventsToCreate ) )
				fCheckHardStop = TRUE;
		}
		else
		{
			/* Old 31 events image. */
			if ( ( ( ulWritePtr - ulReadPtr ) & 0x1F ) >= ( 0x20 - ulEventsToCreate ) )
				fCheckHardStop = TRUE;

			fOldBufferPlayoutVersion = TRUE;
		}

		if ( fCheckHardStop == TRUE )
		{
			/* Ok.  From what was read, the list is full.  But we might still have a chance if the hard-stop */
			/* version was used.  In this case, some of the buffers in the list might */
			/* become free in a couple of milliseconds, so try to wait for this. */
			
			if ( ( ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT ) && ( pEchoChannel->fRinHardStop == TRUE ) )
				|| ( ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( pEchoChannel->fSoutHardStop == TRUE ) ) )
			{
				/* Read the 'chip' write pointer in the hardware. */
				ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

				ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
				ReadParams.pusReadData = &usReadData;
				ReadParams.ulReadAddress = ulPlayoutBaseAddress + ulReadPtrBytesOfst;

				/* Get the write pointer in the chip. */
				ulAddress = ulPlayoutBaseAddress + ulWritePtrBytesOfst;

				mOCT6100_RETRIEVE_NLP_CONF_DWORD( f_pApiInstance, pEchoChannel, ulAddress, &ulReadData, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );

				/* Store the write pointer. */
				ulChipWritePtr = ( ulReadData & ulMask ) >> ulWritePtrBitOfst;

				/* Optimize this access by only reading the word we are interested in. */
				if ( ulReadPtrBitOfst < 16 )
					ReadParams.ulReadAddress += 2;

				while( fStillPlaying == TRUE )
				{				
					/* Read the read pointer until equals to the write pointer. */
					mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					/* Move data at correct position according to what was read. */
					if ( ulReadPtrBitOfst < 16 )
						ulTempData = usReadData;
					else
						ulTempData = usReadData << 16;
					
					mOCT6100_CREATE_FEATURE_MASK( ulReadPtrFieldSize, ulReadPtrBitOfst, &ulMask );
					
					/* Store the read pointer.*/
					ulReadPtr = ( ulTempData & ulMask ) >> ulReadPtrBitOfst;

					/* Playout has finished when the read pointer reaches the write pointer. */
					if ( ulReadPtr == ulChipWritePtr )
						break;

					ulLoopCnt++;
					if ( ulLoopCnt > cOCT6100_MAX_LOOP )
					{
						return cOCT6100_ERR_FATAL_E7;
					}

					aulWaitTime[ 0 ] = 100;
					aulWaitTime[ 1 ] = 0;
					ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;			
				}

				/* Clear hard-stop flag. */
				if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
				{
					/* No hard stop for now. */
					pEchoChannel->fRinHardStop = FALSE;
				}
				else /* if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) */
				{
					/* No hard stop for now. */
					pEchoChannel->fSoutHardStop = FALSE;
				}

				/* Now check again if the event can be added... */
				if ( fOldBufferPlayoutVersion == FALSE )
				{
					if ( (UINT8)( ( ulWritePtr - ulReadPtr ) & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 ) ) >= ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - (UINT8)ulEventsToCreate ) )
						return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_EVENT_BUF_FULL;
				}
				else /* if ( fOldBufferPlayoutVersion == TRUE ) */
				{
					/* Old 31 events image. */
					if ( ( ( ulWritePtr - ulReadPtr ) & 0x1F ) >= ( 0x20 - ulEventsToCreate ) )
						return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_EVENT_BUF_FULL;
				}

				/* Good, at least another buffer can be added!  Add the buffer to the list. */
			}
			else
			{
				/* Well the list is full! */
				return cOCT6100_ERR_BUFFER_PLAYOUT_ADD_EVENT_BUF_FULL;
			}
		}
	}

	/*=======================================================================*/
	/* Write the events. */

	for ( ulEventIndex = 0; ulEventIndex < ulEventsToCreate; ulEventIndex ++  )
	{
		/* Set the playout event base address. */
		if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == TRUE )
			&& ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == TRUE ) )
		{
			/* 127 or 31 events image. */
			ulAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + (f_ulChannelIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + ulEventBuffer + (cOCT6100_PLAYOUT_EVENT_MEM_SIZE * (ulWritePtr & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 )));
		}
		else
		{
			/* Old 31 events image. */
			ulAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + (f_ulChannelIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + ulEventBuffer + (cOCT6100_PLAYOUT_EVENT_MEM_SIZE * (ulWritePtr & 0x1F));
		}
	
		/* EVENT BASE + 0 */
		/* Make sure the xIS and xHS bits are cleared. */
		ulTempData = 0;

		/* Set the repeat count. */
		if ( fRepeatCountSet == TRUE )
		{
			if ( ( ulRepeatCount != 0x0 ) && ( ulRepeatCount <= cOCT6100_REPEAT_MAX ) )
			{
				/* Use repeat count directly. */
				ulTempData |= ulRepeatCount;

				/* Will be used later when creating the duration modulo event. */
				ulRepeatCount = 0;
			}
			else if ( ulRepeatCount != 0x0 )
			{
				/* Get ready for next event. */
				ulRepeatCount -= cOCT6100_REPEAT_MAX;

				/* Set maximum for this event. */
				ulTempData |= cOCT6100_REPEAT_MAX;
			}
			else
			{
				/* Duration modulo case.  Nothing to set here. */
			}
		}
		else /* if ( fRepeatCountSet != TRUE ) */
		{
			/* Repeat only once. */
			ulTempData |= 0x1;
		}

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulAddress, ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* EVENT BASE + 4 */
		/* Set the buffer base address and playout configuration. */
		ulAddress += 4;
		ulTempData = pBufferEntry->ulBufferBase & 0x07FFFFFF;

		/* Set play indefinitely or loop N times. */
		if ( ( fRepeatCountSet == FALSE ) && ( f_pBufferPlayoutAdd->fRepeat == TRUE ) )
		{
			/* Repeat indefinitely. */
			ulTempData |= 0x1 << cOCT6100_PLAYOUT_EVENT_REPEAT_OFFSET;

			if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
				pEchoChannel->fRinBufPlayoutRepeatUsed = TRUE;
			else /* if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) */
				pEchoChannel->fSoutBufPlayoutRepeatUsed = TRUE;
		}
		
		/* Use loop N times feature. */
		ulTempData |= 0x1 << cOCT6100_PLAYOUT_EVENT_LOOP_TIMES_OFFSET;

		/* Set the law.*/
		ulTempData |= ( pBufferEntry->byBufferPcmLaw << cOCT6100_PLAYOUT_EVENT_LAW_OFFSET );

		/* Set the mixing configuration.*/
		ulTempData |= f_pBufferPlayoutAdd->ulMixingMode << cOCT6100_PLAYOUT_EVENT_MIX_OFFSET;

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulAddress, ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		
		/* EVENT BASE + 8 */
		/* Set the buffer size and playout gain. */
		ulAddress += 4;

		/* Check if we are setting the duration modulo.  This would be the last event and this */
		/* event is of a very specific size. */
		if ( ( fRepeatCountSet == TRUE ) 
			&& ( ulEventIndex == ( ulEventsToCreate - 1 ) ) 
			&& ( ulDurationModulo != 0x0 ) )
		{
			/* The duration modulo variable contains all that is needed here. */
			ulBufferLength = ulDurationModulo;
		}
		ulTempData = ulBufferLength;

		/* Adjust playout gain. */
		if ( f_pBufferPlayoutAdd->lGainDb != 0 )
		{
			/* Convert the dB value into OctFloat format. */
			usTempData = Oct6100ApiDbAmpHalfToOctFloat( f_pBufferPlayoutAdd->lGainDb );
			ulTempData |= ( usTempData & 0xFF00 ) << 16;
		}
		else
		{
			ulTempData |= cOCT6100_PLAYOUT_GAIN;
		}

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulAddress, ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
			
		/* EVENT BASE + 0xC */
		ulAddress += 4;
		ulTempData = ( ulBufferLength - 1 ) & 0xFFFFFFC0;	/* Must be multiple of 64 bytes */

		/* Adjust playout gain. */
		if ( f_pBufferPlayoutAdd->lGainDb != 0 )
		{
			ulTempData |= ( usTempData & 0xFF ) << 24;
		}

		ulResult = Oct6100ApiWriteDword( f_pApiInstance, ulAddress, ulTempData );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Next event. */
		ulWritePtr++;
	}

	/*=======================================================================*/


	/*=======================================================================*/
	/* Increment the write pointer to make it point to the next empty entry. */

	if ( f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		pEchoChannel->ulRinBufWritePtr = ( pEchoChannel->ulRinBufWritePtr + ulEventsToCreate ) & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 );
		/* Remember that a buffer was added on the rin port. */
		pEchoChannel->fRinBufAdded = TRUE;
	}
	else /* f_pBufferPlayoutAdd->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		pEchoChannel->ulSoutBufWritePtr = ( pEchoChannel->ulSoutBufWritePtr + ulEventsToCreate ) & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 );
		/* Remember that a buffer was added on the sout port. */
		pEchoChannel->fSoutBufAdded = TRUE;
	}

	/*=======================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutStartSer

Description:    Starts buffer playout on a channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pBufferPlayoutStart		Pointer to buffer playout start structure.

f_ulPlayoutStopEventType	Playout stop event type to be generated if required.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutStartSer
UINT32 Oct6100BufferPlayoutStartSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart,
				IN		UINT32								f_ulPlayoutStopEventType )
{
	UINT32	ulBufferIndex = 0;
	UINT32	ulChannelIndex;
	BOOL	fNotifyOnPlayoutStop;
	UINT32	ulUserEventId;
	BOOL	fAddToCurrentlyPlayingList;
	UINT32	ulResult;

	/* Check the user's configuration of the buffer for errors. */
	ulResult = Oct6100ApiCheckPlayoutStartParams( f_pApiInstance, f_pBufferPlayoutStart, &ulChannelIndex, &ulBufferIndex, &fNotifyOnPlayoutStop, &ulUserEventId, &fAddToCurrentlyPlayingList );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write to all resources needed to activate buffer playout. */
	ulResult = Oct6100ApiWriteChanPlayoutStructs( f_pApiInstance, f_pBufferPlayoutStart, ulChannelIndex, ulBufferIndex, fNotifyOnPlayoutStop, ulUserEventId, fAddToCurrentlyPlayingList, f_ulPlayoutStopEventType );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckPlayoutStartParams

Description:	Check the validity of the channel and buffer requested.
				Check the validity of the flags requested.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutStart	Pointer to buffer playout start structure.  
f_pulChannelIndex		Pointer to the channel index of the selected channel.
f_pulBufferIndex		Pointer to the buffer index within the API's buffer list.
f_pfNotifyOnPlayoutStop	Pointer to the notify on playout stop flag.
f_pulUserEventId		Pointer to the user event id specified.
f_pfAllowStartIfActive	Pointer to the add to currently playing list flag.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckPlayoutStartParams
UINT32 Oct6100ApiCheckPlayoutStartParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart,
				OUT		PUINT32								f_pulChannelIndex, 
				OUT		PUINT32								f_pulBufferIndex,
				OUT		PBOOL								f_pfNotifyOnPlayoutStop,
				OUT		PUINT32								f_pulUserEventId,
				OUT		PBOOL								f_pfAllowStartIfActive )
{
	tPOCT6100_API_CHANNEL	pEchoChannel;
	UINT32					ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_DISABLED;
	
	if ( f_pBufferPlayoutStart->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	if ( f_pBufferPlayoutStart->ulPlayoutPort != cOCT6100_CHANNEL_PORT_ROUT && 
		 f_pBufferPlayoutStart->ulPlayoutPort != cOCT6100_CHANNEL_PORT_SOUT )
		return cOCT6100_ERR_BUFFER_PLAYOUT_PLAYOUT_PORT;

	if ( f_pBufferPlayoutStart->fNotifyOnPlayoutStop != FALSE
		&& f_pBufferPlayoutStart->fNotifyOnPlayoutStop != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOTIFY_ON_STOP;

	if ( f_pBufferPlayoutStart->fAllowStartWhileActive != FALSE
		&& f_pBufferPlayoutStart->fAllowStartWhileActive != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ALLOW_ACTIVE;

	/*=====================================================================*/
	/* Check the channel handle. */

	if ( (f_pBufferPlayoutStart->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	*f_pulChannelIndex = f_pBufferPlayoutStart->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK;
	if ( *f_pulChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, *f_pulChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pBufferPlayoutStart->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChannel->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChannel->byEntryOpenCnt )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	/* The channel cannot be in POWER_DOWN or HT_FREEZE to start the playout. */
	if ( ( pEchoChannel->byEchoOperationMode == cOCT6100_ECHO_OP_MODE_POWER_DOWN )
		|| ( pEchoChannel->byEchoOperationMode == cOCT6100_ECHO_OP_MODE_HT_FREEZE ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_ECHO_OP_MODE;
	
	/* The channel's NLP must be enabled for playout to occur. */
	if ( pEchoChannel->VqeConfig.fEnableNlp == FALSE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NLP_DISABLED;

	/*=====================================================================*/

	/*=====================================================================*/
	/* Check if the user activated the buffer playout events. */

	if ( f_pBufferPlayoutStart->fNotifyOnPlayoutStop == TRUE
		&& f_pApiInstance->pSharedInfo->ChipConfig.ulSoftBufPlayoutEventsBufSize == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_EVENT_DISABLED;

	/*=====================================================================*/

	/*=====================================================================*/
	/* Check if there is actually a buffer added in the list. */

	if ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		if ( pEchoChannel->fRinBufAdded == FALSE )
			return cOCT6100_ERR_BUFFER_PLAYOUT_LIST_EMPTY;
	}
	else /* if ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) */
	{
		if ( pEchoChannel->fSoutBufAdded == FALSE )
			return cOCT6100_ERR_BUFFER_PLAYOUT_LIST_EMPTY;
	}

	/*=====================================================================*/

	/* Return the requested information. */
	*f_pfNotifyOnPlayoutStop = f_pBufferPlayoutStart->fNotifyOnPlayoutStop;
	*f_pulUserEventId = f_pBufferPlayoutStart->ulUserEventId;
	*f_pfAllowStartIfActive = f_pBufferPlayoutStart->fAllowStartWhileActive;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiWriteChanPlayoutStructs

Description:    Write the buffer playout event in the channel main structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
	
f_pBufferPlayoutStart		Pointer to buffer playout start structure.
f_ulChannelIndex			Index of the channel within the API's channel list.
f_ulBufferIndex				Index of the buffer within the API's buffer list.
f_fNotifyOnPlayoutStop		Flag for the notify on playout stop.
f_ulUserEventId				User event id passed to the user when a playout event is generated.
f_fAllowStartIfActive		Add to currently playing list flag.
f_ulPlayoutStopEventType	Playout stop event type to be generated if required.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiWriteChanPlayoutStructs
UINT32 Oct6100ApiWriteChanPlayoutStructs(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_START		f_pBufferPlayoutStart,
				IN		UINT32								f_ulChannelIndex, 
				IN		UINT32								f_ulBufferIndex,
				IN		BOOL								f_fNotifyOnPlayoutStop,
				IN		UINT32								f_ulUserEventId,
				IN		BOOL								f_fAllowStartIfActive,
				IN		UINT32								f_ulPlayoutStopEventType )
{
	tPOCT6100_API_BUFFER			pBufferEntry;
	tPOCT6100_API_CHANNEL			pEchoChannel;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_READ_PARAMS			ReadParams;
	
	UINT32	ulResult;

	UINT32	ulWritePtr;
	UINT32	ulChipWritePtr;
	PUINT32	pulSkipPtr;
	UINT32	ulWritePtrBytesOfst;
	UINT32	ulSkipPtrBytesOfst;
	UINT32	ulWritePtrBitOfst;
	UINT32	ulSkipPtrBitOfst;
	UINT32	ulWritePtrFieldSize;
	UINT32	ulSkipPtrFieldSize;

	UINT32	ulIgnoreBytesOfst;
	UINT32	ulIgnoreBitOfst;
	UINT32	ulIgnoreFieldSize;
	
	UINT32	ulHardSkipBytesOfst;
	UINT32	ulHardSkipBitOfst;
	UINT32	ulHardSkipFieldSize;

	UINT32	ulReadPtrBytesOfst;
	UINT32	ulReadPtrBitOfst;
	UINT32	ulReadPtrFieldSize;

	UINT32	ulPlayoutBaseAddress;
	UINT32	ulAddress;
	UINT32	ulTempData;
	UINT32	ulMask;
	UINT32	ulReadData;
	UINT32	ulReadPtr;
	UINT32	ulLoopCnt = 0;

	UINT16	usReadData;

	BOOL	fBufferPlayoutStopDetected;
	BOOL	fWriteSkipPtr = FALSE;
	BOOL	fStillPlaying = TRUE;

	UINT32			aulWaitTime[ 2 ];

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChannel, f_ulChannelIndex );
	mOCT6100_GET_BUFFER_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBufferEntry, f_ulBufferIndex );

	/* First off, check for buffer playout events, if requested for this channel/port. */
	/* At the same time, if requested, check that the playout has stopped for this channel/port. */
	if ( ( ( pEchoChannel->fRinBufPlaying == TRUE )
			&& ( ( pEchoChannel->fRinBufPlayoutNotifyOnStop == TRUE ) || ( f_fAllowStartIfActive == FALSE ) )
			&& ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT ) )
		|| ( ( ( pEchoChannel->fSoutBufPlaying == TRUE ) || ( f_fAllowStartIfActive == FALSE ) )
			&& ( pEchoChannel->fSoutBufPlayoutNotifyOnStop == TRUE )
			&& ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) ) )
	{
		/* Buffer playout might still be going on for this channel/port. */
		ulResult = Oct6100BufferPlayoutCheckForSpecificEvent(	f_pApiInstance, 
																f_ulChannelIndex, 
																f_pBufferPlayoutStart->ulPlayoutPort,
																pEchoChannel->fRinBufPlayoutNotifyOnStop, 
																&fBufferPlayoutStopDetected );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Check if the user requested to only start if playout is over.  Return an error if */
		/* buffer playout is still going on on this channel/port. */
		if ( ( f_fAllowStartIfActive == FALSE ) && ( fBufferPlayoutStopDetected == FALSE ) )
		{
			/* No go!  User should wait for the current list to stop, or call the */
			/* Oct6100BufferPlayoutStop function. */
			return cOCT6100_ERR_BUFFER_PLAYOUT_STILL_ACTIVE;
		}
	}

	/* Select the buffer of interest. */
	if ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		ulWritePtr  = pEchoChannel->ulRinBufWritePtr;
		pulSkipPtr	= &pEchoChannel->ulRinBufSkipPtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.usDwordOffset * 4;
		ulSkipPtrBytesOfst  = pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.usDwordOffset * 4;
		ulIgnoreBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.usDwordOffset * 4;
		ulHardSkipBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.usDwordOffset * 4;
		ulReadPtrBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.usDwordOffset * 4;

		ulWritePtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byBitOffset;
		ulSkipPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.byBitOffset;
		ulIgnoreBitOfst		= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.byBitOffset;
		ulHardSkipBitOfst	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.byBitOffset;
		ulReadPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byBitOffset;

		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byFieldSize;
		ulSkipPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.byFieldSize;
		ulIgnoreFieldSize	= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.byFieldSize;
		ulHardSkipFieldSize	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.byFieldSize;
		ulReadPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byFieldSize;
	}
	else /* f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		ulWritePtr	= pEchoChannel->ulSoutBufWritePtr;
		pulSkipPtr	= &pEchoChannel->ulSoutBufSkipPtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.usDwordOffset * 4;
		ulSkipPtrBytesOfst  = pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.usDwordOffset * 4;
		ulIgnoreBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.usDwordOffset * 4;
		ulHardSkipBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.usDwordOffset * 4;
		ulReadPtrBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.usDwordOffset * 4;

		ulWritePtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byBitOffset;
		ulSkipPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.byBitOffset;
		ulIgnoreBitOfst		= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.byBitOffset;
		ulHardSkipBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.byBitOffset;
		ulReadPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byBitOffset;

		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byFieldSize;
		ulSkipPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.byFieldSize;
		ulIgnoreFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.byFieldSize;
		ulHardSkipFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.byFieldSize;
		ulReadPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byFieldSize;
	}
	


	/* Set the playout feature base address. */
	ulPlayoutBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_ulChannelIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

	/* Check if we must wait for stop to complete before starting a new list. */
	if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE )
	{
		if ( ( ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT ) && ( pEchoChannel->fRinHardStop == TRUE ) )
			|| ( ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( pEchoChannel->fSoutHardStop == TRUE ) ) )
		{
			/* Read the read pointer. */
			ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

			ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
			ReadParams.pusReadData = &usReadData;
			ReadParams.ulReadAddress = ulPlayoutBaseAddress + ulReadPtrBytesOfst;

			/* Get the write pointer in the chip. */
			ulAddress = ulPlayoutBaseAddress + ulWritePtrBytesOfst;

			mOCT6100_RETRIEVE_NLP_CONF_DWORD( f_pApiInstance, pEchoChannel, ulAddress, &ulReadData, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );

			/* Store the write pointer. */
			ulChipWritePtr = ( ulReadData & ulMask ) >> ulWritePtrBitOfst;

			/* Optimize this access by only reading the word we are interested in. */
			if ( ulReadPtrBitOfst < 16 )
				ReadParams.ulReadAddress += 2;

			while( fStillPlaying == TRUE )
			{				
				/* Read the read pointer until equals to the write pointer. */
				mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Move data at correct position according to what was read. */
				if ( ulReadPtrBitOfst < 16 )
					ulTempData = usReadData;
				else
					ulTempData = usReadData << 16;
				
				mOCT6100_CREATE_FEATURE_MASK( ulReadPtrFieldSize, ulReadPtrBitOfst, &ulMask );
				
				/* Store the read pointer. */
				ulReadPtr = ( ulTempData & ulMask ) >> ulReadPtrBitOfst;

				/* Playout has finished when the read pointer reaches the write pointer. */
				if ( ulReadPtr == ulChipWritePtr )
					break;

				ulLoopCnt++;
				if( ulLoopCnt > cOCT6100_MAX_LOOP )
				{
					return cOCT6100_ERR_FATAL_E6;
				}

				aulWaitTime[ 0 ] = 100;
				aulWaitTime[ 1 ] = 0;
				ulResult = Oct6100ApiWaitForTime( f_pApiInstance, aulWaitTime );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;			
			}
		}
	}

	/* Check if must clear the skip bit. */
	if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE )
	{
		if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == TRUE )
			&& ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == TRUE ) )
		{
			/* Make sure the skip bit is cleared to start playout! */
			ulAddress = ulPlayoutBaseAddress + ulIgnoreBytesOfst;

			mOCT6100_RETRIEVE_NLP_CONF_DWORD( f_pApiInstance, pEchoChannel, ulAddress, &ulTempData, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			mOCT6100_CREATE_FEATURE_MASK( ulIgnoreFieldSize, ulIgnoreBitOfst, &ulMask );

			/* Cleared! */
			ulTempData &= ( ~ulMask );

			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pEchoChannel,
											ulAddress,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Make sure the hard skip bit is cleared to start playout! */
			ulAddress = ulPlayoutBaseAddress + ulHardSkipBytesOfst;

			mOCT6100_RETRIEVE_NLP_CONF_DWORD(	f_pApiInstance,
												pEchoChannel,
												ulAddress,
												&ulTempData,
												ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			mOCT6100_CREATE_FEATURE_MASK( ulIgnoreFieldSize, ulHardSkipBitOfst, &ulMask );

			/* Cleared! */
			ulTempData &= ( ~ulMask );

			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pEchoChannel,
											ulAddress,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Write the skip and write pointer to activate buffer playout. */

	/* Update the skip pointer. */
	if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == FALSE )
		|| ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == FALSE ) )
	{
		/* Old 31 events image. */
		if ( ( ( ulWritePtr - *pulSkipPtr ) & 0x7F ) > 63 )
		{
			*pulSkipPtr = ( ulWritePtr - 63 ) & 0x7F;
			fWriteSkipPtr = TRUE;
		}
	}
	else
	{
		/* No need to update the skip pointer, a bit needs to be set when skipping. */
		/* fWriteSkipPtr set to FALSE from variable declaration. */
	}

	if ( fWriteSkipPtr == TRUE )
	{
		/*=======================================================================*/
		/* Fetch and modify the skip pointer. */	

		ulAddress = ulPlayoutBaseAddress + ulSkipPtrBytesOfst;

		mOCT6100_RETRIEVE_NLP_CONF_DWORD(	f_pApiInstance,
											pEchoChannel,
											ulAddress,
											&ulTempData,
											ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		mOCT6100_CREATE_FEATURE_MASK( ulSkipPtrFieldSize, ulSkipPtrBitOfst, &ulMask );
		
		ulTempData &= ( ~ulMask );
		ulTempData |= *pulSkipPtr << ulSkipPtrBitOfst;
		
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pEchoChannel,
										ulAddress,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/
	}


	/*=======================================================================*/
	/* Fetch and modify the write pointer. */	

	ulAddress = ulPlayoutBaseAddress + ulWritePtrBytesOfst;

	mOCT6100_RETRIEVE_NLP_CONF_DWORD(	f_pApiInstance,
										pEchoChannel,
										ulAddress,
										&ulTempData,
										ulResult );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );
	
	ulTempData &= ( ~ulMask );
	ulTempData |= ulWritePtr << ulWritePtrBitOfst;
	
	ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
									pEchoChannel,
									ulAddress,
									ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*=======================================================================*/
	

	/*=======================================================================*/
	/* Now update the state of the channel stating that the buffer playout is activated. */

	/* Select the buffer of interest.*/
	if ( f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		/* Check if the global ports active stat must be incremented. */
		if ( pEchoChannel->fRinBufPlaying == FALSE )
		{
			/* Increment the number of active buffer playout ports. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts++;
		}

		pEchoChannel->fRinBufPlaying = TRUE;
		/* Keep the new notify on event flag. */
		pEchoChannel->fRinBufPlayoutNotifyOnStop = (UINT8)( f_fNotifyOnPlayoutStop & 0xFF );
		/* Keep the specified user event id. */
		pEchoChannel->ulRinUserBufPlayoutEventId = f_ulUserEventId;
		/* Keep type of event to be generated. */
		pEchoChannel->byRinPlayoutStopEventType = (UINT8)( f_ulPlayoutStopEventType & 0xFF );
		/* No hard stop for now. */
		pEchoChannel->fRinHardStop = FALSE;
		/* No buffer added in the rin list for now. */
		pEchoChannel->fRinBufAdded = FALSE;
		/* Buffer playout is active on this channel. */
		pEchoChannel->fBufPlayoutActive = TRUE;
	}
	else /* f_pBufferPlayoutStart->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		/* Check if the global ports active stat must be incremented. */
		if ( pEchoChannel->fSoutBufPlaying == FALSE )
		{
			/* Increment the number of active buffer playout ports. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts++;
		}

		pEchoChannel->fSoutBufPlaying = TRUE;
		/* Keep the new notify on event flag. */
		pEchoChannel->fSoutBufPlayoutNotifyOnStop = (UINT8)( f_fNotifyOnPlayoutStop & 0xFF );
		/* Keep the specified user event id. */
		pEchoChannel->ulSoutUserBufPlayoutEventId = f_ulUserEventId;
		/* Keep type of event to be generated. */
		pEchoChannel->bySoutPlayoutStopEventType = (UINT8)( f_ulPlayoutStopEventType & 0xFF );
		/* No hard stop for now. */
		pEchoChannel->fSoutHardStop = FALSE;
		/* No buffer added in the sout list for now. */
		pEchoChannel->fSoutBufAdded = FALSE;
		/* Buffer playout is active on this channel. */
		pEchoChannel->fBufPlayoutActive = TRUE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100BufferPlayoutStopSer

Description:    Stops buffer playout on a channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutStop	Pointer to buffer playout stop structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100BufferPlayoutStopSer
UINT32 Oct6100BufferPlayoutStopSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN OUT	tPOCT6100_BUFFER_PLAYOUT_STOP		f_pBufferPlayoutStop )
{
	UINT32	ulChannelIndex;
	UINT16	usEchoMemIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the buffer for errors. */
	ulResult = Oct6100ApiAssertPlayoutStopParams( 
												f_pApiInstance, 
												f_pBufferPlayoutStop, 
												&ulChannelIndex, 
												&usEchoMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Write to  all resources needed to deactivate buffer playout. */
	ulResult = Oct6100ApiInvalidateChanPlayoutStructs( 
												f_pApiInstance, 
												f_pBufferPlayoutStop, 
												ulChannelIndex, 
												usEchoMemIndex 

												);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertPlayoutStopParams

Description:	Check the validity of the channel and buffer requested.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pBufferPlayoutStop	Pointer to buffer playout stop structure.  
f_pulChannelIndex		Pointer to the channel index on which playout is to be stopped.
f_pusEchoMemIndex		Pointer to the echo mem index on which playout is to be stopped.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertPlayoutStopParams
UINT32 Oct6100ApiAssertPlayoutStopParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_STOP		f_pBufferPlayoutStop,
				OUT		PUINT32								f_pulChannelIndex,
				OUT		PUINT16								f_pusEchoMemIndex )
{
	tPOCT6100_API_CHANNEL		pEchoChannel;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxPlayoutBuffers == 0 )
		return cOCT6100_ERR_BUFFER_PLAYOUT_DISABLED;
	
	if ( f_pBufferPlayoutStop->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	if ( f_pBufferPlayoutStop->ulPlayoutPort != cOCT6100_CHANNEL_PORT_ROUT && 
		 f_pBufferPlayoutStop->ulPlayoutPort != cOCT6100_CHANNEL_PORT_SOUT )
		return cOCT6100_ERR_BUFFER_PLAYOUT_PLAYOUT_PORT;

	if ( f_pBufferPlayoutStop->fStopCleanly != TRUE && f_pBufferPlayoutStop->fStopCleanly != FALSE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_STOP_CLEANLY;
	
	/*=====================================================================*/
	/* Check the channel handle. */

	if ( (f_pBufferPlayoutStop->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	*f_pulChannelIndex = f_pBufferPlayoutStop->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK;
	if ( *f_pulChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChannel, *f_pulChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pBufferPlayoutStop->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChannel->fReserved != TRUE )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChannel->byEntryOpenCnt )
		return cOCT6100_ERR_BUFFER_PLAYOUT_CHANNEL_HANDLE_INVALID;

	/* Return echo memory index. */
	*f_pusEchoMemIndex = pEchoChannel->usEchoMemIndex;

	/* Check if buffer playout is active for the selected port. */
	if ( ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
		&& ( pEchoChannel->fRinBufPlaying == FALSE )
		&& ( pEchoChannel->fRinBufAdded == FALSE ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOT_STARTED;

	if ( ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT )
		&& ( pEchoChannel->fSoutBufPlaying == FALSE )
		 && ( pEchoChannel->fSoutBufAdded == FALSE ) )
		return cOCT6100_ERR_BUFFER_PLAYOUT_NOT_STARTED;
	
	/*=====================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiInvalidateChanPlayoutStructs

Description:    Write the buffer playout event in the channel main structure.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
	
f_pBufferPlayoutStop		Pointer to buffer playout stop structure.  
f_ulChannelIndex			Index of the channel within the API's channel list.
f_usEchoMemIndex			Index of the echo channel in hardware memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiInvalidateChanPlayoutStructs
UINT32 Oct6100ApiInvalidateChanPlayoutStructs(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_BUFFER_PLAYOUT_STOP		f_pBufferPlayoutStop,
				IN		UINT32								f_ulChannelIndex,
				IN		UINT16								f_usEchoMemIndex

				)
{
	tPOCT6100_API_CHANNEL	pEchoChannel;
	tPOCT6100_SHARED_INFO	pSharedInfo;
	tOCT6100_READ_PARAMS	ReadParams;
	tOCT6100_WRITE_PARAMS	WriteParams;

	UINT32	ulResult;

	UINT32	ulWritePtrBytesOfst;
	UINT32	ulWritePtrBitOfst;
	UINT32	ulWritePtrFieldSize;
	UINT32	ulSkipPtrBytesOfst;
	UINT32	ulSkipPtrBitOfst;
	UINT32	ulSkipPtrFieldSize;
	UINT32	ulIgnoreBytesOfst;
	UINT32	ulIgnoreBitOfst;
	UINT32	ulIgnoreFieldSize;
	UINT32	ulHardSkipBytesOfst;
	UINT32	ulHardSkipBitOfst;
	UINT32	ulHardSkipFieldSize;
	UINT32	ulReadPtrBytesOfst;
	UINT32	ulReadPtrBitOfst;
	UINT32	ulReadPtrFieldSize;

	UINT32	ulSkipPtr;
	UINT32	ulWritePtr;
	UINT32	ulReadPtr = 0;
	UINT32	ulCurrentPtr;

	UINT32	ulPlayoutBaseAddress;
	UINT32	ulAddress;
	UINT32	ulTempData;
	UINT32	ulMask;
	UINT32	ulReadData;

	UINT16	usReadData;	
	BOOL	fCheckStop = FALSE;

	UINT32	ulEventBuffer;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = f_pApiInstance->pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChannel, f_ulChannelIndex );

	/* Select the port of interest. */
	if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		ulWritePtr = pEchoChannel->ulRinBufWritePtr; 
		ulSkipPtr  = ulWritePtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutRinWritePtrOfst.byFieldSize;

		ulSkipPtrBytesOfst  = pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.usDwordOffset * 4;
		ulSkipPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.byBitOffset;
		ulSkipPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutRinSkipPtrOfst.byFieldSize;

		ulIgnoreBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.usDwordOffset * 4;
		ulIgnoreBitOfst		= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.byBitOffset;
		ulIgnoreFieldSize	= pSharedInfo->MemoryMap.PlayoutRinIgnoreSkipCleanOfst.byFieldSize;

		ulHardSkipBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.usDwordOffset * 4;
		ulHardSkipBitOfst	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.byBitOffset;
		ulHardSkipFieldSize	= pSharedInfo->MemoryMap.PlayoutRinHardSkipOfst.byFieldSize;

		ulReadPtrBytesOfst	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutRinReadPtrOfst.byFieldSize;
	}
	else /* f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		ulWritePtr = pEchoChannel->ulSoutBufWritePtr; 
		ulSkipPtr  = ulWritePtr;

		ulWritePtrBytesOfst = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.usDwordOffset * 4;
		ulWritePtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byBitOffset;
		ulWritePtrFieldSize = pSharedInfo->MemoryMap.PlayoutSoutWritePtrOfst.byFieldSize;

		ulSkipPtrBytesOfst  = pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.usDwordOffset * 4;
		ulSkipPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.byBitOffset;
		ulSkipPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutSkipPtrOfst.byFieldSize;

		ulIgnoreBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.usDwordOffset * 4;
		ulIgnoreBitOfst		= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.byBitOffset;
		ulIgnoreFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutIgnoreSkipCleanOfst.byFieldSize;

		ulHardSkipBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.usDwordOffset * 4;
		ulHardSkipBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.byBitOffset;
		ulHardSkipFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutHardSkipOfst.byFieldSize;

		ulReadPtrBytesOfst	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.usDwordOffset * 4;
		ulReadPtrBitOfst	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byBitOffset;
		ulReadPtrFieldSize	= pSharedInfo->MemoryMap.PlayoutSoutReadPtrOfst.byFieldSize;
	}

	/* Set the playout feature base address. */
	ulPlayoutBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usEchoMemIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

	/* Check if something is currently playing. */
	if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		if ( pEchoChannel->fRinBufPlaying == TRUE )
		{
			/* Check if we are stopping it or if it stopped by itself. */
			fCheckStop = TRUE;
		}
		else
		{
			/* Not playing! */
			if ( f_pBufferPlayoutStop->pfAlreadyStopped != NULL )
				*f_pBufferPlayoutStop->pfAlreadyStopped = TRUE;
		}
	}
	else /* if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) */
	{
		if ( pEchoChannel->fSoutBufPlaying == TRUE )
		{
			/* Check if we are stopping it or if it stopped by itself. */
			fCheckStop = TRUE;
		}
		else
		{
			/* Not playing! */
			if ( f_pBufferPlayoutStop->pfAlreadyStopped != NULL )
				*f_pBufferPlayoutStop->pfAlreadyStopped = TRUE;
		}
	}

	if ( ( fCheckStop == TRUE ) || ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE ) )
	{
		/* Read the read pointer. */
		ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

		ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
		ReadParams.pusReadData = &usReadData;
		ReadParams.ulReadAddress = ulPlayoutBaseAddress + ulReadPtrBytesOfst;

		/* Optimize this access by only reading the word we are interested in. */
		if ( ulReadPtrBitOfst < 16 )
			ReadParams.ulReadAddress += 2;

		/* Must read in memory directly since this value is changed by hardware */
		mOCT6100_DRIVER_READ_API( ReadParams, ulResult )
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Move data at correct position according to what was read. */
		if ( ulReadPtrBitOfst < 16 )
			ulTempData = usReadData;
		else
			ulTempData = usReadData << 16;
		
		mOCT6100_CREATE_FEATURE_MASK( ulReadPtrFieldSize, ulReadPtrBitOfst, &ulMask );
		
		/* Store the read pointer. */
		ulReadPtr = ( ulTempData & ulMask ) >> ulReadPtrBitOfst;

		/* Playout has finished when the read pointer reaches the write pointer. */
		if ( f_pBufferPlayoutStop->pfAlreadyStopped != NULL )
		{
			if ( ulReadPtr != ulWritePtr )
				*f_pBufferPlayoutStop->pfAlreadyStopped = FALSE;
			else /* if ( ulReadPtr == ulWritePtr ) */
				*f_pBufferPlayoutStop->pfAlreadyStopped = TRUE;
		}
	}

	/* If the skip bits are located in the event itself, the playout is stopped by setting the */
	/* skip pointer to the hardware chip write pointer.  Read it directly from the NLP configuration. */
	if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE )
	{
		if ( ulReadPtr != ulWritePtr )
		{
			/* Get the write pointer in the chip. */
			ulAddress = ulPlayoutBaseAddress + ulWritePtrBytesOfst;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance, pEchoChannel, ulAddress, &ulReadData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );

			/* Store the write pointer. */
			ulWritePtr = ( ulReadData & ulMask ) >> ulWritePtrBitOfst;
			ulSkipPtr = ulWritePtr;
		}
	}

	/* Check if must clear repeat bit. */
	if ( ( ( pEchoChannel->fRinBufPlayoutRepeatUsed == TRUE ) && ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT ) )
		|| ( ( pEchoChannel->fSoutBufPlayoutRepeatUsed == TRUE ) && ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) ) )
	{
		if ( ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE )
			|| ( ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE )
				&& ( ulWritePtr != ulReadPtr ) ) )
		{
			if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
			{
				ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainRinPlayoutMemOfst;
			}
			else /* f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
			{
				ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainSoutPlayoutMemOfst;
			}

			/* Set the playout event base address. */
			if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == TRUE )
				&& ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == TRUE ) )
			{
				/* 127 or 31 events image. */
				ulAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + ulEventBuffer + (cOCT6100_PLAYOUT_EVENT_MEM_SIZE * ( ( ulWritePtr - 1 ) & ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 )));
			}
			else
			{
				/* Old 31 events image. */
				ulAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + ulEventBuffer + (cOCT6100_PLAYOUT_EVENT_MEM_SIZE * ( ( ulWritePtr - 1 ) & 0x1F));
			}

			/* EVENT BASE + 4 */
			/* Playout configuration. */
			ulAddress += 4;

			ReadParams.ulReadAddress = ulAddress;
			mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Read-clear-write the new repeat bit. */
			usReadData &= 0x7FFF;

			WriteParams.ulWriteAddress = ulAddress;
			WriteParams.usWriteData = usReadData;
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Write the skip to the value of the write pointer to stop buffer playout. */

	/*=======================================================================*/
	/* First set the ignore skip clean bit if required. */	

	if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == FALSE )
		|| ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == FALSE ) )
	{
		ulAddress = ulPlayoutBaseAddress + ulIgnoreBytesOfst;

		ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
											pEchoChannel,
											ulAddress,
											&ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		mOCT6100_CREATE_FEATURE_MASK( ulIgnoreFieldSize, ulIgnoreBitOfst, &ulMask );
		
		ulTempData &= ( ~ulMask );

		/* Check if the skip need to be clean or not. */
		if ( f_pBufferPlayoutStop->fStopCleanly == FALSE )
			ulTempData |= 0x1 << ulIgnoreBitOfst;
		
		ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
										pEchoChannel,
										ulAddress,
										ulTempData);
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/*=======================================================================*/

	
	/*=======================================================================*/
	/* Fetch and modify the write pointer. */	

	ulAddress = ulPlayoutBaseAddress + ulWritePtrBytesOfst;

	ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance, pEchoChannel, ulAddress, &ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	mOCT6100_CREATE_FEATURE_MASK( ulWritePtrFieldSize, ulWritePtrBitOfst, &ulMask );
	
	ulTempData &= ( ~ulMask );
	ulTempData |= ulWritePtr << ulWritePtrBitOfst;
	
	ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
									pEchoChannel,
									ulAddress,
									ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*=======================================================================*/

	
	/*=======================================================================*/
	/* Fetch and modify the skip pointer. */	

	ulAddress = ulPlayoutBaseAddress + ulSkipPtrBytesOfst;

	ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
										pEchoChannel,
										ulAddress,
										&ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	mOCT6100_CREATE_FEATURE_MASK( ulSkipPtrFieldSize, ulSkipPtrBitOfst, &ulMask );
	
	ulTempData &= ( ~ulMask );
	ulTempData |= ulSkipPtr << ulSkipPtrBitOfst;
	
	ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
									pEchoChannel,
									ulAddress,
									ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;
	
	/*=======================================================================*/
	

	/*=======================================================================*/
	/* If in the new buffer playout case, things are in a different order. */	

	if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE )
	{
		if ( ( pSharedInfo->ImageInfo.fRinBufferPlayoutHardSkip == TRUE )
			&& ( pSharedInfo->ImageInfo.fSoutBufferPlayoutHardSkip == TRUE ) )
		{
			ulAddress = ulPlayoutBaseAddress + ulHardSkipBytesOfst;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pEchoChannel,
												ulAddress,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			mOCT6100_CREATE_FEATURE_MASK( ulHardSkipFieldSize, ulHardSkipBitOfst, &ulMask );
			
			ulTempData &= ( ~ulMask );

			/* Check if the skip need to be clean or not. */
			if ( f_pBufferPlayoutStop->fStopCleanly == FALSE )
				ulTempData |= 0x1 << ulHardSkipBitOfst;
			
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pEchoChannel,
											ulAddress,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Now is the appropriate time to skip! */
			ulAddress = ulPlayoutBaseAddress + ulIgnoreBytesOfst;

			ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
												pEchoChannel,
												ulAddress,
												&ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			mOCT6100_CREATE_FEATURE_MASK( ulIgnoreFieldSize, ulIgnoreBitOfst, &ulMask );

			ulTempData &= ( ~ulMask );

			/* Set the skip bit. */
			ulTempData |= 0x1 << ulIgnoreBitOfst;
			
			ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
											pEchoChannel,
											ulAddress,
											ulTempData);
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/*=======================================================================*/


	/*=======================================================================*/
	/* The API must set the skip bit in all the events that are queued. */

	if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE )
	{
		if ( fCheckStop == TRUE )
		{
			if ( ulReadPtr != ulWritePtr )
			{
				if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
				{
					ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainRinPlayoutMemOfst;
				}
				else /* f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
				{
					ulEventBuffer = pSharedInfo->MemoryMap.ulChanMainSoutPlayoutMemOfst;
				}

				for ( ulCurrentPtr = ulReadPtr; ulCurrentPtr != ulWritePtr; )
				{
					/* Set the playout event base address. */
					
					/* 127 or 31 events image. */
					ulAddress = pSharedInfo->MemoryMap.ulChanMainMemBase + ( f_usEchoMemIndex * pSharedInfo->MemoryMap.ulChanMainMemSize ) + ulEventBuffer + ( cOCT6100_PLAYOUT_EVENT_MEM_SIZE * ulCurrentPtr );
					ulCurrentPtr++;
					ulCurrentPtr &= ( pSharedInfo->ImageInfo.byMaxNumberPlayoutEvents - 1 );

					/* EVENT BASE + 0 playout configuration. */
					WriteParams.ulWriteAddress = ulAddress;

					/* Set skip bit + hard-skip bit. */
					WriteParams.usWriteData = 0x8000;
					if ( f_pBufferPlayoutStop->fStopCleanly == FALSE )
						WriteParams.usWriteData |= 0x4000;
					mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
		}
	}

	/*=======================================================================*/
	/* If stop immediatly, wait the stop before leaving the function. */

	if ( f_pBufferPlayoutStop->fStopCleanly == FALSE )
	{
		/* Remember that an "hard stop" was used for the next start. */
		if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
			pEchoChannel->fRinHardStop = TRUE;
		else /* if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT ) */
			pEchoChannel->fSoutHardStop = TRUE;
	}

	/*=======================================================================*/
	/* Update the channel entry to set the playing flag to FALSE. */

	/* Select the port of interest. */
	if ( f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_ROUT )
	{
		/* Check if the global ports active stat must be decremented. */
		if ( pEchoChannel->fRinBufPlaying == TRUE )
		{
			/* Decrement the number of active buffer playout ports. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts--;
		}

		pEchoChannel->fRinBufPlaying = FALSE;

		/* Return user information. */
		if ( f_pBufferPlayoutStop->pfNotifyOnPlayoutStop != NULL )
			*f_pBufferPlayoutStop->pfNotifyOnPlayoutStop = pEchoChannel->fRinBufPlayoutNotifyOnStop;

		/* Make sure no new event is recorded for this channel/port. */
		pEchoChannel->fRinBufPlayoutNotifyOnStop = FALSE;
		if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE )
		{
			pEchoChannel->ulRinBufSkipPtr = ulSkipPtr;
			pEchoChannel->ulRinBufWritePtr = ulWritePtr;
		}
		else /* if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE ) */
			pEchoChannel->ulRinBufSkipPtr = pEchoChannel->ulRinBufWritePtr;

		/* The repeat flag can now be used. */
		pEchoChannel->fRinBufPlayoutRepeatUsed = FALSE;

		/* For sure, all buffers have now been cleared on the Rin port. */
		pEchoChannel->fRinBufAdded = FALSE;

		/* Clear optimization flag if possible. */
		if ( ( pEchoChannel->fSoutBufPlaying == FALSE )
			&& ( pEchoChannel->fSoutBufPlayoutNotifyOnStop == FALSE ) )
		{
			/* Buffer playout is no more active on this channel. */
			pEchoChannel->fBufPlayoutActive = FALSE;
		}
	}
	else /* f_pBufferPlayoutStop->ulPlayoutPort == cOCT6100_CHANNEL_PORT_SOUT */
	{
		/* Check if the global ports active stat must be decremented. */
		if ( pEchoChannel->fSoutBufPlaying == TRUE )
		{
			/* Decrement the number of active buffer playout ports. */
			pSharedInfo->ChipStats.usNumberActiveBufPlayoutPorts--;
		}

		pEchoChannel->fSoutBufPlaying = FALSE;

		/* Return user information. */
		if ( f_pBufferPlayoutStop->pfNotifyOnPlayoutStop != NULL )
			*f_pBufferPlayoutStop->pfNotifyOnPlayoutStop = pEchoChannel->fSoutBufPlayoutNotifyOnStop;

		/* Make sure no new event is recorded for this channel/port. */
		pEchoChannel->fSoutBufPlayoutNotifyOnStop = FALSE;
		if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == TRUE )
		{
			pEchoChannel->ulSoutBufSkipPtr = ulSkipPtr;
			pEchoChannel->ulSoutBufWritePtr = ulWritePtr;
		}
		else /* if ( pSharedInfo->ImageInfo.fBufferPlayoutSkipInEvents == FALSE ) */
			pEchoChannel->ulSoutBufSkipPtr = pEchoChannel->ulSoutBufWritePtr;

		/* The repeat flag can now be used. */
		pEchoChannel->fSoutBufPlayoutRepeatUsed = FALSE;

		/* For sure, all buffers have now been cleared on the Sout port. */
		pEchoChannel->fSoutBufAdded = FALSE;

		/* Clear optimization flag if possible. */
		if ( ( pEchoChannel->fRinBufPlaying == FALSE )
			&& ( pEchoChannel->fRinBufPlayoutNotifyOnStop == FALSE ) )
		{
			/* Buffer playout is no more active on this channel. */
			pEchoChannel->fBufPlayoutActive = FALSE;
		}
	}

	/*=======================================================================*/



	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBufPlayoutListEntry

Description:    Reserves a free entry in the Buffer playout list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pulBufferIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBufPlayoutListEntry
UINT32 Oct6100ApiReserveBufPlayoutListEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT32							f_pulBufferIndex )
{
	PVOID	pBufPlayoutAlloc;
	UINT32	ulResult;

	mOCT6100_GET_BUFFER_ALLOC_PNT( f_pApiInstance->pSharedInfo, pBufPlayoutAlloc )

	ulResult = OctapiLlmAllocAlloc( pBufPlayoutAlloc, f_pulBufferIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_BUFFER_PLAYOUT_ALL_BUFFERS_OPEN;
		else
			return cOCT6100_ERR_FATAL_40;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBufPlayoutListEntry

Description:    Release an entry from the Buffer playout list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_ulBufferIndex			List entry to be freed.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBufPlayoutListEntry
UINT32 Oct6100ApiReleaseBufPlayoutListEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT32							f_ulBufferIndex )
{
	PVOID	pBufPlayoutAlloc;
	UINT32	ulResult;

	mOCT6100_GET_BUFFER_ALLOC_PNT( f_pApiInstance->pSharedInfo, pBufPlayoutAlloc )

	ulResult = OctapiLlmAllocDealloc( pBufPlayoutAlloc, f_ulBufferIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_41;

	return cOCT6100_ERR_OK;
}
#endif
